#!/usr/bin/env python3
"""Memory membership: a recall reply must quote a memory the speaker holds.

The recall move's target is an authored line plus a connector plus one of the
prompt's `mind.memories`. A generated reply that quotes none of them has
reached for a memory the speaker does not hold -- the failure behind the
benchmark's recall content miss, which no entity-name or copy check reaches.

The root cause is structural, not semantic: recall rows carry `copies: []`, so
the memory is copied from the prompt but never marked as a copy target, and the
copy mechanism never learns to reproduce it. The model emits a memorised line
instead ("Rosespire's mill uses 1 Wood to make 4 Paper." regardless of the
memory actually held). A copy-span wire is the fix, not a similarity model.

Measured on the wording split with the shipped checkpoint: the authored
targets all quote a held memory (0 false positives), while 26/48 (54%) of the
generated replies do not.

    python3 tools/language/measure_memory_membership.py \
        --zero <zero-checkout> \
        --checkpoint <zero>/out/moves-v7/core.ccv2 \
        --tokenizer <zero>/models/crownless-core-v2/tokenizer.json
"""
import argparse
import json
import re
import sys
from pathlib import Path

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


def norm(text):
    return re.sub(r'\s+', ' ', text.lower()).strip()


def quotes_memory(candidate, memories):
    """A held memory, matched on a prefix: the builder truncates long memories
    at a word boundary, so the full text need not appear verbatim."""
    text = norm(candidate)
    for memory in memories:
        key = norm(memory)[:40]
        if key and key in text:
            return True
    return False


def main():
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
          f'({missed / len(held):.0%})')


if __name__ == '__main__':
    main()
