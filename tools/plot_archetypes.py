#!/usr/bin/env python3
"""Archetype comparison charts from a Crownless sweep endpoint CSV."""
import argparse
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

GROUPS = {
    "Rich + strong dragon": lambda r: (
        r["dragon_slain"] == "0"
        and int(r["dragon_crown_strength"]) >= 70
        and int(r["average_prosperity"]) >= 35),
    "Poor + no dragon": lambda r: (
        r["dragon_slain"] == "1"
        and int(r["average_prosperity"]) < 30
        and int(r["average_hunger"]) > 50
        and int(r["average_legitimacy"]) < 30),
    "Dragon survived": lambda r: r["dragon_slain"] == "0",
    "Dragon slain": lambda r: r["dragon_slain"] == "1",
}
COLORS = ["#3aa76d", "#e05252", "#4c8bf5", "#d97742"]


def mean(rows, key):
    return sum(int(r[key]) for r in rows) / len(rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    parser.add_argument("--out", default="out/sweeps/charts-v4")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    with open(args.csv, newline="") as f:
        rows = list(csv.DictReader(f))
    groups = {name: [r for r in rows if pred(r)] for name, pred in GROUPS.items()}

    def bar(ax, key, title, fmt="%.0f"):
        names = [n for n in groups if groups[n]]
        values = [mean(groups[n], key) for n in names]
        ax.barh(names, values, color=COLORS[:len(names)])
        for i, v in enumerate(values):
            ax.text(v, i, " " + fmt % v, va="center", fontsize=9)
        ax.set_title(title)
        ax.invert_yaxis()

    # 1. Human economy and society
    fig, axes = plt.subplots(2, 3, figsize=(13, 7))
    bar(axes[0][0], "average_prosperity", "Prosperity")
    bar(axes[0][1], "average_hunger", "Hunger")
    bar(axes[0][2], "average_legitimacy", "Legitimacy")
    bar(axes[1][0], "total_kingdom_treasury", "Kingdom treasuries", "%.0f cr")
    bar(axes[1][1], "active_settlements", "Active settlements")
    bar(axes[1][2], "average_inequality", "Inequality")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "archetype-society.png"), dpi=120)
    plt.close(fig)

    # 2. Politics: war/peace exposure and campaigns
    fig, axes = plt.subplots(1, 3, figsize=(13, 4.2))
    bar(axes[0], "days_at_war", "Days at war", "%.0f d")
    bar(axes[1], "days_allied", "Days allied", "%.0f d")
    bar(axes[2], "dragon_campaign_attempts", "Campaign attempts", "%.2f")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "archetype-politics.png"), dpi=120)
    plt.close(fig)

    # 3. Dragon ecology
    fig, axes = plt.subplots(1, 3, figsize=(13, 4.2))
    bar(axes[0], "dragon_crown_strength", "Dragon crown strength")
    bar(axes[1], "dragon_hoard", "Dragon hoard", "%.0f cr")
    bar(axes[2], "dragon_retaliations", "Dragon retaliations", "%.0f")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "archetype-dragon.png"), dpi=120)
    plt.close(fig)

    # 4. Goblins and bandits
    fig, axes = plt.subplots(1, 3, figsize=(13, 4.2))
    bar(axes[0], "goblin_tributes", "Goblin tributes delivered", "%.0f")
    bar(axes[1], "goblin_cohesion_end", "Goblin cohesion (end)")
    bar(axes[2], "bandit_raids_end", "Bandit raids completed", "%.0f")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "archetype-threats.png"), dpi=120)
    plt.close(fig)

    # 5. Scriptorium proxy: monastery reserve and treasure
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.2))
    bar(axes[0], "iron_ledger_reserve", "Monastery reserve (end)", "%.0f cr")
    bar(axes[1], "treasure_count", "Treasures in world (end)", "%.1f")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "archetype-scriptorium.png"), dpi=120)
    plt.close(fig)

    print("wrote archetype charts to %s" % args.out)


if __name__ == "__main__":
    main()
