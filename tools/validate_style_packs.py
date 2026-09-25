#!/usr/bin/env python3
"""Structural check for assets/stylepacks/*/style.json.

This is a fast, Python-side companion to the strict C loader in
src/client/cc_style_pack.c (the actual runtime validator the game uses).
It exists so CI can catch an obviously broken manifest -- an unknown
field, a missing shader role, a shader path that does not resolve to a
file on disk, a schema_version this build does not understand -- without
building and running the whole client. It intentionally mirrors, rather
than replaces, the C validator's rules; the C loader is still the source
of truth for what the game actually accepts.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STYLE_PACKS_DIR = ROOT / "assets" / "stylepacks"
SCHEMA_VERSION = 1

TOP_LEVEL_KEYS = {
    "schema_version", "id", "version", "shaders",
    "constants", "palette", "render_target", "post_chain",
}
SHADER_ROLE_KEYS = {
    "world_vertex", "world_fragment", "skinned_vertex",
    "painted_environment_fragment", "tree_foliage_fragment",
    "hero_fragment", "npc_fragment", "grade_fragment",
}
CONSTANT_KEYS = {"hero_ink_strength", "material_ink", "dither_strength"}
RENDER_TARGET_KEYS = {"width", "height", "upscale_filter"}
POST_PASS_KEYS = {"name", "shader", "inputs", "cache"}
POST_PASS_INPUTS = {"scene_color", "scene_depth", "scene_normal"}
RAMP_KEYS = {"shadow", "base", "light"}
PALETTE_RAMP_FIELDS = {
    "teal", "gold", "danger", "violet", "earth", "road", "wood", "stone",
    "grass", "foliage", "crop", "metal", "parchment", "contraband",
    "people_skin",
}
PALETTE_COLOR_FIELDS = {
    "cool_ink", "warm_ink", "background", "panel", "panel_deep",
    "panel_hover", "bar_track", "ink", "muted", "contact_shadow_soft",
    "contact_shadow_strong", "road_dust", "footstep_print",
    "robot_chassis_teal", "robot_chassis_gold", "robot_limb_dark",
    "robot_skin_bronze",
}
CROWNLESS_KEYS = {
    "skin_shadow", "skin", "skin_light", "hair", "underlayer", "outer",
    "trousers", "leather", "metal", "accent", "panel_ink",
}


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def check_unknown(errors: list[str], obj: dict, allowed: set[str], where: str) -> None:
    for key in obj:
        if key not in allowed:
            fail(errors, f"{where}: unknown field \"{key}\"")


def check_color(errors: list[str], value: object, where: str) -> None:
    if not isinstance(value, list) or len(value) not in (3, 4):
        fail(errors, f"{where}: must be a [r, g, b] or [r, g, b, a] array")
        return
    for channel in value:
        if not isinstance(channel, int) or not 0 <= channel <= 255:
            fail(errors, f"{where}: channel {channel!r} is not an integer 0..255")


def check_ramp(errors: list[str], value: object, where: str) -> None:
    if not isinstance(value, dict):
        fail(errors, f"{where}: must be an object with shadow/base/light")
        return
    check_unknown(errors, value, RAMP_KEYS, where)
    for field in RAMP_KEYS:
        if field not in value:
            fail(errors, f"{where}.{field}: is required")
        else:
            check_color(errors, value[field], f"{where}.{field}")


def check_palette(errors: list[str], value: object, where: str) -> None:
    if not isinstance(value, dict):
        fail(errors, f"{where}: must be an object")
        return
    allowed = PALETTE_COLOR_FIELDS | PALETTE_RAMP_FIELDS | {"crownless"}
    check_unknown(errors, value, allowed, where)
    for field in PALETTE_COLOR_FIELDS:
        if field in value:
            check_color(errors, value[field], f"{where}.{field}")
    for field in PALETTE_RAMP_FIELDS:
        if field in value:
            check_ramp(errors, value[field], f"{where}.{field}")
    if "crownless" in value:
        crownless = value["crownless"]
        if not isinstance(crownless, dict):
            fail(errors, f"{where}.crownless: must be an object")
        else:
            check_unknown(errors, crownless, CROWNLESS_KEYS, f"{where}.crownless")
            for field in CROWNLESS_KEYS:
                if field in crownless:
                    check_color(errors, crownless[field], f"{where}.crownless.{field}")


def validate_manifest(path: Path) -> list[str]:
    errors: list[str] = []
    pack_dir = path.parent
    pack_id = pack_dir.name
    try:
        manifest = json.loads(path.read_text())
    except json.JSONDecodeError as error:
        return [f"{path}: invalid JSON ({error})"]
    if not isinstance(manifest, dict):
        return [f"{path}: manifest root must be an object"]

    check_unknown(errors, manifest, TOP_LEVEL_KEYS, str(path))

    if manifest.get("schema_version") != SCHEMA_VERSION:
        fail(errors, f"{path}: schema_version must be {SCHEMA_VERSION}")

    if manifest.get("id") != pack_id:
        fail(errors, f"{path}: \"id\" must be \"{pack_id}\" (its directory name)")

    if not isinstance(manifest.get("version"), str) or not manifest.get("version"):
        fail(errors, f"{path}: \"version\" must be a non-empty string")

    shaders = manifest.get("shaders")
    if not isinstance(shaders, dict):
        fail(errors, f"{path}: \"shaders\" object is required")
    else:
        check_unknown(errors, shaders, SHADER_ROLE_KEYS, f"{path}:shaders")
        for role in SHADER_ROLE_KEYS:
            relative = shaders.get(role)
            if not isinstance(relative, str) or not relative:
                fail(errors, f"{path}: missing shader role \"shaders.{role}\"")
                continue
            resolved = (pack_dir / relative).resolve()
            if not resolved.is_file():
                fail(errors,
                     f"{path}: shaders.{role} -> {relative} does not exist "
                     f"({resolved})")

    constants = manifest.get("constants")
    if constants is not None:
        if not isinstance(constants, dict):
            fail(errors, f"{path}: \"constants\" must be an object")
        else:
            check_unknown(errors, constants, CONSTANT_KEYS, f"{path}:constants")
            material_ink = constants.get("material_ink")
            if material_ink is not None and (
                not isinstance(material_ink, list) or len(material_ink) != 9
            ):
                fail(errors, f"{path}: \"constants.material_ink\" must have 9 numbers")

    palette = manifest.get("palette")
    if palette is not None:
        check_palette(errors, palette, f"{path}:palette")

    render_target = manifest.get("render_target")
    if not isinstance(render_target, dict):
        fail(errors, f"{path}: \"render_target\" object is required")
    else:
        check_unknown(errors, render_target, RENDER_TARGET_KEYS,
                      f"{path}:render_target")
        width = render_target.get("width")
        height = render_target.get("height")
        if not isinstance(width, int) or width <= 0:
            fail(errors, f"{path}: \"render_target.width\" must be a positive integer")
        if not isinstance(height, int) or height <= 0:
            fail(errors, f"{path}: \"render_target.height\" must be a positive integer")
        if render_target.get("upscale_filter") not in ("point", "bilinear"):
            fail(errors,
                 f"{path}: \"render_target.upscale_filter\" must be "
                 "\"point\" or \"bilinear\"")

    post_chain = manifest.get("post_chain")
    if not isinstance(post_chain, list) or not (1 <= len(post_chain) <= 4):
        fail(errors, f"{path}: \"post_chain\" must be an array of 1 to 4 passes")
    else:
        found_grade = False
        for index, pass_entry in enumerate(post_chain):
            where = f"{path}:post_chain[{index}]"
            if not isinstance(pass_entry, dict):
                fail(errors, f"{where}: must be an object")
                continue
            check_unknown(errors, pass_entry, POST_PASS_KEYS, where)
            name = pass_entry.get("name")
            shader = pass_entry.get("shader")
            if not isinstance(name, str) or not name:
                fail(errors, f"{where}.name: is required")
            if not isinstance(shader, str) or not shader:
                fail(errors, f"{where}.shader: is required")
            elif not (pack_dir / shader).resolve().is_file():
                fail(errors, f"{where}.shader: {shader} does not exist")
            inputs = pass_entry.get("inputs")
            if not isinstance(inputs, list) or not inputs:
                fail(errors, f"{where}.inputs: must be a non-empty array")
            else:
                for input_name in inputs:
                    if input_name not in POST_PASS_INPUTS:
                        fail(errors,
                             f"{where}.inputs: \"{input_name}\" must be one of "
                             + ", ".join(sorted(POST_PASS_INPUTS)))
            cache = pass_entry.get("cache", "per_frame")
            if cache not in ("per_frame", "per_shot"):
                fail(errors, f"{where}.cache: must be \"per_frame\" or \"per_shot\"")
            if name == "grade":
                found_grade = True
                if isinstance(shaders, dict) and shader != shaders.get("grade_fragment"):
                    fail(errors,
                         f"{where}: the \"grade\" pass shader must match "
                         "\"shaders.grade_fragment\"")
        if not found_grade:
            fail(errors,
                 f"{path}: \"post_chain\" must include a pass named \"grade\"")

    return errors


def main() -> int:
    if not STYLE_PACKS_DIR.is_dir():
        print(f"No style packs directory at {STYLE_PACKS_DIR}", file=sys.stderr)
        return 1
    manifests = sorted(STYLE_PACKS_DIR.glob("*/style.json"))
    if not manifests:
        print(f"No style pack manifests found under {STYLE_PACKS_DIR}", file=sys.stderr)
        return 1
    all_errors: list[str] = []
    for manifest in manifests:
        all_errors.extend(validate_manifest(manifest))
    if all_errors:
        print("Style pack validation failed:", file=sys.stderr)
        for error in all_errors:
            print(f"  {error}", file=sys.stderr)
        return 1
    print(f"Validated {len(manifests)} style pack manifest(s): "
          + ", ".join(m.parent.name for m in manifests))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
