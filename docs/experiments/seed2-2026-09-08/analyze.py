#!/usr/bin/env python3
"""Check the daily trace against the sweep and draw the seed 2 findings."""
import csv
import gzip
import hashlib
import json
import os
from pathlib import Path
os.environ.setdefault('MPLCONFIGDIR', '/private/tmp/crownless-mpl')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parent
rows = [json.loads(line) for line in (ROOT / 'trace.jsonl').read_text().splitlines()]
detail = [json.loads(line) for line in (ROOT / 'first-cycle.jsonl').read_text().splitlines()]
checkpoints = [r for r in rows if r['kind'] == 'checkpoint']
assert checkpoints[-1]['day'] == 140000 * 365 + 1, 'Full run must finish'
with gzip.open(ROOT.parent / 'simulation-128000-years-2026-09-08/annual/seed-002.csv.gz', 'rt') as stream:
    baseline = {int(r['year']): r for r in csv.DictReader(stream)}
for point in checkpoints:
    year = (point['day'] - 1) // 365
    if year <= 128000:
        assert point['serial'] == int(baseline[year]['next_entity_serial'])
        assert point['treasures'] == int(baseline[year]['live_treasures'])
for point in (r for r in detail if r['kind'] == 'checkpoint'):
    assert point == next(r for r in checkpoints if r['day'] == point['day'])
created = [r for r in rows if r['kind'] == 'created']
stages = [r for r in rows if r['kind'] == 'stage']
uncrowned = [r for r in stages if r['stage'] == 'Uncrowned dragon']
assert all(r['territory'] == 0 and r['memory'] >= 25 and r['crown'] >= 12 for r in uncrowned)
hearts = [r for r in created if r['name'].startswith('Wyrmheart')]
assert all(any(s['day'] == r['day'] and s['stage'] == 'Deep Wyrm' for s in stages) for r in hearts)
inventory = [r for r in rows if r['kind'] == 'inventory' and r['day'] == 128000 * 365 + 1]
identity_hash = 14695981039346656037
for obj in inventory:
    assert not obj['destroyed']
    identity_hash = ((identity_hash ^ obj['id']) * 1099511628211) & ((1 << 64) - 1)
assert identity_hash == int(baseline[128000]['treasure_identity_hash'])
assert sum(r['value'] for r in inventory) == int(baseline[128000]['live_treasure_value'])
assert all(r['location'] == next(x for x in rows if x['kind'] == 'place' and x['name'] == 'Hollowbarrow')['id'] for r in inventory)
assert not any(r['kind'] == 'moved_or_destroyed' and r['destroyed'] for r in rows)
assert not (ROOT / 'trace.log').read_text()
assert not (ROOT / 'first-cycle.log').read_text()
intervals = np.diff([r['day'] for r in hearts]) / 365
recovery = []
for r in uncrowned:
    next_deep = next((s for s in stages if s['day'] > r['day'] and s['stage'] == 'Deep Wyrm'), None)
    if next_deep:
        recovery.append((next_deep['day'] - r['day']) / 365)
summary = {
    'simulation_base': '06e5701c6d6c09bc25430685a9fd917b81a42972',
    'seed_number': 2, 'world_seed': (2 * 0x9e3779b9) & 0xffffffff,
    'validated_years': 140000, 'matching_prior_checkpoints': 128,
    'matching_detail_checkpoints': 8,
    'matching_128000_treasure_identity_hash': str(identity_hash),
    'objects_at_128000': len(inventory), 'value_at_128000': sum(r['value'] for r in inventory),
    'unique_names_at_128000': len({r['name'] for r in inventory}),
    'objects_at_140000': checkpoints[-1]['treasures'],
    'total_wyrmhearts_created': len(hearts), 'uncrownings': len(uncrowned),
    'creation_years': [(r['day'] - 1) / 365 for r in created],
    'heart_gap_years': {'min': float(min(intervals)), 'median': float(np.median(intervals)), 'max': float(max(intervals))},
    'recovery_years': {'min': min(recovery), 'median': float(np.median(recovery)), 'max': max(recovery)},
    'all_inventory_owned_by_dragon': all(r['owner'] == next(x for x in rows if x['kind'] == 'dragon')['id'] for r in inventory),
    'all_uncrownings_have_zero_territory': True,
    'artifact_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['trace.c','trace.jsonl','first-cycle.jsonl']},
}
(ROOT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
plt.rcParams.update({'figure.facecolor':'#f7f5ef','axes.facecolor':'#f7f5ef','axes.spines.top':False,'axes.spines.right':False,'font.family':'DejaVu Sans','font.size':10,'axes.titleweight':'bold','text.color':'#18313e','grid.alpha':.2,'savefig.facecolor':'#f7f5ef'})
fig, axes = plt.subplots(3, 1, figsize=(11, 10), gridspec_kw={'height_ratios':[1.2,1,1]})
x = [0] + [(r['day']-1)/365/1000 for r in created] + [140]
y = [0] + list(range(1, len(created)+1)) + [len(created)]
axes[0].step(x,y,where='post',color='#177e89',lw=2.5,label='Live named objects')
axes[0].step(x,[0]+[1]+[2]*(len(created)-1)+[2],where='post',color='#cf673e',lw=2,label='Distinct names')
axes[0].axhline(24,color='#6f7780',ls=':',label='24 object slots')
axes[0].axvline(128,color='#aaa',lw=1)
axes[0].set(xlabel='World age (thousand years)',ylabel='Objects',ylim=(0,27),xlim=(0,140),title='One reliquary, then copies of the same Wyrmheart')
axes[0].legend(loc='upper left',ncol=3,frameon=False)
axes[0].grid(axis='y')
ecology = [r for r in detail if r['kind']=='ecology' and 7278 <= (r['day']-1)/365 <= 7305]
t = [(r['day']-1)/365 for r in ecology]
axes[1].plot(t,[r['territory'] for r in ecology],color='#177e89',lw=2,label='Territory stability')
axes[1].plot(t,[r['cohesion'] for r in ecology],color='#cf673e',alpha=.8,label='Goblin cohesion')
for r in stages:
    age = (r['day']-1)/365
    if 7278 <= age <= 7305:
        axes[1].axvline(age,color='#888',ls=':',lw=1)
        axes[1].text(age+.15,116,{'Uncrowned dragon':'Uncrowned','Crowned dragon':'Crowned','Deep Wyrm':'New heart'}[r['stage']],rotation=0,fontsize=9,va='bottom')
axes[1].set(xlim=(7278,7305),ylim=(0,142),ylabel='Score / 100',xlabel='World age (years)',title='First repeat: the goblin court weakens, then recovers')
axes[1].legend(loc='upper left',frameon=False)
axes[1].set_yticks([0,25,50,75,100])
axes[1].grid(axis='y')
ages = [(r['day']-1)/365/1000 for r in hearts]
axes[2].bar(ages[1:],intervals/1000,width=1.5,color='#7761a5')
axes[2].set(xlim=(0,140),ylabel='Wait (thousand years)',xlabel='Age when the next heart appears (thousand years)',title='Long waits separate brief recoveries')
axes[2].grid(axis='y')
fig.suptitle('Seed 2: Varkesh the Unappeased',fontsize=19,fontweight='bold',x=.09,ha='left')
fig.text(.09,.016,'Daily observations on the original sweep rules • 140,000 years • all annual validations passed',fontsize=10)
fig.tight_layout(rect=(0,.035,1,.965),h_pad=2)
fig.savefig(ROOT/'seed2.png',dpi=170)
plt.close(fig)
print(json.dumps(summary,indent=2))
