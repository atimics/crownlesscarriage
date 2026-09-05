"""Run the desktop transport against the real shared host without a window."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from wsgiref.simple_server import make_server, WSGIRequestHandler

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'coop'))
from engine import Engine
from server import Application, Worlds, issue_world_pass


class Quiet(WSGIRequestHandler):
    def log_message(self, *args):
        pass


with tempfile.TemporaryDirectory() as folder:
    root = Path(folder)
    database = root / 'worlds.sqlite3'
    worlds = Worlds(database, Engine(sys.argv[2]))
    owner, guest, world = 'a' * 64, 'b' * 64, '1' * 32
    worlds.create(owner, {'id': world, 'name': 'Native road', 'player': 'Mara',
        'seed': 42, 'world_pass': issue_world_pass(database)})
    invite = worlds.invite(world, owner)['invite']
    worlds.join(world, guest, {'player': 'Bren', 'invite': invite})
    view = worlds.view(world, guest, campaign=True, enter=True)
    worlds.pose(world, guest, {'visit': view['visit'], 'context': view['session_context'],
        'scene': 0, 'pose': [0] * 83})
    identity = root / 'identity'
    identity.write_text(owner)
    identity.chmod(0o600)
    server = make_server('127.0.0.1', 0, Application(worlds), handler_class=Quiet)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        env = dict(os.environ, CC_COOP_ORIGIN=f'http://127.0.0.1:{server.server_port}')
        args = [sys.argv[1], str(identity), str(root / 'campaign'), world]
        subprocess.run(args + ['start'], env=env, check=True, timeout=30)
        saved = worlds.view(world, owner, campaign=True)['session']
        assert saved and float(saved['session'].splitlines()[1].split()[5]) == 6.25
        assert (world, view['member']) in worlds.visits
        time.sleep(1.1)
        subprocess.run(args + ['resume'], env=env, check=True, timeout=30)
    finally:
        server.shutdown()
        server.server_close()
        worker.join()
        worlds.close()
