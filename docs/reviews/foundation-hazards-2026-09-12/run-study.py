import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).parent

def run(job):
    arm,ordinal=job
    result=subprocess.run([sys.argv[1 if arm=='parent' else 2],str(ordinal),'100','95'],capture_output=True,text=True)
    row=json.loads(result.stdout);row['arm']=arm;row['exit_code']=result.returncode
    if result.stderr:row['stderr']=result.stderr
    return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows=list(pool.map(run,[(arm,n) for arm in ('parent','current') for n in range(1,33)]))
(root/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
parent={r['ordinal']:r['hashes'] for r in rows if r['arm']=='parent'}
for row in rows:
    assert row['exit_code']==0 and len(row['hashes'])==100
    assert row['hashes']==parent[row['ordinal']]
(root/'summary.json').write_text(json.dumps({'matching_annual_hashes':3200,'worlds_per_arm':32},indent=2)+'\n')
print('32 worlds per arm passed; all 3200 annual hashes match.')
