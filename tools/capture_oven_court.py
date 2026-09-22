#!/usr/bin/env python3
"""Capture staged Oven Court cases with the native game, not concept art.

Run beneath xvfb-run on headless Linux. The native screenshot API uses the
working directory: pass basenames and retain an explicit receipt per image.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

CASES = [(25, 'street', 1280, 0), (26, 'grain-needed', 1280, 0),
         (27, 'capacity-missing', 1280, 0), (28, 'resupplied', 1280, 0),
         (29, 'dated-note', 1280, 0), (30, 'repair-advice', 1280, 0),
         (26, 'grain-large-text', 1040, 2), (29, 'note-large-text', 1040, 2)]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    receipt = {'revision': args.revision, 'scope': 'staged native cases, not a human playthrough',
               'complete': False, 'captures': []}
    try:
        with tempfile.TemporaryDirectory(prefix='oven-court-') as temporary:
            for view, name, width, text in CASES:
                file = Path(temporary) / (name + '.png')
                command = [str(binary), '--capture-ux', str(view), file.name, str(width), str(text)]
                result = subprocess.run(command, cwd=temporary, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, timeout=90)
                (out / (name + '.log')).write_bytes(result.stdout)
                if result.returncode:
                    raise RuntimeError(f'{name}: renderer exited {result.returncode}; see its log')
                data = file.read_bytes()
                if data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
                    raise RuntimeError(f'{name}: not a PNG')
                dimensions = struct.unpack('>II', data[16:24])
                expected = (width, 620 if width == 1040 else 720)
                if dimensions != expected:
                    raise RuntimeError(f'{name}: dimensions {dimensions}, expected {expected}')
                (out / file.name).write_bytes(data)
                receipt['captures'].append({'file': file.name, 'command': command[1:],
                    'dimensions': dimensions, 'sha256': hashlib.sha256(data).hexdigest()})
        receipt['complete'] = True
    finally:
        (out / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f"Captured {len(receipt['captures'])} Oven Court views at {args.revision}")

if __name__ == '__main__':
    main()
