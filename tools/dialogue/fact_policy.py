"""Typed fact selection over a participant's held facts.

A bounded fact table gives each fact an event, a role, a value, a source, a
certainty and a day. A typed question names an event and a role. The model
chooses the fact that answers the question, or defers. The renderer owns the
wording, so the choice is a typed reference rather than generated text.

This is the fact analogue of the v3 meaning choice loop in meaning.py. It is a
mechanism demonstration: natural question wording must still be parsed into the
typed role before this runs.
"""
from __future__ import annotations

import copy

FORMAT = 'crownless-fact-selection-v1'
ROLES = ('actor', 'recipient', 'place', 'object', 'group', 'material', 'quantity')
SOURCES = ('observed', 'told')
CERTAINTY = ('witnessed', 'told', 'doubtful')
MAX_FACTS = 8
MAX_CHOICES = 16


def identity(value):
    if not isinstance(value, str) or not value.isdecimal() or not 0 < int(value) < 2**64:
        raise ValueError('identity needs a nonzero uint64 string')
    return value


def integer(value, low=0, high=2**31 - 1):
    if type(value) is not int or not low <= value <= high:
        raise ValueError('integer outside its bounds')
    return value


def facts(person):
    """Return the participant's own, disclosable facts in a stable order."""
    own = identity(person['self']['id'])
    identity(person['listener']['id'])
    if own == person['listener']['id']:
        raise ValueError('distinct participants required')
    today = integer(person['day'])
    items = person.get('facts', [])
    if not isinstance(items, list) or len(items) > MAX_FACTS:
        raise ValueError('bounded facts required')
    seen = set()
    result = []
    for raw in items:
        fact = copy.deepcopy(raw)
        if fact.get('owner') != own:
            raise ValueError('foreign fact')
        fact_id = fact.get('fact_id')
        if not isinstance(fact_id, str) or not fact_id or fact_id in seen:
            raise ValueError('distinct fact ids required')
        seen.add(fact_id)
        if fact.get('role') not in ROLES:
            raise ValueError('unknown role')
        if fact.get('source') not in SOURCES:
            raise ValueError('unknown source')
        if fact.get('certainty') not in CERTAINTY:
            raise ValueError('unknown certainty')
        identity(fact.get('event_id'))
        integer(fact.get('day'), 0, today)
        if type(fact.get('private', False)) is not bool:
            raise ValueError('invalid disclosure flag')
        value = fact.get('value')
        if not isinstance(value, str) or not value or len(value) > 100:
            raise ValueError('invalid value')
        if not fact.get('private', False):
            result.append(fact)
    return sorted(result, key=lambda f: (f['event_id'], ROLES.index(f['role']), f['fact_id']))


def question(person):
    asked = person.get('question')
    if not isinstance(asked, dict) or set(asked) != {'event_id', 'role'}:
        raise ValueError('question needs event_id and role')
    identity(asked['event_id'])
    if asked['role'] not in ROLES:
        raise ValueError('unknown question role')
    return asked


def candidates(person):
    """One report choice per disclosable fact, plus a defer choice."""
    known = facts(person)
    asked = question(person)
    choices = [{'version': 1, 'kind': 'report', 'actor': person['self']['id'],
                'recipient': person['listener']['id'], 'fact_id': fact['fact_id'],
                'event_id': fact['event_id'], 'role': fact['role']} for fact in known]
    choices.append({'version': 1, 'kind': 'defer', 'actor': person['self']['id'],
                    'recipient': person['listener']['id'], 'fact_id': None,
                    'event_id': asked['event_id'], 'role': None})
    if len(choices) > MAX_CHOICES:
        raise ValueError('candidate bound exceeded')
    return choices


def preferred(person, choices=None):
    """The authored teacher: the asked fact, most certain then most recent."""
    choices = candidates(person) if choices is None else choices
    asked = question(person)
    matches = [fact for fact in facts(person)
               if fact['event_id'] == asked['event_id'] and fact['role'] == asked['role']]
    if not matches:
        return next(index for index, act in enumerate(choices) if act['kind'] == 'defer')
    rank = {'witnessed': 0, 'told': 1, 'doubtful': 2}
    best = min(matches, key=lambda f: (rank[f['certainty']], -f['day'], f['fact_id']))
    return next(index for index, act in enumerate(choices) if act['fact_id'] == best['fact_id'])


def validate(act, person):
    if act not in candidates(person):
        raise ValueError('act differs from current facts and choices')
    return act


def encode_input(person, choices=None):
    """Encode the typed question, the fact table, and the legal candidates."""
    choices = candidates(person) if choices is None else choices
    if choices != candidates(person):
        raise ValueError('candidate list changed')
    known = facts(person)
    asked = question(person)
    # The topic event is a local slot, not a raw id. The renderer owns the
    # event's text, so the model only has to match slot and role.
    events = sorted({asked['event_id']} | {fact['event_id'] for fact in known})
    ids = [1280]

    def number(value):
        integer(value, 0, 2**31 - 1)
        while value >= 128:
            ids.append(2048 + value % 128)
            value //= 128
        ids.extend((2048 + value, 2304))

    # The typed question: role first, then the event slot it belongs to.
    ids.append(1600 + ROLES.index(asked['role']))
    number(events.index(asked['event_id']))
    # The bounded fact table.
    ids.append(1620 + len(known))
    for index, fact in enumerate(known):
        # The role token is shared with the question so the model can compare
        # the asked role against each fact role directly.
        ids.extend((1630 + index, 1600 + ROLES.index(fact['role'])))
        number(events.index(fact['event_id']))
        ids.extend((1700 + SOURCES.index(fact['source']),
                    1720 + CERTAINTY.index(fact['certainty'])))
        number(fact['day'])
    # Legal candidate choices, bounded by the native context.
    ids.append(1580)
    ids += [1024 + index for index in range(len(choices))]
    ids.append(1281)
    if len(ids) > 352:
        raise ValueError('native context budget exceeded')
    return ids


def render(act, person):
    """Render the selected fact as a short attributed claim."""
    act = validate(act, person)
    if act['kind'] == 'defer':
        return 'I do not hold that account.'
    fact = next(f for f in facts(person) if f['fact_id'] == act['fact_id'])
    # Acquisition controls the claim of witnessing, even for legacy tables
    # carrying an inconsistent certainty label. Reliability is a separate cue.
    prefix = 'I saw it myself' if fact['source'] == 'observed' else 'I was told'
    if fact['certainty'] == 'doubtful':
        prefix += ', but I am uncertain'
    return f'{prefix}: {fact["value"]} (on day {fact["day"]}).'
