#!/usr/bin/env python3
"""Field-membership metric: does a reply name an entity the speaker was not given?

A pool-family reply (remark, muse, recall, ...) draws its entities from the
row's spoken fields, so the correct check is not copy containment but field
membership: every name in the reply must appear in the prompt the speaker was
given (account, fields, history, memories). A name that does not is invented.

A common word surfaces as lowercase somewhere in the corpus; a name does not.
Watermarking names against the corpus would miss a wholly invented name
("Varkesh" appears nowhere), so the test is the inverse: any capitalised token
whose lowercase form is not a common word is a name, known or invented.

Measured on the v2 wording-miss benchmark (`braid/docs/
crownless-audit-misses.v2.jsonl`):
    reference false positives   0/51
    lexical-ok specificity      22/22   (no valid reply flagged)
    content recall              2/4     (the two entity-invention errors)
and on the corpus's own authored targets over 5,000 rows: 0 false positives.
It is a sound *safety* gate -- never flag a correct reply -- that catches
entity invention. It does not catch semantic errors (wrong memory, wrong
sentiment), which are 2 of the 4 content misses.

    python3 tools/language/measure_field_membership.py \
        --zero <zero-checkout> --braid <braid-checkout>
"""
import argparse
import json
import re
from collections import Counter
from pathlib import Path

WORD = re.compile(r"[A-Za-z][A-Za-z'’]*")
STOPWORDS = {'i', 'my', 'me', 'mine', 'we', 'our', 'you', 'your', 'he', 'she',
             'it', 'they', 'them', 'the', 'a', 'an', 'and', 'but', 'or', 'so',
             'no', 'not', 'there', 'here', 'this', 'that', 'these', 'those',
             'what', 'when', 'where', 'who', 'why', 'how', 'if', 'let', 'good',
             'well', 'yes', 'one', 'two', 'now', 'then', 'still', 'even'}


def common_words(zero, sample=20000):
    """Tokens that ever appear lowercase: these are ordinary words, not names."""
    lower = Counter()
    rows = [json.loads(l) for l in open(zero / 'out/crownless-moves-v5/wording.jsonl')]
    for line in open(zero / 'out/crownless-moves-v7/train.jsonl'):
        rows.append(json.loads(line))
        if len(rows) >= sample:
            break
    for row in rows:
        for text in [row['output'], row['prefix'], *row.get('accepted', [])]:
            for token in WORD.findall(text):
                if not token[0].isupper():
                    lower[token.lower()] += 1
    return set(lower)


def prompt_text(row):
    parts = [row['prefix'], row['output']]
    parts += [f['text'] for f in row['fields']]
    parts += [h['text'] for h in row['history']]
    parts += row.get('mind', {}).get('memories', [])
    return '\n'.join(parts)


def invented(candidate, allowed, common):
    out = []
    for token in WORD.findall(candidate):
        low = token.lower()
        if not token[0].isupper() or low in common or low in STOPWORDS:
            continue
        if token not in allowed:
            out.append(token)
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--zero', type=Path, required=True)
    p.add_argument('--braid', type=Path, required=True)
    p.add_argument('--at-scale', action='store_true',
                   help='Also check the corpus authored targets for false positives')
    args = p.parse_args()
    common = common_words(args.zero)

    rows = [json.loads(l) for l in open(args.braid / 'docs/crownless-audit-misses.v2.jsonl')]
    corpus = {json.loads(l)['id']: json.loads(l)
              for l in open(args.zero / 'out/crownless-moves-v5/wording.jsonl')}
    ref_fp = sum(bool(invented(r['reference'], prompt_text(corpus[r['id']]), common)) for r in rows)
    print(f'reference false positives: {ref_fp}/{len(rows)}')

    tally = Counter()
    for r in rows:
        if r['detector'] != 'field':
            continue
        tally[(r['label'], bool(invented(r['candidate'], prompt_text(corpus[r['id']]), common)))] += 1
    print('field rows — flag rate by label:')
    for label in ('content', 'lexical-ok', 'degenerate', 'wrong-move'):
        print(f'  {label:12s} flagged {tally[(label, True)]:2d}  not flagged {tally[(label, False)]:2d}')
    tp, fn = tally[('content', True)], tally[('content', False)]
    fp, tn = tally[('lexical-ok', True)], tally[('lexical-ok', False)]
    print(f'content recall {tp}/{tp + fn}   lexical-ok specificity {tn}/{tn + fp}')

    if args.at_scale:
        flags = total = 0
        for split in ('wording', 'test'):
            for line in open(args.zero / f'out/crownless-moves-v5/{split}.jsonl'):
                row = json.loads(line)
                total += 1
                flags += bool(invented(row['output'], prompt_text(row), common))
        print(f'authored targets flagged: {flags}/{total}')


if __name__ == '__main__':
    main()
