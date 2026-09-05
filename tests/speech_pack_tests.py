"""Check the shipped cast and exact campaign recordings with standard Python."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/audio'))
from speech_format import cached_record, check_wav, load_cast, validate_record

cast = load_cast()
references = ROOT / 'assets/audio/cast'
for voice in cast.values():
    path = references / (voice['id'] + '.wav')
    check_wav(path)
    receipt = json.loads(path.with_suffix('.json').read_text())
    assert receipt['id'] == voice['id']
    assert receipt['description'] == voice['description']
    assert receipt['sha256'] == hashlib.sha256(path.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory() as folder:
    script = Path(folder) / 'campaign.json'
    subprocess.run([sys.argv[1], '--campaign', str(script)], check=True)
    records = json.loads(script.read_text())
    assert len(records) > 60
    assert len({row['key'] for row in records}) == len(records)
    assert {'warm', 'firm', 'worried', 'urgent', 'quiet', 'plain'} <= {row['delivery'] for row in records}
    assert all(any(row['voice'] == voice for row in records) for voice in list(cast)[:5])
    assert any('received all 8 food boxes' in row['text'] for row in records)
    assert any('Jory Fen' in row['text'] and 'Silverwick' in row['text'] for row in records)
    pack = ROOT / 'assets/audio/speech'
    for row in records:
        record = validate_record(row, cast)
        path = cached_record(pack, record)
        assert path is not None, 'Refresh campaign pack: ' + row['text']
        receipt = json.loads(path.with_suffix('.json').read_text())
        reference = references / (row['voice'] + '.wav')
        assert receipt['reference_sha256'] == hashlib.sha256(reference.read_bytes()).hexdigest()
print(f'Checked {len(cast)} cast references and {len(records)} campaign recordings')
