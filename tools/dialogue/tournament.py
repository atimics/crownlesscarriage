"""Round-robin between roles: raider brains against town brains in the same worlds.

Each cell is one raider brain (raid launch and target) against one town brain (trade and kingdom relief)
over the same worlds. The two roles are scored separately, on their own outcomes: raiders by goods taken,
towns by hunger, famine and prosperity. No single rating is computed: the matrices are the result, and a
diagonal of steady gains, reversing bands (cycling) or flat rows (disengagement) can be read off them.
`rule` means the simulation's own rule; a .npy file is a weight vector for that role.
"""
import argparse
import json
from multiprocessing import get_context
from pathlib import Path

import numpy as np

import fastworld as fw

START, DAYS = 30, 730
RAIDER, TRADE, RELIEF = 1, 0, 2   # brain ids


def load(spec):
    return None if spec == 'rule' else np.load(spec)


def play(job):
    raider, trade, relief, seeds = job
    rows = []
    for seed in seeds:
        fw.clear_brains()
        n = fw.policy_size()
        fw.set_brain(RAIDER, raider if raider is not None else np.zeros(n), fw.RAID)
        fw.set_brain(TRADE, trade if trade is not None else np.zeros(n), fw.TRADE)
        fw.set_brain(RELIEF, relief if relief is not None else np.zeros(n), fw.KINGDOM_RELIEF)
        fw.set_kind_brains({'raid_target': RAIDER, 'raid_launch': RAIDER, 'trade': TRADE, 'kingdom_relief': RELIEF})
        world = fw.FastWorld.new(seed, START)
        m = fw.run_days(world, DAYS)
        world.close()
        days = max(1.0, m['town_days'])
        rows.append({'loot': m['loot'], 'raids': m['raids'], 'band_supplies': m['band_supplies'],
                     'hunger': m['town_hunger'] / days, 'famine': m['town_famine'] / days,
                     'prosperity': m['town_prosperity'] / days})
    fw.clear_brains()
    return {k: float(np.mean([r[k] for r in rows])) for k in rows[0]}, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--raiders', nargs='+', required=True, help="'rule' or .npy files")
    parser.add_argument('--trade', default='rule')
    parser.add_argument('--relief', default='rule')
    parser.add_argument('--town-rule-too', action='store_true', help='also play every raider against the plain rule town')
    parser.add_argument('--worlds', type=int, default=100)
    parser.add_argument('--first-seed', type=int, default=201)
    parser.add_argument('--workers', type=int, default=10)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    raiders = [(Path(s).parent.name if s != 'rule' else 'rule', load(s)) for s in args.raiders]
    towns = [('rule', None, None)]
    if args.trade != 'rule' or args.relief != 'rule':
        towns.append(('evolved', load(args.trade), load(args.relief)))
    seeds = list(range(args.first_seed, args.first_seed + args.worlds))
    jobs = [(r, t, k, seeds) for _, r in raiders for _, t, k in towns]
    with get_context('spawn').Pool(args.workers) as pool:
        results = pool.map(play, jobs, chunksize=1)
    cells, at = {}, 0
    for rn, _ in raiders:
        for tn, _, _ in towns:
            cells[(rn, tn)] = results[at][0]; at += 1
    print('RAIDER payoff: goods taken (rows = raider brain, columns = town brain)')
    print(f"{'':16}" + ''.join(f'{tn:>12}' for tn, _, _ in towns))
    for rn, _ in raiders:
        print(f'{rn:16}' + ''.join(f"{cells[(rn, tn)]['loot']:12.1f}" for tn, _, _ in towns))
    print('\nTOWN payoff: famine share of town-days / prosperity')
    print(f"{'':16}" + ''.join(f'{tn:>20}' for tn, _, _ in towns))
    for rn, _ in raiders:
        print(f'{rn:16}' + ''.join(f"{cells[(rn, tn)]['famine']:12.4f}/{cells[(rn, tn)]['prosperity']:6.2f}" for tn, _, _ in towns))
    if args.output:
        args.output.write_text(json.dumps({f'{r}|{t}': v for (r, t), v in cells.items()}, indent=2))


if __name__ == '__main__':
    main()
