#!/usr/bin/env python3
"""Charts from a Crownless simulation sweep endpoint CSV."""
import argparse
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

STAGE = {
    0: "Egg", 1: "Whelp", 2: "Wanderer", 3: "Crowned",
    4: "Deep Wyrm", 5: "Uncrowned", 6: "Afterdragon",
}


def load(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def ints(rows, name):
    return [int(r[name]) for r in rows]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", help="endpoint CSV from sim_sweep.py")
    parser.add_argument("--out", default="out/sweeps")
    args = parser.parse_args()
    rows = load(args.csv)
    os.makedirs(args.out, exist_ok=True)

    slain = ints(rows, "dragon_slain")
    stages = ints(rows, "dragon_stage")
    active = ints(rows, "active_settlements")
    abandoned = ints(rows, "abandoned_settlements")
    pop = ints(rows, "total_population")
    legit = ints(rows, "average_legitimacy")
    prosper = ints(rows, "average_prosperity")
    security = ints(rows, "average_security")
    hunger = ints(rows, "average_hunger")
    wars = ints(rows, "wars")
    alliances = ints(rows, "alliances")
    closed = ints(rows, "closed_routes")
    crown = ints(rows, "dragon_crown_strength")
    min_active = ints(rows, "minimum_active_settlements")
    max_closed = ints(rows, "maximum_closed_routes")
    years_all_closed = ints(rows, "years_all_routes_closed")
    years_abandoned = ints(rows, "years_with_abandoned_settlement")
    route_closures = ints(rows, "route_closures")

    # 1. Dragon outcomes and life stage
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.6))
    n_slain = sum(slain)
    n_alive = len(rows) - n_slain
    axes[0].pie([n_alive, n_slain], labels=["Survived", "Slain"],
                autopct="%.1f%%", startangle=90,
                colors=["#4c8bf5", "#e05252"], explode=(0, 0.05))
    axes[0].set_title("Dragon at year 1000")
    from collections import Counter
    counts = Counter(stages)
    names = [STAGE[s] for s in sorted(counts)]
    vals = [counts[s] for s in sorted(counts)]
    axes[1].barh(names, vals, color="#7a6ff0")
    axes[1].set_xlabel("worlds")
    axes[1].set_title("Dragon life stage at year 1000")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "dragon.png"), dpi=120)
    plt.close(fig)

    # 2. Settlement survival
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))
    axes[0].hist(active, bins=range(1, 8), align="left", color="#3aa76d",
                 edgecolor="white")
    axes[0].set_title("Active settlements at year 1000")
    axes[0].set_xlabel("active settlements")
    axes[0].set_ylabel("worlds")
    axes[1].hist(abandoned, bins=range(0, 8), align="left", color="#d97742",
                 edgecolor="white")
    axes[1].set_title("Abandoned settlements at year 1000")
    axes[1].set_xlabel("abandoned settlements")
    axes[1].set_ylabel("worlds")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "settlements.png"), dpi=120)
    plt.close(fig)

    # 3. Society histograms
    fig, axes = plt.subplots(2, 3, figsize=(12, 7))
    panels = [
        (axes[0][0], pop, "Population", "#4c8bf5"),
        (axes[0][1], legit, "Legitimacy", "#7a6ff0"),
        (axes[0][2], prosper, "Prosperity", "#3aa76d"),
        (axes[1][0], security, "Security", "#2aa7c9"),
        (axes[1][1], hunger, "Hunger", "#e05252"),
        (axes[1][2], closed, "Closed routes", "#d97742"),
    ]
    for ax, data, title, color in panels:
        ax.hist(data, bins=24, color=color, edgecolor="white")
        ax.set_title(title)
        ax.set_ylabel("worlds")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "society.png"), dpi=120)
    plt.close(fig)

    # 4. Diplomacy
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))
    axes[0].hist(wars, bins=range(0, 4), align="left", color="#e05252",
                 edgecolor="white")
    axes[0].set_title("Wars at year 1000")
    axes[0].set_xlabel("wars")
    axes[0].set_ylabel("worlds")
    axes[1].hist(alliances, bins=range(0, 5), align="left", color="#4c8bf5",
                 edgecolor="white")
    axes[1].set_title("Alliances at year 1000")
    axes[1].set_xlabel("alliances")
    axes[1].set_ylabel("worlds")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "diplomacy.png"), dpi=120)
    plt.close(fig)

    # 5. Trajectory summaries
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))
    axes[0].hist(years_all_closed, bins=24, color="#d97742",
                 edgecolor="white")
    axes[0].set_title("Years with every route closed")
    axes[0].set_xlabel("years out of 1000")
    axes[0].set_ylabel("worlds")
    axes[1].scatter(route_closures, min_active, s=18, alpha=0.55,
                    color="#7a6ff0", edgecolors="none")
    axes[1].set_title("Route closure churn vs settlement floor")
    axes[1].set_xlabel("route closure transitions")
    axes[1].set_ylabel("minimum active settlements")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "trajectory.png"), dpi=120)
    plt.close(fig)

    # 6. Scatter: population vs legitimacy, colored by dragon fate
    fig, ax = plt.subplots(figsize=(7.5, 5.4))
    colors = ["#4c8bf5" if s == 0 else "#e05252" for s in slain]
    ax.scatter(pop, legit, c=colors, s=18, alpha=0.55, edgecolors="none")
    ax.set_xlabel("population")
    ax.set_ylabel("legitimacy")
    ax.set_title("Population vs legitimacy (blue = dragon survived, red = slain)")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "pop_legit.png"), dpi=120)
    plt.close(fig)

    # 7. Scatter: prosperity vs hunger, colored by dragon crown strength
    fig, ax = plt.subplots(figsize=(7.5, 5.4))
    sc = ax.scatter(prosper, hunger, c=crown, s=18, alpha=0.6,
                    cmap="viridis", edgecolors="none")
    ax.set_xlabel("prosperity")
    ax.set_ylabel("hunger")
    ax.set_title("Prosperity vs hunger (color = dragon crown strength)")
    fig.colorbar(sc, ax=ax, label="crown strength")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "prosper_hunger.png"), dpi=120)
    plt.close(fig)

    print("wrote charts to %s" % args.out)


if __name__ == "__main__":
    main()