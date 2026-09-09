#!/usr/bin/env python3
"""Plot tracked species and cult activity from the read-only species sweep."""
import argparse
import csv
import gzip
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter

SPECIES = ['dragon', 'human', 'goblin', 'pony', 'cow', 'sheep']
LABELS = ['Dragon', 'Human', 'Goblin', 'Pony', 'Cow', 'Sheep']
COLORS = ['#a64b3c', '#3d7189', '#537c45', '#987541', '#716188', '#438981']
PAPER, INK = '#faf7ef', '#243841'


def read(path):
    with gzip.open(path, 'rt') as stream:
        return list(csv.DictReader(stream))


def values(rows, key):
    return np.array([float(r[key]) for r in rows])


def finish(fig, folder, name, note):
    fig.text(.035, .025, note, fontsize=10, color=INK)
    fig.savefig(folder / (name + '.png'), dpi=170, facecolor=PAPER)
    fig.savefig(folder / (name + '.svg'), facecolor=PAPER)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('folder', type=Path)
    args = ap.parse_args()
    folder = args.folder
    manifest = json.loads((folder / 'manifest.json').read_text())
    ends, history = read(folder / 'endpoints.csv.gz'), read(folder / 'history.csv.gz')
    if not ends:
        raise ValueError('A completed world is needed for these charts')
    years = sorted({int(r['year']) for r in history})
    by_year = {y: [r for r in history if int(r['year']) == y] for y in years}
    n, horizon = len(ends), manifest['years']
    caption = f'{n:,} completed worlds of {manifest["seeds"]:,} requested. {horizon:,} years each. Source {manifest["source"]}.'
    plt.rcParams.update({'figure.facecolor': PAPER, 'axes.facecolor': PAPER, 'text.color': INK,
        'axes.labelcolor': INK, 'xtick.color': INK, 'ytick.color': INK, 'axes.edgecolor': '#b8bcb6',
        'font.size': 11, 'axes.spines.top': False, 'axes.spines.right': False, 'svg.fonttype': 'none'})
    summary = {'completed': n, 'requested': manifest['seeds'], 'years': horizon, 'species': {}}
    fig, axes = plt.subplots(2, 3, figsize=(15, 9))
    fig.subplots_adjust(top=.82, bottom=.14, hspace=.43, wspace=.28)
    fig.suptitle('Life in the age of dragons: six species', x=.035, ha='left', y=.965, fontsize=24, weight='bold')
    fig.text(.035, .895, 'Population through time. Each panel has its own population scale.\nLines show medians and bands the middle 80%; the dragon panel shows the share of worlds with a living focal dragon.', fontsize=11)
    scope = ['Living focal dragon; eggs counted separately', 'Town residents', 'Members of the one tracked cult', 'Common herds plus seven named ponies', 'Adults and calves in town herds', 'Adults and lambs in town flocks']
    for ax, key, label, color, meaning in zip(axes.flat, SPECIES, LABELS, COLORS, scope):
        qs = np.array([np.quantile(values(by_year[y], key), [.1, .5, .9]) for y in years])
        if key == 'dragon':
            ax.plot(years, [values(by_year[y], key).mean() * 100 for y in years], color=color, lw=2.5)
            ax.set_ylim(0, 105); ax.set_ylabel('Worlds with a living dragon (%)')
        else:
            ax.fill_between(years, qs[:, 0], qs[:, 2], color=color, alpha=.18)
            ax.plot(years, qs[:, 1], color=color, lw=2.3)
            ax.set_ylim(bottom=0); ax.set_ylabel('Count')
            ax.yaxis.set_major_formatter(FuncFormatter(lambda x, _p: f'{x:,.0f}'))
        ax.set_title(label, loc='left', fontsize=17, weight='bold', color=color, pad=25)
        ax.text(0, 1.035, meaning, transform=ax.transAxes, fontsize=9)
        ax.set_xlabel('Years'); ax.grid(axis='y', alpha=.16)
        start, end = values(by_year[0], key), values(ends, key)
        summary['species'][key] = dict(start_median=float(np.median(start)), end_median=float(np.median(end)),
            end_mean=float(end.mean()), end_min=float(end.min()), end_max=float(end.max()),
            zero_worlds=int((end == 0).sum()))
    finish(fig, folder, 'six-species', caption + '\nThe count scopes differ. Named ponies are a persistent cast; dispersed dragon whelps leave the tracked population.')

    days = horizon * 365
    total_raids = values(ends, 'raids').sum()
    total_empty = values(ends, 'empty_raids').sum()
    after = values(ends, 'afterdragon_days').sum()
    active = values(ends, 'active_days').sum()
    summary['goblins'] = dict(raids=int(total_raids), empty_raids=int(total_empty), empty_raid_percent=float(100 * total_empty / total_raids) if total_raids else None,
        tribute_deliveries=int(values(ends, 'tribute_events').sum()), rallies=int(values(ends, 'rallies').sum()),
        recruits=int(values(ends, 'recruits').sum()), offering_events=int(values(ends, 'offerings').sum()),
        egg_rituals=int(values(ends, 'seeds').sum()), worlds_with_egg_ritual=int((values(ends, 'seeds') > 0).sum()),
        activity_day_percent=float(100 * active / (n * days)),
        floor_day_percent=float(100 * values(ends, 'floor_days').sum() / (n * days)),
        divided_day_percent=float(100 * values(ends, 'divided_days').sum() / (n * days)),
        afterdragon_days=int(after), saturated_days=int(values(ends, 'saturated_days').sum()))
    for key in ['hunger_days', 'equipment_days', 'tribute_days']:
        summary['goblins'][key + '_percent_of_active'] = float(100 * values(ends, key).sum() / active) if active else None
    blockers = ['members', 'devotion', 'cohesion', 'food', 'coins', 'relics', 'tools', 'weapons']
    summary['ritual_blocked_percent_afterdragon_days'] = {
        k: float(100 * values(ends, 'blocked_' + k).sum() / after) if after else None for k in blockers}
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    fig.subplots_adjust(top=.82, bottom=.13, hspace=.52, wspace=.27)
    fig.suptitle('Goblins and cults: activity behind the tribute count', x=.035, ha='left', y=.965, fontsize=23, weight='bold')
    fig.text(.035, .90, 'Daily observation counts raids, recruitment, offerings, and egg rituals as they happen.\nA tribute delivery is one part of the cult’s work.', fontsize=12)
    ax = axes[0, 0]
    rates_raids = [values(by_year[y], 'raids').mean() for y in years]
    rates_tributes = [values(by_year[y], 'tribute_events').mean() for y in years]
    spans = np.diff(years)
    ax.plot(years[1:], np.diff(rates_raids) / spans, color=COLORS[2], lw=2, label='Raids on towns')
    ax.plot(years[1:], np.diff(rates_tributes) / spans, color=COLORS[0], lw=2, label='Tributes delivered')
    ax.set_title('Raids remain visible across the age', loc='left', weight='bold')
    ax.set_xlabel('Years'); ax.set_ylabel('Mean events per world per year'); ax.legend(frameon=False)
    ax = axes[0, 1]
    empty_pct = np.divide(values(ends, 'empty_raids') * 100, values(ends, 'raids'), out=np.zeros(n), where=values(ends, 'raids') > 0)
    scatter = ax.scatter(values(ends, 'raids') / horizon, values(ends, 'tribute_events') / horizon,
        c=empty_pct, cmap='YlOrRd', s=14, alpha=.65, vmin=0, vmax=100)
    ax.set_title('Tributes cover a small part of raiding', loc='left', weight='bold')
    ax.set_xlabel('Raids per year'); ax.set_ylabel('Tribute deliveries per year')
    fig.colorbar(scatter, ax=ax, label='Raids taking no goods, coins, or treasure (%)')
    ax = axes[1, 0]
    keys = ['prepared', 'raids', 'tribute_events', 'rallies', 'offerings', 'seeds']
    labels = ['Raid musters', 'Raids', 'Tributes', 'Recruitment', 'Offering work', 'Egg rituals']
    heights = [values(ends, k).sum() / n for k in keys]
    ax.barh(labels[::-1], heights[::-1], color=COLORS[2]); ax.set_xscale('symlog', linthresh=1)
    ax.set_title(f'Cult work over {horizon:,} years', loc='left', weight='bold')
    ax.set_xlabel('Mean events per world (linear to 1, then log scale)')
    ax = axes[1, 1]
    percentages = [summary['ritual_blocked_percent_afterdragon_days'][k] or 0 for k in blockers]
    ax.barh([k.capitalize() for k in blockers][::-1], percentages[::-1], color=COLORS[0])
    ax.set_xlim(0, 100); ax.set_title('What keeps a dead dragon’s cult from a ritual?', loc='left', weight='bold', fontsize=11)
    ax.set_xlabel('Afterdragon days with this unmet requirement (%)')
    for ax in axes.flat: ax.grid(axis='y', alpha=.12)
    finish(fig, folder, 'goblins-and-cults', caption + '\nRitual requirements overlap. These bars cover all days with a slain dragon, including time before the ritual can begin.')

    fig, axes = plt.subplots(2, 3, figsize=(15, 8))
    fig.subplots_adjust(top=.83, bottom=.14, hspace=.4, wspace=.28)
    fig.suptitle(f'Species populations at year {horizon:,}', x=.035, ha='left', y=.965, fontsize=24, weight='bold')
    fig.text(.035, .9, 'Endpoint distributions across completed worlds. Dashed lines mark the median starting population.', fontsize=12)
    for ax, key, label, color in zip(axes.flat, SPECIES, LABELS, COLORS):
        end = values(ends, key)
        bins = [-.5, .5, 1.5] if key == 'dragon' else min(35, max(1, int(end.max() - end.min()) + 1))
        ax.hist(end, bins=bins, color=color, alpha=.85)
        ax.axvline(summary['species'][key]['start_median'], color=INK, ls='--', lw=1.5)
        ax.set_title(label, loc='left', color=color, weight='bold', fontsize=16)
        if key == 'dragon': ax.set_xticks([0, 1])
        if key == 'human': ax.xaxis.set_major_formatter(FuncFormatter(lambda x, _p: f'{x / 1000:g}k'))
        ax.set_xlabel('Count'); ax.set_ylabel('Worlds'); ax.grid(axis='y', alpha=.12)
    finish(fig, folder, 'species-distributions', caption + '\nPony totals include seven named ponies. Common-herd extinction is reported separately in the study.')
    summary['common_pony_extinction_worlds'] = int((values(ends, 'common_ponies') == 0).sum())
    (folder / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')


if __name__ == '__main__':
    main()
