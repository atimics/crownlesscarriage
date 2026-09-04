#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys

from PIL import Image, ImageDraw


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import compose_screen_first_character_experiments as style


ROOT = Path(__file__).resolve().parents[2]
FRAME_ROOT = ROOT / "out" / "character-experiments" / "animation_v05"
PREVIEW_ROOT = ROOT / "assets" / "previews" / "experiments"

CLIPS = {
    "idle": "IDLE / BREATHING",
    "walk": "WALK / WEIGHT SHIFT",
    "turn": "TURN / VOLUME CHECK",
}

CANVAS_SIZE = (760, 438)
PANEL_SIZE = 360


def compose_frame(source: Image.Image, clip_title: str) -> Image.Image:
    source = source.convert("RGBA")
    small = source.resize((90, 90), Image.Resampling.BOX)
    small = style.apply_pipeline_palette(small)
    pixel = small.resize((PANEL_SIZE, PANEL_SIZE), Image.Resampling.NEAREST)

    canvas = Image.new("RGBA", CANVAS_SIZE, style.BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    title_font = style.font(21, bold=True)
    label_font = style.font(15, bold=True)
    draw.text((20, 14), clip_title, font=title_font, fill=style.INK)
    style.draw_centered(draw, 200, 48, "SOURCE 3D", label_font, style.GOLD)
    style.draw_centered(draw, 560, 48, "~60 ART PIXELS", label_font, style.TEAL)
    canvas.alpha_composite(source, (20, 68))
    canvas.alpha_composite(pixel, (380, 68))
    draw.rectangle((20, 68, 379, 427), outline=(48, 52, 50, 255), width=1)
    draw.rectangle((380, 68, 739, 427), outline=(48, 52, 50, 255), width=1)
    return canvas.convert("RGB")


def save_gif(clip: str, title: str) -> Path:
    paths = sorted((FRAME_ROOT / clip).glob("frame_*.png"))
    if not paths:
        raise SystemExit(f"no rendered frames found for {clip}")
    frames = [compose_frame(Image.open(path), title) for path in paths]
    first = frames[0].convert("P", palette=Image.Palette.ADAPTIVE, colors=128)
    palette_frames = [first]
    for frame in frames[1:]:
        palette_frames.append(frame.quantize(palette=first, dither=Image.Dither.NONE))
    output = PREVIEW_ROOT / f"screen_first_character_{clip}_v05.gif"
    PREVIEW_ROOT.mkdir(parents=True, exist_ok=True)
    palette_frames[0].save(
        output,
        save_all=True,
        append_images=palette_frames[1:],
        duration=83,
        loop=0,
        disposal=2,
        optimize=False,
    )
    print(f"Composed {clip} animation to {output}")
    return output


def main() -> None:
    for clip, title in CLIPS.items():
        save_gif(clip, title)


if __name__ == "__main__":
    main()
