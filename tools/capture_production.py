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
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    status = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)
    cache = runner.parent / 'CMakeCache.txt'
    build_mode = None
    if cache.exists():
        build_mode = next((line.split('=', 1)[1] for line in cache.read_text().splitlines()
                           if line.startswith('CMAKE_BUILD_TYPE:STRING=')), None)
    manifest = {'protocol': 7, 'status': 'running', 'commit': commit, 'working_tree_status': status,
                'runner_sha256': digest(runner), 'build_mode': build_mode,
                'seed': args.seed, 'seed_index': None, 'seed_mapping': 'direct numeric seed',
                'years': args.years, 'runs': []}
    output.mkdir(parents=True, exist_ok=False)

    def checkpoint() -> None:
        pending = output / 'manifest.json.tmp'
        pending.write_text(json.dumps(manifest, indent=2) + '\n')
        pending.replace(output / 'manifest.json')

    checkpoint()
    try:
        for fixture in ['baseline', 'opened-production-pilots']:
            for policy in ['natural-history', 'slain-at-day-1']:
                runs = []
                group = {'fixture': fixture, 'dragon_policy': policy,
                         'comparison_scope': 'whole-policy', 'repeat_match': None,
                         'captures': runs}
                manifest['runs'].append(group)
                for repeat in range(2):
                    report = output / f'{fixture}-{policy}-{repeat}.jsonl'
                    save = output / f'{fixture}-{policy}-{repeat}.ccsave'
                    errors = output / f'{fixture}-{policy}-{repeat}.stderr'
                    command = [str(runner), '--json', '--seed', str(args.seed), '--years', str(args.years),
                               '--report-every', '1', '--save', str(save)]
                    if fixture == 'opened-production-pilots':
                        command.append('--opened-production-pilots')
                    if policy == 'slain-at-day-1':
                        command.append('--dragon-slain-day-one')
                    capture = {'command': command, 'status': 'running',
                               'report': report.name, 'save': save.name, 'stderr': errors.name}
                    runs.append(capture)
                    checkpoint()
                    try:
                        with report.open('w') as stream, errors.open('w') as stderr:
                            result = subprocess.run(command, stdout=stream, stderr=stderr, cwd=ROOT)
                        capture['exit_code'] = result.returncode
                        result.check_returncode()
                        records = [json.loads(line) for line in report.read_text().splitlines()]
                        expected_days = [1 + 365 * year for year in range(args.years + 1)]
                        if [record['day'] for record in records] != expected_days:
                            raise RuntimeError('Capture checkpoint days differ from the requested protocol.')
                        capture['checkpoint_days'] = expected_days
                        capture['final_state_hash'] = records[-1]['state_hash']
                        # A successful report also requires its requested saved artifact.
                        digest(save)
                        capture['status'] = 'complete'
                    except (Exception, KeyboardInterrupt) as error:
                        capture['status'] = 'interrupted' if isinstance(error, KeyboardInterrupt) else 'failed'
                        capture['error'] = f'{type(error).__name__}: {error}'
                        raise
                    finally:
                        for label, artifact in [('report', report), ('save', save), ('stderr', errors)]:
                            if artifact.is_file():
                                capture[label + '_sha256'] = digest(artifact)
                        checkpoint()
                group['repeat_match'] = runs[0]['report_sha256'] == runs[1]['report_sha256']
                checkpoint()
                if not group['repeat_match']:
                    raise RuntimeError(f'{fixture} repeats produced different reports.')
        manifest['status'] = 'complete'
    except (Exception, KeyboardInterrupt) as error:
        manifest['status'] = 'interrupted' if isinstance(error, KeyboardInterrupt) else 'failed'
        manifest['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        checkpoint()
    print(output / 'manifest.json')

if __name__ == '__main__':
    main()
