"""Connect free question wording and grammar fields to typed fact selection.

`fact_policy` chooses a typed fact from a typed question. This module supplies
the two missing pieces: a bounded question parser that maps natural wording to
a role, and an adapter that turns parsed account fields into the fact table.

The parser is a small ordered keyword map. It is deliberately conservative: an
unrecognised question returns None rather than guessing a role.
"""
from __future__ import annotations

import copy

# Grammar field role numbers (src/story/cc_core_model.h) to policy roles.
ROLE_BY_NUMBER = {1: 'actor', 2: 'recipient', 3: 'place', 4: 'object',
                  5: 'group', 6: 'material', 8: 'quantity'}

_MATERIAL = ('made from', 'made of', 'made with', 'what material', 'what did they use',
             'what did it use', 'what was used', 'made out of')
_QUANTITY = ('how many', 'how much')
_PLACE = ('where', 'which town', 'which place', 'which village', 'which settlement',
          'which city', 'which road', 'in which')
_GROUP = ('which group', 'which band', 'which order', 'which court', 'which house',
          'which faction', 'which clan', 'which company')
_RECIPIENT = ('who else', 'with whom', 'who received', 'who was with', 'whom did they join',
              'who did they join', 'declare war on', 'chosen as', 'as champion',
              'which horses', 'which two')
_ACTOR = ('which horses', 'which two')


def role_from_question(text):
    """Map a natural question to a typed role, or None when unsure."""
    if not isinstance(text, str):
        raise ValueError('question must be text')
    lowered = text.lower().strip().rstrip('?')
    for keys, role in ((_MATERIAL, 'material'), (_QUANTITY, 'quantity'), (_PLACE, 'place'),
                       (_GROUP, 'group'), (_RECIPIENT, 'recipient'), (_ACTOR, 'actor')):
        if any(key in lowered for key in keys):
            return role
    if lowered.startswith('whom') or ' whom ' in lowered:
        return 'recipient'
    if lowered.startswith('who') or ' who ' in lowered:
        return 'actor'
    if lowered.startswith('what') or ' what ' in lowered:
        return 'object'
    return None


def _certainty(confidence, source):
    # Legacy categorical tokens remain compatible; acquisition is supplied
    # separately and can never be inferred from confidence.
    if confidence is not None and (type(confidence) is not int or not 0 <= confidence <= 100):
        raise ValueError('confidence must be an integer from 0 to 100 or None')
    if confidence is not None and confidence < 40:
        return 'doubtful'
    return 'witnessed' if source == 'observed' else 'told'


def facts_from_fields(fields, owner, event_id, confidence=None, day=0, *, source='told'):
    """Turn disclosable fields into facts without upgrading hearsay to sight.

    `source` describes acquisition, not reliability. Old accounts without
    acquisition metadata conservatively remain received accounts. The numeric
    confidence is retained as metadata, without changing the model token format.
    """
    if source not in ('observed', 'told'):
        raise ValueError('source must be observed or told')
    certainty = _certainty(confidence, source)
    facts = []
    for field in fields:
        role = ROLE_BY_NUMBER.get(field.get('role'))
        if role is None or field.get('knowledge') == 3:
            continue
        value = field.get('text')
        if not isinstance(value, str) or not value:
            continue
        facts.append({'owner': owner, 'fact_id': f'F{event_id}-{field.get("field")}', 'event_id': event_id,
                      'role': role, 'value': value, 'source': source,
                      'certainty': certainty, 'confidence': confidence, 'day': day, 'private': False})
    return facts


def person(owner, listener, facts, question, day=0):
    """Assemble a fact_policy person from typed facts and a parsed question."""
    if question not in ROLE_BY_NUMBER.values():
        raise ValueError('question needs a known role')
    return {'self': {'id': owner, 'name': 'speaker'},
            'listener': {'id': listener, 'name': 'listener'},
            'day': day, 'facts': copy.deepcopy(facts),
            'question': {'event_id': facts[0]['event_id'] if facts else '1', 'role': question}}
