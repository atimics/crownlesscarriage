"""Check nine native goal exchanges and replay saved participant snapshots."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
from event_facts import EVENT_KIND_REGISTRY
from policy import GOALS, GROUPS
from policy_data import fixture
from run_policy import run

ROOT = Path(__file__).resolve().parents[2]


def showcase(goal):
    people = [fixture(1), fixture(2, '2', '1')]
    for i, p in enumerate(people):
        p['self'].update(name=('Ruk', 'Vesh')[i], stress=28, hungry_days=0, coins=3,
                         courage=80, unsheltered_nights=0, faction_id='7', in_transit=False)
        p['place'] = {'id': '7', 'name': 'Ash Hollow'}
        p['relationship'] = {'trust': 2, 'affinity': 2, 'obligation': 2}
        p['memories'] = [{'kind': 3, 'day': 1, 'subject_id': '3', 'event_id': '50'}]
        p['held_accounts'] = [{'event_id': '51', 'source_id': p['self']['id'], 'day': 1,
            'kind': EVENT_KIND_REGISTRY['DRAGON_RETALIATION'], 'confidence': 80,
            'account': 'Embermaw burns Thornford because 7 stolen crowns remain missing.'}]
    if goal == 'help': people[0]['self']['hungry_days'] = 2
    if goal == 'grief': people[0]['self']['stress'] = 70
    if goal == 'conflict': people[0]['relationship']['trust'] = -2
    return {'version': 1, 'world_seed': 9000+GOALS.index(goal), 'day': 2,
            'state_hash': hashlib.sha256(json.dumps(people, sort_keys=True).encode()).hexdigest(),
            'participants': people}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--language-probe', type=Path, required=True)
    args = parser.parse_args()
    results = []; failures = []
    cases = [(g, showcase(g), g, 'synthetic') for g in GOALS]
    saved = json.load(gzip.open(ROOT / 'docs/reviews/participant-minds-2026-09-19/syntax-prototype.json.gz'))
    cases += [(name, value['snapshot'], None, 'saved-simulation') for name, value in saved.items()
              if isinstance(value, dict) and 'snapshot' in value]
    for name, snapshot, requested, source in cases:
        try:
            result = run(snapshot, args.run/'last.ccv2', args.probe, 'goblin', args.language_probe, requested)
            if not result['ended']: raise ValueError('conversation did not close')
            if requested and result['turns'][0]['act']['intent'] not in GROUPS[requested]:
                raise ValueError('goal opening was not reached')
            results.append({'case': name, 'source': source, **result})
        except Exception as error:
            failures.append({'case': name, 'error': str(error),
                             'stdout': getattr(error, 'stdout', None),
                             'stderr': getattr(error, 'stderr', None)})
    receipt = {'checked': len(cases), 'completed': len(results), 'failures': failures, 'conversations': results}
    (args.run/'rollouts.json').write_text(json.dumps(receipt, indent=2)+'\n')
    if failures: raise SystemExit(json.dumps(failures))
    print(json.dumps({'checked': len(cases), 'completed': len(results)}))


if __name__ == '__main__': main()
