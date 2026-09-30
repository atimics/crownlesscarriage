"""Summarise a Pareto sweep of the raid stage: one row per prosperity weight, with the frontier marked."""
import json
from pathlib import Path
import sys


def main(folder):
    rows = []
    for path in sorted(Path(folder).glob('l4-*/result.json'), key=lambda p: float(p.parent.name.split('-', 1)[1])):
        r = json.loads(path.read_text())
        weight = float(r['stage'].split(':', 1)[1]) if ':' in r['stage'] else 1.0
        rows.append({'weight': weight, **{f'{side}_{k}': r[side][k] for side in ('rule', 'evolved') for k in r[side]}})
    if not rows:
        raise SystemExit('no results found')
    print(f"{'weight':>7} {'loot':>8} {'prosperity':>11} {'famine':>8}   (rule: loot {rows[0]['rule_loot']:.1f} prosperity {rows[0]['rule_prosperity']:.2f} famine {rows[0]['rule_famine']:.4f})")
    for r in rows:
        dominated = any(o is not r and o['evolved_loot'] >= r['evolved_loot'] and o['evolved_prosperity'] >= r['evolved_prosperity']
                        and (o['evolved_loot'] > r['evolved_loot'] or o['evolved_prosperity'] > r['evolved_prosperity']) for o in rows)
        print(f"{r['weight']:7g} {r['evolved_loot']:8.1f} {r['evolved_prosperity']:11.2f} {r['evolved_famine']:8.4f}   {'' if dominated else 'frontier'}")


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'results')
