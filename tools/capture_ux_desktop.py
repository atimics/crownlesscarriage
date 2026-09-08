#!/usr/bin/env python3
"""Capture the desktop reading matrix for issues #258 and #293."""
import argparse
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    root = Path(__file__).resolve().parents[1]
    output = root / "out" / "ux-desktop-review"
    output.mkdir(parents=True, exist_ok=True)
    (output / "results.json").unlink(missing_ok=True)
    rows = []
    for width, height in [(1040, 620), (1200, 700), (1280, 720), (1920, 1080)]:
        for scale in [0, 2]:
            for scene, view in [("conversation", 2), ("trade", 3), ("book", 4), ("road", 7)]:
                path = output / f"{scene}-{width}-{scale}.png"
                path.unlink(missing_ok=True)
                with path.with_suffix(".log").open("w") as log:
                    subprocess.run([str(binary), "--capture-ux", str(view),
                                    str(path.relative_to(root)), str(width), str(scale)],
                                   cwd=root, stdout=log, stderr=log, check=True, timeout=90)
                data = path.read_bytes()
                if data[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", data[16:24]) != (width, height):
                    raise RuntimeError(f"Unexpected image dimensions: {path}")
                rows.append({"scene": scene, "width": width, "height": height,
                             "text_size": scale, "file": path.name})
                print(path.name, flush=True)
    (output / "results.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    main()
