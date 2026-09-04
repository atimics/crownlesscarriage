#!/usr/bin/env python3
"""Build the shared Crownless action-figure world kit and review sheets."""

from __future__ import annotations

from dataclasses import asdict, dataclass
import json
import math
from pathlib import Path
import sys
from typing import Iterable

import bpy
from mathutils import Matrix, Vector

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import paint_channels


ROOT = Path(__file__).resolve().parents[2]
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_world_kit.blend"
EXPORT_DIR = ROOT / "assets" / "exports" / "world_kit"
PREVIEW_DIR = ROOT / "assets" / "previews" / "world_kit"
MANIFEST_PATH = ROOT / "assets" / "world_kit_manifest.json"
CONNECTION_PATH = ROOT / "assets" / "world_kit_connections.json"
LIBRARY_VERSION = "0.5.1"

MATERIALS: dict[str, bpy.types.Material] = {}
ASSET_COLLECTIONS: dict[str, bpy.types.Collection] = {}
ASSET_RECORDS: list[dict[str, object]] = []
CONNECTION_RECORDS: list[dict[str, object]] = []
BODY_SKIN_RECORDS: list[dict[str, object]] = []
BODY_SKIN_OBJECTS: dict[tuple[str, str, str], bpy.types.Object] = {}

BODY_PAINT_SEMANTICS = (
    "skin", "hair", "underlayer", "outer", "trousers",
    "leather", "metal", "accent", "eye",
)




BODY_RIG_BONES = (
    ("root", None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.18)),
    ("pelvis", "root", (0.0, 0.0, 0.90), (0.0, 0.0, 1.10)),
    ("spine", "pelvis", (0.0, 0.0, 1.08), (0.0, 0.0, 1.38)),
    ("chest", "spine", (0.0, 0.0, 1.36), (0.0, 0.0, 1.62)),
    ("neck", "chest", (0.0, 0.0, 1.60), (0.0, 0.0, 1.76)),
    ("head", "neck", (0.0, 0.0, 1.74), (0.0, 0.0, 2.08)),
    ("upper_arm.L", "chest", (-0.29, 0.0, 1.58), (-0.46, 0.0, 1.26)),
    ("forearm.L", "upper_arm.L", (-0.46, 0.0, 1.26), (-0.57, -0.015, 0.94)),
    ("hand.L", "forearm.L", (-0.57, -0.015, 0.94), (-0.58, -0.045, 0.78)),
    ("upper_arm.R", "chest", (0.29, 0.0, 1.58), (0.46, 0.0, 1.26)),
    ("forearm.R", "upper_arm.R", (0.46, 0.0, 1.26), (0.57, -0.015, 0.94)),
    ("hand.R", "forearm.R", (0.57, -0.015, 0.94), (0.58, -0.045, 0.78)),
    ("thigh.L", "pelvis", (-0.14, 0.0, 1.00), (-0.15, 0.0, 0.56)),
    ("shin.L", "thigh.L", (-0.15, 0.0, 0.56), (-0.15, 0.0, 0.13)),
    ("foot.L", "shin.L", (-0.15, 0.0, 0.13), (-0.15, -0.27, 0.08)),
    ("thigh.R", "pelvis", (0.14, 0.0, 1.00), (0.15, 0.0, 0.56)),
    ("shin.R", "thigh.R", (0.15, 0.0, 0.56), (0.15, 0.0, 0.13)),
    ("foot.R", "shin.R", (0.15, 0.0, 0.13), (0.15, -0.27, 0.08)),
)

PALETTE = {
    "skin": (0.58, 0.33, 0.22, 1.0),
    "skin_dark": (0.38, 0.20, 0.14, 1.0),
    "hair": (0.045, 0.030, 0.024, 1.0),
    "underlayer": (0.19, 0.23, 0.23, 1.0),
    "teal": (0.035, 0.30, 0.28, 1.0),
    "oxblood": (0.43, 0.055, 0.065, 1.0),
    "soft_tissue": (0.72, 0.32, 0.14, 1.0),
    "blue": (0.075, 0.19, 0.29, 1.0),
    "ochre": (0.46, 0.30, 0.09, 1.0),
    "green": (0.15, 0.27, 0.12, 1.0),
    "cloth": (0.43, 0.38, 0.28, 1.0),
    "leather": (0.19, 0.075, 0.038, 1.0),
    "metal": (0.27, 0.32, 0.33, 1.0),
    "gold": (0.62, 0.39, 0.08, 1.0),
    "wood": (0.34, 0.14, 0.055, 1.0),
    "wood_dark": (0.16, 0.065, 0.032, 1.0),
    "stone": (0.34, 0.35, 0.32, 1.0),
    "stone_dark": (0.17, 0.19, 0.18, 1.0),
    "plaster": (0.54, 0.49, 0.38, 1.0),
    "roof": (0.27, 0.095, 0.065, 1.0),
    "cream": (0.78, 0.72, 0.58, 1.0),
    "glass": (0.025, 0.12, 0.15, 1.0),
    "ink": (0.008, 0.012, 0.013, 1.0),
    "ground": (0.12, 0.18, 0.14, 1.0),
    "silhouette_bg": (0.70, 0.67, 0.57, 1.0),
}


@dataclass(frozen=True)
class Buck:
    id: str
    chest: float
    waist: float
    hip: float
    shoulder: float
    upper_arm: float
    forearm: float
    thigh: float
    shin: float
    hand: tuple[float, float, float]
    foot: tuple[float, float, float]


BUCKS = {
    "lean": Buck("lean", 0.255, 0.165, 0.175, 0.285,
                 0.058, 0.048, 0.082, 0.061,
                 (0.085, 0.052, 0.145), (0.105, 0.245, 0.125)),
    "standard": Buck("standard", 0.295, 0.190, 0.195, 0.325,
                     0.070, 0.056, 0.098, 0.070,
                     (0.095, 0.058, 0.155), (0.115, 0.265, 0.135)),
    "heavy": Buck("heavy", 0.345, 0.245, 0.235, 0.370,
                  0.086, 0.070, 0.118, 0.085,
                  (0.108, 0.066, 0.165), (0.128, 0.280, 0.145)),
}



MUSCLE_PROFILES = {
    "slight": {
        "chest": 0.88, "waist": 0.92, "pelvis": 0.92,
        "shoulder": 0.88, "upper_arm": 0.86, "forearm": 0.88,
        "thigh": 0.90, "calf": 0.90,
    },
    "athletic": {
        "chest": 1.00, "waist": 1.00, "pelvis": 1.00,
        "shoulder": 1.00, "upper_arm": 1.00, "forearm": 1.00,
        "thigh": 1.00, "calf": 1.00,
    },
    "power": {
        "chest": 1.12, "waist": 1.08, "pelvis": 1.10,
        "shoulder": 1.14, "upper_arm": 1.17, "forearm": 1.13,
        "thigh": 1.16, "calf": 1.14,
    },
}




SOFT_TISSUE_PROFILES = {
    "low": {
        "chest": 0.000, "abdomen": 0.000, "waist": 0.000,
        "hip": 0.000, "upper_arm": 0.000, "forearm": 0.000,
        "thigh": 0.000, "calf": 0.000,
    },
    "balanced": {
        "chest": 0.012, "abdomen": 0.014, "waist": 0.010,
        "hip": 0.014, "upper_arm": 0.006, "forearm": 0.004,
        "thigh": 0.010, "calf": 0.004,
    },
    "central": {
        "chest": 0.020, "abdomen": 0.055, "waist": 0.042,
        "hip": 0.026, "upper_arm": 0.010, "forearm": 0.006,
        "thigh": 0.014, "calf": 0.006,
    },
    "lower_body": {
        "chest": 0.010, "abdomen": 0.025, "waist": 0.024,
        "hip": 0.050, "upper_arm": 0.008, "forearm": 0.004,
        "thigh": 0.038, "calf": 0.014,
    },
}


def body_envelope(frame: str, muscle_profile: str,
                  soft_tissue_profile: str) -> dict[str, float]:
    buck = BUCKS[frame]
    muscle = MUSCLE_PROFILES[muscle_profile]
    tissue = SOFT_TISSUE_PROFILES[soft_tissue_profile]
    return {
        "chest": buck.chest * muscle["chest"] + tissue["chest"],
        "waist": buck.waist * muscle["waist"] + tissue["waist"],
        "hip": buck.hip * muscle["pelvis"] + tissue["hip"],
        "upper_arm": buck.upper_arm * muscle["upper_arm"] + tissue["upper_arm"],
        "forearm": buck.forearm * muscle["forearm"] + tissue["forearm"],
        "thigh": buck.thigh * muscle["thigh"] + tissue["thigh"],
        "calf": buck.shin * muscle["calf"] + tissue["calf"],
        "abdomen_depth": 0.13 + tissue["abdomen"],
        "mid_torso_depth": 0.16 + tissue["abdomen"] * 0.72,
        "chest_depth": 0.18 + tissue["chest"] * 0.55,
        "pelvis_depth": 0.16 + tissue["hip"] * 0.55,
    }

HEADS = {
    "square": (0.225, 0.205, 0.305, 0.172),


    "long": (0.248, 0.218, 0.250, 0.180),
    "broad": (0.238, 0.215, 0.295, 0.185),
    "veteran": (0.220, 0.205, 0.310, 0.165),
}

SCALE_CONTRACT = {
    "unit": "meter",
    "adult_body_height": 1.90,
    "normal_adult_body_range": [1.75, 2.02],
    "hair_headgear_allowance": 0.12,
    "door_clear_opening": [1.02, 2.10],
    "door_outer_frame": [1.30, 2.24],
    "counter_height": 0.92,
    "seat_height": 0.46,
    "rail_height": 1.05,
    "floor_to_floor_range": [2.8, 3.2],
    "public_lane_minimum": 2.5,
    "runtime_whole_model_scale": 1.0,
}

FIGURE_SOCKETS = {
    "hand_left": [-0.35, -0.03, 0.92],
    "hand_right": [0.35, -0.03, 0.92],
    "forearm_left": [-0.37, 0.0, 1.08],
    "forearm_right": [0.37, 0.0, 1.08],
    "shoulder_left": [-0.325, 0.0, 1.50],
    "shoulder_right": [0.325, 0.0, 1.50],
    "chest_front": [0.0, -0.19, 1.38],
    "upper_back": [0.0, 0.18, 1.44],
    "lower_back": [0.0, 0.16, 1.10],
    "belt_left": [-0.19, 0.0, 1.02],
    "belt_right": [0.19, 0.0, 1.02],
    "head_top": [0.0, 0.0, 1.90],
    "face_front": [0.0, -0.105, 1.75],
    "foot_left": [-0.15, -0.08, 0.0],
    "foot_right": [0.15, -0.08, 0.0],
}


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_WORLD_KIT"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene.unit_settings.scale_length = 1.0
    scene["cc_library_version"] = LIBRARY_VERSION
    scene["cc_design_language"] = "80s action figure construction and playsets"
    scene["cc_unit"] = "meter"
    world = bpy.data.worlds.new("CC_WorldKitWorld")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.018, 0.027, 0.026, 1.0)
    background.inputs["Strength"].default_value = 0.34
    scene.world = world


def make_materials() -> None:
    for name, color in PALETTE.items():
        material = bpy.data.materials.new(f"MAT_WK_{name.upper()}")
        material.diffuse_color = color
        material.use_nodes = True
        principled = material.node_tree.nodes.get("Principled BSDF")
        principled.inputs["Base Color"].default_value = color
        principled.inputs["Roughness"].default_value = 0.82
        if name in {"metal", "gold"}:
            principled.inputs["Metallic"].default_value = 0.58
            principled.inputs["Roughness"].default_value = 0.42
        MATERIALS[name] = material


def new_collection(name: str, parent: bpy.types.Collection | None = None
                   ) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    (parent or bpy.context.scene.collection).children.link(collection)
    return collection


def new_asset(asset_id: str, *, kind: str, slot: str, anchor: str,
              compatible: Iterable[str], silhouette: Iterable[str],
              allowed_scale: tuple[float, float] = (0.98, 1.02),
              category: str | None = None,
              keywords: Iterable[str] = (),
              layer: str | None = None,
              ) -> bpy.types.Collection:
    compatible = tuple(compatible)
    silhouette = tuple(silhouette)
    keywords = tuple(dict.fromkeys((*keywords, slot, *silhouette)))
    collection = new_collection(asset_id.upper())
    collection["cc_asset_id"] = asset_id
    collection["cc_asset_kind"] = kind
    collection["cc_slot"] = slot
    collection["cc_anchor"] = anchor
    collection["cc_compatible_families"] = ",".join(compatible)
    collection["cc_library_version"] = LIBRARY_VERSION
    collection["cc_allowed_scale"] = allowed_scale
    collection["cc_silhouette_tags"] = ",".join(silhouette)
    collection["cc_category"] = category or kind
    collection["cc_keywords"] = ",".join(keywords)
    collection["cc_figure_layer"] = layer or layer_for_kind(kind)
    ASSET_COLLECTIONS[asset_id] = collection
    ASSET_RECORDS.append({
        "id": asset_id,
        "kind": kind,
        "model_kind": "component",
        "layer": layer or layer_for_kind(kind),
        "category": category or kind,
        "keywords": list(keywords),
        "slot": slot,
        "anchor": anchor,
        "compatible_families": list(compatible),
        "allowed_scale": list(allowed_scale),
        "silhouette_tags": list(silhouette),
        "export": f"assets/exports/world_kit/{asset_id}.glb",
    })
    CONNECTION_RECORDS.append({
        "asset_id": asset_id,
        "source": "shadow_metadata",
        "requires": [{
            "anchor": anchor,
            "profile": connection_profile(kind, slot, anchor),
            "local_transform": identity_transform(),
        }],
        "offers": [],
        "inherits": [],
        "clear_inherited": [],
    })
    return collection


def identity_transform() -> dict[str, list[float]]:
    return {
        "translation": [0.0, 0.0, 0.0],
        "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
        "scale": [1.0, 1.0, 1.0],
    }


def connection_profile(kind: str, slot: str, anchor: str) -> str:
    if kind == "skeleton_part":
        return "skeleton_joint"
    if kind == "muscle_module":
        return "muscle_bed"
    if kind == "soft_tissue_module":
        return "soft_tissue_bed"
    if kind in {"figure_core", "body_surface"}:
        return "bone_frame" if slot not in {"hand", "foot"} else f"{slot}_frame"
    if kind in {"head_family", "molded_hair", "identity_shell"}:
        return "head_mount"
    if kind in {"fitted_shell", "garment_shell", "armor_shell"}:
        return "fitted_shell"
    if kind == "prop":
        return "hand_grip"
    if kind in {"facade_part", "roof_part"}:
        return "building_bay"
    if kind in {"place_module", "state_module"}:
        return "surface_mount"
    return anchor


def layer_for_kind(kind: str) -> str:
    return {
        "skeleton_part": "skeleton",
        "muscle_module": "muscle",
        "soft_tissue_module": "soft_tissue",
        "figure_core": "skin_surface",
        "body_surface": "skin_surface",
        "head_family": "skin_surface",
        "molded_hair": "identity",
        "garment_shell": "garment",
        "fitted_shell": "garment",
        "armor_shell": "armor",
        "identity_shell": "identity",
        "prop": "equipment",
        "facade_part": "structure",
        "roof_part": "structure",
        "place_module": "place",
        "state_module": "state",
    }.get(kind, "unspecified")


def finish(obj: bpy.types.Object, collection: bpy.types.Collection,
           material: str, role: str) -> bpy.types.Object:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[material])
    obj["cc_material_role"] = material
    obj["cc_part_role"] = role
    if "cc_asset_id" in collection:
        obj["cc_asset_id"] = collection["cc_asset_id"]
        obj["cc_library_version"] = LIBRARY_VERSION
    return obj


def bevel(obj: bpy.types.Object, width: float = 0.015) -> None:
    modifier = obj.modifiers.new("CC_MoldedEdge", "BEVEL")
    modifier.width = width
    modifier.segments = 1
    modifier.limit_method = "ANGLE"


def add_box(name: str, center: tuple[float, float, float],
            dimensions: tuple[float, float, float], collection: bpy.types.Collection,
            material: str, role: str, *, width: float = 0.02,
            rotation: tuple[float, float, float] = (0.0, 0.0, 0.0)
            ) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=center, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if width > 0.0:
        bevel(obj, min(width, min(dimensions) * 0.20))
    return finish(obj, collection, material, role)


def add_ico(name: str, center: tuple[float, float, float],
            scale: tuple[float, float, float], collection: bpy.types.Collection,
            material: str, role: str, *, subdivisions: int = 2
            ) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(
        subdivisions=subdivisions, radius=1.0, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, collection, material, role)


def add_segment(name: str, start: tuple[float, float, float] | Vector,
                end: tuple[float, float, float] | Vector,
                start_radius: float, end_radius: float,
                collection: bpy.types.Collection, material: str, role: str,
                *, vertices: int = 8) -> bpy.types.Object:
    a = Vector(start)
    b = Vector(end)
    axis = b - a
    if axis.length < 0.0001:
        raise ValueError(f"{name}: zero-length segment")
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices, radius1=start_radius, radius2=end_radius,
        depth=axis.length, location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = axis.to_track_quat("Z", "Y")
    return finish(obj, collection, material, role)


def add_loft(name: str, rings: Iterable[tuple[float, float, float]],
             collection: bpy.types.Collection, material: str, role: str,
             *, center: tuple[float, float, float] = (0.0, 0.0, 0.0),
             sides: int = 8) -> bpy.types.Object:
    rings = tuple(rings)
    cx, cy, cz = center
    vertices: list[tuple[float, float, float]] = []
    for height, radius_x, radius_y in rings:
        for index in range(sides):
            angle = math.tau * index / sides
            vertices.append((cx + math.cos(angle) * radius_x,
                             cy + math.sin(angle) * radius_y,
                             cz + height))
    faces: list[tuple[int, ...]] = [tuple(reversed(range(sides)))]
    for row in range(len(rings) - 1):
        first = row * sides
        following = (row + 1) * sides
        for index in range(sides):
            nxt = (index + 1) % sides
            faces.append((first + index, first + nxt,
                          following + nxt, following + index))
    top = (len(rings) - 1) * sides
    faces.append(tuple(range(top, top + sides)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[material])
    obj["cc_material_role"] = material
    obj["cc_part_role"] = role
    return obj


def add_panel(name: str, points: Iterable[tuple[float, float, float]],
              collection: bpy.types.Collection, material: str, role: str,
              *, thickness: float = 0.025) -> bpy.types.Object:
    points = tuple(points)
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(points, [], [tuple(range(len(points)))])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[material])
    obj["cc_material_role"] = material
    obj["cc_part_role"] = role
    solidify = obj.modifiers.new("CC_MoldedThickness", "SOLIDIFY")
    solidify.thickness = thickness
    bevel(obj, min(0.012, thickness * 0.35))
    return obj


def add_wedge(name: str, center: tuple[float, float, float],
              bottom: tuple[float, float], top: tuple[float, float],
              height: float, collection: bpy.types.Collection,
              material: str, role: str) -> bpy.types.Object:
    cx, cy, cz = center
    bw, bd = bottom
    tw, td = top
    z0 = cz - height * 0.5
    z1 = cz + height * 0.5
    vertices = [
        (cx - bw * 0.5, cy - bd * 0.5, z0),
        (cx + bw * 0.5, cy - bd * 0.5, z0),
        (cx + bw * 0.5, cy + bd * 0.5, z0),
        (cx - bw * 0.5, cy + bd * 0.5, z0),
        (cx - tw * 0.5, cy - td * 0.5, z1),
        (cx + tw * 0.5, cy - td * 0.5, z1),
        (cx + tw * 0.5, cy + td * 0.5, z1),
        (cx - tw * 0.5, cy + td * 0.5, z1),
    ]
    faces = ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
             (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[material])
    obj["cc_material_role"] = material
    obj["cc_part_role"] = role
    bevel(obj, min(0.012, height * 0.05))
    return obj


def add_hair_clump(name: str,
                   path: Iterable[tuple[float, float, float]],
                   widths: Iterable[float], depths: Iterable[float],
                   collection: bpy.types.Collection,
                   material: str = "hair") -> bpy.types.Object:
    path = tuple(Vector(point) for point in path)
    widths = tuple(widths)
    depths = tuple(depths)
    if len(path) < 3 or len(path) > 5 or not (
            len(path) == len(widths) == len(depths)):
        raise ValueError(f"{name}: invalid hair clump sections")
    vertices: list[tuple[float, float, float]] = []
    for index, center in enumerate(path):
        before = path[max(0, index - 1)]
        after = path[min(len(path) - 1, index + 1)]
        tangent = (after - before).normalized()
        side = Vector((0.0, 1.0, 0.0)).cross(tangent)
        if side.length_squared < 1.0e-8:
            side = Vector((1.0, 0.0, 0.0))
        side.normalize()
        thickness = tangent.cross(side).normalized()
        vertices.extend((
            tuple(center - side * widths[index] * 0.5),
            tuple(center - thickness * depths[index] * 0.5),
            tuple(center + side * widths[index] * 0.5),
            tuple(center + thickness * depths[index] * 0.5),
        ))
    faces: list[tuple[int, ...]] = [(3, 2, 1, 0)]
    for section in range(len(path) - 1):
        first = section * 4
        following = (section + 1) * 4
        for edge in range(4):
            nxt = (edge + 1) % 4
            faces.append((first + edge, first + nxt,
                          following + nxt, following + edge))
    final = (len(path) - 1) * 4
    faces.append((final, final + 1, final + 2, final + 3))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[material])
    obj["cc_material_role"] = material
    obj["cc_part_role"] = "molded_hair_clump"
    return obj


def add_head_geometry(family: str, center: tuple[float, float, float],
                      collection: bpy.types.Collection,
                      skin: str = "skin") -> None:
    width, depth, height, jaw = HEADS[family]
    x, y, z = center


    add_loft(f"GEO_Head_{family}_Shell", (
        (-height * 0.50, jaw * 0.36, depth * 0.34),
        (-height * 0.34, jaw * 0.50, depth * 0.44),
        (-height * 0.04, width * 0.50, depth * 0.50),
        ( height * 0.30, width * 0.48, depth * 0.48),
        ( height * 0.48, width * 0.38, depth * 0.39),
    ), collection, skin, "tapered_head_shell", center=(x, y, z), sides=10)


    nose_width = 0.026 if family != "veteran" else 0.032
    add_wedge(f"GEO_Head_{family}_Nose", (x, y - depth * 0.52, z - 0.008),
              (nose_width * 0.65, 0.018), (nose_width, 0.026),
              0.045, collection, skin, "nose_plane")



    if family == "veteran":
        add_box("GEO_Head_veteran_Brow", (x, y - depth * 0.49, z + 0.055),
                (width * 0.78, 0.024, 0.026), collection,
                skin, "strong_brow", width=0.006)


def hair_paths(style: str, center: tuple[float, float, float]
               ) -> tuple[tuple[tuple[tuple[float, float, float], ...],
                                tuple[float, ...], tuple[float, ...]], ...]:
    x, y, z = center
    def shifted(points: Iterable[tuple[float, float, float]]) -> tuple[tuple[float, float, float], ...]:
        return tuple((x + px, y + py, z + pz) for px, py, pz in points)

    crown_l = (shifted(((-0.02, -0.015, 0.155), (-0.070, -0.025, 0.145),
                        (-0.120, -0.005, 0.080), (-0.110, 0.015, -0.010))),
               (0.17, 0.16, 0.12, 0.018), (0.16, 0.14, 0.10, 0.018))
    crown_r = (shifted(((0.02, -0.015, 0.155), (0.070, -0.025, 0.145),
                        (0.120, -0.005, 0.080), (0.110, 0.015, -0.010))),
               (0.17, 0.16, 0.12, 0.018), (0.16, 0.14, 0.10, 0.018))
    back_l = (shifted(((-0.055, 0.055, 0.135), (-0.105, 0.095, 0.075),
                       (-0.115, 0.105, -0.035), (-0.075, 0.095, -0.125))),
              (0.17, 0.16, 0.105, 0.018), (0.13, 0.12, 0.08, 0.018))
    back_r = (shifted(((0.055, 0.055, 0.135), (0.105, 0.095, 0.075),
                       (0.115, 0.105, -0.035), (0.075, 0.095, -0.125))),
              (0.17, 0.16, 0.105, 0.018), (0.13, 0.12, 0.08, 0.018))



    back_center = (shifted(((0.0, 0.065, 0.145), (0.0, 0.112, 0.075),
                            (0.0, 0.122, -0.035), (0.0, 0.100, -0.130))),
                   (0.145, 0.145, 0.105, 0.018),
                   (0.125, 0.125, 0.080, 0.018))
    bang_l = (shifted(((-0.060, -0.055, 0.125), (-0.075, -0.105, 0.060),
                       (-0.055, -0.120, -0.015), (-0.020, -0.118, -0.095))),
              (0.13, 0.12, 0.075, 0.014), (0.11, 0.095, 0.055, 0.014))
    bang_r = (shifted(((0.060, -0.055, 0.125), (0.080, -0.105, 0.065),
                       (0.070, -0.120, -0.005), (0.045, -0.118, -0.075))),
              (0.13, 0.115, 0.068, 0.014), (0.11, 0.09, 0.05, 0.014))

    if style == "cropped":
        return crown_l, crown_r, back_l, back_r, back_center
    if style == "swept":
        long_side = (shifted(((-0.090, -0.020, 0.125), (-0.125, -0.075, 0.035),
                              (-0.130, -0.070, -0.085), (-0.110, -0.025, -0.185))),
                     (0.14, 0.13, 0.08, 0.015), (0.12, 0.10, 0.06, 0.015))
        return crown_l, crown_r, bang_l, bang_r, back_r, long_side
    if style == "bob":
        side_l = (shifted(((-0.09, -0.005, 0.12), (-0.135, -0.045, 0.015),
                           (-0.14, -0.015, -0.10), (-0.105, 0.015, -0.19))),
                  (0.15, 0.14, 0.09, 0.016), (0.13, 0.11, 0.06, 0.016))
        side_r = tuple((tuple((-px + 2*x, py, pz) for px, py, pz in side_l[0]),
                        side_l[1], side_l[2]))
        return crown_l, crown_r, bang_l, bang_r, side_l, side_r
    if style == "crest":
        crest = (shifted(((0.0, 0.045, 0.12), (0.0, 0.035, 0.22),
                          (0.0, 0.015, 0.30), (0.0, -0.005, 0.34))),
                 (0.11, 0.105, 0.07, 0.018), (0.18, 0.16, 0.12, 0.018))
        return crown_l, crown_r, back_l, back_r, crest
    if style == "braided":
        braid = (shifted(((0.095, 0.065, 0.08), (0.13, 0.105, -0.06),
                          (0.12, 0.115, -0.22), (0.085, 0.105, -0.36))),
                 (0.105, 0.09, 0.065, 0.014), (0.095, 0.08, 0.055, 0.014))
        return crown_l, crown_r, bang_l, back_l, back_r, braid
    if style == "rear_lock":
        rear = (shifted(((0.0, 0.07, 0.14), (-0.03, 0.13, 0.015),
                         (-0.02, 0.14, -0.17), (0.025, 0.12, -0.34))),
                (0.22, 0.20, 0.12, 0.018), (0.14, 0.12, 0.075, 0.018))
        return crown_l, crown_r, bang_l, bang_r, rear
    raise ValueError(f"unknown hair style {style}")


def add_hair_style(style: str, center: tuple[float, float, float],
                   collection: bpy.types.Collection) -> None:
    for index, (path, widths, depths) in enumerate(hair_paths(style, center)):
        add_hair_clump(f"GEO_Hair_{style}_{index + 1:02d}", path,
                       widths, depths, collection)


def add_record_contract(asset_id: str, key: str, value: object) -> None:
    for record in ASSET_RECORDS:
        if record["id"] == asset_id:
            record[key] = value
            return
    raise KeyError(asset_id)


def build_skeleton_and_muscle_assets() -> None:
    skeleton_specs = (
        ("spine", "pelvis_to_neck", 0.045, 0.038),
        ("pelvis", "pelvis_frame", 0.065, 0.065),
        ("upper_arm", "shoulder_joint", 0.042, 0.035),
        ("forearm", "elbow_joint", 0.036, 0.028),
        ("thigh", "hip_joint", 0.050, 0.040),
        ("shin", "knee_joint", 0.042, 0.032),
    )
    for slot, anchor, root_radius, tip_radius in skeleton_specs:
        asset_id = f"wk_skeleton_{slot}_v01"
        part = new_asset(asset_id, kind="skeleton_part", slot=slot,
                         anchor=anchor, compatible=tuple(BUCKS),
                         silhouette=("internal", "bone_frame"),
                         allowed_scale=(1.0, 1.0), layer="skeleton")
        add_segment(f"GEO_Skeleton_{slot}", (0.0, 0.0, 0.0),
                    (0.0, 0.0, 1.0), root_radius, tip_radius,
                    part, "cream", "skeleton_bone", vertices=8)
        add_ico(f"GEO_Skeleton_{slot}_Joint", (0.0, 0.0, 0.0),
                (root_radius * 1.35,) * 3, part, "gold",
                "skeleton_joint", subdivisions=1)

    for slot, anchor, shape in (
        ("hand", "wrist_joint", (0.070, 0.035, 0.115)),
        ("foot", "ankle_joint", (0.080, 0.16, 0.075)),
    ):
        asset_id = f"wk_skeleton_{slot}_v01"
        part = new_asset(asset_id, kind="skeleton_part", slot=slot,
                         anchor=anchor, compatible=tuple(BUCKS),
                         silhouette=("internal", "contact_frame"),
                         allowed_scale=(1.0, 1.0), layer="skeleton")
        add_wedge(f"GEO_Skeleton_{slot}", (0.0, 0.0, 0.0),
                  (shape[0] * 0.72, shape[1] * 0.82),
                  (shape[0], shape[1]), shape[2], part,
                  "cream", "skeleton_contact")

    muscle_specs = (
        ("chest", "ribcage", "broad_front", "chest"),
        ("back", "ribcage", "rear_taper", "chest"),
        ("abdomen", "spine", "waist_taper", "waist"),
        ("glute", "pelvis", "pelvis_mass", "pelvis"),
        ("deltoid", "shoulder", "shoulder_cap", "shoulder"),
        ("upper_arm", "upper_arm", "limb_taper", "upper_arm"),
        ("forearm", "forearm", "limb_taper", "forearm"),
        ("thigh", "thigh", "limb_taper", "thigh"),
        ("calf", "shin", "rear_bulge", "calf"),
        ("neck", "neck", "neck_column", "shoulder"),
    )
    for slot, anchor, silhouette, parameter in muscle_specs:
        asset_id = f"wk_muscle_{slot}_v01"
        muscle = new_asset(asset_id, kind="muscle_module", slot=slot,
                           anchor=anchor, compatible=tuple(BUCKS),
                           silhouette=(silhouette, "inflatable"),
                           allowed_scale=(1.0, 1.0), layer="muscle")
        if slot == "chest":
            for side in (-1.0, 1.0):
                add_ico(f"GEO_MuscleChest{side:+.0f}",
                        (side * 0.14, -0.055, 0.10),
                        (0.17, 0.10, 0.20), muscle,
                        "oxblood", "muscle_volume", subdivisions=1)
        elif slot == "back":
            add_wedge("GEO_MuscleBack", (0.0, 0.06, 0.08),
                      (0.32, 0.10), (0.50, 0.13), 0.48,
                      muscle, "oxblood", "muscle_volume")
        elif slot == "abdomen":
            add_loft("GEO_MuscleAbdomen", ((-0.22, 0.16, 0.10),
                                            (0.0, 0.18, 0.115),
                                            (0.22, 0.20, 0.12)),
                     muscle, "oxblood", "muscle_volume", sides=8)
        elif slot == "glute":
            for side in (-1.0, 1.0):
                add_ico(f"GEO_MuscleGlute{side:+.0f}",
                        (side * 0.105, 0.045, 0.0),
                        (0.14, 0.12, 0.16), muscle,
                        "oxblood", "muscle_volume", subdivisions=1)
        elif slot == "deltoid":
            add_ico("GEO_MuscleDeltoid", (0.0, 0.0, 0.0),
                    (0.115, 0.095, 0.12), muscle,
                    "oxblood", "muscle_volume", subdivisions=1)
        elif slot in {"upper_arm", "forearm", "thigh", "calf", "neck"}:
            radii = {
                "upper_arm": (0.090, 0.060), "forearm": (0.075, 0.052),
                "thigh": (0.125, 0.075), "calf": (0.095, 0.060),
                "neck": (0.085, 0.075),
            }[slot]
            add_segment(f"GEO_Muscle_{slot}", (0.0, 0.0, 0.0),
                        (0.0, 0.0, 1.0), radii[0], radii[1],
                        muscle, "oxblood", "muscle_volume")
        add_record_contract(asset_id, "morph_contract", {
            "parameter": f"muscle.{parameter}",
            "range": [0.0, 1.0],
            "operation": "inflate_cross_section",
            "preserves": ["bone_length", "anchor", "joint_axis"],
            "output": "muscle_fit_envelope",
        })


def build_soft_tissue_assets() -> None:
    tissue_specs = (
        ("chest", "muscle_chest", "upper_torso_padding"),
        ("abdomen", "muscle_abdomen", "front_abdomen_padding"),
        ("waist", "muscle_abdomen", "waist_padding"),
        ("hip", "muscle_glute", "hip_padding"),
        ("upper_arm", "muscle_upper_arm", "limb_padding"),
        ("forearm", "muscle_forearm", "limb_padding"),
        ("thigh", "muscle_thigh", "limb_padding"),
        ("calf", "muscle_calf", "limb_padding"),
    )
    for slot, anchor, silhouette in tissue_specs:
        asset_id = f"wk_soft_tissue_{slot}_v01"
        tissue = new_asset(
            asset_id, kind="soft_tissue_module", slot=slot, anchor=anchor,
            compatible=tuple(BUCKS),
            silhouette=(silhouette, "additive_volume", "mixable"),
            allowed_scale=(1.0, 1.0), layer="soft_tissue")
        if slot == "chest":
            for side in (-1.0, 1.0):
                add_ico(f"GEO_SoftTissueChest{side:+.0f}",
                        (side * 0.14, -0.095, 0.08),
                        (0.18, 0.055, 0.18), tissue,
                        "soft_tissue", "soft_tissue_volume", subdivisions=1)
        elif slot == "abdomen":
            add_ico("GEO_SoftTissueAbdomen", (0.0, -0.085, 0.0),
                    (0.19, 0.075, 0.22), tissue,
                    "soft_tissue", "soft_tissue_volume", subdivisions=1)
        elif slot == "waist":
            add_loft("GEO_SoftTissueWaist", ((-0.16, 0.19, 0.10),
                                               (0.0, 0.21, 0.115),
                                               (0.16, 0.19, 0.10)),
                     tissue, "soft_tissue", "soft_tissue_volume", sides=8)
        elif slot == "hip":
            for side in (-1.0, 1.0):
                add_ico(f"GEO_SoftTissueHip{side:+.0f}",
                        (side * 0.13, 0.015, 0.0),
                        (0.16, 0.115, 0.17), tissue,
                        "soft_tissue", "soft_tissue_volume", subdivisions=1)
        else:
            radii = {
                "upper_arm": (0.098, 0.064),
                "forearm": (0.080, 0.054),
                "thigh": (0.138, 0.080),
                "calf": (0.102, 0.062),
            }[slot]
            add_segment(f"GEO_SoftTissue_{slot}", (0.0, 0.0, 0.0),
                        (0.0, 0.0, 1.0), radii[0], radii[1], tissue,
                        "soft_tissue", "soft_tissue_volume")
        add_record_contract(asset_id, "volume_contract", {
            "parameter": f"soft_tissue.{slot}",
            "unit": "meter",
            "range": [0.0, 0.06],
            "operation": "add_soft_volume",
            "preserves": ["bone_length", "joint_center", "socket_transform",
                          "muscle_attachment"],
            "output": "soft_tissue_fit_envelope",
        })


def build_body_assets() -> None:
    for buck in BUCKS.values():
        prefix = f"wk_buck_{buck.id}"
        torso = new_asset(f"{prefix}_torso_v01", kind="body_surface",
                          slot="torso", anchor="spine_to_neck",
                          compatible=(buck.id,), silhouette=(buck.id, "tapered"))
        add_loft("GEO_Torso", ((0.00, buck.waist, 0.13),
                               (0.34, buck.chest * 0.82, 0.16),
                               (0.76, buck.chest, 0.18),
                               (1.00, buck.shoulder * 0.94, 0.155)),
                 torso, "skin", "derived_skin_torso", sides=10)

        pelvis = new_asset(f"{prefix}_pelvis_v01", kind="body_surface",
                           slot="pelvis", anchor="pelvis_to_spine",
                           compatible=(buck.id,), silhouette=(buck.id, "pelvis"))
        add_loft("GEO_Pelvis", ((0.00, buck.hip * 0.88, 0.145),
                                (0.48, buck.hip, 0.165),
                                (1.00, buck.waist, 0.135)),
                 pelvis, "skin", "derived_skin_pelvis", sides=10)

        core_parts = (
            ("upper_arm", buck.upper_arm * 1.10, buck.upper_arm * 0.78),
            ("forearm", buck.forearm * 1.18, buck.forearm * 0.74),
            ("thigh", buck.thigh * 1.12, buck.thigh * 0.72),
            ("shin", buck.shin * 1.10, buck.shin * 0.70),
        )
        for slot, root_radius, tip_radius in core_parts:
            part = new_asset(f"{prefix}_{slot}_v01", kind="body_surface",
                             slot=slot, anchor="bone_segment",
                             compatible=(buck.id,),
                             silhouette=(buck.id, "tapered", slot))
            add_segment(f"GEO_{slot}", (0.0, 0.0, 0.0),
                        (0.0, 0.0, 1.0), root_radius, tip_radius,
                        part, "skin", f"derived_skin_{slot}")

        hand = new_asset(f"{prefix}_hand_v01", kind="body_surface",
                         slot="hand", anchor="hand_center",
                         compatible=(buck.id,), silhouette=(buck.id, "grip"))
        add_wedge("GEO_Hand", (0.0, 0.0, 0.0),
                  (buck.hand[0] * 0.72, buck.hand[1]),
                  (buck.hand[0], buck.hand[1] * 1.12), buck.hand[2],
                  hand, "skin", "directional_hand")

        foot = new_asset(f"{prefix}_foot_v01", kind="body_surface",
                         slot="foot", anchor="ankle_to_toe",
                         compatible=(buck.id,), silhouette=(buck.id, "planted"))
        add_wedge("GEO_Foot", (0.0, -buck.foot[1] * 0.08, 0.0),
                  (buck.foot[0], buck.foot[1]),
                  (buck.foot[0] * 0.82, buck.foot[1] * 0.72),
                  buck.foot[2], foot, "skin", "derived_skin_foot")

        for slot in ("torso", "pelvis", "upper_arm", "forearm",
                     "thigh", "shin", "hand", "foot"):
            add_record_contract(f"{prefix}_{slot}_v01", "surface_contract", {
                "inputs": [f"skeleton_frame.{slot}",
                           f"muscle_fit_envelope.{slot}",
                           f"soft_tissue_fit_envelope.{slot}"],
                "operation": "derive_skin_wrap",
                "preserves": ["bone_length", "joint_center", "socket_transform"],
                "output": f"body_fit_envelope.{slot}",
            })

    for family in HEADS:
        head = new_asset(f"wk_head_{family}_v01", kind="head_family",
                         slot="head", anchor="head_center",
                         compatible=tuple(BUCKS),
                         silhouette=(family, "adult", "rounded_back"))
        add_head_geometry(family, (0.0, 0.0, 0.0), head)


def build_hair_assets() -> None:
    for style in ("cropped", "swept", "bob", "crest", "braided", "rear_lock"):
        hair = new_asset(f"wk_hair_{style}_v01", kind="molded_hair",
                         slot="hair", anchor="head_center",
                         compatible=tuple(HEADS),
                         silhouette=(style, "clumped", "tapered"))
        add_hair_style(style, (0.0, 0.0, 0.0), hair)


def build_shell_assets() -> None:
    compatible = tuple(BUCKS)
    chest_tunic = new_asset("wk_shell_chest_tunic_v01", kind="garment_shell",
                            slot="chest", anchor="chest_front",
                            compatible=compatible,
                            silhouette=("layered", "tapered"))
    add_loft("GEO_Tunic", ((-0.24, 0.205, 0.155),
                            (0.00, 0.225, 0.175),
                            (0.38, 0.305, 0.195),
                            (0.50, 0.315, 0.175)),
             chest_tunic, "teal", "chest_shell", sides=10)

    cuirass = new_asset("wk_shell_chest_cuirass_v01", kind="armor_shell",
                        slot="chest", anchor="chest_front",
                        compatible=("standard", "heavy"),
                        silhouette=("armored", "broad"))
    add_wedge("GEO_Cuirass", (0.0, -0.02, 0.0),
              (0.43, 0.13), (0.60, 0.19), 0.52,
              cuirass, "metal", "front_plate")
    add_box("GEO_CuirassRidge", (0.0, -0.092, 0.04),
            (0.065, 0.035, 0.38), cuirass, "gold", "molded_ridge", width=0.009)

    belt = new_asset("wk_shell_waist_belt_v01", kind="garment_shell",
                     slot="waist", anchor="belt",
                     compatible=compatible, silhouette=("belt", "compact"))
    add_box("GEO_Belt", (0.0, 0.0, 0.0), (0.43, 0.22, 0.095),
            belt, "leather", "belt", width=0.025)
    add_box("GEO_Buckle", (0.0, -0.13, 0.0), (0.085, 0.035, 0.075),
            belt, "gold", "buckle", width=0.009)

    apron = new_asset("wk_shell_waist_apron_v01", kind="garment_shell",
                      slot="waist", anchor="belt",
                      compatible=compatible,
                      silhouette=("work", "front_weight"))
    add_panel("GEO_Apron", ((-0.21, -0.02, 0.18), (0.21, -0.02, 0.18),
                            (0.18, -0.035, -0.48), (-0.18, -0.035, -0.48)),
              apron, "cloth", "apron", thickness=0.035)

    mantle = new_asset("wk_shell_shoulder_mantle_left_v01", kind="garment_shell",
                       slot="shoulder_left", anchor="shoulder_left",
                       compatible=compatible,
                       silhouette=("left_heavy", "cloth"))
    add_panel("GEO_MantleLeft", ((-0.31, 0.0, 0.14), (0.16, 0.0, 0.10),
                                 (0.13, 0.07, -0.35), (-0.18, 0.09, -0.56),
                                 (-0.36, 0.07, -0.30)),
              mantle, "oxblood", "shoulder_mantle", thickness=0.04)

    pauldron = new_asset("wk_shell_shoulder_pauldron_v01", kind="armor_shell",
                         slot="shoulder", anchor="shoulder",
                         compatible=("standard", "heavy"),
                         silhouette=("armor", "hard_edge"))
    add_ico("GEO_Pauldron", (0.0, 0.0, 0.0), (0.145, 0.12, 0.115),
            pauldron, "metal", "pauldron", subdivisions=1)
    add_box("GEO_PauldronLip", (0.0, -0.055, -0.075),
            (0.25, 0.055, 0.055), pauldron, "gold", "pauldron_lip", width=0.012)

    cape = new_asset("wk_shell_back_cape_v01", kind="garment_shell",
                     slot="back", anchor="upper_back", compatible=compatible,
                     silhouette=("rear_heavy", "long_cloth"))
    add_panel("GEO_Cape", ((-0.24, 0.0, 0.18), (0.24, 0.0, 0.18),
                           (0.30, 0.08, -0.72), (0.07, 0.11, -0.92),
                           (-0.26, 0.08, -0.78)),
              cape, "oxblood", "cape", thickness=0.035)

    pack = new_asset("wk_shell_back_pack_v01", kind="fitted_shell",
                     slot="back", anchor="upper_back", compatible=compatible,
                     silhouette=("rear_heavy", "travel"), layer="equipment")
    add_box("GEO_Pack", (0.0, 0.0, 0.0), (0.42, 0.19, 0.48),
            pack, "leather", "pack", width=0.055)
    add_segment("GEO_Bedroll", (-0.22, 0.0, 0.30), (0.22, 0.0, 0.30),
                0.095, 0.095, pack, "cloth", "bedroll", vertices=8)

    bracer = new_asset("wk_shell_forearm_bracer_v01", kind="armor_shell",
                       slot="forearm", anchor="forearm",
                       compatible=compatible,
                       silhouette=("hard_edge", "forearm"))
    add_segment("GEO_Bracer", (0.0, 0.0, -0.18), (0.0, 0.0, 0.18),
                0.080, 0.065, bracer, "leather", "bracer")

    glove = new_asset("wk_shell_hand_glove_v01", kind="garment_shell",
                      slot="hand", anchor="hand_center", compatible=compatible,
                      silhouette=("grip", "cuff"))
    add_wedge("GEO_Glove", (0.0, 0.0, 0.0), (0.075, 0.055),
              (0.105, 0.067), 0.18, glove, "leather", "glove")

    greave = new_asset("wk_shell_shin_greave_v01", kind="armor_shell",
                       slot="shin", anchor="shin", compatible=compatible,
                       silhouette=("armor", "shin"))
    add_segment("GEO_Greave", (0.0, 0.0, -0.20), (0.0, 0.0, 0.20),
                0.095, 0.070, greave, "metal", "greave")

    boot = new_asset("wk_shell_foot_boot_v01", kind="garment_shell",
                     slot="foot", anchor="ankle_to_toe", compatible=compatible,
                     silhouette=("planted", "directional"))
    add_wedge("GEO_Boot", (0.0, -0.02, 0.0), (0.13, 0.28),
              (0.105, 0.19), 0.15, boot, "leather", "boot")

    trouser_thigh = new_asset("wk_garment_trouser_thigh_v01",
                              kind="garment_shell", slot="thigh",
                              anchor="thigh", compatible=compatible,
                              silhouette=("trouser", "limb_shell"))
    add_segment("GEO_TrouserThigh", (0.0, 0.0, 0.0),
                (0.0, 0.0, 1.0), 0.125, 0.085,
                trouser_thigh, "underlayer", "trouser_shell")
    add_record_contract("wk_garment_trouser_thigh_v01", "fit_contract", {
        "input": "body_fit_envelope.thigh", "clearance_m": 0.010,
        "operation": "refit_cross_section", "preserves": ["hem", "joint_gap"],
    })

    trouser_shin = new_asset("wk_garment_trouser_shin_v01",
                             kind="garment_shell", slot="shin",
                             anchor="shin", compatible=compatible,
                             silhouette=("trouser", "limb_shell"))
    add_segment("GEO_TrouserShin", (0.0, 0.0, 0.0),
                (0.0, 0.0, 1.0), 0.090, 0.065,
                trouser_shin, "underlayer", "trouser_shell")
    add_record_contract("wk_garment_trouser_shin_v01", "fit_contract", {
        "input": "body_fit_envelope.calf", "clearance_m": 0.010,
        "operation": "refit_cross_section", "preserves": ["hem", "joint_gap"],
    })

    crown = new_asset("wk_shell_head_crown_band_v01", kind="identity_shell",
                      slot="head_top", anchor="head_top", compatible=tuple(HEADS),
                      silhouette=("three_point", "small_accent"))
    add_box("GEO_CrownBand", (0.0, 0.0, -0.03), (0.18, 0.13, 0.045),
            crown, "gold", "crown_band", width=0.009)
    for index, x in enumerate((-0.065, 0.0, 0.065)):
        add_wedge(f"GEO_CrownPoint{index}", (x, 0.0, 0.035),
                  (0.035, 0.035), (0.012, 0.018), 0.12,
                  crown, "gold", "crown_point")

    for asset_id, envelope, clearance in (
        ("wk_shell_chest_tunic_v01", "torso", 0.012),
        ("wk_shell_chest_cuirass_v01", "torso_plus_garment", 0.018),
        ("wk_shell_waist_belt_v01", "waist_plus_garment", 0.010),
        ("wk_shell_waist_apron_v01", "waist_plus_garment", 0.014),
        ("wk_shell_shoulder_mantle_left_v01", "shoulder_plus_garment", 0.014),
        ("wk_shell_shoulder_pauldron_v01", "deltoid_plus_garment", 0.018),
        ("wk_shell_back_cape_v01", "upper_back_plus_garment", 0.016),
        ("wk_shell_forearm_bracer_v01", "forearm_plus_garment", 0.012),
        ("wk_shell_hand_glove_v01", "hand", 0.008),
        ("wk_shell_shin_greave_v01", "calf_plus_garment", 0.014),
        ("wk_shell_foot_boot_v01", "foot", 0.012),
    ):
        add_record_contract(asset_id, "fit_contract", {
            "input": f"body_fit_envelope.{envelope}",
            "clearance_m": clearance,
            "operation": "refit_cross_section",
            "forbidden": ["change_bone_length", "move_socket"],
        })


def build_prop_assets() -> None:
    specs = (
        ("sword", "two_hand", "balanced", 1.05),
        ("spear", "pole", "long", 1.90),
        ("hammer", "hand", "work", 0.48),
        ("lantern", "hand", "hanging", 0.32),
        ("axe", "two_hand", "top_heavy", 0.92),
        ("shovel", "two_hand", "work", 1.25),
        ("crossbow", "two_hand", "wide", 0.72),
        ("staff", "pole", "vertical", 1.75),
    )
    for name, size_class, silhouette, length in specs:
        asset = new_asset(f"wk_prop_{name}_v01", kind="prop", slot=size_class,
                          anchor="hand_grip", compatible=("figure",),
                          silhouette=(silhouette, size_class),
                          allowed_scale=(0.95, 1.05))
        if name == "sword":
            add_box("GEO_SwordBlade", (0.0, 0.0, 0.30),
                    (0.055, 0.018, 0.72), asset, "metal", "blade", width=0.008)
            add_box("GEO_SwordGuard", (0.0, 0.0, -0.075),
                    (0.25, 0.045, 0.045), asset, "gold", "guard", width=0.012)
            add_segment("GEO_SwordGrip", (0.0, 0.0, -0.08),
                        (0.0, 0.0, -0.31), 0.028, 0.025,
                        asset, "leather", "grip", vertices=8)
        elif name in {"spear", "staff", "shovel", "axe", "hammer"}:
            add_segment(f"GEO_{name}_Shaft", (0.0, 0.0, -length * 0.5),
                        (0.0, 0.0, length * 0.5), 0.025, 0.021,
                        asset, "wood", "shaft", vertices=8)
            if name == "spear":
                add_wedge("GEO_SpearHead", (0.0, 0.0, length * 0.5 + 0.10),
                          (0.075, 0.045), (0.008, 0.012), 0.23,
                          asset, "metal", "spear_head")
            elif name == "staff":
                add_ico("GEO_StaffCap", (0.0, 0.0, length * 0.5 + 0.035),
                        (0.055, 0.055, 0.065), asset, "gold", "staff_cap",
                        subdivisions=1)
            elif name == "shovel":
                add_wedge("GEO_ShovelBlade", (0.0, 0.0, length * 0.5 + 0.11),
                          (0.20, 0.07), (0.12, 0.05), 0.28,
                          asset, "metal", "shovel_blade")
            elif name == "axe":
                add_wedge("GEO_AxeHead", (0.08, 0.0, length * 0.5 - 0.04),
                          (0.31, 0.075), (0.19, 0.06), 0.25,
                          asset, "metal", "axe_head")
            elif name == "hammer":
                add_box("GEO_HammerHead", (0.0, 0.0, length * 0.5 - 0.02),
                        (0.27, 0.12, 0.13), asset, "metal", "hammer_head",
                        width=0.025)
        elif name == "lantern":
            add_box("GEO_LanternBody", (0.0, 0.0, -0.05),
                    (0.16, 0.13, 0.20), asset, "gold", "lantern_frame", width=0.018)
            add_box("GEO_LanternGlass", (0.0, -0.005, -0.05),
                    (0.105, 0.09, 0.14), asset, "glass", "lantern_glass", width=0.009)
            add_segment("GEO_LanternHandle", (-0.07, 0.0, 0.08),
                        (0.07, 0.0, 0.08), 0.012, 0.012,
                        asset, "metal", "handle", vertices=6)
        elif name == "crossbow":
            add_box("GEO_CrossbowStock", (0.0, 0.0, 0.0),
                    (0.09, 0.10, 0.58), asset, "wood", "stock", width=0.018)
            add_segment("GEO_CrossbowBow", (-0.36, 0.0, 0.20),
                        (0.36, 0.0, 0.20), 0.020, 0.020,
                        asset, "metal", "bow", vertices=8)


def build_facade_assets() -> None:
    building = ("building",)
    wall = new_asset("wk_facade_wall_v01", kind="facade_part", slot="facade_bay",
                     anchor="foundation_bay", compatible=building,
                     silhouette=("solid", "one_bay"))
    add_box("GEO_WallPanel", (0.0, 0.0, 1.40), (1.50, 0.22, 2.80),
            wall, "plaster", "wall_panel", width=0.025)
    for x in (-0.68, 0.68):
        add_box(f"GEO_WallPost{x:+.0f}", (x, -0.13, 1.40),
                (0.14, 0.16, 2.72), wall, "wood_dark", "frame_post", width=0.018)

    window = new_asset("wk_facade_window_v01", kind="facade_part", slot="facade_bay",
                       anchor="foundation_bay", compatible=building,
                       silhouette=("window", "one_bay"))
    add_box("GEO_WindowLower", (0.0, 0.0, 0.50), (1.50, 0.22, 1.00),
            window, "plaster", "wall_lower", width=0.025)
    add_box("GEO_WindowUpper", (0.0, 0.0, 2.32), (1.50, 0.22, 0.96),
            window, "plaster", "wall_upper", width=0.025)
    for x in (-0.61, 0.61):
        add_box(f"GEO_WindowSide{x:+.0f}", (x, 0.0, 1.48),
                (0.28, 0.22, 0.72), window, "plaster", "wall_side", width=0.025)
    add_box("GEO_WindowGlass", (0.0, -0.13, 1.48), (0.92, 0.035, 0.66),
            window, "glass", "window_glass", width=0.008)
    for x in (-0.68, 0.68):
        add_box(f"GEO_WindowPost{x:+.0f}", (x, -0.13, 1.40),
                (0.14, 0.16, 2.72), window, "wood_dark", "frame_post", width=0.018)

    door = new_asset("wk_facade_door_v01", kind="facade_part", slot="facade_bay",
                     anchor="foundation_bay", compatible=building,
                     silhouette=("threshold", "one_bay"))
    for x in (-0.62, 0.62):
        add_box(f"GEO_DoorSide{x:+.0f}", (x, 0.0, 1.40),
                (0.26, 0.22, 2.80), door, "plaster", "wall_side", width=0.025)
    add_box("GEO_DoorHeader", (0.0, 0.0, 2.48), (0.98, 0.22, 0.64),
            door, "plaster", "wall_header", width=0.025)
    add_box("GEO_DoorLeaf", (0.0, 0.05, 1.05), (1.02, 0.10, 2.10),
            door, "wood_dark", "door_leaf", width=0.025)
    add_box("GEO_DoorLintel", (0.0, -0.14, 2.18), (1.28, 0.17, 0.16),
            door, "wood", "lintel", width=0.018)

    corner = new_asset("wk_facade_corner_v01", kind="facade_part", slot="corner",
                       anchor="foundation_corner", compatible=building,
                       silhouette=("corner", "frame"))
    add_box("GEO_CornerPost", (0.0, 0.0, 1.40), (0.22, 0.22, 2.80),
            corner, "wood_dark", "corner_post", width=0.028)
    add_box("GEO_CornerFoot", (0.0, 0.0, 0.13), (0.34, 0.34, 0.26),
            corner, "stone_dark", "corner_foot", width=0.028)

    roof = new_asset("wk_roof_gable_span_v01", kind="roof_part", slot="roof_span",
                     anchor="wall_top", compatible=building,
                     silhouette=("gable", "wedge"))
    add_panel("GEO_RoofLeft", ((-0.86, -1.65, 0.0), (0.0, -1.65, 0.70),
                               (0.0, 1.65, 0.70), (-0.86, 1.65, 0.0)),
              roof, "roof", "roof_plane", thickness=0.10)
    add_panel("GEO_RoofRight", ((0.0, -1.65, 0.70), (0.86, -1.65, 0.0),
                                (0.86, 1.65, 0.0), (0.0, 1.65, 0.70)),
              roof, "roof", "roof_plane", thickness=0.10)
    add_segment("GEO_RoofRidge", (0.0, -1.70, 0.71),
                (0.0, 1.70, 0.71), 0.065, 0.065,
                roof, "wood_dark", "roof_ridge", vertices=8)

    awning = new_asset("wk_facade_awning_v01", kind="facade_part", slot="awning",
                       anchor="facade_front", compatible=building,
                       silhouette=("overhang", "work"))
    add_panel("GEO_Awning", ((-0.78, -0.02, 0.10), (0.78, -0.02, 0.10),
                             (0.72, -0.80, -0.20), (-0.72, -0.80, -0.20)),
              awning, "oxblood", "awning_cloth", thickness=0.045)
    for x in (-0.67, 0.67):
        add_segment(f"GEO_AwningBrace{x:+.0f}", (x, -0.03, 0.07),
                    (x, -0.75, -0.18), 0.025, 0.025,
                    awning, "wood_dark", "awning_brace", vertices=8)

    sign = new_asset("wk_facade_sign_v01", kind="facade_part", slot="sign",
                     anchor="sign_mount", compatible=building,
                     silhouette=("hanging", "identity"))
    add_segment("GEO_SignArm", (0.0, 0.0, 0.25), (0.46, 0.0, 0.25),
                0.025, 0.025, sign, "metal", "sign_arm", vertices=8)
    add_box("GEO_SignBoard", (0.40, 0.0, -0.02), (0.36, 0.07, 0.42),
            sign, "wood", "sign_board", width=0.035)

    counter = new_asset("wk_place_market_counter_v01", kind="place_module",
                        slot="work_area", anchor="floor",
                        compatible=("market", "shop"),
                        silhouette=("counter", "horizontal"))
    add_box("GEO_CounterBase", (0.0, 0.0, 0.43), (1.80, 0.62, 0.86),
            counter, "wood_dark", "counter_base", width=0.055)
    add_box("GEO_CounterTop", (0.0, -0.04, 0.89), (1.95, 0.72, 0.12),
            counter, "wood", "counter_top", width=0.035)

    stocked = new_asset("wk_state_market_stocked_v01", kind="state_module",
                        slot="counter_top", anchor="counter_top",
                        compatible=("market_counter",),
                        silhouette=("stocked", "abundant"))
    for index, x in enumerate((-0.55, 0.0, 0.55)):
        add_ico(f"GEO_StockSack{index}", (x, 0.0, 0.16),
                (0.20, 0.15, 0.24), stocked,
                "ochre" if index != 1 else "green", "stock_sack", subdivisions=1)

    shortage = new_asset("wk_state_market_shortage_v01", kind="state_module",
                         slot="counter_front", anchor="counter_front",
                         compatible=("market_counter",),
                         silhouette=("shortage", "barrier"))
    add_box("GEO_RationBar", (0.0, 0.0, 0.42), (1.95, 0.09, 0.11),
            shortage, "oxblood", "ration_bar", width=0.025)
    for x in (-0.72, 0.72):
        add_box(f"GEO_RationPost{x:+.0f}", (x, 0.0, 0.21),
                (0.08, 0.08, 0.52), shortage, "metal", "ration_post", width=0.018)


def collection_bounds(collection: bpy.types.Collection) -> dict[str, list[float]]:
    points: list[Vector] = []
    for obj in collection.all_objects:
        if obj.type != "MESH":
            continue
        points.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    if not points:
        return {"min": [0.0, 0.0, 0.0], "max": [0.0, 0.0, 0.0],
                "size": [0.0, 0.0, 0.0]}
    minimum = [min(point[axis] for point in points) for axis in range(3)]
    maximum = [max(point[axis] for point in points) for axis in range(3)]
    return {
        "min": [round(value, 4) for value in minimum],
        "max": [round(value, 4) for value in maximum],
        "size": [round(maximum[i] - minimum[i], 4) for i in range(3)],
    }


def export_assets() -> None:
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    records = {record["id"]: record for record in ASSET_RECORDS}
    for asset_id, collection in ASSET_COLLECTIONS.items():
        objects = [obj for obj in collection.all_objects if obj.type == "MESH"]
        if not objects:
            raise RuntimeError(f"{asset_id} has no mesh objects")
        if asset_id.startswith("wk_head_") or asset_id.startswith("wk_hair_"):
            default_semantic = 0 if asset_id.startswith("wk_head_") else 1
            for obj in objects:
                slot = str(obj.get("cc_palette_slot", ""))
                semantic = (BODY_PAINT_SEMANTICS.index(slot)
                            if slot in BODY_PAINT_SEMANTICS
                            else default_semantic)
                paint_channels.add_indexed_paint_channels(
                    obj, [semantic] * len(obj.data.polygons),
                    BODY_PAINT_SEMANTICS)
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        path = EXPORT_DIR / f"{asset_id}.glb"
        bpy.ops.export_scene.gltf(
            filepath=str(path), export_format="GLB", use_selection=True,
            export_yup=True, export_animations=False, export_skins=False,
            export_morph=False, export_extras=True, export_materials="EXPORT",
            export_vertex_color="ACTIVE")
        record = records[asset_id]
        record["bounds"] = collection_bounds(collection)
        record["material_roles"] = sorted({
            obj.get("cc_material_role", "unknown") for obj in objects})
        record["mesh_objects"] = len(objects)
        record["bytes"] = path.stat().st_size
        collection.hide_render = True


def body_skin_asset_id(frame: str, muscle: str, tissue: str) -> str:
    return f"wk_body_skin_{frame}_{muscle}_{tissue}_v01"


def apply_object_modifiers(obj: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.hide_viewport = False
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for modifier in tuple(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=modifier.name)


def point_segment_distance(point: Vector, start: Vector, end: Vector) -> float:
    axis = end - start
    length_squared = axis.length_squared
    if length_squared <= 1.0e-10:
        return (point - start).length
    amount = max(0.0, min(1.0, (point - start).dot(axis) / length_squared))
    return (point - (start + axis * amount)).length


def create_body_rig(asset_id: str,
                    collection: bpy.types.Collection) -> bpy.types.Object:
    data = bpy.data.armatures.new(f"ARM_{asset_id}")
    rig = bpy.data.objects.new(f"ARM_{asset_id}", data)
    collection.objects.link(rig)
    rig.show_in_front = True
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    created: dict[str, bpy.types.EditBone] = {}
    for name, _, head, tail in BODY_RIG_BONES:
        bone = data.edit_bones.new(name)
        bone.head = head
        bone.tail = tail
        bone.roll = 0.0
        bone.use_connect = False
        bone.use_deform = True
        created[name] = bone
    for name, parent, _, _ in BODY_RIG_BONES:
        if parent is not None:
            created[name].parent = created[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.select_set(False)
    return rig


def add_body_raw_geometry(collection: bpy.types.Collection, frame: str,
                          muscle_profile: str, tissue_profile: str) -> None:
    buck = BUCKS[frame]
    muscle = MUSCLE_PROFILES[muscle_profile]
    tissue = SOFT_TISSUE_PROFILES[tissue_profile]
    envelope = body_envelope(frame, muscle_profile, tissue_profile)
    shoulder = max(envelope["chest"] * 0.92,
                   buck.shoulder * muscle["shoulder"] * 0.94)




    add_loft(
        "RAW_BodyCore",
        ((0.00, envelope["hip"] * 0.82, envelope["pelvis_depth"] * 0.88),
         (0.08, envelope["hip"], envelope["pelvis_depth"]),
         (0.18, envelope["waist"] * 1.04, envelope["abdomen_depth"]),
         (0.32, envelope["waist"], envelope["mid_torso_depth"]),
         (0.48, envelope["chest"] * 0.82, envelope["mid_torso_depth"]),
         (0.62, envelope["chest"], envelope["chest_depth"]),
         (0.70, shoulder, envelope["chest_depth"] * 0.86)),
        collection, "skin", "runtime_body_raw", center=(0.0, 0.0, 0.90),
        sides=12)



    add_box("RAW_ChestPlane", (0.0, -0.045, 1.43),
            (envelope["chest"] * 1.72,
             envelope["chest_depth"] * 1.48, 0.22),
            collection, "skin", "runtime_body_raw", width=0.040)
    add_box("RAW_ClavicleYoke", (0.0, -0.004, 1.535),
            (shoulder * 1.74, envelope["chest_depth"] * 1.28, 0.10),
            collection, "skin", "runtime_body_raw", width=0.028)
    add_box("RAW_AbdomenPlane",
            (0.0, -0.040 - tissue["abdomen"] * 0.28, 1.20),
            (envelope["waist"] * 1.72,
             0.18 + tissue["abdomen"] * 2.0, 0.29),
            collection, "skin", "runtime_body_raw", width=0.052)
    for side in (-1.0, 1.0):
        add_ico(f"RAW_Shoulder{side:+.0f}",
                (side * 0.29, 0.0, 1.55),
                (envelope["upper_arm"] * 1.22,
                 envelope["upper_arm"] * 1.06,
                 envelope["upper_arm"] * 1.04),
                collection, "skin", "runtime_body_raw", subdivisions=1)
        add_ico(f"RAW_HipBridge{side:+.0f}",
                (side * 0.12, 0.0, 0.98),
                (envelope["thigh"] * 1.14,
                 envelope["pelvis_depth"] * 0.88, 0.15),
                collection, "skin", "runtime_body_raw", subdivisions=1)
        add_ico(f"RAW_Glute{side:+.0f}",
                (side * envelope["hip"] * 0.48,
                 0.070 + tissue["hip"] * 0.32, 0.99),
                (envelope["hip"] * 0.56,
                 0.11 + tissue["hip"] * 0.64, 0.15),
                collection, "skin", "runtime_body_raw", subdivisions=1)

    add_segment("RAW_Neck", (0.0, 0.0, 1.55), (0.0, 0.0, 1.76),
                0.088 + tissue["chest"] * 0.18, 0.073,
                collection, "skin", "runtime_body_raw", vertices=10)

    for side, suffix in ((-1.0, "L"), (1.0, "R")):
        shoulder_point = Vector((side * 0.29, 0.0, 1.58))
        elbow = Vector((side * 0.46, 0.0, 1.26))
        wrist = Vector((side * 0.57, -0.015, 0.94))
        hand_tip = Vector((side * 0.58, -0.045, 0.78))
        add_segment(f"RAW_UpperArm{suffix}", shoulder_point, elbow,
                    envelope["upper_arm"] * 1.16,
                    envelope["upper_arm"] * 0.82,
                    collection, "skin", "runtime_body_raw", vertices=10)
        add_ico(f"RAW_Elbow{suffix}", tuple(elbow),
                (envelope["forearm"] * 1.04,
                 envelope["forearm"] * 0.94,
                 envelope["forearm"] * 1.04),
                collection, "skin", "runtime_body_raw", subdivisions=1)
        add_segment(f"RAW_Forearm{suffix}", elbow, wrist,
                    envelope["forearm"] * 1.18,
                    envelope["forearm"] * 0.76,
                    collection, "skin", "runtime_body_raw", vertices=10)
        add_segment(f"RAW_Hand{suffix}", wrist, hand_tip,
                    buck.hand[0] * 0.62, buck.hand[0] * 0.46,
                    collection, "skin", "runtime_body_raw", vertices=8)
        add_ico(f"RAW_Palm{suffix}", tuple((wrist + hand_tip) * 0.5),
                (buck.hand[0] * 0.76, buck.hand[1] * 0.88,
                 buck.hand[2] * 0.46),
                collection, "skin", "runtime_body_raw", subdivisions=1)
        hand_midpoint = (wrist + hand_tip) * 0.5
        thumb = hand_midpoint + Vector((side * 0.030, -0.020, 0.015))
        add_ico(f"RAW_Thumb{suffix}", tuple(thumb),
                (buck.hand[0] * 0.27, buck.hand[1] * 0.62,
                 buck.hand[2] * 0.26),
                collection, "skin", "runtime_body_raw", subdivisions=1)

        hip = Vector((side * 0.14, 0.0, 1.00))
        knee = Vector((side * 0.15, 0.0, 0.56))
        ankle = Vector((side * 0.15, 0.0, 0.13))
        toe = Vector((side * 0.15, -0.27, 0.08))
        add_segment(f"RAW_Thigh{suffix}", hip, knee,
                    envelope["thigh"] * 1.22,
                    envelope["thigh"] * 0.76,
                    collection, "skin", "runtime_body_raw", vertices=10)
        add_ico(f"RAW_Knee{suffix}", tuple(knee + Vector((0.0, -0.018, 0.0))),
                (envelope["calf"] * 1.04,
                 envelope["calf"] * 0.94,
                 envelope["calf"] * 0.94),
                collection, "skin", "runtime_body_raw", subdivisions=1)
        add_segment(f"RAW_Shin{suffix}", knee, ankle,
                    envelope["calf"] * 1.15,
                    envelope["calf"] * 0.66,
                    collection, "skin", "runtime_body_raw", vertices=10)
        add_box(f"RAW_FootLast{suffix}", tuple((ankle + toe) * 0.5),
                (buck.foot[0] * 1.20, buck.foot[1] * 1.08,
                 buck.foot[2] * 0.82),
                collection, "skin", "runtime_body_raw", width=0.022)


def consolidate_body_skin(collection: bpy.types.Collection,
                          asset_id: str) -> bpy.types.Object:
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
    if not objects:
        raise RuntimeError(f"{asset_id}: no raw body geometry")
    for obj in objects:
        apply_object_modifiers(obj)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    body = objects[0]
    body.name = f"SKIN_{asset_id}"
    body.data.name = body.name



    remesh = body.modifiers.new("CC_ContinuousSkinUnion", "REMESH")
    remesh.mode = "VOXEL"
    remesh.voxel_size = 0.032
    remesh.use_smooth_shade = True
    apply_object_modifiers(body)
    polygon_count = len(body.data.polygons)
    if polygon_count > 1400:
        decimate = body.modifiers.new("CC_ActionFigureLowPoly", "DECIMATE")
        decimate.ratio = 1400.0 / float(polygon_count)
        decimate.use_collapse_triangulate = True
        apply_object_modifiers(body)
    triangulate = body.modifiers.new("CC_RuntimeTriangles", "TRIANGULATE")
    apply_object_modifiers(body)
    for polygon in body.data.polygons:
        polygon.use_smooth = True
        polygon.material_index = 0
    body.data.materials.clear()
    body.data.materials.append(MATERIALS["skin"])
    body.data.update()
    paint_channels.add_indexed_paint_channels(
        body, [0] * len(body.data.polygons), BODY_PAINT_SEMANTICS)
    body["cc_asset_id"] = asset_id
    body["cc_material_role"] = "skin"
    body["cc_part_role"] = "continuous_baked_body_skin"
    body["cc_runtime_strategy"] = "fixed topology recipe baked before skinning"
    return body


def bind_body_skin(body: bpy.types.Object, rig: bpy.types.Object) -> None:
    groups = {name: body.vertex_groups.new(name=name)
              for name, _, _, _ in BODY_RIG_BONES}
    segments = {
        name: (Vector(head), Vector(tail))
        for name, _, head, tail in BODY_RIG_BONES
        if name not in {"root", "head"}
    }
    torso_bones = {"pelvis", "spine", "chest", "neck"}
    end_bones = {"hand.L", "hand.R", "foot.L", "foot.R"}
    for vertex in body.data.vertices:
        point = vertex.co
        scores: list[tuple[float, str]] = []
        for name, (start, end) in segments.items():
            if name.endswith(".L") and point.x > 0.055:
                continue
            if name.endswith(".R") and point.x < -0.055:
                continue
            sigma = 0.19 if name in torso_bones else (
                0.10 if name in end_bones else 0.13)
            distance = point_segment_distance(point, start, end)
            score = math.exp(-2.0 * (distance / sigma) ** 2)
            if score > 1.0e-7:
                scores.append((score, name))
        if not scores:
            scores = [(1.0, "pelvis")]
        selected = sorted(scores, reverse=True)[:4]
        total = sum(score for score, _ in selected)
        for score, name in selected:
            groups[name].add((vertex.index,), score / total, "REPLACE")
    armature = body.modifiers.new("CC_BodySkin", "ARMATURE")
    armature.object = rig
    armature.use_deform_preserve_volume = True
    body.parent = rig
    body.matrix_parent_inverse = rig.matrix_world.inverted()


def export_body_skin(body: bpy.types.Object, rig: bpy.types.Object,
                     frame: str, muscle: str, tissue: str) -> dict[str, object]:
    asset_id = body_skin_asset_id(frame, muscle, tissue)
    path = EXPORT_DIR / f"{asset_id}.glb"
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    body.hide_set(False)
    body.hide_viewport = False
    rig.hide_set(False)
    rig.hide_viewport = False
    body.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.gltf(
        filepath=str(path), export_format="GLB", use_selection=True,
        export_yup=True, export_animations=False, export_skins=True,
        export_morph=False, export_extras=True, export_materials="EXPORT",
        export_vertex_color="ACTIVE")
    body.data.calc_loop_triangles()
    record = {
        "id": asset_id,
        "export": str(path.relative_to(ROOT)),
        "frame": frame,
        "muscle_profile": muscle,
        "soft_tissue_profile": tissue,
        "topology": "one closed continuous neck-down skin",
        "generation": "voxel-union construction masses, low-poly bake",
        "triangles": len(body.data.loop_triangles),
        "vertices": len(body.data.vertices),
        "bytes": path.stat().st_size,
        "bones": [name for name, _, _, _ in BODY_RIG_BONES],
        "skin_weights": "four nearest bone-local influences",
        "runtime": "GPU skinned; garments, armor, head, hair and props stay modular",
    }
    body.hide_render = True
    body.hide_set(True)
    rig.hide_render = True
    rig.hide_set(True)
    return record


def build_runtime_body_skins() -> None:
    root = new_collection("RUNTIME_BODY_SKINS")
    for frame in BUCKS:
        for muscle in MUSCLE_PROFILES:
            for tissue in SOFT_TISSUE_PROFILES:
                asset_id = body_skin_asset_id(frame, muscle, tissue)
                collection = new_collection(asset_id.upper(), root)
                add_body_raw_geometry(collection, frame, muscle, tissue)
                body = consolidate_body_skin(collection, asset_id)
                rig = create_body_rig(asset_id, collection)
                bind_body_skin(body, rig)
                BODY_SKIN_OBJECTS[(frame, muscle, tissue)] = body
                BODY_SKIN_RECORDS.append(
                    export_body_skin(body, rig, frame, muscle, tissue))


def add_ground(collection: bpy.types.Collection, width: float, depth: float,
               center: tuple[float, float, float] = (0.0, 0.6, -0.055)) -> None:
    add_box("GEO_ReviewGround", center, (width, depth, 0.10), collection,
            "ground", "review_ground", width=0.015)


def add_text(collection: bpy.types.Collection, body: str,
             location: tuple[float, float, float], size: float = 0.18,
             material: str = "cream", align: str = "CENTER") -> None:
    curve = bpy.data.curves.new(f"TXT_{body}", "FONT")
    curve.body = body
    curve.align_x = align
    curve.align_y = "CENTER"
    curve.size = size
    curve.extrude = 0.002
    obj = bpy.data.objects.new(f"TXT_{body}", curve)
    collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (math.pi * 0.5, 0.0, 0.0)
    obj.data.materials.append(MATERIALS[material])


def add_camera(collection: bpy.types.Collection,
               location: tuple[float, float, float],
               target: tuple[float, float, float], ortho_scale: float) -> None:
    camera_data = bpy.data.cameras.new("CAM_WorldKitReview")
    camera = bpy.data.objects.new("CAM_WorldKitReview", camera_data)
    collection.objects.link(camera)
    camera.location = location
    direction = Vector(target) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = ortho_scale
    bpy.context.scene.camera = camera


def add_lights(collection: bpy.types.Collection) -> None:
    key_data = bpy.data.lights.new("LGT_WorldKitKey", "AREA")
    key_data.energy = 1250.0
    key_data.shape = "DISK"
    key_data.size = 7.0
    key = bpy.data.objects.new("LGT_WorldKitKey", key_data)
    collection.objects.link(key)
    key.location = (-5.0, -7.0, 10.0)
    key.rotation_euler = ((Vector((0.0, 0.0, 1.0)) - key.location)
                          .to_track_quat("-Z", "Y").to_euler())
    fill_data = bpy.data.lights.new("LGT_WorldKitFill", "AREA")
    fill_data.energy = 650.0
    fill_data.size = 8.0
    fill = bpy.data.objects.new("LGT_WorldKitFill", fill_data)
    collection.objects.link(fill)
    fill.location = (6.0, 1.0, 7.0)
    fill.rotation_euler = ((Vector((0.0, 0.0, 1.0)) - fill.location)
                           .to_track_quat("-Z", "Y").to_euler())


def add_eye_marks(collection: bpy.types.Collection,
                  head_center: tuple[float, float, float], width: float) -> None:
    x, y, z = head_center
    for side in (-1.0, 1.0):
        add_box(f"GEO_Eye{side:+.0f}", (x + side * width * 0.20, y - 0.108, z + 0.025),
                (0.022, 0.012, 0.018), collection, "ink", "eye", width=0.003)


def add_figure(collection: bpy.types.Collection, x: float, buck_name: str,
               head_family: str, hair_style: str, *, shells: tuple[str, ...] = (),
               prop: str | None = None, pose: str = "neutral",
               muscle_profile: str = "athletic",
               soft_tissue_profile: str = "balanced",
               outer: str = "teal", accent: str = "oxblood",
               label: str | None = None) -> None:
    buck = BUCKS[buck_name]
    envelope = body_envelope(buck_name, muscle_profile, soft_tissue_profile)
    chest = envelope["chest"]
    waist = envelope["waist"]
    hip = envelope["hip"]
    upper_arm_radius = envelope["upper_arm"]
    forearm_radius = envelope["forearm"]
    thigh_radius = envelope["thigh"]
    shin_radius = envelope["calf"]
    foot_x = 0.135 if buck_name == "lean" else 0.15 if buck_name == "standard" else 0.17
    ankle_z, knee_z, hip_z = 0.13, 0.54, 0.98
    shoulder_z = 1.50
    shoulder_x = buck.shoulder
    hand_z = 0.91
    if pose == "upright":
        elbow_out, elbow_z, hand_out = 0.015, 1.20, 0.02
    elif pose == "open":
        elbow_out, elbow_z, hand_out = 0.13, 1.26, 0.22
    elif pose == "heavy":
        elbow_out, elbow_z, hand_out = 0.08, 1.14, 0.10
    elif pose == "walking":
        elbow_out, elbow_z, hand_out = 0.06, 1.24, 0.06
    else:
        elbow_out, elbow_z, hand_out = 0.05, 1.20, 0.045


    for side in (-1.0, 1.0):
        side_x = x + side * foot_x
        foot_y = -0.075 if (pose == "walking" and side < 0) else -0.02
        add_wedge(f"GEO_{label}_Foot{side:+.0f}",
                  (side_x, foot_y, buck.foot[2] * 0.5),
                  (buck.foot[0], buck.foot[1]),
                  (buck.foot[0] * 0.80, buck.foot[1] * 0.70),
                  buck.foot[2], collection, "skin", "skin_surface_foot")
        add_segment(f"GEO_{label}_Shin{side:+.0f}",
                    (side_x, 0.0, ankle_z), (x + side * foot_x, 0.0, knee_z),
                    shin_radius * 0.72, shin_radius * 1.08,
                    collection, "skin", "skin_surface_shin")
        add_segment(f"GEO_{label}_Thigh{side:+.0f}",
                    (x + side * foot_x, 0.0, knee_z),
                    (x + side * buck.hip * 0.58, 0.0, hip_z),
                    thigh_radius * 0.72, thigh_radius * 1.12,
                    collection, "skin", "skin_surface_thigh")


    add_loft(f"GEO_{label}_Pelvis", ((0.00, hip * 0.82,
                                      envelope["pelvis_depth"] * 0.88),
                                     (0.10, hip, envelope["pelvis_depth"]),
                                     (0.22, waist,
                                      envelope["abdomen_depth"])),
             collection, "skin", "skin_surface_pelvis", center=(x, 0.0, 0.94), sides=10)
    add_loft(f"GEO_{label}_Torso", ((0.00, waist, envelope["abdomen_depth"]),
                                    (0.18, chest * 0.82,
                                     envelope["mid_torso_depth"]),
                                    (0.42, chest, envelope["chest_depth"]),
                                    (0.50, buck.shoulder * 0.94,
                                     envelope["chest_depth"] * 0.84)),
             collection, "skin", "skin_surface_torso", center=(x, 0.0, 1.08), sides=10)


    for side in (-1.0, 1.0):
        shoulder = Vector((x + side * shoulder_x, 0.0, shoulder_z))
        elbow = Vector((x + side * (shoulder_x + elbow_out), -0.005, elbow_z))
        hand = Vector((x + side * (shoulder_x + hand_out), -0.025, hand_z))
        add_segment(f"GEO_{label}_UpperArm{side:+.0f}", shoulder, elbow,
                    upper_arm_radius * 1.10, upper_arm_radius * 0.78,
                    collection, "skin", "skin_surface_upper_arm")
        add_segment(f"GEO_{label}_Forearm{side:+.0f}", elbow, hand,
                    forearm_radius * 1.16, forearm_radius * 0.74,
                    collection, "skin", "skin_surface_forearm")
        add_wedge(f"GEO_{label}_Hand{side:+.0f}", tuple(hand),
                  (buck.hand[0] * 0.72, buck.hand[1]),
                  (buck.hand[0], buck.hand[1] * 1.10), buck.hand[2],
                  collection, "skin", "hand")

    add_segment(f"GEO_{label}_Neck", (x, 0.0, 1.56), (x, 0.0, 1.61),
                0.072 if buck_name != "heavy" else 0.082,
                0.070 if buck_name != "heavy" else 0.080,
                collection, "skin", "neck", vertices=8)
    head_center = (x, -0.005, 1.75)
    add_head_geometry(head_family, head_center, collection)
    add_eye_marks(collection, head_center, HEADS[head_family][0])
    add_hair_style(hair_style, head_center, collection)


    if "tunic" in shells:
        add_loft(f"GEO_{label}_TunicShell",
                 ((0.00, waist + 0.018, envelope["abdomen_depth"] + 0.015),
                  (0.20, chest * 0.88 + 0.018,
                   envelope["mid_torso_depth"] + 0.015),
                  (0.45, chest + 0.020, envelope["chest_depth"] + 0.015)),
                 collection, outer, "chest_shell", center=(x, -0.002, 1.09), sides=10)
        for side in (-1.0, 1.0):
            shoulder = Vector((x + side * shoulder_x, 0.0, shoulder_z))
            elbow = Vector((x + side * (shoulder_x + elbow_out), -0.005, elbow_z))
            add_segment(f"GEO_{label}_TunicSleeve{side:+.0f}", shoulder, elbow,
                        upper_arm_radius * 1.22 + 0.010,
                        upper_arm_radius * 0.86 + 0.008,
                        collection, outer, "garment_sleeve")
    if "trousers" in shells:
        for side in (-1.0, 1.0):
            side_x = x + side * foot_x
            add_segment(f"GEO_{label}_TrouserShin{side:+.0f}",
                        (side_x, -0.002, ankle_z + 0.015),
                        (side_x, -0.002, knee_z - 0.018),
                        shin_radius * 0.80 + 0.010,
                        shin_radius * 1.15 + 0.010,
                        collection, "underlayer", "garment_trouser_shin")
            add_segment(f"GEO_{label}_TrouserThigh{side:+.0f}",
                        (side_x, -0.002, knee_z + 0.018),
                        (x + side * hip * 0.58, -0.002, hip_z - 0.008),
                        thigh_radius * 0.80 + 0.010,
                        thigh_radius * 1.18 + 0.012,
                        collection, "underlayer", "garment_trouser_thigh")
    if "cuirass" in shells:
        add_wedge(f"GEO_{label}_Cuirass", (x, -0.155, 1.36),
                  (waist * 1.75 + 0.025, 0.09), (chest * 1.82 + 0.030, 0.12),
                  0.46, collection, "metal", "cuirass")
        add_box(f"GEO_{label}_CuirassRidge", (x, -0.225, 1.38),
                (0.055, 0.028, 0.32), collection, "gold", "cuirass_ridge", width=0.006)
    if "belt" in shells:
        add_box(f"GEO_{label}_Belt", (x, -0.005, 1.055),
                (hip * 2.05 + 0.025, 0.25, 0.08), collection,
                "leather", "belt", width=0.018)
        add_box(f"GEO_{label}_Buckle", (x, -0.145, 1.055),
                (0.075, 0.026, 0.06), collection, "gold", "buckle", width=0.005)
    if "apron" in shells:
        add_panel(f"GEO_{label}_Apron", ((x - hip * 0.86, -0.16, 1.03),
                                         (x + hip * 0.86, -0.16, 1.03),
                                         (x + hip * 0.72, -0.17, 0.52),
                                         (x - hip * 0.72, -0.17, 0.52)),
                  collection, accent, "apron", thickness=0.025)
    if "mantle_left" in shells:
        add_panel(f"GEO_{label}_Mantle", ((x - shoulder_x - 0.08, 0.01, 1.57),
                                          (x + 0.08, 0.01, 1.54),
                                          (x + 0.04, 0.08, 1.20),
                                          (x - 0.16, 0.12, 0.95),
                                          (x - shoulder_x - 0.10, 0.08, 1.20)),
                  collection, accent, "mantle", thickness=0.035)
    if "pauldron_left" in shells:
        add_ico(f"GEO_{label}_PauldronL", (x - shoulder_x, -0.005, 1.50),
                (0.145, 0.115, 0.105), collection, "metal", "pauldron", subdivisions=1)
    if "pauldron_pair" in shells:
        for side in (-1.0, 1.0):
            add_ico(f"GEO_{label}_Pauldron{side:+.0f}",
                    (x + side * shoulder_x, -0.005, 1.50),
                    (0.145, 0.115, 0.105), collection, "metal", "pauldron", subdivisions=1)
    if "cape" in shells:
        add_panel(f"GEO_{label}_Cape", ((x - 0.22, 0.13, 1.52),
                                        (x + 0.22, 0.13, 1.52),
                                        (x + 0.26, 0.20, 0.62),
                                        (x + 0.05, 0.23, 0.48),
                                        (x - 0.25, 0.19, 0.65)),
                  collection, accent, "cape", thickness=0.03)
    if "pack" in shells:
        add_box(f"GEO_{label}_Pack", (x, 0.19, 1.28),
                (0.40, 0.20, 0.47), collection, "leather", "pack", width=0.045)
        add_segment(f"GEO_{label}_Bedroll", (x - 0.21, 0.19, 1.57),
                    (x + 0.21, 0.19, 1.57), 0.085, 0.085,
                    collection, "cloth", "bedroll", vertices=8)
    if "bracers" in shells:
        for side in (-1.0, 1.0):
            hand_x = x + side * (shoulder_x + hand_out)
            add_segment(f"GEO_{label}_Bracer{side:+.0f}",
                        (hand_x, -0.015, 0.98),
                        (hand_x + side * 0.015, -0.01, 1.10),
                        forearm_radius * 1.17 + 0.008,
                        forearm_radius * 1.02 + 0.008,
                        collection, "leather", "bracer")
    if "greaves" in shells:
        for side in (-1.0, 1.0):
            add_segment(f"GEO_{label}_Greave{side:+.0f}",
                        (x + side * foot_x, -0.005, 0.17),
                        (x + side * foot_x, -0.005, 0.43),
                        shin_radius * 0.82 + 0.010,
                        shin_radius * 1.15 + 0.010,
                        collection, "metal", "greave")
    if "boots" in shells:
        for side in (-1.0, 1.0):
            side_x = x + side * foot_x
            add_wedge(f"GEO_{label}_BootShell{side:+.0f}",
                      (side_x, -0.022, buck.foot[2] * 0.52),
                      (buck.foot[0] + 0.018, buck.foot[1] + 0.022),
                      (buck.foot[0] * 0.82 + 0.014,
                       buck.foot[1] * 0.72 + 0.018),
                      buck.foot[2] + 0.018, collection,
                      "leather", "garment_boot_shell")
    if "crown" in shells:
        add_box(f"GEO_{label}_CrownBand", (x, 0.0, 1.905),
                (0.18, 0.13, 0.04), collection, "gold", "crown_band", width=0.006)
        for index, px in enumerate((-0.064, 0.0, 0.064)):
            add_wedge(f"GEO_{label}_CrownPoint{index}",
                      (x + px, 0.0, 1.965), (0.034, 0.034),
                      (0.010, 0.016), 0.11, collection, "gold", "crown_point")

    if prop:
        hand_x = x + buck.shoulder + hand_out
        if prop in {"spear", "staff"}:
            length = 1.90 if prop == "spear" else 1.75
            add_segment(f"GEO_{label}_{prop}Shaft", (hand_x, -0.09, 0.18),
                        (hand_x, -0.09, 0.18 + length), 0.021, 0.025,
                        collection, "wood", "held_prop", vertices=8)
            if prop == "spear":
                add_wedge(f"GEO_{label}_SpearHead",
                          (hand_x, -0.09, 0.18 + length + 0.10),
                          (0.075, 0.04), (0.008, 0.01), 0.22,
                          collection, "metal", "prop_head")
        elif prop in {"hammer", "sword"}:
            length = 0.50 if prop == "hammer" else 0.90
            add_segment(f"GEO_{label}_{prop}Grip", (hand_x, -0.10, 0.72),
                        (hand_x, -0.10, 0.72 + length), 0.022, 0.025,
                        collection, "leather" if prop == "sword" else "wood",
                        "held_prop", vertices=8)
            if prop == "hammer":
                add_box(f"GEO_{label}_HammerHead",
                        (hand_x, -0.10, 1.20), (0.25, 0.11, 0.12),
                        collection, "metal", "prop_head", width=0.022)
            else:
                add_box(f"GEO_{label}_SwordBlade",
                        (hand_x, -0.10, 1.43), (0.052, 0.015, 0.72),
                        collection, "metal", "blade", width=0.006)
                add_box(f"GEO_{label}_SwordGuard",
                        (hand_x, -0.10, 1.05), (0.22, 0.04, 0.04),
                        collection, "gold", "guard", width=0.008)

    if label:
        add_text(collection, label.upper(), (x, -0.42, 0.075), 0.12)


def add_door_scale(collection: bpy.types.Collection, x: float) -> None:
    for side in (-1.0, 1.0):
        add_box(f"GEO_ScaleDoorSide{side:+.0f}", (x + side * 0.59, 0.0, 1.12),
                (0.16, 0.24, 2.24), collection, "wood_dark", "door_frame", width=0.018)
    add_box("GEO_ScaleDoorTop", (x, 0.0, 2.17), (1.34, 0.24, 0.14),
            collection, "wood_dark", "door_frame", width=0.018)
    add_text(collection, "1.02 x 2.10 m DOOR", (x, -0.35, 0.075), 0.12)


def add_counter_scale(collection: bpy.types.Collection, x: float) -> None:
    add_box("GEO_ScaleCounter", (x, 0.0, 0.43), (1.45, 0.65, 0.86),
            collection, "wood_dark", "counter", width=0.04)
    add_box("GEO_ScaleCounterTop", (x, -0.02, 0.89), (1.55, 0.72, 0.11),
            collection, "wood", "counter_top", width=0.025)
    add_text(collection, "0.92 m COUNTER", (x, -0.46, 0.075), 0.12)


def add_small_house(collection: bpy.types.Collection, x: float, y: float = 0.45,
                    label: str | None = None) -> None:
    add_box("GEO_HouseWall", (x, y, 1.40), (3.0, 2.6, 2.80),
            collection, "plaster", "house_wall", width=0.035)
    for px in (x - 1.4, x, x + 1.4):
        add_box(f"GEO_HouseFrame{px}", (px, y - 1.34, 1.42),
                (0.16, 0.15, 2.72), collection, "wood_dark", "house_frame", width=0.018)
    add_box("GEO_HouseDoor", (x, y - 1.36, 1.05), (1.02, 0.09, 2.10),
            collection, "wood", "house_door", width=0.022)
    add_panel("GEO_HouseRoofL", ((x - 1.72, y - 1.55, 2.75),
                                 (x, y - 1.55, 3.55),
                                 (x, y + 1.55, 3.55),
                                 (x - 1.72, y + 1.55, 2.75)),
              collection, "roof", "house_roof", thickness=0.12)
    add_panel("GEO_HouseRoofR", ((x, y - 1.55, 3.55),
                                 (x + 1.72, y - 1.55, 2.75),
                                 (x + 1.72, y + 1.55, 2.75),
                                 (x, y + 1.55, 3.55)),
              collection, "roof", "house_roof", thickness=0.12)
    if label:
        add_text(collection, label, (x, y - 1.75, 0.075), 0.12)


def render_review(filename: str, build_scene, *, camera_location,
                  camera_target, ortho_scale: float,
                  resolution: tuple[int, int] = (1600, 900)) -> None:
    review = new_collection(f"REVIEW_{filename.upper().replace('.', '_')}")
    build_scene(review)
    add_lights(review)
    add_camera(review, camera_location, camera_target, ortho_scale)
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = resolution
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(PREVIEW_DIR / filename)
    bpy.ops.render.render(write_still=True)
    bpy.data.collections.remove(review, do_unlink=True)


def build_scale_lineup(collection: bpy.types.Collection) -> None:
    add_ground(collection, 22.0, 5.0, (1.0, 0.75, -0.055))
    add_text(collection, "ONE WORLD SCALE", (0.5, 0.4, 3.95), 0.28)
    add_figure(collection, -8.0, "lean", "long", "cropped",
               shells=("tunic", "trousers", "boots", "belt"),
               muscle_profile="slight", soft_tissue_profile="low",
               label="LEAN 1.90 m")
    add_figure(collection, -6.2, "standard", "square", "swept",
               shells=("tunic", "trousers", "boots", "belt"),
               muscle_profile="athletic", soft_tissue_profile="balanced",
               label="STANDARD 1.90 m")
    add_figure(collection, -4.3, "heavy", "broad", "cropped",
               shells=("tunic", "trousers", "boots", "belt"),
               muscle_profile="power", soft_tissue_profile="central",
               label="HEAVY 1.90 m")
    add_door_scale(collection, -1.8)
    add_counter_scale(collection, 0.8)
    add_box("GEO_Seat", (2.4, 0.0, 0.23), (0.55, 0.55, 0.46),
            collection, "wood", "seat", width=0.035)
    add_text(collection, "0.46 m SEAT", (2.4, -0.42, 0.075), 0.12)
    add_box("GEO_Rail", (3.8, 0.0, 0.53), (0.10, 0.10, 1.05),
            collection, "wood_dark", "rail", width=0.015)
    add_box("GEO_RailTop", (4.4, 0.0, 1.03), (1.3, 0.10, 0.10),
            collection, "wood_dark", "rail", width=0.015)
    add_text(collection, "1.05 m RAIL", (4.1, -0.42, 0.075), 0.12)
    add_small_house(collection, 8.0, label="3.0 m PLAYSET HOUSE")


def build_parts_board(collection: bpy.types.Collection) -> None:
    add_ground(collection, 17.0, 7.5, (0.0, 1.0, -0.055))
    add_text(collection, "ACTION-FIGURE PARTS BIN", (0.0, 0.6, 4.35), 0.27)
    for index, muscle_profile in enumerate(("slight", "athletic", "power")):
        add_figure(collection, -6.5 + index * 1.65, "standard",
                   ("long", "square", "broad")[index], "cropped",
                   muscle_profile=muscle_profile,
                   label=f"{muscle_profile} muscle")
    for index, family in enumerate(HEADS):
        center = (-0.8 + index * 0.82, -0.05, 3.05)
        add_head_geometry(family, center, collection)
        add_eye_marks(collection, center, HEADS[family][0])
        add_text(collection, family.upper(), (center[0], -0.25, 2.67), 0.11)
    for index, style in enumerate(("cropped", "swept", "bob", "crest", "braided", "rear_lock")):
        center = (-1.6 + index * 0.68, -0.02, 1.75)
        add_ico(f"GEO_HeadCore{index}", center, (0.10, 0.09, 0.14),
                collection, "skin_dark", "head_guide", subdivisions=1)
        add_hair_style(style, center, collection)
        add_text(collection, style.upper().replace("_", " "),
                 (center[0], -0.24, 1.38), 0.085)

    add_figure(collection, 4.8, "standard", "veteran", "rear_lock",
               shells=("tunic", "trousers", "boots", "belt", "pauldron_left",
                       "bracers", "greaves", "pack"),
               prop="spear", pose="upright", outer="blue", accent="oxblood",
               label="FITTED SHELLS")
    add_text(collection, "ONE SKELETON — THREE MUSCLE RECIPES",
             (-4.85, 0.4, 2.55), 0.14)
    add_text(collection, "HEAD + MOLDED HAIR FAMILIES", (0.4, 0.4, 3.58), 0.16)


def build_cast_review(collection: bpy.types.Collection) -> None:
    add_ground(collection, 13.5, 5.5, (0.0, 0.75, -0.055))
    add_text(collection, "FIVE RECIPES — ONE KIT", (0.0, 0.4, 3.05), 0.28)
    recipes = (
        (-4.8, "standard", "long", "swept",
         ("tunic", "trousers", "boots", "belt", "mantle_left", "pauldron_left",
          "bracers", "greaves", "crown"),
         "sword", "neutral", "athletic", "low", "teal", "oxblood", "CROWNLESS"),
        (-2.4, "heavy", "veteran", "cropped",
         ("tunic", "trousers", "boots", "cuirass", "belt", "pauldron_pair", "greaves"),
         "spear", "upright", "athletic", "balanced", "blue", "oxblood", "GUARD"),
        (0.0, "heavy", "broad", "bob",
         ("tunic", "trousers", "boots", "belt", "apron"),
         None, "open", "slight", "central", "ochre", "green", "MERCHANT"),
        (2.4, "heavy", "square", "crest",
         ("tunic", "trousers", "boots", "belt", "apron", "bracers"),
         "hammer", "heavy", "power", "lower_body", "green", "ochre", "LABORER"),
        (4.8, "lean", "long", "braided",
         ("tunic", "trousers", "boots", "belt", "mantle_left", "pack"),
         "staff", "walking", "slight", "low", "cloth", "blue", "TRAVELLER"),
    )
    for recipe in recipes:
        add_figure(collection, recipe[0], recipe[1], recipe[2], recipe[3],
                   shells=recipe[4], prop=recipe[5], pose=recipe[6],
                   muscle_profile=recipe[7], soft_tissue_profile=recipe[8],
                   outer=recipe[9], accent=recipe[10], label=recipe[11])


def transform_collection(collection: bpy.types.Collection, x: float,
                         rotation_degrees: float) -> None:
    transform = (Matrix.Translation((x, 0.0, 0.0)) @
                 Matrix.Rotation(math.radians(rotation_degrees), 4, "Z"))
    for obj in collection.all_objects:
        obj.matrix_world = transform @ obj.matrix_world


def override_mesh_materials(collection: bpy.types.Collection,
                            material: str) -> None:
    for obj in collection.all_objects:
        if obj.type != "MESH":
            continue
        obj.data.materials.clear()
        obj.data.materials.append(MATERIALS[material])


def add_skeleton_figure(collection: bpy.types.Collection, x: float,
                        frame: str, label: str | None = None) -> None:
    buck = BUCKS[frame]
    foot_x = 0.135 if frame == "lean" else 0.15 if frame == "standard" else 0.17
    ankle_z, knee_z, hip_z = 0.13, 0.54, 0.98
    shoulder_z, elbow_z, wrist_z = 1.50, 1.20, 0.91
    joints: list[Vector] = []
    for side in (-1.0, 1.0):
        ankle = Vector((x + side * foot_x, 0.0, ankle_z))
        knee = Vector((x + side * foot_x, 0.0, knee_z))
        hip = Vector((x + side * buck.hip * 0.58, 0.0, hip_z))
        shoulder = Vector((x + side * buck.shoulder, 0.0, shoulder_z))
        elbow = Vector((x + side * (buck.shoulder + 0.05), 0.0, elbow_z))
        wrist = Vector((x + side * (buck.shoulder + 0.045), 0.0, wrist_z))
        add_segment(f"GEO_SkeletonThigh{side:+.0f}", knee, hip,
                    0.032, 0.043, collection, "cream", "skeleton_bone")
        add_segment(f"GEO_SkeletonShin{side:+.0f}", ankle, knee,
                    0.027, 0.035, collection, "cream", "skeleton_bone")
        add_segment(f"GEO_SkeletonUpperArm{side:+.0f}", shoulder, elbow,
                    0.034, 0.029, collection, "cream", "skeleton_bone")
        add_segment(f"GEO_SkeletonForearm{side:+.0f}", elbow, wrist,
                    0.028, 0.022, collection, "cream", "skeleton_bone")
        add_wedge(f"GEO_SkeletonFoot{side:+.0f}",
                  (ankle.x, -0.055, 0.065), (0.065, 0.16),
                  (0.055, 0.12), 0.06, collection, "cream", "skeleton_foot")
        add_wedge(f"GEO_SkeletonHand{side:+.0f}", tuple(wrist),
                  (0.050, 0.035), (0.065, 0.040), 0.11,
                  collection, "cream", "skeleton_hand")
        joints.extend((ankle, knee, hip, shoulder, elbow, wrist))
    add_segment("GEO_SkeletonSpine", (x, 0.0, 0.98), (x, 0.0, 1.61),
                0.040, 0.032, collection, "cream", "skeleton_spine")
    add_segment("GEO_SkeletonPelvis", (x - buck.hip * 0.62, 0.0, 1.00),
                (x + buck.hip * 0.62, 0.0, 1.00), 0.045, 0.045,
                collection, "cream", "skeleton_pelvis")
    add_segment("GEO_SkeletonShoulderYoke",
                (x - buck.shoulder, 0.0, shoulder_z),
                (x + buck.shoulder, 0.0, shoulder_z), 0.038, 0.038,
                collection, "cream", "skeleton_yoke")
    add_ico("GEO_SkeletonSkull", (x, 0.0, 1.75),
            (0.085, 0.075, 0.14), collection, "cream", "skeleton_skull",
            subdivisions=1)
    joints.extend((Vector((x, 0.0, 0.98)), Vector((x, 0.0, 1.50)),
                   Vector((x, 0.0, 1.61))))
    for index, joint in enumerate(joints):
        add_ico(f"GEO_SkeletonJoint{index}", tuple(joint),
                (0.043, 0.043, 0.043), collection,
                "gold", "skeleton_joint", subdivisions=1)
    if label:
        add_text(collection, label, (x, -0.40, 0.075), 0.11)


def add_muscle_overlay(collection: bpy.types.Collection, x: float,
                       frame: str, profile: str) -> None:
    buck = BUCKS[frame]
    muscle = MUSCLE_PROFILES[profile]
    foot_x = 0.135 if frame == "lean" else 0.15 if frame == "standard" else 0.17
    chest = buck.chest * muscle["chest"]
    waist = buck.waist * muscle["waist"]
    hip = buck.hip * muscle["pelvis"]
    for side in (-1.0, 1.0):
        add_ico(f"GEO_MusclePec{side:+.0f}",
                (x + side * chest * 0.42, -0.055, 1.40),
                (chest * 0.52, 0.095, 0.15), collection,
                "oxblood", "muscle_chest", subdivisions=1)
        add_ico(f"GEO_MuscleDeltoid{side:+.0f}",
                (x + side * buck.shoulder, 0.0, 1.48),
                (0.085 * muscle["shoulder"], 0.075, 0.095),
                collection, "oxblood", "muscle_deltoid", subdivisions=1)
        add_segment(f"GEO_MuscleUpperArm{side:+.0f}",
                    (x + side * buck.shoulder, 0.0, 1.46),
                    (x + side * (buck.shoulder + 0.05), 0.0, 1.21),
                    buck.upper_arm * muscle["upper_arm"] * 1.12,
                    buck.upper_arm * muscle["upper_arm"] * 0.78,
                    collection, "oxblood", "muscle_upper_arm")
        add_segment(f"GEO_MuscleForearm{side:+.0f}",
                    (x + side * (buck.shoulder + 0.05), 0.0, 1.18),
                    (x + side * (buck.shoulder + 0.045), 0.0, 0.96),
                    buck.forearm * muscle["forearm"] * 1.18,
                    buck.forearm * muscle["forearm"] * 0.76,
                    collection, "oxblood", "muscle_forearm")
        add_ico(f"GEO_MuscleGlute{side:+.0f}",
                (x + side * hip * 0.48, 0.045, 1.00),
                (hip * 0.55, 0.11, 0.13), collection,
                "oxblood", "muscle_glute", subdivisions=1)
        add_segment(f"GEO_MuscleThigh{side:+.0f}",
                    (x + side * hip * 0.55, 0.0, 0.96),
                    (x + side * foot_x, 0.0, 0.57),
                    buck.thigh * muscle["thigh"] * 1.14,
                    buck.thigh * muscle["thigh"] * 0.76,
                    collection, "oxblood", "muscle_thigh")
        add_segment(f"GEO_MuscleCalf{side:+.0f}",
                    (x + side * foot_x, 0.02, 0.51),
                    (x + side * foot_x, 0.02, 0.17),
                    buck.shin * muscle["calf"] * 1.15,
                    buck.shin * muscle["calf"] * 0.72,
                    collection, "oxblood", "muscle_calf")
    add_loft("GEO_MuscleAbdomen", ((0.0, waist * 0.90, 0.11),
                                    (0.20, chest * 0.72, 0.14),
                                    (0.42, chest * 0.88, 0.16)),
             collection, "oxblood", "muscle_abdomen",
             center=(x, 0.0, 1.08), sides=8)


def add_soft_tissue_overlay(collection: bpy.types.Collection, x: float,
                            frame: str, muscle_profile: str,
                            tissue_profile: str) -> None:
    buck = BUCKS[frame]
    envelope = body_envelope(frame, muscle_profile, tissue_profile)
    tissue = SOFT_TISSUE_PROFILES[tissue_profile]
    foot_x = 0.135 if frame == "lean" else 0.15 if frame == "standard" else 0.17
    for side in (-1.0, 1.0):
        add_ico(f"GEO_SoftChest{side:+.0f}",
                (x + side * envelope["chest"] * 0.42, -0.105, 1.40),
                (envelope["chest"] * 0.52,
                 0.075 + tissue["chest"], 0.15),
                collection, "soft_tissue", "soft_tissue_chest", subdivisions=1)
        add_ico(f"GEO_SoftHip{side:+.0f}",
                (x + side * envelope["hip"] * 0.48, 0.025, 1.00),
                (envelope["hip"] * 0.55,
                 0.10 + tissue["hip"] * 0.5, 0.13),
                collection, "soft_tissue", "soft_tissue_hip", subdivisions=1)
        add_segment(f"GEO_SoftUpperArm{side:+.0f}",
                    (x + side * buck.shoulder, 0.0, 1.46),
                    (x + side * (buck.shoulder + 0.05), 0.0, 1.21),
                    envelope["upper_arm"] * 1.10,
                    envelope["upper_arm"] * 0.78,
                    collection, "soft_tissue", "soft_tissue_upper_arm")
        add_segment(f"GEO_SoftThigh{side:+.0f}",
                    (x + side * envelope["hip"] * 0.55, 0.0, 0.96),
                    (x + side * foot_x, 0.0, 0.57),
                    envelope["thigh"] * 1.12,
                    envelope["thigh"] * 0.74,
                    collection, "soft_tissue", "soft_tissue_thigh")
        add_segment(f"GEO_SoftCalf{side:+.0f}",
                    (x + side * foot_x, 0.02, 0.51),
                    (x + side * foot_x, 0.02, 0.17),
                    envelope["calf"] * 1.10,
                    envelope["calf"] * 0.70,
                    collection, "soft_tissue", "soft_tissue_calf")
    add_ico("GEO_SoftAbdomen", (x, -0.075, 1.20),
            (envelope["waist"] * 0.94,
             0.105 + tissue["abdomen"], 0.24),
            collection, "soft_tissue", "soft_tissue_abdomen", subdivisions=1)


def build_layer_stack_review(collection: bpy.types.Collection) -> None:
    add_ground(collection, 17.5, 5.5, (0.0, 0.75, -0.055))
    add_text(collection,
             "SKELETON → MUSCLE → SOFT TISSUE → SKIN → GARMENT → ARMOR",
             (0.0, 0.4, 3.08), 0.21)
    add_skeleton_figure(collection, -6.25, "standard", "1  SKELETON")
    add_skeleton_figure(collection, -3.75, "standard")
    add_muscle_overlay(collection, -3.75, "standard", "athletic")
    add_text(collection, "2  MUSCLE", (-3.75, -0.40, 0.075), 0.11)
    add_skeleton_figure(collection, -1.25, "standard")
    add_muscle_overlay(collection, -1.25, "standard", "athletic")
    add_soft_tissue_overlay(collection, -1.25, "standard", "athletic", "central")
    add_text(collection, "3  SOFT TISSUE", (-1.25, -0.40, 0.075), 0.11)
    add_figure(collection, 1.25, "standard", "long", "cropped",
               muscle_profile="athletic", soft_tissue_profile="central",
               label="4  DERIVED SKIN")
    add_figure(collection, 3.75, "standard", "long", "cropped",
               muscle_profile="athletic", soft_tissue_profile="central",
               shells=("tunic", "trousers", "boots", "belt"),
               outer="teal", label="5  FITTED GARMENT")
    add_figure(collection, 6.25, "standard", "long", "cropped",
               muscle_profile="athletic", soft_tissue_profile="central",
               shells=("tunic", "trousers", "boots", "belt", "cuirass",
                       "pauldron_pair", "bracers", "greaves"),
               prop="spear", pose="upright", outer="teal",
               label="6  ARMOR + LOADOUT")


def build_body_type_review(collection: bpy.types.Collection) -> None:
    add_ground(collection, 13.5, 5.5, (0.0, 0.75, -0.055))
    add_text(collection, "ONE FRAME + ONE MUSCLE SET — FOUR TISSUE RECIPES",
             (0.0, 0.4, 3.12), 0.23)
    add_text(collection, "EVERY REGION CAN MIX INDEPENDENTLY",
             (0.0, 0.4, 2.76), 0.13)
    recipes = (
        (-4.5, "low", "LOW TISSUE"),
        (-1.5, "balanced", "BALANCED"),
        (1.5, "central", "CENTRAL"),
        (4.5, "lower_body", "LOWER BODY"),
    )
    for x, tissue_profile, label in recipes:
        add_figure(collection, x, "standard", "long", "cropped",
                   muscle_profile="athletic",
                   soft_tissue_profile=tissue_profile, label=label)


def add_baked_body_review_figure(collection: bpy.types.Collection, x: float,
                                 frame: str, muscle: str, tissue: str,
                                 label: str) -> None:
    source = BODY_SKIN_OBJECTS[(frame, muscle, tissue)]
    duplicate = source.copy()
    duplicate.data = source.data.copy()
    duplicate.name = f"REVIEW_{source.name}_{label}"
    for modifier in tuple(duplicate.modifiers):
        duplicate.modifiers.remove(modifier)
    duplicate.parent = None
    duplicate.hide_render = False
    duplicate.hide_viewport = False
    duplicate.hide_set(False)
    duplicate.location = (x, 0.0, 0.0)
    duplicate.data.materials.clear()
    duplicate.data.materials.append(MATERIALS["skin"])
    collection.objects.link(duplicate)
    head_center = (x, 0.0, 1.86)
    add_head_geometry("long" if frame == "lean" else
                      "broad" if frame == "heavy" else "square",
                      head_center, collection)
    add_eye_marks(collection, head_center,
                  HEADS["long" if frame == "lean" else
                        "broad" if frame == "heavy" else "square"][0])
    add_hair_style("cropped", head_center, collection)
    add_text(collection, label, (x, -0.42, 0.075), 0.095)


def build_baked_body_review(collection: bpy.types.Collection) -> None:
    add_ground(collection, 15.8, 5.5, (0.0, 0.75, -0.055))
    add_text(collection, "ONE MOLDED SKIN — FRAME + MUSCLE + TISSUE",
             (0.0, 0.4, 3.10), 0.22)
    add_text(collection, "GARMENTS AND ARMOR SNAP ON AFTER THIS BAKE",
             (0.0, 0.4, 2.76), 0.12)
    recipes = (
        (-6.1, "lean", "slight", "low", "LEAN / SLIGHT / LOW"),
        (-3.65, "lean", "power", "balanced", "LEAN / POWER / BALANCED"),
        (-1.2, "standard", "athletic", "balanced", "STANDARD / BALANCED"),
        (1.2, "standard", "athletic", "central", "STANDARD / CENTRAL"),
        (3.65, "heavy", "slight", "lower_body", "HEAVY / LOWER BODY"),
        (6.1, "heavy", "power", "central", "HEAVY / POWER / CENTRAL"),
    )
    for x, frame, muscle, tissue, label in recipes:
        add_baked_body_review_figure(
            collection, x, frame, muscle, tissue, label)


def build_turnaround_review(collection: bpy.types.Collection) -> None:
    add_ground(collection, 11.0, 5.0, (0.0, 0.75, -0.055))
    add_text(collection, "HEAD + HAIR TURNAROUND", (0.0, 0.4, 3.05), 0.25)
    turns = ((-3.6, 0.0, "FRONT"), (-1.2, 45.0, "THREE-QUARTER"),
             (1.2, 90.0, "SIDE"), (3.6, 180.0, "BACK"))
    for index, (x, angle, label) in enumerate(turns):
        figure = new_collection(f"TURN_FIGURE_{index}", collection)
        add_figure(figure, 0.0, "standard", "long", "swept",
                   shells=("tunic", "trousers", "boots", "belt"),
                   outer="teal", accent="oxblood")
        transform_collection(figure, x, angle)
        add_text(collection, label, (x, -0.42, 0.075), 0.12)


def build_silhouette_review(collection: bpy.types.Collection) -> None:
    add_box("GEO_SilhouetteBackdrop", (0.0, 1.2, 2.15),
            (15.0, 0.10, 4.5), collection, "silhouette_bg",
            "silhouette_backdrop", width=0.0)
    add_box("GEO_SilhouetteGround", (0.0, 0.0, -0.04),
            (15.0, 3.0, 0.08), collection, "silhouette_bg",
            "silhouette_ground", width=0.0)
    recipes = (
        (-4.8, "standard", "long", "swept",
         ("tunic", "trousers", "boots", "belt", "mantle_left", "pauldron_left",
         "bracers", "greaves", "crown"), "sword", "neutral", "athletic", "low"),
        (-2.4, "heavy", "veteran", "cropped",
         ("tunic", "trousers", "boots", "cuirass", "belt", "pauldron_pair", "greaves"),
         "spear", "upright", "athletic", "balanced"),
        (0.0, "heavy", "broad", "bob",
         ("tunic", "trousers", "boots", "belt", "apron"), None, "open", "slight", "central"),
        (2.4, "heavy", "square", "crest",
         ("tunic", "trousers", "boots", "belt", "apron", "bracers"),
         "hammer", "heavy", "power", "lower_body"),
        (4.8, "lean", "long", "braided",
         ("tunic", "trousers", "boots", "belt", "mantle_left", "pack"),
         "staff", "walking", "slight", "low"),
    )
    for index, recipe in enumerate(recipes):
        figure = new_collection(f"SILHOUETTE_FIGURE_{index}", collection)
        add_figure(figure, recipe[0], recipe[1], recipe[2], recipe[3],
                   shells=recipe[4], prop=recipe[5], pose=recipe[6],
                   muscle_profile=recipe[7], soft_tissue_profile=recipe[8])
        override_mesh_materials(figure, "ink")


def add_facade_run(collection: bpy.types.Collection, x: float, y: float,
                   bays: tuple[str, ...], *, roof_color: str = "roof",
                   state: str = "neutral", label: str = "") -> None:
    width = len(bays) * 1.5
    center_x = x
    add_box(f"GEO_{label}_Foundation", (center_x, y, 0.13),
            (width + 0.25, 2.8, 0.26), collection,
            "stone_dark", "foundation", width=0.035)
    for index, bay in enumerate(bays):
        bx = center_x - width * 0.5 + 0.75 + index * 1.5
        if bay == "door":
            for side in (-1.0, 1.0):
                add_box(f"GEO_{label}_DoorSide{index}{side:+.0f}",
                        (bx + side * 0.62, y - 1.42, 1.53),
                        (0.26, 0.22, 2.80), collection,
                        "plaster", "door_wall", width=0.022)
            add_box(f"GEO_{label}_DoorHead{index}", (bx, y - 1.42, 2.61),
                    (0.98, 0.22, 0.64), collection,
                    "plaster", "door_header", width=0.022)
            add_box(f"GEO_{label}_Door{index}", (bx, y - 1.55, 1.18),
                    (1.02, 0.10, 2.10), collection,
                    "wood_dark", "door", width=0.022)
        elif bay == "window":
            add_box(f"GEO_{label}_WindowWallLow{index}", (bx, y - 1.42, 0.63),
                    (1.50, 0.22, 1.00), collection,
                    "plaster", "window_wall", width=0.022)
            add_box(f"GEO_{label}_WindowWallHigh{index}", (bx, y - 1.42, 2.45),
                    (1.50, 0.22, 0.96), collection,
                    "plaster", "window_wall", width=0.022)
            add_box(f"GEO_{label}_Window{index}", (bx, y - 1.55, 1.61),
                    (0.92, 0.035, 0.66), collection,
                    "glass", "window", width=0.006)
        else:
            add_box(f"GEO_{label}_Wall{index}", (bx, y - 1.42, 1.53),
                    (1.50, 0.22, 2.80), collection,
                    "plaster", "solid_wall", width=0.022)
        add_box(f"GEO_{label}_Post{index}",
                (center_x - width * 0.5 + index * 1.5, y - 1.57, 1.53),
                (0.14, 0.15, 2.72), collection,
                "wood_dark", "frame_post", width=0.015)
    add_box(f"GEO_{label}_PostEnd", (center_x + width * 0.5, y - 1.57, 1.53),
            (0.14, 0.15, 2.72), collection,
            "wood_dark", "frame_post", width=0.015)

    left, right = center_x - width * 0.5 - 0.18, center_x + width * 0.5 + 0.18
    add_panel(f"GEO_{label}_RoofFront", ((left, y - 1.72, 2.88),
                                         (right, y - 1.72, 2.88),
                                         (right, y, 3.58), (left, y, 3.58)),
              collection, roof_color, "roof", thickness=0.10)
    add_panel(f"GEO_{label}_RoofBack", ((left, y, 3.58), (right, y, 3.58),
                                        (right, y + 1.72, 2.88),
                                        (left, y + 1.72, 2.88)),
              collection, roof_color, "roof", thickness=0.10)
    if state == "stocked":
        for index, sx in enumerate((-0.5, 0.0, 0.5)):
            add_ico(f"GEO_{label}_Stock{index}",
                    (center_x + sx, y - 1.85, 0.27), (0.20, 0.16, 0.24),
                    collection, "ochre" if index != 1 else "green",
                    "state_stock", subdivisions=1)
    elif state == "shortage":
        add_box(f"GEO_{label}_RationBar", (center_x, y - 1.82, 0.62),
                (1.8, 0.09, 0.10), collection, "oxblood", "state_barrier", width=0.02)
    if label:
        add_text(collection, label, (center_x, y - 2.05, 0.075), 0.12)


def build_playset_review(collection: bpy.types.Collection) -> None:
    add_ground(collection, 18.0, 9.0, (0.0, 1.4, -0.055))
    add_text(collection, "ONE PLAYSET KIT — THREE PLACES", (0.0, 1.0, 4.35), 0.28)
    add_facade_run(collection, -5.8, 0.8, ("solid", "door"),
                   state="neutral", label="HOME")
    add_facade_run(collection, 0.0, 0.8, ("window", "door", "window"),
                   state="stocked", label="MARKET — STOCKED")
    add_facade_run(collection, 6.2, 0.8, ("solid", "door", "window"),
                   roof_color="stone_dark", state="shortage",
                   label="CHECKPOINT — CONTROLLED")


def render_previews() -> None:
    render_review("world_scale_lineup.png", build_scale_lineup,
                  camera_location=(0.5, -24.0, 4.2),
                  camera_target=(0.5, 0.0, 1.65), ortho_scale=22.5,
                  resolution=(1800, 720))
    render_review("figure_parts_board.png", build_parts_board,
                  camera_location=(0.0, -18.0, 5.5),
                  camera_target=(0.0, 0.0, 1.95), ortho_scale=15.5,
                  resolution=(1800, 1000))
    render_review("figure_layer_stack.png", build_layer_stack_review,
                  camera_location=(0.0, -18.0, 4.4),
                  camera_target=(0.0, 0.0, 1.35), ortho_scale=17.2,
                  resolution=(1900, 850))
    render_review("body_type_matrix.png", build_body_type_review,
                  camera_location=(0.0, -18.0, 4.4),
                  camera_target=(0.0, 0.0, 1.35), ortho_scale=12.5,
                  resolution=(1800, 800))
    render_review("baked_body_recipe_matrix.png", build_baked_body_review,
                  camera_location=(0.0, -18.0, 4.4),
                  camera_target=(0.0, 0.0, 1.35), ortho_scale=15.2,
                  resolution=(1900, 850))
    render_review("procedural_cast.png", build_cast_review,
                  camera_location=(0.0, -18.0, 4.4),
                  camera_target=(0.0, 0.0, 1.35), ortho_scale=12.8,
                  resolution=(1800, 800))
    render_review("figure_turnaround.png", build_turnaround_review,
                  camera_location=(0.0, -18.0, 4.4),
                  camera_target=(0.0, 0.0, 1.35), ortho_scale=10.5,
                  resolution=(1600, 800))
    render_review("gameplay_silhouettes.png", build_silhouette_review,
                  camera_location=(0.0, -18.0, 3.5),
                  camera_target=(0.0, 0.0, 1.25), ortho_scale=14.5,
                  resolution=(457, 285))
    render_review("playset_combinations.png", build_playset_review,
                  camera_location=(10.0, -21.0, 10.5),
                  camera_target=(0.0, 0.5, 1.65), ortho_scale=20.5,
                  resolution=(1800, 900))


SHELL_ASSET_IDS = {
    "tunic": "wk_shell_chest_tunic_v01",
    "cuirass": "wk_shell_chest_cuirass_v01",
    "belt": "wk_shell_waist_belt_v01",
    "apron": "wk_shell_waist_apron_v01",
    "mantle_left": "wk_shell_shoulder_mantle_left_v01",
    "pauldron_left": "wk_shell_shoulder_pauldron_v01",
    "pauldron_pair": "wk_shell_shoulder_pauldron_v01",
    "cape": "wk_shell_back_cape_v01",
    "pack": "wk_shell_back_pack_v01",
    "bracers": "wk_shell_forearm_bracer_v01",
    "gloves": "wk_shell_hand_glove_v01",
    "greaves": "wk_shell_shin_greave_v01",
    "boots": "wk_shell_foot_boot_v01",
    "crown": "wk_shell_head_crown_band_v01",
}

REFERENCE_RECIPES = (
    {"id": "crownless", "skeleton": "standard", "muscle_profile": "athletic",
     "soft_tissue_profile": "low",
     "head": "long", "hair": "swept",
     "shells": ["tunic", "trousers", "boots", "belt", "mantle_left",
     "pauldron_left", "bracers", "greaves", "crown"], "prop": "sword"},
    {"id": "guard", "skeleton": "heavy", "muscle_profile": "athletic",
     "soft_tissue_profile": "balanced",
     "head": "veteran", "hair": "cropped",
     "shells": ["tunic", "trousers", "boots", "cuirass", "belt",
     "pauldron_pair", "greaves"], "prop": "spear"},
    {"id": "merchant", "skeleton": "heavy", "muscle_profile": "slight",
     "soft_tissue_profile": "central",
     "head": "broad", "hair": "bob",
     "shells": ["tunic", "trousers", "boots", "belt", "apron"]},
    {"id": "laborer", "skeleton": "heavy", "muscle_profile": "power",
     "soft_tissue_profile": "lower_body",
     "head": "square", "hair": "crest",
     "shells": ["tunic", "trousers", "boots", "belt", "apron", "bracers"],
     "prop": "hammer"},
    {"id": "traveller", "skeleton": "lean", "muscle_profile": "slight",
     "soft_tissue_profile": "low",
     "head": "long", "hair": "braided",
     "shells": ["tunic", "trousers", "boots", "belt", "mantle_left", "pack"],
     "prop": "staff"},
)


def figure_assembly(recipe: dict[str, object]) -> dict[str, object]:
    buck = str(recipe["skeleton"])
    references: list[dict[str, object]] = []
    for slot in ("spine", "pelvis", "upper_arm", "forearm",
                 "thigh", "shin", "hand", "foot"):
        references.append({
            "asset": f"wk_skeleton_{slot}_v01",
            "slot": f"skeleton.{slot}",
            "socket": f"skeleton.{slot}",
            "transform_source": "skeleton_frame",
            "multiplicity": "left_and_right" if slot in {
                "upper_arm", "forearm", "thigh", "shin", "hand", "foot"} else "one",
            "local_transform": identity_transform(),
        })
    for slot in ("chest", "back", "abdomen", "glute", "deltoid",
                 "upper_arm", "forearm", "thigh", "calf", "neck"):
        references.append({
            "asset": f"wk_muscle_{slot}_v01",
            "slot": f"muscle.{slot}",
            "socket": f"muscle_bed.{slot}",
            "transform_source": "skeleton_frame",
            "inflation_source": f"muscle_profile.{slot}",
            "multiplicity": "left_and_right" if slot in {
                "deltoid", "upper_arm", "forearm", "thigh", "calf"} else "one",
            "local_transform": identity_transform(),
        })
    for slot in ("chest", "abdomen", "waist", "hip", "upper_arm",
                 "forearm", "thigh", "calf"):
        references.append({
            "asset": f"wk_soft_tissue_{slot}_v01",
            "slot": f"soft_tissue.{slot}",
            "socket": f"soft_tissue_bed.{slot}",
            "transform_source": "muscle_fit_envelope",
            "inflation_source": f"soft_tissue_profile.{slot}",
            "multiplicity": "left_and_right" if slot in {
                "upper_arm", "forearm", "thigh", "calf"} else "one",
            "local_transform": identity_transform(),
        })
    for slot in ("torso", "pelvis"):
        references.append({
            "asset": f"wk_buck_{buck}_{slot}_v01",
            "slot": slot,
            "socket": slot,
            "transform_source": "combined_body_fit_envelope",
            "local_transform": identity_transform(),
        })
    for slot in ("upper_arm", "forearm", "hand", "thigh", "shin", "foot"):
        for side in ("left", "right"):
            references.append({
                "asset": f"wk_buck_{buck}_{slot}_v01",
                "slot": f"{slot}_{side}",
                "socket": f"{slot}_{side}",
                "transform_source": "combined_body_fit_envelope",
                "mirror": side == "right",
                "local_transform": identity_transform(),
            })
    references.extend((
        {"asset": f"wk_head_{recipe['head']}_v01", "slot": "head",
         "socket": "head_center", "transform_source": "socket_frame",
         "local_transform": identity_transform()},
        {"asset": f"wk_hair_{recipe['hair']}_v01", "slot": "hair",
         "socket": "head_center", "transform_source": "socket_frame",
         "local_transform": identity_transform()},
    ))
    for shell in recipe.get("shells", []):
        if shell == "trousers":
            for limb_slot, asset_id in (
                ("thigh", "wk_garment_trouser_thigh_v01"),
                ("shin", "wk_garment_trouser_shin_v01"),
            ):
                references.append({
                    "asset": asset_id,
                    "slot": f"garment.trousers.{limb_slot}",
                    "socket": limb_slot,
                    "transform_source": "body_fit_envelope",
                    "multiplicity": "left_and_right",
                    "local_transform": identity_transform(),
                })
            continue
        reference = {
            "asset": SHELL_ASSET_IDS[str(shell)],
            "slot": str(shell),
            "socket": str(shell),
            "transform_source": "socket_frame",
            "local_transform": identity_transform(),
        }
        if shell in {"pauldron_pair", "bracers", "greaves", "gloves", "boots"}:
            reference["multiplicity"] = "left_and_right"
        references.append(reference)
    if recipe.get("prop"):
        references.append({
            "asset": f"wk_prop_{recipe['prop']}_v01",
            "slot": "held_prop",
            "socket": "hand_right",
            "transform_source": "socket_frame",
            "local_transform": identity_transform(),
        })
    return {
        "id": f"wk_assembly_{recipe['id']}_v01",
        "model_kind": "assembly",
        "category": "figure_recipe",
        "variant_sets": {
            "skeleton": [recipe["skeleton"]],
            "muscle_profile": [recipe["muscle_profile"]],
            "soft_tissue_profile": [recipe["soft_tissue_profile"]],
            "head": [recipe["head"]],
            "hair": [recipe["hair"]],
            "wear": ["clean", "worn", "damaged"],
        },
        "references": references,
    }


def transform_at(x: float, y: float, z: float,
                 rotation_degrees: float = 0.0) -> dict[str, list[float]]:
    angle = math.radians(rotation_degrees) * 0.5
    return {
        "translation": [x, y, z],
        "rotation_xyzw": [0.0, 0.0, round(math.sin(angle), 6),
                          round(math.cos(angle), 6)],
        "scale": [1.0, 1.0, 1.0],
    }


def playset_assembly(asset_id: str, bays: tuple[str, ...],
                     state: str | None = None) -> dict[str, object]:
    width = len(bays) * 1.5
    references: list[dict[str, object]] = []
    for index, bay in enumerate(bays):
        x = -width * 0.5 + 0.75 + index * 1.5
        references.append({
            "asset": f"wk_facade_{bay}_v01",
            "slot": f"facade_bay_{index}",
            "socket": f"foundation_bay_{index}",
            "transform_source": "explicit",
            "local_transform": transform_at(x, 0.0, 0.0),
        })
        references.append({
            "asset": "wk_roof_gable_span_v01",
            "slot": f"roof_span_{index}",
            "socket": f"wall_top_{index}",
            "transform_source": "explicit",
            "local_transform": transform_at(x, 0.0, 2.80),
        })
    if state:
        references.append({
            "asset": f"wk_state_market_{state}_v01",
            "slot": "state_layer",
            "socket": "market_counter",
            "transform_source": "explicit",
            "local_transform": transform_at(0.0, -1.55, 0.92),
        })
    return {
        "id": asset_id,
        "model_kind": "assembly",
        "category": "playset_recipe",
        "variant_sets": {
            "state": ["neutral", "stocked", "shortage"],
            "roof": ["gable"],
        },
        "references": references,
    }


def write_connections() -> None:
    assemblies = [figure_assembly(recipe) for recipe in REFERENCE_RECIPES]
    assemblies.extend((
        playset_assembly("wk_assembly_home_v01", ("wall", "door")),
        playset_assembly("wk_assembly_market_stocked_v01",
                         ("window", "door", "window"), "stocked"),
        playset_assembly("wk_assembly_checkpoint_shortage_v01",
                         ("wall", "door", "window"), "shortage"),
    ))
    document = {
        "library_version": LIBRARY_VERSION,
        "metadata_strategy": "shadow connectivity kept separate from geometry",
        "inspiration": ["LDraw sub-file references", "LDCad shadow snap data",
                        "OpenUSD references and variant sets"],
        "connection_profiles": {
            "skeleton_joint": {"shape": "oriented_joint_frame",
                               "polarity": "neutral", "rendered": False},
            "muscle_bed": {"shape": "bone_local_envelope",
                           "polarity": "receiver", "rendered": False},
            "soft_tissue_bed": {"shape": "muscle_local_envelope",
                                 "polarity": "receiver", "rendered": False},
            "bone_frame": {"shape": "oriented_frame", "polarity": "neutral"},
            "hand_frame": {"shape": "oriented_frame", "polarity": "receiver"},
            "foot_frame": {"shape": "contact_plane", "polarity": "neutral"},
            "head_mount": {"shape": "oriented_frame", "polarity": "receiver"},
            "fitted_shell": {"shape": "surface_envelope", "polarity": "plug",
                             "clearance_m": 0.006},
            "hand_grip": {"shape": "cylinder", "polarity": "plug",
                          "radius_m": 0.026, "depth_m": 0.095},
            "building_bay": {"shape": "plane_frame", "polarity": "neutral",
                             "grid_step_m": [1.5, 2.8]},
            "surface_mount": {"shape": "plane_frame", "polarity": "plug"},
        },
        "parts": sorted(CONNECTION_RECORDS,
                        key=lambda record: str(record["asset_id"])),
        "assemblies": assemblies,
    }
    CONNECTION_PATH.write_text(json.dumps(document, indent=2) + "\n",
                               encoding="utf-8")


def write_manifest() -> None:
    manifest = {
        "library_version": LIBRARY_VERSION,
        "art_direction": "80s action figure construction and playset logic",
        "generation": "offline procedural compatible world modules",
        "coordinate_system": "Blender +Z up; glTF +Y up",
        "geometry_contract": {
            "front_face_winding": "counter_clockwise",
            "normals": "outward",
            "transforms": "components export in local anchor space",
            "assembly_policy": "reference stable components; do not copy geometry",
            "connectivity": "assets/world_kit_connections.json",
            "runtime_delivery": "glTF 2.0 with application metadata in extras",
            "instancing_candidate": "EXT_mesh_gpu_instancing",
        },
        "part_hierarchy": {
            "primitive": "shared generator shape helper",
            "subpart": "repeated molded shape inside a component",
            "component": "terminal published module",
            "assembly": "references components and transforms",
            "state_layer": "strong additive override on an assembly",
        },
        "figure_layer_order": [
            "skeleton", "muscle", "soft_tissue", "skin_surface", "garment",
            "armor", "identity", "equipment",
        ],
        "body_generation_contract": {
            "skeleton": "owns bone length, joints, posture, and sockets",
            "muscle": "bone-local modules inflate cross-section only",
            "soft_tissue": "adds mixable regional padding in metres",
            "skin_surface": "one continuous neck-down wrap derived from the combined muscle and soft-tissue envelope",
            "runtime_bake": "36 safe frame, muscle, and soft-tissue recipe skins",
            "runtime_skinning": "18 stable bones, four weights per vertex, GPU matrix palette",
            "modular_boundary": "garments, armor, head, hair, and equipment stay separate",
            "head_delivery": "four indexed modular head families selected from appearance",
            "garment": "refits to the body envelope with cloth clearance",
            "armor": "refits above garments and preserves hard sockets",
            "forbidden": [
                "clothing changes body mass", "muscle changes bone length",
                "soft tissue moves joints or sockets",
                "whole-model scale fixes fit",
            ],
        },
        "scale_contract": SCALE_CONTRACT,
        "figure_standard": {
            "height": 1.90,
            "head_count_range": [5.75, 6.25],
            "skeleton_families": list(BUCKS),
            "muscle_profiles": MUSCLE_PROFILES,
            "soft_tissue_profiles": SOFT_TISSUE_PROFILES,
            "soft_tissue_mix_policy": {
                "regions_are_independent": True,
                "unit": "meter",
                "maximum_padding_per_region": 0.06,
                "profiles_are_safe_presets_not_locked_types": True,
            },
            "head_families": list(HEADS),
            "sockets": FIGURE_SOCKETS,
        },
        "recipe_grammar": [
            "skeleton_frame", "muscle_recipe", "soft_tissue_recipe",
            "derived_skin", "garment_layers", "armor_loadout",
            "head_family", "silhouette_part", "palette", "wear",
        ],
        "reference_recipes": list(REFERENCE_RECIPES),
        "modules": sorted(ASSET_RECORDS, key=lambda record: str(record["id"])),
        "baked_body_skins": sorted(
            BODY_SKIN_RECORDS, key=lambda record: str(record["id"])),
        "review_previews": {
            "world_scale_lineup": "assets/previews/world_kit/world_scale_lineup.png",
            "figure_parts_board": "assets/previews/world_kit/figure_parts_board.png",
            "figure_layer_stack": "assets/previews/world_kit/figure_layer_stack.png",
            "body_type_matrix": "assets/previews/world_kit/body_type_matrix.png",
            "baked_body_recipe_matrix": "assets/previews/world_kit/baked_body_recipe_matrix.png",
            "procedural_cast": "assets/previews/world_kit/procedural_cast.png",
            "figure_turnaround": "assets/previews/world_kit/figure_turnaround.png",
            "gameplay_silhouettes": "assets/previews/world_kit/gameplay_silhouettes.png",
            "playset_combinations": "assets/previews/world_kit/playset_combinations.png",
        },
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def build() -> None:
    reset_scene()
    make_materials()
    build_skeleton_and_muscle_assets()
    build_soft_tissue_assets()
    build_body_assets()
    build_hair_assets()
    build_shell_assets()
    build_prop_assets()
    build_facade_assets()
    export_assets()
    build_runtime_body_skins()
    render_previews()
    write_connections()
    write_manifest()
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH), compress=True)
    print(f"built {len(ASSET_RECORDS)} world-kit modules")
    print(f"built {len(BODY_SKIN_RECORDS)} baked runtime body skins")
    print(f"manifest: {MANIFEST_PATH}")
    print(f"previews: {PREVIEW_DIR}")


if __name__ == "__main__":
    build()
