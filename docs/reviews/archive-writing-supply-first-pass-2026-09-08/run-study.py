import argparse
import concurrent.futures
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('binaries',type=Path)
parser.add_argument('--worlds',type=int,default=32)
parser.add_argument('--years',type=int,default=100)
args=parser.parse_args()
output=Path(__file__).parent
(output/'traces').mkdir(exist_ok=True)

def run(job):
    arm,n=job
    result=subprocess.run([str(args.binaries/arm/'probe'),str(n),str(args.years)],capture_output=True)
    trace=output/'traces'/f'{arm}-{n:02d}.csv.gz'
    trace.write_bytes(gzip.compress(b'kind,day,shipment_id,reserve,staff,units,charge\n'+result.stderr,mtime=0))
    try: row=json.loads(result.stdout)
    except json.JSONDecodeError: row={'ordinal':n,'probe_output':result.stdout.decode(errors='replace')}
    row.update(arm=arm,exit_code=result.returncode,trace=str(trace.relative_to(output)),trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest())
    if result.returncode==0:
        events=list(csv.DictReader(io.StringIO('kind,day,shipment_id,reserve,staff,units,charge\n'+result.stderr.decode())))
        purchases=[e for e in events if e['kind']=='purchase']
        staffing=[e for e in events if e['kind']=='staff']
        assert len(purchases)==row['purchases']
        assert sum(int(e['charge']) for e in purchases)==row['spent']
        assert all(int(e['reserve'])>=int(e['charge'])>0 for e in purchases)
        assert len(staffing)==row['weeks']
        assert sum(row['ordered'])==sum(row['delivered'])+sum(row['redirected'])+sum(row['lost'])+row['unresolved_units']
        row['before_staff_low_reserve']=sum(int(e['reserve'])<50 for e in staffing)
        row['purchase_crossed_50']=sum(int(e['reserve'])>=50 and int(e['reserve'])-int(e['charge'])<50 for e in purchases)
    print(arm,n,result.returncode,flush=True)
    return row

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows=list(pool.map(run,[(arm,n) for arm in ('protected',) for n in range(1,args.worlds+1)]))
(output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
summary={}
for arm in ('protected',):
    selected=[r for r in rows if r['arm']==arm]
    valid=[r for r in selected if r['exit_code']==0]
    summary[arm]={'runs':len(selected),'valid':len(valid),'failures':[r for r in selected if r['exit_code']]}
    for key in ('weeks','zero_staff','low_reserve','ready','staff_sum','lore_sum','end_staff','end_lore','purchases','spent','unresolved_units','before_staff_low_reserve','purchase_crossed_50'):
        summary[arm][key]=sum(r[key] for r in valid)
    for key in ('ordered','delivered','redirected','lost'):
        summary[arm][key]=[sum(r[key][i] for r in valid) for i in range(len(valid[0][key]))] if valid else []
(output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
