#!/usr/bin/env python3
"""Run the Cppcheck rules that guard against dead generated code."""

from __future__ import annotations

import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BASELINE_PATH = ROOT / "tools" / "static_analysis_baseline.txt"
BLOCKED_IDS = {
    "duplicateBranch",
    "duplicateCondition",
    "duplicateExpressionTernary",
    "duplicateValueTernary",
    "knownConditionTrueFalse",
    "unreadVariable",
}
BLOCKED_SEVERITIES = {"error", "warning"}


def load_baseline() -> dict[tuple[str, str, int], str]:
    entries: dict[tuple[str, str, int], str] = {}
    for number, raw_line in enumerate(
        BASELINE_PATH.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|", 3)
        if len(parts) != 4:
            raise ValueError(
                f"{BASELINE_PATH}:{number}: expected id|file|line|reason"
            )
        finding_id, file_name, line_text, reason = parts
        key = (finding_id, file_name, int(line_text))
        if key in entries:
            raise ValueError(f"{BASELINE_PATH}:{number}: duplicate entry")
        entries[key] = reason
    return entries


def main() -> int:
    try:
        baseline = load_baseline()
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 2

    command = [
        "cppcheck",
        "--enable=warning,style",
        "--inconclusive",
        "--std=c17",
        "--language=c",
        "--inline-suppr",
        "--quiet",
        "--xml",
        "--xml-version=2",
        "-j",
        "4",
        "-Isrc",
        "src",
    ]
    try:
        result = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
    except FileNotFoundError:
        print("cppcheck is required for static analysis.", file=sys.stderr)
        return 2

    try:
        report = ET.fromstring(result.stderr)
    except ET.ParseError as error:
        print(f"Cppcheck report could not be read: {error}", file=sys.stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        return 2

    findings: list[tuple[tuple[str, str, int], str]] = []
    seen_baseline: set[tuple[str, str, int]] = set()
    for error in report.findall(".//error"):
        finding_id = error.get("id", "unknown")
        severity = error.get("severity", "unknown")
        if finding_id not in BLOCKED_IDS and severity not in BLOCKED_SEVERITIES:
            continue
        location = error.find("location")
        if location is None:
            file_name = error.get("file0", "unknown")
            line = 0
        else:
            file_name = location.get("file", "unknown")
            line = int(location.get("line", "0"))
        key = (finding_id, file_name, line)
        if key in baseline:
            seen_baseline.add(key)
            continue
        findings.append((key, error.get("msg", "Static analysis finding")))

    stale = sorted(set(baseline) - seen_baseline)
    if findings:
        print("Static analysis found blocked items:", file=sys.stderr)
        for (finding_id, file_name, line), message in findings:
            print(
                f"  {file_name}:{line}: [{finding_id}] {message}",
                file=sys.stderr,
            )
    if stale:
        print("Static analysis baseline has stale items:", file=sys.stderr)
        for finding_id, file_name, line in stale:
            print(
                f"  {file_name}:{line}: [{finding_id}] {baseline[(finding_id, file_name, line)]}",
                file=sys.stderr,
            )
    if findings or result.returncode != 0:
        if result.returncode != 0:
            print(
                f"Cppcheck exited with status {result.returncode}.",
                file=sys.stderr,
            )
        return 1

    print(
        "Static analysis passed with "
        f"{len(seen_baseline)} reviewed baseline item(s)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
