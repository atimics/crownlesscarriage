"""Verify native v3 decisions, food movement, and reunion memories."""
import argparse
import json
from pathlib import Path
import subprocess

from meaning import choose, validate
from meaning_language import render
from world_dialogue import World, exchange, check_model


def fixture(actor='1', listener='2', stock=16, coins=8, hunger=0):
    return {'self': {'id': actor, 'name': 'Ruk' if actor == '1' else 'Vesh',
                     'coins': coins, 'hungry_days': hunger, 'stress': 20},
            'listener': {'id': listener, 'name': 'Vesh' if listener == '2' else 'Ruk'},
            'place': {'id': '3', 'name': 'Ash Hollow'}, 'day': 2,
            'relationship': {'trust': 2},
            'facts': [{'kind': 'food_store', 'owner': actor, 'place_id': '3',
                       'place_name': 'Ash Hollow', 'stock': stock, 'target': 75,
                       'unit_price': 2, 'day': 2, 'source': 'observed', 'private': False}]}


def counterfactuals(model, probe):
    request = choose(fixture(hunger=2), [], model, probe)
    if request['intent'] != 'request_food':
        raise ValueError('hungry speaker must request food in this case')
    heard = [{'speaker_id': '1', 'act': request}]
    results = []
    for name, person, expected, quantity in (
            ('severe_shortage', fixture('2', '1', 16), 'offer_food', 1),
            ('small_shortfall', fixture('2', '1', 74), 'offer_food', 3),
            ('empty_purse', fixture('2', '1', 74, 0), 'decline', None),
            ('empty_store', fixture('2', '1', 0), 'decline', None)):
        act = choose(person, heard, model, probe)
        validate(act, person, heard)
        result = {'case': name, 'act': act, 'human': render(act), 'goblin': render(act, 'hrakhor')}
        results.append(result)
        if act['intent'] != expected or (quantity is not None and act['proposal']['quantity'] != quantity):
            raise ValueError('counterfactual choice differs: ' + json.dumps(result))
    return results


def find_pair(folder, food_probe, participant_probe):
    attempts = []
    for seed, days in ((1202, 30), (1202, 180), (11, 60), (77, 120), (1202, 0)):
        path = folder / f'world-{seed}-{days}.ccsave'
        result = subprocess.run([str(food_probe), '--seed', str(seed), '--days', str(days),
                                 '--list', '--save', str(path)], capture_output=True,
                                text=True, timeout=120, check=True)
        people = json.loads(result.stdout)['people']
        world = World(path, food_probe, participant_probe)
        available = [person for person in people if person['alive'] and not person['in_transit']]
        hungry = [person for person in available if person['hungry_days'] > 0]
        for first in hungry:
            for second in available:
                if (first['id'] == second['id'] or first['place_id'] != second['place_id'] or
                        second['hungry_days'] > 0):
                    continue
                try:
                    snapshot = world.snapshot(first['id'], second['id'])
                    listener = snapshot['participants'][1]
                    fact = listener['facts'][0]
                    if listener['self']['coins'] < fact['unit_price'] or (listener.get('relationship') or {}).get('trust', 0) < 0:
                        continue
                    if fact['stock'] < 1:
                        continue
                    return world, first['id'], second['id'], attempts
                except (RuntimeError, subprocess.CalledProcessError, ValueError) as error:
                    attempts.append({'seed': seed, 'days': days, 'first': first['id'],
                                     'second': second['id'], 'error': str(error)})
    raise ValueError('no suitable generated pair found: ' + json.dumps(attempts[-5:]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('run', 'probe', 'food-probe', 'participant-probe'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    model = args.run / 'last.ccv2'
    check_model(model)
    receipt = {'status': 'running', 'counterfactuals': []}
    folder = args.run / 'world-proof'
    folder.mkdir(exist_ok=False)
    try:
        receipt['counterfactuals'] = counterfactuals(model, args.probe)
        world, first, second, attempts = find_pair(folder, args.food_probe, args.participant_probe)
        receipt.update(first=first, second=second, search_attempts=attempts,
                       world=str(world.path), exchange={})
        exchange(world, first, second, model, args.probe, 'hrakhor', receipt=receipt['exchange'])
        result = receipt['exchange']
        execution = result.get('execution') or {}
        if not execution or execution.get('result', {}).get('status') != 'fulfilled':
            raise ValueError('model conversation must complete a native food purchase')
        terms = execution['result']
        before = result['initial']['participants']
        after = result['final']['participants']
        old = {person['self']['id']: person for person in before}
        new = {person['self']['id']: person for person in after}
        if old[terms['payer_id']]['self']['coins'] - new[terms['payer_id']]['self']['coins'] != terms['total_cost']:
            raise ValueError('payer movement differs from agreement')
        if old[first]['facts'][0]['stock'] - new[first]['facts'][0]['stock'] != terms['quantity']:
            raise ValueError('stock movement differs from agreement')
        if new[terms['beneficiary_id']]['self']['hungry_days'] >= old[terms['beneficiary_id']]['self']['hungry_days']:
            raise ValueError('food must relieve the recipient hunger')
        receipt['reunion'] = {}
        exchange(world, first, second, model, args.probe, 'hrakhor', receipt=receipt['reunion'])
        if receipt['reunion']['turns'][0]['act']['intent'] != 'recall_success':
            raise ValueError('reunion must recall the native outcome')
        replay = world.call('--execute', terms['agreement_id'], terms['payer_id'], mutate=True)
        receipt['replay'] = replay
        if world.snapshot(first, second)['state_hash'] != result['final']['state_hash']:
            raise ValueError('repeated execution changed the world')
        receipt['status'] = 'complete'
    except BaseException as error:
        receipt.update(status='failed', error=repr(error))
        raise
    finally:
        (args.run / 'rollouts.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'status': receipt['status'], 'counterfactuals': len(receipt['counterfactuals']),
                      'first': first, 'second': second}))


if __name__ == '__main__':
    main()
