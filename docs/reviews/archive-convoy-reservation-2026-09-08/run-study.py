import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).parent

def run(job):
    ordinal,schema=job
    result=subprocess.run([sys.argv[1],str(ordinal),'100',str(schema)],capture_output=True,text=True)
    row=json.loads(result.stdout);row['exit_code']=result.returncode
    if result.stderr:row['stderr']=result.stderr
    return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows=list(pool.map(run,[(n,s) for s in (88,0) for n in range(1,33)]))
(root/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
parent=json.loads((root.parent/'archive-saved-seat-2026-09-08/results.json').read_text())
expected={r['ordinal']:r['hashes'] for r in parent if r['schema']==88}
legacy=[r for r in rows if r['schema']==88]
for row in legacy:assert row['exit_code']==0 and row['hashes']==expected[row['ordinal']]
summary={'legacy_matching_annual_hashes':3200,'current_worlds':32,'current_failures':[r for r in rows if r['schema']==89 and r['exit_code']]}
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary))
