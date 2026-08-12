#!/usr/bin/env python3
"""Compose the assembled, anatomical, and exploded Blender hero renders."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
PREVIEW_DIR = ROOT / "assets" / "previews" / "hero"
FONT_PATH = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"


def font(size: int):
    try:
        return ImageFont.truetype(FONT_PATH, size)
    except OSError:
        return ImageFont.load_default()


items = [
    ("hero_assembled.png", "ASSEMBLED HERO"),
    ("hero_anatomy.png", "BODY + MUSCLE/RIG GUIDES"),
    ("hero_exploded.png", "MODULAR COMPONENT LIBRARY"),
]
image_height = 900
label_height = 68
title_height = 86
cell_widths = [900, 900, 1400]
canvas_width = sum(cell_widths)
canvas = Image.new("RGB", (canvas_width, title_height + image_height + label_height), "#11161a")
draw = ImageDraw.Draw(canvas)
draw.text((28, 24), "CROWNLESS HERO — BLENDER MODULAR PROTOTYPE", fill="#f1eadc", font=font(34))

x = 0
for index, (filename, label) in enumerate(items):
    cell_width = cell_widths[index]
    image = Image.open(PREVIEW_DIR / filename).convert("RGB")
    image.thumbnail((cell_width, image_height), Image.Resampling.LANCZOS)
    tile = Image.new("RGB", (cell_width, image_height), "#171d21")
    tile.paste(image, ((cell_width - image.width) // 2, (image_height - image.height) // 2))
    canvas.paste(tile, (x, title_height))
    draw.rectangle((x, title_height + image_height, x + cell_width,
                    title_height + image_height + label_height), fill="#1b2328")
    draw.text((x + 18, title_height + image_height + 20), label,
              fill="#f1eadc", font=font(23))
    x += cell_width

canvas.save(PREVIEW_DIR / "hero_component_triptych.png", optimize=True)
print(f"Composed {PREVIEW_DIR / 'hero_component_triptych.png'}")
