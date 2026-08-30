#!/usr/bin/env python3
"""Build the silhouette-first Crownless creature library.

The library expands the human cast with three body-plan families:

* goblins, using the existing biped stepped-pose contract;
* horses and cows, sharing one quadruped locomotion contract;
* a dragon, using a quadruped base with authored neck, tail, jaw, and wings.

Every runtime file is one mesh with one indexed material. Goblins and the
dragon keep held pose GLBs. Horses and cows use one rigid-weighted skin each,
driven by the game's shared quadruped bone pose at runtime.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass, replace
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
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_creature_library.blend"
EXPORT_DIR = ROOT / "assets" / "exports" / "creatures"
PREVIEW_PATH = ROOT / "assets" / "previews" / "creatures" / "creature_family_sheet.png"
MANIFEST_PATH = ROOT / "assets" / "creature_manifest.json"
LIBRARY_VERSION = "0.3.0"

BIPED_POSES = (
    "idle",
    "contact_a", "down_a", "passing_a", "up_a",
    "contact_b", "down_b", "passing_b", "up_b",
)
DRAGON_POSES = ("idle", "stalk_a", "stalk_b", "threat", "rest")

QUADRUPED_BONES = (
    ("root", None),
    ("body", "root"),
    ("chest", "body"),
    ("neck", "chest"),
    ("head", "neck"),
    ("upper_leg.FL", "chest"),
    ("lower_leg.FL", "upper_leg.FL"),
    ("hoof.FL", "lower_leg.FL"),
    ("upper_leg.FR", "chest"),
    ("lower_leg.FR", "upper_leg.FR"),
    ("hoof.FR", "lower_leg.FR"),
    ("upper_leg.HL", "body"),
    ("lower_leg.HL", "upper_leg.HL"),
    ("hoof.HL", "lower_leg.HL"),
    ("upper_leg.HR", "body"),
    ("lower_leg.HR", "upper_leg.HR"),
    ("hoof.HR", "lower_leg.HR"),
    ("tail.root", "body"),
    ("tail", "tail.root"),
)

MATERIAL_ORDER = (
    "skin",
    "secondary",
    "hide",
    "cloth",
    "leather",
    "horn",
    "metal",
    "accent",
    "eye",
)

FAMILY_PALETTES = {
    "goblin": {
        "skin": (0.31, 0.46, 0.20, 1.0),
        "secondary": (0.16, 0.25, 0.12, 1.0),
        "hide": (0.22, 0.16, 0.10, 1.0),
        "cloth": (0.25, 0.18, 0.13, 1.0),
        "leather": (0.16, 0.085, 0.045, 1.0),
        "horn": (0.64, 0.55, 0.34, 1.0),
        "metal": (0.29, 0.34, 0.34, 1.0),
        "accent": (0.58, 0.18, 0.20, 1.0),
        "eye": (0.88, 0.66, 0.13, 1.0),
    },
    "horse": {
        "skin": (0.36, 0.17, 0.075, 1.0),
        "secondary": (0.075, 0.050, 0.035, 1.0),
        "hide": (0.64, 0.39, 0.18, 1.0),
        "cloth": (0.12, 0.30, 0.29, 1.0),
        "leather": (0.14, 0.070, 0.035, 1.0),
        "horn": (0.62, 0.55, 0.39, 1.0),
        "metal": (0.31, 0.36, 0.36, 1.0),
        "accent": (0.62, 0.20, 0.18, 1.0),
        "eye": (0.018, 0.016, 0.013, 1.0),
    },
    "cow": {
        "skin": (0.68, 0.61, 0.47, 1.0),
        "secondary": (0.20, 0.13, 0.085, 1.0),
        "hide": (0.76, 0.69, 0.55, 1.0),
        "cloth": (0.14, 0.31, 0.29, 1.0),
        "leather": (0.17, 0.085, 0.042, 1.0),
        "horn": (0.67, 0.59, 0.42, 1.0),
        "metal": (0.31, 0.36, 0.36, 1.0),
        "accent": (0.55, 0.18, 0.18, 1.0),
        "eye": (0.018, 0.016, 0.013, 1.0),
    },
    "dragon": {
        "skin": (0.075, 0.16, 0.16, 1.0),
        "secondary": (0.30, 0.075, 0.070, 1.0),
        "hide": (0.12, 0.22, 0.20, 1.0),
        "cloth": (0.24, 0.08, 0.08, 1.0),
        "leather": (0.12, 0.065, 0.035, 1.0),
        "horn": (0.65, 0.52, 0.28, 1.0),
        "metal": (0.26, 0.31, 0.31, 1.0),
        "accent": (0.77, 0.35, 0.12, 1.0),
        "eye": (0.96, 0.70, 0.10, 1.0),
    },
}

PREVIEW_MATERIALS: dict[tuple[str, str], bpy.types.Material] = {}
INDEXED_MATERIAL: bpy.types.Material | None = None


@dataclass(frozen=True)
class CreatureSpec:
    family: str
    variant: str
    variant_index: int
    silhouette: str
    runtime_morphology: str
    gait_contract: str
    equipment: tuple[str, ...] = ()
    pose: str = "idle"

    @property
    def asset_id(self) -> str:
        suffix = "" if self.pose == "idle" else f"_{self.pose}"
        return f"creature_{self.variant}{suffix}_v01"


CREATURES = (
    CreatureSpec("goblin", "goblin_scavenger", 0,
                 "low narrow body, oversized ears and hands",
                 "biped", "npc_stepped", ("pack", "hook")),
    CreatureSpec("goblin", "goblin_raider", 1,
                 "forward armored wedge with a high spear",
                 "biped", "npc_stepped", ("helmet", "armor", "spear")),
    CreatureSpec("goblin", "goblin_tribute_bearer", 2,
                 "broad burdened carrier framing a bright offering",
                 "biped", "npc_stepped", ("offering", "harness")),
    CreatureSpec("horse", "horse", 3,
                 "high shoulder, arched neck, open leg gaps",
                 "quadruped", "quadruped_runtime_skin", ("mane", "tail")),
    CreatureSpec("cow", "cow", 4,
                 "deep barrel, low head, horned horizontal line",
                 "quadruped", "quadruped_runtime_skin", ("horns", "udder")),
    CreatureSpec("dragon", "dragon", 5,
                 "long grounded predator with crown horns and folded wings",
                 "quadruped", "dragon_authored",
                 ("wings", "neck", "tail", "jaw", "spines")),
)


def poses_for(spec: CreatureSpec) -> tuple[str, ...]:
    if spec.family == "dragon":
        return DRAGON_POSES
    if spec.runtime_morphology == "quadruped":
        return ("idle",)
    return BIPED_POSES


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_CREATURE_LIBRARY"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene["cc_library_version"] = LIBRARY_VERSION
    scene["cc_art_direction"] = "silhouette_first_pseudo_pixel_creatures"
    scene["cc_forward_axis"] = "-Y"
    scene["cc_up_axis"] = "+Z"
    world = bpy.data.worlds.new("CC_CreatureWorld")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.012, 0.017, 0.018, 1.0)
    background.inputs["Strength"].default_value = 0.30
    scene.world = world


def make_materials() -> None:
    global INDEXED_MATERIAL
    for family, palette in FAMILY_PALETTES.items():
        for semantic in MATERIAL_ORDER:
            material = bpy.data.materials.new(
                f"MAT_CREATURE_PREVIEW_{family.upper()}_{semantic.upper()}")
            material.diffuse_color = palette[semantic]
            material.use_nodes = True
            material.use_backface_culling = False
            principled = material.node_tree.nodes.get("Principled BSDF")
            principled.inputs["Base Color"].default_value = palette[semantic]
            principled.inputs["Roughness"].default_value = 0.80
            if semantic == "metal":
                principled.inputs["Metallic"].default_value = 0.45
                principled.inputs["Roughness"].default_value = 0.48
            PREVIEW_MATERIALS[(family, semantic)] = material
    INDEXED_MATERIAL = bpy.data.materials.new("MAT_CREATURE_INDEXED")
    INDEXED_MATERIAL.diffuse_color = (1.0, 1.0, 1.0, 1.0)
    INDEXED_MATERIAL.use_nodes = True
    INDEXED_MATERIAL.use_backface_culling = False
    nodes = INDEXED_MATERIAL.node_tree.nodes
    principled = nodes.get("Principled BSDF")
    vertex_color = nodes.new("ShaderNodeVertexColor")
    vertex_color.layer_name = "COLOR_0"
    INDEXED_MATERIAL.node_tree.links.new(
        vertex_color.outputs["Color"], principled.inputs["Base Color"])
    principled.inputs["Roughness"].default_value = 0.80


def new_collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def move_to(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)


def tag(obj: bpy.types.Object, spec: CreatureSpec, part: str) -> None:
    obj["cc_asset_id"] = spec.asset_id
    obj["cc_family"] = spec.family
    obj["cc_variant"] = spec.variant
    obj["cc_pose"] = spec.pose
    obj["cc_part"] = part
    obj["cc_library_version"] = LIBRARY_VERSION


def assign(obj: bpy.types.Object, semantic: str, spec: CreatureSpec) -> None:
    obj.data.materials.append(PREVIEW_MATERIALS[(spec.family, semantic)])


def add_bevel(obj: bpy.types.Object, width: float, segments: int = 1) -> None:
    bevel = obj.modifiers.new("CC_SilhouetteBevel", "BEVEL")
    bevel.width = width
    bevel.segments = segments
    bevel.limit_method = "ANGLE"


def add_box(name: str, center: Vector | tuple[float, float, float],
            dimensions: tuple[float, float, float], semantic: str,
            collection: bpy.types.Collection, spec: CreatureSpec, part: str,
            *, rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
            bevel: float = 0.012) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=center, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0.0:
        add_bevel(obj, min(bevel, min(dimensions) * 0.20))
    assign(obj, semantic, spec)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_ellipsoid(name: str, center: Vector | tuple[float, float, float],
                  scale: tuple[float, float, float], semantic: str,
                  collection: bpy.types.Collection, spec: CreatureSpec,
                  part: str, *, subdivisions: int = 1) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(
        subdivisions=subdivisions, radius=1.0, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign(obj, semantic, spec)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_segment(name: str, start: Vector | tuple[float, float, float],
                end: Vector | tuple[float, float, float], start_radius: float,
                end_radius: float, semantic: str,
                collection: bpy.types.Collection, spec: CreatureSpec,
                part: str, *, sides: int = 8) -> bpy.types.Object:
    a = Vector(start)
    b = Vector(end)
    axis = b - a
    if axis.length < 0.0001:
        raise ValueError(f"{name} has no length")
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    tangent = axis.normalized()
    helper = Vector((0.0, 0.0, 1.0))
    if abs(tangent.dot(helper)) > 0.90:
        helper = Vector((1.0, 0.0, 0.0))
    right = tangent.cross(helper).normalized()
    up = right.cross(tangent).normalized()
    for point, radius in ((a, start_radius), (b, end_radius)):
        for side in range(sides):
            angle = math.tau * float(side) / float(sides)
            vertex = point + right * math.cos(angle) * radius
            vertex += up * math.sin(angle) * radius
            vertices.append(tuple(vertex))
    for side in range(sides):
        following = (side + 1) % sides
        faces.append((side, following, sides + following, sides + side))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple(range(sides, sides * 2)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def add_cone(name: str, center: Vector | tuple[float, float, float],
             radius: float, depth: float, semantic: str,
             collection: bpy.types.Collection, spec: CreatureSpec, part: str,
             *, rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
             vertices: int = 6) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices, radius1=radius, radius2=0.0, depth=depth,
        location=center, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    assign(obj, semantic, spec)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_prism(name: str, points: tuple[tuple[float, float, float], ...],
              thickness: float, semantic: str,
              collection: bpy.types.Collection, spec: CreatureSpec,
              part: str) -> bpy.types.Object:
    count = len(points)
    vertices = [(x, y, z - thickness * 0.5) for x, y, z in points]
    vertices += [(x, y, z + thickness * 0.5) for x, y, z in points]
    faces: list[tuple[int, ...]] = [
        tuple(reversed(range(count))),
        tuple(range(count, count * 2)),
    ]
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, following, count + following, count + index))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def add_lateral_prism(
    name: str,
    points: tuple[tuple[float, float, float], ...],
    thickness: float,
    semantic: str,
    collection: bpy.types.Collection,
    spec: CreatureSpec,
    part: str,
) -> bpy.types.Object:
    """Build a thin shape in the vertical YZ plane.

    The general prism helper adds depth on Z, which is right for wings and
    ground-facing shapes. A horse's mane needs width across X so its jagged
    profile stays visible from both sides.
    """
    count = len(points)
    vertices = [(x - thickness * 0.5, y, z) for x, y, z in points]
    vertices += [(x + thickness * 0.5, y, z) for x, y, z in points]
    faces: list[tuple[int, ...]] = [
        tuple(reversed(range(count))),
        tuple(range(count, count * 2)),
    ]
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, following, count + following, count + index))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def pose_phase(pose: str) -> float:
    phases = {
        "idle": 0.0,
        "contact_a": 0.0,
        "down_a": 0.125,
        "passing_a": 0.25,
        "up_a": 0.375,
        "contact_b": 0.5,
        "down_b": 0.625,
        "passing_b": 0.75,
        "up_b": 0.875,
        "stalk_a": 0.12,
        "stalk_b": 0.62,
        "threat": 0.0,
        "rest": 0.0,
    }
    return phases[pose]


def build_goblin(spec: CreatureSpec,
                 collection: bpy.types.Collection) -> None:
    phase = pose_phase(spec.pose)
    moving = spec.pose != "idle"
    cycle = math.sin(phase * math.tau) if moving else 0.0
    lift_left = max(0.0, math.sin(phase * math.tau)) * 0.13 if moving else 0.0
    lift_right = max(0.0, -math.sin(phase * math.tau)) * 0.13 if moving else 0.0
    bob = -0.035 * abs(math.sin(phase * math.tau)) if moving else 0.0
    forward = 0.20 * cycle
    root = Vector((0.0, 0.0, bob))

    hip_l = root + Vector((-0.13, 0.015, 0.63))
    hip_r = root + Vector((0.13, 0.015, 0.63))
    foot_l = root + Vector((-0.15, -forward, 0.08 + lift_left))
    foot_r = root + Vector((0.15, forward, 0.08 + lift_right))
    knee_l = (hip_l + foot_l) * 0.5 + Vector((-0.035, -0.10, 0.02))
    knee_r = (hip_r + foot_r) * 0.5 + Vector((0.035, -0.10, 0.02))
    chest = root + Vector((0.0, -0.055, 0.93))
    shoulder_l = root + Vector((-0.23, -0.075, 0.98))
    shoulder_r = root + Vector((0.23, -0.075, 0.98))
    hand_l = root + Vector((-0.31, forward * 0.85 - 0.05, 0.47))
    hand_r = root + Vector((0.31, -forward * 0.85 - 0.05, 0.47))
    elbow_l = (shoulder_l + hand_l) * 0.5 + Vector((-0.055, -0.045, 0.0))
    elbow_r = (shoulder_r + hand_r) * 0.5 + Vector((0.055, -0.045, 0.0))

    add_ellipsoid("GOBLIN_Torso", chest, (0.27, 0.20, 0.31), "cloth",
                  collection, spec, "torso")
    add_box("GOBLIN_Belt", root + Vector((0.0, -0.01, 0.68)),
            (0.35, 0.22, 0.10), "leather", collection, spec, "belt")
    add_segment("GOBLIN_Thigh_L", hip_l, knee_l, 0.105, 0.085, "cloth",
                collection, spec, "thigh_left")
    add_segment("GOBLIN_Shin_L", knee_l, foot_l, 0.080, 0.060, "skin",
                collection, spec, "shin_left")
    add_segment("GOBLIN_Thigh_R", hip_r, knee_r, 0.105, 0.085, "cloth",
                collection, spec, "thigh_right")
    add_segment("GOBLIN_Shin_R", knee_r, foot_r, 0.080, 0.060, "skin",
                collection, spec, "shin_right")
    add_box("GOBLIN_Foot_L", foot_l + Vector((0.0, -0.06, 0.0)),
            (0.20, 0.30, 0.10), "leather", collection, spec, "foot_left")
    add_box("GOBLIN_Foot_R", foot_r + Vector((0.0, -0.06, 0.0)),
            (0.20, 0.30, 0.10), "leather", collection, spec, "foot_right")
    add_segment("GOBLIN_UpperArm_L", shoulder_l, elbow_l, 0.085, 0.070,
                "cloth", collection, spec, "upper_arm_left")
    add_segment("GOBLIN_Forearm_L", elbow_l, hand_l, 0.074, 0.057,
                "skin", collection, spec, "forearm_left")
    add_segment("GOBLIN_UpperArm_R", shoulder_r, elbow_r, 0.085, 0.070,
                "cloth", collection, spec, "upper_arm_right")
    add_segment("GOBLIN_Forearm_R", elbow_r, hand_r, 0.074, 0.057,
                "skin", collection, spec, "forearm_right")
    add_ellipsoid("GOBLIN_Hand_L", hand_l, (0.090, 0.072, 0.11), "skin",
                  collection, spec, "hand_left")
    add_ellipsoid("GOBLIN_Hand_R", hand_r, (0.090, 0.072, 0.11), "skin",
                  collection, spec, "hand_right")

    head = root + Vector((0.0, -0.09, 1.26))
    add_ellipsoid("GOBLIN_Head", head, (0.24, 0.20, 0.23), "skin",
                  collection, spec, "head", subdivisions=2)
    add_cone("GOBLIN_Ear_L", head + Vector((-0.27, 0.01, 0.015)),
             0.13, 0.34, "skin", collection, spec, "ear_left",
             rotation=(0.0, -math.pi * 0.5, 0.0), vertices=3)
    add_cone("GOBLIN_Ear_R", head + Vector((0.27, 0.01, 0.015)),
             0.13, 0.34, "skin", collection, spec, "ear_right",
             rotation=(0.0, math.pi * 0.5, 0.0), vertices=3)
    add_cone("GOBLIN_Nose", head + Vector((0.0, -0.22, -0.025)),
             0.075, 0.20, "secondary", collection, spec, "nose",
             rotation=(math.pi * 0.5, 0.0, 0.0), vertices=5)
    add_ellipsoid("GOBLIN_Eye_L", head + Vector((-0.082, -0.185, 0.055)),
                  (0.036, 0.021, 0.045), "eye", collection, spec, "eye_left")
    add_ellipsoid("GOBLIN_Eye_R", head + Vector((0.082, -0.185, 0.055)),
                  (0.036, 0.021, 0.045), "eye", collection, spec, "eye_right")

    if spec.variant == "goblin_scavenger":
        add_ellipsoid("GOBLIN_ScavengerPack",
                      root + Vector((0.0, 0.20, 0.91)),
                      (0.25, 0.17, 0.30), "hide", collection, spec, "pack")
        add_segment("GOBLIN_HookHandle", hand_r,
                    hand_r + Vector((0.02, -0.05, -0.34)),
                    0.030, 0.027, "leather", collection, spec, "hook_handle")
        add_cone("GOBLIN_Hook", hand_r + Vector((0.02, -0.07, -0.40)),
                 0.070, 0.16, "metal", collection, spec, "hook",
                 rotation=(math.pi * 0.5, 0.0, 0.0), vertices=5)
    elif spec.variant == "goblin_raider":
        add_box("GOBLIN_ChestArmor", chest + Vector((0.0, -0.19, 0.015)),
                (0.39, 0.055, 0.33), "metal", collection, spec, "armor")
        add_cone("GOBLIN_Helmet", head + Vector((0.0, 0.0, 0.20)),
                 0.23, 0.28, "metal", collection, spec, "helmet", vertices=7)
        spear_bottom = hand_r + Vector((0.02, 0.02, -0.38))
        spear_top = hand_r + Vector((-0.02, -0.02, 0.92))
        add_segment("GOBLIN_Spear", spear_bottom, spear_top,
                    0.028, 0.024, "leather", collection, spec, "spear")
        add_cone("GOBLIN_SpearHead", spear_top + Vector((0.0, 0.0, 0.09)),
                 0.070, 0.22, "metal", collection, spec, "spear_head")
    else:
        offering = root + Vector((0.0, -0.35, 0.72))
        add_box("GOBLIN_Offering", offering, (0.44, 0.34, 0.31), "accent",
                collection, spec, "offering", bevel=0.025)
        add_box("GOBLIN_OfferingBand", offering + Vector((0.0, -0.18, 0.0)),
                (0.12, 0.025, 0.33), "horn", collection, spec, "offering_band")
        add_segment("GOBLIN_Harness_L", shoulder_l,
                    offering + Vector((-0.16, 0.12, 0.12)),
                    0.026, 0.026, "leather", collection, spec, "harness_left")
        add_segment("GOBLIN_Harness_R", shoulder_r,
                    offering + Vector((0.16, 0.12, 0.12)),
                    0.026, 0.026, "leather", collection, spec, "harness_right")


def quadruped_stride(spec: CreatureSpec) -> dict[str, float]:
    phase = pose_phase(spec.pose)
    moving = spec.pose != "idle"
    if not moving:
        return {"fl": 0.0, "fr": 0.0, "hl": 0.0, "hr": 0.0,
                "lift_fl": 0.0, "lift_fr": 0.0,
                "lift_hl": 0.0, "lift_hr": 0.0, "bob": 0.0}
    offsets = {"fl": 0.0, "fr": 0.5, "hl": 0.5, "hr": 0.0}
    result: dict[str, float] = {}
    for name, offset in offsets.items():
        value = math.sin((phase + offset) * math.tau)
        result[name] = value
        result[f"lift_{name}"] = max(0.0, value) * 0.15
    result["bob"] = -0.030 * abs(math.sin(phase * math.tau * 2.0))
    return result


def build_quadruped(spec: CreatureSpec,
                    collection: bpy.types.Collection) -> None:
    cow = spec.family == "cow"
    stride = quadruped_stride(spec)
    body_z = (1.08 if cow else 1.24) + stride["bob"]
    half_width = 0.38 if cow else 0.31
    front_y = -0.57
    hind_y = 0.57
    body_scale = (0.62, 0.93, 0.45) if cow else (0.48, 0.86, 0.40)
    body_semantic = "hide" if cow else "skin"
    add_ellipsoid("CREATURE_Barrel", (0.0, 0.0, body_z), body_scale,
                  body_semantic, collection, spec, "barrel", subdivisions=2)
    add_ellipsoid("CREATURE_Chest", (0.0, -0.53, body_z + 0.06),
                  (body_scale[0] * 0.91, 0.43, body_scale[2] * 1.04),
                  "skin", collection, spec, "chest", subdivisions=1)
    if not cow:
        # Separate shoulder and haunch masses stop the horse from reading as a
        # barrel on sticks at the game's small art resolution.
        add_ellipsoid("HORSE_Shoulder", (0.0, -0.50, body_z + 0.08),
                      (0.43, 0.38, 0.45), "skin", collection, spec,
                      "chest", subdivisions=2)
        add_ellipsoid("HORSE_Haunch", (0.0, 0.55, body_z + 0.04),
                      (0.45, 0.43, 0.43), "hide", collection, spec,
                      "barrel", subdivisions=2)
        add_ellipsoid("HORSE_Belly", (0.0, 0.02, body_z - 0.18),
                      (0.41, 0.60, 0.23), "hide", collection, spec,
                      "barrel")

    leg_roots = {
        "fl": Vector((-half_width, front_y, body_z - 0.08)),
        "fr": Vector((half_width, front_y, body_z - 0.08)),
        "hl": Vector((-half_width, hind_y, body_z - 0.10)),
        "hr": Vector((half_width, hind_y, body_z - 0.10)),
    }
    for name, root in leg_roots.items():
        travel = stride[name] * (0.20 if cow else 0.27)
        lift = stride[f"lift_{name}"]
        hoof = Vector((root.x, root.y - travel, 0.10 + lift))
        direction = -1.0 if name.startswith("f") else 1.0
        knee = (root + hoof) * 0.5 + Vector((0.0, direction * 0.10, 0.02))
        radius = 0.105 if cow else 0.120
        add_segment(f"CREATURE_UpperLeg_{name.upper()}", root, knee,
                    radius, radius * (0.82 if cow else 0.72), "skin",
                    collection, spec,
                    f"upper_leg_{name}")
        add_segment(f"CREATURE_LowerLeg_{name.upper()}", knee, hoof,
                    radius * (0.78 if cow else 0.70),
                    radius * (0.58 if cow else 0.48),
                    "secondary" if cow else "skin", collection, spec,
                    f"lower_leg_{name}")
        hoof_size = (0.18, 0.23, 0.12) if cow else (0.18, 0.25, 0.14)
        add_box(f"CREATURE_Hoof_{name.upper()}",
                hoof + Vector((0.0, -0.050, -0.015)), hoof_size,
                "secondary", collection, spec, f"hoof_{name}")

    if cow:
        neck_start = Vector((0.0, -0.52, body_z + 0.20))
        neck_end = Vector((0.0, -0.91, body_z + 0.08))
        head = Vector((0.0, -1.15, body_z + 0.02))
        add_segment("COW_Neck", neck_start, neck_end, 0.27, 0.23, "skin",
                    collection, spec, "neck", sides=9)
        add_ellipsoid("COW_Head", head, (0.31, 0.39, 0.26), "skin",
                      collection, spec, "head", subdivisions=2)
        add_ellipsoid("COW_Muzzle", head + Vector((0.0, -0.31, -0.06)),
                      (0.28, 0.19, 0.16), "secondary", collection, spec,
                      "muzzle")
        for side, sign in (("L", -1.0), ("R", 1.0)):
            eye = head + Vector((0.22 * sign, -0.17, 0.075))
            add_ellipsoid(f"COW_Eye_{side}", eye, (0.036, 0.025, 0.044),
                          "eye", collection, spec, f"eye_{side.lower()}")
            horn_base = head + Vector((0.23 * sign, -0.02, 0.20))
            add_cone(f"COW_Horn_{side}",
                     horn_base + Vector((0.13 * sign, 0.0, 0.06)),
                     0.075, 0.34, "horn", collection, spec,
                     f"horn_{side.lower()}",
                     rotation=(0.0, math.pi * 0.5 * sign, 0.0), vertices=6)
        add_ellipsoid("COW_Udder", (0.0, 0.26, body_z - 0.47),
                      (0.24, 0.28, 0.16), "accent", collection, spec, "udder")
        for x in (-0.12, 0.12):
            add_segment("COW_Teat", (x, 0.18, body_z - 0.54),
                        (x, 0.18, body_z - 0.70), 0.035, 0.025, "accent",
                        collection, spec, "teat", sides=6)
        add_prism("COW_ShoulderPatch",
                  ((-0.42, -0.46, body_z + 0.12),
                   (-0.45, -0.10, body_z + 0.35),
                   (-0.43, 0.20, body_z + 0.12),
                   (-0.41, -0.02, body_z - 0.25)),
                  0.035, "secondary", collection, spec, "hide_patch")
    else:
        neck_start = Vector((0.0, -0.48, body_z + 0.20))
        neck_curve = Vector((0.0, -0.68, body_z + 0.49))
        neck_end = Vector((0.0, -0.95, body_z + 0.59))
        head = Vector((0.0, -1.18, body_z + 0.53))
        add_segment("HORSE_NeckLower", neck_start, neck_curve,
                    0.30, 0.23, "skin", collection, spec, "neck", sides=9)
        add_segment("HORSE_NeckUpper", neck_curve, neck_end,
                    0.23, 0.15, "skin", collection, spec, "neck", sides=9)
        add_ellipsoid("HORSE_Head", head, (0.23, 0.34, 0.25), "skin",
                      collection, spec, "head", subdivisions=2)
        face = head + Vector((0.0, -0.25, -0.035))
        muzzle = head + Vector((0.0, -0.48, -0.12))
        add_segment("HORSE_Face", face, muzzle, 0.16, 0.12, "skin",
                    collection, spec, "head", sides=7)
        add_ellipsoid("HORSE_Muzzle", muzzle,
                      (0.18, 0.20, 0.14), "secondary", collection, spec,
                      "muzzle", subdivisions=2)
        for side, sign in (("L", -1.0), ("R", 1.0)):
            add_cone(f"HORSE_Ear_{side}",
                     head + Vector((0.105 * sign, 0.015, 0.30)),
                     0.072, 0.28, "skin", collection, spec,
                     f"ear_{side.lower()}", vertices=5)
            add_ellipsoid(f"HORSE_Eye_{side}",
                          head + Vector((0.175 * sign, -0.15, 0.075)),
                          (0.034, 0.024, 0.042), "eye", collection, spec,
                          f"eye_{side.lower()}")
            add_ellipsoid(f"HORSE_Nostril_{side}",
                          muzzle + Vector((0.095 * sign, -0.15, 0.025)),
                          (0.030, 0.018, 0.020), "eye", collection, spec,
                          f"nostril_{side.lower()}")
        add_box("HORSE_Blaze", head + Vector((0.0, -0.315, 0.055)),
                (0.070, 0.025, 0.24), "horn", collection, spec, "blaze",
                rotation=(0.16, 0.0, 0.0), bevel=0.010)
        add_cone("HORSE_Forelock", head + Vector((0.0, -0.08, 0.23)),
                 0.095, 0.24, "secondary", collection, spec, "forelock",
                 rotation=(math.pi, 0.0, 0.0), vertices=5)
        mane_points = (
            (0.0, -0.38, body_z + 0.33),
            (0.0, -0.49, body_z + 0.58),
            (0.0, -0.65, body_z + 0.75),
            (0.0, -0.82, body_z + 0.77),
            (0.0, -0.98, body_z + 0.66),
            (0.0, -0.82, body_z + 0.52),
            (0.0, -0.64, body_z + 0.43),
            (0.0, -0.47, body_z + 0.24),
        )
        add_lateral_prism("HORSE_Mane", mane_points, 0.11, "secondary",
                          collection, spec, "mane")

    tail_base = Vector((0.0, 0.82, body_z + 0.08))
    tail_mid = Vector((0.0, 1.10, body_z - 0.08))
    tail_end = Vector((0.0, 1.28, body_z - (0.62 if cow else 0.48)))
    add_segment("CREATURE_TailRoot", tail_base, tail_mid,
                0.075 if cow else 0.105, 0.055 if cow else 0.095,
                "secondary", collection, spec, "tail_root")
    add_segment("CREATURE_Tail", tail_mid, tail_end,
                0.060 if cow else 0.135, 0.035 if cow else 0.050,
                "secondary", collection, spec, "tail")
    if not cow:
        for side, sign in (("L", -1.0), ("R", 1.0)):
            strand_end = tail_end + Vector((0.075 * sign, 0.015, 0.04))
            add_segment(f"HORSE_TailStrand_{side}", tail_mid, strand_end,
                        0.070, 0.028, "secondary", collection, spec, "tail",
                        sides=7)


def build_dragon(spec: CreatureSpec,
                 collection: bpy.types.Collection) -> None:
    phase = pose_phase(spec.pose)
    threat = spec.pose == "threat"
    resting = spec.pose == "rest"
    cycle = math.sin(phase * math.tau)
    body_z = 0.86 if resting else 1.24 - 0.035 * abs(cycle)
    add_ellipsoid("DRAGON_Body", (0.0, 0.15, body_z),
                  (0.78, 1.34, 0.58), "skin", collection, spec, "body",
                  subdivisions=2)
    add_ellipsoid("DRAGON_Chest", (0.0, -0.70, body_z + 0.16),
                  (0.72, 0.67, 0.66), "hide", collection, spec, "chest",
                  subdivisions=2)

    leg_roots = {
        "fl": Vector((-0.54, -0.70, body_z - 0.05)),
        "fr": Vector((0.54, -0.70, body_z - 0.05)),
        "hl": Vector((-0.58, 0.72, body_z - 0.12)),
        "hr": Vector((0.58, 0.72, body_z - 0.12)),
    }
    offsets = {"fl": 0.0, "fr": 0.5, "hl": 0.5, "hr": 0.0}
    for name, root in leg_roots.items():
        leg_cycle = math.sin((phase + offsets[name]) * math.tau)
        moving = spec.pose in ("stalk_a", "stalk_b")
        travel = leg_cycle * 0.26 if moving else 0.0
        lift = max(0.0, leg_cycle) * 0.16 if moving else 0.0
        if resting:
            hoof = Vector((root.x * 1.10, root.y + 0.18, 0.17))
        else:
            hoof = Vector((root.x * 1.18, root.y - travel, 0.14 + lift))
        bend = -0.16 if name.startswith("f") else 0.18
        elbow = (root + hoof) * 0.5 + Vector((root.x * 0.22, bend, 0.10))
        add_segment(f"DRAGON_UpperLeg_{name.upper()}", root, elbow,
                    0.17, 0.13, "skin", collection, spec,
                    f"upper_leg_{name}", sides=9)
        add_segment(f"DRAGON_LowerLeg_{name.upper()}", elbow, hoof,
                    0.13, 0.085, "secondary", collection, spec,
                    f"lower_leg_{name}", sides=8)
        add_box(f"DRAGON_Foot_{name.upper()}",
                hoof + Vector((0.0, -0.08, 0.0)), (0.32, 0.46, 0.14),
                "secondary", collection, spec, f"foot_{name}")
        for claw in (-0.09, 0.0, 0.09):
            add_cone(f"DRAGON_Claw_{name.upper()}",
                     hoof + Vector((claw, -0.31, 0.015)),
                     0.035, 0.18, "horn", collection, spec, f"claw_{name}",
                     rotation=(math.pi * 0.5, 0.0, 0.0), vertices=5)

    neck_points = [
        Vector((0.0, -0.78, body_z + 0.34)),
        Vector((0.0, -1.20, body_z + (0.78 if threat else 0.56))),
        Vector((0.0, -1.66, body_z + (1.05 if threat else 0.70))),
        Vector((0.0, -2.10, body_z + (1.00 if threat else 0.67))),
    ]
    radii = (0.35, 0.29, 0.23, 0.18)
    for index in range(len(neck_points) - 1):
        add_segment(f"DRAGON_Neck_{index}", neck_points[index],
                    neck_points[index + 1], radii[index], radii[index + 1],
                    "skin", collection, spec, f"neck_{index}", sides=10)
    head = neck_points[-1] + Vector((0.0, -0.20, 0.0))
    add_ellipsoid("DRAGON_Head", head, (0.39, 0.54, 0.30), "skin",
                  collection, spec, "head", subdivisions=2)
    jaw_drop = 0.12 if threat else 0.02
    add_box("DRAGON_Jaw", head + Vector((0.0, -0.38, -0.17 - jaw_drop)),
            (0.52, 0.52, 0.16), "secondary", collection, spec, "jaw",
            rotation=(0.10 if threat else 0.0, 0.0, 0.0), bevel=0.025)
    for side, sign in (("L", -1.0), ("R", 1.0)):
        add_ellipsoid(f"DRAGON_Eye_{side}",
                      head + Vector((0.28 * sign, -0.32, 0.10)),
                      (0.055, 0.028, 0.065), "eye", collection, spec,
                      f"eye_{side.lower()}")
        add_cone(f"DRAGON_Horn_{side}",
                 head + Vector((0.25 * sign, 0.05, 0.31)),
                 0.11, 0.62, "horn", collection, spec,
                 f"horn_{side.lower()}",
                 rotation=(0.25, math.pi * 0.30 * sign, 0.0), vertices=7)

    tail_points = [
        Vector((0.0, 1.08, body_z + 0.05)),
        Vector((0.0, 1.78, body_z - 0.10)),
        Vector((0.30 * cycle, 2.48, body_z - 0.34)),
        Vector((0.48 * cycle, 3.18, body_z - 0.52)),
        Vector((0.56 * cycle, 3.76, body_z - 0.50)),
    ]
    tail_radii = (0.36, 0.27, 0.18, 0.105, 0.025)
    for index in range(len(tail_points) - 1):
        add_segment(f"DRAGON_Tail_{index}", tail_points[index],
                    tail_points[index + 1], tail_radii[index],
                    tail_radii[index + 1], "skin", collection, spec,
                    f"tail_{index}", sides=9)

    wing_height = body_z + (2.35 if threat else 0.76)
    wing_reach = 2.70 if threat else 1.22
    wing_back = 1.42 if threat else 1.92
    for side, sign in (("L", -1.0), ("R", 1.0)):
        shoulder = (0.50 * sign, -0.47, body_z + 0.53)
        points = (
            shoulder,
            (1.48 * sign, -0.06, wing_height),
            (wing_reach * sign, wing_back, body_z + (0.72 if threat else 0.28)),
            (1.16 * sign, 1.20, body_z + 0.16),
            (0.63 * sign, 0.55, body_z + 0.30),
        )
        add_prism(f"DRAGON_Wing_{side}", points, 0.070, "secondary",
                  collection, spec, f"wing_{side.lower()}")
        add_segment(f"DRAGON_WingArm_{side}", shoulder, points[1],
                    0.13, 0.075, "skin", collection, spec,
                    f"wing_arm_{side.lower()}", sides=8)
        add_segment(f"DRAGON_WingFinger_{side}", points[1], points[2],
                    0.075, 0.028, "skin", collection, spec,
                    f"wing_finger_{side.lower()}", sides=7)

    spine_positions = (
        (0.0, -1.70, body_z + 0.96),
        (0.0, -1.18, body_z + 0.84),
        (0.0, -0.48, body_z + 0.78),
        (0.0, 0.18, body_z + 0.68),
        (0.0, 0.82, body_z + 0.51),
        (0.0, 1.50, body_z + 0.17),
        (0.0, 2.15, body_z - 0.09),
    )
    for index, position in enumerate(spine_positions):
        height = max(0.20, 0.42 - index * 0.030)
        add_cone(f"DRAGON_Spine_{index}", position, 0.12, height, "horn",
                 collection, spec, f"spine_{index}", vertices=6)


def build_creature(spec: CreatureSpec,
                   collection: bpy.types.Collection) -> None:
    if spec.family == "goblin":
        build_goblin(spec, collection)
    elif spec.family in ("horse", "cow"):
        build_quadruped(spec, collection)
    elif spec.family == "dragon":
        build_dragon(spec, collection)
    else:
        raise ValueError(f"unsupported creature family {spec.family}")


def duplicate_preview_parts(collection: bpy.types.Collection,
                            spec: CreatureSpec) -> list[bpy.types.Object]:
    result: list[bpy.types.Object] = []
    for source in tuple(collection.objects):
        if source.type != "MESH":
            continue
        duplicate = source.copy()
        duplicate.data = source.data.copy()
        duplicate.name = f"PREVIEW_SOURCE_{spec.variant}_{source.name}"
        bpy.context.scene.collection.objects.link(duplicate)
        duplicate.hide_render = True
        duplicate.hide_set(True)
        result.append(duplicate)
    return result


def quadruped_bone_points(
    spec: CreatureSpec,
) -> dict[str, tuple[Vector, Vector]]:
    cow = spec.family == "cow"
    body_z = 1.08 if cow else 1.24
    half_width = 0.38 if cow else 0.31
    points: dict[str, tuple[Vector, Vector]] = {
        "root": (Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, 0.20))),
        "body": (
            Vector((0.0, 0.42, body_z)),
            Vector((0.0, -0.30, body_z)),
        ),
        "chest": (
            Vector((0.0, -0.16, body_z + 0.06)),
            Vector((0.0, -0.72, body_z + 0.06)),
        ),
    }
    if cow:
        points["neck"] = (
            Vector((0.0, -0.50, body_z + 0.20)),
            Vector((0.0, -0.91, 1.16)),
        )
        points["head"] = (
            Vector((0.0, -1.15, 1.10)),
            Vector((0.0, -1.46, 1.04)),
        )
        tail_drop = 0.62
    else:
        points["neck"] = (
            Vector((0.0, -0.50, body_z + 0.20)),
            Vector((0.0, -0.87, 1.74)),
        )
        points["head"] = (
            Vector((0.0, -1.13, 1.74)),
            Vector((0.0, -1.47, 1.66)),
        )
        tail_drop = 0.48

    for name, x, longitudinal, front in (
        ("FL", -half_width, -0.57, True),
        ("FR", half_width, -0.57, True),
        ("HL", -half_width, 0.57, False),
        ("HR", half_width, 0.57, False),
    ):
        root = Vector((x, longitudinal,
                       body_z - (0.08 if front else 0.10)))
        hoof = Vector((x, longitudinal, 0.10))
        knee = (root + hoof) * 0.5
        knee += Vector((0.0, -0.10 if front else 0.10, 0.02))
        hoof_tail = Vector((x, longitudinal - 0.20, 0.085))
        points[f"upper_leg.{name}"] = (root, knee)
        points[f"lower_leg.{name}"] = (knee, hoof)
        points[f"hoof.{name}"] = (hoof, hoof_tail)

    tail_base = Vector((0.0, 0.82, body_z + 0.08))
    tail_mid = Vector((0.0, 1.10, body_z - 0.08))
    tail_end = Vector((0.0, 1.28, body_z - tail_drop))
    points["tail.root"] = (tail_base, tail_mid)
    points["tail"] = (tail_mid, tail_end)
    return points


def quadruped_bone_for_part(part: str) -> str:
    direct = {
        "barrel": "body",
        "udder": "body",
        "teat": "body",
        "hide_patch": "chest",
        "chest": "chest",
        "neck": "neck",
        "mane": "neck",
        "head": "head",
        "muzzle": "head",
        "blaze": "head",
        "forelock": "head",
        "ear_l": "head",
        "ear_r": "head",
        "eye_l": "head",
        "eye_r": "head",
        "nostril_l": "head",
        "nostril_r": "head",
        "horn_l": "head",
        "horn_r": "head",
        "tail_root": "tail.root",
        "tail": "tail",
    }
    if part in direct:
        return direct[part]
    for prefix, bone_prefix in (
        ("upper_leg_", "upper_leg."),
        ("lower_leg_", "lower_leg."),
        ("hoof_", "hoof."),
    ):
        if part.startswith(prefix):
            return bone_prefix + part.removeprefix(prefix).upper()
    raise RuntimeError(f"quadruped part {part!r} has no deform bone")


def apply_modifiers(obj: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for modifier in tuple(obj.modifiers):
        if modifier.type == "ARMATURE":
            continue
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def skin_quadruped(collection: bpy.types.Collection,
                    spec: CreatureSpec) -> bpy.types.Object:
    armature_data = bpy.data.armatures.new(f"RIG_{spec.variant}")
    rig = bpy.data.objects.new(f"RIG_{spec.variant}", armature_data)
    collection.objects.link(rig)
    rig.show_in_front = True
    rig["cc_asset_id"] = spec.asset_id
    rig["cc_rig_contract"] = "CcQuadrupedPose"
    rig["cc_bone_count"] = len(QUADRUPED_BONES)

    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    points = quadruped_bone_points(spec)
    edit_bones: dict[str, bpy.types.EditBone] = {}
    for name, _parent in QUADRUPED_BONES:
        bone = armature_data.edit_bones.new(name)
        bone.head, bone.tail = points[name]
        bone.use_deform = True
        edit_bones[name] = bone
    for name, parent in QUADRUPED_BONES:
        if parent is not None:
            edit_bones[name].parent = edit_bones[parent]
            edit_bones[name].use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.select_set(False)

    for obj in tuple(collection.objects):
        if obj.type != "MESH":
            continue
        apply_modifiers(obj)
        bone_name = quadruped_bone_for_part(str(obj["cc_part"]))
        obj.vertex_groups.clear()
        group = obj.vertex_groups.new(name=bone_name)
        group.add(tuple(range(len(obj.data.vertices))), 1.0, "REPLACE")
        modifier = obj.modifiers.new("CC_QuadrupedSkin", "ARMATURE")
        modifier.object = rig
        world = obj.matrix_world.copy()
        obj.parent = rig
        obj.matrix_parent_inverse = rig.matrix_world.inverted()
        obj.matrix_world = world
        obj["cc_deform_bone"] = bone_name
    return rig


def consolidate(collection: bpy.types.Collection,
                spec: CreatureSpec,
                rig: bpy.types.Object | None = None) -> bpy.types.Object:
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
    if not objects:
        raise RuntimeError(f"{spec.variant} generated no meshes")
    for obj in objects:
        apply_modifiers(obj)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    joined = objects[0]
    old_materials = list(joined.data.materials)
    old_names = [material.name if material else "" for material in old_materials]
    canonical: dict[str, int] = {}
    for family in FAMILY_PALETTES:
        for index, semantic in enumerate(MATERIAL_ORDER):
            canonical[PREVIEW_MATERIALS[(family, semantic)].name] = index
    polygon_materials = [canonical[old_names[polygon.material_index]]
                         for polygon in joined.data.polygons]
    paint_channels.add_indexed_paint_channels(
        joined, polygon_materials, MATERIAL_ORDER)
    joined.data.materials.clear()
    if INDEXED_MATERIAL is None:
        raise RuntimeError("indexed creature material was not initialized")
    joined.data.materials.append(INDEXED_MATERIAL)
    for polygon in joined.data.polygons:
        polygon.material_index = 0
    joined.name = f"GEO_{spec.asset_id}"
    joined.data.name = joined.name
    tag(joined, spec, "assembled_creature")
    joined["cc_material_contract"] = "COLOR_0:palette,value,fold"
    if rig is not None:
        joined.parent = rig
        armatures = [modifier for modifier in joined.modifiers
                     if modifier.type == "ARMATURE"]
        if not armatures:
            armature = joined.modifiers.new("CC_QuadrupedSkin", "ARMATURE")
            armature.object = rig
        else:
            armatures[0].object = rig
            for redundant in armatures[1:]:
                joined.modifiers.remove(redundant)
        joined["cc_skin_contract"] = "CcQuadrupedPose"
    return joined


def export_model(model: bpy.types.Object, spec: CreatureSpec,
                 rig: bpy.types.Object | None = None) -> Path:
    path = EXPORT_DIR / f"{spec.asset_id}.glb"
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    model.hide_set(False)
    model.select_set(True)
    if rig is not None:
        rig.hide_set(False)
        rig.select_set(True)
        bpy.context.view_layer.objects.active = rig
    else:
        bpy.context.view_layer.objects.active = model
    bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_animations=False,
        export_skins=rig is not None,
        export_morph=False,
        export_extras=True,
        export_materials="EXPORT",
    )
    model.select_set(False)
    if rig is not None:
        rig.select_set(False)
    return path


def look_at(obj: bpy.types.Object,
            target: tuple[float, float, float]) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_stage(collection: bpy.types.Collection) -> None:
    material = bpy.data.materials.new("MAT_CREATURE_STAGE")
    material.diffuse_color = (0.060, 0.085, 0.080, 1.0)
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = material.diffuse_color
    principled.inputs["Roughness"].default_value = 0.92
    bpy.ops.mesh.primitive_cube_add(location=(0.0, 0.45, -0.10))
    stage = bpy.context.object
    stage.name = "STAGE_CreatureFamilySheet"
    stage.dimensions = (13.2, 7.2, 0.16)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    stage.data.materials.append(material)
    move_to(stage, collection)


def add_preview_scene(
    preview_sources: list[tuple[CreatureSpec, list[bpy.types.Object]]],
) -> None:
    preview = new_collection("90_PREVIEW")
    placements = {
        "goblin_scavenger": (-4.65, -0.55, 1.0),
        "goblin_raider": (-3.55, -0.35, 1.0),
        "goblin_tribute_bearer": (-2.35, -0.15, 1.0),
        "horse": (-0.35, 0.20, 1.0),
        "cow": (1.85, 0.40, 1.0),
        "dragon": (4.35, 1.25, 0.82),
    }
    for spec, sources in preview_sources:
        x, y, scale = placements[spec.variant]
        for source in sources:
            source.hide_render = True
            source.hide_set(True)
            duplicate = source.copy()
            duplicate.data = source.data
            duplicate.name = f"PREVIEW_{spec.variant}_{source.name}"
            preview.objects.link(duplicate)
            duplicate.hide_render = False
            duplicate.hide_viewport = False
            duplicate.hide_set(False)
            duplicate.location = (
                x + source.location.x * scale,
                y + source.location.y * scale,
                source.location.z * scale,
            )
            duplicate.scale = (
                source.scale.x * scale,
                source.scale.y * scale,
                source.scale.z * scale,
            )
    add_stage(preview)

    bpy.ops.object.camera_add(location=(10.8, -18.0, 8.6))
    camera = bpy.context.object
    camera.name = "CAM_CreatureFamilySheet"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 12.0
    look_at(camera, (0.0, 0.55, 1.15))
    bpy.context.scene.camera = camera
    move_to(camera, preview)

    bpy.ops.object.light_add(type="AREA", location=(-4.5, -6.5, 9.0))
    key = bpy.context.object
    key.name = "LIGHT_Key"
    key.data.energy = 1350.0
    key.data.shape = "DISK"
    key.data.size = 6.0
    key.data.color = (1.0, 0.77, 0.59)
    look_at(key, (0.0, 0.5, 1.0))
    move_to(key, preview)

    bpy.ops.object.light_add(type="AREA", location=(6.0, -1.5, 6.5))
    fill = bpy.context.object
    fill.name = "LIGHT_Fill"
    fill.data.energy = 900.0
    fill.data.size = 7.0
    fill.data.color = (0.43, 0.75, 1.0)
    look_at(fill, (0.0, 0.8, 1.4))
    move_to(fill, preview)

    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.scene.render.filepath = str(PREVIEW_PATH)
    bpy.ops.render.render(write_still=True)


def build() -> None:
    reset_scene()
    make_materials()
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    preview_sources: list[tuple[CreatureSpec, list[bpy.types.Object]]] = []
    manifest_entries: list[dict[str, object]] = []
    for base_spec in CREATURES:
        for pose in poses_for(base_spec):
            spec = replace(base_spec, pose=pose)
            collection = new_collection(
                f"CREATURE_{spec.variant.upper()}_{pose.upper()}")
            collection["cc_asset_id"] = spec.asset_id
            collection["cc_family"] = spec.family
            collection["cc_variant"] = spec.variant
            collection["cc_variant_index"] = spec.variant_index
            collection["cc_pose"] = pose
            collection["cc_runtime_morphology"] = spec.runtime_morphology
            collection["cc_library_version"] = LIBRARY_VERSION
            build_creature(spec, collection)
            if pose == "idle":
                preview_sources.append(
                    (spec, duplicate_preview_parts(collection, spec)))
            rig = skin_quadruped(collection, spec) \
                if spec.family in ("horse", "cow") else None
            model = consolidate(collection, spec, rig)
            path = export_model(model, spec, rig)
            model.hide_render = True
            model.hide_set(True)
            entry: dict[str, object] = {
                **asdict(spec),
                "id": spec.asset_id,
                "export": str(path.relative_to(ROOT)),
                "material_order": list(MATERIAL_ORDER),
            }
            if rig is not None:
                rig.hide_render = True
                rig.hide_set(True)
                entry["skinned"] = True
                entry["armature"] = rig.name
                entry["bones"] = [name for name, _parent in QUADRUPED_BONES]
            manifest_entries.append(entry)

    referenced = {ROOT / str(entry["export"]) for entry in manifest_entries}
    for stale in EXPORT_DIR.glob("*.glb"):
        if stale not in referenced:
            stale.unlink()

    manifest = {
        "library_version": LIBRARY_VERSION,
        "art_direction": "silhouette_first_pseudo_pixel_creatures",
        "generation": "offline_curated_procedural_geometry",
        "runtime_strategy": "held poses for goblins and dragon; runtime skins for horse and cow",
        "coordinate_system": "glTF +Y up, +Z forward",
        "material_contract": "single indexed material; COLOR_0 stores palette, value, and fold",
        "material_order": list(MATERIAL_ORDER),
        "families": ["goblin", "dragon", "animal"],
        "variants": [spec.variant for spec in CREATURES],
        "archetypes": manifest_entries,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8")
    from generate_creature_catalog import generate
    generate(MANIFEST_PATH)
    add_preview_scene(preview_sources)
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    print(f"built {len(CREATURES)} creature variants / "
          f"{len(manifest_entries)} pose assets")
    print(f"manifest: {MANIFEST_PATH}")
    print(f"preview: {PREVIEW_PATH}")


if __name__ == "__main__":
    build()
