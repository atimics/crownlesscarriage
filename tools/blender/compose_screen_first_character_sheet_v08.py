#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "out" / "character-experiments" / "hair_v08" / "sheet"
OUTPUT_PATH = (
    ROOT / "assets" / "previews" / "experiments" /
    "screen_first_character_sheet_v08.png"
)
VIEWS = (
    ("front", "FRONT"),
    ("three_quarter", "THREE-QUARTER"),
    ("side", "SIDE"),
    ("back", "BACK"),
)


def main() -> None:
    images = []
    for filename, label in VIEWS:
        path = SOURCE_DIR / f"{filename}.png"
        if not path.is_file():
            raise SystemExit(f"missing character sheet view: {path}")
        images.append((Image.open(path).convert("RGB"), label))

    panel_width = 240
    top = 58
    bottom = 30
    sheet = Image.new("RGB", (panel_width * len(images), 320 + top + bottom), (9, 12, 13))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    draw.text((14, 12), "CROWNLESS V08 / SCREEN-FIRST CHARACTER", fill=(226, 216, 193), font=font)
    draw.text((14, 30), "DARK TAPERED CLUMPS / ROUNDED SKULL / 60PX READ", fill=(86, 184, 177), font=font)
    for index, (view, label) in enumerate(images):
        x = index * panel_width
        sheet.paste(view, (x, top))
        draw.text((x + 12, top + 8), label, fill=(226, 216, 193), font=font)
        if index:
            draw.line((x, top, x, top + 320), fill=(35, 43, 44), width=1)
    draw.text((14, top + 326), "REVIEW: SILHOUETTE / FACE / REAR CLUMP GAPS", fill=(128, 126, 116), font=font)
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(OUTPUT_PATH)
    print(f"composed V08 character sheet to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
