import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).parent

def run(job):
    arm,ordinal,schema=job
    binary=sys.argv[1] if arm=='parent' else sys.argv[2]
    result=subprocess.run([binary,str(ordinal),'100',str(schema)],capture_output=True,text=True)
    row=json.loads(result.stdout);row['arm']=arm;row['exit_code']=result.returncode
    if result.stderr:row['stderr']=result.stderr
    return row
jobs=[(arm,n,s) for arm,s in [('parent',95),('legacy',95),('current',0)] for n in range(1,33)]
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows=list(pool.map(run,jobs))
(root/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
expected={r['ordinal']:r['hashes'] for r in rows if r['arm']=='parent'}
for row in rows:
    assert row['exit_code']==0 and len(row['hashes'])==100
    if row['arm']=='legacy':assert row['hashes']==expected[row['ordinal']]
summary={'legacy_matching_annual_hashes':3200,'current_worlds':32,'current_failures':[]}
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary))
