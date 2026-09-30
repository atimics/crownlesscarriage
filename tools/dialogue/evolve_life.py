"""Evolve the shared daily-life brain (L1) against welfare in real simulated worlds.

The brain scores the legal options at the simulation's decision hook: where to
travel, whether to buy a meal or a bed, whether to join a bandit camp. Zero weights
reproduce the simulation's own rule exactly, so evolution starts from the rule and
every change is a departure the search found. Fitness is the welfare of the
road-going people (scouts, travellers, refugees, couriers) over a season, averaged
over worlds; the same worlds are used for every candidate in a generation.
"""
import argparse
import json
from multiprocessing import get_context
from pathlib import Path
import time

import numpy as np

import fastworld

START_DAY = 30    # let the world settle before the policy takes over
SEASON = 180      # days scored for L1
WEIGHTS = {'hungry': 1.0, 'unsheltered': 0.5, 'bandit': 1.0}
SETTLEMENTS = 6   # towns per world (docs/npc-census.json)


def rates(metrics):
    days = max(1.0, metrics['road_days'])
    return {'hungry': metrics['road_hungry'] / days, 'unsheltered': metrics['road_unsheltered'] / days,
            'bandit': metrics['road_bandit'] / days, 'stress': metrics['road_stress'] / days,
            'moves': metrics['road_moves'] / max(1.0, days / SEASON),
            'coins': metrics['road_coins'] / max(1.0, days / SEASON)}


def welfare(metrics):
    r = rates(metrics)
    return -sum(WEIGHTS[k] * r[k] for k in WEIGHTS)


def gossip_rates(metrics):
    """Coverage (share of towns that know the average story), accuracy (mean confidence of the
    version each town holds), and their product, how well informed the world is."""
    pairs, story_days = max(1.0, metrics['gossip_pairs']), max(1.0, metrics['story_days'])
    coverage = metrics['gossip_pairs'] / (story_days * SETTLEMENTS)
    accuracy = metrics['gossip_confidence'] / pairs / 100.0
    return {'informed': coverage * accuracy, 'coverage': coverage, 'accuracy': accuracy,
            'retellings': metrics['gossip_retellings'] / pairs}


def town_rates(metrics):
    """Hunger and famine across towns, and how prosperous they are."""
    days = max(1.0, metrics['town_days'])
    return {'hunger': metrics['town_hunger'] / days, 'famine': metrics['town_famine'] / days,
            'prosperity': metrics['town_prosperity'] / days}


def town_welfare(metrics):
    r = town_rates(metrics)
    return -r['hunger'] / 100.0 - r['famine'] + 0.2 * r['prosperity'] / 100.0


def raid_rates(metrics):
    """What raiders take, and what it costs the towns."""
    towns = town_rates(metrics)
    return {'loot': metrics['loot'], 'raids': metrics['raids'], 'prosperity': towns['prosperity'],
            'famine': towns['famine']}


def raid_welfare(metrics, weight=1.0):
    # One unit of loot and `weight` points of prosperity count the same: weight 1 is an even split,
    # 0 is a raider-only reward and large weights favour the towns. `l4:W` selects the weight.
    r = raid_rates(metrics)
    return r['loot'] / 100.0 + weight * r['prosperity'] / 100.0


def realm_rates(metrics):
    """How stable the kingdoms are and how their towns fare."""
    days = max(1.0, metrics['kingdom_days'])
    towns = town_rates(metrics)
    return {'legitimacy': metrics['kingdom_legitimacy'] / days, 'treasury': metrics['kingdom_treasury'] / days,
            'hunger': towns['hunger'], 'famine': towns['famine']}


def realm_welfare(metrics):
    r = realm_rates(metrics)
    return r['legitimacy'] / 100.0 - r['hunger'] / 100.0 - r['famine']


# Each stage: which decisions the brain takes, when scoring starts, how long, and what is scored.
STAGES = {
    'l1': {'mask': fastworld.LEARNED, 'start': START_DAY, 'days': SEASON, 'rates': rates, 'welfare': welfare,
           'keys': ('hungry', 'unsheltered', 'bandit', 'stress', 'moves'), 'label': 'welfare'},
    'l2': {'mask': fastworld.GOSSIP, 'start': START_DAY, 'days': 365, 'rates': gossip_rates,
           'welfare': lambda m: gossip_rates(m)['informed'],
           'keys': ('informed', 'coverage', 'accuracy', 'retellings'), 'label': 'informed'},
    'l5': {'mask': fastworld.KINGDOM_RELIEF, 'start': START_DAY, 'days': 730, 'rates': realm_rates,
           'welfare': realm_welfare, 'keys': ('legitimacy', 'treasury', 'hunger', 'famine'), 'label': 'realm welfare'},
    'l4': {'mask': fastworld.RAID, 'start': START_DAY, 'days': 730, 'rates': raid_rates, 'welfare': raid_welfare,
           'keys': ('loot', 'raids', 'prosperity', 'famine'), 'label': 'raid balance'},
    'l3': {'mask': fastworld.TRADE, 'start': START_DAY, 'days': 730, 'rates': town_rates, 'welfare': town_welfare,
           'keys': ('hunger', 'famine', 'prosperity'), 'label': 'town welfare'},
}


def stage_cfg(spec):
    """A stage by name, or `l4:W` for the raid stage with prosperity weight W."""
    name, _, param = spec.partition(':')
    cfg = dict(STAGES[name])
    if name == 'l4' and param:
        weight = float(param)
        cfg['welfare'] = lambda m: raid_welfare(m, weight)
        cfg['label'] = f'raid balance (prosperity x {weight:g})'
    return cfg


def play(theta, seed, stage='l1'):
    """One world under a weight vector (None = the simulation's rule); returns its metrics."""
    cfg = stage_cfg(stage)
    fastworld.set_policy(theta, cfg['mask'])
    try:
        world = fastworld.FastWorld.new(seed, cfg['start'])
        try:
            return fastworld.run_days(world, cfg['days'])
        finally:
            world.close()
    finally:
        fastworld.set_policy(None)


def _fitness(job):
    theta, seeds, stage = job
    cfg = stage_cfg(stage)
    return float(np.mean([cfg['welfare'](play(theta, s, stage)) for s in seeds]))


def _metrics(job):
    theta, seeds, stage = job
    return [play(theta, s, stage) for s in seeds]


def ranks(values):
    order = np.argsort(np.argsort(values))
    return order / (len(values) - 1) - 0.5


def chunks(items, pieces):
    return [items[k::pieces] for k in range(pieces) if items[k::pieces]]


def paired(rule, other, name, stage='l1'):
    """Differences other - rule per world for each rate, with standard errors."""
    cfg = stage_cfg(stage)
    lines = []
    for key in cfg['keys']:
        d = np.array([cfg['rates'](b)[key] - cfg['rates'](a)[key] for a, b in zip(rule, other)])
        se = d.std(ddof=1) / np.sqrt(len(d)) if len(d) > 1 else 0.0
        lines.append(f"{name} vs rule {key:12} {d.mean():+.4f} +- {se:.4f} (z={d.mean() / se if se else 0:.1f}); "
                     f"lower in {int((d < 0).sum())}, higher in {int((d > 0).sum())} of {len(d)}")
    d = np.array([cfg['welfare'](b) - cfg['welfare'](a) for a, b in zip(rule, other)])
    se = d.std(ddof=1) / np.sqrt(len(d))
    lines.append(f"{name} vs rule {cfg['label']:12} {d.mean():+.4f} +- {se:.4f} (z={d.mean() / se:.1f}); "
                 f"better in {int((d > 0).sum())}, worse in {int((d < 0).sum())} of {len(d)}")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--stage', default='l1', help='l1..l5, or l4:W for the raid stage with prosperity weight W')
    parser.add_argument('--generations', type=int, default=150)
    parser.add_argument('--pairs', type=int, default=32)
    parser.add_argument('--batch', type=int, default=24, help='worlds per generation')
    parser.add_argument('--sigma', type=float, default=0.1)
    parser.add_argument('--lr', type=float, default=0.05)
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--workers', type=int, default=10)
    parser.add_argument('--fresh', type=int, default=0, help='also evaluate the best weights on this many fresh worlds (seeds 201+)')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('use a fresh output directory')
    args.output.mkdir(parents=True)
    size = fastworld.policy_size()
    train, valid, test = list(range(1, 61)), list(range(61, 91)), list(range(101, 201))
    rng = np.random.default_rng(args.seed)
    theta = np.zeros(size)          # the rule
    m, v = np.zeros(size), np.zeros(size)
    log = (args.output / 'log.jsonl').open('w')
    ctx = get_context('spawn')
    with ctx.Pool(args.workers) as pool:
        def mean_welfare(th, seeds):
            parts = pool.map(_fitness, [(th, c, args.stage) for c in chunks(seeds, args.workers)])
            return float(np.average(parts, weights=[len(c) for c in chunks(seeds, args.workers)]))
        rule_valid = mean_welfare(None, valid)
        best, best_valid, started = theta.copy(), rule_valid, time.time()
        print(json.dumps({'rule_validation_welfare': rule_valid, 'weights': size}), flush=True)
        for gen in range(1, args.generations + 1):
            batch = rng.choice(train, size=args.batch, replace=False).tolist()
            eps = rng.normal(0, 1, (args.pairs, size))
            cands = [theta + args.sigma * e for e in eps] + [theta - args.sigma * e for e in eps]
            fit = np.array(pool.map(_fitness, [(c, batch, args.stage) for c in cands], chunksize=1))
            shaped = ranks(fit)
            grad = ((shaped[:args.pairs] - shaped[args.pairs:])[:, None] * eps).sum(0) / (2 * args.pairs * args.sigma)
            m = 0.9 * m + 0.1 * grad
            v = 0.999 * v + 0.001 * grad * grad
            theta = theta + args.lr * (m / (1 - 0.9 ** gen)) / (np.sqrt(v / (1 - 0.999 ** gen)) + 1e-8)
            row = {'generation': gen, 'mean_fitness': float(fit.mean()), 'best_fitness': float(fit.max()),
                   'seconds': round(time.time() - started, 1)}
            if gen % 5 == 0 or gen == args.generations:
                row['validation'] = mean_welfare(theta, valid)
                if row['validation'] > best_valid:
                    best_valid, best = row['validation'], theta.copy()
            log.write(json.dumps(row) + '\n'); log.flush()
            print(json.dumps(row), flush=True)
        np.save(args.output / 'theta_best_validation.npy', best)
        np.save(args.output / 'theta_last.npy', theta)
        rule = [r for c in pool.map(_metrics, [(None, c, args.stage) for c in chunks(test, args.workers)]) for r in c]
        found = [r for c in pool.map(_metrics, [(best, c, args.stage) for c in chunks(test, args.workers)]) for r in c]
        # chunks() interleaves seeds, so both lists are in the same interleaved order and pair up
        for name, rows in (('rule', rule), ('evolved', found)):
            cfg = stage_cfg(args.stage)
            r = {k: np.mean([cfg['rates'](x)[k] for x in rows]) for k in cfg['keys']}
            print(name, json.dumps({k: round(float(x), 4) for k, x in r.items()}),
                  cfg['label'], round(float(np.mean([cfg['welfare'](x) for x in rows])), 4))
        print('\n'.join(paired(rule, found, 'evolved', args.stage)))
        if args.fresh:
            cfg = stage_cfg(args.stage)
            fresh = list(range(201, 201 + args.fresh))
            rule_f = [r for c in pool.map(_metrics, [(None, c, args.stage) for c in chunks(fresh, args.workers)]) for r in c]
            found_f = [r for c in pool.map(_metrics, [(best, c, args.stage) for c in chunks(fresh, args.workers)]) for r in c]
            print('FRESH', args.fresh)
            print('\n'.join(paired(rule_f, found_f, 'evolved', args.stage)))
            summary = {'stage': args.stage, 'worlds': len(fresh),
                       'rule': {k: float(np.mean([cfg['rates'](x)[k] for x in rule_f])) for k in cfg['keys']},
                       'evolved': {k: float(np.mean([cfg['rates'](x)[k] for x in found_f])) for k in cfg['keys']}}
            (args.output / 'result.json').write_text(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
