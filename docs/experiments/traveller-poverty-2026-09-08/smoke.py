import concurrent.futures,csv,json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
BIN='/private/tmp/crownless-poverty-build/crownless_sim_metrics'
def run(seed):
 command=[BIN,'--seed',str(seed),'--years','16000','--final-only','--campaign-metrics'];start=time.monotonic()
 result=subprocess.run(command,capture_output=True,text=True)
 (ROOT/f'seed-{seed}.csv').write_text(result.stdout);(ROOT/f'seed-{seed}.log').write_text(result.stderr)
 print(seed,result.returncode,flush=True)
 return dict(seed=seed,command=command,returncode=result.returncode,seconds=time.monotonic()-start,endpoint=list(csv.DictReader(result.stdout.splitlines())))
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:r=list(pool.map(run,range(1,9)))
(ROOT/'manifest.json').write_text(json.dumps(r,indent=2)+'\n')
assert all(x['returncode']==0 and len(x['endpoint'])==1 for x in r)
