#!/usr/bin/env python3
"""Compose the runtime economic icon atlas and production review sheet."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
ICON_SOURCE_DIR = ROOT / "assets" / "previews" / "economic-icons"
THUMB_DIR = ROOT / "assets" / "previews" / "catalog" / "thumbs"
ATLAS_PATH = ROOT / "assets" / "ui" / "economic_goods_v01.png"
REVIEW_PATH = ROOT / "docs" / "images" / "economic-assets-v01.png"
FONT_PATH = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

SOURCES = (
    ("economy_source_grain_v01", "Grain"),
    ("economy_source_iron_ore_v01", "Iron ore"),
    ("economy_source_gold_ore_v01", "Gold ore"),
    ("economy_source_timber_v01", "Timber"),
    ("economy_source_sheep_v01", "Sheep"),
    ("economy_source_gem_seam_v01", "Gem seam"),
    ("economy_source_stone_quarry_v01", "Stone quarry"),
)
CARGO = (
    ("economy_cargo_food_v01", "Food"),
    ("economy_cargo_iron_v01", "Iron"),
    ("economy_cargo_tools_v01", "Tools"),
    ("economy_cargo_weapons_v01", "Weapons"),
    ("economy_cargo_gold_v01", "Raw gold"),
    ("economy_cargo_gems_v01", "Gems"),
    ("economy_cargo_wood_v01", "Wood"),
    ("economy_cargo_wheat_v01", "Wheat"),
    ("economy_cargo_meat_v01", "Meat"),
    ("economy_cargo_wool_v01", "Wool"),
    ("economy_cargo_stone_v01", "Stone"),
)


def load_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    try:
        return ImageFont.truetype(FONT_PATH, size)
    except OSError:
        return ImageFont.load_default()


def fit_icon(source: Image.Image, size: int) -> Image.Image:
    source = source.convert("RGBA")
    alpha_bounds = source.getchannel("A").getbbox()
    if alpha_bounds is None:
        raise RuntimeError("Economic icon render has no visible pixels")
    cropped = source.crop(alpha_bounds)
    cropped.thumbnail((size, size), Image.Resampling.LANCZOS)
    icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    icon.alpha_composite(
        cropped, ((size - cropped.width) // 2, (size - cropped.height) // 2))
    return icon


def compose_atlas() -> None:
    frame_size = 32
    atlas = Image.new(
        "RGBA", (frame_size * len(CARGO), frame_size), (0, 0, 0, 0))
    for frame, (asset_id, _) in enumerate(CARGO):
        source = Image.open(ICON_SOURCE_DIR / f"{asset_id}.png")
        icon = fit_icon(source, 28)
        atlas.alpha_composite(icon, (frame * frame_size + 2, 2))
    ATLAS_PATH.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(ATLAS_PATH, optimize=True)


def compose_review_sheet() -> None:
    columns = 6
    cell_width = 280
    image_height = 230
    label_height = 40
    title_height = 70
    section_height = 42
    row_height = image_height + label_height
    sections = (("WORLD SOURCES", SOURCES), ("CARRIED GOODS", CARGO))
    section_rows = [
        (len(entries) + columns - 1) // columns
        for _, entries in sections
    ]
    canvas_height = title_height + sum(
        section_height + rows * row_height for rows in section_rows)
    canvas = Image.new(
        "RGB", (cell_width * columns, canvas_height), "#111019")
    draw = ImageDraw.Draw(canvas)
    title_font = load_font(28)
    section_font = load_font(18)
    label_font = load_font(20)
    draw.text((24, 20), "Crownless economic sources and carried goods",
              fill="#e2d8c1", font=title_font)
    section_y = title_height
    for (section_name, entries), rows in zip(sections, section_rows):
        draw.text((24, section_y + 8), section_name,
                  fill="#79c8c3", font=section_font)
        grid_y = section_y + section_height
        for index, (asset_id, label) in enumerate(entries):
            row = index // columns
            column = index % columns
            source = Image.open(THUMB_DIR / f"{asset_id}.png").convert("RGB")
            source = source.resize((image_height, image_height),
                                   Image.Resampling.LANCZOS)
            x = column * cell_width + (cell_width - image_height) // 2
            y = grid_y + row * row_height
            canvas.paste(source, (x, y))
            bounds = draw.textbbox((0, 0), label, font=label_font)
            label_width = bounds[2] - bounds[0]
            draw.text((column * cell_width + (cell_width - label_width) // 2,
                       y + image_height + 8), label,
                      fill="#c9b684", font=label_font)
        section_y = grid_y + rows * row_height
    REVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(REVIEW_PATH, optimize=True)


def main() -> None:
    compose_atlas()
    compose_review_sheet()
    print(f"Wrote {ATLAS_PATH.relative_to(ROOT)} and {REVIEW_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
