"""Prove the event gate fails when the simulation and grammar drift apart."""
import json
import copy
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
sys.path.insert(0, str(ROOT / 'tools'))
from audit_grammar import check_coverage, inventory
from compile_core_accounts import compatible_grammars


class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / 'src/sim', self.root / 'src/sim')
        for name in ('tests/fixtures/legacy_event_formats.c', 'tools/data/core_account_rules.json'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)

    def test_current_events_have_rules(self):
        check_coverage(inventory(self.root))

    def test_new_event_fails_without_rule(self):
        header = self.root / 'src/sim/cc_sim.h'
        header.write_text(header.read_text().replace(
            '    CC_EVENT_KIND_COUNT',
            '    CC_EVENT_NEW_TEST_EVENT_2 = 999,\n    CC_EVENT_KIND_COUNT', 1))
        with self.assertRaisesRegex(ValueError, 'CC_EVENT_NEW_TEST_EVENT_2'):
            check_coverage(inventory(self.root))

    def test_empty_inventory_fails(self):
        with self.assertRaisesRegex(ValueError, 'inventory is empty'):
            check_coverage({'event_kinds': [], 'account_missing_events': [],
                            'unknown_rule_events': []})

    def test_removed_rule_fails(self):
        path = self.root / 'tools/data/core_account_rules.json'
        data = json.loads(path.read_text())
        kind = data['rules'][0]['kind']
        data['rules'] = [r for r in data['rules'] if r['kind'] != kind]
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'CC_EVENT_' + kind):
            check_coverage(inventory(self.root))

    def test_unknown_kind_fails(self):
        path = self.root / 'tools/data/core_account_rules.json'
        data = json.loads(path.read_text())
        data['rules'][0]['kind'] = 'SPELLING_ERROR'
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'unknown events: CC_EVENT_SPELLING_ERROR'):
            check_coverage(inventory(self.root))

    def test_invented_source_fails(self):
        path = self.root / 'tools/data/core_account_rules.json'
        data = json.loads(path.read_text())
        data['rules'][0]['source'] = '{0} invents an unsupported event at {1}: {2}.'
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'lack simulation source wording'):
            check_coverage(inventory(self.root))

    def test_model_compatibility_requires_every_learned_rule(self):
        data = json.loads((ROOT / 'tools/data/core_account_rules.json').read_text())
        manifest = json.loads((ROOT / 'tools/data/core_account_compatibility.json').read_text())
        expected = [manifest['grammars'][0]['sha256']]
        self.assertEqual(compatible_grammars(data, manifest), expected)
        changed = copy.deepcopy(data)
        changed['rules'][0]['outputs'][0] += ' changed'
        self.assertEqual(compatible_grammars(changed, manifest), [])
        removed = copy.deepcopy(data)
        removed['rules'].pop(0)
        self.assertEqual(compatible_grammars(removed, manifest), [])
        changed = copy.deepcopy(data)
        changed['roles'].reverse()
        self.assertEqual(compatible_grammars(changed, manifest), [])


if __name__ == '__main__':
    unittest.main()
