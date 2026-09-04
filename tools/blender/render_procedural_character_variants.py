#!/usr/bin/env python3

from __future__ import annotations

import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import build_hero_component_library as hero
from procedural_character import (
    ACTION_FIGURE_WAYFARER,
    CUIRASS_SHELL,
    FITTED_BRACER,
    FITTED_GREAVE,
    FITTED_TUNIC,
    HEAVY_VANGUARD,
    LEAN_SCOUT,
    body_profiles,
    clip_profile,
    derive_shell,
    loft_rows,
    sweep_rows,
)


ROOT = Path(__file__).resolve().parents[2]
PREVIEW_PATH = ROOT / "assets" / "previews" / "hero" / "hero_procedural_variants.png"
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_procedural_character_variants.blend"


def shifted(point: tuple[float, float, float], offset: Vector) -> tuple[float, float, float]:
    return tuple(Vector(point) + offset)


def add_body(
    prefix: str,
    preset,
    collection: bpy.types.Collection,
    offset: Vector,
    material: str,
) -> dict[str, tuple]:
    profiles = body_profiles(preset)
    torso = hero.add_loft(f"{prefix}_BodyTorso", loft_rows(profiles["torso"]),
                          material, collection, "procedural_body", segments=16)
    torso.location += offset
    pelvis = hero.add_loft(f"{prefix}_BodyPelvis", loft_rows(profiles["pelvis"]),
                           material, collection, "procedural_body", segments=16)
    pelvis.location += offset
    segments = (
        ("UpperArmL", (-0.29, 0.0, 1.56), (-0.46, 0.0, 1.26), "upper_arm"),
        ("ForearmL", (-0.46, 0.0, 1.26), (-0.57, -0.015, 0.94), "forearm"),
        ("UpperArmR", (0.29, 0.0, 1.56), (0.46, 0.0, 1.26), "upper_arm"),
        ("ForearmR", (0.46, 0.0, 1.26), (0.57, -0.015, 0.94), "forearm"),
        ("ThighL", (-0.14, 0.0, 1.00), (-0.15, 0.0, 0.56), "thigh"),
        ("ShinL", (-0.15, 0.0, 0.56), (-0.15, 0.0, 0.13), "shin"),
        ("ThighR", (0.14, 0.0, 1.00), (0.15, 0.0, 0.56), "thigh"),
        ("ShinR", (0.15, 0.0, 0.56), (0.15, 0.0, 0.13), "shin"),
    )
    for name, start, end, region in segments:
        hero.add_limb_loft(
            f"{prefix}_Body{name}", shifted(start, offset), shifted(end, offset),
            sweep_rows(profiles[region]), material, collection,
            "procedural_body", segments=14,
        )
    hero.add_limb_loft(
        f"{prefix}_BodyNeck", shifted((0.0, 0.0, 1.61), offset),
        shifted((0.0, 0.0, 1.77), offset), sweep_rows(profiles["neck"]),
        material, collection, "procedural_body", segments=14,
    )
    hero.add_ico(f"{prefix}_BodyHead", shifted((0.0, 0.0, 1.92), offset),
                 (0.126, 0.110, 0.165), material, collection,
                 "procedural_body", subdivisions=2)
    for side, x in (("L", -0.58), ("R", 0.58)):
        scale = preset.hand_scale
        hero.add_ico(f"{prefix}_BodyHand{side}", shifted((x, -0.04, 0.85), offset),
                     (0.064 * scale, 0.050 * scale, 0.092 * scale),
                     material, collection, "procedural_body", subdivisions=2)
        foot = hero.add_boot_loft(f"{prefix}_BodyFoot{side}", offset.x + x * 0.2586,
                                  material, collection, "procedural_body",
                                  scale=0.86 * preset.foot_scale)
        foot.location.z += offset.z
        foot.location.y += offset.y
    return profiles


def add_garment_layer(
    prefix: str,
    profiles: dict[str, tuple],
    collection: bpy.types.Collection,
    offset: Vector,
) -> None:
    torso_profile = clip_profile(
        derive_shell(profiles["torso"], FITTED_TUNIC), 1.04, 1.67,
    )
    torso = hero.add_loft(f"{prefix}_TunicTorso", loft_rows(torso_profile),
                          "teal", collection, "procedural_garment", segments=16)
    torso.location += offset
    sleeve_profile = clip_profile(
        derive_shell(profiles["upper_arm"], FITTED_TUNIC),
        0.0, 0.62, normalize=True,
    )
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        hero.add_limb_loft(
            f"{prefix}_TunicSleeve{suffix}",
            shifted((sign * 0.30, 0.0, 1.57), offset),
            shifted((sign * 0.405, 0.0, 1.38), offset),
            sweep_rows(sleeve_profile), "teal_light", collection,
            "procedural_garment", segments=14,
        )


def add_equipment_layer(
    prefix: str,
    profiles: dict[str, tuple],
    collection: bpy.types.Collection,
    offset: Vector,
) -> None:
    cuirass_profile = clip_profile(
        derive_shell(profiles["torso"], CUIRASS_SHELL), 1.15, 1.63,
    )
    cuirass = hero.add_loft(f"{prefix}_Cuirass", loft_rows(cuirass_profile),
                            "brigandine", collection, "procedural_equipment",
                            segments=16)
    cuirass.location += offset
    bracer_profile = clip_profile(
        derive_shell(profiles["forearm"], FITTED_BRACER),
        0.10, 0.90, normalize=True,
    )
    greave_profile = clip_profile(
        derive_shell(profiles["shin"], FITTED_GREAVE),
        0.07, 0.84, normalize=True,
    )
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        hero.add_limb_loft(
            f"{prefix}_Bracer{suffix}",
            shifted((sign * 0.49, -0.012, 1.19), offset),
            shifted((sign * 0.56, -0.020, 0.98), offset),
            sweep_rows(bracer_profile), "leather", collection,
            "procedural_equipment", segments=14,
        )
        hero.add_limb_loft(
            f"{prefix}_Greave{suffix}",
            shifted((sign * 0.15, 0.0, 0.53), offset),
            shifted((sign * 0.15, -0.01, 0.20), offset),
            sweep_rows(greave_profile), "steel_dark", collection,
            "procedural_equipment", segments=14,
        )


def add_label(text: str, location: tuple[float, float, float], size: float = 0.14) -> None:
    curve = bpy.data.curves.new(f"TEXT_{text}", "FONT")
    curve.body = text.replace("_", " ").upper()
    curve.align_x = "CENTER"
    curve.size = size
    curve.extrude = 0.004
    obj = bpy.data.objects.new(f"LABEL_{text}", curve)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler.x = math.radians(90)
    curve.materials.append(hero.MATERIALS["padding"])


def add_camera_and_lights() -> None:
    camera_data = bpy.data.cameras.new("CAM_ProceduralVariants")
    camera = bpy.data.objects.new("CAM_ProceduralVariants", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = (0.0, -12.0, 3.40)
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 9.20
    hero.point_at(camera, Vector((0.0, 0.0, 3.40)))
    bpy.context.scene.camera = camera
    for name, location, energy, size, color in (
        ("KEY", (-4.5, -5.0, 8.0), 1200, 5.0, (1.0, 0.82, 0.68)),
        ("FILL", (4.0, -4.0, 5.0), 700, 4.0, (0.60, 0.78, 1.0)),
        ("RIM", (0.0, 3.0, 7.0), 850, 3.0, (0.55, 0.70, 1.0)),
    ):
        data = bpy.data.lights.new(name, "AREA")
        data.energy = energy
        data.shape = "DISK"
        data.size = size
        data.color = color
        light = bpy.data.objects.new(name, data)
        light.location = location
        bpy.context.scene.collection.objects.link(light)
        hero.point_at(light, Vector((0.0, 0.0, 3.0)))


def main() -> None:
    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    hero.reset_scene()
    hero.make_palette()
    scene = bpy.context.scene
    scene.name = "CC_PROCEDURAL_CHARACTER_VARIANTS"
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.resolution_percentage = 100
    collection = bpy.data.collections.new("CC_PROCEDURAL_VARIANTS")
    scene.collection.children.link(collection)

    presets = (LEAN_SCOUT, ACTION_FIGURE_WAYFARER, HEAVY_VANGUARD)
    columns = (-1.55, 0.0, 1.55)
    rows = ((4.45, "BODY"), (2.20, "CLOTH BASIC"), (-0.05, "EQUIPMENT BASIC"))
    for preset, x in zip(presets, columns):
        add_label(preset.name, (x, -0.55, 6.66), size=0.125)
        for row_index, (z, _label) in enumerate(rows):
            offset = Vector((x, 0.0, z))
            material = "body_neutral" if row_index == 0 else "ghost"
            profiles = add_body(f"{preset.name}_{row_index}", preset,
                                collection, offset, material)
            if row_index == 1:
                add_garment_layer(preset.name, profiles, collection, offset)
            elif row_index == 2:
                add_equipment_layer(preset.name, profiles, collection, offset)
    for z, label in rows:
        add_label(label, (-2.55, -0.55, z + 1.05), size=0.12)

    add_camera_and_lights()
    scene.render.filepath = str(PREVIEW_PATH)
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH), compress=True)
    print(f"Rendered procedural character variants to {PREVIEW_PATH}")


if __name__ == "__main__":
    main()
