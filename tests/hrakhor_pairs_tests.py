"""Check actual English model outputs, paired labels, and native failure paths."""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from hrakhor_pairs import pairs

probe = Path(sys.argv[1]).resolve()
evidence = json.loads((ROOT / 'docs/reviews/gossip-flow-2026-09-12/speech-results.json').read_text())
rows = [{'kind': row['kind'], 'text': row['core_account'], 'output': row['model_text'],
         'confidence': row['confidence'], 'split': 'test', 'event_id': row['event_id']}
        for row in evidence['rows'] if row['model_supported']]
result = list(pairs(rows, probe, 100))
assert len(result) == len(rows) == 24
assert any(row['english'] != row['hrakhor'] for row in result)
for source, pair in zip(rows, result):
    assert pair['source'] == source and pair['english'] == source['output']
    assert pair['source']['split'] == 'test'
    packet = json.loads(subprocess.check_output(
        [str(probe), str(source['kind']), str(source['confidence']), '0', source['text'], '--packet'], text=True))
    for field in packet['fields']:
        if field['role'] in (1, 2, 3, 4, 5) and field['text'] in pair['english']:
            assert field['text'] in pair['hrakhor'], (field, pair)
assert all(p['english'] == p['hrakhor'] for p in pairs(rows, probe, 0))
for strength in ('-1', '101', '', 'bad'):
    row = rows[0]
    assert subprocess.run([str(probe), str(row['kind']), '80', '0', row['text'],
                           '--hrakhor', strength], capture_output=True).returncode == 2
try:
    list(pairs([{**rows[0], 'output': 'first\nsecond'}], probe, 100))
except ValueError:
    pass
else:
    raise AssertionError('Multiline output accepted')
print('24 recorded model outputs passed Hra\'khor pairing and named-field checks')
