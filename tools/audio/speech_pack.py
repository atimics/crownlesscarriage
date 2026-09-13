#!/usr/bin/env python3
"""Design a cast or prepare exact speech records with local models."""

import argparse
import importlib.metadata
import json
from pathlib import Path
import shutil
import tempfile

from speech_format import (ROOT, cached_record, expected_cache_metadata,
                           load_cast, package_version, validate_record)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('script', type=Path, nargs='?')
    parser.add_argument('--cast', type=Path, default=ROOT / 'assets/audio/cast.json')
    parser.add_argument('--references', type=Path, default=ROOT / 'assets/audio/cast')
    parser.add_argument('--output', type=Path, default=ROOT / 'assets/audio/speech',
                        help='cache directory (legacy name: output)')
    parser.add_argument('--cache', type=Path, help='alias for --output')
    parser.add_argument('--promote', type=Path,
                        help='atomically copy validated cache entries into this runtime pack')
    parser.add_argument('--manifest', type=Path,
                        help='manifest path; defaults to <promote>/speech-pack-manifest.json')
    parser.add_argument('--device', choices=('cpu', 'mps', 'cuda'), default='cpu')
    parser.add_argument('--engine', choices=('pocket',), default='pocket')
    parser.add_argument('--limit', type=int, default=32)
    parser.add_argument('--voice', default='')
    parser.add_argument('--cfg-weight', type=float, default=0.5)
    parser.add_argument('--take', type=int, default=0, help='Positive values replace selected clips with a fresh take')
    parser.add_argument('--allow-download', action='store_true')
    parser.add_argument('--design-cast', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    cast = load_cast(args.cast)
    cache = args.cache or args.output
    if args.limit < 1 or not 0 <= args.cfg_weight <= 1 or not 0 <= args.take <= 1000 or (args.voice and args.voice not in cast):
        parser.error('Choose a positive limit and an existing voice')
    if args.design_cast:
        from speech_engine import design_cast
        selected = {key: value for key, value in cast.items() if not args.voice or key == args.voice}
        design_cast(selected, args.references, args.device, args.allow_download)
        return
    if args.script is None:
        parser.error('Choose a script or --design-cast')
    unique = {}
    for raw in json.loads(args.script.read_text()):
        record = validate_record(raw, cast)
        if not args.voice or record['voice'] == args.voice:
            unique[record['key']] = record
    records = list(unique.values())[:args.limit]
    if args.check:
        print(f'Checked {len(unique)} unique speech records; selected {len(records)}')
        return
    from speech_engine import SpeechEngine
    engine_version = package_version('pocket-tts')
    pending = [record for record in records if args.take > 0 or
               cached_record(cache, record,
                             expected_cache_metadata(record, args.references,
                                                     engine_version)) is None]
    if pending:
        engine = SpeechEngine(args.device, args.references, args.engine, args.allow_download, args.take, args.cfg_weight)
        for record in pending:
            engine(record, cache / (record['key'] + '.wav'))
            print(f"Prepared {record['voice']}: {record['text']}", flush=True)
    if args.promote:
        args.promote.mkdir(parents=True, exist_ok=True)
        promoted = []
        for record in records:
            source = cached_record(cache, record,
                                   expected_cache_metadata(record, args.references,
                                                           engine_version))
            if source is None:
                raise SystemExit(f"No valid cached recording for {record['key']}")
            destination = args.promote / source.name
            receipt_source = source.with_suffix('.json')
            receipt_destination = args.promote / receipt_source.name
            with tempfile.TemporaryDirectory(dir=args.promote) as temporary:
                temporary_path = Path(temporary)
                shutil.copy2(source, temporary_path / source.name)
                shutil.copy2(receipt_source, temporary_path / receipt_source.name)
                (temporary_path / source.name).replace(destination)
                (temporary_path / receipt_source.name).replace(receipt_destination)
            promoted.append(record['key'])
        manifest = args.manifest or args.promote / 'speech-pack-manifest.json'
        manifest.write_text(json.dumps({
            'version': 1,
            'cache_format': 'cc-speech-cache-v2',
            'engine': 'pocket',
            'engine_version': engine_version,
            'records': len(promoted),
            'keys': promoted,
        }, indent=2) + '\n')
        print(f'{len(promoted)} recordings promoted to {args.promote}')
    print(f'{len(records)} recordings ready')


if __name__ == '__main__':
    main()
