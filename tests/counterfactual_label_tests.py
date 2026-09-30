"""The relabelling rule decides what a counterfactual row teaches; test it without the simulation."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'dialogue'))
from counterfactual import relabel, train_label


class RelabelTests(unittest.TestCase):
    def test_a_strictly_better_option_wins(self):
        self.assertEqual(relabel([0.26, 0.30, 0.34], base_choice=0), 2)

    def test_ties_keep_the_base_choice(self):
        self.assertEqual(relabel([0.3, 0.3, 0.3], base_choice=1), 1)

    def test_a_difference_inside_epsilon_keeps_the_base_choice(self):
        self.assertEqual(relabel([0.3, 0.3 + 1e-9], base_choice=0), 0)

    def test_refused_options_never_win(self):
        self.assertEqual(relabel([0.2, float('-inf'), 0.1], base_choice=2), 0)
        self.assertEqual(relabel([float('-inf'), 0.1], base_choice=1), 1)

    def test_best_of_several_wins_over_the_base(self):
        self.assertEqual(relabel([0.1, 0.5, 0.4, 0.5], base_choice=0), 1)

    def test_relabels_to_refusing_are_not_taught(self):
        row = {'choice': 0, 'label': 1, 'intents': ['accept', 'decline']}
        self.assertEqual(train_label(row), 0)
        row = {'choice': 0, 'label': 1, 'intents': ['accept', 'counter_offer']}
        self.assertEqual(train_label(row), 1)
        self.assertEqual(train_label({'choice': 2, 'label': 2, 'intents': ['a', 'b', 'end']}), 2)


if __name__ == '__main__':
    unittest.main()
