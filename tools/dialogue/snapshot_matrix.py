"""Cross-generation matrix for a co-evolution run: every raider snapshot against every town snapshot.

Rows are raider snapshots (0 is the rule), columns town snapshots (0 is the rule). Read the raider matrix down a
column: rising means later raiders take more from the same town. Read across a row: how a fixed raider fares as
towns adapt. A clean diagonal of gains is progress; bands that reverse are cycling; near-constant rows or columns
are disengagement (the opponent does not matter).
"""
import argparse
import json
from multiprocessing import get_context
from pathlib import Path

import numpy as np

import coevolve as co
import fastworld as fw


def cell(job):
    raider, town, seeds = job
    n = fw.policy_size()
    rows = []
    for seed in seeds:
        fw.clear_brains()
        fw.set_brain(co.RAIDER_BRAIN, raider, fw.RAID)
        fw.set_brain(co.TOWN_BRAIN, town, fw.TRADE | fw.KINGDOM_RELIEF)
        fw.set_kind_brains({**co.ROLES['raider']['kinds'], **co.ROLES['town']['kinds']})
        world = fw.FastWorld.new(seed, co.START)
        m = fw.run_days(world, co.DAYS)
        world.close()
        rows.append((co.score('raider', m), co.score('town', m)))
    fw.clear_brains()
    return tuple(float(x) for x in np.mean(rows, axis=0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True, help='a coevolve.py output directory')
    parser.add_argument('--worlds', type=int, default=100)
    parser.add_argument('--workers', type=int, default=10)
    args = parser.parse_args()
    raiders = np.load(args.run / 'raider_snapshots.npy')
    towns = np.load(args.run / 'town_snapshots.npy')
    seeds = list(range(301, 301 + args.worlds))
    jobs = [(r, t, seeds) for r in raiders for t in towns]
    with get_context('spawn').Pool(args.workers) as pool:
        out = pool.map(cell, jobs, chunksize=1)
    grid = np.array(out).reshape(len(raiders), len(towns), 2)
    for label, k in (('RAIDER score (goods taken / 100)', 0), ('TOWN score (prosperity - famine - hunger)', 1)):
        print(label)
        print('        ' + ''.join(f'town{j:<7}' for j in range(len(towns))))
        for i in range(len(raiders)):
            print(f'raider{i:<2}' + ''.join(f'{grid[i, j, k]:11.3f}' for j in range(len(towns))))
        print()
    (args.run / 'matrix.json').write_text(json.dumps({'raider': grid[:, :, 0].tolist(), 'town': grid[:, :, 1].tolist()}))


if __name__ == '__main__':
    main()
