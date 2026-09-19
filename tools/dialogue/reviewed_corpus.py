#!/usr/bin/env python3
"""Assemble reviewed actor turns whose cited evidence survives compact input."""
import argparse
import hashlib
import json
from pathlib import Path

from paired_participants import digest
from participant_training import NativeTokenizer, compile_row

SELF_FIELDS = ('name', 'occupation', 'age', 'goal', 'activity', 'stress', 'courage',
               'hungry_days', 'unsheltered_nights', 'coins', 'in_transit', 'home')


def retained_evidence(row, packed):
    request = row['input']
    person = request['participant']
    retained = {('self', key) for key in SELF_FIELDS}
    retained.update({('listener', 'name'), ('place', 'name'), ('day',),
                     ('available_actions',)})
    if packed['prompt']['format'] == 'crownless-person-v1':
        retained.add(('relationship',))
    else:
        retained.update(('relationship', key) for key in ('affinity', 'trust', 'obligation', 'history'))
    optional = {tuple(key) for key in packed['prompt']['included']}
    retained.update(optional)
    if ('group', 0) in optional:
        retained.update({('self', 'band'), ('self', 'faction_id')})
    turns = request.get('observed_turns', [])
    if turns:
        retained.add(('turn', len(turns) - 1))
    available = {('self', key) for key in SELF_FIELDS}
    available.update({('self', 'band'), ('self', 'faction_id'), ('listener', 'name'),
                      ('place', 'name'), ('day',), ('relationship',), ('available_actions',), ('group', 0)})
    if isinstance(person['relationship'], dict):
        available.update(('relationship', key) for key in person['relationship'])
    for kind, records in (('account', person.get('held_accounts', [])),
                          ('turn', turns),
                          ('observed_memory', request.get('remembered_observations', [])),
                          ('memories', person.get('memories', [])),
                          ('knowledge', person.get('knowledge', []))):
        available.update((kind, i) for i in range(len(records)))
    return available, retained


def approve(row, packed, review, world_group):
    """Reviews bind the complete candidate and exact compact preview."""
    if not isinstance(world_group, str) or not world_group.strip():
        raise ValueError('world_group is required')
    if review.get('source_sha256') != digest(row) or review.get('compact_sha256') != digest(packed):
        raise ValueError('review belongs to different source or compact content')
    if not all(isinstance(review.get(k), str) and review[k].strip() for k in ('reviewer', 'notes')):
        raise ValueError('reviewer and notes are required')
    if review.get('source_decision') != 'approved' or review.get('compact_decision') != 'approved':
        raise ValueError('source or compact review excludes this target')
    if row.get('review_status', '').startswith('rejected'):
        raise ValueError('source candidate was rejected')
    required = review.get('required_evidence')
    if not isinstance(required, list):
        raise ValueError('review must list required evidence')
    available, retained = retained_evidence(row, packed)
    for ref in required:
        if not isinstance(ref, list) or not 1 <= len(ref) <= 2 or any(
                type(x) not in (str, int) for x in ref):
            raise ValueError('invalid evidence reference')
        key = tuple(ref)
        if key not in available:
            raise ValueError('evidence reference is absent from the source: ' + str(ref))
        if key not in retained:
            raise ValueError('required evidence was omitted: ' + str(ref))
    return {**packed, 'source_review_status': 'approved', 'review_status': 'approved_compact',
            'world_group': world_group, 'review': review,
            'source_candidate_sha256': digest(row)}


def assemble(candidate_path, review_path, output, tokenizer, world_group):
    if not world_group.strip():
        raise ValueError('world_group is required')
    candidate_bytes, review_bytes = candidate_path.read_bytes(), review_path.read_bytes()
    entries = json.loads(review_bytes)
    if not isinstance(entries, list):
        raise ValueError('expected a review list')
    by_source = {}
    for entry in entries:
        key = entry['source_sha256']
        if key in by_source:
            raise ValueError('duplicate source review')
        by_source[key] = entry
    output.mkdir(parents=True, exist_ok=False)
    (output / 'source-candidates.jsonl').write_bytes(candidate_bytes)
    (output / 'reviews.json').write_bytes(review_bytes)
    counts = {'approved': 0, 'excluded': 0}
    used, seen = set(), set()
    with (output / 'trainable.jsonl').open('w') as accepted, \
         (output / 'excluded.jsonl').open('w') as excluded:
        for line_no, line in enumerate(candidate_bytes.decode().splitlines(), 1):
            try:
                row = json.loads(line)
                key = digest(row)
                if key in seen:
                    raise ValueError('duplicate source candidate')
                seen.add(key)
                packed = compile_row(row, tokenizer)
                if key not in by_source:
                    raise ValueError('candidate needs a review')
                used.add(key)
                value = approve(row, packed, by_source[key], world_group)
                accepted.write(json.dumps(value, ensure_ascii=False) + '\n')
                counts['approved'] += 1
            except (ValueError, TypeError, KeyError) as error:
                excluded.write(json.dumps({'source_line': line_no, 'source': line, 'error': str(error)}) + '\n')
                counts['excluded'] += 1
    receipt = {**counts, 'world_group': world_group, 'unused_reviews': sorted(set(by_source) - used),
               'source_sha256': hashlib.sha256(candidate_bytes).hexdigest(),
               'review_sha256': hashlib.sha256(review_bytes).hexdigest(),
               'native_probe_sha256': tokenizer.sha256,
               'sources': {name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                           for name in ('reviewed_corpus.py', 'participant_training.py', 'paired_participants.py')},
               'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir() if p.is_file()}}
    (output / 'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidates', type=Path, required=True)
    parser.add_argument('--reviews', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--world-group', required=True, help='Shared history group, including forks')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    receipt = assemble(args.candidates, args.reviews, args.output,
                       NativeTokenizer(args.probe), args.world_group)
    print(json.dumps(receipt))
    return 0 if receipt['approved'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
