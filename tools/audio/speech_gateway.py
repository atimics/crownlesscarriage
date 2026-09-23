"""Speech gateway — remote-cacheable voice generation service.

This gateway sits in front of PocketTTS and returns generated WAV audio
identified by an immutable render key. The same speech record always
produces the same render key. When the render key's audio already exists
in object storage, the gateway returns it immediately without running the
model at all — saving money and latency on every repeated line.

Usage
-----
Create a config and run the gateway::

    from speech_format import load_cast, compute_reference_hashes, FilesystemStorage
    from speech_gateway import SpeechGateway, run

    cast = load_cast()
    storage = FilesystemStorage("out/voice-objects", serve_url="https://voice.example.com")
    reference_hashes = compute_reference_hashes(cast, "assets/audio/cast")
    gateway = SpeechGateway(
        cast=cast,
        storage=storage,
        reference_hashes=reference_hashes,
        engine_factory=engine_factory,
        model_version="pocket-tts-3.1.0",
        effects_version="voice-style-v1",
    )
    run(gateway, host="0.0.0.0", port=8767)

Three modes
-----------
1. **Generate on demand** — the gateway embeds its own PocketTTS engine
   and fills generation jobs internally.
2. **Worker-proxy mode** — the gateway receives speech records, checks
   object storage, and returns queued/ready states. Generation is
   handled by a separate speech_worker.py process pointed at the same
   object storage directory.
3. **Object-storage-only** — the gateway serves already-generated audio
   from object storage without running any model.

For development, inline generation is simplest. For production, the proxy
mode separates HTTP serving from GPU/CPU generation.
"""

import argparse
import json
import os
import re
import time
from collections import OrderedDict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import queue
import threading
import tempfile
import wave

import hashlib

from speech_format import (
    ROOT, DELIVERIES, ObjectStorage, FilesystemStorage,
    audio_key, derive_render_key, validate_record,
    load_cast, check_wav, compute_reference_hashes)


RATE_LIMIT_WINDOW = 60          # seconds
RATE_LIMIT_BURST = 120          # requests per window per client
MAX_TEXT_LENGTH = 500
MAX_REQUEST_SIZE = 8192


class RateLimiter:
    """Simple per-client token-bucket rate limiter."""

    def __init__(self, burst=RATE_LIMIT_BURST, window=RATE_LIMIT_WINDOW):
        self.burst = burst
        self.rate = burst / window
        self.clients = {}
        self.lock = threading.Lock()

    def allow(self, client_key):
        now = time.monotonic()
        with self.lock:
            state = self.clients.get(client_key)
            if state is None:
                tokens = self.burst - 1
                self.clients[client_key] = [now, tokens]
                return True
            elapsed = now - state[0]
            state[0] = now
            state[1] = min(self.burst, state[1] + elapsed * self.rate) - 1
            if state[1] < 0:
                state[1] = 0
                return False
            return True


class SpeechGateway:
    """Remote-cacheable speech generation service.

    Parameters
    ----------
    cast : dict
        Loaded cast records (see load_cast).
    storage : ObjectStorage
        Pluggable object storage (FilesystemStorage, S3, etc.).
    engine_factory : callable or None
        When provided, the gateway generates speech inline.
        The factory returns an engine with the same API as SpeechEngine.
    reference_hashes : dict or None
        Pre-computed voice reference hashes (see compute_reference_hashes).
    model_version : str
        Version identifier for the PocketTTS model. Part of the render key.
    effects_version : str or None
        Version identifier for post-processing effects.
    worker_pool : int
        Number of inline generation workers. The constructor defaults to zero;
        the CLI starts one worker in inline mode.
    generation_mode : str or None
        ``inline``, ``proxy``, or ``storage-only`` for health diagnostics.
    auth_token : str or None
        When set, GET and POST requests must carry
        ``Authorization: Bearer <token>``.
    rate_limit_burst : int
        Maximum requests per client within the rate-limit window.
    rate_limit_window : int
        Rate-limit window in seconds.
    """

    def __init__(self, cast, storage, engine_factory=None,
                 reference_hashes=None,
                 model_version='pocket-tts',
                 effects_version='voice-style-v1',
                 worker_pool=0, auth_token=None,
                 rate_limit_burst=RATE_LIMIT_BURST,
                 rate_limit_window=RATE_LIMIT_WINDOW,
                 generation_mode=None):
        self.cast = cast
        self.storage = storage
        self.engine_factory = engine_factory
        self.reference_hashes = reference_hashes or {}
        self.model_version = model_version
        self.effects_version = effects_version
        self.auth_token = auth_token
        self.generation_mode = generation_mode or (
            'inline' if engine_factory is not None else 'storage-only')
        self.rate_limiter = RateLimiter(burst=rate_limit_burst,
                                        window=rate_limit_window)

        # Inline generation queue
        self._jobs = OrderedDict()
        self._lock = threading.Lock()
        self._queue = queue.Queue()
        self._workers = []
        self._closed = threading.Event()
        if engine_factory is not None and worker_pool > 0:
            for _ in range(worker_pool):
                t = threading.Thread(target=self._run_worker, daemon=True)
                t.start()
                self._workers.append(t)

    # ------------------------------------------------------------------
    # Public API (used by the HTTP handler)
    # ------------------------------------------------------------------

    def submit(self, record):
        """Submit a validated speech record.

        Returns (*speech_key*, *render_key*, *state*, *url*).

        *state* is ``"ready"`` if the audio is already in object storage,
        ``"queued"`` if generation is needed, or ``"failed"`` if the
        record is invalid.
        """
        try:
            record = validate_record(record, self.cast)
        except (ValueError, KeyError, TypeError) as exc:
            return None, None, 'failed', str(exc)

        speech_key = record['key']
        render_key = derive_render_key(
            record, self.model_version, self.reference_hashes,
            self.effects_version)

        # Check object storage first
        stored = self.storage.fetch(render_key)
        if stored is not None:
            wav_data, receipt = stored
            return speech_key, render_key, 'ready', self.storage.url(render_key)

        # Queue for generation
        if self.engine_factory is None:
            return speech_key, render_key, 'queued', None

        with self._lock:
            if render_key not in self._jobs or \
               self._jobs[render_key]['state'] in ('failed', 'ready'):
                self._jobs[render_key] = {
                    'record': record,
                    'render_key': render_key,
                    'state': 'queued',
                    'time': time.monotonic(),
                }
                try:
                    self._queue.put_nowait(render_key)
                except queue.Full:
                    return speech_key, render_key, 'failed', 'generation queue is full'

        return speech_key, render_key, 'queued', None

    def status(self, render_key):
        """Return the current state of a render key (``ready``, ``queued``,
        ``running``, ``failed``, or ``missing``)."""
        stored = self.storage.fetch(render_key)
        if stored is not None:
            return 'ready'
        with self._lock:
            job = self._jobs.get(render_key)
            if job is None:
                return 'missing'
            return job['state']

    def fetch(self, render_key):
        """Return (*wav_data*, *receipt*) for a render key, or ``None``."""
        return self.storage.fetch(render_key)

    def health(self):
        """Return a diagnostic status dict."""
        with self._lock:
            queued = sum(1 for j in self._jobs.values() if j['state'] == 'queued')
            running = sum(1 for j in self._jobs.values() if j['state'] == 'running')
        if self.generation_mode == 'proxy':
            worker_state = 'external_unverified'
        elif self.generation_mode == 'storage-only':
            worker_state = 'none'
        elif self._closed.is_set() or not any(t.is_alive() for t in self._workers):
            worker_state = 'stopped'
        else:
            worker_state = 'running' if running else 'idle'
        return {
            'status': 'degraded' if worker_state == 'stopped' else 'ready',
            'voices': list(self.cast.keys()),
            'voices_count': len(self.cast),
            'model_version': self.model_version,
            'effects_version': self.effects_version or 'none',
            'reference_hashes_pinned': len(self.reference_hashes) > 0,
            'inline_generation': self.engine_factory is not None,
            'generation_mode': self.generation_mode,
            'worker_state': worker_state,
            'storage': self.storage.health(),
            'queue': {'queued': queued, 'running': running, 'limit': self._queue.maxsize},
            'auth_required': self.auth_token is not None,
        }

    # ------------------------------------------------------------------
    # Inline generation worker
    # ------------------------------------------------------------------

    def _run_worker(self):
        engine = None
        while not self._closed.is_set():
            try:
                render_key = self._queue.get(timeout=0.25)
            except queue.Empty:
                continue
            if engine is None and self.engine_factory is not None:
                engine = self.engine_factory()
            with self._lock:
                job = self._jobs.get(render_key)
                if job is None:
                    self._queue.task_done()
                    continue
                job['state'] = 'running'
                record = job['record']
            try:
                wav_path = Path(self.storage.root) / 'tmp' / f'{render_key}.wav'
                wav_path.parent.mkdir(parents=True, exist_ok=True)
                engine(record, wav_path)
                check_wav(wav_path)
                wav_data = wav_path.read_bytes()
                receipt = {
                    'key': record['key'],
                    'render_key': render_key,
                    'voice': record['voice'],
                    'text': record['text'],
                    'delivery': record.get('delivery', 'plain'),
                    'model': 'pocket',
                    'model_version': self.model_version,
                    'effects_version': self.effects_version,
                    'reference_hash': self.reference_hashes.get(record['voice'], 'unpinned'),
                    'wav_sha256': hashlib.sha256(wav_data).hexdigest(),
                    'sample_rate': 24000,
                }
                self.storage.store(render_key, wav_data, receipt)
                wav_path.unlink(missing_ok=True)
                with self._lock:
                    job['state'] = 'ready'
            except Exception as exc:
                print(f'Generation failed for {render_key}: {type(exc).__name__}: {exc}', flush=True)
                with self._lock:
                    if job is not None:
                        job['state'] = 'failed'
            finally:
                self._queue.task_done()

    def close(self):
        self._closed.set()
        for t in self._workers:
            t.join(timeout=1)

    # ------------------------------------------------------------------
    # Authentication and rate limiting helpers for the HTTP handler
    # ------------------------------------------------------------------

    def check_auth(self, request_headers):
        """Return True if the request is authorised."""
        if self.auth_token is None:
            return True
        auth = request_headers.get('Authorization', '')
        return auth == f'Bearer {self.auth_token}'

    def check_rate_limit(self, client_key):
        """Return True if the request is within the rate limit."""
        return self.rate_limiter.allow(client_key)


# ------------------------------------------------------------------
# HTTP handler factory
# ------------------------------------------------------------------

def make_handler(gateway, allowed_origins=None):
    """Return a ``BaseHTTPRequestHandler`` subclass bound to *gateway*."""

    allowed_origins = allowed_origins or []

    class Handler(BaseHTTPRequestHandler):

        def log_message(self, *args):
            pass

        def _origin_allowed(self):
            origin = self.headers.get('Origin')
            return origin is None or origin in allowed_origins

        def _client_key(self):
            auth = self.headers.get('Authorization', '')
            if auth.startswith('Bearer ') and auth[7:]:
                return auth[7:]
            return self.client_address[0]

        def _respond(self, status, body, content_type='application/json'):
            data = json.dumps(body).encode() if content_type == 'application/json' else body
            self.send_response(status)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            origin = self.headers.get('Origin')
            if origin and origin in allowed_origins:
                self.send_header('Access-Control-Allow-Origin', origin)
                self.send_header('Vary', 'Origin')
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_OPTIONS(self):
            if not self._origin_allowed():
                self._respond(403, {'error': 'origin not allowed'})
                return
            self.send_response(204)
            origin = self.headers.get('Origin', '')
            if origin in allowed_origins:
                self.send_header('Access-Control-Allow-Origin', origin)
            self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
            self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization')
            self.send_header('Content-Length', '0')
            self.end_headers()

        def do_GET(self):
            if not self._origin_allowed():
                self._respond(403, {'error': 'origin not allowed'})
                return
            if not gateway.check_auth(self.headers):
                self._respond(401, {'error': 'unauthorised'})
                return
            if self.path == '/health':
                health = gateway.health()
                self._respond(200 if health['status'] == 'ready' else 503, health)
                return
            match = re.fullmatch(r'/v1/speech/([0-9a-f]{16})', self.path)
            if match is None:
                self._respond(404, {'error': 'unknown route'})
                return
            render_key = match[1]
            stored = gateway.fetch(render_key)
            if stored is not None:
                wav_data, receipt = stored
                self._respond(200, wav_data, 'audio/wav')
                return
            state = gateway.status(render_key)
            if state == 'queued' or state == 'running':
                self._respond(202, {'state': state, 'render_key': render_key})
            elif state == 'failed':
                self._respond(503, {'state': 'failed', 'render_key': render_key})
            else:
                self._respond(404, {'state': 'missing', 'render_key': render_key})

        def do_POST(self):
            if not self._origin_allowed():
                self._respond(403, {'error': 'origin not allowed'})
                return
            if not gateway.check_auth(self.headers):
                self._respond(401, {'error': 'unauthorised'})
                return
            if not gateway.check_rate_limit(self._client_key()):
                self._respond(429, {'error': 'rate limit exceeded'})
                return
            if self.path != '/v1/speech':
                self._respond(404, {'error': 'unknown route'})
                return
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= MAX_REQUEST_SIZE:
                    raise ValueError('invalid content length')
                self.connection.settimeout(5)
                raw = self.rfile.read(length)
                if len(raw) != length:
                    raise ValueError('incomplete request')
                record = json.loads(raw)
            except (ValueError, TypeError, OSError) as exc:
                self._respond(400, {'error': str(exc)})
                return
            speech_key, render_key, state, detail = gateway.submit(record)
            if speech_key is None:
                self._respond(400, {'error': detail})
                return
            body = {
                'speech_key': speech_key,
                'render_key': render_key,
                'state': state,
            }
            if state == 'ready':
                url = gateway.storage.url(render_key)
                if url:
                    body['url'] = url
                self._respond(200, body)
            elif state == 'queued':
                self._respond(202, body)
            else:
                self._respond(503, body)

    return Handler


# ------------------------------------------------------------------
# CLI
# ------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='Crownless speech gateway')
    parser.add_argument('--port', type=int, default=8767)
    parser.add_argument('--host', type=str, default='127.0.0.1')
    parser.add_argument('--engine', choices=('pocket', 'inline', 'proxy', 'storage-only'),
                        default='inline')
    parser.add_argument('--cast', type=Path, default=ROOT / 'assets/audio/cast.json')
    parser.add_argument('--references', type=Path, default=ROOT / 'assets/audio/cast')
    parser.add_argument('--storage-root', type=Path,
                        default=ROOT / 'out/voice-objects')
    parser.add_argument('--storage-serve-url', type=str, default=None)
    parser.add_argument('--model-version', type=str, default='pocket-tts-3.1.0')
    parser.add_argument('--effects-version', type=str, default='voice-style-v1')
    parser.add_argument('--allow-origin', action='append', default=[])
    parser.add_argument('--auth-token', type=str, default=None,
                        help='require Bearer auth for every request')
    parser.add_argument('--rate-limit-burst', type=int, default=RATE_LIMIT_BURST)
    parser.add_argument('--rate-limit-window', type=int, default=RATE_LIMIT_WINDOW)
    parser.add_argument('--device', choices=('cpu', 'mps', 'cuda'), default='cpu')
    parser.add_argument('--allow-download', action='store_true')
    args = parser.parse_args()

    cast = load_cast(args.cast)
    reference_hashes = compute_reference_hashes(cast, args.references)
    storage = FilesystemStorage(args.storage_root,
                                serve_url=args.storage_serve_url)

    engine_factory = None
    worker_pool = 0
    if args.engine == 'inline' or args.engine == 'pocket':
        from speech_engine import SpeechEngine
        engine_factory = lambda: SpeechEngine(
            device=args.device,
            references=args.references,
            engine='pocket',
            allow_download=args.allow_download)
        worker_pool = 1
        if args.engine == 'inline':
            pass  # already set
    elif args.engine == 'proxy':
        pass  # no inline engine; expect a separate worker pointed at storage
    # storage-only: no engine, no workers

    gateway = SpeechGateway(
        cast=cast,
        storage=storage,
        engine_factory=engine_factory,
        reference_hashes=reference_hashes,
        model_version=args.model_version,
        effects_version=args.effects_version,
        worker_pool=worker_pool,
        auth_token=args.auth_token,
        rate_limit_burst=args.rate_limit_burst,
        rate_limit_window=args.rate_limit_window,
        generation_mode='inline' if args.engine == 'pocket' else args.engine,
    )

    allowed_origins = list(args.allow_origin)
    handler = make_handler(gateway, allowed_origins)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    server.daemon_threads = True

    print(f'Crownless speech gateway: http://{args.host}:{args.port}', flush=True)
    print(f'  engine: {args.engine}', flush=True)
    print(f'  voices: {len(cast)}', flush=True)
    print(f'  storage: {storage.health()["objects"]} objects', flush=True)
    print(f'  auth: {"required" if args.auth_token else "none"}', flush=True)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        gateway.close()
        server.server_close()


if __name__ == '__main__':
    main()
