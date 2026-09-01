#!/usr/bin/env python3
"""Compose the high-resolution and art-grid character experiment renders."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "out" / "character-experiments"
HIGH_PATH = OUT_DIR / "screen_first_character_v04_high.png"
LOW_PATH = OUT_DIR / "screen_first_character_v04_low.png"
PREVIEW_PATH = ROOT / "assets" / "previews" / "experiments" / "screen_first_character_shading_v04.png"
VIEWS_PATH = ROOT / "assets" / "previews" / "experiments" / "screen_first_character_painted_3d_views_v04.png"
FRONT_PATH = OUT_DIR / "screen_first_character_v04_front.png"
THREE_QUARTER_PATH = OUT_DIR / "screen_first_character_v04_three_quarter.png"
SIDE_PATH = OUT_DIR / "screen_first_character_v04_side.png"

WIDTH = 1440
BACKGROUND = (11, 13, 14, 255)
INK = (226, 216, 193, 255)
MUTED = (145, 137, 122, 255)
TEAL = (87, 165, 153, 255)
GOLD = (207, 157, 67, 255)
OXBLOOD = (174, 68, 61, 255)

PIPELINE_PALETTE = (
    (0, 0, 0), (11, 13, 14), (24, 28, 26),
    (47, 24, 14), (91, 48, 24),
    (133, 72, 38), (190, 111, 56),
    (98, 24, 15), (158, 43, 18),
    (23, 49, 43), (39, 82, 68), (55, 104, 85),
    (20, 25, 22), (57, 29, 12), (101, 49, 17),
    (143, 84, 7), (222, 139, 12),
    (50, 52, 45), (76, 77, 61), (104, 95, 67),
)


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    choices = (
        "/System/Library/Fonts/SFNSMono.ttf" if not bold else "/System/Library/Fonts/SFNSMonoBold.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "DejaVuSansMono.ttf",
    )
    for choice in choices:
        try:
            return ImageFont.truetype(choice, size)
        except OSError:
            pass
    return ImageFont.load_default()


def draw_centered(draw: ImageDraw.ImageDraw, x: int, y: int, text: str,
                  text_font: ImageFont.ImageFont, fill: tuple[int, int, int, int]) -> None:
    box = draw.textbbox((0, 0), text, font=text_font)
    draw.text((x - (box[2] - box[0]) * 0.5, y), text, font=text_font, fill=fill)


def apply_pipeline_palette(image: Image.Image) -> Image.Image:
    palette_image = Image.new("P", (1, 1))
    palette = [channel for color in PIPELINE_PALETTE for channel in color]
    palette += [0] * (768 - len(palette))
    palette_image.putpalette(palette)
    return image.convert("RGB").quantize(
        palette=palette_image,
        dither=Image.Dither.NONE,
    ).convert("RGBA")


def compose_model_views() -> None:
    paths = (FRONT_PATH, THREE_QUARTER_PATH, SIDE_PATH)
    missing = [path for path in paths if not path.exists()]
    if missing:
        raise SystemExit(f"missing 3D view render: {missing[0]}")
    views = [Image.open(path).convert("RGBA") for path in paths]
    canvas = Image.new("RGBA", (1440, 620), BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    title_font = font(32, bold=True)
    label_font = font(19, bold=True)
    note_font = font(16)
    draw.text((48, 28), "PAINTED 3D MODEL — THREE CAMERA ANGLES", font=title_font, fill=INK)
    draw.text((49, 76),
              "Broad value masks stay attached to the model. No screen-space texture and no camera-specific geometry.",
              font=note_font, fill=MUTED)
    labels = (("FRONT", TEAL), ("THREE-QUARTER", GOLD), ("SIDE", OXBLOOD))
    for index, (view, (label, color)) in enumerate(zip(views, labels)):
        x = index * 480
        canvas.alpha_composite(view, (x, 124))
        draw_centered(draw, x + 240, 106, label, label_font, color)
        draw.rectangle((x, 124, x + 479, 603), outline=(39, 43, 42, 255), width=1)
    VIEWS_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(VIEWS_PATH, quality=96)
    print(f"Composed 3D view sheet to {VIEWS_PATH}")


def main() -> None:
    if not HIGH_PATH.exists() or not LOW_PATH.exists():
        raise SystemExit("render the Blender experiment before composing it")

    high = Image.open(HIGH_PATH).convert("RGBA")
    low = Image.open(LOW_PATH).convert("RGBA")




    low_indexed = apply_pipeline_palette(low)



    high_strip = high.crop((0, 240, 1440, 555))
    low_strip = low_indexed.crop((0, 80, 480, 185)).resize((1440, 315), Image.Resampling.NEAREST)
    value_strip = ImageOps.grayscale(low_indexed.crop((0, 80, 480, 185)))
    value_strip = Image.merge("RGBA", (value_strip, value_strip, value_strip,
                                        Image.new("L", value_strip.size, 255)))
    value_strip = value_strip.resize((1440, 315), Image.Resampling.NEAREST)

    header_height = 128
    label_height = 76
    strip_height = 315
    gap = 28
    footer_height = 124
    height = header_height + (label_height + strip_height + gap) * 3 + footer_height
    canvas = Image.new("RGBA", (WIDTH, height), BACKGROUND)
    draw = ImageDraw.Draw(canvas)

    title_font = font(34, bold=True)
    section_font = font(20, bold=True)
    name_font = font(17, bold=True)
    note_font = font(17)
    small_font = font(15)

    draw.text((48, 28), "PAINTERLY SHADING EXPERIMENT V04", font=title_font, fill=INK)
    draw.text((49, 78), "Same 3D geometry and uniform light; only the authored model-space value masks change",
              font=note_font, fill=MUTED)
    draw.line((48, 112, WIDTH - 48, 112), fill=(50, 54, 53, 255), width=2)

    centers = (432, 720, 1008)
    names = (
        ("A  LIGHT ONLY", TEAL),
        ("B  BROAD PAINT", GOLD),
        ("C  PAINT + ACCENTS", OXBLOOD),
    )

    rows = (
        ("MODEL READ", "180 px tall — enough detail to inspect the source geometry", high_strip),
        ("PIPELINE READ", "60 art pixels, enlarged 3x with nearest-neighbor sampling", low_strip),
        ("VALUE READ", "same 60 art pixels in grayscale — silhouette and value grouping only", value_strip),
    )

    y = header_height
    for section, description, strip in rows:
        draw.text((48, y + 4), section, font=section_font, fill=INK)
        draw.text((238, y + 8), description, font=small_font, fill=MUTED)
        for center, (name, color) in zip(centers, names):
            draw_centered(draw, center, y + 43, name, name_font, color)
        y += label_height
        canvas.alpha_composite(strip, (0, y))
        draw.rectangle((0, y, WIDTH - 1, y + strip_height - 1), outline=(39, 43, 42, 255), width=1)
        y += strip_height + gap

    footer_y = height - footer_height
    draw.line((48, footer_y, WIDTH - 48, footer_y), fill=(50, 54, 53, 255), width=2)
    draw.text((48, footer_y + 25), "READ AT 60 PX", font=section_font, fill=INK)
    draw.text((238, footer_y + 27),
              "The diagonal tunic wedge and scarf lip must be authored paint; normal-based lighting cannot infer them.",
              font=note_font, fill=MUTED)
    draw.text((48, footer_y + 72),
              "Production path: encode 2–3 stable shade weights in vertex colors, then keep the normal light broad and weak",
              font=small_font, fill=TEAL)

    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(PREVIEW_PATH, quality=96)
    print(f"Composed experiment sheet to {PREVIEW_PATH}")
    compose_model_views()


if __name__ == "__main__":
    main()
