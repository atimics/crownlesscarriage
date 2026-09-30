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
SEASON = 180      # days scored
WEIGHTS = {'hungry': 1.0, 'unsheltered': 0.5, 'bandit': 1.0}


def rates(metrics):
    days = max(1.0, metrics['road_days'])
    return {'hungry': metrics['road_hungry'] / days, 'unsheltered': metrics['road_unsheltered'] / days,
            'bandit': metrics['road_bandit'] / days, 'stress': metrics['road_stress'] / days,
            'moves': metrics['road_moves'] / max(1.0, days / SEASON),
            'coins': metrics['road_coins'] / max(1.0, days / SEASON)}


def welfare(metrics):
    r = rates(metrics)
    return -sum(WEIGHTS[k] * r[k] for k in WEIGHTS)


def play(theta, seed):
    """One world under a weight vector (None = the simulation's rule); returns its metrics."""
    fastworld.set_policy(theta)
    try:
        world = fastworld.FastWorld.new(seed, START_DAY)
        try:
            return fastworld.run_days(world, SEASON)
        finally:
            world.close()
    finally:
        fastworld.set_policy(None)


def _fitness(job):
    theta, seeds = job
    return float(np.mean([welfare(play(theta, s)) for s in seeds]))


def _metrics(job):
    theta, seeds = job
    return [play(theta, s) for s in seeds]


def ranks(values):
    order = np.argsort(np.argsort(values))
    return order / (len(values) - 1) - 0.5


def chunks(items, pieces):
    return [items[k::pieces] for k in range(pieces) if items[k::pieces]]


def paired(rule, other, name):
    """Differences other - rule per world for each rate, with standard errors."""
    lines = []
    for key in ('hungry', 'unsheltered', 'bandit', 'stress', 'moves'):
        d = np.array([rates(b)[key] - rates(a)[key] for a, b in zip(rule, other)])
        se = d.std(ddof=1) / np.sqrt(len(d)) if len(d) > 1 else 0.0
        lines.append(f"{name} vs rule {key:12} {d.mean():+.4f} +- {se:.4f} (z={d.mean() / se if se else 0:.1f}); "
                     f"lower in {int((d < 0).sum())}, higher in {int((d > 0).sum())} of {len(d)}")
    d = np.array([welfare(b) - welfare(a) for a, b in zip(rule, other)])
    se = d.std(ddof=1) / np.sqrt(len(d))
    lines.append(f"{name} vs rule {'welfare':12} {d.mean():+.4f} +- {se:.4f} (z={d.mean() / se:.1f}); "
                 f"better in {int((d > 0).sum())}, worse in {int((d < 0).sum())} of {len(d)}")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--generations', type=int, default=150)
    parser.add_argument('--pairs', type=int, default=32)
    parser.add_argument('--batch', type=int, default=24, help='worlds per generation')
    parser.add_argument('--sigma', type=float, default=0.1)
    parser.add_argument('--lr', type=float, default=0.05)
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--workers', type=int, default=10)
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
            parts = pool.map(_fitness, [(th, c) for c in chunks(seeds, args.workers)])
            return float(np.average(parts, weights=[len(c) for c in chunks(seeds, args.workers)]))
        rule_valid = mean_welfare(None, valid)
        best, best_valid, started = theta.copy(), rule_valid, time.time()
        print(json.dumps({'rule_validation_welfare': rule_valid, 'weights': size}), flush=True)
        for gen in range(1, args.generations + 1):
            batch = rng.choice(train, size=args.batch, replace=False).tolist()
            eps = rng.normal(0, 1, (args.pairs, size))
            cands = [theta + args.sigma * e for e in eps] + [theta - args.sigma * e for e in eps]
            fit = np.array(pool.map(_fitness, [(c, batch) for c in cands], chunksize=1))
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
        rule = [r for c in pool.map(_metrics, [(None, c) for c in chunks(test, args.workers)]) for r in c]
        found = [r for c in pool.map(_metrics, [(best, c) for c in chunks(test, args.workers)]) for r in c]
        # chunks() interleaves seeds, so both lists are in the same interleaved order and pair up
        for name, rows in (('rule', rule), ('evolved', found)):
            r = {k: np.mean([rates(x)[k] for x in rows]) for k in ('hungry', 'unsheltered', 'bandit', 'stress', 'moves')}
            print(name, json.dumps({k: round(float(x), 4) for k, x in r.items()}),
                  'welfare', round(float(np.mean([welfare(x) for x in rows])), 4))
        print('\n'.join(paired(rule, found, 'evolved')))


if __name__ == '__main__':
    main()
