#!/usr/bin/env python3
"""Collect independent world histories and paired road-steward trials."""
import argparse
import concurrent.futures as cf
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    parser.add_argument("--seeds", type=int, default=128)
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    tasks = [("world", s, 1000) for s in range(1, args.seeds + 1)]
    tasks += [("agent", s, y) for y in (10, 100) for s in range(1, args.seeds + 1)]

    def run(task):
        kind, seed, years = task
        name = "crownless_sim_metrics" if kind == "world" else "crownless_agent_sweep"
        command = [str(args.build / name), "--seed", str(seed), "--years", str(years)]
        start = time.monotonic()
        result = subprocess.run(command, capture_output=True, text=True)
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        expected = years if kind == "world" else 1
        passed = result.returncode == 0 and len(rows) == expected
        return task, rows, dict(command=command, seconds=time.monotonic()-start,
                               returncode=result.returncode, passed=passed,
                               rows=len(rows), stderr=result.stderr)

    start = time.monotonic()
    records, worlds, agents = [], {}, {10: {}, 100: {}}
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for task, rows, record in pool.map(run, tasks):
            kind, seed, years = task
            records.append(record)
            if record["passed"]:
                (worlds if kind == "world" else agents[years])[seed] = rows
            else:
                (root / f"partial-{kind}-{seed}-{years}.json").write_text(json.dumps(rows))
            if len(records) % 32 == 0:
                print(f"Finished {len(records)}/{len(tasks)} runs", flush=True)

    def write(path, rows, compressed=False):
        opener = gzip.open if compressed else open
        with opener(path, "wt", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)

    if worlds:
        write(root / "annual.csv.gz", [r for s in sorted(worlds) for r in worlds[s]], True)
        write(root / "endpoints.csv", [worlds[s][-1] for s in sorted(worlds)])
    for years, cohort in agents.items():
        if cohort:
            write(root / f"agent-{years}.csv", [cohort[s][0] for s in sorted(cohort)])
    manifest = dict(source=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                    seeds=args.seeds, jobs=args.jobs, seconds=time.monotonic()-start,
                    runs=records, hashes={})
    for path in sorted(root.glob("*.csv*")):
        manifest["hashes"][path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    for name in ("crownless_sim_metrics", "crownless_agent_sweep"):
        manifest["hashes"][name] = hashlib.sha256((args.build / name).read_bytes()).hexdigest()
    (root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Passed {sum(r['passed'] for r in records)}/{len(records)}; {manifest['seconds']:.1f}s")


if __name__ == "__main__":
    main()
