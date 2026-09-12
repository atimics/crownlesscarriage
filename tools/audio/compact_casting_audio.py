#!/usr/bin/env python3
"""Save small listening copies and retain the original WAV masters separately."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import wave


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--review', type=Path, required=True)
    parser.add_argument('--masters', type=Path, required=True)
    args = parser.parse_args()
    review, masters = args.review.resolve(), args.masters.resolve()
    if masters == review or review in masters.parents:
        parser.error('Choose a master directory outside the listening review.')
    records = []
    for folder in ('proposed', 'trial'):
        for source in sorted((review / folder).glob('*.wav')):
            relative = source.relative_to(review)
            backup = masters / relative
            backup.parent.mkdir(parents=True, exist_ok=True)
            if backup.exists() and sha(backup) != sha(source):
                raise ValueError(f'A different master already exists: {backup}')
            shutil.copy2(source, backup)
            target = source.with_suffix('.ogg')
            subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
                            '-i', str(source), '-map_metadata', '-1', '-c:a', 'libopus',
                            '-b:a', '48k', str(target)], check=True)
            with wave.open(str(source)) as original:
                duration = original.getnframes() / original.getframerate()
            info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error',
                '-show_entries', 'format=duration:stream=codec_name', '-of', 'json', str(target)]))
            if info['streams'][0]['codec_name'] != 'opus' or abs(float(info['format']['duration']) - duration) > 0.03:
                raise ValueError(f'Listening copy failed verification: {target}')
            subprocess.run(['ffmpeg', '-v', 'error', '-i', str(target), '-f', 'null', '-'], check=True)
            records.append(dict(master=str(relative), master_sha256=sha(source),
                master_bytes=source.stat().st_size, listening=str(target.relative_to(review)),
                listening_sha256=sha(target), listening_bytes=target.stat().st_size, seconds=duration))
            source.unlink()
    if records:
        manifest = review / 'listening-copies.json'
        prior = json.loads(manifest.read_text())['files'] if manifest.exists() else []
        changed = {row['master'] for row in records}
        records = [row for row in prior if row['master'] not in changed] + records
        manifest.write_text(json.dumps(dict(codec='Opus', bitrate=48000,
            note='Compressed listening copies. Original trial hashes describe the WAV masters.',
            files=sorted(records, key=lambda row: row['master'])), indent=2) + '\n')
    print(f'Verified {len(records)} listening copies; masters retained in {masters}')


if __name__ == '__main__':
    main()
