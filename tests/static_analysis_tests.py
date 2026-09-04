#!/usr/bin/env python3
"""Tests for stable static-analysis finding identities."""

import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools import static_analysis  # noqa: E402


def report(line: int, message: str) -> ET.Element:
    return ET.fromstring(
        f"""<results><errors><error id="syntaxError" severity="error"
        msg="{message}"><location file="src/sample.c" line="{line}"/>
        </error></errors></results>"""
    )


class StableBaselineTests(unittest.TestCase):
    def setUp(self) -> None:
        self.scratch = tempfile.TemporaryDirectory()
        self.root = Path(self.scratch.name)
        (self.root / "src").mkdir()

    def tearDown(self) -> None:
        self.scratch.cleanup()

    def finding(self, source: str, line: int,
                message: str = "sample diagnostic") -> static_analysis.Finding:
        (self.root / "src/sample.c").write_text(source, encoding="utf-8")
        findings = static_analysis.collect_findings(
            report(line, message), self.root
        )
        self.assertEqual(len(findings), 1)
        return findings[0]

    @staticmethod
    def baseline_for(
        finding: static_analysis.Finding,
    ) -> dict[static_analysis.FindingKey, static_analysis.BaselineEntry]:
        entry = static_analysis.BaselineEntry(
            finding.key, finding.line, "reviewed test finding"
        )
        return {finding.key: entry}

    def test_unrelated_line_move_keeps_identity(self) -> None:
        original = self.finding("await sample();\n", 1)
        moved = self.finding("// unrelated\nawait sample();\n", 2)
        unexpected, stale, matched = static_analysis.match_baseline(
            [moved], self.baseline_for(original)
        )
        self.assertEqual(unexpected, [])
        self.assertEqual(stale, [])
        self.assertEqual(matched, 1)

    def test_changed_source_statement_is_new(self) -> None:
        original = self.finding("await sample();\n", 1)
        changed = self.finding("await changed();\n", 1)
        unexpected, stale, matched = static_analysis.match_baseline(
            [changed], self.baseline_for(original)
        )
        self.assertEqual(unexpected, [changed])
        self.assertEqual(len(stale), 1)
        self.assertEqual(matched, 0)

    def test_changed_diagnostic_is_new(self) -> None:
        original = self.finding("await sample();\n", 1)
        changed = self.finding(
            "await sample();\n", 1, "changed diagnostic"
        )
        unexpected, stale, matched = static_analysis.match_baseline(
            [changed], self.baseline_for(original)
        )
        self.assertEqual(unexpected, [changed])
        self.assertEqual(len(stale), 1)
        self.assertEqual(matched, 0)

    def test_missing_finding_leaves_visible_stale_entry(self) -> None:
        original = self.finding("await sample();\n", 1)
        unexpected, stale, matched = static_analysis.match_baseline(
            [], self.baseline_for(original)
        )
        self.assertEqual(unexpected, [])
        self.assertEqual(len(stale), 1)
        self.assertEqual(matched, 0)


if __name__ == "__main__":
    unittest.main()
