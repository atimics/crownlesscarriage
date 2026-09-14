#!/usr/bin/env python3

from __future__ import annotations

import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOTS = (
    ROOT / "assets" / "campaigns",
    ROOT / "tests" / "fixtures" / "shipped",
    ROOT / "docs" / "experiments",
)


def save_paths(roots: tuple[Path, ...]) -> list[Path]:
    return sorted(path for root in roots if root.exists() for path in root.rglob("*.ccsave"))


def validate(path: Path) -> str | None:
    uri = f"file:{path.resolve()}?mode=ro"
    try:
        with sqlite3.connect(uri, uri=True) as database:
            rows = database.execute("PRAGMA integrity_check;").fetchall()
    except sqlite3.Error as error:
        return str(error)
    if rows != [("ok",)]:
        return "; ".join(str(row[0]) for row in rows)
    return None


def main() -> int:
    roots = tuple(Path(argument).resolve() for argument in sys.argv[1:]) or DEFAULT_ROOTS
    paths = save_paths(roots)
    if not paths:
        print("No Crownless save files found.", file=sys.stderr)
        return 1
    failures = [(path, error) for path in paths if (error := validate(path)) is not None]
    for path, error in failures:
        print(f"{path}: {error}", file=sys.stderr)
    print(f"Validated {len(paths)} Crownless save files.")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
