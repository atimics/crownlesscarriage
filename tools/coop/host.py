"""Start the shared host with world files owned by its service user."""
import os
from pathlib import Path
import sys

directory = Path('/data/worlds')
directory.mkdir(parents=True, exist_ok=True)
if os.getuid() == 0:
    os.chown(directory, 10001, 10001)
    os.setgroups([])
    os.setgid(10001)
    os.setuid(10001)
os.umask(0o027)
args = [sys.executable, str(Path(__file__).with_name('server.py')),
        '--library', '/app/libcrownless_coop.so',
        '--database', str(directory / 'worlds.sqlite3'),
        '--game-dir', '/app/game', '--bind', '0.0.0.0', '--port', '8787']
origin = os.environ.get('CROWNLESS_PUBLIC_ORIGIN')
if origin:
    args += ['--public-origin', origin]
os.execv(sys.executable, args)
