#!/usr/bin/env python3
"""Render the paired Age of Dragons study from its published CSV artifacts."""
import argparse
import csv
import gzip
import hashlib
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter, MaxNLocator

INK = '#203641'
PAPER = '#f7f3e9'
TEAL = '#197b7b'
RUST = '#bd583b'
STAGES = ['Egg', 'Whelp', 'Wanderer', 'Crowned', 'Deep wyrm', 'Uncrowned', 'Afterdragon']
STAGE_KEYS = ['dragon_egg_days', 'dragon_whelp_days', 'dragon_wanderer_days',
              'dragon_crowned_days', 'dragon_deep_wyrm_days', 'dragon_uncrowned_days',
              'dragon_afterdragon_days']
STAGE_COLORS = ['#e8ce85', '#b6ba65', '#79a890', '#247579', '#283e59', '#aa7655', '#d7c8ad']


def read(path):
    with gzip.open(path, 'rt') as stream:
        return list(csv.DictReader(stream))


def verify_artifacts(folder, manifest):
    for name in ('endpoints.csv.gz', 'history.csv.gz'):
        actual = hashlib.sha256((folder / name).read_bytes()).hexdigest()
        if actual != manifest['artifact_sha256'][name]:
            raise ValueError(f'Artifact hash mismatch: {folder / name}')


def values(rows, key):
    return np.array([float(r[key]) for r in rows])


def living(rows):
    return [r for r in rows if int(r['total_population']) > 0 and int(r['active_settlements']) > 0]


def pairs(a, b):
    by_seed = {r['seed_number']: r for r in a}
    return [(by_seed[r['seed_number']], r) for r in b if r['seed_number'] in by_seed]


def describe(data):
    return dict(mean=float(np.mean(data)), median=float(np.median(data)),
                p10=float(np.quantile(data, .1)), p90=float(np.quantile(data, .9)),
                minimum=float(np.min(data)), maximum=float(np.max(data))) if len(data) else None


def summary(a, b, manifests):
    result = {'arms': {}, 'paired': {}}
    keys = ['total_population', 'weighted_hunger', 'weighted_prosperity',
            'dragon_campaign_victories', 'lore_stored', 'archive_scribes',
            'years_all_routes_closed', 'active_settlements', 'years_population_weighted_hunger_40_plus']
    for name, rows in [('main', a), ('candidate', b)]:
        observed = living(rows)
        result['arms'][name] = dict(
            passed=len(rows), requested=manifests[name]['seeds'],
            empty_worlds=len(rows) - len(observed),
            final_dragon_slain=sum(int(r['dragon_slain']) for r in rows),
            worlds_with_campaign_victory=sum(int(r['dragon_campaign_victories']) > 0 for r in rows),
            zero_lore=sum(int(r['lore_stored']) == 0 for r in rows),
            zero_scribes=sum(int(r['archive_scribes']) == 0 for r in rows),
            all_roads_closed_at_least_once=sum(int(r['years_all_routes_closed']) > 0 for r in rows),
            final_stage_counts={stage: sum(int(r['dragon_stage']) == i for r in rows) for i, stage in enumerate(STAGES)},
            metrics={k: describe(values(observed if k.startswith('weighted_') else rows, k)) for k in keys})
    common = pairs(a, b)
    result['paired']['worlds'] = len(common)
    result['paired']['metrics'] = {}
    for key in keys:
        usable = [(x, y) for x, y in common if not key.startswith('weighted_') or
                  (int(x['total_population']) > 0 and int(y['total_population']) > 0)]
        delta = np.array([float(y[key]) - float(x[key]) for x, y in usable])
        result['paired']['metrics'][key] = dict(observed=len(delta), changed=int(np.count_nonzero(delta)),
                                                increased=int(np.sum(delta > 0)), decreased=int(np.sum(delta < 0)),
                                                delta=describe(delta))
    ignored = {'schema_version', 'state_hash'}
    result['paired']['worlds_with_any_endpoint_metric_change'] = sum(
        any(x[k] != y[k] for k in x if k not in ignored) for x, y in common)
    return result


def frame(title, subtitle, nrows=2, ncols=3):
    fig, axes = plt.subplots(nrows, ncols, figsize=(18, 11.6), facecolor=PAPER)
    fig.subplots_adjust(left=.065, right=.965, bottom=.10, top=.78, hspace=.48, wspace=.32)
    fig.text(.065, .946, 'CROWNLESS  /  WORLD STUDY', fontsize=12, weight='bold', color=TEAL)
    fig.text(.065, .902, title, fontsize=31, weight='bold', color=INK)
    fig.text(.065, .866, subtitle, fontsize=12, color=INK)
    return fig, axes.ravel()


def save(fig, folder, name, footer):
    fig.text(.065, .035, footer, fontsize=10, color=INK, linespacing=1.5)
    fig.savefig(folder / f'{name}.png', dpi=150, facecolor=PAPER)
    fig.savefig(folder / f'{name}.svg', facecolor=PAPER)
    svg = folder / f'{name}.svg'
    svg.write_text('\n'.join(line.rstrip() for line in svg.read_text().splitlines()) + '\n')
    plt.close(fig)


def scatter(ax, x, y, color, label, cmap='viridis'):
    dots = ax.scatter(x, y, c=color, s=14, alpha=.58, edgecolors='none', cmap=cmap, rasterized=True)
    bar = ax.figure.colorbar(dots, ax=ax, pad=.025, fraction=.045)
    bar.set_label(label, fontsize=9)
    return dots


def hero(rows, manifest, folder):
    observed = living(rows)
    years = manifest['years']
    total_days = years * 365
    fig, ax = frame('Life in the Age of Dragons',
                    f"Archive delivery candidate · {len(rows):,}/{manifest['seeds']:,} valid worlds · {years:,} years per world · same starting seed set as main")
    cards = [(f'{np.median(values(rows, "total_population")):,.0f}', 'median final population'),
             (f'{np.mean(values(observed, "weighted_hunger")):.1f} / 100', 'mean final hunger among residents'),
             (f'{sum(int(r["dragon_campaign_victories"]) > 0 for r in rows):,}', 'worlds with a campaign victory'),
             (f'{sum(int(r["lore_stored"]) == 0 for r in rows):,}', 'worlds ending with zero stored lore')]
    for x, (number, label) in zip([.065, .295, .525, .755], cards):
        fig.text(x, .825, number, fontsize=20, weight='bold', color=RUST)
        fig.text(x, .803, label, fontsize=9, color=INK)
    scatter(ax[0], 100-values(observed, 'weighted_prosperity'), values(observed, 'dragon_campaign_victories'),
            values(observed, 'weighted_hunger'), 'Resident hunger')
    ax[0].set(title='Dragon victories and poverty', xlabel='Poverty proxy: 100 − resident prosperity', ylabel='Campaign victories over the whole run')
    ax[0].yaxis.set_major_locator(MaxNLocator(integer=True))
    scatter(ax[1], values(observed, 'weighted_hunger'), values(observed, 'total_population'),
            values(observed, 'active_settlements'), 'Inhabited towns', cmap='cividis')
    ax[1].set(title='The people who remain', xlabel='Final population-weighted hunger', ylabel='Final population')
    stages = np.array([values(rows, key) for key in STAGE_KEYS]) / total_days * 100
    if not np.allclose(stages.sum(axis=0), 100):
        raise ValueError('Dragon stage days must cover the complete run')
    order = np.argsort(stages[3] + stages[4], kind='stable')
    ax[2].stackplot(np.arange(1, len(rows)+1), stages[:, order], colors=STAGE_COLORS, labels=STAGES)
    ax[2].set(title='Time under each dragon stage', xlabel='World rank by crowned + deep wyrm exposure', ylabel='Share of simulated days (%)', ylim=(0, 100), xlim=(1, max(2, len(rows))))
    ax[2].legend(ncol=4, fontsize=7, loc='upper center', bbox_to_anchor=(.5, -.24), frameon=False)
    scatter(ax[3], values(rows, 'days_bandit_influence_70_plus') / total_days * 100,
            values(rows, 'years_population_weighted_hunger_40_plus') / years * 100,
            values(rows, 'total_population'), 'Final population')
    ax[3].set(title='Bandit power and lasting hunger', xlabel='Days with any bandit group influence ≥70 (%)', ylabel='Annual resident hunger ≥40 (%)')
    scatter(ax[4], values(observed, 'lore_stored'), values(observed, 'weighted_prosperity'),
            values(observed, 'archive_scribes'), 'Archive scribes', cmap='cividis')
    ax[4].set(title='Can knowledge survive?', xlabel='Stored lore at the final year', ylabel='Final population-weighted prosperity')
    ax[4].xaxis.set_major_locator(MaxNLocator(integer=True))
    scatter(ax[5], values(rows, 'years_all_routes_closed') / years * 100, values(rows, 'total_population'),
            values(rows, 'dragon_afterdragon_days') / total_days * 100, 'Afterdragon days (%)')
    ax[5].set(title='Life after the roads close', xlabel='Annual checkpoints with every road closed (%)', ylabel='Final population')
    save(fig, folder, 'life-in-the-age-of-dragons',
         f"Candidate {manifest['source'][:12]} · Each point is one world. Poverty is a prosperity proxy. Hunger and prosperity use population weights.\n"
         "Integer counts form real horizontal bands. Bandit exposure counts days with any qualifying group. These are associations across generated worlds.")


def histories(arms, manifests, folder):
    fig, axes = frame('How the age unfolds', 'Median world and the middle 80% of worlds · early years shown in detail · main and archive delivery candidate')
    metrics = [('total_population', 'Population', 'People'), ('weighted_hunger', 'Hunger among residents', 'Hunger / 100'),
               ('weighted_prosperity', 'Prosperity among residents', 'Prosperity / 100'),
               ('closed_routes', 'Roads closed', 'Road count'), ('lore_stored', 'Knowledge held in the archive', 'Stored lore'),
               ('dragon_campaign_victories', 'Campaign victories accumulate', 'Victories since world creation')]
    for ax, (key, title, unit) in zip(axes, metrics):
        for name, color, style in [('main', RUST, '--'), ('candidate', TEAL, '-')]:
            rows = living(arms[name]) if key.startswith('weighted_') else arms[name]
            by_year = {}
            for row in rows:
                by_year.setdefault(int(row['year']), []).append(float(row[key]))
            years = sorted(by_year)
            q = np.array([np.quantile(by_year[y], [.1, .5, .9]) for y in years])
            ax.fill_between(years, q[:, 0], q[:, 2], color=color, alpha=.10)
            ax.plot(years, q[:, 1], color=color, linestyle=style, linewidth=2, label=name)
        ax.set(title=title, xlabel='Year (log scale)', ylabel=unit, xscale='log')
        ax.set_xticks([1, 10, 100, manifests['candidate']['years']])
        ax.xaxis.set_major_formatter(FuncFormatter(lambda x, _: f'{x:,.0f}'))
    axes[0].legend(frameon=False)
    save(fig, folder, 'how-the-age-unfolds',
         "Bands show the 10th–90th percentiles across worlds. Both arms use the same seeds with successful full runs.\n"
         f"Main {manifests['main']['source'][:12]} · Candidate {manifests['candidate']['source'][:12]} · Annual validation; years 1–10, then every 25 years retained.")


def comparison(a, b, manifests, folder):
    common = pairs(a, b)
    fig, axes = frame('What the pending changes shift', f'{len(common):,} paired worlds · identical world seeds · points on the diagonal have the same final value')
    metrics = [('total_population', 'Final population'), ('weighted_hunger', 'Resident hunger'),
               ('weighted_prosperity', 'Resident prosperity'), ('lore_stored', 'Stored lore'),
               ('archive_scribes', 'Archive scribes'), ('dragon_campaign_victories', 'Campaign victories')]
    for ax, (key, title) in zip(axes, metrics):
        usable = [(x, y) for x, y in common if not key.startswith('weighted_') or
                  (int(x['total_population']) > 0 and int(y['total_population']) > 0)]
        x = np.array([float(p[0][key]) for p in usable])
        y = np.array([float(p[1][key]) for p in usable])
        low, high = min(x.min(), y.min()), max(x.max(), y.max())
        margin = max((high-low)*.05, 1)
        ax.plot([low-margin, high+margin], [low-margin, high+margin], color=RUST, linewidth=1, linestyle='--', zorder=1)
        ax.scatter(x, y, s=17, color=TEAL, alpha=.5, edgecolors='none', zorder=2, rasterized=True)
        ax.set(title=f'{title} · {np.count_nonzero(x != y):,} changed', xlabel='Main', ylabel='Archive delivery candidate',
               xlim=(low-margin, high+margin), ylim=(low-margin, high+margin))
        if key in ('lore_stored', 'archive_scribes', 'dragon_campaign_victories'):
            ax.xaxis.set_major_locator(MaxNLocator(integer=True))
            ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    save(fig, folder, 'paired-worlds',
         f"Main {manifests['main']['source'][:12]} · Candidate {manifests['candidate']['source'][:12]} · Each plot uses the same metric on both axes.\n"
         "This comparison measures the complete pending stack. It combines archive funding, supply booking, dispatch, and shared freight changes.")


def archive_probe(folder):
    manifest = json.loads((folder / 'archive-probe-manifest.json').read_text())
    arms = {}
    for name in ('main', 'candidate'):
        path = folder / name / 'archive-probe.csv'
        if hashlib.sha256(path.read_bytes()).hexdigest() != manifest['arms'][name]['csv_sha256']:
            raise ValueError(f'Archive probe hash mismatch: {path}')
        with path.open() as stream:
            arms[name] = list(csv.DictReader(stream))
    fig, axes = frame('The archive competes for its own purse',
                      '32 paired worlds · first 100 years · weekly observations after each update · shared ledger funds staffing and supply purchases',
                      nrows=1, ncols=3)
    fig.set_size_inches(18, 7)
    fig.texts[0].set_y(.955)
    fig.texts[1].set_y(.887)
    fig.texts[2].set_y(.835)
    fig.subplots_adjust(bottom=.25, top=.70)
    keys = ['scribes', 'binding', 'tools', 'grain', 'paper', 'ready']
    labels = ['No scribes', 'Binding materials', 'Tools', 'Spare grain', 'Paper', 'Ready']
    colors = [RUST, '#a27e57', '#d7b76c', '#94a37a', '#75aaa3', TEAL]
    bottom = np.zeros(2)
    for key, label, color in zip(keys, labels, colors):
        shares = np.array([manifest['arms'][name]['blocker_percent'][key] for name in arms])
        axes[0].bar(['Main', 'Candidate'], shares, bottom=bottom, label=label, color=color, width=.6)
        for i, share in enumerate(shares):
            if share > 5:
                axes[0].text(i, bottom[i] + share/2, f'{share:.1f}%', ha='center', va='center', color='white', weight='bold')
        bottom += shares
    axes[0].set(title='The first reported blocker', ylabel='Weekly observations (%)', ylim=(0, 100))
    axes[0].legend(ncol=2, loc='upper center', bbox_to_anchor=(.5, -.1), frameon=False, fontsize=9)
    for name, color, style in [('main', RUST, '--'), ('candidate', TEAL, '-')]:
        years = list(range(1, 101))
        yearly = [[r for r in arms[name] if int(r['year']) == y] for y in years]
        low = [100 * sum(int(r['ledger_below_50']) for r in rows) / sum(int(r['samples']) for r in rows) for rows in yearly]
        lore = [np.mean(values(rows, 'lore_stored')) for rows in yearly]
        axes[1].plot(years, low, color=color, linestyle=style, linewidth=2, label=name)
        axes[2].plot(years, lore, color=color, linestyle=style, linewidth=2, label=name)
    axes[1].set(title='Ledger below the staffing threshold', xlabel='Year', ylabel='Weekly observations with reserve <50 (%)')
    axes[2].set(title='Knowledge retained', xlabel='Year', ylabel='Mean stored lore per world')
    axes[1].legend(frameon=False)
    save(fig, folder, 'archive-funding-pressure',
         'Each arm has 166,848 weekly observations; 448 sampled state hashes match its main sweep. Blockers follow the query’s priority order.\n'
         'The funding link is a hypothesis supported by these observations and the spending rules. A budget separation experiment can test it.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'text.color': INK,
                         'axes.labelcolor': INK, 'axes.titleweight': 'bold', 'axes.titlesize': 13,
                         'axes.spines.top': False, 'axes.spines.right': False,
                         'axes.facecolor': PAPER, 'axes.edgecolor': '#c8c8ba',
                         'xtick.color': INK, 'ytick.color': INK, 'grid.color': '#dedbcf',
                         'axes.grid': True, 'grid.alpha': .5, 'axes.axisbelow': True,
                         'svg.hashsalt': 'crownless-age-of-dragons'})
    manifests = {name: json.loads((args.folder / name / 'manifest.json').read_text()) for name in ('main', 'candidate')}
    if any(manifests['main'][k] != manifests['candidate'][k] for k in ('seeds', 'years', 'seed_mapping')):
        raise ValueError('Both arms must use the same seed set and duration')
    for name, manifest in manifests.items():
        verify_artifacts(args.folder / name, manifest)
    ends = {name: read(args.folder / name / 'endpoints.csv.gz') for name in manifests}
    histories_data = {name: read(args.folder / name / 'history.csv.gz') for name in manifests}
    common_seeds = {r['seed_number'] for r in ends['main']} & {r['seed_number'] for r in ends['candidate']}
    histories_data = {name: [r for r in rows if r['seed_number'] in common_seeds]
                      for name, rows in histories_data.items()}
    result = summary(ends['main'], ends['candidate'], manifests)
    (args.folder / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    hero(ends['candidate'], manifests['candidate'], args.folder)
    histories(histories_data, manifests, args.folder)
    comparison(ends['main'], ends['candidate'], manifests, args.folder)
    if (args.folder / 'archive-probe-manifest.json').exists():
        archive_probe(args.folder)


if __name__ == '__main__':
    main()
