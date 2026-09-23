"""Bind a question predicate to the held account and field that answer it.

`event_facts` gives account-level facts. This module parses each account's
grammar `source` template into typed fields, builds the `fact_policy` table,
and routes a question predicate to the right account and field.

The parser is validated against the native grammar parse: on all 200 rows of
ZERO's `grammar-test.jsonl` it recovers the native field values exactly. It does
not replace the native parser; it is the Python bridge that lets typed fact
selection run on held accounts before the native act protocol carries a field.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

from event_facts import build_facts
from fact_question import ROLE_BY_NUMBER, parse_question
from fact_roles import resolve_field

ROOT = Path(__file__).resolve().parents[2]
RULES_PATH = ROOT / 'tools/data/core_account_rules.json'


def load_rules(path=RULES_PATH):
    """Grammar rules grouped by event kind name."""
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    by_kind = {}
    for rule in data['rules']:
        by_kind.setdefault(rule['kind'], []).append(rule)
    return by_kind


def _template_re(template):
    parts = []
    for piece in re.split(r'(\{\d+\})', template):
        slot = re.fullmatch(r'\{(\d+)\}', piece)
        parts.append(f'(?P<f{slot.group(1)}>.+?)' if slot else re.escape(piece))
    return re.compile('^' + ''.join(parts) + '$')


def topic_line(text):
    """The topic is the last non-empty line, without the `- ? ~` cues."""
    lines = [line for line in text.splitlines() if line.strip()]
    return re.sub(r'^[\s\-–?~]+', '', (lines[-1] if lines else '').strip())


def parse_account(text, rule):
    """Field values for an account text, or None when the rule does not match."""
    match = _template_re(rule['source']).match(topic_line(text))
    if match is None:
        return None
    values = {int(name[1:]): value for name, value in match.groupdict().items()}
    fields = []
    for index, role_name in enumerate(rule['roles']):
        if role_name == 'none' or index not in values:
            continue
        fields.append({'field': index, 'role': _role_number(role_name),
                       'text': values[index], 'spoken': role_name not in ('quantity', 'detail'),
                       'knowledge': 0})
    return fields


_ROLE_NUMBER = {name: number for number, name in ROLE_BY_NUMBER.items()}
_ROLE_NUMBER.update({'none': 0, 'detail': 7})


def _role_number(role_name):
    if role_name not in _ROLE_NUMBER:
        raise ValueError(f'unknown role {role_name}')
    return _ROLE_NUMBER[role_name]


def typed_facts(participant, rules):
    """Disclosable facts with parsed fields and their account reference."""
    result = []
    for fact in build_facts(participant):
        if fact.private:
            continue
        for rule in rules.get(fact.kind_name, []):
            fields = parse_account(fact.text, rule)
            if fields is None:
                continue
            result.append({'account_ref': fact.account_ref, 'event_id': fact.event_id,
                           'owner_id': fact.owner_id,
                           'kind_name': fact.kind_name, 'source_id': fact.source_id,
                           'certainty': fact.certainty, 'confidence': fact.confidence,
                           'day': fact.day, 'text': fact.text, 'rule': rule['id'],
                           'fields': fields})
            break
    return result


def select(participant, question, rules):
    """Route a question to (account, field), or None when nothing matches."""
    predicate = parse_question(question)
    if predicate is None:
        return None
    for account in typed_facts(participant, rules):
        index = resolve_field(account['rule'], predicate, account['fields'])
        if index is None:
            continue
        field = next(f for f in account['fields'] if f['field'] == index)
        role = ROLE_BY_NUMBER.get(field['role'])
        if role is None:
            continue
        return {'predicate': predicate, 'account_ref': account['account_ref'],
                'event_id': account['event_id'], 'owner_id': account['owner_id'],
                'kind_name': account['kind_name'],
                'source_id': account['source_id'], 'certainty': account['certainty'],
                'confidence': account['confidence'], 'day': account['day'],
                'field': index, 'role': role, 'value': field['text'],
                'text': account['text']}
    return None


def render(selected):
    """Render a routed fact as a short attributed claim."""
    if selected is None:
        return 'I do not hold that account.'
    source = ('my own account' if selected['source_id'] == selected.get('owner_id')
              else 'another person' if selected['source_id'] != 'unknown' else 'an unknown source')
    certainty = {'witnessed': 'I saw it myself', 'told': 'I was told',
                 'doubtful': 'I have a doubtful account'}.get(selected['certainty'], 'I have heard')
    return f"{certainty}: {selected['value']} ({selected['role']}, from {source}, on day {selected['day']})."


def claim(participant, question, rules=None):
    """A policy-schema claim for the account that answers the question.

    Returns None when no held account matches, so a caller keeps its existing
    fallback. The claim names the routed account; the caller renders it.
    """
    rules = rules or load_rules()
    selected = select(participant, question, rules)
    if selected is None:
        return None
    return {'owner': participant['self']['id'], 'ref': selected['account_ref'],
            'event_id': selected['event_id'], 'source_id': selected['source_id'],
            'day': selected['day'], 'certainty': selected['certainty'],
            'confidence': selected['confidence'], 'text': selected['text']}