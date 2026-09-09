"""Verify published traces, cargo accounting, and observer parity."""
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path

root=Path(__file__).parent
rows=json.loads((root/'results.json').read_text())
reference=json.loads((root.parent/'archive-volume-rules-2026-09-08/parity.json').read_text())
expected={r['ordinal']:r['hashes'] for r in reference['worlds']}
assert len(rows)==96
assert {(r['arm'],r['ordinal']) for r in rows}=={(arm,n) for arm in ('protected','unprotected','paused') for n in range(1,33)}
matched=0
for row in rows:
    path=root/row['trace'];data=path.read_bytes()
    assert hashlib.sha256(data).hexdigest()==row['trace_sha256']
    if row['exit_code']:
        continue
    assert row['failed_year']==0 and len(row['annual_hashes'])==100
    events=list(csv.DictReader(io.StringIO(gzip.decompress(data).decode())))
    purchases={e['shipment_id']:e for e in events if e['kind']=='purchase'}
    assert len(purchases)==row['purchases']
    assert sum(int(e['charge']) for e in purchases.values())==row['spent']
    assert all(int(e['reserve'])>=int(e['charge'])>0 for e in purchases.values())
    seen=set()
    for event in events:
        if event['kind'] in ('delivered','redirected','lost'):
            key=event['shipment_id'];assert key in purchases and key not in seen
            assert int(event['day'])>=int(purchases[key]['day'])
            assert event['units']==purchases[key]['units']
            seen.add(key)
    assert sum(int(e['units']) for key,e in purchases.items() if key not in seen)==row['unresolved_units']
    assert sum(row['ordered'])==sum(row['delivered'])+sum(row['redirected'])+sum(row['lost'])+row['unresolved_units']
    if row['arm']=='protected':
        assert row['annual_hashes']==expected[row['ordinal']]
        matched+=len(row['annual_hashes'])
    if row['arm']=='paused':assert row['purchases']==0
assert matched==3200
report={'runs':len(rows),'failures':sum(r['exit_code']!=0 for r in rows),'observer_parity_annual_hashes':matched,'trace_hashes_verified':len(rows),'cargo_accounting':'ordered = delivered + redirected + lost + unresolved'}
(root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

index={(r['arm'],r['ordinal']):r for r in rows}
paired={}
for first,second in [('protected','unprotected'),('paused','protected')]:
    paired[first+'_versus_'+second]={}
    for key in ('zero_staff','ready','staff_sum','lore_sum','end_lore'):
        deltas=[index[first,n][key]-index[second,n][key] for n in range(1,33)]
        paired[first+'_versus_'+second][key]={'higher':sum(d>0 for d in deltas),'equal':sum(d==0 for d in deltas),'lower':sum(d<0 for d in deltas),'total_delta':sum(deltas)}
(root/'paired.json').write_text(json.dumps(paired,indent=2)+'\n')
