"""Run one independently conditioned model turn at a time for two people."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from paired_participants import validate_snapshot
from policy import encode_input, decode_choice, make_act, decide, pack_act
from policy_language import render


def run(snapshot, model=None, probe=None, language='human', language_probe=None, goal=None, limit=14):
    validate_snapshot(snapshot)
    if not 1 <= limit <= 16: raise ValueError('turn limit must be 1..16')
    if (model is None) != (probe is None): raise ValueError('model and native probe belong together')
    if model:
        manifest = json.loads(Path(model).with_name('manifest.json').read_text())
        model_hash = hashlib.sha256(Path(model).read_bytes()).hexdigest()
        policy_hash = hashlib.sha256(Path(__file__).with_name('policy.py').read_bytes()).hexdigest()
        policy_sources = [value for path, value in manifest.get('sources', {}).items()
                          if Path(path).name == 'policy.py']
        if (manifest.get('format') != 'crownless-policy-v2' or manifest.get('status') != 'complete' or
                manifest.get('export_sha256') != model_hash or policy_sources != [policy_hash]):
            raise ValueError('model manifest must match the current policy and model')
    history = []; turns = []
    for index in range(limit):
        person = snapshot['participants'][index % 2]
        requested = goal if not history else None
        prefix = encode_input(person, history, requested)
        if model:
            result = subprocess.run([str(probe), str(model), '--policy-prefix',
                ','.join(map(str, prefix)), '--generate'], capture_output=True, text=True, timeout=30, check=True)
            ids = [int(token) for token in result.stdout.split()]
            act = make_act(decode_choice(ids), person, history, requested)
        else:
            act = decide(person, history, requested); ids = None
        text = render(act, person, history, requested, language, language_probe, index % 2)
        turns.append({'speaker': person['self']['name'], 'speaker_id': person['self']['id'],
                      'act': act, 'ids': ids, 'record_hex': pack_act(act).hex(), 'speech': text})
        history.append({'speaker_id': person['self']['id'], 'act': act})
        if act['intent'] == 'end': break
    return {'format': 'crownless-policy-v2', 'source': 'native-model' if model else 'procedural-policy',
            'model_sha256': hashlib.sha256(Path(model).read_bytes()).hexdigest() if model else None,
            'turns': turns, 'ended': turns[-1]['act']['intent'] == 'end',
            'world_seed': snapshot['world_seed'], 'state_hash': snapshot['state_hash']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--model', type=Path)
    parser.add_argument('--probe', type=Path)
    parser.add_argument('--language-probe', type=Path)
    parser.add_argument('--language', choices=('human', 'goblin'), default='human')
    parser.add_argument('--goal')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = run(json.loads(args.snapshot.read_text()), args.model, args.probe,
                 args.language, args.language_probe, args.goal)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    for turn in result['turns']: print(turn['speaker'] + ': ' + turn['speech'])


if __name__ == '__main__': main()
