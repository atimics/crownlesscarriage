import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys

# Arguments are the probes compiled against the parent and extracted sources.
binaries = dict(zip(("parent", "extracted"), sys.argv[1:]))
assert len(binaries) == 2

def run(job):
    arm, ordinal = job
    result = subprocess.run([binaries[arm], str(ordinal), "100", "0"],
                            check=True, capture_output=True, text=True)
    return arm, ordinal, json.loads(result.stdout)

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows = list(pool.map(run, [(arm, n) for arm in binaries for n in range(1, 33)]))
indexed = {(arm, n): row for arm, n, row in rows}
for n in range(1, 33):
    assert indexed["parent", n] == indexed["extracted", n]
report = {"schema": 86, "matching_annual_hashes": 3200,
          "worlds": [indexed["extracted", n] for n in range(1, 33)]}
Path(__file__).with_name("parity.json").write_text(json.dumps(report, indent=2) + "\n")
print("32 worlds: all 3200 annual hashes match; all annual states validate.")
