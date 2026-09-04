#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "out" / "character-experiments" / "redo_v09"
OUTPUT_PATH = (
    ROOT / "assets" / "previews" / "experiments" /
    "screen_first_redo_silhouette_v09.png"
)
VIEWS = (
    ("front", "FRONT"),
    ("three_quarter", "THREE-QUARTER"),
    ("side", "SIDE"),
    ("back", "BACK"),
)


def main() -> None:
    panels = []
    for filename, label in VIEWS:
        path = SOURCE_DIR / f"{filename}.png"
        if not path.is_file():
            raise SystemExit(f"missing silhouette view: {path}")
        panels.append((Image.open(path).convert("RGB"), label))

    width = 240 * len(panels)
    top = 62
    bottom = 42
    sheet = Image.new("RGB", (width, 320 + top + bottom), (184, 180, 171))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    draw.text((14, 12), "CROWNLESS V09 / PROPORTION RESET", fill=(16, 20, 21), font=font)
    draw.text((14, 31), "ADULT SILHOUETTE / 2.14M BOUNDS / NO DETAIL", fill=(39, 88, 85), font=font)
    for index, (panel, label) in enumerate(panels):
        x = index * 240
        sheet.paste(panel, (x, top))
        draw.text((x + 12, top + 9), label, fill=(35, 39, 39), font=font)
        if index:
            draw.line((x, top, x, top + 320), fill=(146, 143, 136), width=1)
    draw.text((14, top + 329), "GATE 1: HEIGHT / HEAD SIZE / LIMB LENGTH / BODY WIDTH", fill=(55, 58, 57), font=font)
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(OUTPUT_PATH)
    print(f"composed V09 silhouette review to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
