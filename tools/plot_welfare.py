#!/usr/bin/env python3
"""Plot precise welfare and select reproducible examples of endpoint clusters."""
import argparse
import csv
import gzip
from collections import Counter
import json
from pathlib import Path
from statistics import mean, median


COLORS = {"Original survives": "#b66d0b", "Living successor": "#008d91", "Slain at end": "#ad5264"}


def outcome(row):
    if int(row["dragon_stage"]) == 6:
        return "Slain at end"
    if int(row["dragon_campaign_victories"]) > 0:
        return "Living successor"
    return "Original survives"


def load_rows(path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or any(row.get("metrics_version") != "2" for row in rows):
        raise ValueError("Use a fresh metrics version 2 endpoint sweep.")
    if len({r["year"] for r in rows}) != 1 or len({r["seed_number"] for r in rows}) != len(rows):
        raise ValueError("Expected one endpoint per seed at one common year.")
    return rows


def representatives(rows):
    selected = {}
    for label in COLORS:
        group = [r for r in rows if outcome(r) == label]
        if group:
            target = median(float(r["weighted_prosperity"]) for r in group)
            sample = min(group, key=lambda r: (abs(float(r["weighted_prosperity"]) - target), int(r["seed_number"])))
            selected[label] = int(sample["seed_number"])
    for field in ("average_prosperity", "average_security"):
        mode = Counter(r[field] for r in rows).most_common(1)[0][0]
        sample = min((r for r in rows if r[field] == mode), key=lambda r: int(r["seed_number"]))
        selected[f"Most common {field}: {mode}"] = int(sample["seed_number"])
    return selected


def plot_histories(directory, output, plt):
    files = sorted(directory.glob("seed-*-settlements.csv*"))
    if not files:
        raise ValueError("Expected seed-*-settlements.csv files in the trace directory.")
    fig, axes = plt.subplots(3, len(files), figsize=(4 * len(files), 10), squeeze=False)
    summary = ["| Seed | Town ID | Final population | Final prosperity | Prosperity range | Final security | Security range |",
               "| --- | --- | ---: | ---: | --- | ---: | --- |"]
    for column, path in enumerate(files):
        opener = gzip.open if path.suffix == ".gz" else open
        with opener(path, "rt", newline="") as stream:
            rows = list(csv.DictReader(stream))
        seed = rows[0]["seed_number"]
        ids = list(dict.fromkeys(r["settlement_id"] for r in rows))
        for town, identity in enumerate(ids):
            samples = [r for r in rows if r["settlement_id"] == identity]
            x = [int(r["elapsed_days"]) / 365 for r in samples]
            for index, field in enumerate(("population", "prosperity", "security")):
                axes[index, column].plot(x, [int(r[field]) for r in samples],
                                         linewidth=1, label=f"Town {town + 1}")
            ranges = [f"{min(int(r[f]) for r in samples)}–{max(int(r[f]) for r in samples)}" for f in ("prosperity", "security")]
            final = samples[-1]
            summary.append(f"| {seed} | {identity} | {final['population']} | {final['prosperity']} | {ranges[0]} | {final['security']} | {ranges[1]} |")
        axes[0, column].set_title(f"Seed {seed}", weight="bold")
        axes[2, column].set_xlabel("Elapsed years (365 days)")
        for index, field in enumerate(("Population", "Prosperity", "Security")):
            axes[index, column].set_ylabel(field)
            axes[index, column].grid(alpha=.2)
    axes[0, 0].legend(fontsize=8, ncol=2)
    fig.suptitle("Town histories in the selected window", fontsize=18)
    fig.tight_layout(rect=(0, .02, 1, .95))
    fig.savefig(output / "town-histories.png", dpi=140)
    plt.close(fig)
    (output / "trace-summary.md").write_text("\n".join(summary) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--traces", type=Path, help="Plot settlement histories from selected worlds")
    args = parser.parse_args()
    rows = load_rows(args.csv)
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "selected-seeds.json").write_text(json.dumps(representatives(rows), indent=2) + "\n")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11})
    fig, axes = plt.subplots(2, 2, figsize=(13, 10), facecolor="#f8fafc")
    panels = [
        ("average_prosperity", "inhabited_prosperity", "All-town prosperity (includes ruins)", "Inhabited-town prosperity", "Separate collapse from living-town welfare"),
        ("weighted_prosperity", "weighted_hunger", "Population-weighted prosperity", "Population-weighted hunger", "Poverty and hunger among living people"),
        ("total_population", "inhabited_security", "Population", "Inhabited-town security", "Population and security bands"),
        ("years_population_weighted_hunger_40_plus", "weighted_prosperity", "Annual checkpoints with weighted hunger ≥ 40", "Final population-weighted prosperity", "Hunger through the history")]
    for ax, (x, y, xlabel, ylabel, title) in zip(axes.flat, panels):
        ax.set_facecolor("#f8fafc")
        for label in reversed(COLORS):
            group = [r for r in rows if outcome(r) == label and float(r[x]) >= 0 and float(r[y]) >= 0]
            ax.scatter([float(r[x]) for r in group], [float(r[y]) for r in group],
                       s=16, alpha=.45, color=COLORS[label], linewidths=0)
        ax.set(xlabel=xlabel, ylabel=ylabel)
        ax.set_title(title, loc="left", fontsize=12, weight="bold", pad=12)
        ax.grid(alpha=.2)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
    fig.suptitle(f"Dragon outcomes and welfare: {len(rows):,} worlds × {rows[0]['year']} years", fontsize=20, x=.07, ha="left", y=.97)
    fig.legend(handles=[Line2D([], [], marker="o", linestyle="", color=color,
                    label=f"{label} ({sum(outcome(r) == label for r in rows)})") for label, color in COLORS.items()],
               loc="upper left", bbox_to_anchor=(.06, .93), ncol=3, frameon=False)
    fig.subplots_adjust(left=.08, right=.97, top=.84, bottom=.14, wspace=.26, hspace=.38)
    empty = sum(int(r["total_population"]) == 0 for r in rows)
    fig.text(.08, .065, f"One dot per world. Colors show final dragon state. Empty-population worlds: {empty} (welfare undefined).", fontsize=10)
    fig.text(.08, .035, "Scores span 0–100. Precise means preserve fractions. Source: endpoints.csv in the accompanying report.", fontsize=10)
    fig.savefig(args.out / "welfare.png", dpi=160)
    plt.close(fig)

    text = ["| Dragon outcome | Worlds | Ruins | Inhabited prosperity | Weighted prosperity | Weighted hunger | Hunger 40+ years |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for label in COLORS:
        group = [r for r in rows if outcome(r) == label]
        if not group:
            continue
        values = []
        for field in ("abandoned_settlements", "inhabited_prosperity", "weighted_prosperity", "weighted_hunger", "years_population_weighted_hunger_40_plus"):
            defined = [float(r[field]) for r in group if float(r[field]) >= 0]
            values.append(f"{mean(defined):.2f}" if defined else "undefined")
        text.append(f"| {label} | {len(group)} | " + " | ".join(values) + " |")
    text += ["", "Group welfare means use worlds with a living population.", "", "| Field | Most common value | Worlds |", "| --- | ---: | ---: |"]
    for field in ("average_prosperity", "inhabited_prosperity", "average_security", "inhabited_security", "average_legitimacy", "abandoned_settlements"):
        value, count = Counter(r[field] for r in rows).most_common(1)[0]
        text.append(f"| {field} | {value} | {count} |")
    (args.out / "summary.md").write_text("\n".join(text) + "\n")
    if args.traces:
        plot_histories(args.traces, args.out, plt)


if __name__ == "__main__":
    main()
