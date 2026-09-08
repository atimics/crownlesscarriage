#!/usr/bin/env python3
"""Run independent Crownless metric seeds in parallel without aborting the sweep."""
import argparse
import csv
import io
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from statistics import mean, median

SEED_MULTIPLIER = 0x9E3779B9


def run_seed(binary, seed, years):
    result = subprocess.run(
        [binary, "--seed", str(seed), "--years", str(years), "--final-only"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return seed, None, result.stderr.strip() or "metrics runner failed"
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    if len(rows) != 1 or int(rows[0]["year"]) != years:
        return seed, None, "expected final row for year %d, got %d rows" % (years, len(rows))
    return seed, rows[0], None


def describe(rows, name):
    values = sorted(int(row[name]) for row in rows)
    return "%s mean=%.2f median=%d min=%d max=%d" % (
        name, mean(values), int(median(values)), values[0], values[-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="out/build/play/crownless_sim_metrics")
    parser.add_argument("--first-seed", type=int, default=1)
    parser.add_argument("--seeds", type=int, default=100)
    parser.add_argument("--years", type=int, default=1000)
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--output", help="write successful endpoint rows as CSV")
    args = parser.parse_args()
    if args.first_seed < 1 or args.seeds < 1 or args.years < 1 or args.jobs < 1:
        parser.error("seed, count, years, and jobs must be positive")

    results = []
    failures = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {
            pool.submit(run_seed, args.binary, seed, args.years): seed
            for seed in range(args.first_seed, args.first_seed + args.seeds)
        }
        for future in as_completed(futures):
            seed, row, error = future.result()
            if error is not None:
                failures.append((seed, error))
            else:
                results.append(row)

    results.sort(key=lambda row: int(row["seed_number"]))
    print("== Crownless simulation sweep ==")
    print("requested seeds=%d..%d years=%d jobs=%d" % (
        args.first_seed, args.first_seed + args.seeds - 1, args.years, args.jobs))
    print("passed=%d failed=%d checkpoints=%d" % (
        len(results), len(failures), len(results) * args.years))
    print("seed multiplier=0x%08X" % SEED_MULTIPLIER)
    if results:
        for name in [
            "total_population", "active_settlements", "average_prosperity",
            "average_security", "average_hunger", "average_legitimacy",
            "closed_routes", "wars", "alliances", "dragon_slain",
        ]:
            print(describe(results, name))
    if failures:
        print("-- failures --")
        for seed, error in sorted(failures):
            print("seed=%d world_seed=%d: %s" % (
                seed, (seed * SEED_MULTIPLIER) & 0xFFFFFFFF, error))

    if args.output and results:
        with open(args.output, "w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=results[0].keys())
            writer.writeheader()
            writer.writerows(results)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
