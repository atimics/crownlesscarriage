#!/usr/bin/env python3
"""Capture six town roads and the bakery and stonecutter through the native client."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess

from capture_four_towns import png_receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--output', type=Path, default=Path('out/town-refresh-review'))
    args = parser.parse_args()
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    receipt = {'revision': revision, 'evidence': 'staged native client captures',
               'captures': [], 'complete': False}
    scenes = []
    towns = ('thornford', 'gloamgate', 'alderwatch', 'silverwick', 'rosespire', 'hollowbarrow')
    for town, name in enumerate(towns):
        scenes.append((f'{name}-arrival', ['--capture-town-arrival', str(town), '0.45']))
        scenes.append((f'{name}-carriage-court', ['--capture-town', str(town), '42.4', '55.2']))
    for name, good in (('bakery', '0'), ('stonecutter', '10')):
        for view in ('front', 'interior', 'trade'):
            flag = '--capture-shop' if view == 'trade' else f'--capture-shop-{view}'
            scenes.append((f'rosespire-{name}-{view}', [flag, '4', good]))
    try:
        for name, arguments in scenes:
            image = output / f'{name}.png'
            image.unlink(missing_ok=True)
            command = [str(binary), *arguments, os.path.relpath(image, Path.cwd())]
            with (output / f'{name}.log').open('w') as log:
                subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT,
                               timeout=120)
            receipt['captures'].append({'scene': name, 'command': command,
                                        **png_receipt(image)})
        receipt['complete'] = True
    finally:
        (output / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Captured {len(scenes)} town and shop views at {revision}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
