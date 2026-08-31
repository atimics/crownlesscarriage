#!/usr/bin/env python3
"""Validate the shipped procedural creature manifest and GLB files."""

from __future__ import annotations

import json
from pathlib import Path
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from inspect_glb import accessor_first_values, collect_stats, parse_glb
from generate_creature_catalog import OUTPUT_PATH, render_catalog


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "creature_manifest.json"
EXPORT_DIR = ROOT / "assets" / "exports" / "creatures"
EXPECTED_VARIANTS = (
    "goblin_scavenger",
    "goblin_raider",
    "goblin_tribute_bearer",
    "horse",
    "cow",
    "dragon",
    "dragon_whelp",
    "dragon_wanderer",
    "dragon_deep_wyrm",
    "sheep",
)
EXPECTED_FAMILIES = ("goblin", "dragon", "animal")
EXPECTED_STEPPED_POSES = (
    "idle",
    "contact_a", "down_a", "passing_a", "up_a",
    "contact_b", "down_b", "passing_b", "up_b",
)
EXPECTED_DRAGON_POSES = ("idle", "stalk_a", "stalk_b", "threat", "rest")
EXPECTED_QUADRUPED_BONES = (
    "root", "body", "chest", "neck", "head",
    "upper_leg.FL", "lower_leg.FL", "hoof.FL",
    "upper_leg.FR", "lower_leg.FR", "hoof.FR",
    "upper_leg.HL", "lower_leg.HL", "hoof.HL",
    "upper_leg.HR", "lower_leg.HR", "hoof.HR",
    "tail.root", "tail",
)
EXPECTED_MATERIALS = ("MAT_CREATURE_INDEXED",)
EXPECTED_PALETTE = (
    "skin", "secondary", "hide", "cloth", "leather",
    "horn", "metal", "accent", "eye",
)
EXPECTED_MORPHOLOGY = {
    "goblin_scavenger": "biped",
    "goblin_raider": "biped",
    "goblin_tribute_bearer": "biped",
    "horse": "quadruped",
    "cow": "quadruped",
    "dragon": "quadruped",
    "dragon_whelp": "quadruped",
    "dragon_wanderer": "quadruped",
    "dragon_deep_wyrm": "quadruped",
    "sheep": "quadruped",
}
EXPECTED_GAIT = {
    "goblin_scavenger": "npc_stepped",
    "goblin_raider": "npc_stepped",
    "goblin_tribute_bearer": "npc_stepped",
    "horse": "quadruped_runtime_skin",
    "cow": "quadruped_runtime_skin",
    "dragon": "dragon_authored",
    "dragon_whelp": "dragon_authored",
    "dragon_wanderer": "dragon_authored",
    "dragon_deep_wyrm": "dragon_authored",
    "sheep": "quadruped_runtime_skin",
}
HEIGHT_LIMITS = {
    "goblin": (1.05, 1.70),
    "horse": (1.15, 2.00),
    "cow": (1.15, 1.90),
    "sheep": (0.75, 1.45),
    "dragon_whelp": (0.65, 2.80),
    "dragon_wanderer": (2.00, 8.40),
    "dragon": (3.60, 14.40),
    "dragon_deep_wyrm": (4.20, 26.00),
}
TRIANGLE_LIMITS = {
    "goblin": 2800,
    "horse": 3200,
    "cow": 3600,
    "sheep": 3600,
    "dragon": 7500,
}


def expected_pairs() -> tuple[tuple[str, str], ...]:
    pairs: list[tuple[str, str]] = []
    for variant in EXPECTED_VARIANTS:
        if variant.startswith("dragon"):
            poses = EXPECTED_DRAGON_POSES
        elif variant in ("horse", "cow", "sheep"):
            poses = ("idle",)
        else:
            poses = EXPECTED_STEPPED_POSES
        pairs.extend((variant, pose) for pose in poses)
    return tuple(pairs)


def validate() -> int:
    failures: list[str] = []
    if not MANIFEST_PATH.exists():
        print(f"FAIL: missing {MANIFEST_PATH}")
        return 1
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    expected_catalog = render_catalog(manifest)
    if not OUTPUT_PATH.exists():
        failures.append(f"missing generated runtime catalog {OUTPUT_PATH}")
    elif OUTPUT_PATH.read_text(encoding="utf-8") != expected_catalog:
        failures.append("generated runtime creature catalog is stale")
    entries = manifest.get("archetypes", [])
    pairs = tuple((entry.get("variant"), entry.get("pose"))
                  for entry in entries)
    if pairs != expected_pairs():
        failures.append("variant/pose order does not match the creature contract")
    if tuple(manifest.get("families", ())) != EXPECTED_FAMILIES:
        failures.append("family list changed")
    if tuple(manifest.get("variants", ())) != EXPECTED_VARIANTS:
        failures.append("variant list changed")
    if tuple(manifest.get("material_order", ())) != EXPECTED_PALETTE:
        failures.append("manifest material order changed")
    referenced_paths = {ROOT / entry["export"] for entry in entries}
    actual_paths = set(EXPORT_DIR.glob("*.glb"))
    for path in sorted(referenced_paths - actual_paths):
        failures.append(f"manifest references missing export {path.name}")
    for path in sorted(actual_paths - referenced_paths):
        failures.append(f"stale export is not in the manifest: {path.name}")

    total_triangles = 0
    idle_heights: dict[str, float] = {}
    dragon_idle_heights: dict[str, float] = {}
    dragon_idle_lengths: dict[str, float] = {}
    dragon_idle_snake_ratios: dict[str, float] = {}
    dragon_idle_triangles: dict[str, int] = {}
    for entry in entries:
        variant = entry.get("variant", "unknown")
        family = entry.get("family", "unknown")
        pose = entry.get("pose", "unknown")
        if entry.get("runtime_morphology") != EXPECTED_MORPHOLOGY.get(variant):
            failures.append(f"{variant}: wrong runtime morphology")
        if entry.get("gait_contract") != EXPECTED_GAIT.get(variant):
            failures.append(f"{variant}: wrong gait contract")
        if tuple(entry.get("material_order", ())) != EXPECTED_PALETTE:
            failures.append(f"{variant}: entry material order changed")
        path = ROOT / entry["export"]
        if not path.exists():
            failures.append(f"{variant}: missing {path}")
            continue
        document, binary = parse_glb(path)
        stats = collect_stats(path, document)
        total_triangles += stats.triangles
        if stats.failures:
            failures.extend(f"{variant}: {failure}"
                            for failure in stats.failures)
        if tuple(stats.materials) != EXPECTED_MATERIALS:
            failures.append(
                f"{variant}: material contract {stats.materials!r}")
        triangle_limit = TRIANGLE_LIMITS.get(family, 0)
        if stats.triangles > triangle_limit:
            failures.append(
                f"{variant}: {stats.triangles} triangles > {triangle_limit}")
        if stats.primitives != 1:
            failures.append(
                f"{variant}: {stats.primitives} primitives != 1")
        primitives = [primitive for mesh in document.get("meshes", [])
                      for primitive in mesh.get("primitives", [])]
        if any("COLOR_0" not in primitive.get("attributes", {})
               for primitive in primitives):
            failures.append(
                f"{variant}: indexed primitive has no COLOR_0")
        for primitive in primitives:
            color = primitive.get("attributes", {}).get("COLOR_0")
            if color is None:
                continue
            sample = accessor_first_values(document, binary, color)
            if len(sample) < 3 or (abs(sample[0] - sample[1]) < 0.01 and
                                   abs(sample[1] - sample[2]) < 0.01):
                failures.append(
                    f"{variant}: COLOR_0 has no authored value/fold channels")
        skinned = variant in ("horse", "cow", "sheep")
        if bool(entry.get("skinned")) != skinned:
            failures.append(f"{variant}: wrong skinned contract")
        skins = document.get("skins", [])
        if skinned:
            if tuple(entry.get("bones", ())) != EXPECTED_QUADRUPED_BONES:
                failures.append(f"{variant}: manifest bone contract changed")
            if len(skins) != 1:
                failures.append(f"{variant}: expected exactly one skin")
            else:
                nodes = document.get("nodes", [])
                joint_names = tuple(
                    nodes[index].get("name", "")
                    for index in skins[0].get("joints", [])
                )
                if (len(joint_names) != len(EXPECTED_QUADRUPED_BONES) or
                        set(joint_names) != set(EXPECTED_QUADRUPED_BONES)):
                    failures.append(
                        f"{variant}: exported bone contract {joint_names!r}")
            if any("JOINTS_0" not in primitive.get("attributes", {}) or
                   "WEIGHTS_0" not in primitive.get("attributes", {})
                   for primitive in primitives):
                failures.append(f"{variant}: skin weights are missing")
        elif skins:
            failures.append(f"{variant}: held-pose creature contains a skin")
        if document.get("animations"):
            failures.append(f"{variant}: creature contains baked animation")
        if stats.bounds_min and stats.bounds_max:
            height = stats.bounds_max[1] - stats.bounds_min[1]
            if pose == "idle":
                idle_heights[variant] = height
            if variant.startswith("dragon") and pose == "idle":
                length = stats.bounds_max[2] - stats.bounds_min[2]
                dragon_idle_heights[variant] = height
                dragon_idle_lengths[variant] = length
                dragon_idle_snake_ratios[variant] = length / height
                dragon_idle_triangles[variant] = stats.triangles
            minimum, maximum = HEIGHT_LIMITS.get(
                variant, HEIGHT_LIMITS.get(family, (0.0, 0.0)))
            if not minimum <= height <= maximum:
                failures.append(
                    f"{variant}: implausible height {height:.3f}m")
        print(f"{variant + '/' + pose:<36} {stats.triangles:>5} tris  "
              f"{stats.vertices:>5} verts  {stats.primitives} material")

    growth_order = (
        "dragon_whelp", "dragon_wanderer", "dragon", "dragon_deep_wyrm")
    length_growth_ratios = (4.00, 2.20, 2.10)
    if all(variant in dragon_idle_lengths for variant in growth_order):
        if dragon_idle_lengths["dragon_wanderer"] < 15.0:
            failures.append(
                "dragon_wanderer is smaller than the former deep wyrm")
        if dragon_idle_lengths["dragon"] < 38.0:
            failures.append("crowned dragon is not at least 38m long")
        if dragon_idle_lengths["dragon_deep_wyrm"] < 85.0:
            failures.append("ancient dragon is not at least 85m long")
        for index, minimum_ratio in enumerate(length_growth_ratios):
            young = dragon_idle_lengths[growth_order[index]]
            old = dragon_idle_lengths[growth_order[index + 1]]
            if old < young * minimum_ratio:
                failures.append(
                    f"dragon length {growth_order[index]} -> "
                    f"{growth_order[index + 1]} is only {old / young:.2f}x")
    if all(variant in dragon_idle_snake_ratios for variant in growth_order):
        for index in range(len(growth_order) - 1):
            young = dragon_idle_snake_ratios[growth_order[index]]
            old = dragon_idle_snake_ratios[growth_order[index + 1]]
            if old < young * 1.03:
                failures.append(
                    f"dragon shape {growth_order[index]} -> "
                    f"{growth_order[index + 1]} is not more serpentine")
    if (len(dragon_idle_triangles) == len(growth_order) and
            len(set(dragon_idle_triangles.values())) != len(growth_order)):
        failures.append("dragon stages reuse the same geometry topology")
    if "dragon_whelp" in idle_heights and "sheep" in idle_heights:
        whelp_display_height = idle_heights["dragon_whelp"] * 0.38 * 0.90
        sheep_display_height = idle_heights["sheep"] * 0.38
        difference = abs(sheep_display_height - whelp_display_height)
        if difference > whelp_display_height * 0.08:
            failures.append(
                "sheep display height does not match the baby dragon")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(f"validated {len(EXPECTED_VARIANTS)} variants / {len(entries)} "
          f"pose assets, {total_triangles} triangles total")
    return 0


if __name__ == "__main__":
    raise SystemExit(validate())
