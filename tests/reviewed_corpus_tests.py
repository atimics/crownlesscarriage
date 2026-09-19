#!/usr/bin/env python3
"""Verify review binding and real evidence loss before assembling training data."""
import copy
import gzip
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
from paired_participants import digest
from participant_training import NativeTokenizer, compile_row
from reviewed_corpus import approve, assemble

PROBE = Path(sys.argv.pop(1)).resolve()
DATA = ROOT / 'docs/reviews/participant-minds-2026-09-19'


class ReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tokenizer = NativeTokenizer(PROBE)
        cls.dragon = json.loads(gzip.decompress((DATA / 'paired-teacher.json.gz').read_bytes()))['rows']
        cls.coworkers = json.loads(gzip.decompress((DATA / 'coworker-teachers.json.gz').read_bytes()))['rows']

    def review(self, row, evidence):
        packed = compile_row(row, self.tokenizer)
        review = {'source_sha256': digest(row), 'compact_sha256': digest(packed),
                  'source_decision': 'approved', 'compact_decision': 'approved',
                  'reviewer': 'test reviewer', 'notes': 'Test evidence annotation.',
                  'required_evidence': evidence}
        return packed, review

    def test_real_retained_account_can_support_target(self):
        row = self.dragon[0]
        packed, review = self.review(row, [['account', 0], ['self', 'hungry_days'], ['self', 'occupation']])
        result = approve(row, packed, review, 'world-1202')
        self.assertEqual(result['review_status'], 'approved_compact')
        self.assertEqual(result['world_group'], 'world-1202')
        self.assertEqual(result['labels'], packed['labels'])

    def test_real_omitted_account_rejects_even_an_approval(self):
        row = copy.deepcopy(self.coworkers[1])
        # Deliberately mistaken approval must still fail the evidence check.
        row['review_status'] = 'pending'
        row['input']['participant']['held_accounts'][0]['account'] += ' x' * 200
        packed, review = self.review(row, [['account', 0]])
        with self.assertRaisesRegex(ValueError, 'required evidence was omitted'):
            approve(row, packed, review, 'world-1202')

    def test_latest_received_speech_is_evidence(self):
        row = self.dragon[2]
        packed, review = self.review(row, [['turn', 1], ['self', 'coins']])
        self.assertEqual(approve(row, packed, review, 'world-1202')['source_review_status'], 'approved')

    def test_changed_source_requires_new_review(self):
        row = copy.deepcopy(self.dragon[0])
        packed, review = self.review(row, [['self', 'coins']])
        row['input']['participant']['self']['coins'] += 1
        with self.assertRaisesRegex(ValueError, 'different source'):
            approve(row, packed, review, 'world-1202')

    def test_changed_compact_input_requires_new_review(self):
        row = self.dragon[0]
        packed, review = self.review(row, [['self', 'coins']])
        packed['prompt']['text'] += 'extra'
        with self.assertRaisesRegex(ValueError, 'different source or compact'):
            approve(row, packed, review, 'world-1202')

    def test_unknown_evidence_and_missing_world_are_rejected(self):
        row = self.dragon[0]
        for evidence in ([['account', 100]], [['self', 'death_day']], [['turn', True]]):
            packed, review = self.review(row, evidence)
            with self.assertRaises(ValueError):
                approve(row, packed, review, 'world-1202')
        packed, review = self.review(row, [])
        with self.assertRaisesRegex(ValueError, 'world_group'):
            approve(row, packed, review, '')

    def test_relationship_review_distinguishes_retained_values_from_cause_id(self):
        row = self.coworkers[0]
        packed, review = self.review(row, [['relationship', 'history']])
        self.assertEqual(approve(row, packed, review, 'world-1202')['review_status'], 'approved_compact')
        for ref in (['relationship'], ['relationship', 'cause_event_id']):
            packed, review = self.review(row, [ref])
            with self.assertRaisesRegex(ValueError, 'required evidence was omitted'):
                approve(row, packed, review, 'world-1202')

    def test_original_rejection_is_preserved(self):
        row = self.coworkers[1]
        packed, review = self.review(row, [])
        with self.assertRaisesRegex(ValueError, 'candidate was rejected'):
            approve(row, packed, review, 'world-1202')

    def test_assembly_retains_unreviewed_and_malformed_lines(self):
        a, b = self.dragon[:2]
        _, review = self.review(a, [['account', 0]])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidates, reviews = root / 'candidates', root / 'reviews'
            candidates.write_text(json.dumps(a)+'\n'+json.dumps(b)+'\n{broken\n')
            reviews.write_text(json.dumps([review]))
            receipt = assemble(candidates, reviews, root / 'out', self.tokenizer, 'world-1202')
            self.assertEqual((receipt['approved'], receipt['excluded']), (1, 2))
            failed = [json.loads(line) for line in (root/'out/excluded.jsonl').read_text().splitlines()]
            self.assertEqual(failed[0]['error'], 'candidate needs a review')
            self.assertEqual(failed[1]['source'], '{broken')
            self.assertEqual((root/'out/source-candidates.jsonl').read_bytes(), candidates.read_bytes())
            with self.assertRaises(FileExistsError):
                assemble(candidates, reviews, root/'out', self.tokenizer, 'world-1202')

    def test_repeated_source_is_counted_once(self):
        row = self.dragon[0]
        _, review = self.review(row, [['account', 0]])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidates, reviews = root/'candidates', root/'reviews'
            candidates.write_text((json.dumps(row)+'\n')*2)
            reviews.write_text(json.dumps([review]))
            result = assemble(candidates, reviews, root/'out', self.tokenizer, 'world-1202')
            self.assertEqual((result['approved'], result['excluded']), (1, 1))
            self.assertIn('duplicate source candidate', (root/'out/excluded.jsonl').read_text())


if __name__ == '__main__':
    unittest.main()
