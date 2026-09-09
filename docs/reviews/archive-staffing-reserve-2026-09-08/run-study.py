"""Run the 32 fixed seeds: python run-study.py BINARY SCHEMA OUTPUT_JSON."""
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
binary, schema, destination = sys.argv[1:]
def run(index):
    seed = index * 0x9e3779b9 & 0xffffffff
    result = subprocess.run([binary, str(seed), schema], capture_output=True, text=True)
    if result.returncode:
        return dict(seed=seed, schema=int(schema), exit_code=result.returncode,
                    error=result.stderr, partial=result.stdout)
    return json.loads(result.stdout)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows = list(pool.map(run, range(1, 33)))
Path(destination).write_text(json.dumps(rows, indent=2) + '\n')
print(sum('error' not in row for row in rows), 'completed out of', len(rows))
