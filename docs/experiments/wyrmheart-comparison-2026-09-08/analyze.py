#!/usr/bin/env python3
"""Summarize matched histories and plot direct and wider effects."""
import csv
import gzip
import json
import os
from pathlib import Path
os.environ.setdefault('MPLCONFIGDIR','/private/tmp/crownless-mpl')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT=Path(__file__).resolve().parent
METRICS=['live_treasures','live_treasure_value','total_population','average_hunger',
         'average_prosperity','active_settlements','closed_routes','lore_stored',
         'dragon_hoard','dragon_hunts','dragon_broods','dragon_campaign_victories',
         'dragon_deep_wyrm_days','dragon_uncrowned_days','dragon_slain']
m=json.loads((ROOT/'manifest.json').read_text())
assert len(m['runs'])==16
assert all(r['returncode']==0 and r['rows']==m['years'] for r in m['runs'])
data={}
for run in m['runs']:
    with gzip.open(ROOT/run['file'],'rt') as stream:
        data[run['label'],run['seed']]=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(stream)]
    assert len(data[run['label'],run['seed']])==16000
    assert not (ROOT/f"{run['label']}-{run['seed']:03}.log").read_text()
summary={'seeds':m['seeds'],'years':m['years'],'validated_years':sum(r['rows'] for r in m['runs']),
         'before':m['before'],'after':m['after'],'cohort':{},'paired':{},'seed2_milestones':{}}
for metric in METRICS:
    before=np.array([data['before',s][-1][metric] for s in m['seeds']],dtype=float)
    after=np.array([data['after',s][-1][metric] for s in m['seeds']],dtype=float)
    summary['cohort'][metric]={'before_mean':float(before.mean()),'after_mean':float(after.mean()),
        'difference_mean':float((after-before).mean()),'pairs_changed':int(np.count_nonzero(after-before)),
        'relative_mean_change_percent':float(100*(after.mean()/before.mean()-1)) if before.mean() else None}
for seed in m['seeds']:
    before,after=data['before',seed],data['after',seed]
    first=next((i+1 for i,(b,a) in enumerate(zip(before,after)) if b!=a),None)
    summary['paired'][str(seed)]={'first_differing_annual_row':first,
        'first_changed_columns':[k for k in before[first-1] if before[first-1][k]!=after[first-1][k]] if first else [],
        'endpoint':{k:{'before':before[-1][k],'after':after[-1][k]} for k in METRICS}}
for y in [1000,2000,4000,8000,16000]:
    summary['seed2_milestones'][str(y)]={k:{label:data[label,2][y-1][k] for label in ['before','after']} for k in METRICS}
(ROOT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
plt.rcParams.update({'figure.facecolor':'#f7f5ef','axes.facecolor':'#f7f5ef','axes.spines.top':False,
 'axes.spines.right':False,'font.family':'DejaVu Sans','font.size':10,'axes.titleweight':'bold',
 'text.color':'#18313e','grid.alpha':.2,'savefig.facecolor':'#f7f5ef'})
fig,ax=plt.subplots(2,2,figsize=(12,8))
colors={'before':'#b46b42','after':'#177e89'}
for label in ['before','after']:
    points=data[label,2]
    ax[0,0].plot(np.arange(1,16001)/1000,[r['live_treasures'] for r in points],label=label.title(),color=colors[label],lw=2)
ax[0,0].set(title='Seed 2: surviving named objects',xlabel='World age (thousand years)',ylabel='Objects')
ax[0,0].legend(frameon=False)
x=np.arange(8)
for label,offset in [('before',-.18),('after',.18)]:
    ax[0,1].bar(x+offset,[data[label,s][-1]['live_treasures'] for s in m['seeds']],width=.36,color=colors[label],label=label.title())
ax[0,1].set(xticks=x,xticklabels=m['seeds'],xlabel='Seed',ylabel='Objects',title='All eight pairs at year 16,000')
ax[0,1].legend(frameon=False)
pop=[100*(data['after',s][-1]['total_population']/data['before',s][-1]['total_population']-1) for s in m['seeds']]
ax[1,0].bar(x,pop,color=['#177e89' if d>=0 else '#b46b42' for d in pop])
ax[1,0].set(xticks=x,xticklabels=m['seeds'],xlabel='Seed',ylabel='Population change (%)',title='Wider effects vary by world')
ax[1,0].axhline(0,color='#777',lw=.8)
shares=[100*(data['after',s][-1]['dragon_deep_wyrm_days']-data['before',s][-1]['dragon_deep_wyrm_days'])/(16000*365) for s in m['seeds']]
ax[1,1].bar(x,shares,color=['#177e89' if d>=0 else '#b46b42' for d in shares])
ax[1,1].set(xticks=x,xticklabels=m['seeds'],xlabel='Seed',ylabel='Change in share of days (points)',title='Time spent as a deep wyrm')
ax[1,1].axhline(0,color='#777',lw=.8)
for a in ax.flat:a.grid(axis='y')
fig.suptitle('What the Wyrmheart rule changed',fontsize=20,fontweight='bold',x=.08,ha='left')
fig.text(.08,.02,'Seeds 1–8 • matched world rules • 16,000 years each • 256,000 annual validations passed',fontsize=10)
fig.tight_layout(rect=(0,.045,1,.95),h_pad=2,w_pad=2)
fig.savefig(ROOT/'comparison.png',dpi=160)
print(json.dumps(summary['cohort'],indent=2))
print('Seed 2:',json.dumps(summary['paired']['2'],indent=2))
