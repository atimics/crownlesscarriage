"""Procedural, split-safe fact-selection examples for the participant teacher.

Each person holds one to three events, and each event carries two to four typed
facts. The typed question names an event and a role. The teacher is the fact
matching both, most certain then most recent, or a defer when the asked role is
not held. Worlds are split by profile so held-out events and roles share no
exact input with training.
"""
from __future__ import annotations

import copy
import hashlib
import json
import random

import fact_policy
from fact_policy import (CERTAINTY, FORMAT, ROLES, candidates, encode_input,
                         preferred)

VALUES = {
    'actor': 'Person', 'recipient': 'Other', 'place': 'Place', 'object': 'Object',
    'group': 'Group', 'material': 'Material', 'quantity': 'Quantity',
}


def _id(value):
    return str(100000 + value)


def _value(role, seed, event):
    return f'{VALUES[role]} {seed}-{event}'


def person(seed, side=0):
    rng = random.Random(seed * 53 + side)
    own = _id(seed * 2 + side)
    other = _id(seed * 2 + (1 - side))
    today = 10 + seed % 7
    facts = []
    budget = 7
    for event in range(rng.randint(1, 3)):
        if budget < 2:
            break
        event_id = _id(5000 + seed * 10 + event)
        roles = rng.sample(ROLES, min(budget, rng.randint(2, 4)))
        budget -= len(roles)
        for role in roles:
            certainty = rng.choice(CERTAINTY)
            source = 'observed' if certainty == 'witnessed' else 'told'
            facts.append({
                'owner': own, 'fact_id': f'F{event}-{role}', 'event_id': event_id,
                'role': role, 'value': _value(role, seed, event), 'source': source,
                'certainty': certainty, 'day': rng.randint(0, today),
                'private': role == 'quantity' and seed % 4 == 0,
            })
    return {
        'self': {'id': own, 'name': ('Mara ' if side == 0 else 'Kesh-') + str(seed)},
        'listener': {'id': other, 'name': ('Kesh-' if side == 0 else 'Mara ') + str(seed)},
        'day': today,
        'facts': facts,
    }


def questions(p):
    """Every held (event, role) plus a few absent roles that must defer."""
    rng = random.Random(int(p['self']['id']) * 7)
    held = {}
    for fact in p['facts']:
        if not fact.get('private', False):
            held.setdefault(fact['event_id'], set()).add(fact['role'])
    asked = []
    for event_id in sorted(held):
        for role in held[event_id]:
            asked.append({'event_id': event_id, 'role': role})
    events = sorted(held)
    for role in ROLES:
        if events and role not in held[events[0]]:
            asked.append({'event_id': events[0], 'role': role})
    rng.shuffle(asked)
    return asked


def _row(p, asked):
    person = copy.deepcopy(p)
    person['question'] = copy.deepcopy(asked)
    choices = candidates(person)
    index = preferred(person, choices)
    prefix = encode_input(person, choices)
    target = [1024 + index]
    fact = choices[index]
    held_roles = sorted({f['role'] for f in p['facts']
                         if f['event_id'] == asked['event_id'] and not f.get('private', False)},
                        key=ROLES.index)
    return {
        'input': {'person': copy.deepcopy(person)},
        'prompt': {'format': FORMAT, 'tokens': prefix},
        'target_text': json.dumps({'choiceindex': index}, separators=(',', ':')),
        'target': target, 'tokens': prefix + target,
        'labels': [-100] * (len(prefix) - 1) + target + [0],
        'choice_count': len(choices), 'teacher_index': index,
        'teacher_kind': fact['kind'], 'teacher_role': fact['role'],
        'defer': fact['kind'] == 'defer',
        'different': bool(held_roles) and asked['role'] != held_roles[0],
    }


def _profile(p):
    facts = sorted(({key: fact[key] for key in ('event_id', 'role', 'source', 'certainty', 'day')}
                    for fact in p['facts'] if not fact.get('private', False)),
                   key=lambda item: (item['event_id'], item['role']))
    return {'day': p['day'], 'facts': facts}


def dataset(worlds=256):
    if worlds < 12:
        raise ValueError('at least 12 worlds are needed for three splits')
    split_rows = {'train': [], 'development': [], 'test': []}
    seen = set()
    for seed in range(worlds):
        people = [person(seed, 0), person(seed, 1)]
        profile = hashlib.sha256(json.dumps([_profile(p) for p in people], sort_keys=True).encode()).hexdigest()[:16]
        bucket = int(profile[:8], 16) % 10
        split = 'test' if bucket == 0 else 'development' if bucket in (1, 2) else 'train'
        for p in people:
            for asked in questions(p):
                row = _row(p, asked)
                key = tuple(row['prompt']['tokens'])
                if key in seen:
                    continue
                seen.add(key)
                row['state_group'] = profile
                row['world_group'] = f'world-{seed}'
                split_rows[split].append(row)
    for name, rows in split_rows.items():
        if not rows:
            raise ValueError(f'procedural worlds did not populate {name}')
    groups = {name: {row['state_group'] for row in rows} for name, rows in split_rows.items()}
    if (groups['train'] & groups['development']) or (groups['train'] & groups['test']) or (groups['development'] & groups['test']):
        raise ValueError('profile appears in more than one split')
    tokens = {name: {tuple(row['prompt']['tokens']) for row in rows} for name, rows in split_rows.items()}
    if any(tokens[a] & tokens[b] for a, b in (('train', 'development'), ('train', 'test'), ('development', 'test'))):
        raise ValueError('exact input appears in more than one split')
    return split_rows


if __name__ == '__main__':
    print({key: len(value) for key, value in dataset().items()})