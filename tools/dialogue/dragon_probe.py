"""L6: what does it change which town the dragon burns? Inject a theft, force each town in turn."""
import argparse

import numpy as np

import fastworld as fw


def trial(seed, force=None, days=120, amount=300):
    fw.set_policy(np.zeros(fw.policy_size()), fw.DRAGON_TARGET)   # zero weights install the hook and follow the rule
    fw.set_dragon_force(force)
    world = fw.FastWorld.new(seed, 60)
    before = fw.run_days(world, 0)
    fw.inject_theft(world, amount)
    after = fw.run_days(world, days)
    world.close()
    fw.set_dragon_force(None)
    fw.set_policy(None)
    return {'burns': after['dragon_retaliations'] - before['dragon_retaliations'],
            'repaid': after['dragon_hoard'] - before['dragon_hoard'],
            'population_lost': before['population'] - after['population'],
            'prosperity_lost': before['town_prosperity_now'] - after['town_prosperity_now'],
            'legitimacy_lost': before['legitimacy_now'] - after['legitimacy_now']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worlds', type=int, default=100)
    args = parser.parse_args()
    seeds = range(101, 101 + args.worlds)
    keys = ('burns', 'repaid', 'population_lost', 'prosperity_lost', 'legitimacy_lost')
    for label, force in [('rule (richest)', None)] + [(f'town #{k}', k) for k in range(5)]:
        rows = [trial(s, force) for s in seeds]
        print(f'{label:16}', ' '.join(f'{k} {np.mean([r[k] for r in rows]):8.1f}' for k in keys))


if __name__ == '__main__':
    main()
