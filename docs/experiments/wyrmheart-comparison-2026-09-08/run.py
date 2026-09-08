#!/usr/bin/env python3
"""Compare eight matched 16,000-year histories before and after the heart fix."""
import concurrent.futures
import csv
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import time
import sys

ROOT = Path(__file__).resolve().parent
BINS = {'before': '/private/tmp/crownless-wyrmheart-control-build/crownless_sim_metrics',
        'after': '/private/tmp/crownless-wyrmheart-build/crownless_sim_metrics'}
SEEDS = list(range(1, 9))
YEARS = 16000

def run(case):
    label, seed = case
    years = 128000 if len(sys.argv)>1 and sys.argv[1]=="--long-seed2" else YEARS
    command = [BINS[label], '--seed', str(seed), '--years', str(years), '--campaign-metrics']
    start = time.monotonic()
    tag = f'{label}-{seed:03}' + ('-128000' if years == 128000 else '')
    filename = f'{tag}.csv.gz'
    with (ROOT/f'{tag}.log').open('w') as err:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=err, text=True)
        reader = csv.DictReader(process.stdout)
        last = None
        rows = 0
        with gzip.open(ROOT/filename, 'wt') as dest:
            writer = csv.DictWriter(dest, fieldnames=reader.fieldnames)
            writer.writeheader()
            for row in reader:
                writer.writerow(row)
                last = row
                rows += 1
        code = process.wait()
    result = dict(label=label, seed=seed, command=command, returncode=code, rows=rows,
                  seconds=time.monotonic()-start, endpoint=last, file=filename,
                  sha256=hashlib.sha256((ROOT/filename).read_bytes()).hexdigest())
    print(f'{label} seed {seed}: {rows} years, exit {code}', flush=True)
    return result

if __name__ == '__main__':
    start = time.monotonic()
    long_run = len(sys.argv)>1 and sys.argv[1]=="--long-seed2"
    seeds = [2] if long_run else SEEDS
    years = 128000 if long_run else YEARS
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(run, [(label, seed) for seed in seeds for label in BINS]))
    manifest = dict(before='5f13096', after='7bccd65', seeds=seeds, years=years,
                    seconds=time.monotonic()-start, runs=results,
                    binaries={k:dict(path=v,sha256=hashlib.sha256(Path(v).read_bytes()).hexdigest()) for k,v in BINS.items()})
    (ROOT/('long-manifest.json' if long_run else 'manifest.json')).write_text(json.dumps(manifest,indent=2)+'\n')
    assert all(r['returncode']==0 and r['rows']==years for r in results)
