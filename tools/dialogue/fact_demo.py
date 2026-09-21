"""Show typed fact selection on real grammar accounts.

Each account's parsed fields become typed facts. A natural question is parsed
to a role, the trained selection model picks a fact (or defers), and the
renderer owns the wording. Pass --model to use the trained checkpoint; without
it the authored teacher is used.

    python tools/dialogue/fact_demo.py --zero /path/to/zero \
        --model /tmp/fact-policy/last.ccv2 \
        --tokenizer assets/language/tokenizer.json
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
import fact_policy as f
from fact_question import (ROLE_BY_NUMBER, facts_from_fields, parse_question,
                           person, role_from_question)
from fact_roles import PREDICATE_FIELD_OVERRIDES, resolve_field

FIXTURE = ROOT / 'tools/dialogue/fixtures/fact-dialogue.json'


def build(account, question):
    """Return a fact_policy person for one account and natural question."""
    predicate = parse_question(question)
    if predicate is None:
        return None, 'question did not parse'
    event_id = str(900000 + int(account['id'].split(':')[-1]) + 1)
    target = resolve_field(account['rule'], predicate, account['fields'])
    if target is None:
        return None, f'account has no {role_from_question(question)} field'
    # Ask in the resolved field's own role so the selection model can match it.
    role = ROLE_BY_NUMBER.get(next(field['role'] for field in account['fields'] if field['field'] == target))
    facts = facts_from_fields(account['fields'], '1', event_id, account.get('confidence'), day=10,
                              source=account.get('source', 'told'))
    if not facts:
        return None, 'account has no typed facts'
    # When a predicate override resolves a duplicated spoken role, drop the
    # other facts of that role so the model is not asked to choose blindly.
    if (account['rule'], predicate) in PREDICATE_FIELD_OVERRIDES:
        keep = f'F{event_id}-{target}'
        facts = [fact for fact in facts if fact['role'] != role or fact['fact_id'] == keep]
    return person('1', '2', facts, role, day=10), predicate


def choose_teacher(p):
    return f.candidates(p)[f.preferred(p)]


def choose_model(model, torch, p):
    ids = f.encode_input(p)
    start = ids.index(1580) + 1
    stop = ids.index(1281, start)
    allowed = ids[start:stop]
    tokens = torch.tensor([ids])
    hidden, _ = model.hidden(tokens, torch.zeros((*tokens.shape, 16), dtype=torch.long))
    weights = model.embedding.weight[allowed]
    token = int(allowed[(weights @ hidden[0, -1]).argmax()])
    return f.validate(f.candidates(p)[token - 1024], p)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zero', type=Path)
    parser.add_argument('--model', type=Path)
    parser.add_argument('--tokenizer', type=Path)
    parser.add_argument('--fixture', type=Path, default=FIXTURE)
    args = parser.parse_args()
    model = torch = None
    if args.model:
        if not (args.zero and args.tokenizer):
            parser.error('--model needs --zero and --tokenizer')
        sys.path.insert(0, str(args.zero / 'scripts'))
        import torch as _torch
        from crownless_v2_export import load_export
        torch = _torch
        model, _ = load_export(args.model, args.tokenizer)
        model.eval()
    fixture = json.loads(args.fixture.read_text())
    for account in fixture['accounts']:
        print(f"# {account['rule']}")
        print(f"  opening: {account['opening']}")
        for question in account['questions']:
            p, predicate = build(account, question)
            if p is None:
                print(f"  Q: {question}\n     -> ({predicate})")
                continue
            act = choose_model(model, torch, p) if model is not None else choose_teacher(p)
            print(f"  Q: {question}")
            print(f"     predicate: {predicate}  role: {p['question']['role']}")
            print(f"     choice: {act['kind']} role={act.get('role')} fact={act.get('fact_id')}")
            print(f"     reply: {f.render(act, p)}")
        print()


if __name__ == '__main__':
    main()