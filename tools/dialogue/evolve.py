"""Evolve a small option-scoring policy against crowd outcomes, without labels.

The policy scores every legal option with a tiny network and takes the best. It
never sees a teacher's choice or a rule. Evolution strategies (antithetic
sampling, rank-shaped fitness, Adam) change its weights to raise the mean crowd
score over a batch of crowds; the same crowds are used for every candidate in a
generation, so differences between candidates come from the weights.
"""
import argparse
import json
from multiprocessing import get_context
from pathlib import Path
import random
import time

import numpy as np

import outcomes
from crowd import Crowd
from meaning import INTENTS, REASONS

SEVERITY = ('no_need', 'shortage', 'severe_shortage')
LAST = ('none',) + tuple(INTENTS)
HIDDEN = 24


def one_hot(value, names):
    return [1.0 if value == n else 0.0 for n in names]


def features(person, history, options):
    """One row per legal option: the situation, the option, and what the purse buys."""
    own = person['self']
    trust = (person.get('relationship') or {}).get('trust', 0)
    last = history[-1]['act']['intent'] if history else 'none'
    base = [own['coins'] / 50, float(own['hungry_days'] > 0), own['stress'] / 100,
            float(trust < 0), float(trust == 0), float(trust > 0),
            person.get('waiting', 0) / 4, len(history) / 12] + one_hot(last, LAST)
    rows = []
    for a in options:
        p, f = a['proposal'], a['claim']
        row = base + one_hot(a['intent'], INTENTS) + one_hot(a['reason'], REASONS)
        row += [p['quantity'] / 3, p['total_cost'] / 50, p['unit_price'] / 20, float(p['payer_id'] == own['id'])] \
            if p else [0.0] * 4
        if f:
            row += [f['stock'] / 50, f['stock'] / max(1, f['target']), min(6, own['coins'] // max(1, f['unit_price'])) / 6]
            row += one_hot(outcomes.severity(f), SEVERITY)
        else:
            row += [0.0] * 6
        rows.append(row)
    return np.array(rows, dtype=np.float64)


FEATURES = len(features({'self': {'id': '1', 'coins': 0, 'hungry_days': 0, 'stress': 0}}, [],
                        [{'intent': 'end', 'reason': 'no_need', 'proposal': None, 'claim': None}])[0])
SHAPES = ((FEATURES, HIDDEN), (HIDDEN,), (HIDDEN,), (1,))
SIZE = sum(int(np.prod(s)) for s in SHAPES)


def unpack(theta):
    parts, at = [], 0
    for shape in SHAPES:
        n = int(np.prod(shape))
        parts.append(theta[at:at + n].reshape(shape))
        at += n
    return parts


def scorer(theta):
    w1, b1, w2, b2 = unpack(theta)

    def policy(person, history, options, rng):
        x = features(person, history, options)
        scores = np.tanh(x @ w1 + b1) @ w2 + b2[0]
        return int(np.argmax(scores))
    return policy


_state = {}


def _setup(scenarios, food_probe, participant_probe):
    _state.update(scenarios=scenarios, fp=food_probe, pp=participant_probe)


def _fitness(job):
    """Mean crowd score of a weight vector over the given crowd indices, and the mean per-crowd scores."""
    theta, indices = job
    policy = scorer(theta)
    scores = []
    for i in indices:
        crowd = Crowd(_state['scenarios'][i], _state['fp'], _state['pp'], i)
        try:
            scores.append(outcomes.crowd_score(crowd.run(policy, random.Random(i))))
        finally:
            crowd.close()
    return float(np.mean(scores))


def _result(job):
    """Full outcome rows for one policy over crowds (for the paired report)."""
    name, theta, indices = job
    policy = scorer(theta) if theta is not None else outcomes.POLICIES[name]
    rows = []
    for i in indices:
        crowd = Crowd(_state['scenarios'][i], _state['fp'], _state['pp'], i)
        try:
            r = crowd.run(policy, random.Random(i))
            rows.append({'ok': True, 'result': r, 'score': outcomes.crowd_score(r)})
        except Exception as error:
            rows.append({'ok': False, 'error': repr(error)[:100]})
        finally:
            crowd.close()
    return rows


def ranks(values):
    order = np.argsort(np.argsort(values))
    return order / (len(values) - 1) - 0.5


def feasible_crowds(scenarios, food_probe, participant_probe):
    """Crowds whose store has food to divide (the rest offer no decision)."""
    keep = []
    for i, s in enumerate(scenarios):
        crowd = Crowd(s, food_probe, participant_probe, 0)
        try:
            if crowd.observed['stock'] > 0:
                keep.append(s)
        finally:
            crowd.close()
    return keep


def split_indices(n, pieces):
    return [list(range(k, n, pieces)) for k in range(pieces)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--food-probe', type=Path, required=True)
    parser.add_argument('--participant-probe', type=Path, required=True)
    parser.add_argument('--worlds', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--generations', type=int, default=60)
    parser.add_argument('--pairs', type=int, default=32, help='antithetic pairs per generation')
    parser.add_argument('--batch', type=int, default=60, help='crowds per generation')
    parser.add_argument('--sigma', type=float, default=0.15)
    parser.add_argument('--lr', type=float, default=0.05)
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--workers', type=int, default=10)
    parser.add_argument('--seeds', type=int, default=399)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('use a fresh output directory')
    args.output.mkdir(parents=True)
    fp, pp = str(args.food_probe), str(args.participant_probe)
    everything = outcomes.harvest_crowds(args.worlds, args.food_probe, range(1, args.seeds + 1))
    random.Random(1).shuffle(everything)
    # Test is the slice the counterfactual experiments held out (900-2100); training and
    # validation split the 0-900 slice they collected from.
    train, valid, test = (feasible_crowds(everything[a:b], fp, pp) for a, b in ((0, 750), (750, 900), (900, 2100)))
    print(json.dumps({'train': len(train), 'validation': len(valid), 'test': len(test)}), flush=True)
    scenarios = train + valid + test
    ti, vi, si = (list(range(0, len(train))), list(range(len(train), len(train) + len(valid))),
                  list(range(len(train) + len(valid), len(scenarios))))
    rng = np.random.default_rng(args.seed)
    theta = rng.normal(0, 0.1, SIZE)
    m, v = np.zeros(SIZE), np.zeros(SIZE)
    ctx = get_context('spawn')
    log = (args.output / 'log.jsonl').open('w')
    with ctx.Pool(args.workers, initializer=_setup, initargs=(scenarios, fp, pp)) as pool:
        def mean_score(th, indices):
            parts = pool.map(_fitness, [(th, chunk) for chunk in split_indices_of(indices, args.workers)])
            sizes = [len(c) for c in split_indices_of(indices, args.workers)]
            return float(np.average(parts, weights=sizes))
        best, best_valid, started = theta.copy(), -1e9, time.time()
        for gen in range(1, args.generations + 1):
            batch = rng.choice(ti, size=args.batch, replace=False).tolist()
            eps = rng.normal(0, 1, (args.pairs, SIZE))
            cands = [theta + args.sigma * e for e in eps] + [theta - args.sigma * e for e in eps]
            jobs = [(c, batch) for c in cands]
            fit = np.array(pool.map(_fitness, jobs, chunksize=1))
            plus, minus = fit[:args.pairs], fit[args.pairs:]
            shaped = ranks(fit)
            grad = ((shaped[:args.pairs] - shaped[args.pairs:])[:, None] * eps).sum(0) / (2 * args.pairs * args.sigma)
            m = 0.9 * m + 0.1 * grad
            v = 0.999 * v + 0.001 * grad * grad
            theta = theta + args.lr * (m / (1 - 0.9 ** gen)) / (np.sqrt(v / (1 - 0.999 ** gen)) + 1e-8)
            row = {'generation': gen, 'mean_fitness': float(fit.mean()), 'best_fitness': float(fit.max()),
                   'seconds': round(time.time() - started, 1)}
            if gen % 5 == 0 or gen == args.generations:
                row['validation'] = mean_score(theta, vi)
                if row['validation'] > best_valid:
                    best_valid, best = row['validation'], theta.copy()
            log.write(json.dumps(row) + '\n'); log.flush()
            print(json.dumps(row), flush=True)
        np.save(args.output / 'theta_last.npy', theta)
        np.save(args.output / 'theta_best_validation.npy', best)
        # Paired report on crowds the search never saw.
        report = {}
        for name, th in (('teacher', None), ('one', None), ('evolved', best), ('evolved_last', theta)):
            chunks = pool.map(_result, [(name, th, chunk) for chunk in split_indices_of(si, args.workers)])
            report[name] = [row for chunk in chunks for row in chunk]
        base = report['teacher']
        for name in ('teacher', 'one', 'evolved', 'evolved_last'):
            s = outcomes.summarize_crowds(report[name])
            print(name, json.dumps({k: round(x, 3) for k, x in s.items() if k in
                                    ('episodes', 'score', 'relieved_now', 'people', 'crowns_spent', 'units_bought')}))
            if name != 'teacher':
                print('\n'.join(outcomes.paired(base, report[name], name)))


def split_indices_of(indices, pieces):
    return [indices[k::pieces] for k in range(pieces) if indices[k::pieces]]


if __name__ == '__main__':
    main()
