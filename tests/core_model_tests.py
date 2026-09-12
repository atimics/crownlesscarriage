#!/usr/bin/env python3
"""Compare native words and token IDs with the frozen Python model reference."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
model = ROOT / 'assets/language/core.ccv2'
reference = json.loads((ROOT / 'tests/data/core-model-reference.json').read_text())
assert hashlib.sha256(model.read_bytes()).hexdigest() == reference['model_sha256']
for row in reference['tokenizer']:
    result = subprocess.check_output([str(binary),'--encode',row['text']],text=True)
    assert list(map(int,result.split())) == row['ids'], row['text']
for row in reference['cases']:
    args = [str(binary),str(model),str(row['kind']),str(row['confidence']),str(row['retellings']),row['account'],*row['history']]
    result = subprocess.check_output(args,text=True).rstrip('\n')
    assert result == row['text'], (row,result)
args = [str(binary),str(model),'0','80','0',"Thornford's drought harvest cannot supply the eastern settlements."]
for replacement in ('', '1.5', '-1', '9999'):
    invalid = args[:]; invalid[2] = replacement
    assert subprocess.run(invalid,capture_output=True).returncode != 0
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)/'bad.ccv2'
    damaged = bytearray(model.read_bytes()); damaged[-1] ^= 1
    for data in (damaged,damaged[:100],damaged+b'x'):
        path.write_bytes(data); invalid = args[:]; invalid[1] = str(path)
        assert subprocess.run(invalid,capture_output=True).returncode == 3
print(f"Native parity: {len(reference['cases'])} sentences, {len(reference['tokenizer'])} token sequences; damaged models rejected")
