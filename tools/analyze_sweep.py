#!/usr/bin/env python3
"""Cross-faction socioeconomic analysis of a Crownless sweep endpoint CSV."""
import argparse
import csv
import statistics as st
from collections import Counter

GROUPS = {
    "all": lambda r: True,
    "rich-strong-dragon": lambda r: (
        r["dragon_slain"] == "0"
        and int(r["dragon_crown_strength"]) >= 70
        and int(r["average_prosperity"]) >= 35),
    "poor-no-dragon": lambda r: (
        r["dragon_slain"] == "1"
        and int(r["average_prosperity"]) < 30
        and int(r["average_hunger"]) > 50
        and int(r["average_legitimacy"]) < 30),
    "dragon-survived": lambda r: r["dragon_slain"] == "0",
    "dragon-slain": lambda r: r["dragon_slain"] == "1",
}

METRICS = [
    # humans
    "total_population", "active_settlements", "average_prosperity",
    "average_security", "average_hunger", "average_legitimacy",
    "average_inequality", "total_kingdom_treasury", "iron_ledger_debt",
    "closed_routes", "years_all_routes_closed",
    # politics
    "days_at_war", "days_allied", "wars", "alliances",
    "dragon_campaign_attempts", "dragon_campaign_victories",
    "dragon_campaign_defeats", "years_hunger_40_plus",
    "years_hunger_60_plus",
    # dragon
    "dragon_crown_strength", "dragon_regional_influence",
    "dragon_hoard", "dragon_hunts", "dragon_broods",
    "dragon_whelps_dispersed", "dragon_retaliations",
    "dragon_crowned_days", "dragon_deep_wyrm_days",
    "dragon_afterdragon_days",
    # goblins
    "goblin_tributes", "goblin_members_end", "goblin_devotion_end",
    "goblin_cohesion_end", "goblin_hoard_defenses", "days_goblin_raid",
    "goblin_lair_food", "goblin_lair_weapons",
    # bandits
    "bandit_members_end", "bandit_supplies_end", "bandit_influence_end",
    "bandit_raids_end", "days_bandit_raid",
    "days_bandit_influence_70_plus", "smuggler_routes",
    # scriptorium / monastery
    "iron_ledger_reserve", "treasure_count",
    # scriptorium
    "archive_scribes", "lore_stored", "lore_lost_total",
    "archive_stewardship", "archive_last_recorded_day", "lore_ceiling",
    "archive_tool_wear", "archive_abbot_present",
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    args = parser.parse_args()
    with open(args.csv, newline="") as f:
        rows = list(csv.DictReader(f))

    for name, pred in GROUPS.items():
        group = [r for r in rows if pred(r)]
        if not group:
            print("%s: no worlds" % name)
            continue
        print("== %s (%d worlds) ==" % (name, len(group)))
        for key in METRICS:
            if key not in group[0]:
                continue
            x = [int(r[key]) for r in group]
            print("  %-34s mean=%9.2f median=%8d min=%6d max=%7d" % (
                key, st.mean(x), st.median(x), min(x), max(x)))
        print()


if __name__ == "__main__":
    main()
