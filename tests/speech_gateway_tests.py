"""Speech gateway tests — render key, object storage, and the HTTP service contract."""

import copy
import hashlib
import json
import math
from array import array
from pathlib import Path
import tempfile
import threading
import time
import unittest
from http.server import ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.request import Request, urlopen
import wave
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/audio'))

from speech_format import (
    ObjectStorage, FilesystemStorage,
    audio_key, derive_render_key, compute_reference_hashes, load_cast, validate_record)


def tone_wav(path, duration_sec=0.3, sample_rate=24000):
    samples = array('h', (int(4000 * math.sin(i * math.pi / 20)) for i in range(int(sample_rate * duration_sec))))
    if sys.byteorder != 'little':
        samples.byteswap()
    with wave.open(str(path), 'wb') as output:
        output.setparams((1, 2, sample_rate, 0, 'NONE', 'not compressed'))
        output.writeframes(samples.tobytes())
    record = dict(version=1, voice='mara-v1', text='Test.', delivery='plain',
                  language='en', id='test', speaker='Tester',
                  key=audio_key('mara-v1', 'Test.'))
    path.with_suffix('.json').write_text(json.dumps(dict(record,
        wav_sha256=hashlib.sha256(path.read_bytes()).hexdigest())))


class DeriveRenderKeyTests(unittest.TestCase):
    def setUp(self):
        self.record = dict(version=1, voice='mara-v1', text='The road is quiet.',
                           delivery='plain', language='en', id='test', speaker='Mara',
                           key=audio_key('mara-v1', 'The road is quiet.'))

    def test_same_record_same_render_key(self):
        k1 = derive_render_key(self.record, model_version='pocket-tts-3.1.0')
        k2 = derive_render_key(self.record, model_version='pocket-tts-3.1.0')
        self.assertEqual(k1, k2)

    def test_different_model_produces_different_key(self):
        k1 = derive_render_key(self.record, model_version='pocket-tts-3.1.0')
        k2 = derive_render_key(self.record, model_version='pocket-tts-4.0.0')
        self.assertNotEqual(k1, k2)

    def test_different_reference_produces_different_key(self):
        refs = {'mara-v1': 'abc123'}
        k1 = derive_render_key(self.record, reference_hashes=refs)
        refs2 = {'mara-v1': 'def456'}
        k2 = derive_render_key(self.record, reference_hashes=refs2)
        self.assertNotEqual(k1, k2)

    def test_pinned_vs_unpinned_is_different(self):
        refs = {'mara-v1': 'abc123'}
        k1 = derive_render_key(self.record, reference_hashes=refs)
        k2 = derive_render_key(self.record)
        self.assertNotEqual(k1, k2)

    def test_different_effects_version(self):
        k1 = derive_render_key(self.record, effects_version='voice-style-v1')
        k2 = derive_render_key(self.record, effects_version='voice-style-v2')
        self.assertNotEqual(k1, k2)

    def test_different_effects_unpinned_same_reference(self):
        refs = {'mara-v1': 'abc'}
        k1 = derive_render_key(self.record, reference_hashes=refs)
        k2 = derive_render_key(self.record, reference_hashes=refs)
        self.assertEqual(k1, k2)

    def test_key_is_16_hex_chars(self):
        k = derive_render_key(self.record)
        self.assertRegex(k, r'^[0-9a-f]{16}$')


class FilesystemStorageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.storage = FilesystemStorage(self.root, serve_url='https://voice.example.com')
        self.render_key = 'abcd123456789012'

    def test_store_and_fetch_roundtrip(self):
        wav_data = b'\x00\x01\x02\x03'
        receipt = {'key': 'test', 'voice': 'mara-v1', 'text': 'Hey.'}
        url = self.storage.store(self.render_key, wav_data, receipt)
        self.assertIsNotNone(url)
        self.assertIn('abcd', url)
        stored = self.storage.fetch(self.render_key)
        self.assertIsNotNone(stored)
        stored_wav, stored_receipt = stored
        self.assertEqual(stored_wav, wav_data)
        self.assertEqual(stored_receipt, receipt)

    def test_store_and_url(self):
        wav_data = b'\x00\x01\x02\x03'
        self.storage.store(self.render_key, wav_data, {})
        url = self.storage.url(self.render_key)
        self.assertEqual(url, f'https://voice.example.com/ab/{self.render_key}.wav')

    def test_missing_returns_none(self):
        self.assertIsNone(self.storage.fetch('ffffffffffffffff'))

    def test_health_counts_objects(self):
        self.assertEqual(self.storage.health()['objects'], 0)
        self.storage.store(self.render_key, b'\x00' * 100, {})
        self.storage.store('fedcba9876543210', b'\x00' * 200, {})
        self.assertEqual(self.storage.health()['objects'], 2)
        self.assertEqual(self.storage.health()['bytes'], 300)


class HttpServiceContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.storage = FilesystemStorage(Path(self.temp.name))
        self.cast = load_cast()
        from speech_gateway import SpeechGateway, make_handler
        self.gateway = SpeechGateway(
            cast=self.cast,
            storage=self.storage,
            engine_factory=None,
            reference_hashes=None,
            worker_pool=0,
        )
        self.handler = make_handler(self.gateway, allowed_origins=[])
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), self.handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)
        self.base = f'http://127.0.0.1:{self.server.server_port}'

    def record(self, text='Test.'):
        return {
            'version': 1, 'voice': 'mara-v1', 'text': text,
            'delivery': 'plain', 'language': 'en', 'id': 'test', 'speaker': 'Tester',
            'key': audio_key('mara-v1', text),
        }

    def test_health_endpoint(self):
        with urlopen(f'{self.base}/health', timeout=2) as response:
            self.assertEqual(response.status, 200)
            body = json.loads(response.read())
            self.assertIn('voices_count', body)
            self.assertIn('status', body)
            self.assertEqual(body['status'], 'ready')
            self.assertEqual(body['inline_generation'], False)

    def test_submit_new_speech_returns_queued(self):
        request = Request(
            f'{self.base}/v1/speech',
            data=json.dumps(self.record()).encode(),
            headers={'Content-Type': 'application/json'})
        with urlopen(request, timeout=2) as response:
            self.assertEqual(response.status, 202)
            body = json.loads(response.read())
            self.assertIn('speech_key', body)
            self.assertIn('render_key', body)
            self.assertEqual(body['state'], 'queued')

    def test_submit_validates_record(self):
        request = Request(
            f'{self.base}/v1/speech',
            data=json.dumps({'version': 1, 'voice': 'missing-voice', 'text': 'Hi.'}).encode(),
            headers={'Content-Type': 'application/json'})
        with self.assertRaises(HTTPError) as ctx:
            urlopen(request, timeout=2)
        self.assertEqual(ctx.exception.code, 400)

    def test_different_text_different_render_key(self):
        r1 = self.record('Hello.')
        r2 = self.record('Goodbye.')
        keys = set()
        for rec in (r1, r2):
            request = Request(
                f'{self.base}/v1/speech',
                data=json.dumps(rec).encode(),
                headers={'Content-Type': 'application/json'})
            with urlopen(request, timeout=2) as response:
                body = json.loads(response.read())
                keys.add(body['render_key'])
        self.assertEqual(len(keys), 2)

    def test_fetch_preexisting_audio(self):
        render_key = 'deadbeef12345678'
        wav_data = b'\x00' * 100
        self.storage.store(render_key, wav_data, {'key': 'stored', 'voice': 'mara-v1'})
        request = Request(
            f'{self.base}/v1/speech',
            data=json.dumps(self.record('Audio ready.')).encode(),
            headers={'Content-Type': 'application/json'})
        with urlopen(request, timeout=2) as response:
            body = json.loads(response.read())
            self.assertEqual(body['state'], 'queued')

    def test_fetch_wav_by_render_key(self):
        render_key = 'aaaaaaaa12345678'
        wav_data = b'\x00' * 56
        self.storage.store(render_key, wav_data, {'key': 'x'})
        with urlopen(f'{self.base}/v1/speech/{render_key}', timeout=2) as response:
            self.assertEqual(response.status, 200)
            self.assertEqual(response.headers['Content-Type'], 'audio/wav')
            self.assertEqual(response.read(), wav_data)

    def test_fetch_missing_returns_404(self):
        with self.assertRaises(HTTPError) as ctx:
            urlopen(f'{self.base}/v1/speech/ffffffffffffffff', timeout=2)
        self.assertEqual(ctx.exception.code, 404)

    def test_auth_enforced_when_configured(self):
        self.gateway.auth_token = 's3cr3t'
        request = Request(
            f'{self.base}/v1/speech',
            data=json.dumps(self.record()).encode(),
            headers={'Content-Type': 'application/json'})
        with self.assertRaises(HTTPError) as ctx:
            urlopen(request, timeout=2)
        self.assertEqual(ctx.exception.code, 401)
        request.add_header('Authorization', 'Bearer s3cr3t')
        with urlopen(request, timeout=2) as response:
            self.assertIn(response.status, (200, 202))

    def test_rate_limiting(self):
        self.gateway.rate_limiter.burst = 2
        self.gateway.rate_limiter.rate = 2
        rec = self.record('Rate test.')
        ok = 0
        refused = 0
        for _ in range(5):
            request = Request(
                f'{self.base}/v1/speech',
                data=json.dumps(rec).encode(),
                headers={'Content-Type': 'application/json'})
            try:
                with urlopen(request, timeout=2) as resp:
                    ok += 1
            except HTTPError as exc:
                if exc.code == 429:
                    refused += 1
                exc.close()
        self.assertGreaterEqual(refused, 2)


if __name__ == '__main__':
    unittest.main()
