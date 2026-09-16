#!/usr/bin/env python3
"""Generalization metrics for the typed checkpoint, greedy.

1. Per-move copy rate: share of replies containing every spoken copy span.
2. Per-cell accuracy: (move, voice, stress) cells, seen-vs-unseen in training.
3. Wording-split accuracy: paraphrase generalization.

Deterministic (argmax); the reference encoder is byte-identical to the C
runtime per the parity suite, so these are checkpoint properties.
"""
import argparse
import json
import random
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve()
CROWN = HERE.parents[2]  # tools/language/<name>.py inside the crownless checkout
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--zero', type=Path, default=CROWN.parent / 'zero')
p.add_argument('--corpus', type=Path, default=None)
p.add_argument('--copy-per-move', type=int, default=20)
p.add_argument('--cell-sample', type=int, default=5)
p.add_argument('--wording-sample', type=int, default=200)
p.add_argument('--model', type=Path, default=None)
p.add_argument('--tokenizer', type=Path, default=None)
a = p.parse_args()
ZERO = a.zero
if a.corpus is None:
    a.corpus = ZERO / 'out/crownless-moves-v2'
if a.model is None:
    a.model = CROWN / 'assets/language/core.ccv2'
if a.tokenizer is None:
    a.tokenizer = CROWN / 'assets/language/tokenizer.json'
sys.path.insert(0, str(ZERO / 'scripts'))

import torch
from tokenizers import Tokenizer
from crownless_v2 import encode_row, generate
from crownless_v2_export import load_export

torch.set_num_threads(4)
CORPUS = a.corpus
model, meta = load_export(a.model, a.tokenizer)
tokenizer = Tokenizer.from_file(str(a.tokenizer))
MEANING = meta['meaning_ids']


def read(split):
    with open(CORPUS / f'{split}.jsonl') as f:
        return [json.loads(line) for line in f]


def prep(row):
    row = dict(row)
    if row['rule'] not in MEANING:
        return None
    row['kind_id'] = MEANING[row['rule']]
    return row


def speak(row):
    rec = encode_row(tokenizer, row, slots=True, conversation=True, typed_stance=True)
    return generate(model, tokenizer, rec)


def main():
    t0 = time.time()
    test = [r for r in (prep(r) for r in read('test')) if r]
    print(f'test rows usable: {len(test)}')

    # 1. Per-move copy rate.
    rng = random.Random(7)
    print('--- per-move copy (spoken spans contained) ---')
    copy_all, copy_hit = 0, 0
    for move in sorted({r['move'] for r in test}):
        pool = [r for r in test if r['move'] == move and
                any(c.get('spoken') and c.get('text') for c in r['copies'])]
        sample = rng.sample(pool, min(a.copy_per_move, len(pool)))
        hits = stopped = 0
        for row in sample:
            out = speak(row)
            stopped += out['stopped']
            spans = [c['text'] for c in row['copies'] if c.get('spoken') and c.get('text')]
            hits += bool(spans) and all(s in out['text'] for s in spans)
        copy_all += len(sample)
        copy_hit += hits
        print(f'{move:9s} {hits:3d}/{len(sample):3d} = {hits / max(1, len(sample)):.0%}  stopped {stopped}/{len(sample)}')
    print(f'copy overall: {copy_hit}/{copy_all} = {copy_hit / copy_all:.0%}')

    # 2. Per-cell accuracy, seen vs unseen.
    train_cells = set()
    with open(CORPUS / 'train.jsonl') as f:
        for line in f:
            r = json.loads(line)
            train_cells.add((r['move'], r['voice'], r['mind']['stress']))
    by_cell = {}
    for r in test:
        by_cell.setdefault((r['move'], r['voice'], r['mind']['stress']), []).append(r)
    seen_hit = seen_n = unseen_hit = unseen_n = 0
    for cell in sorted(by_cell):
        sample = rng.sample(by_cell[cell], min(a.cell_sample, len(by_cell[cell])))
        hits = sum(speak(r)['text'] in r.get('accepted', [r['output']]) for r in sample)
        if cell in train_cells:
            seen_hit += hits
            seen_n += len(sample)
        else:
            unseen_hit += hits
            unseen_n += len(sample)
    print(f'--- cells: {len(by_cell)} test cells, {sum(1 for c in by_cell if c not in train_cells)} unseen in train ---')
    print(f'seen   cells: {seen_hit}/{seen_n} = {seen_hit / max(1, seen_n):.0%}')
    print(f'unseen cells: {unseen_hit}/{unseen_n} = {unseen_hit / max(1, unseen_n):.0%}')

    # 3. Wording split.
    wording = [r for r in (prep(r) for r in read('wording')) if r][:a.wording_sample]
    hits = sum(speak(r)['text'] in r.get('accepted', [r['output']]) for r in wording)
    print(f'--- wording split: {hits}/{len(wording)} = {hits / len(wording):.0%} ---')
    print(f'elapsed {time.time() - t0:.0f}s')


if __name__ == '__main__':
    main()
