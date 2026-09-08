#!/usr/bin/env python3
"""Capture and repeat the existing runner's production protocol."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runner', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--years', type=int, default=40)
    parser.add_argument('--seed', type=lambda text: int(text, 0), default=0x5EED0001)
    args = parser.parse_args()
    if args.years < 0 or not 0 <= args.seed <= 0xFFFFFFFF:
        parser.error('Use a positive year count or zero and a 32-bit seed.')
    runner = args.runner.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    status = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)
    cache = runner.parent / 'CMakeCache.txt'
    build_mode = None
    if cache.exists():
        build_mode = next((line.split('=', 1)[1] for line in cache.read_text().splitlines()
                           if line.startswith('CMAKE_BUILD_TYPE:STRING=')), None)
    manifest = {'protocol': 1, 'commit': commit, 'working_tree_status': status,
                'runner_sha256': digest(runner), 'build_mode': build_mode,
                'seed': args.seed, 'seed_index': None, 'seed_mapping': 'direct numeric seed',
                'years': args.years, 'runs': []}
    for fixture in ['baseline', 'opened-production-pilots']:
        runs = []
        for repeat in range(2):
            report = output / f'{fixture}-{repeat}.jsonl'
            save = output / f'{fixture}-{repeat}.ccsave'
            command = [str(runner), '--json', '--seed', str(args.seed), '--years', str(args.years),
                       '--report-every', '1', '--save', str(save)]
            if fixture == 'opened-production-pilots':
                command.append('--opened-production-pilots')
            with report.open('w') as stream:
                subprocess.run(command, stdout=stream, check=True, cwd=ROOT)
            records = [json.loads(line) for line in report.read_text().splitlines()]
            expected_days = [1 + 365 * year for year in range(args.years + 1)]
            if [record['day'] for record in records] != expected_days:
                raise RuntimeError('Capture checkpoint days differ from the requested protocol.')
            runs.append({'command': command, 'report': report.name, 'report_sha256': digest(report),
                         'save': save.name, 'save_sha256': digest(save),
                         'checkpoint_days': expected_days, 'final_state_hash': records[-1]['state_hash']})
        if runs[0]['report_sha256'] != runs[1]['report_sha256']:
            raise RuntimeError(f'{fixture} repeats produced different reports.')
        manifest['runs'].append({'fixture': fixture, 'repeat_match': True, 'captures': runs})
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(output / 'manifest.json')

if __name__ == '__main__':
    main()
