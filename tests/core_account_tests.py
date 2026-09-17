"""Independent native checks of source roles, complete matching, and omissions."""
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
data = json.loads((root / 'tools/data/core_account_rules.json').read_text())
kinds = {row['kind']: row['value'] for row in json.loads(
    (root / 'docs/core-account-coverage.json').read_text())['events']}
values = {'actor': 'Mara Venn', 'recipient': 'Tomas Rill', 'place': 'Thornford',
          'object': 'Sun Cup', 'group': 'Willow Republic', 'material': 'Wood',
          'detail': 'calves', 'quantity': '7'}
count = 0
for rule in data['rules']:
    fields = [values[x] for x in rule['roles']]
    for slot, allowed in rule.get('allowed', {}).items():
        fields[int(slot)] = allowed[0]
    if 'less_than' in rule: fields[rule['less_than'][1]] = '8'
    source = rule['source'].format(*fields)
    for variant, template in enumerate(rule['outputs']):
        args = [sys.argv[1], str(kinds[rule['kind']]), '80', str(variant), source]
        output = subprocess.check_output(args, text=True).rstrip('\n')
        assert output == template.format(*fields), (rule['id'], output)
        packet = json.loads(subprocess.check_output([*args, '--packet'], text=True))
        assert packet['rule'] == rule['id']
        for field in packet['fields']:
            assert source.encode()[field['start']:field['end']].decode() == field['text']
            assert field['spoken'] == any('{' + str(field['field']) + '}' in t for t in rule['outputs'])
        # Extra claims and malformed numeric fields require their own rules.
        assert subprocess.run([*args[:4], source + ' A different event happened.'],
                              capture_output=True).returncode == 1
        if 'less_than' in rule:
            left, right = rule['less_than']
            for amount in ('8', '9', 'eight', 'nine'):
                altered = list(fields)
                altered[left] = amount
                assert subprocess.run([*args[:4], rule['source'].format(*altered)],
                                      capture_output=True).returncode == 1
        for slot, role in enumerate(rule['roles']):
            if slot in rule.get('positive', []):
                altered = list(fields)
                altered[slot] = '0'
                assert subprocess.run([*args[:4], rule['source'].format(*altered)],
                                      capture_output=True).returncode == 1
            if str(slot) in rule.get('allowed', {}):
                altered = list(fields)
                altered[slot] = 'wolves'
                assert subprocess.run([*args[:4], rule['source'].format(*altered)],
                                      capture_output=True).returncode == 1
            if role == 'quantity':
                altered = list(fields)
                altered[slot] = 'eight' if fields[slot] == '8' else 'seven'
                spoken = subprocess.check_output([*args[:4], rule['source'].format(*altered)], text=True).strip()
                assert spoken == output
                altered = list(fields)
                altered[slot] = 'many'
                assert subprocess.run([*args[:4], rule['source'].format(*altered)],
                                      capture_output=True).returncode == 1
            elif role in ('actor', 'recipient'):
                omitted = subprocess.check_output([*args, str(slot)], text=True).strip()
                altered = list(fields)
                altered[slot] = 'someone'
                expected = template.format(*altered)
                expected = expected[0].upper() + expected[1:]
                assert omitted == expected, (rule['id'], omitted, expected)
        count += 1
print(f'{count} native grammar forms passed with role and malformed-input checks')

# The inverse: a spoken rendering parses back to its event. Kind-constrained
# where it matters, because two kinds (goblin and settlement raid) render
# identically and only the actor's type tells them apart -- the sim knows its
# own actor, so it parses with the kind in hand.
inverse = 0
for rule in data['rules']:
    fields = [values[x] for x in rule['roles']]
    for slot, allowed in rule.get('allowed', {}).items():
        fields[int(slot)] = allowed[0]
    if 'less_than' in rule: fields[rule['less_than'][1]] = '8'
    for template in rule['outputs'] + [rule['challenge']]:
        speech = template.format(*fields)
        parsed = json.loads(subprocess.check_output(
            [sys.argv[1], '--parse-kind', str(kinds[rule['kind']]), speech], text=True))
        assert parsed['kind'] == kinds[rule['kind']], (rule['id'], speech, parsed)
        inverse += 1
        # Every recovered span slices the speech it was parsed from.
        raw = speech.encode()
        for field in parsed['fields']:
            assert raw[field['start']:field['end']].decode() == field['text'], (rule['id'], field)
            # When the parse lands on the rendered rule, the slots that the
            # template carries recover exactly the values it was written from.
        if parsed['rule'] == rule['id']:
            used = {int(x) for x in re.findall(r'\{(\d)\}', template)}
            for field in parsed['fields']:
                if field['field'] in used:
                    assert field['text'] == fields[field['field']], (rule['id'], field, fields)
        # A hedged or retold telling carries a known tail in place of the period.
        if speech.endswith('.'):
            for tail in (', so people say.', ', if the story is right.',
                         ', according to the word going round.'):
                parses = subprocess.run(
                    [sys.argv[1], '--parse-kind', str(kinds[rule['kind']]), speech[:-1] + tail],
                    capture_output=True, text=True)
                assert parses.returncode == 0, (rule['id'], speech[:-1] + tail)
# The inverse abstains rather than guessing at speech no rule renders.
assert subprocess.run([sys.argv[1], '--parse', 'nothing here matches a rule.'],
                      capture_output=True).returncode == 1
assert subprocess.run([sys.argv[1], '--parse-kind', '0', 'nothing here matches a rule.'],
                      capture_output=True).returncode == 1
print(f'{inverse} spoken renderings parsed back to their event, tails and abstention checked')

