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

    def test_gossip_rates_are_coverage_times_accuracy(self):
        m = {'gossip_pairs': 60.0, 'story_days': 20.0, 'gossip_confidence': 4200.0, 'gossip_retellings': 120.0}
        r = evolve_life.gossip_rates(m)
        self.assertAlmostEqual(r['coverage'], 60.0 / (20.0 * evolve_life.SETTLEMENTS))
        self.assertAlmostEqual(r['accuracy'], 0.7)
        self.assertAlmostEqual(r['informed'], r['coverage'] * r['accuracy'])

    def test_town_and_raid_scores_move_the_right_way(self):
        calm = {'town_days': 100.0, 'town_hunger': 500.0, 'town_famine': 5.0, 'town_prosperity': 6600.0,
                'raids': 10.0, 'loot': 100.0}
        famine = dict(calm, town_hunger=2500.0, town_famine=50.0)
        self.assertLess(evolve_life.town_welfare(famine), evolve_life.town_welfare(calm))
        richer = dict(calm, loot=150.0)
        self.assertGreater(evolve_life.raid_welfare(richer), evolve_life.raid_welfare(calm))

    def test_every_stage_names_its_decisions_and_scores(self):
        for name, cfg in evolve_life.STAGES.items():
            self.assertTrue(cfg['mask'] > 0 and cfg['days'] > 0 and cfg['keys'], name)

    def test_an_injected_theft_makes_the_dragon_retaliate_and_be_repaid(self):
        world = fastworld.FastWorld.new(1, 60)
        before = fastworld.run_days(world, 0)
        fastworld.inject_theft(world, 300)
        after = fastworld.run_days(world, 120)
        world.close()
        self.assertGreaterEqual(after['dragon_retaliations'] - before['dragon_retaliations'], 1)
        self.assertGreater(after['dragon_hoard'], before['dragon_hoard'] + 250)

    def test_traits_change_travel_and_slots_report_successions(self):
        theta = np.random.default_rng(3).normal(0, 0.3, fastworld.policy_size())
        fastworld.set_policy(theta, fastworld.LEARNED)
        try:
            base = fastworld.FastWorld.new(2, 30)
            fastworld.run_days(base, 90)
            plain = base.hash()
            base.close()
            fastworld.set_traits(np.full((512, fastworld.TRAITS), 2.0))
            biased = fastworld.FastWorld.new(2, 30)
            fastworld.run_days(biased, 90)
            traited = biased.hash()
            ids, generations, roles, hungry = fastworld.slots(biased)
            biased.close()
        finally:
            fastworld.set_traits(None)
            fastworld.set_policy(None)
        self.assertNotEqual(plain, traited)
        self.assertEqual(len(ids), len(generations))
        self.assertGreater(len(ids), 50)

    def test_accelerated_generations_replace_the_person_in_the_slot(self):
        world = fastworld.FastWorld.new(1, 30)
        before, *_ = fastworld.slots(world)
        fastworld.set_person(world, 0, death_in_days=10)
        fastworld.run_days(world, 20)
        after, gens, *_ = fastworld.slots(world)
        world.close()
        self.assertNotEqual(before[0], after[0])
        self.assertGreaterEqual(gens[0], 1)


if __name__ == '__main__':
    unittest.main()
