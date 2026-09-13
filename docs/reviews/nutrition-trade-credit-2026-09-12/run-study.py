import concurrent.futures, gzip, json, subprocess, sys
from pathlib import Path
parent, changed, output = sys.argv[1:]
def run(case):
    label, binary, ordinal, schema, slain = case
    rows = [json.loads(line) for line in subprocess.check_output([binary,str(ordinal),str(schema),str(slain)],text=True).splitlines()]
    return dict(arm=label,ordinal=ordinal,schema=schema,slain=slain,rows=rows)
cases=[(label,binary,ordinal,schema,slain) for ordinal in range(1,33) for slain in (0,1)
       for label,binary,schema in [('parent',parent,97),('legacy',changed,97),('changed',changed,98)]]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results=list(pool.map(run,cases))
for i in range(0,len(results),3):
    assert results[i]['rows']==results[i+1]['rows'],results[i]['ordinal']
with gzip.open(output,'wt') as f: json.dump(results,f,separators=(',',':'))
summary={}
for slain in (0,1):
    summary[str(slain)]={}
    for arm in ('parent','changed'):
        finals=[r['rows'][-1] for r in results if r['arm']==arm and r['slain']==slain]
        summary[str(slain)][arm]={key:sum(r[key] for r in finals)/len(finals) for key in ('population','hunger','abandoned','wheat_overflow','debt','ledger_reserve')}
print(json.dumps(summary,indent=2))
