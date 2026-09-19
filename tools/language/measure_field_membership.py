#!/usr/bin/env python3
"""Check capitalized name tokens against input evidence.

This lexical diagnostic measures name membership. Use separate claim checks
for roles, quantities, source, time and offer scope. Freeze calibration from
training data before scoring held-out rows. See EVALUATION.md for the contract.
"""
import argparse
import hashlib
import json
import unicodedata
import re
from collections import Counter
from pathlib import Path

WORD = re.compile(r"[A-Za-z][A-Za-z'’]*")
STOPWORDS = {'i', 'my', 'me', 'mine', 'we', 'our', 'you', 'your', 'he', 'she',
             'it', 'they', 'them', 'the', 'a', 'an', 'and', 'but', 'or', 'so',
             'no', 'not', 'there', 'here', 'this', 'that', 'these', 'those',
             'what', 'when', 'where', 'who', 'why', 'how', 'if', 'let', 'good',
             'well', 'yes', 'one', 'two', 'now', 'then', 'still', 'even'}


def normalized(text):
    return unicodedata.normalize('NFKC', text).replace('’', "'").casefold()


def entity_tokens(text):
    # The possessive suffix expresses ownership of the same entity.
    return {normalized(t).removesuffix("'s") for t in WORD.findall(text)}


def freeze_calibration(source, destination, sample=20000):
    """Build once from the training split; record its exact identity."""
    if source.name != 'train.jsonl':
        raise ValueError('calibration source must be train.jsonl')
    if sample <= 0:
        raise ValueError('sample must be positive')
    data = source.read_bytes()
    words = set()
    count = 0
    for line in data.decode('utf-8').splitlines():
        if count == sample:
            break
        row = json.loads(line)
        for text in [row['prefix'], row['output'], *row.get('accepted', [])]:
            for token in WORD.findall(text):
                if token[0].islower():
                    words.add(normalized(token))
        count += 1
    receipt = {'version': 1, 'source_split': 'train',
               'source_sha256': hashlib.sha256(data).hexdigest(),
               'rows': count, 'words': sorted(words)}
    # Exclusive creation keeps an existing calibration fixed.
    with destination.open('x', encoding='utf-8') as f:
        json.dump(receipt, f, indent=2, sort_keys=True)
        f.write('\n')
    return receipt


def common_words(calibration):
    receipt = json.loads(calibration.read_text(encoding='utf-8'))
    if receipt['version'] != 1 or receipt['source_split'] != 'train':
        raise ValueError('expected version 1 training calibration')
    return set(receipt['words'])


def prompt_text(row):
    """Use captured input when present, or the legacy corpus prefix.

    model_input contains the decoded prefix after truncation and the actual
    accessible copy strings. Targets and raw row metadata stay separate.
    """
    if 'model_input' in row:
        evidence = row['model_input']
        return '\n'.join([evidence['prefix'], *evidence['copy_spans']])
    return row['prefix']


def invented(candidate, allowed, common, aliases=None):
    """Match whole name tokens; explicit aliases map a variant to its name."""
    aliases = {normalized(k).removesuffix("'s"): normalized(v).removesuffix("'s")
               for k, v in (aliases or {}).items()}
    allowed_tokens = {aliases.get(t, t) for t in entity_tokens(allowed)}
    out = []
    for token in WORD.findall(candidate):
        low = normalized(token).removesuffix("'s")
        if not token[0].isupper() or low in common or low in STOPWORDS:
            continue
        if aliases.get(low, low) not in allowed_tokens:
            out.append(token)
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--zero', type=Path, required=True)
    p.add_argument('--braid', type=Path)
    p.add_argument('--calibration', type=Path, required=True)
    p.add_argument('--freeze-calibration', action='store_true',
                   help='Create calibration from v7 train.jsonl, then exit')
    p.add_argument('--at-scale', action='store_true',
                   help='Also check the corpus authored targets for false positives')
    args = p.parse_args()
    if args.freeze_calibration:
        freeze_calibration(args.zero / 'out/crownless-moves-v7/train.jsonl',
                           args.calibration)
        return
    if args.braid is None:
        p.error('--braid is required for evaluation')
    common = common_words(args.calibration)
    print('evidence: corpus prefix (legacy rows); captured input when supplied')
    print('calibration sha256:', hashlib.sha256(args.calibration.read_bytes()).hexdigest())

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
