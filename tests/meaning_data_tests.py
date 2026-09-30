"""Checks for v3 procedural training data boundaries."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'dialogue'))
from meaning_data import dataset, person


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

    def test_bucketed_styles_keep_rows_and_stay_native_valid(self):
        for style in ('buckets', 'afford', 'queue'):
            other = dataset(36, style)
            for name, rows in self.rows.items():
                self.assertEqual([r['teacher_index'] for r in rows], [r['teacher_index'] for r in other[name]])
                for digits, row in zip(rows, other[name]):
                    tokens = row['prompt']['tokens']
                    self.assertLessEqual(len(tokens), len(digits['prompt']['tokens']) + 8)
                    self.assertTrue(all(9 <= t < 4096 for t in tokens))
                    self.assertEqual((tokens[0], tokens[-1], tokens.count(1580)), (1280, 1281, 1))
        from meaning import candidates, encode_input
        p = person(3)
        options = candidates(p, [])
        quiet, busy = dict(p, waiting=0), dict(p, waiting=3)
        self.assertNotEqual(encode_input(quiet, [], options, 'queue'), encode_input(busy, [], options, 'queue'))
        self.assertEqual(encode_input(quiet, [], options, 'afford'), encode_input(busy, [], options, 'afford'))
        self.assertEqual(dataset(36)['test'][0]['prompt']['tokens'], self.rows['test'][0]['prompt']['tokens'])
        with self.assertRaises(ValueError):
            dataset(36, 'exact')


if __name__ == '__main__':
    unittest.main()
