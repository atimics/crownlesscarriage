#!/usr/bin/env python3
"""Compare the same arriving traveller under old and casting that uses local people."""
import argparse
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
    parser.add_argument("--legacy-binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seeds", type=int, default=32)
    args = parser.parse_args()
    if not 1 <= args.seeds <= 1000:
        parser.error("Choose 1 to 1000 seeds")
    rows = []
    for seed in range(1, args.seeds + 1):
        def run(binary, schema):
            result = subprocess.run([str(binary.resolve()), str(seed), str(schema)], capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"Seed {seed}, schema {schema}: {result.stderr}")
            records = list(csv.DictReader(io.StringIO(result.stdout)))
            if len(records) != 181 or [int(r["day"]) for r in records] != list(range(181)):
                raise RuntimeError("Incomplete daily records")
            return records
        old = run(args.legacy_binary, 73)
        replay = run(args.binary, 73)
        if old != replay:
            raise RuntimeError(f"Legacy daily hash or state differs at seed {seed}")
        rows.extend(old)
        rows.extend(run(args.binary, 74))
    args.output.mkdir(parents=True, exist_ok=True)
    with gzip.open(args.output / "daily.csv.gz", "wt") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    data = {(int(r["schema"]), int(r["seed"]), int(r["day"])): r for r in rows}
    def values(schema, field):
        return np.array([[float(data[schema, seed, day][field]) for day in range(181)] for seed in range(1, args.seeds + 1)])
    root = Path(__file__).resolve().parents[1]
    sources = [p for folder in ("src/sim", "src/quest") for p in (root / folder).glob("*") if p.is_file()]
    sources += [root / "tools/quest_cast_experiment.c"]
    summary = {"paired_worlds": args.seeds, "rows": len(rows), "legacy_daily_hashes_verified": args.seeds * 181,
        "binary_sha256": {"current": hashlib.sha256(args.binary.read_bytes()).hexdigest(), "legacy": hashlib.sha256(args.legacy_binary.read_bytes()).hexdigest()},
        "source_sha256": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sources)}, "arms": {}}
    for schema in (73, 74):
        summary["arms"][str(schema)] = {field: float(values(schema, field)[:, -1].mean()) for field in
            ("visitor_here", "visitor_bandit", "new_quests", "person_moves", "role_changes", "allegiance_changes", "hunger")}
    (args.output / "results.json").write_text(json.dumps(summary, indent=2) + "\n")
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), layout="constrained")
    for schema, label, color in ((73, "Previous casting", "#a25847"), (74, "New casting", "#247b84")):
        for ax, field, title in zip(axes.flat,
            ("visitor_here", "role_changes", "person_moves", "new_quests"),
            ("Original visitor still in arrival town", "Changes of occupation", "Changes of town", "New commissions")):
            ax.plot(values(schema, field).mean(axis=0), label=label, color=color)
            ax.set(title=title, xlabel="Days after arrival", ylabel="Mean cumulative count" if field != "visitor_here" else "Share of worlds")
    axes[0, 0].set_ylim(-.05, 1.05)
    axes[0, 0].legend(frameon=False)
    fig.suptitle(f"Quest casting and continuing lives · {args.seeds} paired worlds")
    fig.savefig(args.output / "cast-continuity.png", dpi=150)
    plt.close(fig)
    print(json.dumps({k: v for k, v in summary.items() if k not in ("source_sha256", "binary_sha256")}, indent=2))


if __name__ == "__main__":
    main()
