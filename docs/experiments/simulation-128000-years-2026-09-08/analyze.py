#!/usr/bin/env python3
"""Chart long histories and evaluate the comparison rule in METHOD.md."""
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
HORIZONS=[1000,2000,4000,8000,16000,32000,64000,128000]
TOLERANCES={'total_population':('relative',.01), 'live_treasure_value':('relative',.01),
            'dragon_hoard':('relative',.01),'average_hunger':('absolute',1),
            'average_prosperity':('absolute',1),'average_legitimacy':('absolute',1),
            'live_treasures':('absolute',.1),'lore_stored':('absolute',.1),
            'active_settlements':('absolute',.1),'closed_routes':('absolute',.1)}
C=['#177e89','#cf673e','#7761a5','#758b3c','#b49632','#476883']
plt.rcParams.update({'figure.facecolor':'#f7f5ef','axes.facecolor':'#f7f5ef',
 'axes.spines.top':False,'axes.spines.right':False,'font.family':'DejaVu Sans',
 'font.size':10,'axes.titleweight':'bold','text.color':'#18313e','grid.alpha':.2,
 'savefig.facecolor':'#f7f5ef'})

def read(path):
    with gzip.open(path,'rt') as f:return list(csv.DictReader(f))

def describe(v):
    a=np.asarray(v,dtype=float)
    return dict(mean=float(a.mean()),median=float(np.median(a)),minimum=float(a.min()),
                maximum=float(a.max()),p10=float(np.percentile(a,10)),p90=float(np.percentile(a,90)))

def main():
    manifest=json.loads((ROOT/'manifest.json').read_text())
    passed=[r for r in manifest['runs'] if r['passed']]
    assert len(manifest['runs'])==manifest['seeds'], 'Run still in progress'
    seeds=[r['seed'] for r in passed]
    annual={s:read(ROOT/'annual'/f'seed-{s:03}.csv.gz') for s in seeds}
    blocks={s:read(ROOT/'blocks'/f'seed-{s:03}.csv.gz') for s in seeds}
    summary={'seeds':seeds,'requested':manifest['seeds'],'passed':len(seeds),'failed':[r for r in manifest['runs'] if not r['passed']],
             'annual_checkpoints':sum(r['annual_rows'] for r in manifest['runs']),
             'gold_constant':sum(r['gold_constant'] for r in passed),'horizons':{},'plateaus':{},
             'coverage_by_horizon':{str(h):sum(r['annual_rows']>=h for r in manifest['runs']) for h in HORIZONS}}
    for h in HORIZONS:
        window=max(1000,h//4)
        detail={'window_years':window,'window_means':{},'endpoints':{}}
        endpoint=[next(r for r in annual[s] if int(r['year'])==h) for s in seeds]
        for k in endpoint[0]:
            if k not in ('seed_number','world_seed','treasure_identity_hash'):
                detail['endpoints'][k]=describe([int(r[k]) for r in endpoint])
        for k in TOLERANCES:
            vals=[np.mean([float(r['mean']) for r in blocks[s] if r['metric']==k and h-window<int(r['end_year'])<=h]) for s in seeds]
            detail['window_means'][k]={'by_seed':dict(zip(map(str,seeds),map(float,vals))),**describe(vals)}
        detail['worlds_with_zero_lore']=sum(int(r['lore_stored'])==0 for r in endpoint)
        detail['worlds_all_roads_closed']=sum(int(r['closed_routes'])==8 for r in endpoint)
        detail['worlds_at_treasure_cap']=sum(int(r['live_treasures'])==24 for r in endpoint)
        detail['worlds_with_recent_surviving_treasure']=sum(int(r['live_treasures'])>0 and int(r['newest_treasure_day'])>(h-1000)*365+1 for r in endpoint)
        summary['horizons'][str(h)]=detail
    for k,(kind,tol) in TOLERANCES.items():
        comparisons=[]
        for older,newer in zip(HORIZONS,HORIZONS[1:]):
            a=summary['horizons'][str(older)]['window_means'][k]
            b=summary['horizons'][str(newer)]['window_means'][k]
            av=np.array(list(a['by_seed'].values()));bv=np.array(list(b['by_seed'].values()))
            delta=bv-av
            scaled=delta/np.maximum(abs(av),1) if kind=='relative' else delta
            mean_change=(b['mean']-a['mean'])/max(abs(a['mean']),1) if kind=='relative' else b['mean']-a['mean']
            fraction=float(np.mean(abs(scaled)<=tol))
            comparisons.append({'from':older,'to':newer,'mean_change':mean_change,'stable_seed_fraction':fraction,'passes':abs(mean_change)<=tol and fraction>=.8,'absolute_mean_change':float(delta.mean())})
        plateau=next((HORIZONS[i] for i in range(len(comparisons)) if all(c['passes'] for c in comparisons[i:])),None)
        summary['plateaus'][k]={'kind':kind,'tolerance':tol,'first_sustained_year':plateau,'comparisons':comparisons}
    years=np.array([int(r['year']) for r in annual[seeds[0]]])
    assert all([int(r['year']) for r in annual[s]]==list(years) for s in seeds)
    def trajectory(ax,k,title,col):
        values=np.array([[int(r[k]) for r in annual[s]] for s in seeds])
        ax.fill_between(years,np.percentile(values,10,axis=0),np.percentile(values,90,axis=0),color=col,alpha=.18)
        ax.plot(years,np.median(values,axis=0),color=col,lw=1.7)
        ax.axvline(1000,color='#777777',ls=':',lw=1)
        ax.set(xscale='log',xlabel='Simulated year · log scale',title=title)
        ax.grid(True)
    fig,axs=plt.subplots(2,3,figsize=(14,8),layout='constrained')
    for ax,k,label,col in zip(axs.flat,['live_treasures','live_treasure_value','treasures_from_ruins','total_population','active_settlements','closed_routes'],['Surviving treasure objects','Total appraised treasure value','Treasure made by ruined towns','Population','Active settlements / 6','Closed routes / 8'],C):trajectory(ax,k,label,col)
    fig.suptitle(f'Crownless | {len(seeds)} completed histories through 128,000 years',fontsize=20)
    fig.supxlabel('Same completed seeds at every age · median and 10th–90th percentile · dotted line marks year 1,000',fontsize=10)
    fig.savefig(ROOT/'long-trajectories.png',dpi=170);plt.close(fig)
    fig,axs=plt.subplots(2,2,figsize=(13,8),layout='constrained')
    for ax,k,label,col in zip([axs[0,0],axs[0,1],axs[1,0]],['average_hunger','dragon_hoard','lore_stored'],['Hunger (0–100)','Dragon hoard in crowns','Stored lore'],C):trajectory(ax,k,label,col)
    ax=axs[1,1]
    by=np.arange(1000,128001,1000)
    changes=np.array([[float(r['changes']) for r in blocks[s] if r['metric']=='treasure_identity_changes'] for s in seeds])
    ax.plot(by,changes.mean(axis=0),color=C[3],label='Mean across seeds')
    ax.fill_between(by,np.percentile(changes,10,axis=0),np.percentile(changes,90,axis=0),color=C[3],alpha=.18)
    ax.set(xscale='log',xlabel='End of 1,000-year block',ylabel='Annual identity changes per block',title='Does treasure keep turning over?');ax.grid(True);ax.legend(frameon=False)
    fig.suptitle('World conditions and treasure turnover',fontsize=20)
    fig.savefig(ROOT/'long-activity.png',dpi=170);plt.close(fig)
    fig,axs=plt.subplots(1,2,figsize=(13,5),layout='constrained')
    for s in seeds:
        vals=[int(r['live_treasures']) for r in annual[s]]
        axs[0].plot(years,vals,lw=2 if s in (1,2,7) else .8,alpha=1 if s in (1,2,7) else .3,
                    color={1:C[1],2:C[0],7:C[2]}.get(s,'#777777'),label=f'Seed {s}' if s in (1,2,7) else None)
    axs[0].axhline(24,color='#333333',ls=':',label='24-object limit')
    axs[0].set(xscale='log',xlabel='Simulated year · log scale',ylabel='Live treasure objects',title='Each completed world');axs[0].legend(frameon=False);axs[0].grid(True)
    coverage=[summary['coverage_by_horizon'][str(h)] for h in HORIZONS]
    bars=axs[1].bar([str(h//1000)+'k' for h in HORIZONS],coverage,color=C[0])
    axs[1].bar_label(bars,padding=3)
    axs[1].set(ylim=(0,manifest['seeds']+4),xlabel='Simulated year',ylabel='Worlds reaching the checkpoint',title='Validation coverage of all requested seeds')
    fig.suptitle('Different histories and the long-run limits',fontsize=19)
    fig.savefig(ROOT/'individual-histories.png',dpi=170);plt.close(fig)
    summary['late_activity']={}
    for k in list(TOLERANCES)+['treasure_identity_changes']:
        records=[r for s in seeds for r in blocks[s] if r['metric']==k and int(r['end_year'])>96000]
        summary['late_activity'][k]={'annual_changes':sum(int(r['changes']) for r in records),'mean_within_block_stddev':float(np.mean([float(r['stddev']) for r in records if r['stddev']])) if k!='treasure_identity_changes' else None,'minimum':min(float(r['minimum']) for r in records),'maximum':max(float(r['maximum']) for r in records)}
    fig,ax=plt.subplots(figsize=(12,6),layout='constrained')
    keys=list(TOLERANCES)
    values=np.array([[c['stable_seed_fraction']*100 for c in summary['plateaus'][k]['comparisons']] for k in keys])
    im=ax.imshow(values,vmin=0,vmax=100,cmap='YlGnBu',aspect='auto')
    ax.set_yticks(range(len(keys)),['Population','Treasure value','Dragon hoard','Hunger','Prosperity','Legitimacy','Live treasure','Stored lore','Active towns','Closed roads'])
    ax.set_xticks(range(7),[f'{a//1000}k → {b//1000}k' for a,b in zip(HORIZONS,HORIZONS[1:])])
    for i in range(len(keys)):
        for j in range(7):ax.text(j,i,f'{values[i,j]:.0f}%',ha='center',va='center',color='white' if values[i,j]>65 else '#18313e')
    fig.colorbar(im,ax=ax,label='Seeds within the chosen tolerance (%)')
    ax.set_title('Where additional history changes less',fontsize=19,pad=16)
    fig.supxlabel('Final-quarter means · at least 1,000 years per window · thresholds in METHOD.md',fontsize=10)
    fig.savefig(ROOT/'plateau-comparisons.png',dpi=170);plt.close(fig)
    (ROOT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v['first_sustained_year'] for k,v in summary['plateaus'].items()},indent=2))

if __name__=='__main__':main()
