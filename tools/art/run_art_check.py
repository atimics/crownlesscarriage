#!/usr/bin/env python3
"""Capture and validate the complete Crownless painterly art stack."""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import json
import math
from pathlib import Path
import re
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps, ImageStat


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = ROOT / "out" / "art-check"
WORLD_CROP = (17, 81, 931, 651)
ART_SIZE = (457, 285)
EXPECTED_SCREEN_SIZE = (1280, 760)
PALETTE_NEAR_DISTANCE = 0.055


@dataclass(frozen=True)
class CaptureSpec:
    slug: str
    label: str
    group: str
    arguments: tuple[str, ...]


ROOMS = (
    CaptureSpec("01-wayfarer-yard", "Wayfarer Yard", "room",
                ("--capture-room", "10.5", "7.5")),
    CaptureSpec("02-west-crofts", "West Crofts", "room",
                ("--capture-room", "11", "28.5")),
    CaptureSpec("03-old-mine-road", "Old Mine Road", "room",
                ("--capture-room", "14", "52")),
    CaptureSpec("04-artisan-row", "Artisan Row", "room",
                ("--capture-room", "33", "25")),
    CaptureSpec("05-mercercall-commons", "Mercercall Commons", "room",
                ("--capture-room", "44", "29")),
    CaptureSpec("06-coach-yard", "Coach Yard", "room",
                ("--capture-room", "42", "52")),
    CaptureSpec("07-market-steps", "Market Steps", "room",
                ("--capture-room", "50", "27.25")),
    CaptureSpec("08-millers-row", "Miller's Row", "room",
                ("--capture-room", "58", "50")),
    CaptureSpec("09-crown-gate", "Crown Gate", "room",
                ("--capture-room", "78.5", "29")),
    CaptureSpec("10-east-fields", "East Fields", "room",
                ("--capture-room", "78", "50")),
)

SCENES = (
    CaptureSpec("street", "Street", "scene", ("--capture-golden",)),
    CaptureSpec("road", "Road", "scene", ("--capture-road",)),
    CaptureSpec("interior", "Interior", "scene", ("--capture-interior",)),
    CaptureSpec("parley", "Parley", "scene", ("--capture-parley",)),
)

ATMOSPHERES = (
    CaptureSpec("atmosphere-clear", "Clear Day", "atmosphere",
                ("--capture-atmosphere", "clear")),
    CaptureSpec("atmosphere-rain", "Rainy Overcast", "atmosphere",
                ("--capture-atmosphere", "rain")),
    CaptureSpec("atmosphere-dusk", "Amber Dusk", "atmosphere",
                ("--capture-atmosphere", "dusk")),
    CaptureSpec("atmosphere-night", "Moonlit Night", "atmosphere",
                ("--capture-atmosphere", "night")),
    CaptureSpec("atmosphere-omen", "Dragon Omen", "atmosphere",
                ("--capture-atmosphere", "omen")),
)

DRAGON_SCENES = (
    CaptureSpec("dragon-cave-world", "Dragon Cave World", "dragon",
                ("--capture-creatures", "dragon")),
    CaptureSpec("dragon-cave-state", "Dragon Cave State", "dragon",
                ("--capture-dragon-cave",)),
)

NPC_REVIEWS = (
    CaptureSpec("npc-model-review", "NPC Model Review", "npc",
                ("--capture-npc-review",)),
)


def image_pixels(image: Image.Image) -> list[tuple[int, ...] | int]:
    getter = getattr(image, "get_flattened_data", None)
    return list(getter() if getter is not None else image.getdata())


def find_app(explicit: Path | None) -> Path:
    candidates = [
        explicit,
        ROOT / "out" / "build" / "play" /
            "crownless_carriage.app" / "Contents" / "MacOS" /
            "crownless_carriage",
        ROOT / "out" / "build" / "play" / "crownless_carriage",
    ]
    for candidate in candidates:
        if candidate is not None and candidate.is_file():
            return candidate.resolve()
    raise ValueError("built Crownless client was not found; run make build-play first")


def relative_capture_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError as error:
        raise ValueError("art-check output must stay inside the repository") from error


def run_capture(app: Path, arguments: tuple[str, ...], output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [str(app), *arguments, relative_capture_path(output)]
    result = subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=90,
        check=False,
    )
    if result.returncode != 0 or not output.is_file():
        tail = "\n".join(result.stdout.splitlines()[-18:])
        raise ValueError(
            f"capture failed ({' '.join(arguments)}): exit {result.returncode}\n{tail}"
        )


def crop_world(path: Path) -> Image.Image:
    with Image.open(path) as source:
        image = source.convert("RGB")
    if image.size != EXPECTED_SCREEN_SIZE:
        raise ValueError(
            f"{path.name}: expected screen {EXPECTED_SCREEN_SIZE}, got {image.size}"
        )
    return image.crop(WORLD_CROP).resize(ART_SIZE, Image.Resampling.NEAREST)


def read_authored_palette() -> list[tuple[int, int, int]]:
    source = (ROOT / "src" / "client" / "cc_visual_style.h").read_text()
    marker = "static const CcVisualPalette CC_VISUAL_PALETTE = {"
    if marker not in source:
        raise ValueError("authored visual palette was not found")
    block = source.split(marker, 1)[1].split("};", 1)[0]
    colors = [
        tuple(map(int, match[:3]))
        for match in re.findall(
            r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}",
            block,
        )
    ]
    if len(colors) < 24:
        raise ValueError(f"authored palette is unexpectedly small ({len(colors)} colors)")
    return list(dict.fromkeys(colors))


def srgb_channel_to_linear(channel: int) -> float:
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else \
        ((value + 0.055) / 1.055) ** 2.4


def rgb_to_oklab(color: tuple[int, int, int]) -> tuple[float, float, float]:
    red, green, blue = (srgb_channel_to_linear(channel) for channel in color)
    long_wave = 0.4122214708 * red + 0.5363325363 * green + \
        0.0514459929 * blue
    medium_wave = 0.2119034982 * red + 0.6806995451 * green + \
        0.1073969566 * blue
    short_wave = 0.0883024619 * red + 0.2817188376 * green + \
        0.6299787005 * blue
    long_wave = math.copysign(abs(long_wave) ** (1.0 / 3.0), long_wave)
    medium_wave = math.copysign(abs(medium_wave) ** (1.0 / 3.0), medium_wave)
    short_wave = math.copysign(abs(short_wave) ** (1.0 / 3.0), short_wave)
    return (
        0.2104542553 * long_wave + 0.7936177850 * medium_wave -
            0.0040720468 * short_wave,
        1.9779984951 * long_wave - 2.4285922050 * medium_wave +
            0.4505937099 * short_wave,
        0.0259040371 * long_wave + 0.7827717662 * medium_wave -
            0.8086757660 * short_wave,
    )


def oklab_distance(source: tuple[float, float, float],
                   candidate: tuple[float, float, float]) -> float:
    lightness = (source[0] - candidate[0]) * 1.24
    green_red = source[1] - candidate[1]
    blue_yellow = source[2] - candidate[2]
    return math.sqrt(lightness * lightness + green_red * green_red +
                     blue_yellow * blue_yellow)


def palette_metrics(image: Image.Image,
                    palette: list[tuple[int, int, int]]) -> dict[str, float]:
    counts = Counter(image_pixels(image.convert("RGB")))
    palette_set = set(palette)
    perceptual_palette = [rgb_to_oklab(color) for color in palette]
    total = sum(counts.values())
    exact = sum(count for color, count in counts.items() if color in palette_set)
    near = 0
    weighted_distance = 0.0
    maximum_distance = 0.0
    for color, count in counts.items():
        perceptual_color = rgb_to_oklab(color)
        distance = min(oklab_distance(perceptual_color, candidate)
                       for candidate in perceptual_palette)
        weighted_distance += distance * count
        maximum_distance = max(maximum_distance, distance)
        if distance <= PALETTE_NEAR_DISTANCE:
            near += count
    return {
        "exact_ratio": exact / total,
        "near_ratio": near / total,
        "mean_distance": weighted_distance / total,
        "maximum_distance": maximum_distance,
    }


def composition_metrics(image: Image.Image) -> dict[str, float | int]:
    rgb = image.convert("RGB")
    grayscale = ImageOps.grayscale(rgb)
    pixels = image_pixels(rgb)
    counts = Counter(pixels)
    dominant_ratio = counts.most_common(1)[0][1] / len(pixels)
    luminance_std = ImageStat.Stat(grayscale).stddev[0] / 255.0

    edges = grayscale.filter(ImageFilter.FIND_EDGES).crop(
        (1, 1, grayscale.width - 1, grayscale.height - 1)
    )
    edge_pixels = image_pixels(edges)
    edge_density = sum(value > 20 for value in edge_pixels) / len(edge_pixels)

    local_values: list[float] = []
    for top in range(0, grayscale.height, 32):
        for left in range(0, grayscale.width, 32):
            tile = grayscale.crop((
                left,
                top,
                min(left + 32, grayscale.width),
                min(top + 32, grayscale.height),
            ))
            local_values.append(ImageStat.Stat(tile).stddev[0] / 255.0)

    center = rgb.crop((
        rgb.width // 4,
        rgb.height // 4,
        rgb.width * 3 // 4,
        rgb.height * 3 // 4,
    ))
    return {
        "unique_colors": len(counts),
        "center_unique_colors": len(set(image_pixels(center))),
        "dominant_color_ratio": dominant_ratio,
        "luminance_stddev": luminance_std,
        "edge_density": edge_density,
        "mean_local_contrast": sum(local_values) / len(local_values),
        "peak_local_contrast": max(local_values),
    }


def derive_views(image: Image.Image, output_root: Path, slug: str) -> dict[str, Path]:
    paths: dict[str, Path] = {}
    color = image.convert("RGB")
    grayscale = ImageOps.grayscale(color)

    outputs = {
        "color": color,
        "grayscale": grayscale.convert("RGB"),
        "silhouette": grayscale.point(
            lambda value: 15 if value < 112 else 226
        ).convert("RGB"),
        "three-value": grayscale.point(
            lambda value: 15 if value < 85 else (102 if value < 170 else 226)
        ).convert("RGB"),
    }
    for view, derived in outputs.items():
        path = output_root / "views" / view / f"{slug}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        derived.save(path)
        paths[view] = path
    return paths


def contact_sheet(items: list[tuple[str, Path]], output: Path,
                  columns: int, thumb_size: tuple[int, int] = (228, 142)) -> None:
    if not items:
        return
    rows = math.ceil(len(items) / columns)
    label_height = 22
    gutter = 6
    cell_width = thumb_size[0] + gutter * 2
    cell_height = thumb_size[1] + label_height + gutter * 2
    sheet = Image.new("RGB", (columns * cell_width, rows * cell_height),
                      (15, 16, 18))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for index, (label, path) in enumerate(items):
        with Image.open(path) as source:
            thumbnail = ImageOps.contain(
                source.convert("RGB"), thumb_size, Image.Resampling.NEAREST
            )
        column = index % columns
        row = index // columns
        x = column * cell_width + gutter + (thumb_size[0] - thumbnail.width) // 2
        y = row * cell_height + gutter
        sheet.paste(thumbnail, (x, y))
        draw.text((column * cell_width + gutter, y + thumb_size[1] + 5),
                  label, fill=(226, 216, 193), font=font)
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)


def is_character_color(pixel: tuple[int, int, int]) -> bool:
    red, green, blue = pixel
    high = max(pixel)
    low = min(pixel)
    if high < 42 or high - low < 24:
        return False
    warm = red >= green * 1.12 and red >= blue * 1.12
    blue_green = (
        green >= red * 1.12 and blue >= red * 1.12 and
        abs(green - blue) <= max(12, high * 0.28)
    )
    return warm or blue_green


def character_color_bounds(image: Image.Image) -> tuple[int, int, int, int] | None:
    rgb = image.convert("RGB")
    x_start = 70 if rgb.width >= 300 else 0
    x_end = min(290, rgb.width) if rgb.width >= 300 else rgb.width
    y_start = 25 if rgb.height >= 300 else 0
    y_end = min(330, rgb.height) if rgb.height >= 300 else rgb.height
    points = [
        (x, y)
        for y in range(y_start, y_end)
        for x in range(x_start, x_end)
        if is_character_color(rgb.getpixel((x, y)))
    ]
    if not points:
        return None
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    return min(xs), min(ys), max(xs), max(ys)


def build_character_comparison(output_root: Path) -> dict[str, object]:
    source_path = ROOT / "out" / "character-experiments" / \
        "animation_v05" / "idle" / "frame_001.png"
    if not source_path.is_file():
        raise ValueError(
            "V05 idle frame is missing; run make blender-character-animations first"
        )
    with Image.open(source_path) as source:
        source_rgb = source.convert("RGB")
    bounds = character_color_bounds(source_rgb)
    if bounds is None:
        raise ValueError("V05 character comparison source is blank")
    left, top, right, bottom = bounds
    subject_height = bottom - top + 1
    crop_box = (
        max(0, left - 12), max(0, top - 10),
        min(source_rgb.width, right + 13), min(source_rgb.height, bottom + 12),
    )
    crop = source_rgb.crop(crop_box)
    panels: list[tuple[str, Path]] = []
    measured: dict[str, int] = {}
    for target_height in (35, 48, 60):
        scale = target_height / subject_height
        panel = Image.new("RGB", (120, 96), (15, 16, 18))
        measured_height = 0
        for _ in range(4):
            resized = crop.resize(
                (max(1, round(crop.width * scale)),
                 max(1, round(crop.height * scale))),
                Image.Resampling.NEAREST,
            )
            panel = Image.new("RGB", (120, 96), (15, 16, 18))
            panel.paste(resized, ((panel.width - resized.width) // 2,
                                  panel.height - resized.height - 18))
            measured_bounds = character_color_bounds(panel)
            measured_height = 0 if measured_bounds is None else \
                measured_bounds[3] - measured_bounds[1] + 1
            if measured_height == target_height or measured_height == 0:
                break
            scale *= target_height / measured_height
        draw = ImageDraw.Draw(panel)
        draw.text((6, 80), f"{target_height} ART PIXELS",
                  fill=(226, 216, 193), font=ImageFont.load_default())
        panel_path = output_root / "character" / f"hero-{target_height}px.png"
        panel_path.parent.mkdir(parents=True, exist_ok=True)
        panel.save(panel_path)
        measured[str(target_height)] = measured_height
        panels.append((f"{target_height}px", panel_path))
    comparison = output_root / "character" / "hero-height-comparison.png"
    contact_sheet(panels, comparison, 3, thumb_size=(120, 96))
    return {
        "source": str(source_path.relative_to(ROOT)),
        "source_subject_height": subject_height,
        "requested_heights": [35, 48, 60],
        "measured_heights": measured,
        "comparison": str(comparison.relative_to(ROOT)),
    }


def flicker_metrics(paths: list[Path]) -> dict[str, object]:
    frames = [crop_world(path) for path in paths]
    comparisons: list[dict[str, float | int]] = []
    for index in range(1, len(frames)):
        difference = ImageChops.difference(frames[0], frames[index])
        pixels = image_pixels(difference)
        changed = sum(any(channel != 0 for channel in pixel) for pixel in pixels)
        channel_total = sum(sum(pixel) for pixel in pixels)
        maximum = max(max(pixel) for pixel in pixels)
        comparisons.append({
            "frame": index + 1,
            "changed_pixel_ratio": changed / len(pixels),
            "mean_channel_delta": channel_total / (len(pixels) * 3),
            "maximum_channel_delta": maximum,
        })
    passed = all(
        item["changed_pixel_ratio"] <= 0.005 and
        item["mean_channel_delta"] <= 1.0
        for item in comparisons
    )
    return {"passed": passed, "comparisons": comparisons}


def capture_passes(palette: dict[str, float],
                   composition: dict[str, float | int]) -> tuple[bool, list[str]]:
    reasons: list[str] = []
    if palette["exact_ratio"] < 0.82:
        reasons.append("too many exact off-palette pixels")
    if palette["near_ratio"] < 0.985:
        reasons.append("world colors drift too far from the shared palette")
    if composition["dominant_color_ratio"] > 0.86:
        reasons.append("one color covers almost the whole frame")
    if composition["luminance_stddev"] < 0.04:
        reasons.append("frame has too little value separation")
    if composition["edge_density"] < 0.01:
        reasons.append("frame has too little visible subject coverage")
    if composition["center_unique_colors"] < 6:
        reasons.append("center story area appears blank")
    return not reasons, reasons


def write_report(output_root: Path, capture_results: list[dict[str, object]],
                 flicker: dict[str, object], character: dict[str, object],
                 failures: list[str], capture_mode: str) -> None:
    report = {
        "status": "PASS" if not failures else "FAIL",
        "capture_mode": capture_mode,
        "world_crop": list(WORLD_CROP),
        "art_size": list(ART_SIZE),
        "thresholds": {
            "palette_exact_ratio": 0.82,
            "palette_near_ratio": 0.985,
            "palette_near_distance": PALETTE_NEAR_DISTANCE,
            "maximum_dominant_color_ratio": 0.86,
            "minimum_luminance_stddev": 0.04,
            "minimum_edge_density": 0.01,
            "flicker_changed_pixel_ratio": 0.005,
            "flicker_mean_channel_delta": 1.0,
        },
        "captures": capture_results,
        "stationary_flicker": flicker,
        "character_comparison": character,
        "failures": failures,
    }
    json_path = output_root / "report.json"
    json_path.write_text(json.dumps(report, indent=2) + "\n")

    lines = [
        "# Crownless art check",
        "",
        f"Overall: **{report['status']}**",
        "",
        f"Capture mode: **{capture_mode}**",
        "",
        "The checks use only the 457 x 285 world target. UI outside the stage is excluded.",
        "",
        "| View | Palette exact | Palette near | Dominant | Edges | Local contrast | Result |",
        "| --- | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for result in capture_results:
        palette = result["palette"]
        composition = result["composition"]
        lines.append(
            f"| {result['label']} | {palette['exact_ratio']:.1%} | "
            f"{palette['near_ratio']:.1%} | "
            f"{composition['dominant_color_ratio']:.1%} | "
            f"{composition['edge_density']:.1%} | "
            f"{composition['mean_local_contrast']:.3f} | "
            f"{'PASS' if result['passed'] else 'FAIL'} |"
        )
    lines.extend([
        "",
        f"Stationary flicker: **{'PASS' if flicker['passed'] else 'FAIL'}**",
        "",
        "Character comparisons: 35, 48, and 60 art pixels are in "
        "`character/hero-height-comparison.png`.",
        "",
        "The focused NPC model review is in "
        "`contact-sheets/npc-model-review.png`.",
        "",
        "Review sheets are in `contact-sheets/`. Every capture also has color, "
        "grayscale, silhouette, and three-value files in `views/`.",
    ])
    if failures:
        lines.extend(["", "## Failures", ""])
        lines.extend(f"- {failure}" for failure in failures)
    (output_root / "report.md").write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--reuse-captures",
        action="store_true",
        help="validate the current art-check PNGs without opening the client",
    )
    args = parser.parse_args()

    output_root = args.output.resolve()
    relative_capture_path(output_root)
    app = find_app(args.app.resolve() if args.app is not None else None)
    palette = read_authored_palette()
    all_specs = (*ROOMS, *SCENES, *ATMOSPHERES, *DRAGON_SCENES, *NPC_REVIEWS)
    capture_paths: dict[str, Path] = {}

    try:
        for index, spec in enumerate(all_specs, start=1):
            path = output_root / "captures" / f"{spec.slug}.png"
            capture_paths[spec.slug] = path
            if not args.reuse_captures:
                run_capture(app, spec.arguments, path)
            elif not path.is_file():
                raise ValueError(f"missing reused capture {path}")
            print(f"CAPTURE {index:02d}/{len(all_specs)} {spec.label}")

        flicker_paths: list[Path] = []
        for frame in range(1, 4):
            path = output_root / "captures" / "flicker" / \
                f"stationary-{frame:02d}.png"
            flicker_paths.append(path)
            if not args.reuse_captures:
                run_capture(
                    app, ("--capture-room", "50", "27.25"), path
                )
            elif not path.is_file():
                raise ValueError(f"missing reused flicker capture {path}")
            print(f"FLICKER FRAME {frame}/3")

        failures: list[str] = []
        capture_results: list[dict[str, object]] = []
        view_paths: dict[str, dict[str, Path]] = {}
        for spec in all_specs:
            world = crop_world(capture_paths[spec.slug])
            views = derive_views(world, output_root, spec.slug)
            view_paths[spec.slug] = views
            palette_result = palette_metrics(world, palette)
            composition_result = composition_metrics(world)
            passed, reasons = capture_passes(palette_result, composition_result)
            if not passed:
                failures.extend(f"{spec.label}: {reason}" for reason in reasons)
            capture_results.append({
                "slug": spec.slug,
                "label": spec.label,
                "group": spec.group,
                "capture": str(capture_paths[spec.slug].relative_to(ROOT)),
                "views": {
                    key: str(path.relative_to(ROOT))
                    for key, path in views.items()
                },
                "palette": palette_result,
                "composition": composition_result,
                "passed": passed,
                "reasons": reasons,
            })

        clear_atmosphere = crop_world(capture_paths["atmosphere-clear"])
        for spec in ATMOSPHERES[1:]:
            mood = crop_world(capture_paths[spec.slug])
            difference = ImageChops.difference(clear_atmosphere, mood)
            difference_pixels = image_pixels(difference)
            changed = sum(
                any(channel != 0 for channel in pixel)
                for pixel in difference_pixels
            ) / len(difference_pixels)
            mean_delta = sum(
                sum(pixel) for pixel in difference_pixels
            ) / (len(difference_pixels) * 3)
            if changed < 0.35 or mean_delta < 6.0:
                failures.append(
                    f"{spec.label}: mood is not distinct from clear day "
                    f"({changed:.1%} changed, {mean_delta:.1f} mean delta)"
                )

        flicker = flicker_metrics(flicker_paths)
        if not flicker["passed"]:
            failures.append("stationary world target changes between repeat captures")
        character = build_character_comparison(output_root)
        for requested, measured in character["measured_heights"].items():
            if abs(int(requested) - measured) > 2:
                failures.append(
                    f"{requested}px character comparison measures {measured}px"
                )

        contact_sheet(
            [(spec.label, view_paths[spec.slug]["color"]) for spec in ROOMS],
            output_root / "contact-sheets" / "all-camera-rooms.png", 5,
        )
        contact_sheet(
            [(spec.label, view_paths[spec.slug]["color"]) for spec in SCENES],
            output_root / "contact-sheets" / "street-road-interior-parley.png", 4,
        )
        contact_sheet(
            [(spec.label, view_paths[spec.slug]["color"])
             for spec in ATMOSPHERES],
            output_root / "contact-sheets" / "time-and-weather.png", 4,
        )
        contact_sheet(
            [(spec.label, view_paths[spec.slug]["color"])
             for spec in DRAGON_SCENES],
            output_root / "contact-sheets" / "dragon-cave.png", 1,
        )
        contact_sheet(
            [(spec.label, view_paths[spec.slug]["color"])
             for spec in NPC_REVIEWS],
            output_root / "contact-sheets" / "npc-model-review.png", 1,
        )
        contact_sheet(
            [(view.replace("-", " ").title(), view_paths["street"][view])
             for view in ("color", "grayscale", "silhouette", "three-value")],
            output_root / "contact-sheets" / "street-value-study.png", 4,
        )
        write_report(
            output_root, capture_results, flicker, character, failures,
            "reused" if args.reuse_captures else "fresh",
        )
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f"FAIL art-check: {error}") from error

    if failures:
        print(f"FAIL art-check: {len(failures)} issue(s); see {output_root / 'report.md'}")
        raise SystemExit(1)
    print(
        "PASS art-check: 10 rooms, 4 scenes, 5 atmosphere moods, dragon cave, "
        "NPC model review, value studies, character sizes, and flicker"
    )
    print(f"REPORT {output_root / 'report.md'}")


if __name__ == "__main__":
    main()
