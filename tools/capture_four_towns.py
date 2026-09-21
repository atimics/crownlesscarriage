#!/usr/bin/env python3
"""Capture the sixteen authored town views through the shipped native client.

These are staged peaceful/condition snapshots, not an ordinary-input playthrough.
Run under Xvfb on Linux. Each receipt records the command, revision and image hash.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

# Town IDs are campaign order (not CcSettlementFunction order).
SCENES = (
    (0, 'thornford', 'river-crossing', 82.0, 34.0),
    (0, 'thornford', 'threshing-green', 44.0, 29.0),
    (0, 'thornford', 'granary-rise', 78.0, 27.0),
    (0, 'thornford', 'cartwright-yard', 42.4, 55.2),
    (1, 'gloamgate', 'market-circle', 44.0, 29.0),
    (1, 'gloamgate', 'archive-steps', 39.5, 23.0),
    (1, 'gloamgate', 'cloth-yard', 24.0, 28.0),
    (1, 'gloamgate', 'coach-court', 42.4, 55.2),
    (3, 'silverwick', 'foundry-terrace', 82.0, 34.0),
    (3, 'silverwick', 'company-store', 44.0, 29.0),
    (3, 'silverwick', 'workers-lane', 33.0, 25.0),
    (3, 'silverwick', 'ore-wagon-yard', 42.4, 55.2),
    (2, 'alderwatch', 'contested-bridge', 82.0, 34.0),
    (2, 'alderwatch', 'muster-spine', 44.0, 29.0),
    (2, 'alderwatch', 'keep', 78.5, 27.0),
    (2, 'alderwatch', 'lower-bailey', 42.4, 55.2),
)


def png_receipt(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 33 or data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
        raise ValueError(f'Capture is not a PNG: {path}')
    width, height = struct.unpack('>II', data[16:24])
    if width < 320 or height < 180:
        raise ValueError(f'Capture is too small: {path} ({width}x{height})')
    return {'file': path.name, 'width': width, 'height': height,
            'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def capture(binary: Path, output: Path, scene: tuple, state: str) -> dict:
    town_id, town, name, x, z = scene
    stem = f'{town}-{name}-{state}'
    image = output / f'{stem}.png'
    image.unlink(missing_ok=True)  # A stale capture cannot make a failed run pass.
    # The shipped screenshot API prefixes cwd; absolute names duplicate it.
    command = [str(binary), '--capture-town-state', str(town_id), str(x), str(z),
               os.path.relpath(image, Path.cwd()), state]
    with (output / f'{stem}.log').open('w') as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT,
                       timeout=120)
    return {'town': town, 'scene': name, 'state': state, 'command': command,
            **png_receipt(image)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--output', type=Path, default=Path('out/four-town-review'))
    parser.add_argument('--conditions', action='store_true',
                        help='Also capture burnt/rebuilding/hungry town hearts.')
    parser.add_argument('--plan', action='store_true', help='Print the scene plan without launching.')
    args = parser.parse_args()
    if args.plan:
        print(json.dumps(SCENES, indent=2))
        return 0
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f'Native client not found: {binary}')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    revision = subprocess.run(['git', 'rev-parse', 'HEAD'], text=True,
                              capture_output=True, check=False).stdout.strip() or 'unknown'
    receipt = {'revision': revision,
               'evidence': 'staged town captures; not an ordinary-input walkthrough',
               'captures': [], 'complete': False}
    try:
        for scene in SCENES:
            receipt['captures'].append(capture(binary, output, scene, 'peaceful'))
        if args.conditions:
            for scene in (SCENES[1], SCENES[4], SCENES[9]):
                for state in ('burnt', 'rebuilding', 'hungry'):
                    receipt['captures'].append(capture(binary, output, scene, state))
        hashes = [item['sha256'] for item in receipt['captures'][:16]]
        if len(set(hashes)) != 16:
            raise ValueError('Two distinct authored scenes produced identical images')
        receipt['complete'] = True
    finally:
        (output / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f"Captured {len(receipt['captures'])} views at {revision}")
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'Four-town capture failed: {error}', file=sys.stderr)
        raise SystemExit(1)
