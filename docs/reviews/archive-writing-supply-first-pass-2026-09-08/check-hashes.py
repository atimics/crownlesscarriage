import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).parent

def run(job):
    ordinal,schema=job
    result=subprocess.run([sys.argv[1],str(ordinal),'100',str(schema)],capture_output=True,text=True,check=True)
    return json.loads(result.stdout)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    rows=list(pool.map(run,[(n,s) for s in (86,0) for n in range(1,33)]))
(root/'hash-runs.json').write_text(json.dumps(rows,indent=2)+'\n')
print('64 world hash runs passed.')
