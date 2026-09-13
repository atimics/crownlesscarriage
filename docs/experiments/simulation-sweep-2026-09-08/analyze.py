#!/usr/bin/env python3
"""Build charts and summary statistics from the frozen sweep outputs."""
import csv
import gzip
import json
import os
from pathlib import Path
os.environ.setdefault('MPLCONFIGDIR', '/private/tmp/crownless-mpl')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parent

def read(name):
    path = ROOT / name
    opener = gzip.open if name.endswith('.gz') else open
    with opener(path, 'rt') as f:
        rows = list(csv.DictReader(f))
    return {k: np.array([int(r[k]) for r in rows]) for k in rows[0]}

def desc(x):
    return dict(mean=float(np.mean(x)), median=float(np.median(x)),
                p10=float(np.percentile(x,10)), p90=float(np.percentile(x,90)),
                minimum=float(np.min(x)), maximum=float(np.max(x)))

plt.rcParams.update({'figure.facecolor':'#f7f5ef','axes.facecolor':'#f7f5ef',
                     'axes.spines.top':False, 'axes.spines.right':False,
                     'font.family':'DejaVu Sans','font.size':10,
                     'axes.titleweight':'bold','axes.labelcolor':'#303944',
                     'text.color':'#172b39','axes.edgecolor':'#a5adb3',
                     'grid.alpha':0.2,'savefig.facecolor':'#f7f5ef'})
C=['#177e89','#d3683b','#7659a4','#527d3c','#b1932d','#445a83']
a=read('annual.csv.gz'); e=read('endpoints.csv')
years=np.unique(a['year']); seeds=np.unique(a['seed_number']); n=len(seeds)
assert len(a['year']) == n * 1000
assert all(np.array_equal(a['year'][a['seed_number']==s], years) for s in seeds)
summary={'worlds':n,'annual_rows':len(a['year']), 'endpoints':{k:desc(v) for k,v in e.items()},'years':{}}
metrics=['total_population','average_prosperity','average_hunger','average_legitimacy','active_settlements','closed_routes']
fig,axs=plt.subplots(2,3,figsize=(14,8),layout='constrained')
for ax,k,col,label in zip(axs.flat,metrics,C,['Population','Prosperity (0–100)','Hunger (0–100; higher means worse)','Legitimacy (0–100)','Active settlements','Closed routes']):
    values=a[k].reshape(n,1000)
    ax.fill_between(years,np.percentile(values,10,axis=0),np.percentile(values,90,axis=0),color=col,alpha=.18,label='10th–90th percentile')
    ax.plot(years,np.median(values,axis=0),color=col,lw=1.7,label='Median')
    ax.set(xscale='log',xlabel='Simulated year · log scale',title=label)
    ax.grid(True)
axs[0,0].legend(frameon=False,fontsize=8)
fig.suptitle(f'Crownless | {n} worlds over 1,000 years',fontsize=20)
fig.savefig(ROOT/'world-trajectories.png',dpi=170); plt.close(fig)
for y in (1,10,25,50,100,250,500,1000):
    mask=a['year']==y
    summary['years'][str(y)]={k:desc(v[mask]) for k,v in a.items() if k not in ('seed_number','world_seed','year')}
    summary['years'][str(y)]['worlds_with_abandonment']=int(np.sum(a['abandoned_settlements'][mask]>0))

summary['gold_constant_worlds']=int(sum(np.ptp(a['tracked_gold'][a['seed_number']==s])==0 for s in seeds))
summary['ever_abandoned_worlds']=int(sum(e['years_with_abandoned_settlement']>0))
summary['ever_all_routes_closed_worlds']=int(sum(e['years_all_routes_closed']>0))
summary['dragon_survived_worlds']=int(sum(e['dragon_slain']==0))
summary['recovered_after_abandonment']=int(sum((e['minimum_active_settlements']<6)&(e['active_settlements']==6)))

fig,axs=plt.subplots(2,2,figsize=(13,8),layout='constrained')
ax=axs[0,0]
for key,label,col in [('years_hunger_40_plus','Hunger ≥40',C[1]),('years_hunger_60_plus','Hunger ≥60',C[2]),('years_with_abandoned_settlement','Any abandoned town',C[0]),('years_all_routes_closed','All routes closed',C[5])]:
    x=np.sort(e[key]/10)
    ax.plot(x,np.arange(1,n+1)/n*100,label=label,color=col,lw=2)
ax.set(xlabel='Share of annual checkpoints (%)',ylabel='Cumulative share of worlds (%)',title='How long hardship lasts',xlim=(0,100),ylim=(0,100)); ax.legend(frameon=False,fontsize=8)
ax=axs[0,1]
for key,label,col in [('dragon_hoard','Dragon hoard',C[2]),('market_coins','Town markets',C[0]),('iron_ledger_reserve','Monastery reserve',C[4])]:
    vals=(a[key]/a['tracked_gold']*100).reshape(n,1000)
    ax.plot(years,vals.mean(axis=0),label=label,color=col,lw=2)
ax.set(xscale='log',xlabel='Simulated year · log scale',ylabel='Mean share of tracked crowns (%)',title='Where the money gathers'); ax.legend(frameon=False,fontsize=8)
ax=axs[1,0]
for alive,label,col in [(True,'Dragon alive',C[2]),(False,'Dragon slain',C[1])]:
    mask=(e['dragon_slain']==0)==alive
    ax.scatter(e['average_prosperity'][mask],e['average_hunger'][mask],s=26,alpha=.7,color=col,label=f'{label} ({sum(mask)})')
ax.set(xlabel='Prosperity at year 1,000',ylabel='Hunger at year 1,000',title='Different endings'); ax.legend(frameon=False,fontsize=8)
ax=axs[1,1]
for key,label,col in [('lore_stored','Stored lore',C[0]),('lore_ceiling','Lore ceiling',C[2]),('archive_stewardship','Stewardship',C[4])]:
    ax.plot(years,a[key].reshape(n,1000).mean(axis=0),label=label,color=col,lw=2)
ax.set(xscale='log',xlabel='Simulated year · log scale',ylabel='Mean reported value',title='The archive through time'); ax.legend(frameon=False,fontsize=8)
for ax in axs.flat: ax.grid(True)
fig.suptitle('World health | hardship, wealth and memory',fontsize=20)
fig.savefig(ROOT/'world-health.png',dpi=170); plt.close(fig)

summary['agent']={}
fig,axs=plt.subplots(2,3,figsize=(14,8),layout='constrained')
rng=np.random.default_rng(20260908)
for row,y in enumerate((10,100)):
    d=read(f'agent-{y}.csv'); nn=len(d['seed']); info={'pairs':nn,'metrics':{},'totals':{}}
    for k in ('population','prosperity','hunger','active_settlements','closed_routes'):
        delta=d['agent_'+k]-d['control_'+k]
        bootstrap=np.mean(delta[rng.integers(0,nn,size=(10000,nn))],axis=1)
        info['metrics'][k]={**desc(delta),'mean_ci95':np.percentile(bootstrap,[2.5,97.5]).tolist(),'positive':int(sum(delta>0)),'zero':int(sum(delta==0)),'negative':int(sum(delta<0))}
    for k in ('repairs','repair_failures','travel_attempts','travel_successes','jobs_accepted','jobs_completed','jobs_resolved','jobs_expired','jobs_abandoned','jobs_unresolved','combats_initiated','combats_won','combats_lost','objective_pass','objective_loss'):
        info['totals'][k]=int(sum(d[k])); info[k]=desc(d[k])
    summary['agent'][str(y)]=info
    for col,k,label in [(0,'population','Population change'),(1,'hunger','Hunger change (lower is better)')]:
        delta=d['agent_'+k]-d['control_'+k]; ax=axs[row,col]
        ax.hist(delta,bins=20,color=C[col],alpha=.85,edgecolor='#f7f5ef')
        ax.axvline(0,color='#263746',lw=1); ax.axvline(delta.mean(),color=C[2],ls='--',label=f'Mean {delta.mean():+.1f}')
        ax.set(title=f'{y} years · {label}',xlabel='Agent minus paired control',ylabel='World pairs'); ax.legend(frameon=False,fontsize=8)
    ax=axs[row,2]; keys=['jobs_completed','jobs_expired','jobs_abandoned','jobs_unresolved']
    counts=[info['totals'][k] for k in keys]
    bars=ax.bar(['Completed','Expired','Abandoned','Unresolved'],counts,color=[C[0],C[1],C[2],C[4]])
    ax.bar_label(bars,padding=3,fontsize=9)
    ax.set(title=f'{y} years · Reported job counters',ylabel='Counts across pairs · categories overlap'); ax.tick_params(axis='x',labelsize=8)
fig.suptitle('Road steward | 128 paired worlds at each horizon',fontsize=20)
fig.savefig(ROOT/'agent-effects.png',dpi=170); plt.close(fig)
(ROOT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k not in ('years','endpoints','agent')},indent=2))
