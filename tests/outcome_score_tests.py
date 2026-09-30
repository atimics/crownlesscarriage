"""Checks for the outcome scorer's arithmetic and policies; no native probes needed."""
from pathlib import Path
import random
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'dialogue'))
import outcomes
from meaning import candidates
from meaning_data import person


def state(**over):
    base = {'first_coins': 5, 'second_coins': 30, 'first_hungry': 2, 'second_hungry': 0,
            'stock': 20, 'target': 80, 'price': 4, 'trust_first_to_second': 3, 'trust_second_to_first': 3}
    base.update(over)
    return base


DAILY = [{'first_hungry': 0, 'second_hungry': 0, 'second_coins': 30}]
FUTURE = {'first': {'hungry': 0, 'bandit': 0}, 'second': {'hungry': 0, 'bandit': 0}}


class OutcomeScoreTests(unittest.TestCase):
    def test_relief_and_cost_are_read_from_before_and_after(self):
        result = {'before': state(), 'after': state(first_hungry=0, second_coins=26, stock=19),
                  'future': FUTURE, 'daily': DAILY, 'turns': 4, 'executed': True}
        c = outcomes.components(result)
        self.assertEqual((c['relief'], c['coins_to_market'], c['units_taken'], c['conserved']), (1, 4, 1, 1))
        self.assertGreater(outcomes.score(c), 0.5)

    def test_doing_nothing_scores_below_a_cheap_purchase(self):
        idle = outcomes.components({'before': state(), 'after': state(), 'future': FUTURE, 'daily': DAILY, 'turns': 2, 'executed': False})
        bought = outcomes.components({'before': state(), 'after': state(first_hungry=0, second_coins=26, stock=19),
                                      'future': FUTURE, 'daily': DAILY, 'future': FUTURE, 'daily': DAILY, 'turns': 4, 'executed': True})
        self.assertEqual(idle['relief'], 0)
        self.assertLess(outcomes.score(idle), outcomes.score(bought))

    def test_money_without_stock_movement_breaks_conservation(self):
        c = outcomes.components({'before': state(), 'after': state(second_coins=26), 'future': FUTURE, 'daily': DAILY, 'turns': 3, 'executed': True})
        self.assertEqual(c['units_taken'], 0)
        c = outcomes.components({'before': state(), 'after': state(stock=19), 'future': FUTURE, 'daily': DAILY, 'turns': 3, 'executed': True})
        self.assertEqual(c['conserved'], 0)

    def test_days_spent_hungry_count_against_a_week(self):
        base = {'before': state(), 'after': state(), 'turns': 2, 'executed': False}
        calm = outcomes.week_score(outcomes.components(dict(base, future=FUTURE, daily=DAILY)))
        hungry_week = [{'first_hungry': d, 'second_hungry': 0, 'second_coins': 30} for d in range(1, 8)]
        bad = outcomes.week_score(outcomes.components(dict(base, future=FUTURE, daily=hungry_week)))
        self.assertLess(bad, calm - 3.5)

    def test_every_policy_returns_a_legal_index(self):
        p = person(3)
        options = candidates(p, [])
        for name, policy in outcomes.POLICIES.items():
            index = policy(p, [], options, random.Random(1))
            self.assertTrue(0 <= index < len(options), name)


if __name__ == '__main__':
    unittest.main()
