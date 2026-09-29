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


POLICIES = {'teacher': teacher, 'random': random_legal, 'stingy': stingy,
            'generous': generous, 'trusting': trusting}


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


def play(scenario, policy, seed, food_probe, participant_probe, limit=12):
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
        history, intents, executed, invalid = [], [], None, None
        for turn in range(limit):
            person = snap['participants'][turn % 2]
            options = candidates(person, history)
            act = options[chooser(person, history, options, rng)]
            intents.append(act['intent'])
            history.append({'speaker_id': act['actor'], 'act': copy.deepcopy(act)})
            if act['intent'] == 'accept':
                try:
                    executed = world.execute(act['proposal'])
                    snap = world.snapshot(scenario['first'], scenario['second'])
                except (RuntimeError, ValueError) as error:
                    invalid = str(error)
                    break
            if act['intent'] == 'end':
                break
        after = read(snap)
    return {'before': before, 'after': after, 'intents': intents, 'turns': len(intents),
            'executed': executed is not None, 'invalid': invalid,
            'quantity': (history[-1]['act']['proposal'] or {}).get('quantity', 0) if executed else 0}


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
    }


def score(c):
    """One number for comparison; the components above are the evidence."""
    return c['relief'] - 0.5 * (1 - c['helper_still_fed']) - 0.02 * c['coins_to_market'] \
        - 0.02 * c['turns'] - (0.5 if c['executed'] and c['stock_share_after'] < 0.1 else 0)


def _job(args):
    scenario, name, seed, food_probe, participant_probe = args
    try:
        result = play(scenario, name, seed, food_probe, participant_probe)
        c = components(result)
        b = result['before']
        # A purchase is possible: the helper can pay for one unit and the store has one.
        feasible = b['second_coins'] >= (b['price'] or 10**9) and (b['stock'] or 0) >= 1 and b['first_hungry'] > 0
        return {'ok': True, 'components': c, 'score': score(c), 'intents': result['intents'],
                'invalid': result['invalid'], 'feasible': feasible, 'before': b}
    except Exception as error:  # a failed episode is data, not a crash
        return {'ok': False, 'error': repr(error)[:200]}


def evaluate(scenarios, name, food_probe, participant_probe, workers=8, seed=0):
    jobs = [(s, name, seed + i, str(food_probe), str(participant_probe)) for i, s in enumerate(scenarios)]
    with ProcessPoolExecutor(workers) as pool:
        return list(pool.map(_job, jobs, chunksize=4))


def summarize(results, feasible_only=False):
    ok = [r for r in results if r['ok'] and (r['feasible'] or not feasible_only)]
    keys = ok[0]['components'].keys() if ok else []
    out = {'episodes': len(ok), 'failed': sum(not r['ok'] for r in results),
           'score': sum(r['score'] for r in ok) / max(1, len(ok))}
    for k in keys:
        out[k] = sum(r['components'][k] for r in ok) / len(ok)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--food-probe', type=Path, required=True)
    parser.add_argument('--participant-probe', type=Path, required=True)
    parser.add_argument('--worlds', type=Path, required=True)
    parser.add_argument('--policies', nargs='+', default=list(POLICIES))
    parser.add_argument('--limit', type=int, default=100)
    parser.add_argument('--seeds', type=int, default=120)
    parser.add_argument('--workers', type=int, default=8)
    parser.add_argument('--only-feasible', action='store_true')
    args = parser.parse_args()
    scenarios = harvest(args.worlds, args.food_probe, range(1, args.seeds + 1))
    random.Random(1).shuffle(scenarios)
    scenarios = scenarios[:args.limit]
    if args.only_feasible:   # keep the situations where a purchase is possible
        first = evaluate(scenarios, 'teacher', args.food_probe, args.participant_probe, args.workers)
        scenarios = [s for s, r in zip(scenarios, first) if r['ok'] and r['feasible']]
    print(json.dumps({'scenarios': len(scenarios)}))
    for name in args.policies:
        results = evaluate(scenarios, name, args.food_probe, args.participant_probe, args.workers)
        for label, only in (('all', False), ('feasible', True)):
            print(name, label, json.dumps({k: round(v, 3) for k, v in summarize(results, only).items()}))


if __name__ == '__main__':
    main()
