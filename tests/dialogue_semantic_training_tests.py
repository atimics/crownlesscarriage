"""Small dependency-free checks for the semantic training gate."""
import json
from pathlib import Path
import subprocess
import tempfile
import shutil
import unittest
import sys


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "tools/dialogue/verify_semantic_training.py"
sys.path.insert(0, str(ROOT / "tools/dialogue"))
from semantic_training_sources import is_relevant
from verify_semantic_training import _check_v3_rows, policy_gate


class SemanticTrainingGateTests(unittest.TestCase):
    def test_v2_preference_tolerance_keeps_validity_strict(self):
        rows = [{'exact': i < 99, 'valid': True, 'eos': True} for i in range(100)]
        result = {'count': 100, 'exact': 99, 'records': rows}
        self.assertTrue(policy_gate(result, True))
        self.assertFalse(policy_gate(result, False))
        rows[0]['valid'] = False
        self.assertFalse(policy_gate(result, True))
        rows[0]['valid'] = True
        rows[1]['exact'] = False
        result['exact'] = 98
        self.assertFalse(policy_gate(result, True))
    def test_source_selector_includes_training_inputs_and_runtime(self):
        self.assertTrue(is_relevant(["tools/data/core_account_rules.json"]))
        self.assertTrue(is_relevant(["src/sim/cc_sim.h"]))
        self.assertTrue(is_relevant(["tools/dialogue/requirements-semantic-training.txt"]))
        self.assertFalse(is_relevant(["docs/README.md"]))

    def make_run(self, records, export=b"model"):
        directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, directory)
        (directory / "last.ccv2").write_bytes(export)
        import hashlib
        digest = hashlib.sha256(export).hexdigest()
        (directory / "manifest.json").write_text(json.dumps({"status": "complete", "format": "crownless-semantic-ids-v1", "export_sha256": digest,
                                                           'datasets': {'test': {'rows': len(records)}}}))
        (directory / "test-evaluation.json").write_text(json.dumps({
            "count": len(records), "exact": sum(row.get("exact", False) for row in records),
            "records": records,
        }))
        return directory

    def run_gate(self, directory, stdout='9 32 64', status=0):
        probe = directory / "probe"
        probe.write_text("#!/bin/sh\nprintf '" + stdout + "\\n'\nexit " + str(status) + '\n')
        probe.chmod(0o755)
        return subprocess.run(["python3", str(GATE), "--run", str(directory), "--probe", str(probe)], capture_output=True)

    def test_empty_evaluation_fails(self):
        self.assertNotEqual(self.run_gate(self.make_run([])).returncode, 0)

    def test_incomplete_record_fails(self):
        row = {"exact": True, "valid": True, "eos": False, "prefix_ids": [128, 131], "ids": [9, 32, 64]}
        self.assertNotEqual(self.run_gate(self.make_run([row])).returncode, 0)

    def test_tampered_export_fails(self):
        directory = self.make_run([{"exact": True, "valid": True, "eos": True, "prefix_ids": [128, 131], "ids": [9, 32, 64]}])
        (directory / "last.ccv2").write_bytes(b"tampered")
        self.assertNotEqual(self.run_gate(directory).returncode, 0)

    def test_native_failure_keeps_receipt(self):
        for output, status in [('bad output', 0), ('9 32 64', 1), ('9 33 64', 0)]:
            directory = self.make_run([{'exact': True, 'valid': True, 'eos': True,
                                       'prefix_ids': [128, 131], 'ids': [9, 32, 64]}])
            self.assertNotEqual(self.run_gate(directory, output, status).returncode, 0)
            receipt = json.loads((directory / 'native-parity.json').read_text())
            self.assertEqual(len(receipt['failures']), 1)
            self.assertIn(output, receipt['failures'][0]['stdout'])

    def test_complete_parity_passes(self):
        directory = self.make_run([{'exact': True, 'valid': True, 'eos': True,
                                   'prefix_ids': [128, 131], 'ids': [9, 32, 64]}])
        self.assertEqual(self.run_gate(directory).returncode, 0)

    def test_unknown_format_is_rejected(self):
        directory = self.make_run([{'exact': True, 'valid': True, 'eos': True,
                                    'prefix_ids': [128, 131], 'ids': [9, 32, 64]}])
        manifest = json.loads((directory / 'manifest.json').read_text())
        manifest['format'] = 'crownless-meaning-v99'
        (directory / 'manifest.json').write_text(json.dumps(manifest))
        self.assertNotEqual(self.run_gate(directory).returncode, 0)

    def test_v3_dataset_tamper_is_rejected(self):
        import meaning
        person = {'self': {'id': '1', 'name': 'A', 'coins': 8, 'hungry_days': 1, 'stress': 20},
                  'listener': {'id': '2', 'name': 'B'}, 'place': {'id': '3', 'name': 'Ash Hollow'},
                  'day': 2, 'relationship': {'trust': 2},
                  'facts': [{'kind': 'food_store', 'owner': '1', 'place_id': '3',
                             'place_name': 'Ash Hollow', 'stock': 16, 'target': 75,
                             'unit_price': 2, 'day': 2, 'source': 'observed', 'private': False}]}
        choices = meaning.candidates(person, [])
        teacher_index = meaning.preferred(person, [], choices)
        row = {'input': {'person': person, 'heard': []},
               'prompt': {'format': meaning.FORMAT, 'tokens': meaning.encode_input(person, [], choices)},
               'target_text': json.dumps({'choiceindex': teacher_index}, separators=(',', ':')),
               'target': [1024 + teacher_index],
               'tokens': meaning.encode_input(person, [], choices) + [1024 + teacher_index],
               'labels': [-100] * (len(meaning.encode_input(person, [], choices)) - 1) + [1024 + teacher_index, 0],
               'choice_count': len(choices), 'teacher_index': teacher_index,
               'teacher_intent': choices[teacher_index]['intent']}
        directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, directory)
        (directory / 'test.jsonl').write_text(json.dumps(row) + '\n')
        import hashlib
        source = Path(meaning.__file__)
        export = directory / 'last.ccv2'
        export.write_bytes(b'model')
        evaluation = {'count': 1, 'exact': 1, 'records': [{
            'prefix_ids': row['prompt']['tokens'], 'reference': {'choiceindex': row['teacher_index']},
            'ids': row['target'], 'eos': True, 'valid': True, 'exact': True, 'error': None,
        }]}
        (directory / 'test-evaluation.json').write_text(json.dumps(evaluation))
        manifest = {'status': 'complete', 'format': 'crownless-meaning-v3',
                    'export_sha256': hashlib.sha256(export.read_bytes()).hexdigest(),
                    'datasets': {'test': {'rows': 1, 'sha256': hashlib.sha256((directory / 'test.jsonl').read_bytes()).hexdigest()}},
                    'sources': {str(source): hashlib.sha256(source.read_bytes()).hexdigest()}}
        (directory / 'manifest.json').write_text(json.dumps(manifest))
        self.assertEqual(_check_v3_rows(directory, evaluation, manifest), [])
        (directory / 'test.jsonl').write_text(json.dumps({**row, 'teacher_index': (row['teacher_index'] + 1) % row['choice_count']}) + '\n')
        self.assertTrue(any('test dataset hash' in error for error in _check_v3_rows(directory, evaluation, manifest)))


if __name__ == "__main__":
    unittest.main()
