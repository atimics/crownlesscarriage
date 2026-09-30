"""Counterfactual replay: label each decision by what every legal option would cause.

A crowd episode is run with a base policy. At each decision with more than one
legal option the world is cloned. Every other option is then replayed from that
clone, the base policy continues, and the whole crowd is scored. The option with
the best measured outcome becomes the label; when nothing beats the base choice
the label stays with it, so rows only change where the simulation says a
different act is better. Rows are written for the C trainer (ctrain).
"""
import argparse
import copy
import json
from pathlib import Path
import random
import shutil
import tempfile
from concurrent.futures import ProcessPoolExecutor

import outcomes
from meaning import candidates, encode_input
from world_dialogue import World

EPSILON = 1e-6   # an option must beat the base choice by more than this to relabel


def relabel(utilities, base_choice, epsilon=EPSILON):
    """Best option by measured utility; the base choice unless something is strictly better."""
    best = max(range(len(utilities)), key=lambda k: (utilities[k], k == base_choice))
    return best if utilities[best] > utilities[base_choice] + epsilon else base_choice


def train_label(row, refused=('decline', 'end')):
    """The label a row teaches. A relabel to refusing or ending is reverted to the base
    choice: the model cannot see who else is waiting, so it cannot tell when refusing
    helps, and learned refusals lost relief on held-out crowds (see OUTCOMES.md)."""
    if row['label'] != row['choice'] and row['intents'][row['label']] in refused:
        return row['choice']
    return row['label']


class Crowd:
    """One helper meeting each hungry person in turn, resumable and cloneable."""

    def __init__(self, scenario, food_probe, participant_probe, seed, limit=12, _blank=False):
        self.food_probe, self.participant_probe, self.limit = str(food_probe), str(participant_probe), limit
        self.dir = Path(tempfile.mkdtemp(prefix='crowd-'))
        if _blank:
            return
        src = Path(scenario['path'])
        for suffix in ('', '-wal', '-shm'):
            if Path(str(src) + suffix).exists():
                shutil.copyfile(str(src) + suffix, str(self.dir / 'world.ccsave') + suffix)
        self.world = World(self.dir / 'world.ccsave', food_probe, participant_probe)
        self.helper = scenario['helper']
        self.order = list(scenario['hungry'])
        random.Random(seed).shuffle(self.order)
        self.observed = self.world.call('--observe', self.helper, self.order[0])
        self.start = {p['id']: p for p in self.world.call('--list')['people']}
        self.idx, self.history, self.snap, self.turn, self.bought, self.turns = 0, [], None, 0, 0, 0

    def clone(self):
        other = Crowd(None, self.food_probe, self.participant_probe, 0, self.limit, _blank=True)
        for suffix in ('', '-wal', '-shm'):
            path = Path(str(self.dir / 'world.ccsave') + suffix)
            if path.exists():
                shutil.copyfile(path, str(other.dir / 'world.ccsave') + suffix)
        other.world = World(other.dir / 'world.ccsave', self.food_probe, self.participant_probe)
        for key in ('helper', 'order', 'observed', 'start', 'idx', 'history', 'snap', 'turn', 'bought', 'turns'):
            setattr(other, key, copy.deepcopy(getattr(self, key)))
        return other

    def close(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def decision(self):
        """The speaker's view and legal options, or None when every conversation is over."""
        if self.idx >= len(self.order):
            return None
        if self.snap is None:
            self.snap = self.world.snapshot(self.order[self.idx], self.helper)
            self.history, self.turn = [], 0
        person = self.snap['participants'][self.turn % 2]
        return person, candidates(person, self.history)

    def act(self, index):
        person, options = self.decision()
        act = options[index]
        self.turns += 1
        self.turn += 1
        self.history.append({'speaker_id': act['actor'], 'act': copy.deepcopy(act)})
        over = act['intent'] == 'end' or self.turn >= self.limit
        if act['intent'] == 'accept':
            try:
                self.world.execute(act['proposal'])
                self.bought += act['proposal']['quantity']
                self.snap = self.world.snapshot(self.order[self.idx], self.helper)
            except (RuntimeError, ValueError):
                over = True
        if over:
            self.idx, self.snap, self.history, self.turn = self.idx + 1, None, [], 0

    def run(self, chooser, rng):
        while (d := self.decision()) is not None:
            person, options = d
            self.act(chooser(person, self.history, options, rng))
        return self.result()

    def result(self):
        now = {p['id']: p for p in self.world.call('--list')['people']}
        week = {p['id']: p for p in self.world.call('--days', outcomes.HORIZON, '--list')['people']}
        end = self.world.call('--observe', self.helper, self.order[0])
        crowd, helper = self.order, self.helper
        return {'people': len(crowd),
                'relieved_now': sum(now[str(p)]['hungry_days'] == 0 for p in crowd),
                'hungry_days_week': sum(week[str(p)]['hungry_days'] for p in crowd),
                'helper_hungry_week': week[str(helper)]['hungry_days'],
                'crowns_spent': self.start[str(helper)]['coins'] - now[str(helper)]['coins'],
                'units_bought': self.bought, 'turns': self.turns,
                'stock_share_after': end['stock'] / max(1, end['reserve_target'])}


def chooser_for(policy):
    return outcomes.native_model(policy) if policy.startswith('model:') else outcomes.POLICIES[policy]


def collect(args):
    """Rows for one crowd: base run, then every alternative replayed from a clone."""
    scenario, policy, seed, food_probe, participant_probe, style = args
    chooser, rng = chooser_for(policy), random.Random(seed)
    base = Crowd(scenario, food_probe, participant_probe, seed)
    points = []
    try:
        while (d := base.decision()) is not None:
            person, options = d
            choice = chooser(person, base.history, options, rng)
            if len(options) > 1:
                points.append({'person': copy.deepcopy(person), 'history': copy.deepcopy(base.history),
                               'options': options, 'choice': choice, 'clone': base.clone()})
            base.act(choice)
        base_score = outcomes.crowd_score(base.result())
        rows = []
        for point in points:
            utilities = [base_score] * len(point['options'])
            for k in range(len(point['options'])):
                if k == point['choice']:
                    continue
                trial = point['clone'].clone()
                try:
                    trial.decision()
                    trial.act(k)
                    utilities[k] = outcomes.crowd_score(trial.run(chooser, random.Random(seed)))
                except Exception:   # an option the world refuses is not better
                    utilities[k] = float('-inf')
                finally:
                    trial.close()
            label = relabel(utilities, point['choice'])
            rows.append({'tokens': encode_input(point['person'], point['history'], point['options'], style),
                         'choice': point['choice'], 'label': label, 'options': len(point['options']),
                         'utilities': [None if u == float('-inf') else round(u, 6) for u in utilities],
                         'intents': [o['intent'] for o in point['options']]})
        return {'ok': True, 'rows': rows, 'base_score': base_score}
    except Exception as error:
        return {'ok': False, 'error': repr(error)[:200]}
    finally:
        for point in points:
            point['clone'].close()
        base.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--food-probe', type=Path, required=True)
    parser.add_argument('--participant-probe', type=Path, required=True)
    parser.add_argument('--worlds', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--policy', default='teacher', help='base policy: a name or model:STYLE:MODEL:PROBE')
    parser.add_argument('--style', default='afford', choices=('digits', 'buckets', 'afford'))
    parser.add_argument('--offset', type=int, default=0)
    parser.add_argument('--limit', type=int, default=100)
    parser.add_argument('--seeds', type=int, default=399)
    parser.add_argument('--workers', type=int, default=8)
    parser.add_argument('--keep-refusals', action='store_true',
                        help='also teach relabels to decline or end (they hurt held-out crowds)')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('use a fresh output directory')
    scenarios = outcomes.harvest_crowds(args.worlds, args.food_probe, range(1, args.seeds + 1))
    random.Random(1).shuffle(scenarios)
    # Keep crowds whose store has food to divide; the rest offer no choice that matters.
    first = outcomes.evaluate_crowds(scenarios[args.offset:args.offset + args.limit], 'teacher',
                                     args.food_probe, args.participant_probe, args.workers)
    scenarios = [s for s, r in zip(scenarios[args.offset:args.offset + args.limit], first)
                 if r['ok'] and r['result']['stock_before'] > 0]
    jobs = [(s, args.policy, i, str(args.food_probe), str(args.participant_probe), args.style)
            for i, s in enumerate(scenarios)]
    with ProcessPoolExecutor(args.workers) as pool:
        results = list(pool.map(collect, jobs, chunksize=1))
    rows = [r for res in results if res['ok'] for r in res['rows']]
    args.output.mkdir(parents=True)
    with (args.output / 'rows.jsonl').open('w') as f:
        f.write(''.join(json.dumps(r) + '\n' for r in rows))
    # ctrain format: <pool> <target> <count> <tokens>; pool 1 = the simulation changed the label
    with (args.output / 'train.txt').open('w') as f:
        for r in rows:
            label = r['label'] if args.keep_refusals else train_label(r)
            f.write(f"{int(label != r['choice'])} {label} {len(r['tokens'])} {' '.join(map(str, r['tokens']))}\n")
    changed = sum(r['label'] != r['choice'] for r in rows)
    taught = sum((r['label'] if args.keep_refusals else train_label(r)) != r['choice'] for r in rows)
    print(json.dumps({'crowds': len(scenarios), 'failed': sum(not r['ok'] for r in results), 'rows': len(rows),
                      'relabelled': changed, 'taught': taught, 'base_policy': args.policy}))


if __name__ == '__main__':
    main()
