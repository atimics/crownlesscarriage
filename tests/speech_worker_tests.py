"""Voice protocol tests use a small generated tone as the model double."""

from array import array
import hashlib
import json
import math
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen
import wave
from http.server import ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/audio'))
from speech_format import audio_key, cached_record, load_cast, validate_record
from speech_worker import SpeechJobs, make_handler

EXPORTER = sys.argv.pop(1)


def tone(record, path):
    samples = array('h', (int(4000 * math.sin(i * math.pi / 20)) for i in range(4800)))
    if sys.byteorder != 'little':
        samples.byteswap()
    with wave.open(str(path), 'wb') as output:
        output.setparams((1, 2, 24000, 0, 'NONE', 'not compressed'))
        output.writeframes(samples.tobytes())
    path.with_suffix('.json').write_text(json.dumps(dict(record,
        wav_sha256=hashlib.sha256(path.read_bytes()).hexdigest())))


class SpeechWorkerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.cast = load_cast()
        self.record = dict(version=1, voice='mara-v1', text='Eight boxes.', delivery='plain',
            language='en', id='test', speaker='Mara Venn', key=audio_key('mara-v1', 'Eight boxes.'))

    def jobs(self, engine=tone, **kwargs):
        jobs = SpeechJobs(self.cast, self.folder, lambda: engine, **kwargs)
        self.addCleanup(jobs.close)
        return jobs

    def test_exporter_uses_the_same_cast_and_audio_keys(self):
        output = self.folder / 'speech.json'
        subprocess.run([EXPORTER, '--speech', str(output)], check=True)
        records = json.loads(output.read_text())
        self.assertGreater(len(records), 4)
        for record in records:
            self.assertEqual(record['key'], validate_record(record, self.cast)['key'])
        subprocess.run([EXPORTER, '--cast', str(output)], check=True)
        self.assertEqual(json.loads(output.read_text()), list(self.cast.values()))

    def test_bad_words_and_stale_keys_are_rejected(self):
        for change in ({'text': 'Nine boxes.'}, {'voice': '../mara'}, {'text': '\0'},
                       {'text': 'a' * 512}, {'delivery': 'invented'}, {'version': 2}):
            with self.assertRaises(ValueError):
                validate_record(dict(self.record, **change), self.cast)

    def test_cache_checks_audio_and_exact_words(self):
        path = self.folder / (self.record['key'] + '.wav')
        tone(self.record, path)
        self.assertEqual(cached_record(self.folder, self.record), path)
        path.write_bytes(b'corrupt')
        self.assertIsNone(cached_record(self.folder, self.record))
        tone(self.record, path)
        with self.assertRaisesRegex(ValueError, 'collision'):
            cached_record(self.folder, dict(self.record, text='Different words.'))

    def test_duplicate_request_and_queue_limit(self):
        started, finish = threading.Event(), threading.Event()
        calls = []
        def blocked(record, path):
            calls.append(record['key'])
            started.set()
            finish.wait(3)
            tone(record, path)
        jobs = self.jobs(blocked, limit=1)
        self.addCleanup(finish.set)
        key, state = jobs.submit(self.record)
        self.assertEqual(state, 'queued')
        self.assertTrue(started.wait(1))
        self.assertEqual(jobs.submit(self.record), (key, 'running'))
        second = dict(self.record, text='Nine boxes.', key=audio_key('mara-v1', 'Nine boxes.'))
        with self.assertRaises(queue.Full):
            jobs.submit(second)
        finish.set()
        jobs.queue.join()
        self.assertEqual(jobs.submit(self.record), (key, 'ready'))
        self.assertEqual(len(calls), 1)

    def test_failed_generation_can_be_observed(self):
        def broken(record, path):
            raise ValueError('test model failure')
        jobs = self.jobs(broken)
        key, _ = jobs.submit(self.record)
        jobs.queue.join()
        self.assertEqual(jobs.status(key), 'failed')
        self.assertEqual(jobs.submit(self.record), (key, 'failed'))

    def test_budget_evicts_old_recordings(self):
        jobs = self.jobs(budget=10000)
        jobs.submit(self.record)
        jobs.queue.join()
        second = dict(self.record, text='Nine boxes.', key=audio_key('mara-v1', 'Nine boxes.'))
        jobs.submit(second)
        jobs.queue.join()
        self.assertIsNone(cached_record(self.folder, self.record))
        self.assertIsNotNone(cached_record(self.folder, second))
        self.assertLessEqual(sum(p.stat().st_size for p in self.folder.glob('*.wav')), 10000)

    def test_http_retries_when_generation_finishes_during_a_cache_miss(self):
        started, finish = threading.Event(), threading.Event()
        def blocked(record, path):
            started.set()
            finish.wait(3)
            tone(record, path)
        jobs = self.jobs(blocked)
        self.addCleanup(finish.set)
        key, _ = jobs.submit(self.record)
        self.assertTrue(started.wait(1))
        receipt = self.folder / (key + '.json')
        read_text = Path.read_text
        missed = threading.Event()
        def complete_after_miss(path, *args, **kwargs):
            if path == receipt and not missed.is_set():
                missed.set()
                try:
                    return read_text(path, *args, **kwargs)
                except FileNotFoundError:
                    finish.set()
                    jobs.queue.join()
                    raise
            return read_text(path, *args, **kwargs)
        server = ThreadingHTTPServer(('127.0.0.1', 0), make_handler(jobs))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        url = f'http://127.0.0.1:{server.server_port}/v1/speech/{key}'
        with patch.object(Path, 'read_text', complete_after_miss):
            with urlopen(url, timeout=2) as response:
                self.assertEqual(response.status, 202)
        self.assertTrue(missed.is_set())
        self.assertEqual(jobs.status(key), 'ready')
        with urlopen(url, timeout=2) as response:
            self.assertEqual(response.status, 200)
            self.assertEqual(response.headers['Content-Type'], 'audio/wav')
            self.assertTrue(response.read().startswith(b'RIFF'))

    def test_http_returns_only_complete_recordings(self):
        jobs = self.jobs()
        server = ThreadingHTTPServer(('127.0.0.1', 0), make_handler(jobs))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        base = f'http://127.0.0.1:{server.server_port}'
        request = Request(base + '/v1/speech', data=json.dumps(self.record).encode(),
            headers={'Content-Type': 'application/json'})
        with urlopen(request, timeout=2) as response:
            self.assertIn(response.status, (200, 202))
        jobs.queue.join()
        with urlopen(base + '/v1/speech/' + self.record['key'], timeout=2) as response:
            self.assertEqual(response.headers['Content-Type'], 'audio/wav')
            self.assertTrue(response.read().startswith(b'RIFF'))
        with self.assertRaises(HTTPError) as error:
            urlopen(Request(base + '/health', headers={'Origin': 'https://example.invalid'}), timeout=2)
        self.assertEqual(error.exception.code, 403)
        error.exception.close()
        with self.assertRaises(HTTPError) as error:
            urlopen(base + '/v1/speech/../../private', timeout=2)
        self.assertEqual(error.exception.code, 404)
        error.exception.close()


if __name__ == '__main__':
    unittest.main()
