#!/usr/bin/env python3
"""Capture annual validation and sampled history for an Age of Dragons study."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import time


def validate_rows(rows, seed, years):
    """Reject truncated, reordered, or misidentified output before aggregation."""
    if len(rows) != years:
        raise ValueError(f"expected {years} annual rows, received {len(rows)}")
    for year, row in enumerate(rows, 1):
        if (int(row['seed_number']) != seed or int(row['year']) != year or
                int(row['world_seed']) != (seed * 0x9E3779B9) & 0xFFFFFFFF or
                int(row['day']) != 1 + year * 365):
            raise ValueError(f"unexpected seed, world seed, year, or day at year {year}")
        if None in row or any(value is None for value in row.values()):
            raise ValueError(f"malformed CSV at year {year}")


def run_seed(binary, seed, years):
    start = time.monotonic()
    command = [str(binary), '--seed', str(seed), '--years', str(years), '--campaign-metrics']
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    error = result.stderr.strip()
    try:
        validate_rows(rows, seed, years)
    except (ValueError, KeyError, TypeError) as exc:
        error = '\n'.join(filter(None, [error, str(exc)]))
    passed = result.returncode == 0 and not error
    return dict(seed=seed, passed=passed, returncode=result.returncode,
                annual_rows=len(rows), seconds=round(time.monotonic() - start, 3),
                error=error, endpoint=rows[-1] if passed else None,
                samples=[r for r in rows if int(r['year']) <= 10 or
                         int(r['year']) % 25 == 0 or int(r['year']) == years])


def write_csv(path, rows):
    if not rows:
        return
    opener = gzip.open if path.suffix == '.gz' else open
    with opener(path, 'wt', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--source', required=True, help='full simulation source commit')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seeds', type=int, default=1000)
    parser.add_argument('--years', type=int, default=1000)
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    if min(args.seeds, args.years, args.jobs) < 1:
        parser.error('counts must be positive')
    args.binary = args.binary.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    fingerprint = dict(source=args.source,
                       binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                       seeds=args.seeds, years=args.years)
    checkpoint = args.output / '.checkpoint'
    checkpoint.mkdir(exist_ok=True)
    identity = checkpoint / 'identity.json'
    if identity.exists() and json.loads(identity.read_text()) != fingerprint:
        parser.error('checkpoint belongs to a different source, binary, or sweep')
    identity.write_text(json.dumps(fingerprint, indent=2) + '\n')
    start = time.monotonic()
    results = []
    pending = []
    for seed in range(1, args.seeds + 1):
        path = checkpoint / f'{seed}.json'
        if path.exists():
            results.append(json.loads(path.read_text()))
        else:
            pending.append(seed)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(run_seed, args.binary, s, args.years): s for s in pending}
        for future in as_completed(futures):
            result = future.result()
            path = checkpoint / f"{result['seed']}.json"
            temporary = path.with_suffix('.tmp')
            temporary.write_text(json.dumps(result))
            temporary.replace(path)
            results.append(result)
            if len(results) % 50 == 0 or not result['passed']:
                print(f"{args.output.name}: {len(results)}/{args.seeds}; "
                      f"failed={sum(not r['passed'] for r in results)}", flush=True)
    results.sort(key=lambda r: r['seed'])
    write_csv(args.output / 'endpoints.csv.gz', [r['endpoint'] for r in results if r['passed']])
    write_csv(args.output / 'history.csv.gz', [row for r in results if r['passed'] for row in r['samples']])
    # Failed runs keep their last valid samples in a separate artifact.
    write_csv(args.output / 'partial-history.csv.gz', [row for r in results if not r['passed'] for row in r['samples']])
    manifest = dict(**fingerprint, binary=str(args.binary), jobs=args.jobs,
                    seed_mapping='(ordinal * 0x9E3779B9) & 0xFFFFFFFF',
                    command=[str(args.binary), '--seed', 'ORDINAL', '--years', str(args.years), '--campaign-metrics'],
                    sampling='Years 1 through 10, every 25 years, and final year. Validation every year.',
                    invocation_seconds=round(time.monotonic() - start, 3),
                    passed=sum(r['passed'] for r in results),
                    runs=[{k: v for k, v in r.items() if k not in ('endpoint', 'samples')} for r in results])
    manifest['artifact_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in args.output.glob('*.csv.gz')}
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return int(any(not r['passed'] for r in results))


if __name__ == '__main__':
    raise SystemExit(main())
