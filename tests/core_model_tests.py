#!/usr/bin/env python3
"""Compare native words and token IDs with the frozen Python model reference."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

REPEAT = re.compile(r'\b(\w+)(?:\s+\1\b){2,}')
WORD = re.compile(r'[^\W\d_]+', re.UNICODE)


def collapsed(text, allowed):
    """Parity alone passes when the native runtime and the Python reference
    agree on nonsense, which is how a model that had forgotten the plain
    account shipped once already. These are the shapes that failure takes:
    scaffolding spoken aloud, an empty line, a word jammed on repeat, or
    invented spelling -- 'It wellerve bea fim' carries no repeat at all, so
    the word list, built only from authored sources, is what catches it."""
    words = text.split()
    invented = [w for w in WORD.findall(text) if w.lower() not in allowed]
    return ('# ' in text or not text.strip() or bool(REPEAT.search(text)) or len(invented) >= 2
            or (len(words) > 6 and len(set(words)) < len(words) / 2))

ROOT = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
model = ROOT / 'assets/language/core.ccv2'
reference = json.loads((ROOT / 'tests/data/core-model-reference.json').read_text())
assert hashlib.sha256(model.read_bytes()).hexdigest() == reference['model_sha256']
for row in reference['tokenizer']:
    result = subprocess.check_output([str(binary),'--encode',row['text']],text=True)
    assert list(map(int,result.split())) == row['ids'], row['text']
for row in reference['cases']:
    args = [str(binary),str(model),str(row['kind']),str(row['confidence']),str(row['retellings']),row['account']]
    if 'mind' in row:
        args += ['--mind', row['mind']]
        for memory in row.get('memories', []): args += ['--memory', memory]
        for thought in row.get('thoughts', []): args += ['--thought', thought]
    args += row['history']
    result = subprocess.check_output(args,text=True).rstrip('\n')
    assert result == row['text'], (row,result)
allowed = set(reference['vocabulary'])
collapses = [row['text'] for row in reference['cases'] if collapsed(row['text'], allowed)]
assert not collapses, f'{len(collapses)} reference sentences have collapsed: {collapses[:3]}'
args = [str(binary),str(model),'0','80','0',"Thornford's drought harvest cannot supply the eastern settlements."]
for replacement in ('', '1.5', '-1', '9999'):
    invalid = args[:]; invalid[2] = replacement
    assert subprocess.run(invalid,capture_output=True).returncode != 0
unknown = subprocess.run(args + ['--mind','fletcher:keep_order:low:high:remark'],
                         capture_output=True, text=True)
assert unknown.returncode == 0 and unknown.stdout.strip()
assert 'unknown voice' in unknown.stderr, 'a mistyped voice must not pass silently'
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)/'bad.ccv2'
    damaged = bytearray(model.read_bytes()); damaged[-1] ^= 1
    for data in (damaged,damaged[:100],damaged+b'x'):
        path.write_bytes(data); invalid = args[:]; invalid[1] = str(path)
        assert subprocess.run(invalid,capture_output=True).returncode == 3
print(f"Native parity: {len(reference['cases'])} sentences, {len(reference['tokenizer'])} token sequences; damaged models rejected")
