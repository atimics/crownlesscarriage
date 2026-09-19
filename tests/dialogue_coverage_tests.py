"""Prove the event gate fails when the simulation and grammar drift apart."""
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
from audit_grammar import check_coverage, inventory


class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ('src/sim/cc_sim.h', 'tools/data/core_account_rules.json'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)

    def test_current_events_have_rules(self):
        check_coverage(inventory(self.root))

    def test_new_event_fails_without_rule(self):
        header = self.root / 'src/sim/cc_sim.h'
        header.write_text(header.read_text().replace(
            '    CC_EVENT_KIND_COUNT',
            '    CC_EVENT_NEW_TEST_EVENT = 999,\n    CC_EVENT_KIND_COUNT', 1))
        with self.assertRaisesRegex(ValueError, 'CC_EVENT_NEW_TEST_EVENT'):
            check_coverage(inventory(self.root))

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


if __name__ == '__main__':
    unittest.main()
