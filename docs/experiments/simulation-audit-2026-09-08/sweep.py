import concurrent.futures,csv,gzip,hashlib,json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
BIN='/private/tmp/crownless-audit-build/crownless_sim_metrics'
def run(seed):
 command=[BIN,'--seed',str(seed),'--years','16000','--campaign-metrics']
 start=time.monotonic(); n=0;last=None
 with (ROOT/f'seed-{seed:03}.log').open('w') as err:
  p=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=err,text=True)
  reader=csv.DictReader(p.stdout)
  with gzip.open(ROOT/f'seed-{seed:03}.csv.gz','wt') as out:
   writer=csv.DictWriter(out,fieldnames=reader.fieldnames);writer.writeheader()
   for row in reader:
    n+=1;last=row
    if n<=1000 or n%100==0:writer.writerow(row)
  code=p.wait()
 result=dict(seed=seed,command=command,rows=n,returncode=code,seconds=time.monotonic()-start,endpoint=last)
 print(f'seed {seed}: {n} years, exit {code}',flush=True)
 return result
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:runs=list(pool.map(run,range(1,33)))
(ROOT/'manifest.json').write_text(json.dumps(dict(source='d478d2e',years=16000,seeds=32,sampling='Annual through 1000, then every 100 years; validation every year.',binary_sha256=hashlib.sha256(Path(BIN).read_bytes()).hexdigest(),runs=runs),indent=2)+'\n')
assert all(r['rows']==16000 and r['returncode']==0 for r in runs)
