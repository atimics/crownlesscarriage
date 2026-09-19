"""Build single-turn language pairs from authored, source-backed scenarios."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from hrakhor_pairs import pairs

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
from event_facts import EVENT_KIND_REGISTRY

DEFAULT_SOURCE = ROOT / 'tools/data/hrakhor_corpus.json'
RULES = ROOT / 'tools/data/core_account_rules.json'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(data, rules):
    if data.get('schema') != 'crownless.hrakhor_corpus.v1' or not data.get('scenarios'):
        raise ValueError('expected nonempty cultural corpus')
    seen, families, utterances = set(), set(), set()
    for scenario in data['scenarios']:
        rule = rules[scenario['rule']]
        if scenario['id'] in seen or rule['kind'] in families:
            raise ValueError('scenario and event family must be unique across splits')
        seen.add(scenario['id']); families.add(rule['kind'])
        if scenario['split'] not in ('train', 'development', 'test'):
            raise ValueError('unknown split')
        if len(scenario['fields']) != len(rule['roles']):
            raise ValueError('field count differs from event rule')
        if not scenario['turns']:
            raise ValueError('scenario requires speech turns')
        for turn in scenario['turns']:
            if turn['intent'] not in ('report', 'ask', 'propose', 'condition', 'warn', 'express'):
                raise ValueError('unknown intent')
            line = turn['english']
            if not isinstance(line, str) or not line.strip() or any(c in line for c in '\r\n\0'):
                raise ValueError('expected one completed speech turn')
            if line.casefold() in utterances:
                raise ValueError('duplicate utterance across corpus')
            utterances.add(line.casefold())


def build(probe, source=DEFAULT_SOURCE, strength=100):
    data = json.loads(source.read_text())
    rules = {r['id']: r for r in json.loads(RULES.read_text())['rules']}
    validate(data, rules)
    provenance = {'source_sha256': digest(source), 'rules_sha256': digest(RULES),
                  'renderer_sha256': digest(ROOT / 'src/story/cc_hrakhor.c'),
                  'probe_sha256': digest(probe), 'builder_sha256': digest(Path(__file__))}
    for scenario in data['scenarios']:
        rule = rules[scenario['rule']]
        kind = EVENT_KIND_REGISTRY[rule['kind']]
        account = rule['source'].format(*scenario['fields'])
        packet = json.loads(subprocess.check_output(
            [str(probe), str(kind), '80', '0', account, '--packet'], text=True))
        if packet['rule'] != scenario['rule']:
            raise ValueError('native parser selected a different rule')
        # These are authored scenarios, rather than claims about a saved person.
        # The event frame and each language realization have separate fields.
        frame = {'kind': rule['kind'], 'kind_id': kind,
                 'fields': [{'role': role, 'value': value}
                            for role, value in zip(rule['roles'], scenario['fields'])]}
        for index, turn in enumerate(scenario['turns']):
            row = {'kind': kind, 'text': account, 'confidence': 80,
                   'output': turn['english'], 'split': scenario['split']}
            pair = next(pairs([row], probe, strength))
            yield {'schema': 'crownless.hrakhor_turn.v1',
                   'id': f"{scenario['id']}-{index}", 'scenario': scenario['id'],
                   'split': scenario['split'], 'topic': scenario['topic'],
                   'origin': 'authored', 'intent': turn['intent'],
                   'event_frame': frame, 'account_rule': rule['id'],
                   'account_text': account,
                   'english': pair['english'], 'hrakhor': pair['hrakhor'],
                   'strength': strength, 'provenance': provenance}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    parser.add_argument('--strength', type=int, default=100)
    args = parser.parse_args()
    for row in build(args.probe.resolve(strict=True), args.source, args.strength):
        print(json.dumps(row, ensure_ascii=False, sort_keys=True))


if __name__ == '__main__':
    main()
