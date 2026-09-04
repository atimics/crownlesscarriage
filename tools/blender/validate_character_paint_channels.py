#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import sys


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from inspect_glb import accessor_first_values, parse_glb


ROOT = Path(__file__).resolve().parents[2]
HEROES = (
    ROOT / "assets" / "exports" / "hero" /
        "crownless_hero_engine_rig_v01.glb",
    ROOT / "assets" / "exports" / "hero" /
        "crownless_screen_first_engine_rig_v08.glb",
)


def validate(path: Path) -> None:
    if not path.is_file():
        raise ValueError(f"missing {path}")
    document, binary = parse_glb(path)
    if not document.get("skins"):
        raise ValueError(f"{path.name}: runtime hero has no skin")
    primitives = [primitive for mesh in document.get("meshes", [])
                  for primitive in mesh.get("primitives", [])]
    if not primitives:
        raise ValueError(f"{path.name}: runtime hero has no primitives")
    samples: list[tuple[float, ...]] = []
    for primitive in primitives:
        color = primitive.get("attributes", {}).get("COLOR_0")
        if color is None:
            raise ValueError(f"{path.name}: primitive has no COLOR_0")
        sample = accessor_first_values(document, binary, color)
        if len(sample) < 3:
            raise ValueError(f"{path.name}: COLOR_0 has fewer than three channels")
        if min(abs(sample[1] - band) for band in (0.25, 0.50, 0.75)) > 0.04:
            raise ValueError(
                f"{path.name}: authored value {sample[1]:.3f} is outside its bands")
        samples.append(sample)
    if not any(abs(sample[0] - sample[1]) > 0.03 or
               abs(sample[1] - sample[2]) > 0.03 for sample in samples):
        raise ValueError(f"{path.name}: paint channels duplicate the palette index")
    print(f"PASS {path.name}: {len(primitives)} painted material primitives")


def validate_screen_first_hair() -> None:
    path = HEROES[1]
    manifest_path = path.with_suffix(".json")
    if not manifest_path.is_file():
        raise ValueError(f"missing {manifest_path}")
    manifest = json.loads(manifest_path.read_text())
    contract = manifest.get("hair_contract", {})
    if contract.get("scalp_cores") != 1 or contract.get("opaque_clumps") != 6:
        raise ValueError(f"{manifest_path.name}: V08 hair piece count is wrong")
    if contract.get("highlight_planes") != 1:
        raise ValueError(f"{manifest_path.name}: V08 needs one highlight plane")
    if contract.get("secondary_bones") != ["hair.long", "hair.rear"]:
        raise ValueError(f"{manifest_path.name}: V08 hair bones are wrong")

    component_names = {
        component.get("name", "") for component in manifest.get("components", [])
    }
    clumps = {name for name in component_names
              if name.startswith("SCREEN_FIRST_Hair") and
              any(token in name for token in
                  ("Bang_", "LongLock", "ShortLock", "RearWedge_"))}
    if len(clumps) != 6:
        raise ValueError(f"{manifest_path.name}: found {len(clumps)} hair clumps")
    forbidden = ("BackShell", "TopPlane", "HairFringe", "HairLongSide")
    found_forbidden = sorted(name for name in component_names
                             if any(token in name for token in forbidden))
    if found_forbidden:
        raise ValueError(
            f"{manifest_path.name}: cap-like parts remain: {found_forbidden}")

    document, _binary = parse_glb(path)
    node_names = {node.get("name", "") for node in document.get("nodes", [])}
    missing_bones = {"hair.long", "hair.rear"} - node_names
    if missing_bones:
        raise ValueError(
            f"{path.name}: missing hair bones {sorted(missing_bones)}")
    print("PASS screen-first V08 hair: hidden core, six clumps, one highlight, two bones")


def main() -> None:
    try:
        for path in HEROES:
            validate(path)
        validate_screen_first_hair()
    except ValueError as error:
        raise SystemExit(f"FAIL character paint validation: {error}") from error
    print("PASS runtime hero skins: palette, value, and fold channels are distinct")


if __name__ == "__main__":
    main()
