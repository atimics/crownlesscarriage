"""Procedural, split-safe v3 meaning examples for the participant teacher."""
from __future__ import annotations

import copy
import hashlib
import json
import random
from pathlib import Path

import meaning  # noqa: E402
from meaning import FORMAT, candidates, encode_input, preferred  # noqa: E402


def _id(value: int) -> str:
    return str(100000 + value)


def person(seed: int, side: int = 0) -> dict:
    rng = random.Random(seed * 37 + side)
    own, other = _id(seed * 2 + side), _id(seed * 2 + (1 - side))
    place_id = _id(900 + seed)
    facts = []
    count = rng.randint(1, 3)
    for index in range(count):
        stale = index == 1 and seed % 2 == 0
        private = index == 2 and seed % 3 == 0
        source = 'told' if index == 2 else 'observed'
        facts.append({
            'kind': 'food_store', 'owner': own, 'place_id': _id(900 + seed * 4 + index),
            'place_name': ('Vault ' + str(20 + seed) if index == 0 else 'Ash Hollow ' + str(index)),
            'stock': rng.randint(0, 160),
            'target': rng.randint(1, 200),
            'unit_price': rng.randint(1, 12),
            'day': max(0, 10 + seed % 5 - (2 if stale else 0)),
            'source': source, 'private': private,
        })
    # The first observation is current and local, so exchanges have concrete offers.
    facts[0]['place_id'] = place_id
    return {
        'self': {'id': own, 'name': ('Mara ' if side == 0 else 'Kesh-') + str(seed),
                 'coins': rng.randint(0, 80),
                 'hungry_days': rng.choice((0, 0, 0, 1, 2, 5)),
                 'stress': rng.randint(0, 100)},
        'listener': {'id': other, 'name': ('Kesh-' if side == 0 else 'Mara ') + str(seed)},
        'place': {'id': place_id, 'name': facts[0]['place_name']}, 'day': 10 + seed % 5,
        'relationship': {'trust': rng.randint(-100, 100)},
        'facts': facts,
        'conditions': ['daylight'] if rng.randrange(2) else [],
        'outcomes': ([{'outcome': 'fulfilled' if (seed // 4) % 2 else 'failed', 'quantity': 1 + seed % 3,
                       'total_cost': (1 + seed % 3) * facts[0]['unit_price'],
                       'event_id': _id(7000 + seed), 'actor_id': own,
                       'beneficiary_id': other, 'reason': 'outcome'}] if seed % 4 == 0 else []),
    }


def _row(p: dict, heard: list[dict], choices: list[dict]) -> dict:
    index = preferred(p, heard, choices)
    prefix = encode_input(p, heard, choices)
    target = [1024 + index]
    return {
        'input': {'person': copy.deepcopy(p), 'heard': copy.deepcopy(heard)},
        'prompt': {'format': FORMAT, 'tokens': prefix},
        'target_text': json.dumps({'choiceindex': index}, separators=(',', ':')),
        'target': target, 'tokens': prefix + target,
        'labels': [-100] * (len(prefix) - 1) + target + [0],
        'choice_count': len(choices), 'teacher_index': index,
        'teacher_intent': choices[index]['intent'],
    }


def _profile(p: dict) -> dict:
    """Canonical observable state with identities and display names removed."""
    facts = []
    for fact in p.get('facts', []):
        facts.append({key: fact[key] for key in ('kind', 'stock', 'target',
                                                  'unit_price', 'source', 'private')}
                     | {'local': fact['place_id'] == p['place']['id'],
                        'age': p['day'] - fact['day']})
    outcomes = []
    for outcome in p.get('outcomes', []):
        outcomes.append({key: outcome[key] for key in ('outcome', 'quantity', 'total_cost', 'reason')})
    return {'self': {key: p['self'][key] for key in ('coins', 'hungry_days', 'stress')},
            'day': p['day'],
            'relationship': p.get('relationship', {}).get('trust', 0),
            'facts': sorted(facts, key=lambda item: (item['local'], item['age'], item['stock'])),
            'outcomes': outcomes}


def dataset(worlds: int = 256) -> dict[str, list[dict]]:
    """Return train/development/test rows with whole profiles in one split."""
    if worlds < 12:
        raise ValueError('at least 12 worlds are needed for three splits')
    split_rows = {'train': [], 'development': [], 'test': []}
    seen = set()
    for seed in range(worlds):
        people = [person(seed, 0), person(seed, 1)]
        local = copy.deepcopy(people[0]['facts'][0]);local['owner']=people[1]['self']['id']
        people[1]['facts'][0]=local
        if people[0]['outcomes']:
            people[1]['outcomes']=copy.deepcopy(people[0]['outcomes'])
        world_profile = hashlib.sha256(json.dumps([_profile(p) for p in people], sort_keys=True).encode()).hexdigest()[:16]
        bucket = int(world_profile[:8], 16) % 10
        split = 'test' if bucket == 0 else 'development' if bucket in (1, 2) else 'train'
        for branch in range(5):
            heard: list[dict] = []
            for turn in range(10):
                current = people[turn % 2]
                choices = candidates(current, heard)
                row = _row(current, heard, choices)
                key = tuple(row['prompt']['tokens'])
                if key not in seen:
                    seen.add(key)
                    # A profile includes exact resource numbers and relationship state.
                    row['state_group'] = world_profile
                    row['world_group'] = f'world-{seed}'
                    row['branch'] = branch
                    split_rows[split].append(row)
                if not heard:
                    action = choices[(branch + seed) % len(choices)]
                else:
                    # Counterfactual branches exercise changed severity and offer prices.
                    action = choices[(branch * 3 + turn + seed) % len(choices)]
                heard.append({'speaker_id': current['self']['id'], 'act': copy.deepcopy(action)})
                if action['intent'] == 'end':
                    break
    if any(not split_rows[name] for name in split_rows):
        raise ValueError('procedural worlds did not populate every split')
    groups = {name: {row['state_group'] for row in rows} for name, rows in split_rows.items()}
    if groups['train'] & groups['development'] or groups['train'] & groups['test'] or groups['development'] & groups['test']:
        raise ValueError('profile appears in more than one split')
    token_sets = {name: {tuple(row['prompt']['tokens']) for row in rows} for name, rows in split_rows.items()}
    if any(token_sets[a] & token_sets[b] for a, b in (('train', 'development'), ('train', 'test'), ('development', 'test'))):
        raise ValueError('exact input appears in more than one split')
    return split_rows


if __name__ == '__main__':
    print({key: len(value) for key, value in dataset().items()})
