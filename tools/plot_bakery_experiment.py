#!/usr/bin/env python3
"""Run paired arrival experiments and draw charts from their daily records."""
import argparse
import concurrent.futures
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seeds", type=int, default=32)
    parser.add_argument("--workers", type=int, default=6)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    def run(seed):
        result = subprocess.run([str(args.binary.resolve()), str(seed)], capture_output=True, text=True)
        return seed, result

    rows, failures = [], []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        for seed, result in pool.map(run, range(1, args.seeds + 1)):
            if result.returncode:
                failures.append({"seed": seed, "error": result.stderr.strip(), "exit": result.returncode})
                (args.output / f"failed-{seed:03}.txt").write_text(result.stdout + result.stderr)
            else:
                rows.extend(csv.DictReader(io.StringIO(result.stdout)))
    if not rows:
        raise RuntimeError(f"Every experiment failed: {failures}")
    with gzip.open(args.output / "daily.csv.gz", "wt", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    seeds = sorted({int(row["seed"]) for row in rows})
    data = {(int(row["seed"]), row["arm"], int(row["day"])): row for row in rows}
    arms = ["control", "bread", "bakery"]
    colors = {"control": "#808a96", "bread": "#e29b37", "bakery": "#277d86"}
    labels = {"control": "Keep cargo", "bread": "Give 12 bread + wages", "bakery": "Give 12 wheat + wages"}
    def values(arm, metric):
        return np.array([[float(data[s, arm, d][metric]) for d in range(366)] for s in seeds])
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                         "axes.spines.top": False, "axes.spines.right": False})
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    for arm in arms:
        axes[0].plot(values(arm, "nutrition").mean(axis=0), color=colors[arm], label=labels[arm])
        axes[1].plot(values(arm, "hunger").mean(axis=0), color=colors[arm])
    axes[0].set(ylabel="Cumulative civilian nutrition units", xlabel="Days after arrival")
    axes[1].set(ylabel="Mean hunger (0–100; lower is better)", xlabel="Days after arrival", ylim=(-2, 102))
    axes[0].legend(frameon=False, fontsize=9)
    fig.suptitle(f"A single load at Gloamgate after 1,000 years · {len(seeds)} paired worlds")
    fig.savefig(args.output / "year-follow-up.png", dpi=160)
    plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    summary = {}
    for arm in arms[1:]:
        nutrition = values(arm, "nutrition") - values("control", "nutrition")
        hunger = values(arm, "hunger") - values("control", "hunger")
        for line in nutrition:
            axes[0].plot(line, color=colors[arm], alpha=.12, linewidth=.7)
        axes[0].plot(nutrition.mean(axis=0), color=colors[arm], label=labels[arm], linewidth=2.5)
        axes[1].scatter(nutrition[:, -1], hunger.mean(axis=1), color=colors[arm], label=labels[arm], alpha=.8, s=100 if arm == "bread" else 30, facecolors="none" if arm == "bread" else colors[arm])
        summary[arm] = {"mean_extra_nutrition": float(nutrition[:, -1].mean()),
                        "nutrition_range": [float(nutrition[:, -1].min()), float(nutrition[:, -1].max())],
                        "mean_hunger_change": float(hunger.mean()),
                        "worlds_more_nutrition": int((nutrition[:, -1] > 0).sum()),
                        "worlds_lower_mean_hunger": int((hunger.mean(axis=1) < 0).sum())}
    axes[0].axhline(0, color="#53616c", linewidth=.6)
    axes[0].set(xlabel="Days after arrival", ylabel="Extra nutrition eaten versus keep-cargo branch")
    axes[1].set(xlabel="Extra nutrition eaten by day 365", ylabel="Change in mean hunger over the year")
    axes[0].legend(frameon=False, fontsize=9)
    fig.suptitle("Paired effects · thin lines show each world; dots may overlap")
    fig.savefig(args.output / "paired-effects.png", dpi=160)
    plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    for arm in arms:
        axes[0].plot(values(arm, "wheat").mean(axis=0)[:29], label=labels[arm], color=colors[arm])
    axes[1].plot(values("bakery", "remembered").mean(axis=0) * 100, color=colors["bakery"])
    axes[0].set(xlabel="Days after arrival", ylabel="Mean wheat in town stock")
    axes[1].set(xlabel="Days after arrival", ylabel="Original contacts who remember help (%)", ylim=(-2, 102))
    axes[0].legend(frameon=False, fontsize=9)
    fig.suptitle("A short material pulse and a personal memory")
    fig.savefig(args.output / "stock-and-memory.png", dpi=160)
    plt.close(fig)
    manifest = {"requested_seeds": args.seeds, "completed_seeds": seeds, "failures": failures,
                "rows": len(rows), "seed_formula": "ordinal * 2654435761 modulo 2^32",
                "world_age_days": 365000, "follow_up_days": 365,
                "initial_rebuilds": sum(int(data[s, "bakery", 0]["initial_rebuild"]) for s in seeds),
                "memory_day365_percent": float(values("bakery", "remembered")[:, -1].mean() * 100),
                "effects": summary,
                "source_files_sha256": {name: hashlib.sha256(Path(name).read_bytes()).hexdigest()
                    for name in ["src/sim/cc_sim.c", "src/sim/cc_sim.h", "tools/bakery_experiment.c"]}}
    (args.output / "results.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
