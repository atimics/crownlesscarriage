#!/usr/bin/env python3
"""Run the shipped 5M checkpoint as one participant for baseline measurements.

The existing checkpoint reads the supported account and mind fields. Its
limited input adapter is recorded in each result receipt by the pair runner.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def level(value):
    return 'low' if value < 34 else 'high' if value > 66 else 'medium'


def reply(request, probe, model, event):
    p = request['participant']
    me = p['self']
    supported = [a for a in p['held_accounts'] if a['parser_supported']]
    account = next((a for a in supported if a['event_id'] == event), None) if event else (
        supported[0] if supported else None)
    if account is None:
        raise ValueError('participant has no supported account for this baseline')
    speech = [o for o in request['observed_turns'] if o['kind'] == 'speech']
    # This baseline asks for an opening or a personal reaction to what was heard.
    control = 'open' if not speech else 'remark'
    relation = p['relationship'] or {}
    mind = ':'.join([me['occupation'], me['goal'], level(me['stress']), level(me['courage']),
                     control, str(int(me['hungry_days'] > 0)),
                     str(int(me['unsheltered_nights'] == 0)), str(int(me['in_transit'])),
                     str(int(relation.get('obligation', 0) >= 2)),
                     str(int(relation.get('trust', 0) >= 2)), '',
                     str(int(me['home_id'] != p['place']['id']))])
    argv = [str(probe), str(model), str(account['kind']), str(account['confidence']),
            str(account['retellings']), account['model_account'], '--mind', mind]
    if account['source_id'] == me['id']:
        argv += ['--witnessed']
    for memory in [a for a in supported if a['event_id'] != account['event_id']][:2]:
        argv += ['--memory', memory['model_account']]
    argv += [o['text'] for o in speech[-4:]]
    result = subprocess.run(argv, capture_output=True, text=True, check=True, timeout=60)
    if result.stderr:
        print(result.stderr, file=sys.stderr, end='')
    return {'kind': 'speech', 'text': result.stdout.rstrip('\n')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--event')
    args = parser.parse_args()
    identity = None
    for line in sys.stdin:
        request = json.loads(line)
        current = request['participant']['self']['id']
        if identity is None:
            identity = current
        if current != identity:
            raise ValueError('participant identity changed during the worker session')
        print(json.dumps(reply(request, args.probe, args.model, args.event)), flush=True)


if __name__ == '__main__':
    main()
