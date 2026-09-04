#!/usr/bin/env python3
"""Check measured game-size captures and compose the character review sheet."""
import argparse
import csv
from itertools import product
from pathlib import Path

from PIL import Image, ImageDraw


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("--baseline", type=Path)
    args = parser.parse_args()
    actors = ("hero", "guard")
    lights = ("day", "interior")
    heights = (35, 48, 60)
    with (args.directory / "measurements.csv").open() as source:
        rows = list(csv.DictReader(source))
    expected = set(product(actors, lights, heights, range(8)))
    found = set()
    for row in rows:
        key = (row["actor"], row["light"], int(row["height"]), int(row["frame"]))
        if key in found or key not in expected:
            raise ValueError(f"unexpected capture {key}")
        found.add(key)
        if abs(int(row["measured_height"]) - key[2]) > 2 or \
                int(row["visible_pixels"]) < 40:
            raise ValueError(f"character visibility check failed: {row}")
        path = args.directory / ("%s-%s-%d-%d.png" % key)
        with Image.open(path) as image:
            if image.size != (96, 96):
                raise ValueError(f"unexpected capture size: {path}")
    if found != expected:
        raise ValueError(f"missing captures: {expected - found}")

    variants = [("After", args.directory)]
    if args.baseline:
        variants.insert(0, ("Before", args.baseline))
    sheet = Image.new("RGB", (3 * len(variants) * 192, 4 * 214), (18, 20, 26))
    draw = ImageDraw.Draw(sheet)
    for row, (actor, light) in enumerate(product(actors, lights)):
        for size, height in enumerate(heights):
            for variant, (label, directory) in enumerate(variants):
                image = Image.open(directory / f"{actor}-{light}-{height}-0.png")
                x, y = (size * len(variants) + variant) * 192, row * 214
                sheet.paste(image.resize((192, 192), Image.Resampling.NEAREST), (x, y + 22))
                draw.text((x + 6, y + 5), f"{actor} / {light} / {height}px / {label}", fill="white")
    sheet.save(args.directory / "review.png")

    frames = []
    for frame in range(8):
        strip = Image.new("RGB", (4 * 192, 214), (18, 20, 26))
        draw = ImageDraw.Draw(strip)
        for index, (actor, light) in enumerate(product(actors, lights)):
            image = Image.open(args.directory / f"{actor}-{light}-60-{frame}.png")
            strip.paste(image.resize((192, 192), Image.Resampling.NEAREST), (index * 192, 22))
            draw.text((index * 192 + 6, 5), f"{actor} / {light} / 60px", fill="white")
        frames.append(strip)
    frames[0].save(args.directory / "walking-turns.gif", save_all=True,
                   append_images=frames[1:], duration=220, loop=0, disposal=2)
    print(f"PASS {len(found)} captures: both actors, both lights, 35/48/60 pixels")


if __name__ == "__main__":
    main()
