import concurrent.futures
import json
from pathlib import Path
import subprocess
cases=[(353,81),(828,73),(189,191),(197,69),(426,209),(148,845),(372,884),(392,761),(726,645)]
binaries={'parent':'/private/tmp/courier-life-parent-probe','repair':'/private/tmp/courier-life-fixed-probe'}
jobs=[(arm,n,y,0) for arm in binaries for n,y in cases]
jobs += [(arm,n,100,0) for arm in ('parent','repair') for n in range(1,33)]
def run(job):
 arm,n,y,schema=job
 result=subprocess.run([binaries[arm],str(n),str(y),str(schema)],text=True,capture_output=True)
 row=json.loads(result.stdout);row.update(arm=arm,exit_code=result.returncode)
 if result.stderr:row['stderr']=result.stderr
 return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:rows=list(pool.map(run,jobs))
Path(__file__).with_name('results.json').write_text(json.dumps(rows,indent=2)+'\n')
for arm in binaries:
 selected=[r for r in rows if r['arm']==arm]
 print(arm,len(selected),'runs',[(r['ordinal'],r['failed_year'],r.get('error')) for r in selected if r['failed_year']],flush=True)
