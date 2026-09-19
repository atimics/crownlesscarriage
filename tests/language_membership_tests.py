#!/usr/bin/env python3
"""Regression cases for issue 809's ML evaluation metrics."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/language'))
from measure_field_membership import (common_words, freeze_calibration,
                                      invented, prompt_text)
from measure_memory_membership import quotes_memory


class FieldMembershipTests(unittest.TestCase):
    def test_target_is_separate_from_evidence(self):
        row = {'prefix': 'The mill is quiet.', 'output': 'Varkesh arrived.',
               'accepted': ['Varkesh came.'], 'fields': [{'text': 'Varkesh'}],
               'history': [{'text': 'Varkesh'}], 'mind': {'memories': ['Varkesh']}}
        self.assertEqual(invented(row['output'], prompt_text(row), set()), ['Varkesh'])

    def test_capture_replaces_untrimmed_metadata(self):
        row = {'prefix': 'Varkesh met Rose.', 'output': 'Varkesh arrived.',
               'model_input': {'prefix': 'The mill is quiet.', 'copy_spans': ['Rose']}}
        self.assertEqual(invented('Varkesh met Rose.', prompt_text(row), set()), ['Varkesh'])

    def test_whole_names(self):
        self.assertEqual(invented('Rose arrived.', 'Rosewood', set()), ['Rose'])
        self.assertEqual(invented('Rosewood arrived.', 'Rose', set()), ['Rosewood'])

    def test_case_and_possessives(self):
        self.assertEqual(invented("Thornford’s mill.", 'thornford', set()), [])
        self.assertEqual(invented('Thornford', "Thornford's mill", set()), [])

    def test_explicit_alias(self):
        self.assertEqual(invented('Rosie arrived.', 'Rose', set()), ['Rosie'])
        self.assertEqual(invented('Rosie arrived.', 'Rose', set(), {'Rosie': 'Rose'}), [])

    def test_lowercase_and_common_words(self):
        self.assertEqual(invented('The Quiet mill is ready.', '', {'quiet'}), [])


class MemoryMembershipTests(unittest.TestCase):
    memory = "The foundry beside Thornford's northern gate uses 1 Wood to make 4 Paper."

    def test_complete_quote(self):
        self.assertTrue(quotes_memory('I recall: ' + self.memory, [self.memory]))
        self.assertTrue(quotes_memory(self.memory.upper().replace(' ', '  '), [self.memory]))

    def test_changed_quantity(self):
        self.assertFalse(quotes_memory(self.memory.replace('4 Paper', '9 Paper'), [self.memory]))

    def test_changed_role(self):
        changed = self.memory.replace('1 Wood to make 4 Paper', '4 Paper to make 1 Wood')
        self.assertFalse(quotes_memory(changed, [self.memory]))

    def test_partial_claim(self):
        self.assertFalse(quotes_memory(self.memory[:40], [self.memory]))

    def test_selected_memory(self):
        self.assertFalse(quotes_memory('Rose saw the mill.', [self.memory]))

    def test_empty_and_boundaries(self):
        self.assertFalse(quotes_memory('anything', ['', '   ']))
        self.assertFalse(quotes_memory('Rosewood arrived', ['Rose']))
        self.assertFalse(quotes_memory('Primrose', ['rose']))


class CalibrationTests(unittest.TestCase):
    def test_training_only_and_frozen(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            train = root / 'train.jsonl'
            train.write_text(json.dumps({'prefix': 'The mill.', 'output': 'quiet wood',
                                         'accepted': ['soft rain']}) + '\n')
            holdout = root / 'wording.jsonl'
            holdout.write_text(json.dumps({'prefix': 'varkesh', 'output': 'varkesh'}) + '\n')
            artifact = root / 'calibration.json'
            receipt = freeze_calibration(train, artifact)
            self.assertEqual(receipt['source_sha256'], hashlib.sha256(train.read_bytes()).hexdigest())
            self.assertEqual(receipt['rows'], 1)
            self.assertIn('quiet', common_words(artifact))
            self.assertIn('soft', common_words(artifact))
            self.assertNotIn('varkesh', common_words(artifact))
            train.write_text('changed after freeze')
            self.assertIn('quiet', common_words(artifact))
            with self.assertRaises(FileExistsError):
                # Use a valid source to exercise exclusive artifact creation.
                train.write_text('{"prefix":"", "output":""}\n')
                freeze_calibration(train, artifact)
            with self.assertRaises(ValueError):
                freeze_calibration(holdout, root / 'bad.json')

    def test_sample_boundary(self):
        with tempfile.TemporaryDirectory() as tmp:
            train = Path(tmp) / 'train.jsonl'
            train.write_text('{"prefix":"", "output":"first"}\n'
                             '{"prefix":"", "output":"second"}\n')
            artifact = Path(tmp) / 'calibration.json'
            freeze_calibration(train, artifact, sample=1)
            self.assertEqual(common_words(artifact), {'first'})


if __name__ == '__main__':
    unittest.main()
