"""Summarize completed before/after histories and draw the comparison."""
import argparse
import csv
import json
import os
from pathlib import Path


def rows(path):
    with path.open() as handle:
        return list(csv.DictReader(handle))


def summarize(root, seed, years):
    metrics = rows(root / f"seed-{seed}-metrics.csv")
    extra = rows(root / f"seed-{seed}-extra.csv")
    bands = rows(root / f"seed-{seed}-bands.csv")
    assert len(metrics) == len(extra) == years + 1
    assert int(extra[-1]["saturated_days"]) == 0
    counts = {int(r["kind_id"]): int(r["total"]) for r in rows(root / f"seed-{seed}-counts.csv")}
    m, e, b = metrics[-1], extra[-1], bands[-1]
    summary = {"seed": seed, "population": int(m["total_population"]),
        "inhabited_towns": int(m["active_settlements"]), "closed_roads": int(m["closed_routes"]),
        "human_cult": int(m["cult_human_members"]), "goblin_cult": int(m["cult_goblin_members"]),
        "goblins": int(m["goblin_members_end"]), "goblin_cohesion": int(m["goblin_cohesion_end"]),
        "goblin_raids": int(e["raids"]), "empty_goblin_raids": int(e["empty_raids"]),
        "bandit_name": b["name"], "bandits": int(b["members"]), "camp_size": int(b["camp_size"]),
        "bandit_raids": int(b["raids"]), "bandit_peak": max(int(r["members"]) for r in bands),
        "last_bandit_raid_year": max((int(bands[i]["year"]) for i in range(1, len(bands))
                                     if bands[i]["raids"] != bands[i - 1]["raids"]), default=0),
        "dragon_slain": counts.get(66, 0), "dragon_successors": counts.get(74, 0),
        "revival_rituals": int(e["seeds"]), "dragon_stage": int(m["dragon_stage"]),
        "dragon_retaliations": int(m["dragon_retaliations"]), "true_ages": int(e["true_ages"]),
        "meetings": int(e["meetings"]), "almanacs": int(e["editions"]),
        "empty_arrivals": int(e["empty_arrivals"]), "loaded_arrivals": int(e["loaded_arrivals"]),
        "cows": int(e["cows"]), "sheep": int(e["sheep"]), "common_ponies": int(e["common_ponies"]),
        "monster_pressure": int(m["monster_pressure"]), "war_days": int(m["days_at_war"]),
        "weighted_hunger": float(m["weighted_hunger"]), "town_tools": int(e["town_tools"])}
    return summary, metrics, extra, bands


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    metadata = {}
    data = {}
    for label, root in [("before", args.before), ("after", args.after)]:
        run = json.loads((root / "run.json").read_text())
        outcomes = json.loads((root / "outcomes.json").read_text())
        assert len(outcomes) == len(run["raw_world_seeds"]) and all(r["exit_code"] == 0 for r in outcomes)
        metadata[label] = {"run": run, "outcomes": outcomes}
        data[label] = {s: summarize(root, s, run["years_per_world"]) for s in run["raw_world_seeds"]}
    assert metadata["before"]["run"]["raw_world_seeds"] == metadata["after"]["run"]["raw_world_seeds"]
    assert metadata["before"]["run"]["years_per_world"] == metadata["after"]["run"]["years_per_world"]
    (args.output / "provenance.json").write_text(json.dumps(metadata, indent=2) + "\n")
    summaries = [{"version": label, **record[0]} for label in data for record in data[label].values()]
    (args.output / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    with (args.output / "summary.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(summaries[0]))
        writer.writeheader()
        writer.writerows(summaries)

    os.environ.setdefault("MPLCONFIGDIR", "/private/tmp/crownless-recovery-matplotlib")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch
    seeds = list(data["after"])
    colors = ["#167d8d", "#b14d76", "#b57a20", "#526bb0", "#56864b", "#8259a8"]
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
                         "axes.spines.top": False, "axes.spines.right": False})
    fig, axes = plt.subplots(2, 2, figsize=(13, 9))
    for seed, color in zip(seeds, colors):
        _, m, _, b = data["after"][seed]
        axes[0, 0].plot([int(r["year"]) for r in m[:301]],
                        [int(r["cult_human_members"]) for r in m[:301]], color=color, label=str(seed))
        axes[0, 1].plot([int(r["year"]) for r in b[:301]],
                        [int(r["members"]) for r in b[:301]], color=color)
    axes[0, 0].set(title="The human cult survives the founding town", ylabel="Human members", xlabel="Elapsed solar years")
    axes[0, 1].set(title="Bandit groups lose strength", ylabel="Members", xlabel="Elapsed solar years")
    for index, seed in enumerate(seeds):
        before, after = data["before"][seed][0], data["after"][seed][0]
        for offset, row, color in [(-0.18, before, "#9da8b5"), (0.18, after, "#167d8d")]:
            percent = 100 * row["empty_goblin_raids"] / max(1, row["goblin_raids"])
            axes[1, 0].barh(index + offset, percent, height=.32, color=color)
        axes[1, 0].text(.5, index + .18, f'{after["empty_goblin_raids"]} empty trips', fontsize=8, va="center")
    axes[1, 0].set(title="Scouts sharply reduce empty goblin raids", xlabel="Empty raids (%)",
                    yticks=range(len(seeds)), yticklabels=[str(s) for s in seeds], ylabel="World seed")
    axes[1, 0].legend(handles=[Patch(color="#9da8b5", label="Before"), Patch(color="#167d8d", label="After")], frameon=False)
    stage_colors = ["#e8cf9d", "#e29c52", "#c76b46", "#7a944d", "#187c89", "#92556b", "#cad0d8"]
    stage_names = ["Egg", "Whelp", "Wanderer", "Crowned", "Deep wyrm", "Uncrowned", "Afterdragon"]
    from matplotlib.colors import ListedColormap, BoundaryNorm
    stages = [[int(r["dragon_stage"]) for r in data["after"][seed][1]] for seed in seeds]
    axes[1, 1].imshow(stages, aspect="auto", interpolation="nearest", origin="lower",
        extent=(0, len(stages[0]) - 1, -.5, len(seeds) - .5),
        cmap=ListedColormap(stage_colors), norm=BoundaryNorm([i - .5 for i in range(8)], 7))
    axes[1, 1].set(title="Long reigns and repeated successions", xlabel="Elapsed solar years",
                    yticks=range(len(seeds)), yticklabels=[str(s) for s in seeds], ylabel="World seed")
    axes[1, 1].legend(handles=[Patch(color=c, label=n) for c, n in zip(stage_colors, stage_names)],
                        fontsize=8, loc="upper center", bbox_to_anchor=(.5, -.18), ncol=3, frameon=False)
    for ax in axes.flat:
        if ax is not axes[1, 1]: ax.grid(alpha=.15)
    fig.suptitle("Six Crownless worlds · 3,000 years before and after", fontsize=18, y=.985)
    fig.legend(*axes[0, 0].get_legend_handles_labels(), title="World seed", loc="upper center",
               bbox_to_anchor=(.5, .955), ncol=6, frameon=False)
    fig.tight_layout(rect=(0, .05, 1, .9), h_pad=2.8, w_pad=2.2)
    fig.savefig(args.output / "comparison.png", dpi=160, facecolor="white")
    fig.savefig(args.output / "comparison.svg", facecolor="white")
    for label in data:
        r = [v[0] for v in data[label].values()]
        print(label, {k: sum(x[k] for x in r) for k in ["goblin_raids", "empty_goblin_raids", "dragon_successors", "empty_arrivals", "meetings", "almanacs"]})


if __name__ == "__main__":
    main()
