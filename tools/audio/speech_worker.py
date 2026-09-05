#!/usr/bin/env python3
"""Serve a bounded local queue of generated Crownless speech."""

import argparse
from collections import OrderedDict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import queue
import re
import threading
import time

from speech_format import ROOT, cached_record, load_cast, validate_record


class SpeechJobs:
    def __init__(self, cast, folder, engine_factory, limit=16, budget=256 * 1024 * 1024):
        self.cast = cast
        self.folder = Path(folder)
        self.folder.mkdir(parents=True, exist_ok=True)
        self.engine_factory = engine_factory
        self.engine = None
        self.limit = limit
        self.budget = budget
        self.jobs = OrderedDict()
        self.lock = threading.RLock()
        self.queue = queue.Queue(maxsize=limit)
        self.closed = threading.Event()
        self.worker = threading.Thread(target=self.run, daemon=True)
        self.worker.start()

    def submit(self, raw):
        record = validate_record(raw, self.cast)
        key = record['key']
        with self.lock:
            ready = cached_record(self.folder, record)
            if ready is not None:
                ready.touch()
                return key, 'ready'
            job = self.jobs.get(key)
            if job is not None and (job['state'] in ('queued', 'running') or
                    job['state'] == 'failed' and time.monotonic() - job['time'] < 30):
                return key, job['state']
            pending = sum(item['state'] in ('queued', 'running') for item in self.jobs.values())
            if pending >= self.limit or self.closed.is_set():
                raise queue.Full('Speech queue is full')
            self.jobs[key] = {'record': record, 'state': 'queued', 'time': time.monotonic()}
            self.jobs.move_to_end(key)
            while len(self.jobs) > self.limit * 4:
                finished = next((name for name, item in self.jobs.items() if item['state'] not in ('queued', 'running')), None)
                if finished is None:
                    break
                del self.jobs[finished]
            self.queue.put_nowait(key)
        return key, 'queued'

    def status(self, key):
        with self.lock:
            item = self.jobs.get(key)
            return item['state'] if item else 'missing'

    def run(self):
        while not self.closed.is_set():
            try:
                key = self.queue.get(timeout=0.25)
            except queue.Empty:
                continue
            try:
                with self.lock:
                    record = self.jobs[key]['record']
                    self.jobs[key]['state'] = 'running'
                if self.engine is None:
                    self.engine = self.engine_factory()
                self.engine(record, self.folder / (key + '.wav'))
                if cached_record(self.folder, record) is None:
                    raise ValueError('Generation completed with an invalid recording')
                self.trim(key)
                with self.lock:
                    self.jobs[key]['state'] = 'ready'
            except Exception as error:
                with self.lock:
                    self.jobs[key]['state'] = 'failed'
                    self.jobs[key]['time'] = time.monotonic()
                print(f'Speech {key} failed: {type(error).__name__}: {error}', flush=True)
            finally:
                self.queue.task_done()

    def trim(self, keep):
        files = sorted(self.folder.glob('*.wav'), key=lambda path: path.stat().st_mtime)
        total = sum(path.stat().st_size for path in files)
        for path in files:
            if total <= self.budget:
                break
            if path.stem == keep:
                continue
            total -= path.stat().st_size
            path.unlink(missing_ok=True)
            path.with_suffix('.json').unlink(missing_ok=True)

    def close(self):
        self.closed.set()
        self.worker.join(timeout=1)


def make_handler(jobs, allowed_origins=()):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def allowed(self):
            host = self.headers.get('Host', '').split(':', 1)[0]
            if host not in ('localhost', '127.0.0.1'):
                return False
            origin = self.headers.get('Origin')
            return origin is None or origin in allowed_origins or origin in (
                f'http://localhost:{self.server.server_port}', f'http://127.0.0.1:{self.server.server_port}')

        def respond(self, status, body, kind='application/json'):
            data = json.dumps(body).encode() if kind == 'application/json' else body
            self.send_response(status)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            if self.headers.get('Origin') and self.allowed():
                self.send_header('Access-Control-Allow-Origin', self.headers['Origin'])
                self.send_header('Vary', 'Origin')
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_OPTIONS(self):
            if not self.allowed():
                self.respond(403, {'error': 'Origin is not enabled'})
                return
            self.send_response(204)
            self.send_header('Access-Control-Allow-Origin', self.headers.get('Origin', ''))
            self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
            self.send_header('Access-Control-Allow-Headers', 'Content-Type')
            self.send_header('Content-Length', '0')
            self.end_headers()

        def do_POST(self):
            if not self.allowed():
                self.respond(403, {'error': 'Origin is not enabled'})
                return
            if self.path != '/v1/speech':
                self.respond(404, {'error': 'Unknown speech route'})
                return
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 4096:
                    raise ValueError('Speech request size is invalid')
                self.connection.settimeout(5)
                raw = self.rfile.read(length)
                if len(raw) != length:
                    raise ValueError('Speech request is incomplete')
                key, state = jobs.submit(json.loads(raw))
                self.respond(200 if state == 'ready' else 503 if state == 'failed' else 202,
                             {'key': key, 'state': state})
            except queue.Full:
                self.respond(429, {'error': 'Speech queue is full'})
            except (ValueError, TypeError, KeyError, TimeoutError):
                self.respond(400, {'error': 'Invalid speech record'})

        def do_GET(self):
            if not self.allowed():
                self.respond(403, {'error': 'Origin is not enabled'})
                return
            if self.path == '/health':
                self.respond(200, {'status': 'ready', 'voices': len(jobs.cast), 'queue_limit': jobs.limit})
                return
            match = re.fullmatch(r'/v1/speech/([0-9a-f]{16})', self.path)
            if match is None:
                self.respond(404, {'error': 'Unknown speech route'})
                return
            key = match[1]
            path = jobs.folder / (key + '.wav')
            receipt = path.with_suffix('.json')
            try:
                data = json.loads(receipt.read_text())
                record = validate_record(data, jobs.cast)
                ready = cached_record(jobs.folder, record)
                if ready is not None and record['key'] == key:
                    self.respond(200, ready.read_bytes(), 'audio/wav')
                    return
            except (OSError, ValueError, KeyError):
                pass
            state = jobs.status(key)
            self.respond(202 if state in ('queued', 'running') else 503 if state == 'failed' else 404,
                         {'key': key, 'state': state})
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8766)
    parser.add_argument('--cast', type=Path, default=ROOT / 'assets/audio/cast.json')
    parser.add_argument('--references', type=Path, default=ROOT / 'assets/audio/cast')
    parser.add_argument('--cache', type=Path, default=Path.home() / '.cache/crownless/speech-v1')
    parser.add_argument('--device', choices=('cpu', 'mps', 'cuda'), default='cpu')
    parser.add_argument('--engine', choices=('chatterbox', 'qwen'), default='chatterbox')
    parser.add_argument('--allow-download', action='store_true')
    parser.add_argument('--allow-origin', action='append', default=[])
    parser.add_argument('--queue-limit', type=int, default=16)
    parser.add_argument('--cache-mb', type=int, default=256)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or not 1 <= args.queue_limit <= 64 or not 2 <= args.cache_mb <= 4096:
        parser.error('Choose a valid port, queue limit, and cache budget')
    from speech_engine import SpeechEngine
    jobs = SpeechJobs(load_cast(args.cast), args.cache,
        lambda: SpeechEngine(args.device, args.references, args.engine, args.allow_download),
        args.queue_limit, args.cache_mb * 1024 * 1024)
    server = ThreadingHTTPServer(('127.0.0.1', args.port), make_handler(jobs, args.allow_origin))
    server.daemon_threads = True
    print(f'Crownless voice worker: http://127.0.0.1:{args.port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        jobs.close()


if __name__ == '__main__':
    main()
