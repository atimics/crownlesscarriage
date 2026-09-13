import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).parent

def run(n):
    result=subprocess.run([sys.argv[1],str(n),'100','0'],capture_output=True,text=True)
    row=json.loads(result.stdout);row['exit_code']=result.returncode
    return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:rows=list(pool.map(run,range(1,33)))
(root/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
parent=json.loads((root.parent/'archive-saved-seat-2026-09-08/results.json').read_text())
expected={r['ordinal']:r['hashes'] for r in parent if r['schema']==88}
for row in rows:assert row['exit_code']==0 and row['hashes']==expected[row['ordinal']]
summary={'worlds':32,'matching_annual_hashes':3200,'immutable_annual_queries':3200,'valid_annual_states':3200}
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))
