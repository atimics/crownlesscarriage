#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import os
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass
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


@dataclass(frozen=True, order=True)
class FindingKey:
    finding_id: str
    file_name: str
    fingerprint: str


@dataclass(frozen=True)
class BaselineEntry:
    key: FindingKey
    line: int
    reason: str


@dataclass(frozen=True)
class Finding:
    key: FindingKey
    line: int
    message: str


def load_baseline(
    path: Path = BASELINE_PATH,
) -> dict[FindingKey, BaselineEntry]:
    entries: dict[FindingKey, BaselineEntry] = {}
    for number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|", 4)
        if len(parts) != 5:
            raise ValueError(
                f"{path}:{number}: expected id|file|line|fingerprint|reason"
            )
        finding_id, file_name, line_text, fingerprint, reason = parts
        if not re.fullmatch(r"[0-9a-f]{64}", fingerprint):
            raise ValueError(f"{path}:{number}: fingerprint must be SHA-256")
        key = FindingKey(finding_id, file_name, fingerprint)
        if key in entries:
            raise ValueError(f"{path}:{number}: duplicate entry")
        entries[key] = BaselineEntry(key, int(line_text), reason)
    return entries


def source_statement(root: Path, file_name: str, line: int) -> str:
    if line <= 0:
        return ""
    try:
        lines = (root / file_name).read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError):
        return ""
    if line > len(lines):
        return ""
    return lines[line - 1].strip()


def finding_fingerprint(
    finding_id: str, severity: str, message: str, statement: str
) -> str:
    identity = "\0".join((finding_id, severity, message, statement))
    return hashlib.sha256(identity.encode("utf-8")).hexdigest()


def collect_findings(report: ET.Element, root: Path = ROOT) -> list[Finding]:
    findings: list[Finding] = []
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
        message = error.get("msg", "Static analysis finding")
        statement = source_statement(root, file_name, line)
        fingerprint = finding_fingerprint(
            finding_id, severity, message, statement
        )
        findings.append(
            Finding(FindingKey(finding_id, file_name, fingerprint),
                    line, message)
        )
    return findings


def match_baseline(
    findings: list[Finding], baseline: dict[FindingKey, BaselineEntry]
) -> tuple[list[Finding], list[BaselineEntry], int]:
    unexpected: list[Finding] = []
    seen: set[FindingKey] = set()
    for finding in findings:
        if finding.key in baseline and finding.key not in seen:
            seen.add(finding.key)
        else:
            unexpected.append(finding)
    stale = sorted(
        (entry for key, entry in baseline.items() if key not in seen),
        key=lambda entry: entry.key,
    )
    return unexpected, stale, len(seen)


def main() -> int:
    try:
        baseline = load_baseline()
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 2

    command = [
        os.environ.get("CC_CPPCHECK_EXECUTABLE", "cppcheck"),
        "--enable=warning,style",
        "--inconclusive",
        "--std=c17",
        "--language=c",
        "--inline-suppr",
        "--quiet",
        "--xml",
        "--xml-version=2",
        "-Isrc",
        "src",
    ]
    try:
        jobs = max(1, int(os.environ.get("CC_CPPCHECK_JOBS", "1")))
        # A build directory keeps whole-program checks enabled with parallel workers.
        with tempfile.TemporaryDirectory(prefix="crownless-cppcheck-") as build_dir:
            result = subprocess.run(
                command + [f"-j{jobs}", f"--cppcheck-build-dir={build_dir}"],
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

    findings, stale, matched_count = match_baseline(
        collect_findings(report), baseline
    )
    if findings:
        print("Static analysis found blocked items:", file=sys.stderr)
        for finding in findings:
            print(
                f"  {finding.key.file_name}:{finding.line}: "
                f"[{finding.key.finding_id}] {finding.message}\n"
                f"    fingerprint: {finding.key.fingerprint}",
                file=sys.stderr,
            )
    if stale:
        print("Static analysis baseline has stale items:", file=sys.stderr)
        for entry in stale:
            print(
                f"  {entry.key.file_name}:{entry.line}: "
                f"[{entry.key.finding_id}] {entry.reason}",
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
        f"{matched_count} reviewed baseline item(s)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
