import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).parent
jobs = [(arm, seed) for arm in ('parent', 'current') for seed in (4, 15)]
def run(job):
    arm, seed = job
    binary = sys.argv[1 if arm == 'parent' else 2]
    result = subprocess.run([binary, str(seed), '7300'], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr)
    row = json.loads(result.stdout)
    row['arm'] = arm
    return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows = list(pool.map(run, jobs))
(root/'gossip-results.json').write_text(json.dumps(rows, indent=2)+'\n')
print(json.dumps(rows, indent=2))
