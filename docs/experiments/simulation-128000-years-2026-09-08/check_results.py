#!/usr/bin/env python3
"""Verify saved long-run results against the earlier sweep and independent runs."""
import csv
import gzip
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parent
manifest=json.loads((root/'manifest.json').read_text())
assert len(manifest['runs'])==manifest['seeds']
for name,digest in manifest['data_hashes'].items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest,name
baseline=root.parent/'simulation-sweep-2026-09-08'/'annual.csv.gz'
with gzip.open(baseline,'rt') as f:
    old={}
    for r in csv.DictReader(f):
        s=int(r['seed_number'])
        if s<=manifest['seeds']:old[(s,int(r['year']))]=r
checks=[]
for run in manifest['runs']:
    s=run['seed']
    with gzip.open(root/'annual'/f'seed-{s:03}.csv.gz','rt') as f:rows=list(csv.DictReader(f))
    assert all(None not in r and None not in r.values() for r in rows)
    assert all(0<=int(r['live_treasures'])<=int(r['treasure_count'])<=24 for r in rows)
    for r in rows:
        if int(r['year'])>1000:continue
        prior=old[(s,int(r['year']))]
        assert {k:r[k] for k in prior}==prior,(s,r['year'])
    with gzip.open(root/'blocks'/f'seed-{s:03}.csv.gz','rt') as f:blocks=list(csv.DictReader(f))
    assert len(blocks)==run['annual_rows']//1000*21
    checks.append({'seed':s,'baseline_first_1000_years_match':True,'treasure_bounds_pass':True,
                   'sampled_rows':len(rows),'complete_blocks':len(blocks)//21,'passed_to_128000':run['passed']})
(root/'data-checks.json').write_text(json.dumps(checks,indent=2)+'\n')
print(f'Hashes and first 1000 years match for all {len(checks)} seeds.')
