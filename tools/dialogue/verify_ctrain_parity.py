"""Check that the native game runtime picks what the C trainer's forward pass picks.

Builds a native probe for MODEL, runs every row of DATA through both, and fails on
any difference. Standard library only; `ctrain` and the CMake libraries are inputs.
"""
import argparse
import json
from pathlib import Path
import subprocess

from build_syntax_probe import build


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--ctrain', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True, help='CMake build with the story and sim libraries')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build(args.model.resolve(), args.build.resolve(), args.output.resolve())
    probe = args.output / 'probe'
    trainer = subprocess.run([str(args.ctrain), 'eval', '--model', str(args.model), '--data', str(args.data), '--picks'],
                             capture_output=True, text=True, check=True)
    picks = [int(x) + 1024 for x in trainer.stdout.split()]
    rows = [line.split() for line in args.data.read_text().splitlines()]
    if len(picks) != len(rows):
        raise ValueError('trainer did not answer every row')
    differ, correct = [], 0
    for index, (row, pick) in enumerate(zip(rows, picks)):
        count = int(row[2])
        result = subprocess.run([str(probe), str(args.model), '--policy-prefix', ','.join(row[3:3 + count]), '--generate'],
                                capture_output=True, text=True, check=True)
        native = int(result.stdout.split()[0])
        correct += native - 1024 == int(row[1])
        if native != pick:
            differ.append({'row': index, 'trainer': pick, 'native': native})
    report = {'rows': len(rows), 'differences': len(differ), 'native_correct': correct, 'first': differ[:5]}
    (args.output / 'parity.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    if differ:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
