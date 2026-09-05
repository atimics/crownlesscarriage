#!/usr/bin/env python3
"""Prepare the public soundtrack files for Cloudflare Pages."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path


def build(audio_dir, output, catalog_path):
    catalog = json.loads(catalog_path.read_text())
    takes = {row['stem']: row for row in catalog['takes']}
    tracks = []
    (output / 'audio').mkdir(parents=True, exist_ok=True)
    for source in sorted(audio_dir.glob('*.mp3')):
        if source.stem not in takes:
            raise ValueError(f'Unknown soundtrack stem: {source.name}')
        data = source.read_bytes()
        if not 1024 <= len(data) <= 16 * 1024 * 1024:
            raise ValueError(f'Audio file size exceeds the player limit: {source.name}')
        digest = hashlib.sha256(data).hexdigest()
        name = f'{source.stem}-{digest}.mp3'
        shutil.copy2(source, output / 'audio' / name)
        take = takes[source.stem]
        cue = catalog['cues'][take['track'] - 1]
        tracks.append(dict(stem=source.stem, file=f'audio/{name}',
                           title=cue['title'], duration=take['duration'],
                           bytes=len(data), sha256=digest))
    manifest = dict(version=1, total_takes=len(takes), available_takes=len(tracks),
                    tracks=tracks)
    (output / 'catalog.json').write_text(json.dumps(manifest, indent=2) + '\n')
    index = 'CROWNLESS_MUSIC 1\n' + ''.join(f"{t['file'][6:]}\n" for t in tracks)
    (output / 'catalog.txt').write_text(index)
    (output / 'index.html').write_text(
        '<!doctype html><meta charset="utf-8"><title>Crownless music</title>'
        '<h1>Crownless music</h1>'
        f'<p>{len(tracks)} soundtrack takes are ready.</p>'
        '<p><a href="catalog.json">Music catalog</a></p>')
    (output / '404.html').write_text('<!doctype html><title>Track unavailable</title><p>Track unavailable.</p>')
    (output / '_headers').write_text(
        '/*\n  Access-Control-Allow-Origin: *\n  X-Content-Type-Options: nosniff\n'
        '/audio/*\n  Cache-Control: public, max-age=31536000, immutable\n'
        '/catalog.json\n  Cache-Control: public, max-age=60\n'
        '/catalog.txt\n  Cache-Control: public, max-age=60\n')
    return manifest


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audio-dir', type=Path, default=root / 'assets/audio/music')
    parser.add_argument('--output', type=Path, default=root / 'out/music-site')
    args = parser.parse_args()
    result = build(args.audio_dir, args.output, root / 'assets/audio/music/catalog.json')
    print(f"Prepared {result['available_takes']} takes in {args.output}")


if __name__ == '__main__':
    main()
