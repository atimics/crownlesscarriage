#!/usr/bin/env python3
"""Report tracked art size and reject unreferenced binary assets."""

from __future__ import annotations

from collections import defaultdict
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
MAX_TRACKED_ASSET_BYTES = 64 * 1024 * 1024
MAX_SINGLE_ASSET_BYTES = 8 * 1024 * 1024
BINARY_SUFFIXES = {".blend", ".gif", ".glb", ".mp3", ".mp4", ".png"}
TEXT_SUFFIXES = {
    ".c", ".cmake", ".h", ".html", ".inc", ".js", ".json", ".md",
    ".py", ".sh", ".txt", ".yaml", ".yml",
}


def git_files(*paths: str) -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z", "--", *paths],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [ROOT / name.decode("utf-8")
            for name in result.stdout.split(b"\0") if name]


def size_text(byte_count: int) -> str:
    return f"{byte_count / (1024 * 1024):.1f} MiB"


def main() -> int:
    tracked_assets = [path for path in git_files("assets") if path.is_file()]
    tracked_files = [path for path in git_files(".") if path.is_file()]
    failures: list[str] = []

    preview_files = [path for path in tracked_assets
                     if path.is_relative_to(ROOT / "assets" / "previews")]
    if preview_files:
        failures.append(
            "generated files are tracked under assets/previews: " +
            ", ".join(str(path.relative_to(ROOT)) for path in preview_files)
        )

    reference_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in tracked_files
        if path.suffix.lower() in TEXT_SUFFIXES or path.name == "Makefile"
    )
    unreferenced = [
        path for path in tracked_assets
        if path.suffix.lower() in BINARY_SUFFIXES and
        path.name not in reference_text
    ]
    if unreferenced:
        failures.append(
            "unreferenced binary assets: " +
            ", ".join(str(path.relative_to(ROOT)) for path in unreferenced)
        )

    sizes: dict[str, int] = defaultdict(int)
    total = 0
    for path in tracked_assets:
        size = path.stat().st_size
        total += size
        relative = path.relative_to(ROOT)
        group = "/".join(relative.parts[:2])
        sizes[group] += size
        if size > MAX_SINGLE_ASSET_BYTES:
            failures.append(
                f"{relative} is {size_text(size)}; the per-file limit is "
                f"{size_text(MAX_SINGLE_ASSET_BYTES)}"
            )

    print(f"tracked art: {len(tracked_assets)} files, {size_text(total)}")
    for group, size in sorted(sizes.items()):
        print(f"  {group}: {size_text(size)}")
    if total > MAX_TRACKED_ASSET_BYTES:
        failures.append(
            f"tracked art is {size_text(total)}; the limit is "
            f"{size_text(MAX_TRACKED_ASSET_BYTES)}"
        )

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("tracked art size and references pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
