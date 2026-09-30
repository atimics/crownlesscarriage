"""L7: one shared brain, heritable per-person traits, generations of successions.

Every road-going person carries four inherited preferences (rich, cheap, near, home towns)
that shift the shared brain's travel scores. When a person dies the simulation seats a
successor in the same slot; the successor's traits are the parent's plus mutation. Three
conditions run on the same worlds: `none` (the brain alone), `drift` (traits inherited and
mutated with no selection) and `selection` (with probability `cultural` a successor copies
the traits of the best of three road-going people by their own hunger).
"""
import argparse
import json
from multiprocessing import get_context
from pathlib import Path

import numpy as np

import fastworld as fw

ROAD_ROLES = None   # filled from the simulation header on first use


def road_roles():
    global ROAD_ROLES
    if ROAD_ROLES is None:
        import re
        header = (Path(__file__).resolve().parents[2] / 'src' / 'sim' / 'cc_sim.h').read_text()
        body = re.search(r'\{([^{}]*)\}\s*CcCharacterRole\s*;', header).group(1)
        names = [re.sub(r'/\*.*?\*/|//.*', '', p, flags=re.S).split('=')[0].strip()
                 for p in body.split(',') if re.sub(r'/\*.*?\*/|//.*', '', p, flags=re.S).strip()]
        ROAD_ROLES = np.array([i for i, n in enumerate(names)
                               if n.removeprefix('CC_CHARACTER_') in ('SCOUT', 'TRAVELLER', 'REFUGEE', 'COURIER')])
    return ROAD_ROLES


def run(job):
    seed, brain, years, condition, cultural, mutation, init_sd, step, life = job
    rng = np.random.default_rng(seed)
    fw.set_policy(brain, fw.LEARNED)
    world = fw.FastWorld.new(seed, 30)
    ids, gens, roles, hungry = fw.slots(world)
    n = len(ids)
    if life:   # accelerated generations: short lives, successors seated as adults
        for i in range(n):
            fw.set_person(world, i, int(rng.uniform(*life) * 365), adult=False)
    road = np.isin(roles, road_roles())
    traits = np.zeros((n, fw.TRAITS)) if condition == 'none' else rng.normal(0, init_sd, (n, fw.TRAITS))
    lived = np.zeros(n)
    at_birth = hungry.copy()
    per_year, days_done, road_hungry, road_days, successions = [], 0, 0.0, 0.0, 0
    steps_per_year = 365 // step
    for k in range(1, years * steps_per_year + 1):
        fw.set_traits(None if condition == 'none' else traits)
        m = fw.run_days(world, step)
        road_hungry += m['road_hungry']; road_days += m['road_days']
        lived += step
        new_ids, new_gens, new_roles, hungry = fw.slots(world)
        changed = np.nonzero(new_ids != ids)[0]
        if len(changed):
            rates = (hungry - at_birth) / np.maximum(lived, 1.0)
            seasoned = np.nonzero(road & (lived >= 365) & ~np.isin(np.arange(n), changed))[0]
            for i in changed:
                parent = traits[i].copy()
                if condition == 'selection' and len(seasoned) >= 3 and rng.random() < cultural:
                    pool = rng.choice(seasoned, 3, replace=False)
                    parent = traits[pool[np.argmin(rates[pool])]].copy()
                if life:
                    fw.set_person(world, i, int(rng.uniform(*life) * 365), adult=True)
                traits[i] = np.clip(parent + rng.normal(0, mutation, fw.TRAITS), -3, 3) if condition != 'none' else 0.0
                lived[i] = 0.0
                at_birth[i] = hungry[i]
            successions += len(changed)
            ids = new_ids
        if k % steps_per_year == 0:
            year_rate = road_hungry / max(1.0, road_days)
            per_year.append({'year': k // steps_per_year, 'hungry': year_rate,
                             'trait_sd': float(traits[road].std(axis=0).mean()) if condition != 'none' else 0.0,
                             'trait_mean': traits[road].mean(axis=0).round(3).tolist() if condition != 'none' else [0] * fw.TRAITS,
                             'generation': float(new_gens[road].mean())})
            road_hungry = road_days = 0.0
    world.close()
    fw.set_policy(None)
    fw.set_traits(None)
    return {'seed': seed, 'condition': condition, 'years': per_year, 'successions': successions}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--brain', type=Path, required=True, help='.npy weights from evolve_life.py --stage l1')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--years', type=int, default=100)
    parser.add_argument('--seeds', type=int, default=8)
    parser.add_argument('--cultural', type=float, default=0.3)
    parser.add_argument('--mutation', type=float, default=0.15)
    parser.add_argument('--init-sd', type=float, default=0.5)
    parser.add_argument('--lifespan', type=float, nargs=2, metavar=('MIN', 'MAX'),
                        help='accelerate generations: lifespans in years, successors seated as adults')
    parser.add_argument('--workers', type=int, default=8)
    args = parser.parse_args()
    brain = np.load(args.brain)
    jobs = [(300 + s, brain, args.years, c, args.cultural, args.mutation, args.init_sd, 30, args.lifespan)
            for s in range(args.seeds) for c in ('none', 'drift', 'selection')]
    with get_context('spawn').Pool(args.workers) as pool:
        results = pool.map(run, jobs, chunksize=1)
    args.output.write_text(json.dumps(results))
    last = max(1, args.years // 5)
    for c in ('none', 'drift', 'selection'):
        rows = [r for r in results if r['condition'] == c]
        h = np.array([[y['hungry'] for y in r['years'][-last:]] for r in rows]).mean(axis=1)
        sd = np.array([[y['trait_sd'] for y in r['years'][-last:]] for r in rows]).mean(axis=1)
        first = np.array([[y['trait_sd'] for y in r['years'][:last]] for r in rows]).mean(axis=1)
        gen = np.mean([r['years'][-1]['generation'] for r in rows])
        print(f"{c:10} road hungry (last {last}y) {h.mean():.4f}   trait spread first {first.mean():.3f} -> last {sd.mean():.3f}   generation {gen:.1f}")
    base = np.array([[y['hungry'] for y in r['years'][-last:]] for r in results if r['condition'] == 'none']).mean(axis=1)
    for c in ('drift', 'selection'):
        other = np.array([[y['hungry'] for y in r['years'][-last:]] for r in results if r['condition'] == c]).mean(axis=1)
        d = other - base
        se = d.std(ddof=1) / np.sqrt(len(d))
        print(f"{c} vs no traits: hungry {d.mean():+.4f} +- {se:.4f} (z={d.mean() / se if se else 0:.1f}); lower in {(d < 0).sum()} of {len(d)}")


if __name__ == '__main__':
    main()
