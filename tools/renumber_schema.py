#!/usr/bin/env python3
"""Shift a branch's schema-version reservations to a new target.

The simulation allocates ``CC_SIM_SCHEMA_VERSION`` when a branch is written,
not when it lands, so a long-lived branch holds an invisible reservation on a
number that another lineage may take first.  This tool performs the mechanical
half of a renumber: it moves the branch's own version literals by a uniform
offset and leaves historical (already-merged) versions untouched.

A literal belongs to the branch when it is *strictly above the base's schema
version* and at or below the branch's current version.  Those, and the
``#define``, move together; everything older is a save-migration fact and must
not move.

Usage::

    # dry run: show every replacement
    python3 tools/renumber_schema.py --base origin/main --to 82

    # write the changes
    python3 tools/renumber_schema.py --base origin/main --to 82 --apply

Run it on the branch being renumbered, with ``--base`` the branch it will be
rebased onto.  A stacked branch (one that includes an ancestor's bump) shifts
its whole owned range, so the base must also be the shifted base.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

SOURCE_DIRS = ("src", "tests", "tools")
SOURCE_SUFFIXES = (".c", ".h", ".inc", ".cc", ".cpp", ".py")
DEFINE_RE = re.compile(r"^(\s*#\s*define\s+CC_SIM_SCHEMA_VERSION\s+)(\d+)(\b.*)$")
COMPARE_RE = re.compile(r"(\bschema_version\s*(?:>=|<=|==|!=|<|>)\s*)(\d+)(U?)")
ASSIGN_RE = re.compile(r"(\bschema_version\s*)(?<![<>=!])=(?!=)(\s*)(\d+)(U?)")


def run_git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args], check=True, capture_output=True, text=True
    )
    return result.stdout


def git_root() -> Path:
    return Path(run_git("rev-parse", "--show-toplevel").strip())


def schema_from_ref(root: Path, ref: str) -> int:
    header = run_git("-C", str(root), "show", f"{ref}:src/sim/cc_sim.h")
    match = re.search(r"#\s*define\s+CC_SIM_SCHEMA_VERSION\s+(\d+)", header)
    if match is None:
        raise SystemExit(f"no CC_SIM_SCHEMA_VERSION in {ref}:src/sim/cc_sim.h")
    return int(match.group(1))


def merge_base(root: Path, ref: str) -> str:
    return run_git("-C", str(root), "merge-base", ref, "HEAD").strip()


def schema_from_worktree(root: Path) -> int:
    header = (root / "src/sim/cc_sim.h").read_text()
    match = re.search(r"#\s*define\s+CC_SIM_SCHEMA_VERSION\s+(\d+)", header)
    if match is None:
        raise SystemExit("no CC_SIM_SCHEMA_VERSION in src/sim/cc_sim.h")
    return int(match.group(1))


def owned_version(value: int, base_schema: int, current_schema: int) -> bool:
    return base_schema < value <= current_schema


def rewrite_line(
    line: str, base_schema: int, current_schema: int, delta: int
) -> tuple[str, list[tuple[int, int]]]:
    """Return the rewritten line and (old, new) pairs for changed literals."""
    changes: list[tuple[int, int]] = []

    def shift_compare(match: re.Match[str]) -> str:
        value = int(match.group(2))
        if not owned_version(value, base_schema, current_schema):
            return match.group(0)
        changes.append((value, value + delta))
        return f"{match.group(1)}{value + delta}{match.group(3)}"

    def shift_assign(match: re.Match[str]) -> str:
        value = int(match.group(3))
        if not owned_version(value, base_schema, current_schema):
            return match.group(0)
        changes.append((value, value + delta))
        return f"{match.group(1)}={match.group(2)}{value + delta}{match.group(4)}"

    new_line = COMPARE_RE.sub(shift_compare, line)
    new_line = ASSIGN_RE.sub(shift_assign, new_line)

    define = DEFINE_RE.match(new_line)
    if define is not None and int(define.group(2)) == current_schema:
        changes.append((current_schema, current_schema + delta))
        new_line = f"{define.group(1)}{current_schema + delta}{define.group(3)}"

    return new_line, changes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="origin/main",
                        help="ref the branch will be rebased onto (default: origin/main)")
    parser.add_argument("--to", type=int, required=True,
                        help="new CC_SIM_SCHEMA_VERSION for this branch")
    parser.add_argument("--apply", action="store_true",
                        help="write the changes (default is a dry run)")
    args = parser.parse_args()

    root = git_root()
    # The floor is the schema at the branch's fork point, not at the current
    # tip of --base: a stale branch predates versions that main has since
    # taken, so those main-owned literals must not be mistaken for the
    # branch's own.  Run this before rebasing onto --base.
    fork_point = merge_base(root, args.base)
    fork_schema = schema_from_ref(root, fork_point)
    target_schema = schema_from_ref(root, args.base)
    current_schema = schema_from_worktree(root)
    delta = args.to - current_schema

    if delta == 0:
        raise SystemExit(f"already at schema {args.to}; nothing to do")
    if args.to <= target_schema:
        raise SystemExit(
            f"target {args.to} must be greater than base schema {target_schema} "
            f"({args.base})"
        )

    print(
        f"fork {fork_point[:10]}=schema {fork_schema}, branch={current_schema}, "
        f"target={args.to} (offset {delta:+d}); base {args.base}=schema {target_schema}"
    )

    total = 0
    touched_files = 0
    for directory in SOURCE_DIRS:
        for path in sorted((root / directory).rglob("*")):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            try:
                original = path.read_text()
            except (UnicodeDecodeError, OSError):
                continue
            if "schema_version" not in original and "CC_SIM_SCHEMA_VERSION" not in original:
                continue

            output_lines: list[str] = []
            file_changes: list[tuple[int, int, int]] = []
            for number, line in enumerate(original.splitlines(keepends=True), start=1):
                new_line, changes = rewrite_line(
                    line, fork_schema, current_schema, delta
                )
                output_lines.append(new_line)
                for old, new in changes:
                    file_changes.append((number, old, new))

            if not file_changes:
                continue

            touched_files += 1
            total += len(file_changes)
            rel = path.relative_to(root)
            for number, old, new in file_changes:
                print(f"  {rel}:{number}: {old} -> {new}")
            if args.apply:
                path.write_text("".join(output_lines))

    action = "Rewrote" if args.apply else "Would rewrite"
    print(f"{action} {total} literal(s) across {touched_files} file(s).")
    if not args.apply and total:
        print("Dry run only; pass --apply to write.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
