"""Build paired account examples from the same grammar used by NPC speech."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SLOTS = re.compile(r'\{(\d)\}')
ROLE_IDS = {'none': 0, 'actor': 1, 'recipient': 2, 'place': 3, 'object': 4,
            'group': 5, 'material': 6, 'detail': 7, 'quantity': 8}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def render(template, values, roles):
    """Return text and exact byte spans, including repeated field mentions."""
    text, spans, at = '', [], 0
    for match in SLOTS.finditer(template):
        text += template[at:match.start()]
        slot = int(match[1])
        start = len(text.encode())
        text += values[slot]
        spans.append({'field': slot, 'role': ROLE_IDS[roles[slot]], 'start': start,
                      'end': len(text.encode()), 'text': values[slot]})
        at = match.end()
    text += template[at:]
    return text, spans


def name(rng, split, role):
    # Split-specific opening syllables separate complete names before generation.
    first = {'train': ['Bel', 'Tar', 'Fen', 'Nor'], 'validation': ['Vel', 'Zar'],
             'test': ['Kel', 'Yor']}[split]
    middle = ['a', 'en', 'or', 'il', 'um', 'eth', 'ow', 'ash']
    end = ['ford', 'wick', 'mere', 'holt', 'gate', 'den', 'fell', 'moor']
    word = lambda: rng.choice(first) + ''.join(rng.choices(middle, k=2)) + rng.choice(end)
    if role == 'quantity': return str(rng.randrange(2, 200))
    if role == 'material': return rng.choice(['Wood', 'Rags'])
    if role == 'detail': return 'calves'
    if role == 'group': return 'The ' + word() + ' Court'
    if role == 'object': return word() + ' ' + rng.choice(['Cup', 'Crown', 'Book'])
    return word() + (' ' + word() if role in ('actor', 'recipient') else '')


def make_row(rule, values, confidence, retold, variant, pair, member, challenge=False,
             unknown=None, distractor=None):
    source, spans = render(rule['challenge'] if challenge else rule['source'], values, rule['roles'])
    visible = list(values)
    # Partial-field examples carry the partial information in the visible input too.
    if unknown is not None:
        visible[unknown] = 'someone'
        source, spans = render(rule['challenge'] if challenge else rule['source'], visible, rule['roles'])
    target, output_spans = render(rule['outputs'][variant], visible, rule['roles'])
    target = target[0].upper() + target[1:]
    if confidence < 40:
        target = target[:-1] + (', if the story is right.' if variant == 0 else ', if the rumour is true.')
    elif retold:
        target = target[:-1] + (', so people say.' if variant == 0 else ', according to the word going round.')
    context = '' if distractor is None else '- ' + distractor + '\n'
    cue = ('? ' if confidence < 40 else '') + ('~ ' if retold else '')
    prefix = context + '- ' + cue + source + '\n'
    offset = len((context + '- ' + cue).encode())
    fields = []
    for span in spans:
        slot = span['field']
        fields.append({**span, 'start': span['start'] + offset, 'end': span['end'] + offset,
                       'spoken': any('{' + str(slot) + '}' in t for t in rule['outputs']),
                       'knowledge': 3 if slot == unknown else (2 if confidence < 40 else 0),
                       'provenance': 3, 'event': 2 if distractor else 1})
    # A span can be copied only when its complete source field is available.
    copies = []
    for span in output_spans:
        if span['field'] != unknown and span['role'] not in (0, 8):
            copies.append({**span, 'spoken': True})
    return {'schema': 'crownless.core_pair.v2', 'id': f'{pair}:{member}', 'pair': pair,
            'rule': rule['id'], 'kind': rule['kind'], 'prefix': prefix, 'output': target,
            'fields': fields, 'copies': copies, 'variant': variant,
            'confidence': confidence, 'retold': retold, 'unknown': unknown,
            'source_mode': 'authored_schema_counterfactual', 'challenge': challenge}


def build(output, pairs=12500, seed=20260912):
    if output.exists(): raise ValueError('Choose a fresh output directory')
    if pairs < 100: raise ValueError('Use at least 100 pairs')
    rules_path = ROOT / 'tools/data/core_account_rules.json'
    data = json.loads(rules_path.read_text())
    output.mkdir(parents=True)
    (output / 'rules.json').write_bytes(rules_path.read_bytes())
    reports = {}
    used_names = {}
    for split, count in [('train', pairs), ('validation', max(100, pairs // 20)),
                         ('test', max(100, pairs // 20))]:
        rng = random.Random(f'{seed}:{split}')
        rows, names = [], set()
        for i in range(count):
            rule = data['rules'][i % len(data['rules'])]
            values = [name(rng, split, role) for role in rule['roles']]
            if rule['kind'] == 'FOAL_BORN': values[2] = 'colt'
            if rule['kind'] == 'SHEEP_BRED': values[2], values[4] = 'lambs', 'yearlings'
            if rule['kind'] == 'SHEEP_SLAUGHTERED': values[2] = 'after winter fodder runs short'
            if rule['id'] == 'horse_bred_1': values[2] = 'foals'
            if rule['kind'] == 'DRAGON_SLAIN': values[2] = 'the dragon falls'
            if rule['kind'] in ('GOBLIN_RAIDED', 'SETTLEMENT_RAIDED') and len(values) == 3:
                values[2] = 'food from the stores'
            if 'less_than' in rule:
                left, right = rule['less_than']
                values[right] = str(int(values[left]) + rng.randrange(1, 30))
            confidence = 20 if rng.randrange(4) == 0 else 80
            retold, variant = rng.randrange(3) == 0, rng.randrange(2)
            partial = rule['kind'] in ('NOTICE_POSTED', 'CHARACTER_DIED', 'CHARACTER_BORN',
                                       'ROYAL_SUCCESSION', 'MONASTIC_SUCCESSION', 'KING_ANOINTED')
            unknown = next((j for j, role in enumerate(rule['roles']) if role == 'actor'), None) if partial and rng.randrange(7) == 0 else None
            eligible = [j for j, role in enumerate(rule['roles']) if role not in ('quantity', 'detail', 'material') and j != unknown]
            named = [j for j, role in enumerate(rule['roles']) if role not in ('quantity', 'detail', 'material')]
            empty = ['' if j in named else value for j, value in enumerate(values)]
            limit = min(20, (143 - len(rule['source'].format(*empty).encode())) // max(1, len(named)))
            if limit < 6: raise ValueError('Account grammar leaves too little space for names')
            for j in named: values[j] = values[j][:limit].rstrip()
            changed = rng.choice(eligible) if eligible else None
            distractor = None
            if i % 3:
                earlier = data['rules'][(i + 7) % len(data['rules'])]
                earlier_values = [name(rng, split, role) for role in earlier['roles']]
                distractor = earlier['outputs'][0].format(*earlier_values)
            for member in range(2):
                altered = list(values)
                if member and changed is not None:
                    while altered[changed] == values[changed]:
                        altered[changed] = name(rng, split, rule['roles'][changed])[:limit].rstrip()
                names.update(v for v, role in zip(altered, rule['roles']) if role in ('actor', 'recipient', 'place', 'object', 'group'))
                pair = f'{split}:{i}'
                row = make_row(rule, altered, confidence, retold, variant, pair, member,
                               unknown=unknown, distractor=distractor)
                row['changed_field'] = changed
                if len(rule['source'].format(*altered).encode()) > 143:
                    raise ValueError('Held account exceeds the game text capacity')
                rows.append(row)
                if split == 'test':
                    challenge = make_row(rule, altered, confidence, retold, variant,
                                         pair, member, True, unknown, distractor)
                    challenge['changed_field'] = changed
                    with (output / 'wording.jsonl').open('a') as stream:
                        stream.write(json.dumps(challenge) + '\n')
        path = output / f'{split}.jsonl'
        path.write_text(''.join(json.dumps(row) + '\n' for row in rows))
        (output / f'{split}.txt').write_text(''.join(row['prefix'] + row['output'] + '\n\n' for row in rows))
        reports[split] = {'rows': len(rows), 'pairs': count, 'sha256': digest(path),
                          'target_bytes': sum(len(row['output'].encode()) for row in rows),
                          'kinds': sorted(set(row['kind'] for row in rows))}
        used_names[split] = names
    for a, b in [('train', 'validation'), ('train', 'test'), ('validation', 'test')]:
        if used_names[a] & used_names[b]: raise ValueError('Complete names overlap across splits')
    reports['wording'] = {'rows': reports['test']['rows'], 'sha256': digest(output / 'wording.jsonl')}
    manifest = {'schema': 'crownless.core_diagnostic.v2', 'seed': seed, 'splits': reports,
                'grammar_sha256': digest(rules_path), 'builder_sha256': digest(Path(__file__)),
                'source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
                'coverage': json.loads((ROOT / 'docs/core-account-coverage.json').read_text()),
                'scope': 'Synthetic field substitutions in shared game rules; reserved input paraphrases in wording split.'}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(reports, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pairs', type=int, default=12500)
    parser.add_argument('--seed', type=int, default=20260912)
    args = parser.parse_args()
    build(args.output, args.pairs, args.seed)
