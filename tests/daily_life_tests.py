"""The daily-life brain: zero weights are the rule, the harness reports what it changes."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'dialogue'))
try:
    import numpy as np
    import fastworld
    import evolve_life
    fastworld.library()
    READY = True
except (ImportError, FileNotFoundError, OSError):   # numpy or libcrowdsim not built here
    READY = False


def state(theta, seed, days=120):
    fastworld.set_policy(theta)
    try:
        world = fastworld.FastWorld.new(seed, 30)
        metrics = fastworld.run_days(world, days)
        digest = world.hash()
        world.close()
        return metrics, digest, fastworld.policy_stats()
    finally:
        fastworld.set_policy(None)


@unittest.skipUnless(READY, 'numpy or libcrowdsim not available')
class DailyLifeTests(unittest.TestCase):
    def test_zero_weights_reproduce_the_rule_exactly(self):
        for seed in (1, 2, 3):
            self.assertEqual(state(None, seed)[1], state(np.zeros(fastworld.policy_size()), seed)[1])

    def test_other_weights_change_the_world_and_are_counted(self):
        theta = np.random.default_rng(0).normal(0, 0.5, fastworld.policy_size())
        _, plain, _ = state(None, 1)
        _, changed, stats = state(theta, 1)
        self.assertNotEqual(plain, changed)
        self.assertGreater(stats['travel']['changed'], 0)
        self.assertEqual(stats['bandit']['offered'], 0)   # recruitment stays with the rule

    def test_the_policy_never_decides_whether_a_traveller_leaves(self):
        # Movement is the reward-hacking guard: a brain that stops travelling used to look
        # like a huge welfare gain, so the harness keeps the rule's decision to stay or go.
        theta = np.random.default_rng(5).normal(0, 2.0, fastworld.policy_size())
        rule = state(None, 2, 180)[0]['road_moves']
        moved = state(theta, 2, 180)[0]['road_moves']
        self.assertGreater(moved, 0.5 * rule)

    def test_welfare_counts_hunger_shelter_and_outlawry_against(self):
        base = {'road_days': 1000.0, 'road_hungry': 0.0, 'road_unsheltered': 0.0, 'road_bandit': 0.0,
                'road_stress': 0.0, 'road_coins': 0.0, 'road_moves': 0.0}
        worse = dict(base, road_hungry=200.0, road_unsheltered=100.0, road_bandit=50.0)
        self.assertLess(evolve_life.welfare(worse), evolve_life.welfare(base))
        self.assertAlmostEqual(evolve_life.welfare(worse), -(0.2 + 0.5 * 0.1 + 0.05))


if __name__ == '__main__':
    unittest.main()
