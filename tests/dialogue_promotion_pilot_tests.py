"""Regression cases for the issue 809 promotion pilot."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
sys.path.insert(0, str(ROOT / 'tools/language'))
import promotion_pilot as p

EVIDENCE = ROOT / 'docs/reviews/dialogue-promotion-2026-09-21'


class PilotHelpers(unittest.TestCase):
    def test_norm_collapses_whitespace_and_case(self):
        self.assertEqual(p.norm('  A  Fodder\nShortage '), 'a fodder shortage')

    def test_copyable_fields_excludes_hidden_and_unspoken(self):
        packet = {'fields': [{'text': 'Ada', 'spoken': True, 'knowledge': 0},
                             {'text': 'someone', 'spoken': True, 'knowledge': 3},
                             {'text': '19', 'spoken': False, 'knowledge': 0}]}
        self.assertEqual(p.copyable_fields(packet), ['Ada'])


class FrozenResult(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.summary = json.loads((EVIDENCE / 'summary.json').read_text())

    def test_shipped_model_ties_the_authored_reference(self):
        all_block = self.summary['all']
        exact, total = all_block['exact']
        self.assertGreaterEqual(exact / total, 0.9)
        preserved, fields = all_block['field_preserved']
        self.assertEqual(preserved, fields)
        no_invented, cases = all_block['cases_no_invented']
        self.assertEqual(no_invented, cases)

    def test_decision_is_no_promotion_on_this_task(self):
        # B ties A, so the decision rule keeps A and recommends a new capability.
        self.assertEqual(self.summary['arms']['B'], 'shipped core.ccv2')
        exact, total = self.summary['all']['exact']
        self.assertLess(exact, total)


if __name__ == '__main__':
    unittest.main()