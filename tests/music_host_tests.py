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
catalog = json.loads((folder / 'catalog.json').read_text())
hosted = json.loads((folder / 'hosted.json').read_text())['tracks']
expected_hosted = {'61-01', '62-01'} | {f'{cue:02d}-01' for cue in range(65, 83)}
assert {t['stem'] for t in hosted} == expected_hosted
assert {p.stem for p in (folder / 'hosted').glob('*.mp3')} == expected_hosted
for track in hosted:
    assert track['file'] == 'hosted/' + track['stem'] + '.mp3'
    data = (folder / track['file']).read_bytes()
    assert len(data) == track['bytes']
    assert hashlib.sha256(data).hexdigest() == track['sha256']
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
    complete = host.build(folder, directory / 'complete', folder / 'catalog.json')
    assert complete['available_takes'] == 47
    assert {t['stem'] for t in complete['tracks']} == (
        expected_hosted | {t['stem'] for t in offline})
    audio = directory / 'source'
    audio.mkdir()
    # A later export grows the online library independently of the bundled set.
    for stem in ['03-01', '03-03', '65-01', '66-02', '82-02']:
        (audio / (stem + '.mp3')).write_bytes(b'A' * 2048)
    output = directory / 'public'
    manifest = host.build(audio, output, folder / 'catalog.json')
    assert manifest['total_takes'] == len(catalog['takes']) == 185
    assert manifest['available_takes'] == 5
    for track in manifest['tracks']:
        assert (output / track['file']).is_file()
        assert track['sha256'] in track['file']
    assert 'Access-Control-Allow-Origin: *' in (output / '_headers').read_text()
    assert 'immutable' in (output / '_headers').read_text()
    assert (output / '404.html').exists()
    assert len((output / 'catalog.txt').read_text().splitlines()) == 6
    assert {t['title'] for t in manifest['tracks']} >= {
        'Thornford - Flour on the Windowsill',
        'Thornford - Flour on the Windowsill - Shortage',
        'Hollowbarrow - One Lantern Left - Recovery',
    }
    public = json.dumps(manifest)
    assert 'suno.com' not in public and '/Users/' not in public
    hosted = audio / 'hosted'
    hosted.mkdir()
    (hosted / '61-01.mp3').write_bytes(b'B' * 2048)
    manifest = host.build(audio, output, folder / 'catalog.json')
    assert manifest['available_takes'] == 6
    assert any(t['stem'] == '61-01' for t in manifest['tracks'])
    (hosted / '03-01.mp3').write_bytes(b'B' * 2048)
    try:
        host.build(audio, output, folder / 'catalog.json')
    except ValueError as error:
        assert 'Duplicate soundtrack stem' in str(error)
    else:
        raise AssertionError('duplicate stem accepted')
    (hosted / '03-01.mp3').unlink()
    (audio / '99-99.mp3').write_bytes(b'A' * 2048)
    try:
        host.build(audio, output, folder / 'catalog.json')
    except ValueError:
        pass
    else:
        raise AssertionError('unknown stem accepted')
print('Music bundle: 27 verified files; host catalog supports later exports.')
