"""Connect free question wording and grammar fields to typed fact selection.

`fact_policy` chooses a typed fact from a typed question. This module supplies
the two missing pieces: a bounded question parser that maps natural wording to a
question predicate, and an adapter that turns parsed account fields into the
fact table and resolves the predicate to a concrete field.

The parser is a small ordered keyword map. It returns None rather than guessing.
Where a grammar rule labels two spoken fields with the same role, `fact_roles`
supplies a predicate-to-field override.
"""
from __future__ import annotations

import copy

# Grammar field role numbers (src/story/cc_core_model.h) to policy roles.
ROLE_BY_NUMBER = {1: 'actor', 2: 'recipient', 3: 'place', 4: 'object',
                  5: 'group', 6: 'material', 8: 'quantity'}

# A question predicate maps to a base fact_policy role for the selection model.
BASE_ROLE = {
    'actor': 'actor', 'recipient': 'recipient', 'beneficiary': 'recipient',
    'joined': 'recipient', 'place': 'place', 'destination': 'place',
    'object': 'object', 'group': 'group', 'material': 'material',
    'quantity': 'quantity', 'time': 'place',
}

# Ordered: earlier predicates win. Each entry is (substrings, predicate).
MATERIAL = (('made from', 'material'), ('made of', 'material'), ('made with', 'material'),
            ('made out of', 'material'), ('what material', 'material'),
            ('what did they use', 'material'), ('what did it use', 'material'),
            ('what was used', 'material'), ('what did the mill use', 'material'))
QUANTITY = (('how many', 'quantity'), ('how much', 'quantity'))
BENEFICIARY = (('who needed', 'beneficiary'), ('who was waiting', 'beneficiary'),
               ('who received', 'beneficiary'), ('who got the', 'beneficiary'),
               ('who did the supplies', 'beneficiary'))
JOINED = (('which band did they join', 'joined'), ('which band were they in', 'joined'),
          ('which group did they join', 'joined'), ('who did they join', 'joined'),
          ('whom did they join', 'joined'), ('who joined them', 'joined'))
RECIPIENT = (('who did they declare war on', 'recipient'), ('who declared war on', 'recipient'),
             ('chosen as', 'recipient'), ('as champion', 'recipient'),
             ('who was the abbot before', 'recipient'), ('who ruled before', 'recipient'),
             ('who replaced', 'recipient'), ('whom did', 'recipient'),
             ('who else', 'recipient'), ('with whom', 'recipient'),
             ('whose court were they gathering for', 'recipient'))
GROUP = (('whose blessing', 'group'), ('whose court gained', 'group'),
         ('which band', 'group'), ('which order', 'group'), ('which court', 'group'), ('which house', 'group'),
         ('which faction', 'group'), ('which clan', 'group'), ('which company', 'group'),
         ('which guild', 'group'), ('which sect', 'group'), ('which cult', 'group'))
PLACE = (('where', 'place'), ('which town', 'place'), ('which place', 'place'),
         ('which village', 'place'), ('which settlement', 'place'), ('which city', 'place'),
         ('which road', 'place'), ('which stable', 'place'), ('which mill', 'place'),
         ('which quarry', 'place'), ('in which', 'place'), ('at which', 'place'),
         ('where was', 'place'), ('where is', 'place'))
ACTOR = (('which horses', 'actor'), ('which two', 'actor'), ('which courts', 'actor'),
         ('who slew', 'actor'), ('who carried out', 'actor'), ('who sent', 'actor'),
         ('who sealed', 'actor'), ('who made', 'actor'), ('who joined', 'actor'),
         ('who gave', 'actor'), ('who keeps', 'actor'), ('who took', 'actor'),
         ('who do the readers', 'actor'), ('who funded', 'actor'), ('who posted', 'actor'))
OBJECT = (('what happened to', 'object'), ('what brought', 'object'),
          ('what rank', 'object'), ('what was the money for', 'object'),
          ('what did they finish', 'object'), ('what fell', 'object'))

ORDER = (MATERIAL, QUANTITY, BENEFICIARY, JOINED, RECIPIENT, GROUP, PLACE, ACTOR, OBJECT)
NEGATION = ('did not', "didn't", 'was not', "wasn't", 'never', 'no longer', 'not ')


def parse_question(text):
    """Return a question predicate, or None when the wording is not recognised."""
    if not isinstance(text, str):
        raise ValueError('question must be text')
    lowered = text.lower().strip().rstrip('?')
    for group in ORDER:
        for key, predicate in group:
            if key in lowered:
                return predicate
    if lowered.startswith('whom') or ' whom ' in lowered:
        return 'recipient'
    if lowered.startswith('whose') or ' whose ' in lowered:
        return 'group'
    if lowered.startswith('who') or ' who ' in lowered:
        return 'actor'
    if lowered.startswith('what') or ' what ' in lowered:
        return 'object'
    return None


def role_from_question(text):
    """Base fact_policy role for a question, or None."""
    predicate = parse_question(text)
    return BASE_ROLE.get(predicate) if predicate else None


def is_negated(text):
    lowered = text.lower()
    return any(key in lowered for key in NEGATION)


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
        if role is None or field.get('knowledge') == 3 or field.get('spoken') is False:
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
    if question not in set(ROLE_BY_NUMBER.values()):
        raise ValueError('question needs a known role')
    return {'self': {'id': owner, 'name': 'speaker'},
            'listener': {'id': listener, 'name': 'listener'},
            'day': day, 'facts': copy.deepcopy(facts),
            'question': {'event_id': facts[0]['event_id'] if facts else '1', 'role': question}}