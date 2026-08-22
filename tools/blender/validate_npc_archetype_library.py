#!/usr/bin/env python3
"""Validate the shipped procedural NPC manifest and GLB files."""

from __future__ import annotations

import json
from pathlib import Path
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from inspect_glb import collect_stats, parse_glb


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "npc_archetype_manifest.json"
EXPECTED_ROLES = (
    "wayfarer", "guard", "raider", "merchant", "laborer",
    "traveller", "refugee", "scout", "healer",
)
EXPECTED_POSES = (
    "idle",
    "contact_l", "down_l", "passing_l", "up_l",
    "contact_r", "down_r", "passing_r", "up_r",
)
EXPECTED_MATERIALS = ("MAT_NPC_INDEXED",)
EXPECTED_PALETTE = (
    "skin", "hair", "underlayer", "outer", "trousers",
    "leather", "metal", "accent", "eye",
)


def validate() -> int:
    failures: list[str] = []
    if not MANIFEST_PATH.exists():
        print(f"FAIL: missing {MANIFEST_PATH}")
        return 1
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    entries = manifest.get("archetypes", [])
    expected_pairs = tuple((role, pose) for role in EXPECTED_ROLES
                           for pose in EXPECTED_POSES)
    pairs = tuple((entry.get("role"), entry.get("motion_pose"))
                  for entry in entries)
    if pairs != expected_pairs:
        failures.append(f"role/pose order {pairs!r} != {expected_pairs!r}")
    indices = tuple(entry.get("role_index") for entry in entries)
    expected_indices = tuple(index for index in range(len(EXPECTED_ROLES))
                             for _pose in EXPECTED_POSES)
    if indices != expected_indices:
        failures.append(f"role indices do not match pose families: {indices!r}")
    if tuple(manifest.get("material_order", ())) != EXPECTED_PALETTE:
        failures.append("manifest material order changed")

    total_triangles = 0
    for entry in entries:
        path = ROOT / entry["export"]
        if not path.exists():
            failures.append(f"{entry['role']}: missing {path}")
            continue
        document, _binary = parse_glb(path)
        stats = collect_stats(path, document)
        total_triangles += stats.triangles
        if stats.failures:
            failures.extend(f"{entry['role']}: {failure}"
                            for failure in stats.failures)
        if tuple(stats.materials) != EXPECTED_MATERIALS:
            failures.append(
                f"{entry['role']}: material contract {stats.materials!r}")
        if stats.triangles > 6500:
            failures.append(
                f"{entry['role']}: {stats.triangles} triangles > 6500")
        if stats.primitives != 1:
            failures.append(
                f"{entry['role']}: {stats.primitives} primitives != 1")
        primitives = [primitive for mesh in document.get("meshes", [])
                      for primitive in mesh.get("primitives", [])]
        if any("COLOR_0" not in primitive.get("attributes", {})
               for primitive in primitives):
            failures.append(
                f"{entry['role']}: indexed primitive has no COLOR_0")
        if document.get("skins"):
            failures.append(f"{entry['role']}: static archetype contains a skin")
        if document.get("animations"):
            failures.append(f"{entry['role']}: static archetype contains animation")
        if stats.bounds_min and stats.bounds_max:
            height = stats.bounds_max[1] - stats.bounds_min[1]
            if not 1.80 <= height <= 2.35:
                failures.append(
                    f"{entry['role']}: implausible height {height:.3f}m")
        print(f"{entry['role'] + '/' + entry['motion_pose']:<18} "
              f"{stats.triangles:>5} tris  "
              f"{stats.vertices:>5} verts  {stats.primitives} materials")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(f"validated {len(EXPECTED_ROLES)} role families / {len(entries)} "
          f"pose assets, {total_triangles} triangles total")
    return 0


if __name__ == "__main__":
    raise SystemExit(validate())
