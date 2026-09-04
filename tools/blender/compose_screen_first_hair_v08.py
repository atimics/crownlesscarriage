#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
FRAME_DIR = ROOT / "out" / "character-experiments" / "hair_v08" / "walk"
PREVIEW_DIR = ROOT / "assets" / "previews" / "experiments"
SHEET_PATH = PREVIEW_DIR / "screen_first_hair_walk_sheet_v08.png"
GIF_PATH = PREVIEW_DIR / "screen_first_hair_walk_v08.gif"
BACKGROUND = (13, 16, 18)


def frames() -> list[Image.Image]:
    paths = sorted(FRAME_DIR.glob("frame_*.png"))
    if len(paths) != 16:
        raise ValueError(f"expected 16 V08 walk frames, found {len(paths)}")
    return [Image.open(path).convert("RGB") for path in paths]


def save_sheet(images: list[Image.Image]) -> None:
    selected = images[::2]
    sheet = Image.new("RGB", (960, 510), BACKGROUND)
    draw = ImageDraw.Draw(sheet)
    draw.text((14, 8), "CROWNLESS V08 / TAPERED HAIR WALK", fill=(226, 216, 193))
    for index, frame in enumerate(selected):
        x = (index % 4) * 240
        y = 30 + (index // 4) * 240
        sheet.paste(frame, (x, y))
    sheet.save(SHEET_PATH)


def save_gif(images: list[Image.Image]) -> None:
    first = images[0].convert("P", palette=Image.Palette.ADAPTIVE, colors=128)
    rest = [image.quantize(palette=first, dither=Image.Dither.NONE)
            for image in images[1:]]
    first.save(
        GIF_PATH,
        save_all=True,
        append_images=rest,
        duration=83,
        loop=0,
        disposal=2,
        optimize=False,
    )


def main() -> None:
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    images = frames()
    save_sheet(images)
    save_gif(images)
    print(f"composed V08 hair walk previews to {PREVIEW_DIR}")


if __name__ == "__main__":
    main()
