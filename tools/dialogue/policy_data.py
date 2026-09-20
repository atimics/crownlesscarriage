"""Procedural participant views and branching exchanges for all policy goals."""
import copy
import hashlib
import random
from collections import Counter
from policy import (GOALS, ACTIONS, allowed, choose, make_act, encode_input,
                    encode_choice, goal, wire, view, FEATURES)
from event_facts import EVENT_KIND_REGISTRY


def fixture(seed, identity='1', listener='2'):
    rng = random.Random(seed)
    facts = []
    if rng.randrange(2):
        loss = bool(rng.randrange(2))
        facts = [{'event_id': '51', 'source_id': identity, 'day': 1,
                  'kind': EVENT_KIND_REGISTRY['DRAGON_RETALIATION' if loss else 'GOBLIN_TRADE'],
                  'confidence': 80, 'private': rng.randrange(4) == 0,
                  'account': 'Embermaw burns Thornford because 7 stolen crowns remain missing.' if loss else
                  'Nara Soot-Tongue buys 4 Tools from the Crownless company for 12 crowns; the lair relies less on the dragon.'}]
    memory = rng.randrange(5)
    return {'self': {'id': identity, 'name': 'Speaker ' + identity, 'goal': 'secure_livelihood',
                    'coins': rng.choice((0, 1, 3, 7)), 'hungry_days': rng.choice((0, 2)),
                    'stress': rng.choice((28, 70)), 'courage': rng.choice((30, 80)),
                    'unsheltered_nights': rng.choice((0, 1)), 'faction_id': rng.choice(('0', '7'))},
            'listener': {'id': listener, 'name': 'Speaker ' + listener}, 'day': 2,
            'relationship': {'trust': rng.choice((-2, 0, 2)), 'affinity': rng.choice((0, 2)),
                             'obligation': rng.choice((-2, 0, 2))},
            'held_accounts': facts, 'knowledge': [],
            'memories': [{'kind': memory, 'day': 1, 'subject_id': '3', 'event_id': '50'}] if memory else [],
            'available_actions': ['end_conversation']}


def row_for(person, heard, requested):
    action = choose(person, heard, requested)
    x = encode_input(person, heard, requested); y = encode_choice(action)
    return {'input': {'participant': copy.deepcopy(person), 'observed_acts': copy.deepcopy(heard),
                      'requested_goal': requested},
            'prompt': {'format': 'crownless-policy-v2', 'tokens': x},
            'target_text': wire({'choice': action}), 'tokens': x+y,
            'labels': [-100]*(len(x)-1)+y+[0], 'source': 'procedural-policy-v2'}


def dataset(worlds=384):
    rng = random.Random(23); by_input = {}
    for seed in range(worlds):
        people = [fixture(2*seed), fixture(2*seed+1, '2', '1')]
        for requested in GOALS:
            # Two on-policy and two exploratory paths expose different public replies.
            for branch in range(4):
                heard = []
                for turn in range(8):
                    person = people[turn % 2]; request = requested if not heard else None
                    row = row_for(person, heard, request)
                    key = tuple(row['prompt']['tokens'])
                    if key in by_input and by_input[key]['target_text'] != row['target_text']:
                        raise ValueError('same model input has conflicting policy targets')
                    by_input[key] = row
                    action = choose(person, heard, request)
                    if branch >= 2 and turn < 3:
                        action = rng.choice(sorted(allowed(person, heard, request)))
                    act = make_act(action, person, heard, request)
                    heard.append({'speaker_id': person['self']['id'], 'act': act})
                    if action == 'end': break
    splits = {'train': [], 'development': [], 'test': []}
    for row in by_input.values():
        person = row['input']['participant']; v = view(person)
        # All histories for the same observable own-state profile share a split.
        profile = ''.join(str(int(v[key])) for key in FEATURES)
        bucket = int(hashlib.sha256(profile.encode()).hexdigest()[:8], 16) % 10
        split = 'test' if bucket == 0 else 'development' if bucket == 1 else 'train'
        row['state_group'] = profile
        splits[split].append(row)
    return splits


if __name__ == '__main__':
    data = dataset()
    print({k: len(v) for k, v in data.items()})
    print(Counter(__import__('json').loads(r['target_text'])['choice'] for rows in data.values() for r in rows))
