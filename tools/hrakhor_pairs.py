"""Pair completed English core-model lines with Hra'khor corruption.

Input is JSONL: kind (numeric event ID), text (held account), output (English),
confidence (0..100), and optional variant (0 or 1). Other labels pass through.
Output keeps the input row under source and adds english and hrakhor strings.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def pairs(rows, probe, strength):
    if not 0 <= strength <= 100:
        raise ValueError('strength must be 0..100')
    for row in rows:
        english = row['output']
        if not isinstance(english, str) or '\n' in english or '\r' in english or '\0' in english:
            raise ValueError('output must be one completed English line')
        result = subprocess.run(
            [str(probe), str(row['kind']), str(row['confidence']),
             str(row.get('variant', 0)), row['text'], '--hrakhor', str(strength),
             '--english', english], text=True, capture_output=True, check=True)
        yield {'schema': 'crownless.hrakhor_pair.v1', 'strength': strength,
               'source': row, 'english': english, 'hrakhor': result.stdout.removesuffix('\n')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--strength', type=int, default=100)
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    digest = hashlib.sha256(probe.read_bytes()).hexdigest()
    for pair in pairs((json.loads(line) for line in sys.stdin if line.strip()), probe, args.strength):
        pair['probe_sha256'] = digest
        print(json.dumps(pair, ensure_ascii=False))


if __name__ == '__main__':
    main()
