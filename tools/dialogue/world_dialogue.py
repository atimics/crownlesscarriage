"""Run typed participant exchanges against a saved native Crownless world."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess

from meaning import FORMAT, choose, facts, validate
from meaning_language import render


class World:
    def __init__(self, path, food_probe, participant_probe):
        self.path = Path(path)
        self.food_probe = str(food_probe)
        self.participant_probe = str(participant_probe)

    def call(self, *args, mutate=False):
        command = [self.food_probe, '--load', str(self.path), *map(str, args)]
        if mutate:
            command += ['--save', str(self.path)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise RuntimeError(result.stderr.strip() or 'native world command failed')
        return json.loads(result.stdout)

    def snapshot(self, first, second):
        result = subprocess.run([
            self.participant_probe, '--load', str(self.path), '--first', str(first), '--second', str(second)
        ], capture_output=True, text=True, timeout=60, check=True)
        snapshot = json.loads(result.stdout)
        for person in snapshot['participants']:
            own = person['self']['id']
            other = person['listener']['id']
            observed = self.call('--observe', own, other)
            person['facts'] = [{
                'kind': 'food_store', 'owner': own, 'place_id': observed['place_id'],
                'place_name': observed['place_name'], 'stock': observed['stock'],
                'target': observed['reserve_target'], 'unit_price': observed['unit_price'],
                'day': observed['day'], 'source': 'observed', 'private': False,
            }]
            outcomes = self.call('--outcomes', own, other)
            person['outcomes'] = outcomes['outcomes']
            facts(person)
        return snapshot

    def execute(self, terms):
        if terms['condition'] != 'now':
            raise ValueError('wait until the stated condition is available')
        observed = self.call('--observe', terms['payer_id'], terms['beneficiary_id'])
        if terms['place_id'] != observed['place_id']:
            raise ValueError('agreement place changed')
        promised = self.call('--promise', terms['payer_id'], terms['beneficiary_id'],
                             terms['quantity'], terms['unit_price'], mutate=True)
        accepted = self.call('--accept', promised['agreement_id'], terms['beneficiary_id'], mutate=True)
        result = self.call('--execute', promised['agreement_id'], terms['payer_id'], mutate=True)
        return {'promise': promised, 'accepted': accepted, 'result': result}


def check_model(model):
    if model is None:
        return
    manifest = json.loads(Path(model).with_name('manifest.json').read_text())
    source = Path(__file__).with_name('meaning.py')
    hashes = [h for path, h in manifest.get('sources', {}).items() if Path(path).name == 'meaning.py']
    if (manifest.get('format') != FORMAT or manifest.get('status') != 'complete' or
            manifest.get('export_sha256') != hashlib.sha256(Path(model).read_bytes()).hexdigest() or
            hashes != [hashlib.sha256(source.read_bytes()).hexdigest()]):
        raise ValueError('model and meaning contract must match their manifest')


def exchange(world, first, second, model=None, probe=None, language='human', limit=12, receipt=None):
    if (model is None) != (probe is None):
        raise ValueError('model and probe belong together')
    if not 1 <= limit <= 12:
        raise ValueError('bounded turn limit required')
    check_model(model)
    history = []
    turns = []
    execution = None
    initial = world.snapshot(first, second)
    receipt = {} if receipt is None else receipt
    receipt.update(format=FORMAT, policy='native-model' if model else 'procedural-teacher',
                   language=language, initial=initial, turns=turns, execution=None, ended=False)
    for index in range(limit):
        snapshot = world.snapshot(first, second)
        person = snapshot['participants'][index % 2]
        act = choose(person, history, model, probe)
        validate(act, person, history)
        turns.append({'speaker': person['self']['name'], 'act': act,
                      'speech': render(act, language, speaker_id=person['self']['id'])})
        history.append({'speaker_id': act['actor'], 'act': copy.deepcopy(act)})
        if act['intent'] == 'accept':
            execution = world.execute(act['proposal'])
            receipt['execution'] = execution
        if act['intent'] == 'end':
            break
    receipt.update(final=world.snapshot(first, second),
                   ended=bool(turns and turns[-1]['act']['intent'] == 'end'))
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('world', 'food-probe', 'participant-probe', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--first', required=True)
    parser.add_argument('--second', required=True)
    parser.add_argument('--model', type=Path)
    parser.add_argument('--probe', type=Path)
    parser.add_argument('--language', default='human')
    parser.add_argument('--reunion', action='store_true')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('use a fresh output path to preserve results')
    world = World(args.world, args.food_probe, args.participant_probe)
    receipt = {'status': 'running', 'world': str(args.world),
               'world_before_sha256': hashlib.sha256(args.world.read_bytes()).hexdigest()}
    try:
        receipt['exchange'] = {}
        exchange(world, args.first, args.second, args.model, args.probe,
                 args.language, receipt=receipt['exchange'])
        if args.reunion:
            receipt['reunion'] = {}
            exchange(world, args.first, args.second, args.model, args.probe,
                     args.language, receipt=receipt['reunion'])
        receipt['status'] = 'complete'
    except BaseException as error:
        receipt.update(status='failed', error=repr(error))
        raise
    finally:
        receipt['world_after_sha256'] = hashlib.sha256(args.world.read_bytes()).hexdigest()
        args.output.write_text(json.dumps(receipt, indent=2) + '\n')
    for name in ('exchange', 'reunion'):
        for turn in receipt.get(name, {}).get('turns', []):
            print(turn['speaker'] + ': ' + turn['speech'])


if __name__ == '__main__':
    main()
