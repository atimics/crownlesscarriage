"""Build a diverse micro-context corpus from real simulation character state.

Each example packs a character's mind (goal, stress, courage, memories,
thoughts), their held account with certainty cues (? uncertain, ~ widely
retold, ! witnessed), and optional conversation history into a standard
micro context. The model continues one line: either a # thought: line
(internal monologue) or a spoken self:/other: line.

Sim-derived fields come from the Crownless simulation exporter. Thoughts and
conversation responses are authored from that state. The output rows match the
format consumed by the ZERO conversation trainer.
"""
import argparse
import hashlib
import itertools
import json
import random
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SLOTS = re.compile(r'\{(\d)\}')
ROLE_IDS = {'none': 0, 'actor': 1, 'recipient': 2, 'place': 3, 'object': 4,
            'group': 5, 'material': 6, 'detail': 7, 'quantity': 8}
SPLITS = ('train', 'validation', 'test')

DANGER = {'GOBLIN_RAIDED', 'SETTLEMENT_RAIDED', 'BANDIT_PRESSURE',
          'BANDIT_RAID_DEPARTED', 'BANDIT_RAID_RETURNED', 'DRAGON_BROOD',
          'DRAGON_OMEN', 'DRAGON_RETALIATION', 'DRAGON_TERRITORY_LOST',
          'DRAGON_SLAIN', 'DRAGON_BATTLE', 'DRAGON_MUSTERED', 'COURIER_LOST',
          'WAR_DECLARED', 'GOBLIN_RAID_DEPARTED', 'DRAGON_HOARD_STOLEN',
          'DRAGON_TREASURE_RETURNED', 'DRAGON_HOARD_DEFENDED'}
SCARCITY = {'SHORTAGE', 'HARVEST_FAILED', 'SHEEP_SLAUGHTERED', 'COW_SLAUGHTERED'}
GOOD = {'NOTICE_POSTED', 'PEACE_DECLARED', 'ALLIANCE_DECLARED', 'TREASURE_CRAFTED',
        'SHEEP_BRED', 'COW_CALVING', 'BAKERY_PRODUCTION', 'QUARRY_OUTPUT',
        'WOODLOT_HARVEST', 'SHEEP_SHEARED', 'PAPER_MILLED', 'HORSE_BRED',
        'FOAL_BORN', 'GOBLIN_TRADE', 'COURIER_ARRIVED', 'COURIER_DEPARTED',
        'CHARACTER_BORN', 'KING_ANOINTED'}

# Authored thought families keyed by goal and event family. Each entry is a
# list of templates; {place} and {actor} may be filled from the held account.
THOUGHTS = {
    ('secure_livelihood', 'danger'): [
        'If the {actor} keep coming, my family will starve.',
        'I need to hide what little we have before they come again.',
        'The raiders could take everything we have saved.',
    ],
    ('secure_livelihood', 'scarcity'): [
        'I need to find food before the stores run dry.',
        'There is not enough bread to last the winter.',
        'Every loaf counts now that food is short.',
    ],
    ('secure_livelihood', 'good'): [
        'At last, something to be thankful for.',
        'This takes a weight off my shoulders.',
        'Good news means a safer season ahead.',
    ],
    ('survive_crisis', 'danger'): [
        'I should find shelter before nightfall.',
        'The {actor} could be here any day. I have to be ready.',
        'I have to keep my family out of the way of this.',
    ],
    ('survive_crisis', 'scarcity'): [
        'I have to ration what we have.',
        'Every scrap counts now.',
        'We cannot afford to waste anything this season.',
    ],
    ('survive_crisis', 'good'): [
        'Maybe things are turning around.',
        'A little good news goes a long way.',
        'This gives us a chance to recover.',
    ],
    ('carry_news', 'danger'): [
        'This news must reach the next town before the {actor} do.',
        'Someone has to warn the other settlements.',
        'People need to know what is coming.',
    ],
    ('carry_news', 'scarcity'): [
        'People need to know how bad the harvest was.',
        'The next town must hear about the shortage.',
        'If no one carries the word, towns will starve quietly.',
    ],
    ('carry_news', 'good'): [
        'This is the kind of news people want to hear.',
        'Good news travels fast; I should spread it.',
        'The whole valley should know about this.',
    ],
    ('keep_order', 'danger'): [
        'The town needs to post guards before the next raid.',
        'We must keep order or the panic will do more harm than the {actor}.',
        'Someone has to keep the town calm through this.',
    ],
    ('keep_order', 'scarcity'): [
        'The stores must be guarded or people will riot.',
        'We need to share the food fairly or there will be trouble.',
        'Rationing has to be seen as fair or the town will split.',
    ],
    ('keep_order', 'good'): [
        'Good news will settle the town down.',
        "This will lift everyone's spirits.",
        'A steady hand now keeps the peace later.',
    ],
}
STRESS_PREFIX = {
    'high': ['My hands are shaking. ', 'I can barely think. ', 'My heart is pounding. '],
    'medium': ['I need to stay calm. ', 'I should think this through. ', ''],
    'low': ['Perhaps it will pass. ', 'I will keep calm. ', ''],
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_hash(path):
    with Path(path).open('rb') as stream:
        checksum = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            checksum.update(chunk)
        return checksum.hexdigest()


def world_split(seed):
    bucket = int(digest(f'crownless-core-v1:{seed}'.encode())[:8], 16) % 100
    return 'train' if bucket < 80 else 'validation' if bucket < 90 else 'test'


def render(template, values, roles):
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


def family(kind):
    if kind in DANGER: return 'danger'
    if kind in SCARCITY: return 'scarcity'
    if kind in GOOD: return 'good'
    return 'neutral'


def author_thoughts(row, rng):
    """One or two authored thoughts grounded in the character's goal, the event
    family, and stress. A neutral family keeps a simple goal-flavoured line."""
    mind = row['mind']
    key = (mind['goal'], family(row['kind']))
    pool = THOUGHTS.get(key, ['I should keep an eye on how this turns out.'])
    thought = rng.choice(pool)
    values = {}
    for field in row['fields']:
        if field['role'] in (1, 3): values.setdefault(field['role'], field['text'])
    thought = thought.format(actor=values.get(1, 'raiders'), place=values.get(3, 'the towns'))
    prefix = rng.choice(STRESS_PREFIX[mind['stress']])
    return [prefix + thought]


def collect_sim(binary, seeds, days):
    rows = []
    for seed in seeds:
        process = subprocess.Popen([str(binary), '--seed', str(seed), '--days', str(days)],
                                   stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        for line in process.stdout:
            rows.append(json.loads(line))
        process.stdout.close()
        if process.wait() != 0:
            raise RuntimeError(f'World {seed} failed')
    return rows


def parse_packet(probe, kind, confidence, account):
    result = subprocess.run([str(probe), str(kind), str(confidence), '0', account, '--packet'],
                            capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return json.loads(result.stdout)


def parse_accounts(batch_binary, rows):
    """Parse every unique held account through the grammar in one batch call."""
    unique = {}
    for row in rows:
        kind = int(row['rule'].split(':')[0])
        account = row['input']['account']
        key = (kind, account)
        unique.setdefault(key, row)
    lines = ''.join(f'{kind}\t{row["input"]["confidence"]}\t{account}\n'
                    for (kind, account), row in unique.items())
    process = subprocess.Popen([str(batch_binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, text=True)
    out, _ = process.communicate(lines)
    if process.returncode != 0:
        raise RuntimeError('Batch account parser failed')
    packets = {}
    for (kind, account), packet_json in zip(unique, out.splitlines()):
        packet = json.loads(packet_json)
        if packet:
            packets[(kind, account)] = packet
    return packets


def build_base_row(sim_row, packet, rules, rng, split, index, override=None):
    rule = rules[packet['rule']]
    values = {}
    for field in packet['fields']:
        values[field['field']] = field['text']
    confidence = sim_row['input']['confidence']
    retold = sim_row['input']['retellings'] >= 4
    witnessed = sim_row['mind']['witnessed']
    variant = sim_row['input']['variant']
    cue = ('? ' if confidence < 40 else '') + ('~ ' if retold else '') + ('! ' if witnessed else '')
    # Mind lines precede the held account; memories come from the character's
    # other held events, thoughts are authored from goal + stress + event.
    mind = sim_row['mind']
    if override:
        mind = dict(mind, **{k: v for k, v in override.items() if v is not None})
        witnessed = mind['witnessed']
        cue = ('? ' if confidence < 40 else '') + ('~ ' if retold else '') + ('! ' if witnessed else '')
    # How much the character recalls varies: a resident met on the first day
    # holds nothing, so zero, one, and two memories all have to be common.
    memories = [e['text'] for e in sim_row['events'][:-1]][-2:]
    memories = memories[len(memories) - rng.choice((0, 1, 2)):] if memories else []
    lines = []
    lines.append('# goal: ' + mind['goal'])
    lines.append('# stress: ' + mind['stress'])
    lines.append('# courage: ' + mind['courage'])
    lines.extend('# memory: ' + m for m in memories)
    target, output_spans = render(rule['outputs'][variant], values, rule['roles'])
    target = target[0].upper() + target[1:]
    if confidence < 40:
        target = target[:-1] + (', if the story is right.' if variant == 0 else ', if the rumour is true.')
    elif retold:
        target = target[:-1] + (', so people say.' if variant == 0 else ', according to the word going round.')
    row = {'schema': 'crownless.core_mind.v1', 'id': f'{split}:{index}', 'rule': rule['id'],
           'kind': rule['kind'], 'confidence': confidence, 'retold': retold, 'variant': variant,
           'witnessed': witnessed, 'output': target, 'mind': dict(mind, memories=memories, thoughts=[])}
    voice = override.get('voice') if override else None
    row['voice'] = voice or ('bandit' if mind.get('bandit') else
                             (mind.get('occupation') or mind.get('role') or 'resident'))
    row['fields'] = []
    for field in packet['fields']:
        row['fields'].append({**field, 'start': field['start'], 'end': field['end'],
                              'knowledge': 2 if confidence < 40 else 0, 'event': 1})
    row['mind']['thoughts'] = author_thoughts(row, rng)
    lines.extend('# thought: ' + t for t in row['mind']['thoughts'])
    prefix = '\n'.join(lines) + '\n- ' + cue + packet['text'] + '\n'
    offset = len(prefix) - len(packet['text']) - 1
    fields = []
    for field in packet['fields']:
        fields.append({**field, 'start': field['start'] + offset, 'end': field['end'] + offset,
                       'knowledge': 2 if confidence < 40 else 0, 'event': 1})
    copies = []
    for span in output_spans:
        if span['role'] not in (0, 8):
            copies.append({**span, 'spoken': True})
    row['prefix'] = prefix
    row['fields'] = fields
    row['copies'] = copies
    return row


# Balanced mind assignments cycle through every goal, stress, and witnessed
# combination so the model sees all of them, not just the sim's common cases.
BALANCE = [
    ('secure_livelihood', 'low', False), ('secure_livelihood', 'high', True),
    ('survive_crisis', 'high', False), ('survive_crisis', 'medium', True),
    ('carry_news', 'medium', False), ('carry_news', 'low', True),
    ('keep_order', 'low', False), ('keep_order', 'high', True),
]


def build(output, probe, batch_binary, seeds, days, limit=25000, seed=20260919):
    if output.exists():
        raise ValueError('Choose a fresh output directory')
    binary = ROOT / 'out/build/demo/crownless_gossip_corpus'
    rules_data = json.loads((ROOT / 'tools/data/core_account_rules.json').read_text())
    rules = {r['id']: r for r in rules_data['rules']}
    sim_rows = collect_sim(binary, seeds, days)
    packets = parse_accounts(batch_binary, sim_rows)
    output.mkdir(parents=True)
    (output / 'rules.json').write_bytes((ROOT / 'tools/data/core_account_rules.json').read_bytes())
    reports = {}
    used_names = {}
    held_out = max(100, limit // 20)
    split_limits = {'train': limit, 'validation': held_out, 'test': held_out}
    for split in SPLITS:
        split_rows = [r for r in sim_rows if world_split(r['provenance']['world_seed']) == split]
        # The sim emits some kinds far more often than others. Interleaving by
        # kind makes the take-until-limit below a balanced sample instead of a
        # census dominated by whatever the world happens to do most.
        by_kind = {}
        for row in split_rows:
            by_kind.setdefault(int(row['rule'].split(':')[0]), []).append(row)
        split_rows = [row for group in itertools.zip_longest(*(by_kind[k] for k in sorted(by_kind)))
                      for row in group if row is not None]
        rng = random.Random(f'{seed}:{split}')
        voices = sorted({('bandit' if r['mind'].get('bandit') else r['mind'].get('occupation') or r['mind'].get('role') or 'resident')
                         for r in split_rows})
        rows, names = [], set()
        for index, sim_row in enumerate(split_rows):
            kind = int(sim_row['rule'].split(':')[0])
            packet = packets.get((kind, sim_row['input']['account']))
            if packet is None or packet['rule'] not in rules:
                continue
            override = dict(zip(('goal', 'stress', 'witnessed'),
                                BALANCE[index % len(BALANCE)]))
            override['voice'] = voices[index % len(voices)]
            row = build_base_row(sim_row, packet, rules, rng, split, index, override)
            names.update(f['text'] for f in row['fields'] if f['role'] in (1, 2, 3, 4, 5))
            rows.append(row)
            if len(rows) >= split_limits[split]:
                break
        path = output / f'{split}.jsonl'
        path.write_text(''.join(json.dumps(r) + '\n' for r in rows))
        reports[split] = {'rows': len(rows), 'sha256': file_hash(path),
                          'kinds': sorted(set(r['kind'] for r in rows)),
                          'goals': {g: sum(r['mind']['goal'] == g for r in rows)
                                    for g in sorted({r['mind']['goal'] for r in rows})}}
        used_names[split] = names
    overlap = {a: sorted(used_names[a] & used_names[b]) for a, b in
               [('train', 'validation'), ('train', 'test'), ('validation', 'test')]}
    manifest = {'schema': 'crownless.core_mind.v1', 'seed': seed, 'splits': reports,
                'grammar_sha256': digest((ROOT / 'tools/data/core_account_rules.json').read_bytes()),
                'builder_sha256': digest(Path(__file__).read_bytes()),
                'source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
                'name_overlap': {k: len(v) for k, v in overlap.items()},
                'scope': 'Sim-derived goals/stress/memories with authored thoughts and conversations in a standard micro context. Names recur across splits by design; the model copies them from source fields.'}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({s: {'rows': r['rows'], 'goals': r['goals']} for s, r in reports.items()}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--probe', type=Path, default=ROOT / 'out/build/demo/core_account_probe')
    parser.add_argument('--batch', type=Path, default=ROOT / 'out/build/demo/core_account_batch')
    parser.add_argument('--first-seed', type=int, default=1)
    parser.add_argument('--seeds', type=int, default=48)
    parser.add_argument('--days', type=int, default=120)
    parser.add_argument('--limit', type=int, default=25000)
    parser.add_argument('--seed', type=int, default=20260919)
    args = parser.parse_args()
    seeds = [((args.first_seed + i) * 0x9E3779B9) & 0xFFFFFFFF for i in range(args.seeds)]
    build(args.output, args.probe, args.batch, seeds, args.days, args.limit, args.seed)


if __name__ == '__main__':
    main()