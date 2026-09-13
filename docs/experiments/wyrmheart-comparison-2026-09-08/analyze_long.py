#!/usr/bin/env python3
"""Compare every annual seed 2 row through 128,000 years."""
import csv
import gzip
import hashlib
import itertools
import json
import os
from pathlib import Path
os.environ.setdefault('MPLCONFIGDIR','/private/tmp/crownless-mpl')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT=Path(__file__).resolve().parent
m=json.loads((ROOT/'long-manifest.json').read_text())
assert len(m['runs'])==2
assert all(r['returncode']==0 and r['rows']==128000 for r in m['runs'])
for run in m['runs']:
    assert hashlib.sha256((ROOT/run['file']).read_bytes()).hexdigest()==run['sha256']
    assert not (ROOT/f"{run['label']}-002-128000.log").read_text()
for label in ['before','after']:
    with gzip.open(ROOT/f'{label}-002.csv.gz','rb') as short, gzip.open(ROOT/f'{label}-002-128000.csv.gz','rb') as long:
        short_hash=hashlib.sha256(short.read()).hexdigest()
        prefix_hash=hashlib.sha256(b''.join(itertools.islice(long,16001))).hexdigest()
        assert short_hash==prefix_hash
metrics=['live_treasures','live_treasure_value','total_population','average_hunger','average_prosperity',
         'dragon_hunts','dragon_broods','dragon_deep_wyrm_days','dragon_uncrowned_days','dragon_campaign_victories']
first=None
changed=0
samples=[]
milestones={}
with gzip.open(ROOT/'before-002-128000.csv.gz','rt') as bf, gzip.open(ROOT/'after-002-128000.csv.gz','rt') as af:
    for year,(b,a) in enumerate(itertools.zip_longest(csv.DictReader(bf),csv.DictReader(af)),1):
        assert b is not None and a is not None and int(b['year'])==year and int(a['year'])==year
        if b!=a:
            changed+=1
            if first is None:first={'year':year,'columns':{k:{'before':b[k],'after':a[k]} for k in b if b[k]!=a[k]}}
        if year%100==0 or year==1:samples.append({'year':year,'before':{k:int(b[k]) for k in metrics},'after':{k:int(a[k]) for k in metrics}})
        if year in [1000,8000,16000,32000,64000,128000]:
            milestones[str(year)]={k:{'before':int(b[k]),'after':int(a[k])} for k in metrics}
assert year==128000
summary={'seed':2,'years':year,'annual_rows_changed':changed,'first_difference':first,'milestones':milestones,
         'before':m['before'],'after':m['after'],'validated_years':256000,'both_16000_year_prefixes_match':True}
(ROOT/'long-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
plt.rcParams.update({'figure.facecolor':'#f7f5ef','axes.facecolor':'#f7f5ef','axes.spines.top':False,'axes.spines.right':False,
 'font.family':'DejaVu Sans','font.size':11,'axes.titleweight':'bold','text.color':'#18313e','grid.alpha':.2,'savefig.facecolor':'#f7f5ef'})
fig,ax=plt.subplots(2,1,figsize=(10,6),sharex=True)
for axis,metric,title in zip(ax,['live_treasures','total_population'],['Surviving named objects','Population']):
    for label,color,ls in [('before','#b46b42','-'),('after','#177e89','--')]:
        axis.plot([r['year']/1000 for r in samples],[r[label][metric] for r in samples],color=color,ls=ls,lw=2,label=label.title())
    axis.set(title=title);axis.grid(axis='y');axis.legend(frameon=False)
ax[-1].set_xlabel('World age (thousand years)')
fig.suptitle('Seed 2: matched histories through 128,000 years',fontsize=17,fontweight='bold')
fig.tight_layout(rect=(0,0,1,.95))
fig.savefig(ROOT/'seed2-long.png',dpi=160)
print(json.dumps(summary,indent=2))
