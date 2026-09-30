"""Competitive co-evolution of a raider brain and a town brain, Generalist-style.

Phases alternate between the roles. In a phase one role is evolved (evolution strategies, its own score) against a
fixed set of opponents sampled from the other role's archive of frozen snapshots, each world meeting one of them.
Every few generations the candidate is tested on held-out worlds against held-out opponents and the best by that
test is kept, so an update that got worse against unseen opponents is rejected. The kept brain joins its role's
archive. The yardstick is each role's score against the simulation's own rule for the other side, logged per phase.
A final round-robin over all snapshots (see tournament.py) shows progress, cycling or disengagement.

Raider score: goods taken. Town score: prosperity minus famine share and hunger (scaled), for trade and kingdom relief.
"""
import argparse
import json
from multiprocessing import get_context
from pathlib import Path
import time

import numpy as np

import fastworld as fw

START, DAYS = 30, 730
RAIDER_BRAIN, TOWN_BRAIN = 1, 0
ROLES = {'raider': {'brain': RAIDER_BRAIN, 'mask': fw.RAID, 'kinds': {'raid_target': RAIDER_BRAIN, 'raid_launch': RAIDER_BRAIN}},
         'town': {'brain': TOWN_BRAIN, 'mask': fw.TRADE | fw.KINGDOM_RELIEF,
                  'kinds': {'trade': TOWN_BRAIN, 'kingdom_relief': TOWN_BRAIN}}}


def score(role, m):
    if role == 'raider':
        return m['loot'] / 100.0
    days = max(1.0, m['town_days'])
    return m['town_prosperity'] / days / 100.0 - m['town_famine'] / days - m['town_hunger'] / days / 100.0


def play(role, theta, opponent, seed):
    """One world: `role` plays `theta`, the other role plays `opponent` (None = the rule). Returns the role's score."""
    other = 'town' if role == 'raider' else 'raider'
    n = fw.policy_size()
    fw.clear_brains()
    for name, weights in ((role, theta), (other, opponent)):
        spec = ROLES[name]
        fw.set_brain(spec['brain'], weights if weights is not None else np.zeros(n), spec['mask'])
    fw.set_kind_brains({**ROLES['raider']['kinds'], **ROLES['town']['kinds']})
    world = fw.FastWorld.new(seed, START)
    m = fw.run_days(world, DAYS)
    world.close()
    fw.clear_brains()
    return score(role, m)


def fitness(job):
    role, theta, opponents, seeds = job
    return float(np.mean([play(role, theta, opponents[i % len(opponents)], s) for i, s in enumerate(seeds)]))


def ranks(v):
    order = np.argsort(np.argsort(v))
    return order / (len(v) - 1) - 0.5


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--phases', type=int, default=6)
    parser.add_argument('--generations', type=int, default=20, help='per phase')
    parser.add_argument('--pairs', type=int, default=16)
    parser.add_argument('--batch', type=int, default=24)
    parser.add_argument('--opponents', type=int, default=6, help='fixed opponents per phase')
    parser.add_argument('--sigma', type=float, default=0.1)
    parser.add_argument('--lr', type=float, default=0.05)
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--workers', type=int, default=10)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('use a fresh output directory')
    args.output.mkdir(parents=True)
    rng = np.random.default_rng(args.seed)
    size = fw.policy_size()
    archive = {'raider': [None], 'town': [None]}     # None is the rule; new snapshots are appended
    train, valid, yard = list(range(1, 61)), list(range(61, 91)), list(range(101, 141))
    log = (args.output / 'log.jsonl').open('w')
    with get_context('spawn').Pool(args.workers) as pool:
        def mean_score(role, theta, opponents, seeds):
            chunks = [seeds[k::args.workers] for k in range(args.workers) if seeds[k::args.workers]]
            parts = pool.map(fitness, [(role, theta, opponents, c) for c in chunks])
            return float(np.average(parts, weights=[len(c) for c in chunks]))
        started = time.time()
        for phase in range(args.phases):
            role = ['raider', 'town'][phase % 2]
            other = 'town' if role == 'raider' else 'raider'
            pool_ops = archive[other]
            k = min(args.opponents, len(pool_ops))
            # the newest opponent always plays; the rest are sampled from the whole archive
            picks = [len(pool_ops) - 1] + list(rng.choice(len(pool_ops), k - 1, replace=False) if k > 1 else [])
            opponents = [pool_ops[i] for i in picks]
            held = [o for i, o in enumerate(pool_ops) if i not in picks] or [None]
            theta = np.zeros(size) if archive[role][-1] is None else archive[role][-1].copy()
            m, v = np.zeros(size), np.zeros(size)
            best, best_valid = theta.copy(), mean_score(role, theta, held, valid)
            for gen in range(1, args.generations + 1):
                batch = rng.choice(train, size=args.batch, replace=False).tolist()
                eps = rng.normal(0, 1, (args.pairs, size))
                cands = [theta + args.sigma * e for e in eps] + [theta - args.sigma * e for e in eps]
                fit = np.array(pool.map(fitness, [(role, c, opponents, batch) for c in cands], chunksize=1))
                shaped = ranks(fit)
                grad = ((shaped[:args.pairs] - shaped[args.pairs:])[:, None] * eps).sum(0) / (2 * args.pairs * args.sigma)
                m = 0.9 * m + 0.1 * grad
                v = 0.999 * v + 0.001 * grad * grad
                theta = theta + args.lr * (m / (1 - 0.9 ** gen)) / (np.sqrt(v / (1 - 0.999 ** gen)) + 1e-8)
                if gen % 5 == 0 or gen == args.generations:
                    val = mean_score(role, theta, held, valid)   # unseen opponents on unseen worlds: a regression is rejected
                    if val > best_valid:
                        best_valid, best = val, theta.copy()
            archive[role].append(best)
            rule_opp = [None]
            row = {'phase': phase, 'role': role, 'opponents': len(opponents), 'held_out': len(held),
                   'validation': best_valid, 'yardstick_vs_rule': mean_score(role, best, rule_opp, yard),
                   'rule_yardstick': mean_score(role, None, rule_opp, yard), 'seconds': round(time.time() - started)}
            log.write(json.dumps(row) + '\n'); log.flush()
            print(json.dumps(row), flush=True)
    for role in archive:
        np.save(args.output / f'{role}_snapshots.npy',
                np.array([np.zeros(size) if a is None else a for a in archive[role]]))
    print('snapshots:', {r: len(a) for r, a in archive.items()})


if __name__ == '__main__':
    main()
