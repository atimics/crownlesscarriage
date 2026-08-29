#!/usr/bin/env python3
"""Validate the shipped rigid NPC module library."""

from __future__ import annotations

import json
from pathlib import Path
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from inspect_glb import accessor_first_values, collect_stats, parse_glb


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "assets" / "npc_dynamic_module_manifest.json"
EXPECTED_SLOTS = (
    "torso", "pelvis", "upper_arm", "forearm", "thigh", "shin",
    "hand", "foot", "head", "mantle", "coat_tail", "chest_plate",
    "pauldron", "apron", "pack", "satchel", "helmet", "hat", "hood",
    "headwrap",
    "tool_shaft", "tool_head",
) + tuple(f"hair_{index}" for index in range(8))


def validate() -> int:
    failures: list[str] = []
    if not MANIFEST.exists():
        print(f"FAIL: missing {MANIFEST}")
        return 1
    document = json.loads(MANIFEST.read_text(encoding="utf-8"))
    entries = document.get("modules", [])
    slots = tuple(entry.get("slot") for entry in entries)
    if slots != EXPECTED_SLOTS:
        failures.append(f"module slots {slots!r} != {EXPECTED_SLOTS!r}")
    if document.get("runtime_strategy") != \
            "bone-frame instancing without skins or animations":
        failures.append("runtime strategy contract changed")
    chest_entries = [entry for entry in entries
                     if entry.get("slot") == "chest_plate"]
    if len(chest_entries) != 1 or \
            chest_entries[0].get("shape_contract") != "closed torso volume":
        failures.append("chest plate must declare a closed torso volume")
    apron_entries = [entry for entry in entries if entry.get("slot") == "apron"]
    if len(apron_entries) != 1 or apron_entries[0].get("shape_contract") != \
            "fitted bib with split skirt":
        failures.append("apron must declare a fitted split-skirt volume")
    expected_exports = {Path(entry["export"]).name for entry in entries}
    export_dir = ROOT / "assets" / "exports" / "npc"
    shipped_exports = {
        path.name for path in export_dir.glob("npc_module_*.glb")
    }
    for name in sorted(shipped_exports - expected_exports):
        failures.append(f"stale module export is not in the manifest: {name}")
    for name in sorted(expected_exports - shipped_exports):
        failures.append(f"manifest module export is missing: {name}")
    total_triangles = 0
    for entry in entries:
        path = ROOT / entry["export"]
        if not path.exists():
            failures.append(f"{entry['slot']}: missing {path}")
            continue
        gltf, binary = parse_glb(path)
        stats = collect_stats(path, gltf)
        total_triangles += stats.triangles
        failures.extend(f"{entry['slot']}: {failure}"
                        for failure in stats.failures)
        if gltf.get("skins"):
            failures.append(f"{entry['slot']}: module contains a skin")
        if gltf.get("animations"):
            failures.append(f"{entry['slot']}: module contains animation")
        if stats.primitives != 1:
            failures.append(
                f"{entry['slot']}: {stats.primitives} primitives != 1")
        if tuple(stats.materials) != ("MAT_NPC_INDEXED",):
            failures.append(
                f"{entry['slot']}: indexed material contract {stats.materials!r}")
        primitives = [primitive for mesh in gltf.get("meshes", [])
                      for primitive in mesh.get("primitives", [])]
        if any("COLOR_0" not in primitive.get("attributes", {})
               for primitive in primitives):
            failures.append(
                f"{entry['slot']}: indexed primitive has no COLOR_0")
        for primitive in primitives:
            color = primitive.get("attributes", {}).get("COLOR_0")
            if color is None:
                continue
            sample = accessor_first_values(gltf, binary, color)
            if len(sample) < 3 or (abs(sample[0] - sample[1]) < 0.01 and
                                   abs(sample[1] - sample[2]) < 0.01):
                failures.append(
                    f"{entry['slot']}: COLOR_0 has no authored value/fold channels")
        if stats.triangles > 900:
            failures.append(
                f"{entry['slot']}: {stats.triangles} triangles > 900")
        if entry["slot"] == "chest_plate" and \
                stats.bounds_min and stats.bounds_max:
            width = stats.bounds_max[0] - stats.bounds_min[0]
            depth = stats.bounds_max[2] - stats.bounds_min[2]
            if depth < width * 0.28:
                failures.append(
                    f"chest_plate: depth {depth:.3f} is too flat for "
                    f"width {width:.3f}")
        print(f"{entry['slot']:<14} {stats.triangles:>4} tris  "
              f"{stats.vertices:>4} verts  {stats.primitives} material")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(f"validated {len(entries)} rigid modules, "
          f"{total_triangles} triangles total, zero skins/animations")
    return 0


if __name__ == "__main__":
    raise SystemExit(validate())
