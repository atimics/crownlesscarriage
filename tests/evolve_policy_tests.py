"""The evolved policy's scorer must be deterministic and always name a legal option."""
from pathlib import Path
import random
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'dialogue'))
try:
    import numpy as np
    import evolve
except ImportError:   # numpy is optional for the rest of the dialogue tools
    np = None
from meaning import candidates
from meaning_data import person


@unittest.skipIf(np is None, 'numpy not installed')
class EvolvePolicyTests(unittest.TestCase):
    def test_features_have_one_row_per_option_and_a_fixed_width(self):
        p = person(3)
        options = candidates(p, [])
        x = evolve.features(p, [], options)
        self.assertEqual(x.shape, (len(options), evolve.FEATURES))
        self.assertTrue(np.isfinite(x).all())

    def test_scorer_is_deterministic_and_legal(self):
        theta = np.random.default_rng(1).normal(0, 0.3, evolve.SIZE)
        policy = evolve.scorer(theta)
        for seed in range(8):
            p = person(seed)
            options = candidates(p, [])
            first = policy(p, [], options, random.Random(0))
            self.assertEqual(first, policy(p, [], options, random.Random(9)))
            self.assertTrue(0 <= first < len(options))

    def test_waiting_changes_the_features(self):
        p = person(3)
        options = candidates(p, [])
        self.assertFalse(np.array_equal(evolve.features(dict(p, waiting=0), [], options),
                                        evolve.features(dict(p, waiting=3), [], options)))

    def test_ranks_are_centred_and_ordered(self):
        r = evolve.ranks(np.array([3.0, 1.0, 2.0]))
        self.assertEqual(list(r), [0.5, -0.5, 0.0])


if __name__ == '__main__':
    unittest.main()
