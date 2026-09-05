"""Check the bundled files and the public host export contract."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('music_host', root / 'tools/music_host.py')
host = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host)
folder = root / 'assets/audio/music'
offline = json.loads((folder / 'offline.json').read_text())['tracks']
assert len(offline) == 27
assert {p.stem for p in folder.glob('*.mp3')} == {t['stem'] for t in offline}
for track in offline:
    assert track['file'] == track['stem'] + '.mp3'
    data = (folder / (track['stem'] + '.mp3')).read_bytes()
    assert len(data) == track['bytes']
    assert hashlib.sha256(data).hexdigest() == track['sha256']
    assert f'"{track["stem"]}"' in (root / 'src/client/cc_music_offline.inc').read_text()
with tempfile.TemporaryDirectory() as tmp:
    directory = Path(tmp)
    audio = directory / 'source'
    audio.mkdir()
    # A later export grows the online library independently of the bundled set.
    for stem in ['03-01', '03-03']:
        (audio / (stem + '.mp3')).write_bytes(b'A' * 2048)
    output = directory / 'public'
    manifest = host.build(audio, output, folder / 'catalog.json')
    assert manifest['total_takes'] == 149
    assert manifest['available_takes'] == 2
    for track in manifest['tracks']:
        assert (output / track['file']).is_file()
        assert track['sha256'] in track['file']
    assert 'Access-Control-Allow-Origin: *' in (output / '_headers').read_text()
    assert 'immutable' in (output / '_headers').read_text()
    assert (output / '404.html').exists()
    assert len((output / 'catalog.txt').read_text().splitlines()) == 3
    public = json.dumps(manifest)
    assert 'suno.com' not in public and '/Users/' not in public
    (audio / '99-99.mp3').write_bytes(b'A' * 2048)
    try:
        host.build(audio, output, folder / 'catalog.json')
    except ValueError:
        pass
    else:
        raise AssertionError('unknown stem accepted')
print('Music bundle: 27 verified files; host catalog supports later exports.')
