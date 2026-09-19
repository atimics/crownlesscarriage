#!/usr/bin/env python3
"""Check real participant targets, context loss, and native inference parity."""
import copy
import gzip
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
from participant_training import NativeTokenizer, build_prompt, compile_row, wire

PROBE = Path(sys.argv.pop(1)).resolve()
MODEL = ROOT / 'assets/language/core.ccv2'
RECORD = json.loads(gzip.decompress((ROOT / 'docs/reviews/participant-minds-2026-09-19/paired-teacher.json.gz').read_bytes()))


class TrainingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tokenizer = NativeTokenizer(PROBE)

    def test_actual_teacher_turns_fit_and_match_native_prefix(self):
        for row in RECORD['rows']:
            packed = compile_row(row, self.tokenizer)
            prefix = packed['prompt']
            native = subprocess.check_output([str(PROBE), str(MODEL),
                '--participant-prefix', prefix['text'], '--dump-prefix'], text=True)
            self.assertEqual(list(map(int, native.split())), prefix['tokens'])
            self.assertLessEqual(len(packed['tokens']), 512)
            self.assertEqual(json.loads(packed['target_text']), row['output'])
            self.assertIn(row['input']['participant']['self']['name'], prefix['text'])
            if row['input']['observed_turns']:
                self.assertIn(row['input']['observed_turns'][-1]['text'], prefix['text'])
            self.assertEqual(packed['review_status'], 'pending_compact_review')

    def test_native_prefix_carries_zero_meta_and_rejects_bad_input(self):
        prefix = build_prompt(RECORD['rows'][0]['input'], self.tokenizer)
        args = [str(PROBE), str(MODEL), '--participant-prefix']
        meta = subprocess.check_output(args + [prefix['text'], '--dump-meta'], text=True)
        self.assertEqual(list(map(int, meta.split())), [0] * (16 * len(prefix['tokens'])))
        for bad in ('', 'legacy prompt', 'crownless-person-v1\n[EOS]',
                    'crownless-person-v1\n' + 'x ' * 400):
            self.assertNotEqual(subprocess.run(args + [bad, '--dump-prefix'],
                capture_output=True).returncode, 0)

    def test_loss_teaches_only_actor_output_and_eos(self):
        packed = compile_row(RECORD['rows'][2], self.tokenizer)
        n = len(packed['prompt']['tokens'])
        target = list(self.tokenizer.encode(packed['target_text']))
        self.assertEqual(packed['labels'][:n-1], [-100] * (n-1))
        self.assertEqual(packed['labels'][n-1:], target + [0])
        self.assertEqual(packed['tokens'][n:], target)
        self.assertEqual(len(packed['tokens']), len(packed['labels']))

    def test_prompt_selection_independent_of_answer(self):
        a = copy.deepcopy(RECORD['rows'][2])
        b = copy.deepcopy(a)
        b['output'] = {'kind': 'speech', 'text': 'I need time to think.'}
        self.assertEqual(compile_row(a, self.tokenizer)['prompt'],
                         compile_row(b, self.tokenizer)['prompt'])

    def test_other_private_state_is_absent(self):
        # Hartha learns the 117 coins only after Harthild says it on turn two.
        before = compile_row(RECORD['rows'][1], self.tokenizer)['prompt']['text']
        after = compile_row(RECORD['rows'][3], self.tokenizer)['prompt']['text']
        self.assertNotIn('117', before)
        self.assertIn('117', after)
        self.assertIn('"Hartha Stonehewer","smith",36', before)
        self.assertNotIn('"quarryman",59', before)

    def test_budget_keeps_whole_latest_turn_and_reports_drops(self):
        row = RECORD['rows'][-1]
        packed = compile_row(row, self.tokenizer)['prompt']
        self.assertIn(['turn', 0], packed['dropped'])
        self.assertIn(row['input']['observed_turns'][-1]['text'], packed['text'])
        with self.assertRaisesRegex(ValueError, 'latest observation'):
            build_prompt(row['input'], self.tokenizer, context=200, reply_tokens=160)

    def test_target_overflow_is_rejected_whole(self):
        row = copy.deepcopy(RECORD['rows'][0])
        row['output'] = {'kind': 'speech', 'text': 'x ' * 220}
        with self.assertRaisesRegex(ValueError, 'reply budget'):
            compile_row(row, self.tokenizer)
        row['output'] = {'kind': 'speech', 'text': 'a' * 511}
        with self.assertRaisesRegex(ValueError, 'byte limit'):
            compile_row(row, self.tokenizer)
        with self.assertRaisesRegex(ValueError, 'invalid context'):
            build_prompt(row['input'], self.tokenizer, reply_tokens=161)
        prefix = build_prompt(RECORD['rows'][3]['input'], self.tokenizer, reply_tokens=32)
        self.assertLessEqual(len(prefix['tokens']), 352)

    def test_literal_control_strings_survive_as_speech(self):
        row = copy.deepcopy(RECORD['rows'][0])
        row['output'] = {'kind': 'speech', 'text': 'The marks read [EOS] and [F0].'}
        packed = compile_row(row, self.tokenizer)
        self.assertEqual(json.loads(packed['target_text']), row['output'])
        self.assertTrue(all(t >= 9 for t in packed['tokens']))
        self.assertEqual(json.loads(wire('one\ntwo')), 'one\ntwo')
        with self.assertRaisesRegex(ValueError, 'reserved'):
            self.tokenizer.encode('[EOS]')

    def test_action_has_one_actor_target(self):
        row = copy.deepcopy(RECORD['rows'][0])
        row['output'] = {'kind': 'action', 'action': 'end_conversation'}
        packed = compile_row(row, self.tokenizer)
        self.assertEqual(json.loads(packed['target_text']), row['output'])
        row['output']['action'] = 'buy_food'
        with self.assertRaises(ValueError):
            compile_row(row, self.tokenizer)

    def test_cli_preserves_rejections_and_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'input.jsonl'
            source.write_text(json.dumps(RECORD['rows'][0]) + '\n{broken\n')
            output = root / 'compiled'
            run = subprocess.run([sys.executable, str(ROOT / 'tools/dialogue/participant_training.py'),
                '--input', str(source), '--output', str(output), '--probe', str(PROBE)],
                text=True, capture_output=True)
            self.assertEqual(run.returncode, 1)
            receipt = json.loads((output / 'receipt.json').read_text())
            self.assertEqual((receipt['compiled'], receipt['rejected']), (1, 1))
            failure = json.loads((output / 'rejected.jsonl').read_text())
            self.assertEqual(failure['source'], '{broken')
            self.assertEqual(len(receipt['native_probe_sha256']), 64)
            self.assertEqual(len((output / 'previews.jsonl').read_text().splitlines()), 1)

    def test_student_worker_keeps_identity_and_preserves_bad_output(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / 'probe'
            # Test transport around a known response; real native tokenization.
            probe.write_text('#!' + sys.executable + '\n' +
                'import subprocess,sys\n' +
                'if sys.argv[1]=="--encode":\n' +
                ' r=subprocess.run([' + repr(str(PROBE)) + ']+sys.argv[1:]); sys.exit(r.returncode)\n' +
                'print(\'{"kind":"speech","text":"Shall we ask the price?"}\')\n')
            probe.chmod(0o755)
            args = [sys.executable, str(ROOT / 'tools/dialogue/student_worker.py'),
                    '--probe', str(probe), '--model', str(MODEL)]
            requests = [RECORD['rows'][i]['input'] for i in (0, 2, 1)]
            result = subprocess.run(args, input=''.join(json.dumps(r)+'\n' for r in requests),
                                    text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(len(result.stdout.splitlines()), 2)
            self.assertIn('changed person', result.stderr)
            self.assertIn('stdout_hex', result.stderr)
            probe.write_text(probe.read_text().replace(
                'print(\'{"kind":"speech","text":"Shall we ask the price?"}\')',
                'sys.stdout.buffer.write(b"broken\\xff"); sys.exit(1)'))
            result = subprocess.run(args, input=json.dumps(requests[0])+'\n',
                                    text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, '')
            self.assertIn(b'broken\xff'.hex(), result.stderr)


if __name__ == '__main__':
    unittest.main()
