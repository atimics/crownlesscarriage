"""Score participant policies by what happens in the native world.

A scenario is a saved world plus two co-located people, one of them hungry.
`play` runs a policy for both people through the real promise/accept/execute
commands and reads the outcome from before and after snapshots. Nothing here
compares a choice with an authored preference.
"""
import argparse
import copy
import json
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
from concurrent.futures import ProcessPoolExecutor

from meaning import candidates, preferred, severity
from world_dialogue import World

SEEDS = range(1, 400)
HORIZON = 7   # days advanced after the conversation
DAYS = (30, 90, 180)


def harvest(folder, food_probe, seeds=SEEDS, days=DAYS, per_world=3):
    """Find hungry people who share a place with someone who is not hungry."""
    folder.mkdir(parents=True, exist_ok=True)
    found = []
    for seed in seeds:
        for span in days:
            path = folder / f'w{seed}-{span}.ccsave'
            run = subprocess.run([str(food_probe), '--seed', str(seed), '--days', str(span), '--list',
                                  '--save', str(path)], capture_output=True, text=True)
            if run.returncode:
                continue
            people = [p for p in json.loads(run.stdout)['people'] if p['alive'] and not p['in_transit']]
            taken = 0
            for first in (p for p in people if p['hungry_days'] > 0):
                for second in people:
                    if second['id'] != first['id'] and second['place_id'] == first['place_id'] \
                            and second['hungry_days'] == 0 and taken < per_world:
                        found.append({'path': str(path), 'first': first['id'], 'second': second['id']})
                        taken += 1
            if not taken:
                path.unlink(missing_ok=True)
    return found


# ---- policies: (person, history, candidates, rng) -> index ----
def teacher(person, history, options, rng):
    return preferred(person, history, options)


def random_legal(person, history, options, rng):
    return rng.randrange(len(options))


def stingy(person, history, options, rng):
    """Never spend: request when hungry, otherwise decline or end."""
    for want in ('request_food', 'end', 'decline'):
        for n, a in enumerate(options):
            if a['intent'] == want:
                return n
    return 0


def generous(person, history, options, rng):
    """Agree to the largest purchase offered and accept everything."""
    best, size = 0, -1
    for n, a in enumerate(options):
        weight = {'accept': 1000, 'request_food': 500}.get(a['intent'], 0)
        if a['proposal']:
            weight += a['proposal']['quantity']
        if a['intent'] in ('decline', 'end'):
            weight = -1
        if weight > size:
            best, size = n, weight
    return best


def trusting(person, history, options, rng):
    """The teacher's choice with trust ignored."""
    p = copy.deepcopy(person)
    p['relationship'] = dict(p.get('relationship') or {}, trust=max(1, (p.get('relationship') or {}).get('trust', 0)))
    return preferred(p, history, options)


def reserving(units):
    """The teacher, except an offer is the largest one that leaves the helper `units` meals of coins."""
    def policy(person, history, options, rng):
        chosen = preferred(person, history, options)
        if options[chosen]['intent'] != 'offer_food':
            return chosen
        best = None
        for n, a in enumerate(options):
            if a['intent'] == 'offer_food' and person['self']['coins'] - a['proposal']['total_cost'] >= units * a['proposal']['unit_price']:
                best = n
        return chosen if best is None else best
    return policy


def one_unit(person, history, options, rng):
    """The teacher, but every offer is a single portion."""
    chosen = preferred(person, history, options)
    if options[chosen]['intent'] == 'offer_food':
        return next(n for n, a in enumerate(options) if a['intent'] == 'offer_food')
    return chosen


POLICIES = {'teacher': teacher, 'random': random_legal, 'stingy': stingy,
            'generous': generous, 'trusting': trusting, 'one': one_unit,
            **{f'reserve{n}': reserving(n) for n in range(4)}}


def read(snapshot):
    a, b = snapshot['participants']
    fact = (b.get('facts') or a.get('facts') or [{}])[0]
    return {'first_coins': a['self']['coins'], 'second_coins': b['self']['coins'],
            'first_hungry': a['self']['hungry_days'], 'second_hungry': b['self']['hungry_days'],
            'stock': fact.get('stock'), 'target': fact.get('target'), 'price': fact.get('unit_price'),
            'trust_first_to_second': (a.get('relationship') or {}).get('trust', 0),
            'trust_second_to_first': (b.get('relationship') or {}).get('trust', 0)}


def native_model(spec):
    """`model:STYLE:MODEL:PROBE` runs a trained checkpoint through its native probe."""
    _, style, model, probe = spec.split(':', 3)
    def policy(person, history, options, rng):
        from meaning import choose
        return options.index(choose(person, history, model, probe, style))
    return policy


def play(scenario, policy, seed, food_probe, participant_probe, limit=12, days=1):
    """Run one exchange on a private copy of the world; return outcome components."""
    rng = random.Random(seed)
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / 'world.ccsave'
        shutil.copyfile(scenario['path'], path)
        for suffix in ('-wal', '-shm'):
            if Path(scenario['path'] + suffix).exists():
                shutil.copyfile(scenario['path'] + suffix, str(path) + suffix)
        world = World(path, food_probe, participant_probe)
        chooser = native_model(policy) if policy.startswith('model:') else POLICIES[policy]
        snap = world.snapshot(scenario['first'], scenario['second'])
        before = read(snap)
        intents, executed, invalid, bought, daily = [], None, None, 0, []
        for day in range(days):
            history = []
            for turn in range(limit):
                person = snap['participants'][turn % 2]
                options = candidates(person, history)
                act = options[chooser(person, history, options, rng)]
                intents.append(act['intent'])
                history.append({'speaker_id': act['actor'], 'act': copy.deepcopy(act)})
                if act['intent'] == 'accept':
                    try:
                        executed = world.execute(act['proposal'])
                        bought += act['proposal']['quantity']
                        snap = world.snapshot(scenario['first'], scenario['second'])
                    except (RuntimeError, ValueError) as error:
                        invalid = str(error)
                        break
                if act['intent'] == 'end':
                    break
            now = read(snap)
            daily.append({'first_hungry': now['first_hungry'], 'second_hungry': now['second_hungry'],
                          'second_coins': now['second_coins']})
            if day + 1 < days:   # let a day pass, then they meet again
                world.call('--days', 1, '--list', mutate=True)
                snap = world.snapshot(scenario['first'], scenario['second'])
        after = read(snap)
        later = {p['id']: p for p in world.call('--days', HORIZON if days == 1 else 0, '--list')['people']}
        future = {}
        for key, person in (('first', scenario['first']), ('second', scenario['second'])):
            row = later.get(str(person), {})
            future[key] = {'hungry': row.get('hungry_days', 0), 'bandit': int(bool(row.get('bandit'))),
                           'stress': row.get('stress', 0), 'coins': row.get('coins', 0),
                           'alive': int(row.get('alive', True))}
    return {'before': before, 'after': after, 'future': future, 'daily': daily, 'intents': intents,
            'turns': len(intents), 'executed': executed is not None, 'invalid': invalid, 'quantity': bought}


def components(r):
    b, a = r['before'], r['after']
    paid = (b['first_coins'] + b['second_coins']) - (a['first_coins'] + a['second_coins'])
    taken = (b['stock'] or 0) - (a['stock'] or 0)
    return {
        'relief': int(b['first_hungry'] > 0 and a['first_hungry'] == 0),
        'helper_still_fed': int(a['second_hungry'] == 0),
        'coins_to_market': paid,
        'units_taken': taken,
        'stock_share_after': (a['stock'] or 0) / max(1, a['target'] or 1),
        'helper_can_buy_meal': int(a['second_coins'] >= (a['price'] or 1)),
        'turns': r['turns'], 'executed': int(r['executed']),
        'conserved': int(paid >= 0 and taken >= 0 and (taken == 0 or paid > 0)),
        'hungry_days_total': sum(d['first_hungry'] for d in r['daily']),
        'helper_hungry_days_total': sum(d['second_hungry'] for d in r['daily']),
        'hungry_after_week': r['future']['first']['hungry'],
        'bandit_after_week': r['future']['first']['bandit'] + r['future']['second']['bandit'],
        'helper_hungry_after_week': r['future']['second']['hungry'],
    }


def week_score(c):
    """Outcome over the week: every day either person spends hungry counts against,
    the helper's a little less; crowns spent cost a little."""
    return -c['hungry_days_total'] / 7 - 0.5 * c['helper_hungry_days_total'] / 7 - 0.02 * c['coins_to_market']


def score(c):
    """One number for comparison; the components above are the evidence."""
    return c['relief'] - 0.5 * (1 - c['helper_still_fed']) - 0.02 * c['coins_to_market'] \
        - 0.02 * c['turns'] - (0.5 if c['executed'] and c['stock_share_after'] < 0.1 else 0)


def _job(args):
    scenario, name, seed, food_probe, participant_probe, days = args
    try:
        result = play(scenario, name, seed, food_probe, participant_probe, days=days)
        c = components(result)
        b = result['before']
        # A purchase is possible: the helper can pay for one unit and the store has one.
        feasible = b['second_coins'] >= (b['price'] or 10**9) and (b['stock'] or 0) >= 1 and b['first_hungry'] > 0
        return {'ok': True, 'components': c, 'score': score(c), 'week': week_score(c), 'intents': result['intents'],
                'invalid': result['invalid'], 'feasible': feasible, 'before': b}
    except Exception as error:  # a failed episode is data, not a crash
        return {'ok': False, 'error': repr(error)[:200]}


def evaluate(scenarios, name, food_probe, participant_probe, workers=8, seed=0, days=1):
    jobs = [(s, name, seed + i, str(food_probe), str(participant_probe), days) for i, s in enumerate(scenarios)]
    with ProcessPoolExecutor(workers) as pool:
        return list(pool.map(_job, jobs, chunksize=4))


def summarize(results, feasible_only=False):
    ok = [r for r in results if r['ok'] and (r['feasible'] or not feasible_only)]
    keys = ok[0]['components'].keys() if ok else []
    out = {'episodes': len(ok), 'failed': sum(not r['ok'] for r in results),
           'score': sum(r['score'] for r in ok) / max(1, len(ok)),
           'week': sum(r['week'] for r in ok) / max(1, len(ok))}
    for k in keys:
        out[k] = sum(r['components'][k] for r in ok) / len(ok)
    return out


def harvest_crowds(folder, food_probe, seeds=SEEDS, days=DAYS, per_world=2, most=4):
    """Places where several hungry people share a place with someone who can pay."""
    folder.mkdir(parents=True, exist_ok=True)
    found = []
    for seed in seeds:
        for span in days:
            path = folder / f'c{seed}-{span}.ccsave'
            run = subprocess.run([str(food_probe), '--seed', str(seed), '--days', str(span), '--list',
                                  '--save', str(path)], capture_output=True, text=True)
            if run.returncode:
                continue
            people = [p for p in json.loads(run.stdout)['people'] if p['alive'] and not p['in_transit']]
            places, taken = {}, 0
            for p in people:
                places.setdefault(p['place_id'], []).append(p)
            for group in places.values():
                hungry = [p for p in group if p['hungry_days'] > 0]
                helpers = sorted((p for p in group if p['hungry_days'] == 0 and p['coins'] >= 4),
                                 key=lambda p: -p['coins'])
                if len(hungry) >= 2 and helpers and taken < per_world:
                    found.append({'path': str(path), 'helper': helpers[0]['id'],
                                  'hungry': [p['id'] for p in hungry[:most]]})
                    taken += 1
            if not taken:
                path.unlink(missing_ok=True)
    return found


def play_crowd(scenario, policy, seed, food_probe, participant_probe, fast=None):
    """One helper meets each hungry person in turn; the store and purse carry over."""
    from crowd import Crowd
    chooser = native_model(policy) if policy.startswith('model:') else POLICIES[policy]
    crowd = Crowd(scenario, food_probe, participant_probe, seed, fast=fast)
    try:
        return crowd.run(chooser, random.Random(seed))
    finally:
        crowd.close()


def crowd_score(r):
    """Share relieved now, minus hunger a week later, a hungry helper and spending."""
    n = r['people']
    return r['relieved_now'] / n - r['hungry_days_week'] / (7 * n) - 0.5 * r['helper_hungry_week'] / 7 \
        - 0.01 * r['crowns_spent']


def _crowd_job(args):
    scenario, name, seed, food_probe, participant_probe = args
    try:
        r = play_crowd(scenario, name, seed, food_probe, participant_probe)
        return {'ok': True, 'result': r, 'score': crowd_score(r)}
    except Exception as error:
        return {'ok': False, 'error': repr(error)[:200]}


def evaluate_crowds(scenarios, name, food_probe, participant_probe, workers=8, seed=0):
    jobs = [(s, name, seed + i, str(food_probe), str(participant_probe)) for i, s in enumerate(scenarios)]
    with ProcessPoolExecutor(workers) as pool:
        return list(pool.map(_crowd_job, jobs, chunksize=2))


def paired(baseline, other, name):
    """Mean difference other - baseline over episodes both finished, with its standard error."""
    import math
    pairs = [(x, y) for x, y in zip(baseline, other) if x['ok'] and y['ok']]
    lines = []
    fields = (('score', lambda r: r['score']), ('relieved', lambda r: r['result']['relieved_now']),
              ('crowns', lambda r: r['result']['crowns_spent']), ('units', lambda r: r['result']['units_bought']),
              ('hungry_d7', lambda r: r['result']['hungry_days_week']),
              ('helper_hungry_d7', lambda r: r['result']['helper_hungry_week']))
    for label, f in fields:
        d = [f(y) - f(x) for x, y in pairs]
        mean = sum(d) / len(d)
        se = math.sqrt(sum((v - mean) ** 2 for v in d) / (len(d) - 1) / len(d)) if len(d) > 1 else 0.0
        lines.append(f"{name} vs baseline {label:17} {mean:+.3f} +- {se:.3f} (z={mean / se if se else 0:.1f}); "
                     f"higher in {sum(v > 0 for v in d)}, lower in {sum(v < 0 for v in d)} of {len(d)}")
    return lines


def summarize_crowds(results):
    ok = [r for r in results if r['ok']]
    out = {'episodes': len(ok), 'failed': len(results) - len(ok),
           'score': sum(r['score'] for r in ok) / max(1, len(ok))}
    for k in ok[0]['result']:
        out[k] = sum(r['result'][k] for r in ok) / len(ok)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--food-probe', type=Path, required=True)
    parser.add_argument('--participant-probe', type=Path, required=True)
    parser.add_argument('--worlds', type=Path, required=True)
    parser.add_argument('--policies', nargs='+', default=list(POLICIES))
    parser.add_argument('--limit', type=int, default=100)
    parser.add_argument('--offset', type=int, default=0, help='skip this many shuffled scenarios (for held-out evaluation)')
    parser.add_argument('--seeds', type=int, default=120)
    parser.add_argument('--workers', type=int, default=8)
    parser.add_argument('--only-feasible', action='store_true')
    parser.add_argument('--baseline', help='with --crowd: also print paired differences against this policy')
    parser.add_argument('--crowd', action='store_true', help='one helper meets several hungry people')
    parser.add_argument('--days', type=int, default=1, help='meet once a day for this many days')
    args = parser.parse_args()
    if args.crowd:
        scenarios = harvest_crowds(args.worlds, args.food_probe, range(1, args.seeds + 1))
        random.Random(1).shuffle(scenarios)
        scenarios = scenarios[args.offset:args.offset + args.limit]
        if args.only_feasible:   # keep crowds whose store has food to divide
            first = evaluate_crowds(scenarios, 'teacher', args.food_probe, args.participant_probe, args.workers)
            scenarios = [s for s, r in zip(scenarios, first) if r['ok'] and r['result']['stock_before'] > 0]
        print(json.dumps({'crowd_scenarios': len(scenarios)}))
        base = evaluate_crowds(scenarios, args.baseline, args.food_probe, args.participant_probe, args.workers) \
            if args.baseline else None
        for name in args.policies:
            results = evaluate_crowds(scenarios, name, args.food_probe, args.participant_probe, args.workers)
            print(name, json.dumps({k: round(v, 3) for k, v in summarize_crowds(results).items()}))
            if base is not None and name != args.baseline:
                print('\n'.join(paired(base, results, name)))
        return
    scenarios = harvest(args.worlds, args.food_probe, range(1, args.seeds + 1))
    random.Random(1).shuffle(scenarios)
    scenarios = scenarios[args.offset:args.offset + args.limit]
    if args.only_feasible:   # keep the situations where a purchase is possible
        first = evaluate(scenarios, 'teacher', args.food_probe, args.participant_probe, args.workers)
        scenarios = [s for s, r in zip(scenarios, first) if r['ok'] and r['feasible']]
    print(json.dumps({'scenarios': len(scenarios)}))
    for name in args.policies:
        results = evaluate(scenarios, name, args.food_probe, args.participant_probe, args.workers, days=args.days)
        for label, only in (('all', False), ('feasible', True)):
            print(name, label, json.dumps({k: round(v, 3) for k, v in summarize(results, only).items()}))


if __name__ == '__main__':
    main()
