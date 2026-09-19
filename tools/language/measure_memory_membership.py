#!/usr/bin/env python3
"""Measure complete held-memory quote coverage in recall replies.

This lexical check preserves the complete quoted claim. Selected-memory
relevance and surrounding assertions require separate semantic checks.
See EVALUATION.md for the measurement contract.
"""
import argparse
import json
import re
import sys
from pathlib import Path


def norm(text):
    return re.sub(r'\s+', ' ', text.lower()).strip()


def quotes_memory(candidate, memories):
    """Require a full held claim, with word boundaries at both ends.

    Call with [selected_memory] when the selected claim is recorded.
    Case and whitespace are normalized; quantities and punctuation are kept.
    """
    text = norm(candidate)
    for memory in memories:
        key = norm(memory)
        if key and re.search(r"(?<!\w)" + re.escape(key) + r"(?!\w)", text):
            return True
    return False


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--zero', type=Path, required=True)
    p.add_argument('--checkpoint', type=Path, required=True)
    p.add_argument('--tokenizer', type=Path, required=True)
    p.add_argument('--corpus', type=Path, required=True,
                   help='Corpus directory holding the split (e.g. <zero>/out/crownless-moves-v8)')
    p.add_argument('--split', default='wording')
    a = p.parse_args()
    sys.path.insert(0, str(a.zero / 'scripts'))

    from tokenizers import Tokenizer
    from crownless_v2 import encode_row, generate
    from crownless_v2_export import load_export

    model, meta = load_export(a.checkpoint, a.tokenizer)
    tokenizer = Tokenizer.from_file(str(a.tokenizer))
    M = meta['meaning_ids']
    rows = [json.loads(l) for l in open(a.corpus / f'{a.split}.jsonl')]
    rows = [dict(r, kind_id=M[r['rule']]) for r in rows if r['rule'] in M]
    held = [r for r in rows if r['move'] == 'recall' and r['mind']['memories']]

    authored_fp = sum(not quotes_memory(r['output'], r['mind']['memories']) for r in held)
    print(f'recall rows with memories: {len(held)}')
    print(f'authored targets not quoting a held memory: {authored_fp}')
    missed = 0
    for row in held:
        out = generate(model, tokenizer, encode_row(
            tokenizer, row, slots=True, conversation=True,
            typed_stance=True, situation=True, social=True))
        missed += not quotes_memory(out['text'], row['mind']['memories'])
    print(f'generated replies not quoting a held memory: {missed}/{len(held)} '
          f'({missed / len(held):.0%})' if held else 'generated replies: 0 eligible rows')


if __name__ == '__main__':
    main()
