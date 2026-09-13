#!/usr/bin/env python3
"""Run paired town interventions and plot their material and social effects."""
import argparse
from concurrent.futures import ThreadPoolExecutor
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

ARMS = ("wait", "fund", "supplies", "repair", "fund_repair")
LABELS = ("Wait", "Grain fund", "Bring wheat", "Repair road", "Fund + repair")
COLORS = ("#777777", "#2878b5", "#bb8b18", "#8b55a5", "#238467")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seeds", type=int, default=32)
    parser.add_argument("--workers", type=int, default=6)
    parser.add_argument("--reuse-data", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.seeds <= 1000 or args.workers < 1:
        parser.error("Choose 1 to 1000 seeds and at least one worker")
    root = Path(__file__).resolve().parents[1]
    sources = sorted(p for folder in ("src/sim", "src/quest") for p in (root / folder).glob("*") if p.is_file())
    sources += [root / "tools/contested_road_experiment.c", root / "CMakeLists.txt"]
    hashes = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    args.output.mkdir(parents=True, exist_ok=True)
    failures, rows = [], []
    if args.reuse_data:
        saved = json.loads((args.output / "results.json").read_text())
        if saved["source_sha256"] != hashes:
            raise SystemExit("Simulation sources changed. Run fresh experiments before redrawing.")
        failures, args.seeds = saved["failures"], saved["requested_seeds_per_age"]
        with gzip.open(args.output / "daily.csv.gz", "rt") as stream:
            rows = list(csv.DictReader(stream))
    else:
        def run(item):
            seed, age = item
            result = subprocess.run([str(args.binary.resolve()), str(seed), str(age)], capture_output=True, text=True)
            return seed, age, result
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            for seed, age, result in pool.map(run, ((s, a) for a in (0, 1000) for s in range(1, args.seeds + 1))):
                if result.returncode:
                    failures.append({"seed": seed, "age": age, "code": result.returncode, "reason": result.stderr.strip() or "Town or intervention unavailable"})
                    continue
                records = list(csv.DictReader(io.StringIO(result.stdout)))
                keys = {(r["arm"], int(r["day"])) for r in records}
                if len(records) != 905 or keys != {(a, d) for a in ARMS for d in range(181)}:
                    raise RuntimeError(f"Incomplete paired records: {seed}, {age}")
                rows.extend(records)
        if not rows:
            raise SystemExit(f"All experiments failed: {failures}")
        with gzip.open(args.output / "daily.csv.gz", "wt") as stream:
            writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)
    data = {(int(r["age"]), int(r["seed"]), r["arm"], int(r["day"])): r for r in rows}
    seeds = {age: sorted({int(r["seed"]) for r in rows if int(r["age"]) == age}) for age in (0, 1000)}
    def values(age, arm, field):
        return np.array([[float(data[age, seed, arm, day][field]) for day in range(181)] for seed in seeds[age]])
    summary = {"requested_seeds_per_age": args.seeds, "failures": failures, "rows": len(rows), "source_sha256": hashes, "ages": {}}
    fields = [field for field in rows[0] if field not in ("seed", "age", "arm", "day", "visitor_id")]
    for age in (0, 1000):
        if not seeds[age]:
            continue
        results = {"completed_seeds": seeds[age], "visitor_present_worlds": sum(data[age, seed, "wait", 0]["visitor_id"] != "0" for seed in seeds[age]), "arms": {}}
        control = values(age, "wait", "hunger").mean(axis=1)
        for arm in ARMS:
            change = values(age, arm, "hunger").mean(axis=1) - control
            results["arms"][arm] = {
                "visitor_ever_joined_worlds": int((values(age, arm, "visitor_bandit").max(axis=1) > 0).sum()),
                "mean_final": {field: float(values(age, arm, field)[:, -1].mean()) for field in fields},
                "mean_daily": {field: float(values(age, arm, field).mean()) for field in fields},
                "paired_hunger_change": {"mean": float(change.mean()), "min": float(change.min()), "max": float(change.max()), "improved_worlds": int((change < 0).sum())},
            }
        interaction = (values(age, "fund_repair", "hunger") - values(age, "fund", "hunger") - values(age, "repair", "hunger") + values(age, "wait", "hunger")).mean(axis=1)
        results["fund_repair_interaction"] = {"mean_hunger": float(interaction.mean()), "per_seed": dict(zip(map(str, seeds[age]), map(float, interaction)))}
        summary["ages"][str(age)] = results
    (args.output / "results.json").write_text(json.dumps(summary, indent=2) + "\n")
    plt.rcParams.update({"axes.spines.top": False, "axes.spines.right": False, "font.size": 10})
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), layout="constrained")
    for col, age in enumerate((0, 1000)):
        if not seeds[age]:
            continue
        for arm, label, color in zip(ARMS, LABELS, COLORS):
            axes[0, col].plot(values(age, arm, "hunger").mean(axis=0), label=label, color=color)
            axes[1, col].plot(values(age, arm, "nutrition").mean(axis=0), color=color)
        axes[0, col].set(title=f"Age {age:,} years · {len(seeds[age])} paired worlds", ylabel="Mean town hunger", ylim=(0, 100))
        axes[1, col].set(xlabel="Days after the shared decision day", ylabel="Cumulative town civilian nutrition")
    axes[0, 0].legend(frameon=False)
    fig.suptitle("One hungry town, one broken road, five choices")
    fig.savefig(args.output / "food-and-recovery.png", dpi=150)
    plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    for col, age in enumerate((0, 1000)):
        if not seeds[age]:
            continue
        control = values(age, "wait", "hunger").mean(axis=1)
        for arm, label, color in zip(ARMS[1:], LABELS[1:], COLORS[1:]):
            change = values(age, arm, "hunger").mean(axis=1) - control
            axes[col].scatter(values(age, arm, "delivered")[:, -1], change, label=label, color=color, alpha=.65)
        axes[col].axhline(0, color="#999999", linewidth=1)
        axes[col].set(title=f"Age {age:,} years", xlabel="Wheat delivered by organiser", ylabel="Mean hunger change versus waiting\nLower means better")
    axes[0].legend(frameon=False)
    fig.suptitle("Does more organised grain lead to better meals?")
    fig.savefig(args.output / "deliveries-and-hunger.png", dpi=150)
    plt.close(fig)
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), layout="constrained")
    for col, age in enumerate((0, 1000)):
        if not seeds[age]:
            continue
        for arm, label, color in zip(ARMS, LABELS, COLORS):
            axes[0, col].plot(values(age, arm, "road_closed").mean(axis=0), color=color, label=label)
            axes[1, col].plot(values(age, arm, "other_units").mean(axis=0), color=color)
        axes[0, col].set(title=f"Age {age:,} years", ylabel="Share of worlds with this road closed", ylim=(-.05, 1.05))
        axes[1, col].set(xlabel="Days after the shared decision day", ylabel="Other goods in active town shipments")
    axes[0, 0].legend(frameon=False)
    fig.suptitle("Road recovery and shared freight capacity")
    fig.savefig(args.output / "roads-and-cargo.png", dpi=150)
    plt.close(fig)
    fig, axes = plt.subplots(2, 3, figsize=(15, 8), layout="constrained")
    for row, age in enumerate((0, 1000)):
        if not seeds[age]:
            continue
        for col, field in enumerate(("court", "guild", "commons")):
            for arm, label, color in zip(ARMS, LABELS, COLORS):
                axes[row, col].plot(values(age, arm, field).mean(axis=0), color=color, label=label)
            axes[row, col].set(title=f"{field.title()} · age {age:,}", xlabel="Days", ylabel="Mean support", ylim=(0, 100))
    axes[0, 0].legend(frameon=False, fontsize=8)
    fig.suptitle("Faction support follows the existing hunger and trade rules")
    fig.savefig(args.output / "faction-support.png", dpi=150)
    plt.close(fig)
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), layout="constrained")
    for col, age in enumerate((0, 1000)):
        if not seeds[age]:
            continue
        for arm, label, color in zip(ARMS, LABELS, COLORS):
            axes[0, col].plot(values(age, arm, "visitor_bandit").mean(axis=0), label=label, color=color)
            axes[1, col].plot(values(age, arm, "helpers_remembered").mean(axis=0), color=color)
        axes[0, col].set(title=f"Age {age:,} years", ylabel="Share of all worlds with visitor in a camp", ylim=(-.05, 1.05))
        axes[1, col].set(xlabel="Days", ylabel="Present people remembering town aid", ylim=(-.05, 1.05))
    axes[0, 0].legend(frameon=False)
    fig.suptitle("Traveller allegiance and remembered help")
    fig.savefig(args.output / "people-and-memory.png", dpi=150)
    plt.close(fig)
    print(json.dumps({"rows": len(rows), "completed": {a: len(s) for a, s in seeds.items()}, "failures": failures}, indent=2))


if __name__ == "__main__":
    main()
