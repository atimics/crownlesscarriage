"""Frozen A/B pilot for a dialogue-model promotion decision.

Issue 809 asks for a matched comparison before a checkpoint is promoted. This
runs the first two arms on the task the in-game Chat path actually uses: given a
held account, produce the NPC's spoken line.

- **A authored**: the account grammar's authored rendering.
- **B shipped**: the shipped in-game ``assets/language/core.ccv2`` generating.

The comparison is frozen before scoring. It is development material, not the
sealed 24-family pilot, which still needs the offer/memory encounter families.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/language'))
from measure_field_membership import invented  # noqa: E402


def norm(text):
    return re.sub(r'\s+', ' ', text).strip().lower()


def copyable_fields(packet):
    return [f['text'] for f in packet['fields']
            if f.get('spoken') and f.get('knowledge') != 3 and f.get('text')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zero', type=Path, required=True)
    parser.add_argument('--model', type=Path, default=ROOT / 'assets/language/core.ccv2')
    parser.add_argument('--tokenizer', type=Path, default=ROOT / 'assets/language/tokenizer.json')
    parser.add_argument('--accounts', type=Path,
                        default=Path('/Users/ratimics/develop/zero/experiments/reviewed-dialogue-pilot/input/accounts.jsonl'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--limit', type=int, default=0)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output must be fresh')
    sys.path.insert(0, str(args.zero / 'scripts'))
    import torch
    from tokenizers import Tokenizer
    from crownless_v2 import generate
    from crownless_v2_export import load_export
    from run_reviewed_dialogue_pilot import make_row, encode, read

    torch.set_num_threads(4)
    accounts = [a for a in read(args.accounts) if a.get('packet')]
    if args.limit:
        accounts = accounts[:args.limit]
    model, meta = load_export(args.model, args.tokenizer)
    model.mode = 'conversation'
    model.eval()
    tok = Tokenizer.from_file(str(args.tokenizer))

    args.output.mkdir(parents=True)
    cases = []
    for account in accounts:
        authored = account['model_text']
        row = make_row(account, '', meta)
        record = encode(tok, row)
        with torch.no_grad():
            generated = generate(model, tok, record, max_tokens=80)
        text = generated['text']
        fields = copyable_fields(account['packet'])
        evidence = '\n'.join([row['prefix'], *[f['text'] for f in row['fields']]])
        confidence = account['source']['input']['confidence']
        cases.append({
            'id': account['id'], 'rule': account['packet']['rule'], 'confidence': confidence,
            'retold': bool(account['source']['input']['retellings'] >= 4),
            'authored': authored, 'generated': text, 'stopped': generated['stopped'],
            'exact': norm(text) == norm(authored),
            'fields': fields,
            'fields_present': [f for f in fields if norm(f) in norm(text)],
            'invented': invented(text, evidence, set()),
        })

    def rate(rows, predicate):
        return sum(predicate(row) for row in rows), len(rows)

    def block(rows):
        return {
            'cases': len(rows),
            'exact': rate(rows, lambda r: r['exact']),
            'field_preserved': (sum(len(r['fields_present']) for r in rows),
                                sum(len(r['fields']) for r in rows)),
            'cases_all_fields': rate(rows, lambda r: bool(r['fields']) and len(r['fields_present']) == len(r['fields'])),
            'cases_no_invented': rate(rows, lambda r: not r['invented']),
        }

    summary = {
        'task': 'held account -> spoken line (in-game Chat path)',
        'arms': {'A': 'authored', 'B': 'shipped core.ccv2'},
        'all': block(cases),
        'confident': block([r for r in cases if r['confidence'] >= 70]),
        'uncertain': block([r for r in cases if r['confidence'] < 70]),
        'model_sha256': hashlib.sha256(args.model.read_bytes()).hexdigest(),
    }
    (args.output / 'cases.json').write_text(json.dumps(cases, indent=2, ensure_ascii=False) + '\n')
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()