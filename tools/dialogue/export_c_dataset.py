"""Write the v3 meaning dataset as plain text for the C trainer (ctrain).

Each row is `<intent> <target> <count> <token>...`. The teacher stays in Python;
training and export need no Python packages beyond the standard library.
"""
import argparse
import hashlib
import json
from pathlib import Path

from meaning import INTENTS
from meaning_data import dataset


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--worlds', type=int, default=256)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    receipt = {'format': 'crownless-meaning-v3', 'worlds': args.worlds, 'splits': {}}
    for name, rows in dataset(args.worlds).items():
        text = ''.join(f"{INTENTS.index(r['teacher_intent'])} {r['teacher_index']} "
                       f"{len(r['prompt']['tokens'])} {' '.join(map(str, r['prompt']['tokens']))}\n" for r in rows)
        path = args.output / f'{name}.txt'
        path.write_text(text)
        receipt['splits'][name] = {'rows': len(rows), 'sha256': hashlib.sha256(text.encode()).hexdigest()}
    (args.output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt))


if __name__ == '__main__':
    main()
