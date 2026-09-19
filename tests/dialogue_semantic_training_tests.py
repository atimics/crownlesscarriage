"""Small dependency-free checks for the semantic training gate."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import sys


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "tools/dialogue/verify_semantic_training.py"
sys.path.insert(0, str(ROOT / "tools/dialogue"))
from semantic_training_sources import is_relevant


class SemanticTrainingGateTests(unittest.TestCase):
    def test_source_selector_includes_training_inputs_and_runtime(self):
        self.assertTrue(is_relevant(["tools/data/core_account_rules.json"]))
        self.assertTrue(is_relevant(["src/sim/cc_sim.h"]))
        self.assertTrue(is_relevant(["tools/dialogue/requirements-semantic-training.txt"]))
        self.assertFalse(is_relevant(["docs/README.md"]))

    def make_run(self, records, export=b"model"):
        directory = Path(tempfile.mkdtemp())
        (directory / "last.ccv2").write_bytes(export)
        import hashlib
        digest = hashlib.sha256(export).hexdigest()
        (directory / "manifest.json").write_text(json.dumps({"status": "complete", "export_sha256": digest}))
        (directory / "test-evaluation.json").write_text(json.dumps({
            "count": len(records), "exact": sum(row.get("exact", False) for row in records),
            "records": records,
        }))
        return directory

    def run_gate(self, directory):
        probe = directory / "probe"
        probe.write_text("#!/bin/sh\nprintf '9 32 64\\n'")
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


if __name__ == "__main__":
    unittest.main()
