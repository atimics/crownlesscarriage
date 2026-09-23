#!/usr/bin/env python3
"""Capture all six town horizons in six moods, with revision/hash receipts.

Uses the production street renderer under a graphics context. Staged scenes are
not a replacement for an ordinary-control journey or physical-device testing.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from capture_four_towns import png_receipt

MOODS = ('dawn', 'day', 'rain', 'dusk', 'night', 'omen')

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path, help='renderer_regression_tests executable')
    parser.add_argument('--output', type=Path, default=Path('out/town-skies'))
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f'Binary not found: {binary}')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    expected = [output / f'town-{town}-{mood}.png' for town in range(6) for mood in MOODS]
    for image in expected:
        image.unlink(missing_ok=True)
    revision = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True,
                              text=True, check=True).stdout.strip()
    command = [str(binary), '--sky-captures', str(output)]
    receipt = {'revision': revision, 'command': command, 'complete': False,
               'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
               'evidence': 'staged production-renderer captures; not an ordinary-control playthrough',
               'captures': []}
    try:
        with (output / 'capture.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
        for image in expected:
            receipt['captures'].append(png_receipt(image))
        hashes = {item['sha256'] for item in receipt['captures']}
        if len(hashes) != len(expected):
            raise ValueError('Distinct town/mood captures unexpectedly have identical images')
        receipt['complete'] = True
    finally:
        (output / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Captured {len(expected)} town sky frames at {revision}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
