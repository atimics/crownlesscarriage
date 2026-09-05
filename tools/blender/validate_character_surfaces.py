#!/usr/bin/env python3
"""Validate the opaque surface labels carried by the character pilot assets."""
from pathlib import Path
import json

from inspect_glb import accessor_values, parse_glb

ROOT = Path(__file__).resolve().parents[2]
SURFACES = ("skin", "cloth", "leather", "hair", "metal", "eye")


def validate(path: Path, expected: set[str]) -> None:
    document, binary = parse_glb(path)
    found: set[str] = set()
    for material in document.get("materials", []):
        if material.get("alphaMode", "OPAQUE") != "OPAQUE":
            raise ValueError(f"{path.name}: surface labels require opaque materials")
    for mesh in document["meshes"]:
        for primitive in mesh["primitives"]:
            color = primitive["attributes"].get("COLOR_0")
            if color is None:
                raise ValueError(f"{path.name}: COLOR_0 is required")
            for value in accessor_values(document, binary, color):
                if len(value) != 4:
                    raise ValueError(f"{path.name}: four paint channels are required")
                surface = int(value[3] * 8)
                if surface >= len(SURFACES) or surface < 0 or \
                        abs(value[3] - (surface + 0.5) / 8) > 1 / 255:
                    raise ValueError(f"{path.name}: invalid surface label {value[3]}")
                if min(abs(value[1] - band) for band in (0.25, 0.50, 0.75)) > 0.01:
                    raise ValueError(f"{path.name}: invalid painted shadow band")
                found.add(SURFACES[surface])
    if not expected.issubset(found):
        raise ValueError(f"{path.name}: expected {sorted(expected)}, found {sorted(found)}")
    print(f"PASS {path.name}: {', '.join(sorted(found))}")


def main() -> None:
    hero = ROOT / "assets/exports/hero/crownless_screen_first_engine_rig_v08.glb"
    hero_manifest = json.loads(hero.with_suffix(".json").read_text())
    modules = json.loads((ROOT / "assets/npc_dynamic_module_manifest.json").read_text())
    for manifest in (hero_manifest, modules):
        if manifest.get("surface_classes") != list(SURFACES):
            raise ValueError("surface class order must match the opaque shader contract")
    validate(hero, set(SURFACES))
    for style in ("cropped", "swept", "bob", "crest", "braided", "rear_lock"):
        validate(ROOT / f"assets/exports/world_kit/wk_hair_{style}_v01.glb",
                 {"hair"})
    for module in modules["modules"]:
        if module["slot"] in modules["surface_modules"]:
            expected = {"metal", "leather", "cloth"} \
                if module["slot"].startswith(("chest_plate", "pauldron")) else \
                {"leather"} if module["slot"] == "foot" else {"cloth"}
            validate(ROOT / module["export"], expected)


if __name__ == "__main__":
    main()
