"""Run the native downloader against the real queue and a bounded test renderer."""
from array import array
import hashlib
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import wave
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/audio'))
from speech_worker import SpeechJobs, make_handler
from speech_format import load_cast

def tone(record, path):
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, 24000, 0, 'NONE', 'not compressed'))
        samples = array('h', [1000, -1000] * 2400)
        if sys.byteorder != 'little': samples.byteswap()
        out.writeframes(samples.tobytes())
    path.with_suffix('.json').write_text(json.dumps(dict(record,
        wav_sha256=hashlib.sha256(path.read_bytes()).hexdigest())))

class Oversized(BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def do_POST(self):
        self.send_response(200)
        self.end_headers()
    def do_GET(self):
        self.send_response(200)
        self.end_headers()
        try: self.wfile.write(b'x' * 1300000)
        except (BrokenPipeError, ConnectionResetError): pass

def check(handler, mode):
    server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        env = dict(os.environ, CROWNLESS_VOICE_URL=f'http://127.0.0.1:{server.server_port}')
        subprocess.run([sys.argv[1], mode], env=env, check=True, timeout=15)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()

with tempfile.TemporaryDirectory() as folder:
    jobs = SpeechJobs(load_cast(), folder, lambda: tone)
    try:
        check(make_handler(jobs), 'success')
        check(make_handler(jobs), 'cancel')
        check(Oversized, 'failure')
    finally: jobs.close()
