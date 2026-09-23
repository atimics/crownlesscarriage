"""Reconcile question predicates with the grammar's field roles.

The account grammar labels fields with a small role vocabulary (`actor`,
`recipient`, `place`, `object`, `group`, `material`, `detail`, `quantity`).
That vocabulary is used for native parse and render, so it is deliberately
generic. Some rules label two spoken fields with the same role, and some use
`place` for a settlement that a question calls a beneficiary. Role-only routing
cannot separate those, so this module resolves a question predicate to a
concrete field index and reports where the grammar needs a predicate.

It does not change the native grammar. It is the Python-side reconciliation that
lets typed fact selection pick the right field before the grammar is revised.
"""
from __future__ import annotations

from collections import Counter

from fact_question import BASE_ROLE, ROLE_BY_NUMBER

# Roles that carry a spoken, copyable value.
SPOKEN_ROLES = ('actor', 'recipient', 'place', 'object', 'group', 'material')

# Predicate-to-field overrides where the grammar role alone is ambiguous.
# Each maps a question predicate to the field index that answers it.
PREDICATE_FIELD_OVERRIDES = {
    # Two spoken `place` fields: the first is where it happened, the second is
    # the settlement that needed the supplies.
    ('harvest_failed_0', 'beneficiary'): 1,
    # The joined band is stored as `recipient`, not `group`.
    ('bandit_pressure_1', 'joined'): 1,
    # Two horses are `actor` and `recipient`; the question names the pair.
    ('horse_bred_0', 'actor'): 1,
}


def resolve_field(rule, predicate, fields):
    """Field index a question predicate asks about, or None."""
    override = PREDICATE_FIELD_OVERRIDES.get((rule, predicate))
    if override is not None:
        return override
    role = BASE_ROLE.get(predicate)
    if role is None:
        return None
    candidates = [field['field'] for field in fields
                  if field.get('spoken') and field.get('knowledge') != 3
                  and ROLE_BY_NUMBER.get(field['role']) == role]
    return candidates[0] if candidates else None


def duplicate_spoken_roles(rule):
    """Spoken roles a rule uses more than once."""
    roles = [name for name in rule['roles'] if name in SPOKEN_ROLES]
    return {name: count for name, count in Counter(roles).items() if count > 1}


def audit(rules):
    """Report every rule with a duplicate spoken role."""
    flags = []
    for rule in rules:
        duplicates = duplicate_spoken_roles(rule)
        if duplicates:
            flags.append({'rule': rule['id'], 'roles': list(rule['roles']),
                          'duplicate_spoken': duplicates})
    return flags