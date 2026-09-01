#!/usr/bin/env python3
"""Fail when the V05 screen-first character render is missing or blank."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_FRAME_ROOT = ROOT / "out" / "character-experiments" / "animation_v05"
EXPECTED_CLIPS = {
    "idle": range(1, 25),
    "walk": range(25, 49),
    "turn": range(49, 81),
}
EXPECTED_SIZE = (360, 360)


def is_character_color(pixel: tuple[int, int, int]) -> bool:
    """Keep the warm costume/skin and blue-green scarf, but reject the ground."""
    red, green, blue = pixel
    high = max(pixel)
    low = min(pixel)
    if high < 42 or high - low < 24:
        return False
    warm = red >= green * 1.12 and red >= blue * 1.12
    blue_green = (
        green >= red * 1.12
        and blue >= red * 1.12
        and abs(green - blue) <= max(12, high * 0.28)
    )
    return warm or blue_green


def subject_measurements(image: Image.Image) -> tuple[int, tuple[int, int, int, int] | None]:
    rgb = image.convert("RGB")
    points: list[tuple[int, int]] = []


    for y in range(25, 245):
        for x in range(70, 290):
            if is_character_color(rgb.getpixel((x, y))):
                points.append((x, y))
    if not points:
        return 0, None
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    return len(points), (min(xs), min(ys), max(xs), max(ys))


def validate_frame(path: Path) -> tuple[int, tuple[int, int, int, int]]:
    with Image.open(path) as image:
        if image.size != EXPECTED_SIZE:
            raise ValueError(f"{path}: expected {EXPECTED_SIZE}, got {image.size}")
        pixel_count, bounds = subject_measurements(image)
    if bounds is None:
        raise ValueError(f"{path}: no character-colored pixels found")
    left, top, right, bottom = bounds
    width = right - left + 1
    height = bottom - top + 1
    if pixel_count < 3000 or width < 55 or height < 120:
        raise ValueError(
            f"{path}: character coverage is too small "
            f"({pixel_count} pixels, {width}x{height} bounds)"
        )
    if not (95 <= left and right <= 265 and top < 100):
        raise ValueError(f"{path}: character is outside the expected stage area ({bounds})")
    return pixel_count, bounds


def validate_clip(frame_root: Path, clip: str, frames: range) -> None:
    expected = [frame_root / clip / f"frame_{frame:03d}.png" for frame in frames]
    missing = [path for path in expected if not path.is_file()]
    if missing:
        names = ", ".join(path.name for path in missing[:4])
        suffix = "..." if len(missing) > 4 else ""
        raise ValueError(f"{clip}: missing {len(missing)} frame(s): {names}{suffix}")

    unexpected = sorted((frame_root / clip).glob("frame_*.png"))
    if len(unexpected) != len(expected):
        raise ValueError(
            f"{clip}: expected exactly {len(expected)} PNG frames, found {len(unexpected)}"
        )

    measurements = [validate_frame(path) for path in expected]
    counts = [measurement[0] for measurement in measurements]
    print(
        f"PASS {clip}: {len(expected)} frames, "
        f"character coverage {min(counts)}-{max(counts)} pixels"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frame-root", type=Path, default=DEFAULT_FRAME_ROOT)
    args = parser.parse_args()
    try:
        for clip, frames in EXPECTED_CLIPS.items():
            validate_clip(args.frame_root, clip, frames)
    except ValueError as error:
        raise SystemExit(f"FAIL screen-first animation validation: {error}") from error
    print("PASS screen-first animation: all 80 frames contain the staged character")


if __name__ == "__main__":
    main()
