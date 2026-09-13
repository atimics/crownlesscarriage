import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).parent
jobs = [(arm, n) for arm in ('parent', 'changed') for n in range(1, 33)]
def run(job):
    arm, ordinal = job
    binary = sys.argv[1 if arm == 'parent' else 2]
    result = subprocess.run([binary, str(ordinal), '100', '0'], capture_output=True, text=True)
    row = json.loads(result.stdout)
    row.update(arm=arm, exit_code=result.returncode, stderr=result.stderr)
    return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows = list(pool.map(run, jobs))
(root/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
expected = {r['ordinal']: r['hashes'] for r in rows if r['arm'] == 'parent'}
for row in rows:
    assert row['exit_code'] == 0 and len(row['hashes']) == 100
    assert row['hashes'] == expected[row['ordinal']]
print('All 6,400 annual validity checks passed; all 3,200 paired hashes match.')
