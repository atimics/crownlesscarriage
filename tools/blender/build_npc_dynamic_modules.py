#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import asdict, dataclass
import argparse
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import paint_channels


ROOT = Path(__file__).resolve().parents[2]
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_npc_dynamic_modules.blend"
EXPORT_DIR = ROOT / "assets" / "exports" / "npc"
MANIFEST_PATH = ROOT / "assets" / "npc_dynamic_module_manifest.json"
LIBRARY_VERSION = "0.7.0"
MATERIAL_PILOT_SLOTS = {
    "torso", "foot", "mantle", "chest_plate", "chest_plate_hero",
    "pauldron", "pauldron_hero",
}
MATERIAL_NAME = "MAT_NPC_INDEXED"
PALETTE_INDEX = {
    "skin": 0,
    "hair": 1,
    "underlayer": 2,
    "outer": 3,
    "outer_shadow": 3,
    "trousers": 4,
    "trousers_shadow": 4,
    "leather": 5,
    "metal": 6,
    "accent": 7,
    "eye": 8,
}
PAINT_SEMANTICS = (
    "skin", "hair", "underlayer", "outer", "trousers",
    "leather", "metal", "accent", "eye",
)


@dataclass(frozen=True)
class ModuleRecord:
    id: str
    slot: str
    anchor: str
    material: str
    export: str
    shape_contract: str


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_NPC_DYNAMIC_MODULE_LIBRARY"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene["cc_library_version"] = LIBRARY_VERSION
    scene["cc_runtime_strategy"] = "rigid modules on biomechanical bone frames"


def make_material() -> bpy.types.Material:
    material = bpy.data.materials.new(MATERIAL_NAME)
    material.diffuse_color = (1.0, 1.0, 1.0, 1.0)
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    # Export the active attribute as data, including its opaque surface label.
    # A Base Color link makes Blender trim alpha for opaque materials.
    principled.inputs["Base Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    principled.inputs["Roughness"].default_value = 0.78
    return material


def collection_for(asset_id: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(asset_id.upper())
    bpy.context.scene.collection.children.link(collection)
    collection["cc_asset_id"] = asset_id
    collection["cc_library_version"] = LIBRARY_VERSION
    return collection


def link_only(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)


def finish(obj: bpy.types.Object, collection: bpy.types.Collection,
           material: bpy.types.Material) -> bpy.types.Object:
    obj.data.materials.append(material)
    link_only(obj, collection)
    return obj


def paint(obj: bpy.types.Object, palette_slot: str) -> bpy.types.Object:
    obj["cc_palette_slot"] = palette_slot
    return obj


def bevel(obj: bpy.types.Object, width: float) -> None:
    modifier = obj.modifiers.new("CC_ModuleBevel", "BEVEL")
    modifier.width = width
    modifier.segments = 1
    modifier.limit_method = "ANGLE"


def add_box(name: str, center: tuple[float, float, float],
            dimensions: tuple[float, float, float],
            collection: bpy.types.Collection,
            material: bpy.types.Material, *, width: float = 0.04,
            rotation: tuple[float, float, float] = (0.0, 0.0, 0.0)
            ) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=center, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel(obj, min(width, min(dimensions) * 0.20))
    return finish(obj, collection, material)


def add_ico(name: str, center: tuple[float, float, float],
            scale: tuple[float, float, float],
            collection: bpy.types.Collection,
            material: bpy.types.Material, *, subdivisions: int = 2
            ) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(
        subdivisions=subdivisions, radius=1.0, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, collection, material)


def add_cylinder(name: str, center: tuple[float, float, float], radius: float,
                 depth: float, collection: bpy.types.Collection,
                 material: bpy.types.Material, *, vertices: int = 8,
                 rotation: tuple[float, float, float] = (0.0, 0.0, 0.0)
                 ) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth, location=center,
        rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    return finish(obj, collection, material)


def add_loft(name: str,
             rings: tuple[tuple[float, float, float], ...],
             collection: bpy.types.Collection, material: bpy.types.Material,
             *, sides: int = 8) -> bpy.types.Object:
    vertices: list[tuple[float, float, float]] = []
    for height, radius_x, radius_y in rings:
        for index in range(sides):
            angle = math.tau * index / sides
            vertices.append((math.cos(angle) * radius_x,
                             math.sin(angle) * radius_y, height))
    faces: list[tuple[int, ...]] = [tuple(reversed(range(sides)))]
    for row in range(len(rings) - 1):
        base = row * sides
        following = (row + 1) * sides
        for index in range(sides):
            nxt = (index + 1) % sides
            faces.append((base + index, base + nxt,
                          following + nxt, following + index))
    top = (len(rings) - 1) * sides
    faces.append(tuple(range(top, top + sides)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def add_panel(name: str, points: tuple[tuple[float, float, float], ...],
              collection: bpy.types.Collection,
              material: bpy.types.Material, *, thickness: float = 0.04
              ) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(points, [], [tuple(range(len(points)))])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(material)
    solidify = obj.modifiers.new("CC_ModuleThickness", "SOLIDIFY")
    solidify.thickness = thickness
    bevel(obj, 0.025)
    return obj


def add_hair_clump(
    name: str,
    path: tuple[tuple[float, float, float], ...],
    widths: tuple[float, ...],
    depths: tuple[float, ...],
    collection: bpy.types.Collection,
    material: bpy.types.Material,
) -> bpy.types.Object:
    if len(path) < 3 or len(path) > 5:
        raise ValueError(f"{name}: hair clumps need 3 to 5 sections")
    if len(path) != len(widths) or len(path) != len(depths):
        raise ValueError(f"{name}: hair section counts do not match")
    centers = [Vector(point) for point in path]
    vertices: list[tuple[float, float, float]] = []
    for index, center in enumerate(centers):
        before = centers[max(0, index - 1)]
        after = centers[min(len(centers) - 1, index + 1)]
        tangent = (after - before).normalized()
        side = Vector((0.0, 1.0, 0.0)).cross(tangent)
        if side.length_squared < 1.0e-8:
            side = Vector((1.0, 0.0, 0.0))
        side.normalize()
        thickness = tangent.cross(side).normalized()
        half_width = widths[index] * 0.5
        half_depth = depths[index] * 0.5
        vertices.extend((
            tuple(center - side * half_width),
            tuple(center - thickness * half_depth),
            tuple(center + side * half_width),
            tuple(center + thickness * half_depth),
        ))
    faces: list[tuple[int, ...]] = [(3, 2, 1, 0)]
    for section in range(len(path) - 1):
        first = section * 4
        following = (section + 1) * 4
        for edge in range(4):
            next_edge = (edge + 1) % 4
            faces.append((
                first + edge, first + next_edge,
                following + next_edge, following + edge,
            ))
    final = (len(path) - 1) * 4
    faces.append((final, final + 1, final + 2, final + 3))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def add_armor_rivets(
    prefix: str,
    positions: tuple[tuple[float, float], ...],
    front: float,
    collection: bpy.types.Collection,
    material: bpy.types.Material,
) -> None:
    for index, (x, z) in enumerate(positions):
        paint(add_ico(
            f"{prefix}Rivet{index}", (x, front, z),
            (0.038, 0.026, 0.038), collection, material,
            subdivisions=1,
        ), "accent")


def add_guard_armor(collection: bpy.types.Collection,
                    material: bpy.types.Material) -> None:
    paint(add_loft("GEO_ModuleGuardPadding", (
        (-0.54, 0.38, 0.35), (-0.22, 0.47, 0.40),
        (0.18, 0.51, 0.42), (0.48, 0.43, 0.36),
    ), collection, material, sides=10), "underlayer")
    for name, points in (
        ("UpperLeft", (
            (-0.48, -0.43, 0.43), (-0.10, -0.43, 0.50),
            (0.0, -0.43, 0.29), (-0.40, -0.43, 0.17),
        )),
        ("UpperRight", (
            (0.10, -0.43, 0.50), (0.48, -0.43, 0.43),
            (0.40, -0.43, 0.17), (0.0, -0.43, 0.29),
        )),
        ("Middle", (
            (-0.41, -0.46, 0.19), (0.41, -0.46, 0.19),
            (0.37, -0.46, -0.08), (-0.37, -0.46, -0.08),
        )),
        ("Lower", (
            (-0.37, -0.49, -0.05), (0.37, -0.49, -0.05),
            (0.30, -0.49, -0.38), (-0.30, -0.49, -0.38),
        )),
    ):
        paint(add_panel(f"GEO_ModuleGuardPlate{name}", points,
                        collection, material, thickness=0.075), "metal")
    # The broad band remains visible beneath the overlapping breastplate.
    shadow = paint(add_box("GEO_ModuleGuardPlateShadow", (0.0, -0.47, 0.13),
        (0.76, 0.07, 0.09), collection, material, width=0.015), "metal")
    shadow["cc_value_offset"] = -0.25
    paint(add_box("GEO_ModuleGuardCenterRidge", (0.0, -0.515, 0.04),
                  (0.090, 0.050, 0.72), collection, material,
                  width=0.012), "accent")
    paint(add_box("GEO_ModuleGuardBelt", (0.0, -0.49, -0.40),
                  (0.88, 0.10, 0.105), collection, material,
                  width=0.018), "leather")
    for side in (-1.0, 1.0):
        paint(add_box(
            f"GEO_ModuleGuardStrap{side:+.0f}",
            (side * 0.32, -0.49, 0.02), (0.075, 0.055, 0.73),
            collection, material, width=0.012,
            rotation=(0.0, side * 0.10, 0.0),
        ), "leather")
        paint(add_panel(
            f"GEO_ModuleGuardTasset{side:+.0f}", (
                (side * 0.04, -0.38, -0.42),
                (side * 0.31, -0.38, -0.42),
                (side * 0.29, -0.38, -0.78),
                (side * 0.07, -0.38, -0.84),
            ), collection, material, thickness=0.060,
        ), "metal")
    add_armor_rivets("GEO_ModuleGuard", (
        (-0.39, 0.34), (0.39, 0.34), (-0.34, 0.04), (0.34, 0.04),
        (-0.27, -0.27), (0.27, -0.27),
    ), -0.53, collection, material)


def add_raider_armor(collection: bpy.types.Collection,
                     material: bpy.types.Material) -> None:
    paint(add_loft("GEO_ModuleRaiderPadding", (
        (-0.54, 0.36, 0.34), (-0.20, 0.45, 0.39),
        (0.17, 0.49, 0.41), (0.46, 0.42, 0.35),
    ), collection, material, sides=9), "outer")
    paint(add_panel("GEO_ModuleRaiderPlateLeft", (
        (-0.48, -0.45, 0.43), (-0.04, -0.48, 0.48),
        (-0.01, -0.49, -0.13), (-0.39, -0.46, -0.23),
    ), collection, material, thickness=0.070), "metal")
    paint(add_panel("GEO_ModuleRaiderPlateRight", (
        (0.06, -0.44, 0.31), (0.42, -0.42, 0.20),
        (0.34, -0.45, -0.12), (0.04, -0.48, -0.05),
    ), collection, material, thickness=0.060), "metal")
    paint(add_panel("GEO_ModuleRaiderRepair", (
        (0.15, -0.50, 0.18), (0.36, -0.48, 0.11),
        (0.29, -0.50, -0.03), (0.11, -0.51, 0.01),
    ), collection, material, thickness=0.035), "accent")
    paint(add_box("GEO_ModuleRaiderCrossStrap", (0.02, -0.53, 0.00),
                  (0.105, 0.060, 1.10), collection, material,
                  width=0.016, rotation=(0.0, -0.30, 0.0)), "leather")
    paint(add_box("GEO_ModuleRaiderBelt", (0.0, -0.48, -0.40),
                  (0.90, 0.11, 0.12), collection, material,
                  width=0.018, rotation=(0.0, 0.0, 0.035)), "leather")
    paint(add_panel("GEO_ModuleRaiderTassetMetal", (
        (-0.35, -0.39, -0.40), (-0.05, -0.39, -0.43),
        (-0.10, -0.39, -0.84), (-0.31, -0.39, -0.76),
    ), collection, material, thickness=0.065), "metal")
    paint(add_panel("GEO_ModuleRaiderTassetLeather", (
        (0.05, -0.36, -0.42), (0.25, -0.36, -0.39),
        (0.31, -0.36, -0.71), (0.09, -0.36, -0.81),
    ), collection, material, thickness=0.055), "leather")
    add_armor_rivets("GEO_ModuleRaider", (
        (-0.38, 0.34), (-0.10, 0.36), (-0.33, -0.11),
        (0.31, 0.14), (0.20, -0.02),
    ), -0.55, collection, material)


def add_hero_armor(collection: bpy.types.Collection,
                   material: bpy.types.Material) -> None:
    paint(add_loft("GEO_ModuleHeroPadding", (
        (-0.56, 0.37, 0.35), (-0.22, 0.47, 0.40),
        (0.18, 0.51, 0.42), (0.48, 0.43, 0.36),
    ), collection, material, sides=10), "outer")
    for name, points in (
        ("UpperLeft", (
            (-0.48, -0.44, 0.43), (-0.11, -0.46, 0.50),
            (-0.01, -0.49, 0.27), (-0.41, -0.46, 0.15),
        )),
        ("UpperRight", (
            (0.11, -0.46, 0.50), (0.48, -0.44, 0.43),
            (0.41, -0.46, 0.15), (0.01, -0.49, 0.27),
        )),
        ("Middle", (
            (-0.41, -0.48, 0.17), (0.41, -0.48, 0.17),
            (0.36, -0.50, -0.09), (-0.36, -0.50, -0.09),
        )),
        ("Lower", (
            (-0.36, -0.51, -0.06), (0.36, -0.51, -0.06),
            (0.29, -0.50, -0.38), (-0.29, -0.50, -0.38),
        )),
    ):
        paint(add_panel(f"GEO_ModuleHeroPlate{name}", points,
                        collection, material, thickness=0.060), "metal")
    for side in (-1.0, 1.0):
        paint(add_box(
            f"GEO_ModuleHeroHarness{side:+.0f}",
            (side * 0.20, -0.54, -0.01), (0.085, 0.055, 0.92),
            collection, material, width=0.014,
            rotation=(0.0, side * 0.24, 0.0),
        ), "leather")
    paint(add_box("GEO_ModuleHeroBelt", (0.0, -0.50, -0.40),
                  (0.90, 0.11, 0.11), collection, material,
                  width=0.018), "leather")
    paint(add_box("GEO_ModuleBrokenCrownBase", (0.0, -0.565, 0.06),
                  (0.34, 0.045, 0.060), collection, material,
                  width=0.010, rotation=(0.0, 0.0, -0.035)), "accent")
    for index, (x, height, lean) in enumerate((
        (-0.12, 0.15, -0.18), (0.0, 0.21, 0.04), (0.12, 0.12, 0.22),
    )):
        paint(add_box(
            f"GEO_ModuleBrokenCrownTooth{index}",
            (x, -0.565, 0.12 + height * 0.5), (0.055, 0.045, height),
            collection, material, width=0.009,
            rotation=(0.0, lean, 0.0),
        ), "accent")
    paint(add_panel("GEO_ModuleHeroTassetLeft", (
        (-0.32, -0.39, -0.41), (-0.04, -0.39, -0.42),
        (-0.07, -0.39, -0.84), (-0.29, -0.39, -0.78),
    ), collection, material, thickness=0.065), "metal")
    paint(add_panel("GEO_ModuleHeroTassetRight", (
        (0.04, -0.37, -0.42), (0.28, -0.37, -0.40),
        (0.31, -0.37, -0.72), (0.08, -0.37, -0.82),
    ), collection, material, thickness=0.055), "outer")
    add_armor_rivets("GEO_ModuleHero", (
        (-0.39, 0.34), (0.39, 0.34), (-0.34, 0.01), (0.34, 0.01),
        (-0.26, -0.28), (0.26, -0.28),
    ), -0.57, collection, material)


def add_guard_pauldron(collection: bpy.types.Collection,
                       material: bpy.types.Material) -> None:
    paint(add_ico("GEO_ModuleGuardShoulderPad", (0.0, 0.02, -0.02),
                  (0.62, 0.50, 0.48), collection, material,
                  subdivisions=1), "outer")
    paint(add_ico("GEO_ModuleGuardPauldron", (0.0, -0.05, 0.02),
                  (0.64, 0.44, 0.44), collection, material,
                  subdivisions=1), "metal")
    paint(add_box("GEO_ModuleGuardPauldronLip", (0.0, -0.35, -0.18),
                  (0.95, 0.13, 0.18), collection, material,
                  width=0.035), "accent")
    paint(add_box("GEO_ModuleGuardPauldronStrap", (0.0, 0.25, -0.24),
                  (0.72, 0.12, 0.16), collection, material,
                  width=0.025), "leather")


def add_raider_pauldron(collection: bpy.types.Collection,
                        material: bpy.types.Material) -> None:
    paint(add_ico("GEO_ModuleRaiderShoulderPad", (0.0, 0.03, -0.04),
                  (0.62, 0.47, 0.46), collection, material,
                  subdivisions=1), "leather")
    paint(add_panel("GEO_ModuleRaiderPauldron", (
        (-0.55, -0.25, 0.22), (0.45, -0.32, 0.13),
        (0.36, -0.36, -0.31), (-0.31, -0.31, -0.40),
        (-0.61, -0.25, -0.09),
    ), collection, material, thickness=0.095), "metal")
    paint(add_box("GEO_ModuleRaiderPauldronPatch", (-0.15, -0.39, -0.04),
                  (0.38, 0.065, 0.20), collection, material,
                  width=0.025, rotation=(0.0, -0.18, 0.04)), "accent")
    paint(add_box("GEO_ModuleRaiderPauldronStrap", (0.0, 0.23, -0.25),
                  (0.78, 0.13, 0.17), collection, material,
                  width=0.025, rotation=(0.0, 0.0, 0.07)), "leather")


def add_hero_pauldron(collection: bpy.types.Collection,
                      material: bpy.types.Material) -> None:
    paint(add_ico("GEO_ModuleHeroShoulderPad", (0.0, 0.03, -0.03),
                  (0.64, 0.50, 0.49), collection, material,
                  subdivisions=1), "outer")
    paint(add_ico("GEO_ModuleHeroPauldron", (0.0, -0.05, 0.04),
                  (0.68, 0.47, 0.47), collection, material,
                  subdivisions=1), "metal")
    paint(add_box("GEO_ModuleHeroPauldronLowerLip", (0.0, -0.36, -0.20),
                  (1.00, 0.13, 0.18), collection, material,
                  width=0.034), "accent")
    paint(add_box("GEO_ModuleHeroPauldronCrest", (-0.10, -0.12, 0.38),
                  (0.15, 0.18, 0.38), collection, material,
                  width=0.025, rotation=(0.0, -0.10, -0.06)), "accent")
    paint(add_box("GEO_ModuleHeroPauldronStrap", (0.0, 0.25, -0.25),
                  (0.76, 0.12, 0.17), collection, material,
                  width=0.025), "leather")


def build_geometry(slot: str, collection: bpy.types.Collection,
                   material: bpy.types.Material) -> None:
    if slot == "torso":
        add_loft("GEO_ModuleTorso", (
            (-0.05, 0.34, 0.29), (0.18, 0.37, 0.31),
            (0.52, 0.44, 0.34), (0.80, 0.50, 0.36),
            (1.03, 0.43, 0.30),
        ), collection, material, sides=10)
        yoke = add_box("GEO_ModuleTunicYoke", (0.0, -0.01, 0.82),
                       (0.92, 0.62, 0.075), collection, material,
                       width=0.025)
        yoke["cc_palette_slot"] = "underlayer"
        hem = add_box("GEO_ModuleTunicHem", (0.0, 0.0, 0.055),
                      (0.70, 0.56, 0.075), collection, material,
                      width=0.020)
        hem["cc_palette_slot"] = "trousers"
        for side in (-1.0, 1.0):
            fold = paint(add_panel(f"GEO_ModuleTunicFold{side:+.0f}", (
                (side * 0.10, -0.32, 0.12),
                (side * 0.22, -0.32, 0.14),
                (side * 0.30, -0.35, 0.58),
                (side * 0.20, -0.35, 0.63),
            ), collection, material, thickness=0.035), "outer")
            fold["cc_value_offset"] = -0.25
        yoke["cc_value_offset"] = -0.25
    elif slot == "pelvis":
        add_loft("GEO_ModulePelvis", (
            (-0.08, 0.44, 0.34), (0.50, 0.50, 0.37),
            (1.07, 0.43, 0.32),
        ), collection, material, sides=10)
        add_box("GEO_ModuleWaistband", (0.0, 0.0, 0.91),
                (0.92, 0.68, 0.13), collection, material, width=0.030)
    elif slot == "upper_arm":
        add_loft("GEO_ModuleUpperArm", (
            (-0.08, 0.54, 0.50), (0.28, 0.61, 0.57),
            (0.72, 0.48, 0.46), (1.06, 0.38, 0.37),
        ), collection, material)
        add_cylinder("GEO_ModuleSleeveCuff", (0.0, 0.0, 0.92),
                     0.41, 0.12, collection, material, vertices=8)
    elif slot == "forearm":
        add_loft("GEO_ModuleForearm", (
            (-0.06, 0.44, 0.42), (0.32, 0.56, 0.50),
            (0.72, 0.43, 0.39), (1.05, 0.31, 0.29),
        ), collection, material)
        add_cylinder("GEO_ModuleUnderlayerCuff", (0.0, 0.0, 0.94),
                     0.34, 0.10, collection, material, vertices=8)
    elif slot == "thigh":
        add_loft("GEO_ModuleThigh", (
            (-0.10, 0.55, 0.51), (0.28, 0.66, 0.58),
            (0.70, 0.52, 0.47), (1.08, 0.39, 0.37),
        ), collection, material, sides=9)
    elif slot == "shin":
        add_loft("GEO_ModuleShin", (
            (-0.07, 0.43, 0.40), (0.26, 0.53, 0.47),
            (0.58, 0.47, 0.44), (1.06, 0.31, 0.29),
        ), collection, material, sides=9)
    elif slot == "hand":
        add_ico("GEO_ModuleHand", (0.0, 0.0, 0.0),
                (0.50, 0.42, 0.58), collection, material, subdivisions=1)
    elif slot == "foot":

        add_box("GEO_ModuleFoot", (0.0, -0.02, 0.48),
                (0.84, 0.74, 1.12), collection, material, width=0.10)
        sole = add_box("GEO_ModuleFootSole", (0.0, -0.22, 0.53),
                (0.91, 0.23, 1.19), collection, material, width=0.05)
        sole["cc_value_offset"] = -0.25
    elif slot == "head":
        add_ico("GEO_ModuleCranium", (0.0, 0.0, 0.06),
                (0.50, 0.46, 0.44), collection, material)
        add_ico("GEO_ModuleJaw", (0.0, -0.055, -0.30),
                (0.41, 0.40, 0.24), collection, material, subdivisions=1)
    elif slot == "mantle":
        add_panel("GEO_ModuleMantle", (
            (-0.50, 0.0, 0.05), (0.42, 0.0, 0.02),
            (0.36, 0.10, -0.86), (0.05, 0.13, -1.00),
            (-0.46, 0.10, -0.78),
        ), collection, material, thickness=0.055)
        fold = add_panel("GEO_ModuleMantleFold", (
            (-0.10, -0.04, 0.03), (0.06, -0.04, 0.02),
            (0.20, 0.08, -0.88), (0.03, 0.09, -0.96),
        ), collection, material, thickness=0.035)
        fold["cc_value_offset"] = -0.25
    elif slot == "coat_tail":
        add_box("GEO_ModuleCoatTail", (0.0, 0.0, -0.50),
                (0.78, 0.20, 1.0), collection, material, width=0.08,
                rotation=(0.05, 0.0, 0.0))
    elif slot == "chest_plate":
        add_guard_armor(collection, material)
    elif slot == "chest_plate_raider":
        add_raider_armor(collection, material)
    elif slot == "chest_plate_hero":
        add_hero_armor(collection, material)
    elif slot == "pauldron":
        add_guard_pauldron(collection, material)
    elif slot == "pauldron_raider":
        add_raider_pauldron(collection, material)
    elif slot == "pauldron_hero":
        add_hero_pauldron(collection, material)
    elif slot == "apron":
        add_box("GEO_ModuleApronBib", (0.0, -0.05, 0.28),
                (0.62, 0.12, 0.38), collection, material, width=0.06)
        for side in (-1.0, 1.0):
            add_box(f"GEO_ModuleApronSkirt{side:+.0f}",
                    (side * 0.22, -0.03, -0.30),
                    (0.38, 0.14, 0.72), collection, material, width=0.06,
                    rotation=(0.0, side * 0.035, 0.0))
    elif slot == "pack":
        add_box("GEO_ModulePack", (0.0, 0.0, 0.0),
                (1.0, 0.52, 1.0), collection, material, width=0.14)
        add_cylinder("GEO_ModuleBedroll", (0.0, 0.10, 0.60),
                     0.22, 1.05, collection, material, vertices=8,
                     rotation=(0.0, math.pi * 0.5, 0.0))
    elif slot == "satchel":
        add_box("GEO_ModuleSatchel", (0.0, 0.0, 0.0),
                (1.0, 0.48, 0.82), collection, material, width=0.14)
        add_box("GEO_ModuleSatchelFlap", (0.0, -0.28, 0.12),
                (0.88, 0.12, 0.42), collection, material, width=0.06)
    elif slot == "helmet":
        add_ico("GEO_ModuleHelmet", (0.0, 0.0, 0.12),
                (0.57, 0.54, 0.44), collection, material, subdivisions=1)
        add_box("GEO_ModuleHelmetBrow", (0.0, -0.48, -0.02),
                (1.08, 0.18, 0.18), collection, material, width=0.04)
        add_box("GEO_ModuleHelmetRidge", (0.0, 0.02, 0.50),
                (0.16, 0.50, 0.36), collection, material, width=0.04)
        for side in (-1.0, 1.0):
            add_box(f"GEO_ModuleHelmetCheek{side:+.0f}",
                    (side * 0.48, -0.12, -0.22), (0.16, 0.24, 0.40),
                    collection, material, width=0.04,
                    rotation=(0.0, side * 0.08, 0.0))
    elif slot == "hat":
        add_cylinder("GEO_ModuleHatBrim", (0.0, 0.0, 0.48),
                     0.72, 0.10, collection, material, vertices=12)
        add_cylinder("GEO_ModuleHatCrown", (0.0, 0.04, 0.72),
                     0.43, 0.48, collection, material, vertices=9)
        add_box("GEO_ModuleHatTuck", (0.43, 0.06, 0.93),
                (0.12, 0.12, 0.42), collection, material, width=0.04,
                rotation=(0.0, -0.20, -0.18))
    elif slot == "hood":
        add_ico("GEO_ModuleHoodCrown", (0.0, 0.07, 0.24),
                (0.62, 0.57, 0.67), collection, material, subdivisions=1)
        for side in (-1.0, 1.0):
            add_box(f"GEO_ModuleHoodSide{side:+.0f}",
                    (side * 0.48, 0.08, -0.12), (0.26, 0.52, 0.72),
                    collection, material, width=0.08)
        add_box("GEO_ModuleHoodBrow", (0.0, -0.49, 0.16),
                (1.02, 0.16, 0.16), collection, material, width=0.05)
    elif slot == "headwrap":
        add_ico("GEO_ModuleHeadwrapCap", (0.0, 0.02, 0.30),
                (0.55, 0.52, 0.40), collection, material, subdivisions=1)
        add_box("GEO_ModuleHeadwrapBand", (0.0, -0.44, 0.20),
                (1.05, 0.18, 0.24), collection, material, width=0.06,
                rotation=(0.0, 0.0, 0.05))
        add_ico("GEO_ModuleHeadwrapKnot", (0.46, 0.26, 0.15),
                (0.16, 0.15, 0.17), collection, material, subdivisions=1)
        add_panel("GEO_ModuleHeadwrapTail", (
            (0.40, 0.25, 0.12), (0.53, 0.25, 0.08),
            (0.46, 0.29, -0.54), (0.31, 0.29, -0.40),
        ), collection, material, thickness=0.05)
    elif slot == "tool_shaft":
        add_loft("GEO_ModuleToolShaft", (
            (0.00, 0.48, 0.48), (0.08, 0.56, 0.56),
            (0.92, 0.47, 0.47), (1.00, 0.38, 0.38),
        ), collection, material, sides=7)
    elif slot == "tool_head":
        add_box("GEO_ModuleToolHead", (0.0, 0.0, 0.0),
                (1.0, 0.34, 0.42), collection, material, width=0.10)
        add_box("GEO_ModuleToolBlade", (-0.48, 0.0, -0.06),
                (0.30, 0.22, 0.70), collection, material, width=0.07,
                rotation=(0.0, -0.10, 0.0))
    elif slot.startswith("hair_"):
        style = int(slot.removeprefix("hair_"))
        if style == 3:



            core = add_ico("GEO_ModuleHairCore", (0.0, 0.10, 0.16),
                           (0.46, 0.36, 0.38), collection, material,
                           subdivisions=1)
            add_hair_clump(
                "GEO_ModuleHairBangL",
                ((-0.08, -0.16, 0.50), (-0.18, -0.34, 0.47),
                 (-0.30, -0.42, 0.41), (-0.39, -0.39, 0.36)),
                (0.18, 0.16, 0.10, 0.025),
                (0.15, 0.13, 0.075, 0.020), collection, material)
            add_hair_clump(
                "GEO_ModuleHairBangR",
                ((0.08, -0.16, 0.49), (0.18, -0.34, 0.46),
                 (0.30, -0.41, 0.40), (0.39, -0.38, 0.35)),
                (0.18, 0.16, 0.10, 0.025),
                (0.15, 0.13, 0.075, 0.020), collection, material)
            long_side = add_hair_clump(
                "GEO_ModuleHairLongSide",
                ((-0.20, 0.10, 0.49), (-0.34, 0.18, 0.30),
                 (-0.40, 0.30, -0.12), (-0.31, 0.45, -0.82)),
                (0.25, 0.23, 0.14, 0.020),
                (0.21, 0.18, 0.10, 0.020), collection, material)
            long_side["cc_palette_slot"] = "hair"
            add_hair_clump(
                "GEO_ModuleHairShortSide",
                ((0.20, 0.10, 0.46), (0.34, 0.18, 0.30),
                 (0.38, 0.29, 0.04), (0.29, 0.42, -0.48)),
                (0.24, 0.22, 0.14, 0.020),
                (0.20, 0.17, 0.10, 0.020), collection, material)
            add_hair_clump(
                "GEO_ModuleHairRearL",
                ((-0.03, 0.34, 0.52), (-0.14, 0.44, 0.38),
                 (-0.23, 0.60, -0.08), (-0.14, 0.72, -0.62)),
                (0.32, 0.34, 0.22, 0.020),
                (0.20, 0.19, 0.12, 0.020), collection, material)
            add_hair_clump(
                "GEO_ModuleHairRearR",
                ((0.03, 0.33, 0.48), (0.14, 0.43, 0.33),
                 (0.23, 0.59, -0.10), (0.14, 0.71, -0.65)),
                (0.31, 0.33, 0.22, 0.020),
                (0.20, 0.18, 0.12, 0.020), collection, material)


            add_panel("GEO_ModuleHairRearHighlight", (
                (-0.15, 0.665, 0.30), (-0.04, 0.662, 0.25),
                (-0.06, 0.665, -0.18), (-0.13, 0.669, -0.03),
            ), collection, material, thickness=0.012)
            return
        add_ico("GEO_ModuleHairCap", (0.0, 0.04, 0.30),
                (0.54, 0.50, 0.30 if style in {0, 5} else 0.38),
                collection, material, subdivisions=1)
        if style in {1, 2, 4, 6, 7}:
            part_side = -1.0 if style in {1, 6} else 1.0
            add_box("GEO_ModuleHairPart", (part_side * 0.15, -0.42, 0.25),
                    (0.60, 0.16, 0.28), collection, material, width=0.07,
                    rotation=(0.0, part_side * 0.10, part_side * 0.08))
        if style == 1:
            add_box("GEO_ModuleHairBack", (0.0, 0.38, -0.12),
                    (0.82, 0.28, 0.76), collection, material, width=0.10)
        elif style == 2:
            add_ico("GEO_ModuleHairBun", (0.0, 0.45, 0.28),
                    (0.28, 0.28, 0.30), collection, material, subdivisions=1)
        elif style == 4:
            for side in (-1.0, 1.0):
                add_cylinder(f"GEO_ModuleBraid{side:+.0f}",
                             (side * 0.40, 0.10, -0.22), 0.10, 0.78,
                             collection, material, vertices=6)
        elif style == 5:
            add_box("GEO_ModuleHairFringe", (0.16, -0.38, 0.25),
                    (0.56, 0.20, 0.32), collection, material, width=0.08,
                    rotation=(0.0, 0.14, 0.08))
        elif style == 6:
            add_box("GEO_ModuleHairCrest", (0.0, 0.02, 0.64),
                    (0.18, 0.45, 0.62), collection, material, width=0.09)
        elif style == 7:
            for side in (-1.0, 1.0):
                add_box(f"GEO_ModuleHairSide{side:+.0f}",
                        (side * 0.38, 0.0, -0.05), (0.25, 0.64, 0.75),
                        collection, material, width=0.10)
    else:
        raise ValueError(f"unknown dynamic module slot {slot}")


def apply_modifiers(obj: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for modifier in tuple(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=modifier.name)


def consolidate(collection: bpy.types.Collection, asset_id: str,
                material: bpy.types.Material,
                palette_slot: str, surface_labels: bool = False) -> bpy.types.Object:
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
    if not objects:
        raise RuntimeError(f"{asset_id} generated no mesh")
    for obj in objects:
        apply_modifiers(obj)
        object_palette_slot = str(obj.get("cc_palette_slot", palette_slot))
        semantic_index = PALETTE_INDEX[object_palette_slot]
        paint_channels.add_indexed_paint_channels(
            obj, [semantic_index] * len(obj.data.polygons), PAINT_SEMANTICS,
            surface_labels=surface_labels,
            value_offset=float(obj.get("cc_value_offset", 0.0)))
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    joined = objects[0]
    joined.name = f"GEO_{asset_id}"
    joined.data.name = joined.name
    joined.data.materials.clear()
    joined.data.materials.append(material)
    for polygon in joined.data.polygons:
        polygon.material_index = 0
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    joined["cc_asset_id"] = asset_id
    joined["cc_rigid_module"] = True
    joined["cc_material_contract"] = paint_channels.SURFACE_CONTRACT \
        if surface_labels else "COLOR_0:palette,value,fold"
    if surface_labels:
        joined["cc_surface_classes"] = list(paint_channels.SURFACE_CLASSES)
    return joined


def export_model(model: bpy.types.Object, asset_id: str) -> Path:
    path = EXPORT_DIR / f"{asset_id}.glb"
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    model.select_set(True)
    bpy.context.view_layer.objects.active = model
    bpy.ops.export_scene.gltf(
        filepath=str(path), export_format="GLB", use_selection=True,
        export_yup=True, export_animations=False, export_skins=False,
        export_morph=False, export_extras=True, export_materials="EXPORT",
        export_vertex_color="ACTIVE")
    model.select_set(False)
    model.hide_render = True
    model.hide_set(True)
    return path


def build(selected_slots: set[str] | None = None) -> None:
    reset_scene()
    material = make_material()
    specs = (
        ("torso", "torso", "spine_to_neck", "outer"),
        ("pelvis", "pelvis", "pelvis_to_spine", "trousers"),
        ("upper_arm", "upper_arm", "bone_segment", "outer"),
        ("forearm", "forearm", "bone_segment", "underlayer"),
        ("thigh", "thigh", "bone_segment", "trousers"),
        ("shin", "shin", "bone_segment", "trousers_shadow"),
        ("hand", "hand", "bone_head", "skin"),
        ("foot", "foot", "bone_segment", "leather"),
        ("head", "head", "head_center", "skin"),
        ("mantle", "mantle", "back_socket", "trousers"),
        ("coat_tail", "coat_tail", "pelvis", "outer_shadow"),
        ("chest_plate", "chest_plate", "chest_front_socket", "metal"),
        ("chest_plate_raider", "chest_plate_raider", "chest_front_socket",
         "metal"),
        ("chest_plate_hero", "chest_plate_hero", "chest_front_socket",
         "metal"),
        ("pauldron", "pauldron", "shoulder_socket", "metal"),
        ("pauldron_raider", "pauldron_raider", "shoulder_socket", "metal"),
        ("pauldron_hero", "pauldron_hero", "shoulder_socket", "metal"),
        ("apron", "apron", "pelvis", "underlayer"),
        ("pack", "pack", "back_socket", "leather"),
        ("satchel", "satchel", "pelvis", "leather"),
        ("helmet", "helmet", "head_center", "metal"),
        ("hat", "hat", "head_center", "outer"),
        ("hood", "hood", "head_center", "outer"),
        ("headwrap", "headwrap", "head_center", "outer"),
        ("tool_shaft", "tool_shaft", "hand_grip", "leather"),
        ("tool_head", "tool_head", "tool_tip", "metal"),
    ) + tuple((f"hair_{index}", f"hair_{index}", "head_center", "hair")
              for index in range(8))
    records: list[ModuleRecord] = []
    for suffix, slot, anchor, palette in specs:
        asset_id = f"npc_module_{suffix}_v01"
        path = EXPORT_DIR / f"{asset_id}.glb"
        if selected_slots is None or slot in selected_slots:
            collection = collection_for(asset_id)
            build_geometry(slot, collection, material)
            model = consolidate(collection, asset_id, material, palette,
                                slot in MATERIAL_PILOT_SLOTS)
            path = export_model(model, asset_id)
        records.append(ModuleRecord(
            id=asset_id, slot=slot, anchor=anchor, material=palette,
            export=str(path.relative_to(ROOT)),
            shape_contract="layered closed torso armor"
            if slot.startswith("chest_plate") else
            "layered shoulder armor"
            if slot.startswith("pauldron") else
            "fitted bib with split skirt"
            if slot == "apron" else "rigid fitted module"))
    manifest = {
        "library_version": LIBRARY_VERSION,
        "generation": "offline procedural rigid character modules",
        "runtime_strategy": "bone-frame instancing without skins or animations",
        "coordinate_system": "glTF +Y up, +Z forward",
        "material_contract": "single indexed material; COLOR_0 stores palette, value, and fold",
        "surface_contract": paint_channels.SURFACE_CONTRACT,
        "surface_classes": list(paint_channels.SURFACE_CLASSES),
        "surface_modules": sorted(MATERIAL_PILOT_SLOTS),
        "modules": [asdict(record) for record in records],
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8")
    if selected_slots is None:
        BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    print(f"built {len(records)} rigid NPC modules")
    print(f"manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--material-pilot", action="store_true")
    arguments = parser.parse_args(sys.argv[sys.argv.index("--") + 1:]
                                  if "--" in sys.argv else [])
    build(MATERIAL_PILOT_SLOTS if arguments.material_pilot else None)
