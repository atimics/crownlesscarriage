"""Checks for v3 procedural training data boundaries."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'dialogue'))
from meaning_data import dataset


class MeaningDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = dataset(36)

    def test_each_split_has_rows_and_format(self):
        self.assertTrue(all(self.rows.values()))
        for rows in self.rows.values():
            for row in rows:
                self.assertEqual(row['prompt']['format'], 'crownless-meaning-v3')
                self.assertEqual(row['tokens'], row['prompt']['tokens'] + row['target'])
                self.assertEqual(len(row['tokens']), len(row['labels']))
                self.assertEqual(row['target_text'], json.dumps({'choiceindex': row['teacher_index']}, separators=(',', ':')))

    def test_exact_inputs_and_profiles_are_held_out(self):
        profiles = {name: {row['state_group'] for row in rows} for name, rows in self.rows.items()}
        tokens = {name: {tuple(row['prompt']['tokens']) for row in rows} for name, rows in self.rows.items()}
        for left, right in (('train', 'development'), ('train', 'test'), ('development', 'test')):
            self.assertFalse(profiles[left] & profiles[right])
            self.assertFalse(tokens[left] & tokens[right])

    def test_teacher_has_varied_choices_and_counterfactual_fields(self):
        rows = [row for values in self.rows.values() for row in values]
        self.assertGreaterEqual(len({row['teacher_index'] for row in rows}), 3)
        persons = [row['input']['person'] for row in rows]
        self.assertGreater(len({p['self']['coins'] for p in persons}), 2)
        self.assertGreater(len({p['relationship']['trust'] for p in persons}), 2)
        self.assertTrue(any(f['day'] < p['day'] for p in persons for f in p['facts']))
        self.assertTrue(any(f['private'] for p in persons for f in p['facts']))
        self.assertTrue(any(p['outcomes'] for p in persons))


if __name__ == '__main__':
    unittest.main()
