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
from fact_question import facts_from_fields, person, role_from_question

FIXTURE = ROOT / 'tools/dialogue/fixtures/fact-dialogue.json'


def build(account, question):
    """Return a fact_policy person for one account and natural question."""
    role = role_from_question(question)
    if role is None:
        return None, 'question did not parse to a role'
    event_id = str(900000 + int(account['id'].split(':')[-1]) + 1)
    facts = facts_from_fields(account['fields'], '1', event_id, account.get('confidence'), day=10,
                              source=account.get('source', 'told'))
    if not facts:
        return None, 'account has no typed facts'
    return person('1', '2', facts, role, day=10), None


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
            p, error = build(account, question)
            if error:
                print(f"  Q: {question}\n     -> ({error})")
                continue
            act = choose_model(model, torch, p) if model is not None else choose_teacher(p)
            print(f"  Q: {question}")
            print(f"     parsed role: {p['question']['role']}")
            print(f"     choice: {act['kind']} role={act.get('role')} fact={act.get('fact_id')}")
            print(f"     reply: {f.render(act, p)}")
        print()


if __name__ == '__main__':
    main()