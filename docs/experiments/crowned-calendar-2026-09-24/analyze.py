"""Compare Crown Age histories with the preceding recovery rules."""
import argparse
import csv
import importlib.util
import json
import os
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).parents[1] / 'long-history-recovery-2026-09-24' / 'analyze.py'
    spec = importlib.util.spec_from_file_location('history_summary', source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    data, provenance, summaries = {}, {}, []
    for version, root in [('before', args.before), ('after', args.after)]:
        run = json.loads((root / 'run.json').read_text())
        outcomes = json.loads((root / 'outcomes.json').read_text())
        assert len(outcomes) == len(run['raw_world_seeds']) and all(r['exit_code'] == 0 for r in outcomes)
        provenance[version] = {'run': run, 'outcomes': outcomes}
        data[version] = {}
        for seed in run['raw_world_seeds']:
            summary, metrics, extra, _ = module.summarize(root, seed, run['years_per_world'])
            with (root / f'seed-{seed}-events.tsv').open() as handle:
                events = list(csv.DictReader(handle, delimiter='\t'))
            summary.update(version=version, state_hash=metrics[-1]['state_hash'],
                crown_sightings=int(extra[-1]['crown_sightings']) if 'crown_sightings' in extra[-1] else None,
                crown_almanacs=int(extra[-1]['crown_editions']) if 'crown_editions' in extra[-1] else None,
                calendar_events=[e for e in events if e['kind'] in ['crown-sighting', 'crown-almanac'] or
                                 (e['kind'] == 'dragon-stage' and e['text'] == 'Deep Wyrm')])
            summaries.append(summary)
            data[version][seed] = (summary, metrics)
    assert provenance['before']['run']['raw_world_seeds'] == provenance['after']['run']['raw_world_seeds']
    assert provenance['before']['run']['years_per_world'] == provenance['after']['run']['years_per_world']
    (args.output / 'summary.json').write_text(json.dumps(summaries, indent=2) + '\n')
    (args.output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    with (args.output / 'summary.csv').open('w', newline='') as handle:
        writer = csv.DictWriter(handle, fieldnames=[k for k in summaries[0] if k != 'calendar_events'])
        writer.writeheader()
        writer.writerows({k: v for k, v in r.items() if k != 'calendar_events'} for r in summaries)

    os.environ.setdefault('MPLCONFIGDIR', '/private/tmp/crownless-recovery-matplotlib')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.colors import ListedColormap, BoundaryNorm
    from matplotlib.patches import Patch
    colors = ['#e8cf9d', '#e29c52', '#c76b46', '#7a944d', '#187c89', '#92556b', '#cad0d8']
    names = ['Egg', 'Whelp', 'Wanderer', 'Crowned', 'Deep Wyrm', 'Uncrowned', 'Afterdragon']
    seeds = list(data['after'])
    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    for ax, version, title in zip(axes, ['before', 'after'], ['Earlier growth rules', 'Rarer Deep Wyrm epochs']):
        stages = [[int(r['dragon_stage']) for r in data[version][s][1]] for s in seeds]
        ax.imshow(stages, aspect='auto', interpolation='nearest', origin='lower',
            extent=(0, len(stages[0]) - 1, -.5, len(seeds) - .5), cmap=ListedColormap(colors),
            norm=BoundaryNorm([i - .5 for i in range(8)], 7))
        count = sum(data[version][s][0]['true_ages'] for s in seeds)
        for index, seed in enumerate(seeds):
            changes = data[version][seed][0]['calendar_events']
            days = [int(e['day']) / 364 for e in changes if e['kind'] == 'dragon-stage']
            ax.scatter(days, [index] * len(days), marker='o', s=26, facecolors='white', edgecolors='#124d55', linewidths=1.2)
        ax.set(title=f'{title} · {count} Deep Wyrm epochs', ylabel='World seed',
               yticks=range(len(seeds)), yticklabels=[str(s) for s in seeds])
    axes[1].set_xlabel('Elapsed solar years (364 days) · circles mark Deep Wyrm changes')
    fig.legend(handles=[Patch(color=c, label=n) for c, n in zip(colors, names)],
               loc='lower center', ncol=7, frameon=False)
    fig.suptitle('Crown Ages and Deep Wyrm epochs · six 3,000-year histories', fontsize=16)
    fig.tight_layout(rect=(0, .065, 1, .96))
    fig.savefig(args.output / 'dragon-epochs.png', dpi=160, facecolor='white')
    fig.savefig(args.output / 'dragon-epochs.svg', facecolor='white')
    for version in data:
        print(version, {k: sum(data[version][s][0][k] or 0 for s in seeds) for k in
            ['true_ages', 'dragon_successors', 'crown_sightings', 'crown_almanacs', 'empty_goblin_raids']})


if __name__ == '__main__':
    main()
