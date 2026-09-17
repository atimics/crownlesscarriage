#!/usr/bin/env python3
"""Build the v2 wording-miss benchmark: cut by detector family.

The v1 set labeled 51 misses by semantics and handed a syntax study a set that
is mostly not syntactic. v2 keeps the labels but adds the detector each row
actually admits --

  copy     a claim-bearing row with spoken copy spans; the frozen contract
           (diathesis or copy containment) can see it.
  field    a pool row whose reply draws an entity from a spoken field; the
           detector is field membership, a separate metric.
  semantic neither copies nor spoken fields; no syntactic or copy detector
           reaches it, so it is excluded, not scored.

-- and carries copies[], fields[] and stopped so a scorer implements the
contract instead of inferring it. Candidates come from checkpoint moves-v5
(96544bec); stopped is recovered by regenerating with that same export.

    python3 tools/language/build_audit_v2.py \
        --v1 <braid>/docs/crownless-audit-misses.jsonl \
        --zero <zero-checkout> \
        --out <braid>/docs/crownless-audit-misses.v2.jsonl
"""
import argparse
import json
import sys
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--v1', type=Path, required=True)
p.add_argument('--zero', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
sys.path.insert(0, str(a.zero / 'scripts'))

import torch
from tokenizers import Tokenizer
from crownless_v2 import encode_row, generate
from crownless_v2_export import load_export

torch.set_num_threads(4)
CHECKPOINT = a.zero / 'out/moves-v5/core.ccv2'
TOKENIZER = a.zero / 'models/crownless-core-v2/tokenizer.json'
# The corpus the candidates were generated from: v1 was built against the v5
# wording split, so v2 must read the same rows or the histories differ.
CORPUS = a.zero / 'out/crownless-moves-v5/wording.jsonl'
model, meta = load_export(CHECKPOINT, TOKENIZER)
tokenizer = Tokenizer.from_file(str(TOKENIZER))
M = meta['meaning_ids']

# Rows whose label is a semantic judgement no detector in the contract reaches,
# or that contradict the set's own rule. #5 is a remark with no copy span and a
# field-sourced entity: the substitution is not established as an error, so it
# is named and excluded rather than scored.
EXCLUDED = {5: 'ambiguous label: remark with no copy span; field substitution not established as error'}

corpus = {}
for line in open(CORPUS):
    row = json.loads(line)
    corpus[row['id']] = row
rows = [json.loads(l) for l in open(a.v1)]
assert len(rows) == 51, len(rows)

out = []
for index, row in enumerate(rows):
    held = corpus[row['id']]
    copies = [{'field': c['field'], 'role': c['role'], 'text': c['text']}
              for c in held['copies'] if c.get('spoken') and c.get('text')]
    fields = [{'field': f['field'], 'role': f['role'], 'text': f['text'],
               'spoken': bool(f.get('spoken'))} for f in held['fields']]
    speak_fields = [f['text'] for f in fields if f['spoken']]
    detector = 'copy' if copies else 'field' if speak_fields else 'semantic'
    reason = EXCLUDED.get(index)
    if reason is None and detector == 'semantic':
        reason = 'no copies and no spoken fields: no syntactic or copy detector applies'
    encoded = {**held, 'kind_id': M[held['rule']]}
    result = generate(model, tokenizer, encode_row(
        tokenizer, encoded, slots=True, conversation=True,
        typed_stance=True, situation=True, social=True))
    assert result['text'] == row['candidate'], (index, result['text'], row['candidate'])
    out.append({
        'id': row['id'], 'reference': row['reference'], 'candidate': row['candidate'],
        'accepted': row['accepted'], 'label': row['label'], 'move': held['move'],
        'detector': detector,
        'score_group': 'excluded' if reason else detector,
        'excluded_reason': reason,
        'copies': copies, 'fields': fields, 'speak_fields': speak_fields,
        'stopped': bool(result['stopped']),
    })

a.out.write_text(''.join(json.dumps(r, ensure_ascii=False) + '\n' for r in out))
from collections import Counter
groups = Counter(r['score_group'] for r in out)
print(f'{len(out)} rows -> {dict(groups)}')
print(f'stopped: {sum(r["stopped"] for r in out)}/{len(out)}')