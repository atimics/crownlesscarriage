#!/usr/bin/env python3
"""Compare equal cash aid and organised grain freight, with a paired road shock."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seeds', type=int, default=32)
    parser.add_argument('--workers', type=int, default=6)
    parser.add_argument('--reuse-data', action='store_true', help='Redraw the saved daily records')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    def run(item):
        seed, age = item
        return item, subprocess.run([str(args.binary.resolve()), str(seed), str(age)], text=True, capture_output=True)
    rows, failures = [], []
    if args.reuse_data:
        with gzip.open(args.output / 'daily.csv.gz', 'rt') as stream:
            rows = list(csv.DictReader(stream))
        saved = json.loads((args.output / 'results.json').read_text())
        for path, digest in saved['source_sha256'].items():
            if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
                raise ValueError('Simulation source changed; run fresh experiments before redrawing.')
        failures = saved['failures']
        args.seeds = saved['requested_per_age']
    else:
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            for (seed, age), result in pool.map(run, [(s, a) for a in (0, 1000) for s in range(1, args.seeds + 1)]):
                if result.returncode:
                    failures.append({'seed': seed, 'age': age, 'error': result.stderr.strip()})
                    (args.output / f'failed-{age}-{seed}.txt').write_text(result.stdout + result.stderr)
                else:
                    rows.extend(csv.DictReader(io.StringIO(result.stdout)))
    if not rows:
        raise RuntimeError(failures)
    with gzip.open(args.output / 'daily.csv.gz', 'wt', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    data = {(int(r['age']), int(r['seed']), r['arm'], int(r['day'])): r for r in rows}
    seeds = {a: sorted({int(r['seed']) for r in rows if int(r['age']) == a}) for a in (0, 1000)}
    def values(age, arm, field):
        return np.array([[float(data[age, s, arm, d][field]) for d in range(366)] for s in seeds[age]])
    colors = {'cash_open': '#8a929e', 'fund_open': '#247b82', 'cash_cut': '#b4a394', 'fund_cut': '#c36e39'}
    names = {'cash_open': '200 crowns to market', 'fund_open': '200 crowns to organiser',
             'cash_cut': 'Market aid, broken road', 'fund_cut': 'Organiser, broken road'}
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'axes.spines.top': False, 'axes.spines.right': False})
    fig, axes = plt.subplots(2, 2, figsize=(13, 9), layout='constrained')
    summary = {}
    for col, age in enumerate((0, 1000)):
        for arm in colors:
            if not seeds[age]: continue
            style = '--' if arm.endswith('cut') else '-'
            axes[0, col].plot(values(age, arm, 'hunger').mean(axis=0), style, color=colors[arm], label=names[arm])
            axes[1, col].plot(values(age, arm, 'nutrition').mean(axis=0), style, color=colors[arm])
        axes[0, col].set(title=f'World age {age:,} years · {len(seeds[age])} paired worlds', ylabel='Mean hunger (lower is better)', ylim=(0, 40))
        axes[1, col].set(xlabel='Days after funding', ylabel='Mean nutrition eaten')
    axes[0, 0].legend(frameon=False, fontsize=8)
    fig.suptitle('Does an organiser turn the same cash into better food supply?')
    fig.savefig(args.output / 'food-and-hunger.png', dpi=150); plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout='constrained')
    for age, marker in ((0, 'o'), (1000, '^')):
        if not seeds[age]: continue
        for arm in ('fund_open', 'fund_cut'):
            x, y = values(age, arm, 'ordered')[:, -1], values(age, arm, 'delivered')[:, -1]
            axes[0].scatter(x, y, marker=marker, color=colors[arm], alpha=.6, label=f'{names[arm]}, age {age}')
        base = values(age, 'cash_open', 'hunger')
        gain = (values(age, 'fund_open', 'hunger') - base).mean(axis=1)
        axes[1].scatter(values(age, 'fund_open', 'delivered')[:, -1], gain, marker=marker, label=f'Age {age} years', alpha=.7)
        summary[age] = {'completed_seeds': seeds[age], 'arms': {}}
        for arm in ('fund_open', 'fund_cut'):
            control = arm.replace('fund', 'cash')
            deliveries = values(age, arm, 'delivered')
            resumed = 0
            for idx, seed in enumerate(seeds[age]):
                reopen = int(data[age, seed, arm, 0]['cut_day']) + 42
                resumed += bool(deliveries[idx, -1] > deliveries[idx, min(reopen, 365)])
            summary[age]['arms'][arm] = {
                'mean_ordered': float(values(age, arm, 'ordered')[:, -1].mean()),
                'mean_delivered': float(deliveries[:, -1].mean()),
                'mean_lost': float(values(age, arm, 'lost')[:, -1].mean()),
                'mean_redirected': float(values(age, arm, 'redirected')[:, -1].mean()),
                'mean_spent': float(values(age, arm, 'spent')[:, -1].mean()),
                'worlds_with_deliveries': int((deliveries[:, -1] > 0).sum()),
                'worlds_with_arrivals_after_repair_day': int(resumed),
                'mean_hunger_change_vs_cash': float((values(age, arm, 'hunger') - values(age, control, 'hunger')).mean()),
                'mean_extra_nutrition_vs_cash': float((values(age, arm, 'nutrition') - values(age, control, 'nutrition'))[:, -1].mean()),
                'mean_bread_made': float(values(age, arm, 'bread_made')[:, -1].mean()),
                'mean_food_aged': float(values(age, arm, 'food_aged')[:, -1].mean()),
                'mean_food_overflow': float(values(age, arm, 'food_overflow')[:, -1].mean())}
    axes[0].set(xlabel='Wheat bought', ylabel='Wheat delivered', title='Paid loads face real travel and loss')
    axes[0].legend(frameon=False, fontsize=7)
    axes[1].axhline(0, color='#999999', linewidth=.7)
    axes[1].set(xlabel='Wheat delivered by organiser', ylabel='Mean hunger change versus equal market aid', title='More delivery can have different town effects')
    axes[1].legend(frameon=False)
    fig.savefig(args.output / 'delivery-outcomes.png', dpi=150); plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout='constrained')
    for col, age in enumerate((0, 1000)):
        if not seeds[age]: continue
        for arm in ('fund_open', 'fund_cut'):
            aligned = []
            for seed in seeds[age]:
                start = int(data[age, seed, arm, 0]['cut_day'])
                zero = float(data[age, seed, arm, start-1]['delivered'])
                aligned.append([float(data[age, seed, arm, start+d]['delivered'])-zero for d in range(121)])
            axes[col].plot(np.mean(aligned, axis=0), label=names[arm], color=colors[arm])
        axes[col].axvspan(0, 41, color='#c36e39', alpha=.12, label='42-day road break')
        axes[col].set(title=f'World age {age:,} years', xlabel='Days after the selected route breaks', ylabel='Wheat arriving after the break')
    axes[0].legend(frameon=False, fontsize=8)
    fig.suptitle('Delivery histories aligned to each world’s road interruption')
    fig.savefig(args.output / 'road-interruption.png', dpi=150); plt.close(fig)
    result = {'requested_per_age': args.seeds, 'follow_up_days': 365, 'failures': failures,
              'rows': len(rows), 'ages': summary,
              'source_sha256': {p: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['src/sim/cc_sim.c', 'src/sim/cc_grain_supply.inc', 'src/sim/cc_sim.h', 'tools/grain_experiment.c']}}
    (args.output / 'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
