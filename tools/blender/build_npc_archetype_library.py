#!/usr/bin/env python3
"""Build the silhouette-first procedural NPC archetype library.

Run from the repository root:
    blender --background --factory-startup --python \
        tools/blender/build_npc_archetype_library.py

The library deliberately bakes curated procedural geometry to static GLBs.
Runtime code supplies deterministic color, scale, and role selection without
paying an animated skin update for every background person.
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
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_npc_archetypes.blend"
EXPORT_DIR = ROOT / "assets" / "exports" / "npc"
PREVIEW_PATH = ROOT / "assets" / "previews" / "npc" / "npc_archetype_sheet.png"
MANIFEST_PATH = ROOT / "assets" / "npc_archetype_manifest.json"
LIBRARY_VERSION = "0.5.0"
MOTION_POSES = (
    "idle",
    "contact_l", "down_l", "passing_l", "up_l",
    "contact_r", "down_r", "passing_r", "up_r",
)

MATERIAL_ORDER = (
    "skin",
    "hair",
    "underlayer",
    "outer",
    "trousers",
    "leather",
    "metal",
    "accent",
    "eye",
)

PALETTE = {
    "skin": (0.62, 0.36, 0.22, 1.0),
    "hair": (0.075, 0.052, 0.040, 1.0),
    "underlayer": (0.46, 0.43, 0.34, 1.0),
    "outer": (0.10, 0.31, 0.30, 1.0),
    "trousers": (0.095, 0.12, 0.12, 1.0),
    "leather": (0.20, 0.105, 0.055, 1.0),
    "metal": (0.29, 0.34, 0.34, 1.0),
    "accent": (0.58, 0.18, 0.20, 1.0),
    "eye": (0.012, 0.014, 0.015, 1.0),
}

MATERIALS: dict[str, bpy.types.Material] = {}
INDEXED_MATERIAL: bpy.types.Material | None = None


@dataclass(frozen=True)
class Archetype:
    role: str
    role_index: int
    mass: float
    shoulder: float
    hip: float
    head_width: float
    head_height: float
    hand: float
    boot: float
    posture: str
    hair: str
    garment: str
    equipment: tuple[str, ...]
    motion_pose: str = "idle"

    @property
    def asset_id(self) -> str:
        motion = "" if self.motion_pose == "idle" else f"_{self.motion_pose}"
        return f"npc_{self.role}{motion}_v01"


ARCHETYPES = (
    Archetype("wayfarer", 0, 1.06, 1.14, 0.98, 1.08, 1.06, 1.15, 1.13,
              "ready", "swept", "split_tunic", ("mantle", "satchel")),
    Archetype("guard", 1, 1.18, 1.22, 1.06, 1.04, 1.02, 1.18, 1.18,
              "upright", "cropped", "armored_coat",
              ("armor", "helmet", "weapon")),
    Archetype("raider", 2, 1.13, 1.20, 1.02, 1.06, 1.05, 1.20, 1.17,
              "forward", "crest", "rough_mantle",
              ("mantle", "half_armor", "weapon")),
    Archetype("merchant", 3, 1.22, 1.08, 1.14, 1.17, 1.10, 1.15, 1.10,
              "open", "bob", "apron", ("hat", "apron", "satchel")),
    Archetype("laborer", 4, 1.23, 1.19, 1.12, 1.08, 1.05, 1.22, 1.21,
              "heavy", "cropped", "work_apron", ("apron", "tool")),
    Archetype("traveller", 5, 1.07, 1.09, 1.01, 1.09, 1.08, 1.14, 1.16,
              "walking", "wrapped", "road_coat", ("mantle", "pack", "hat")),
    Archetype("refugee", 6, 0.95, 0.98, 0.94, 1.12, 1.10, 1.05, 1.06,
              "inward", "hooded", "wrapped", ("hood", "mantle", "pack")),
    Archetype("scout", 7, 0.98, 1.04, 0.94, 1.04, 1.03, 1.12, 1.15,
              "forward", "short", "short_coat", ("short_mantle", "satchel", "weapon")),
    Archetype("healer", 8, 1.04, 1.06, 1.02, 1.12, 1.10, 1.12, 1.09,
              "measured", "braided", "layered_apron",
              ("mantle", "apron", "satchel", "healer_mark")),
)


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_NPC_ARCHETYPE_LIBRARY"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1080
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene["cc_library_version"] = LIBRARY_VERSION
    scene["cc_art_direction"] = "silhouette_first_pseudo_pixel_cast"
    scene["cc_forward_axis"] = "-Y"
    scene["cc_up_axis"] = "+Z"
    world = bpy.data.worlds.new("CC_NpcWorld")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.015, 0.020, 0.021, 1.0)
    background.inputs["Strength"].default_value = 0.36
    scene.world = world


def make_materials() -> None:
    global INDEXED_MATERIAL
    for name in MATERIAL_ORDER:
        material = bpy.data.materials.new(f"MAT_NPC_{name.upper()}")
        material.diffuse_color = PALETTE[name]
        material.use_nodes = True
        material.use_backface_culling = False
        principled = material.node_tree.nodes.get("Principled BSDF")
        principled.inputs["Base Color"].default_value = PALETTE[name]
        principled.inputs["Roughness"].default_value = 0.78
        if name == "metal":
            principled.inputs["Metallic"].default_value = 0.48
            principled.inputs["Roughness"].default_value = 0.46
        MATERIALS[name] = material
    INDEXED_MATERIAL = bpy.data.materials.new("MAT_NPC_INDEXED")
    INDEXED_MATERIAL.diffuse_color = (1.0, 1.0, 1.0, 1.0)
    INDEXED_MATERIAL.use_nodes = True
    nodes = INDEXED_MATERIAL.node_tree.nodes
    principled = nodes.get("Principled BSDF")
    vertex_color = nodes.new("ShaderNodeVertexColor")
    vertex_color.layer_name = "COLOR_0"
    INDEXED_MATERIAL.node_tree.links.new(
        vertex_color.outputs["Color"], principled.inputs["Base Color"])
    principled.inputs["Roughness"].default_value = 0.78


def new_collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def move_to(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)


def tag(obj: bpy.types.Object, spec: Archetype, part: str) -> None:
    obj["cc_asset_id"] = spec.asset_id
    obj["cc_role"] = spec.role
    obj["cc_part"] = part
    obj["cc_library_version"] = LIBRARY_VERSION


def assign(obj: bpy.types.Object, material: str) -> None:
    obj.data.materials.append(MATERIALS[material])


def add_bevel(obj: bpy.types.Object, width: float, segments: int = 1) -> None:
    bevel = obj.modifiers.new("CC_SilhouetteBevel", "BEVEL")
    bevel.width = width
    bevel.segments = segments
    bevel.limit_method = "ANGLE"


def add_box(name: str, center: tuple[float, float, float],
            dimensions: tuple[float, float, float], material: str,
            collection: bpy.types.Collection, spec: Archetype, part: str,
            *, rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
            bevel: float = 0.012) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=center, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0.0:
        add_bevel(obj, min(bevel, min(dimensions) * 0.22))
    assign(obj, material)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_ellipsoid(name: str, center: tuple[float, float, float],
                  scale: tuple[float, float, float], material: str,
                  collection: bpy.types.Collection, spec: Archetype,
                  part: str, *, subdivisions: int = 2) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions,
                                          radius=1.0, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign(obj, material)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_cylinder(name: str, center: tuple[float, float, float], radius: float,
                 depth: float, material: str,
                 collection: bpy.types.Collection, spec: Archetype, part: str,
                 *, vertices: int = 10,
                 rotation: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=depth, location=center,
                                        rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    assign(obj, material)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_segment(name: str, start: tuple[float, float, float],
                end: tuple[float, float, float], start_radius: float,
                end_radius: float, material: str,
                collection: bpy.types.Collection, spec: Archetype, part: str,
                *, sides: int = 8) -> bpy.types.Object:
    a = Vector(start)
    b = Vector(end)
    axis = b - a
    if axis.length < 0.0001:
        raise ValueError(f"degenerate segment {name}")
    direction = axis.normalized()
    reference = Vector((0.0, 0.0, 1.0))
    if abs(direction.dot(reference)) > 0.92:
        reference = Vector((0.0, 1.0, 0.0))
    right = direction.cross(reference).normalized()
    forward = direction.cross(right).normalized()
    vertices: list[tuple[float, float, float]] = []
    for center, radius in ((a, start_radius), (b, end_radius)):
        for index in range(sides):
            angle = math.tau * index / sides
            point = center + right * math.cos(angle) * radius + \
                    forward * math.sin(angle) * radius
            vertices.append(tuple(point))
    faces: list[tuple[int, ...]] = []
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple(range(sides, sides * 2)))
    for index in range(sides):
        nxt = (index + 1) % sides
        faces.append((index, nxt, sides + nxt, sides + index))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, material)
    tag(obj, spec, part)
    return obj


def add_loft(name: str,
             rings: tuple[tuple[float, float, float, float, float], ...],
             material: str, collection: bpy.types.Collection,
             spec: Archetype, part: str, *, sides: int = 10) -> bpy.types.Object:
    vertices: list[tuple[float, float, float]] = []
    for x, y, z, radius_x, radius_y in rings:
        for index in range(sides):
            angle = math.tau * index / sides
            vertices.append((x + math.cos(angle) * radius_x,
                             y + math.sin(angle) * radius_y, z))
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
    assign(obj, material)
    tag(obj, spec, part)
    return obj


def add_cloak(name: str, width_left: float, width_right: float,
              top_z: float, bottom_z: float, material: str,
              collection: bpy.types.Collection, spec: Archetype,
              *, short: bool = False) -> bpy.types.Object:
    back_y = 0.135
    hem_y = 0.205 if short else 0.245
    center_z = bottom_z + (0.08 if short else 0.0)
    vertices = [
        (-width_left, back_y, top_z),
        (width_right, back_y, top_z - 0.015),
        (width_right * 0.88, hem_y, center_z + 0.06),
        (0.06, hem_y + 0.012, bottom_z),
        (-width_left * 0.92, hem_y, center_z + 0.02),
    ]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], [(0, 1, 2, 3, 4)])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, material)
    tag(obj, spec, "mantle")
    solidify = obj.modifiers.new("CC_ClothThickness", "SOLIDIFY")
    solidify.thickness = 0.018
    bevel = obj.modifiers.new("CC_ClothEdge", "BEVEL")
    bevel.width = 0.008
    bevel.segments = 1
    return obj


def pose_points(spec: Archetype) -> dict[str, Vector]:
    hip_x = 0.125 * spec.hip
    shoulder_x = 0.285 * spec.shoulder
    points = {
        "hip_l": Vector((-hip_x, 0.0, 1.00)),
        "hip_r": Vector((hip_x, 0.0, 1.00)),
        "knee_l": Vector((-hip_x - 0.015, -0.012, 0.56)),
        "knee_r": Vector((hip_x + 0.015, 0.012, 0.56)),
        "ankle_l": Vector((-hip_x - 0.006, -0.015, 0.14)),
        "ankle_r": Vector((hip_x + 0.006, 0.015, 0.14)),
        "shoulder_l": Vector((-shoulder_x, 0.0, 1.57)),
        "shoulder_r": Vector((shoulder_x, 0.0, 1.57)),
        "elbow_l": Vector((-0.43 * spec.shoulder, -0.020, 1.24)),
        "elbow_r": Vector((0.43 * spec.shoulder, 0.015, 1.24)),
        "hand_l": Vector((-0.49 * spec.shoulder, -0.055, 0.92)),
        "hand_r": Vector((0.49 * spec.shoulder, -0.035, 0.94)),
    }
    if spec.posture == "open":
        points["elbow_l"] = Vector((-0.46, -0.08, 1.31))
        points["hand_l"] = Vector((-0.56, -0.13, 1.20))
        points["elbow_r"] = Vector((0.43, 0.00, 1.20))
    elif spec.posture == "inward":
        points["elbow_l"] = Vector((-0.28, -0.08, 1.18))
        points["elbow_r"] = Vector((0.28, -0.08, 1.18))
        points["hand_l"] = Vector((-0.16, -0.14, 0.91))
        points["hand_r"] = Vector((0.16, -0.14, 0.91))
    elif spec.posture in {"forward", "walking"}:
        points["knee_l"].y -= 0.055
        points["ankle_l"].y -= 0.10
        points["knee_r"].y += 0.035
        points["ankle_r"].y += 0.08
        points["elbow_l"].y += 0.055
        points["hand_l"].y += 0.105
        points["elbow_r"].y -= 0.055
        points["hand_r"].y -= 0.105
    elif spec.posture == "heavy":
        points["elbow_l"].x -= 0.035
        points["elbow_r"].x += 0.035
        points["hand_l"].x -= 0.04
        points["hand_r"].x += 0.04
    elif spec.posture == "measured":
        points["hand_l"] = Vector((-0.35, -0.10, 1.02))
        points["hand_r"] = Vector((0.35, -0.10, 1.02))
    if spec.motion_pose != "idle":
        pose_index = MOTION_POSES.index(spec.motion_pose) - 1
        angle = math.tau * pose_index / 8.0
        stride = math.cos(angle)
        points["knee_l"].y -= 0.060 * stride
        points["ankle_l"].y -= 0.150 * stride
        points["knee_r"].y += 0.060 * stride
        points["ankle_r"].y += 0.150 * stride
        points["elbow_l"].y += 0.055 * stride
        points["hand_l"].y += 0.125 * stride
        points["elbow_r"].y -= 0.055 * stride
        points["hand_r"].y -= 0.125 * stride

        # Contact, down, passing, and up are deliberately discrete rather
        # than evenly sampled smooth animation.  The opposite leg swings
        # through the first half-cycle, then the pattern mirrors.
        lift_l = (0.0, 0.0, 0.0, 0.0, 0.0, 0.025, 0.135, 0.075)[pose_index]
        lift_r = (0.0, 0.025, 0.135, 0.075, 0.0, 0.0, 0.0, 0.0)[pose_index]
        points["ankle_l"].z += lift_l
        points["knee_l"].z += lift_l * 0.62
        points["ankle_r"].z += lift_r
        points["knee_r"].z += lift_r * 0.62
        if spec.motion_pose.startswith("down_"):
            points["knee_l"].z -= 0.032
            points["knee_r"].z -= 0.032
        elif spec.motion_pose.startswith("up_"):
            points["knee_l"].z += 0.024
            points["knee_r"].z += 0.024
    return points


def build_head(spec: Archetype, collection: bpy.types.Collection) -> None:
    center = Vector((0.0, -0.010, 1.84))
    width = 0.153 * spec.head_width
    depth = 0.128 * spec.head_width
    height = 0.178 * spec.head_height
    add_segment(f"GEO_{spec.role}_neck", (0.0, 0.0, 1.57),
                (0.0, -0.004, 1.70), 0.077 * spec.mass, 0.070,
                "skin", collection, spec, "neck")
    add_ellipsoid(f"GEO_{spec.role}_head", tuple(center),
                  (width, depth, height), "skin", collection, spec, "head")
    # A narrower, slightly forward jaw breaks the toy-ball head silhouette
    # without baking facial identity into the role mesh.  Eyes, scars, age,
    # and expression remain exclusively owned by the shared face recipe.
    add_ellipsoid(f"GEO_{spec.role}_jaw",
                  (0.0, -0.034, center.z - height * 0.52),
                  (width * 0.80, depth * 0.90, height * 0.48), "skin",
                  collection, spec, "jaw", subdivisions=1)
    # Eyes, brows, nose, mouth, beard, age marks, and scars are drawn from the
    # shared runtime face recipe so the UI portrait cannot drift from the
    # gameplay head.
    for side in (-1.0, 1.0):
        add_ellipsoid(f"GEO_{spec.role}_ear_{side:+.0f}",
                      (side * width * 0.99, -0.004, center.z - 0.005),
                      (0.022, 0.018, 0.036), "skin", collection, spec, "ear",
                      subdivisions=1)

    # Runtime identity modules own the eight hair silhouettes and four
    # headwear families.  The baked role body keeps only a close scalp cap so
    # the same deterministic recipe can be used for world heads and portraits.
    add_ellipsoid(f"GEO_{spec.role}_runtime_scalp",
                  (0.0, 0.016, center.z + height * 0.48),
                  (width * 0.98, depth * 0.98, height * 0.34), "hair",
                  collection, spec, "runtime_identity_base", subdivisions=1)


def build_body(spec: Archetype, collection: bpy.types.Collection) -> None:
    points = pose_points(spec)
    mass = spec.mass
    shoulder = spec.shoulder
    hip = spec.hip
    torso_y = 0.014 if spec.posture in {"forward", "walking"} else 0.0
    add_loft(f"GEO_{spec.role}_torso", (
        (0.0, torso_y, 0.98, 0.205 * hip, 0.145 * mass),
        (0.0, torso_y, 1.20, 0.232 * mass, 0.155 * mass),
        (0.0, torso_y, 1.43, 0.278 * shoulder, 0.163 * mass),
        (0.0, torso_y, 1.58, 0.292 * shoulder, 0.155 * mass),
    ), "outer", collection, spec, "torso")
    add_loft(f"GEO_{spec.role}_pelvis", (
        (0.0, 0.0, 0.83, 0.185 * hip, 0.142 * mass),
        (0.0, 0.0, 1.03, 0.218 * hip, 0.155 * mass),
    ), "trousers", collection, spec, "pelvis")
    add_box(f"GEO_{spec.role}_belt", (0.0, -0.008, 1.025),
            (0.410 * hip, 0.285 * mass, 0.060), "leather",
            collection, spec, "belt", bevel=0.012)
    add_box(f"GEO_{spec.role}_buckle", (0.0, -0.158 * mass, 1.025),
            (0.065, 0.025, 0.060), "metal", collection, spec, "buckle",
            bevel=0.007)
    add_box(f"GEO_{spec.role}_collar", (0.0, -0.126 * mass, 1.535),
            (0.245 * shoulder, 0.030, 0.070), "accent", collection, spec,
            "collar", bevel=0.009)

    leg_radius = 0.116 * mass
    for side in ("l", "r"):
        hip_point = points[f"hip_{side}"]
        knee = points[f"knee_{side}"]
        ankle = points[f"ankle_{side}"]
        sign = -1.0 if side == "l" else 1.0
        add_segment(f"GEO_{spec.role}_thigh_{side}", tuple(hip_point), tuple(knee),
                    leg_radius, leg_radius * 0.82, "trousers", collection,
                    spec, "leg")
        add_segment(f"GEO_{spec.role}_shin_{side}", tuple(knee), tuple(ankle),
                    leg_radius * 0.82, leg_radius * 0.64, "trousers",
                    collection, spec, "leg")
        boot_center = Vector((ankle.x, ankle.y - 0.060, ankle.z - 0.025))
        add_box(f"GEO_{spec.role}_boot_{side}", tuple(boot_center),
                (0.210 * spec.boot, 0.340 * spec.boot, 0.240), "leather",
                collection, spec, "boot", rotation=(0.035 * sign, 0.0, 0.0),
                bevel=0.025)

    arm_radius = 0.100 * mass
    for side in ("l", "r"):
        shoulder_point = points[f"shoulder_{side}"]
        elbow = points[f"elbow_{side}"]
        hand = points[f"hand_{side}"]
        add_segment(f"GEO_{spec.role}_upper_arm_{side}", tuple(shoulder_point),
                    tuple(elbow), arm_radius * 1.06, arm_radius * 0.82,
                    "outer", collection, spec, "arm")
        add_segment(f"GEO_{spec.role}_forearm_{side}", tuple(elbow), tuple(hand),
                    arm_radius * 0.80, arm_radius * 0.60,
                    "underlayer", collection, spec, "arm")
        add_ellipsoid(f"GEO_{spec.role}_hand_{side}", tuple(hand),
                      (0.082 * spec.hand, 0.070 * spec.hand, 0.094 * spec.hand),
                      "skin", collection, spec, "hand", subdivisions=1)

    if spec.garment in {"split_tunic", "road_coat", "short_coat"}:
        length = 0.31 if spec.garment == "short_coat" else 0.42
        add_box(f"GEO_{spec.role}_coat_left", (-0.105, -0.005, 0.91),
                (0.190 * mass, 0.270 * mass, length), "outer", collection,
                spec, "coat_skirt", rotation=(0.0, 0.04, 0.025), bevel=0.018)
        add_box(f"GEO_{spec.role}_coat_right", (0.105, 0.002, 0.91),
                (0.190 * mass, 0.270 * mass, length * 0.92), "outer",
                collection, spec, "coat_skirt", rotation=(0.0, -0.035, -0.02),
                bevel=0.018)

    if "armor" in spec.equipment or "half_armor" in spec.equipment:
        # Armor must read as clothing around a body from every camera angle.
        # A thin front box becomes a floating signboard at the final pixel
        # scale, so guards get a closed cuirass and raiders get a smaller,
        # off-centre half cuirass. Both follow the same torso rings as the
        # garment below them and keep enough depth for a clear side view.
        full_armor = "armor" in spec.equipment
        plate_x = 0.0 if full_armor else -0.08
        width_scale = 1.0 if full_armor else 0.73
        add_loft(f"GEO_{spec.role}_chest_plate", (
            (plate_x, -0.010, 1.13,
             0.205 * mass * width_scale, 0.166 * mass),
            (plate_x, -0.012, 1.25,
             0.232 * mass * width_scale, 0.181 * mass),
            (plate_x, -0.014, 1.43,
             0.274 * shoulder * width_scale, 0.188 * mass),
            (plate_x, -0.012, 1.56,
             0.252 * shoulder * width_scale, 0.170 * mass),
        ), "metal", collection, spec, "armor", sides=10)
        add_box(f"GEO_{spec.role}_chest_ridge",
                (plate_x, -0.192 * mass, 1.365),
                (0.055, 0.040, 0.285), "metal", collection, spec, "armor",
                bevel=0.009)
        pauldron_side = ("l", "r") if "armor" in spec.equipment else ("l",)
        for side in pauldron_side:
            shoulder_point = points[f"shoulder_{side}"]
            add_ellipsoid(f"GEO_{spec.role}_pauldron_{side}", tuple(shoulder_point),
                          (0.125, 0.112, 0.105), "metal", collection, spec,
                          "armor", subdivisions=1)

    if "apron" in spec.equipment:
        add_box(f"GEO_{spec.role}_apron", (0.0, -0.158 * mass, 1.06),
                (0.34 * mass, 0.030, 0.70), "underlayer", collection, spec,
                "apron", bevel=0.022)
        add_box(f"GEO_{spec.role}_apron_hem", (0.0, -0.177 * mass, 0.75),
                (0.35 * mass, 0.034, 0.065), "accent", collection, spec,
                "apron", bevel=0.008)

    mantle = "mantle" in spec.equipment or "short_mantle" in spec.equipment
    if mantle:
        short = "short_mantle" in spec.equipment
        add_cloak(f"GEO_{spec.role}_mantle", 0.35 * shoulder,
                  0.23 * shoulder, 1.61, 1.14 if short else 0.66,
                  "outer", collection, spec, short=short)
        add_box(f"GEO_{spec.role}_mantle_clasp", (-0.13, -0.153, 1.545),
                (0.075, 0.035, 0.075), "accent", collection, spec, "clasp",
                bevel=0.012)

    if "satchel" in spec.equipment:
        add_box(f"GEO_{spec.role}_satchel", (0.31 * mass, 0.075, 0.91),
                (0.25, 0.14, 0.28), "leather", collection, spec, "satchel",
                rotation=(0.0, 0.10, -0.06), bevel=0.028)
        add_segment(f"GEO_{spec.role}_satchel_strap", (-0.20, -0.145, 1.52),
                    (0.30, -0.125, 0.96), 0.018, 0.018, "leather",
                    collection, spec, "satchel", sides=6)
    if "pack" in spec.equipment:
        add_box(f"GEO_{spec.role}_pack", (0.0, 0.180, 1.24),
                (0.40 * mass, 0.22, 0.54), "leather", collection, spec, "pack",
                bevel=0.045)
        add_box(f"GEO_{spec.role}_bedroll", (0.0, 0.225, 1.55),
                (0.46 * mass, 0.17, 0.15), "accent", collection, spec,
                "pack", bevel=0.040)

    if "weapon" in spec.equipment or "tool" in spec.equipment:
        side = 1.0 if spec.role not in {"raider", "scout"} else -1.0
        x = side * (0.48 * shoulder + 0.10)
        add_segment(f"GEO_{spec.role}_tool_shaft", (x, 0.035, 0.22),
                    (x, -0.015, 1.38), 0.027, 0.022, "leather", collection,
                    spec, "tool", sides=7)
        if spec.role == "laborer":
            add_box(f"GEO_{spec.role}_tool_head", (x, -0.020, 1.40),
                    (0.34, 0.075, 0.11), "metal", collection, spec, "tool",
                    rotation=(0.0, 0.18, 0.0), bevel=0.014)
        else:
            add_box(f"GEO_{spec.role}_tool_head", (x, -0.020, 1.43),
                    (0.095, 0.075, 0.22), "metal", collection, spec, "tool",
                    rotation=(0.0, 0.12 * side, 0.0), bevel=0.014)

    if "healer_mark" in spec.equipment:
        add_box(f"GEO_{spec.role}_mark_vertical", (0.0, -0.178, 1.29),
                (0.055, 0.018, 0.19), "accent", collection, spec,
                "healer_mark", bevel=0.006)
        add_box(f"GEO_{spec.role}_mark_horizontal", (0.0, -0.180, 1.29),
                (0.17, 0.018, 0.055), "accent", collection, spec,
                "healer_mark", bevel=0.006)


def apply_modifiers(obj: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for modifier in tuple(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def consolidate(collection: bpy.types.Collection,
                spec: Archetype) -> bpy.types.Object:
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
    if not objects:
        raise RuntimeError(f"{spec.role} generated no meshes")
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
    canonical = {MATERIALS[name].name: index
                 for index, name in enumerate(MATERIAL_ORDER)}
    polygon_materials = [canonical[old_names[polygon.material_index]]
                         for polygon in joined.data.polygons]
    paint_channels.add_indexed_paint_channels(
        joined, polygon_materials, MATERIAL_ORDER)
    joined.data.materials.clear()
    if INDEXED_MATERIAL is None:
        raise RuntimeError("indexed NPC material was not initialized")
    joined.data.materials.append(INDEXED_MATERIAL)
    for polygon in joined.data.polygons:
        polygon.material_index = 0
    joined.name = f"GEO_{spec.asset_id}"
    joined.data.name = joined.name
    tag(joined, spec, "assembled_archetype")
    joined["cc_material_contract"] = "COLOR_0:palette,value,fold"
    return joined


def export_model(model: bpy.types.Object, spec: Archetype) -> Path:
    path = EXPORT_DIR / f"{spec.asset_id}.glb"
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    model.hide_set(False)
    model.select_set(True)
    bpy.context.view_layer.objects.active = model
    bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_animations=False,
        export_skins=False,
        export_morph=False,
        export_extras=True,
        export_materials="EXPORT",
    )
    model.select_set(False)
    return path


def look_at(obj: bpy.types.Object, target: tuple[float, float, float]) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_preview_scene(models: list[tuple[Archetype, bpy.types.Object]]) -> None:
    preview = new_collection("90_PREVIEW")
    for index, (spec, source) in enumerate(models):
        source.hide_render = True
        source.hide_set(True)
        duplicate = source.copy()
        duplicate.data = source.data
        duplicate.name = f"PREVIEW_{spec.role}"
        preview.objects.link(duplicate)
        duplicate.hide_render = False
        duplicate.hide_viewport = False
        duplicate.hide_set(False)
        column = index % 3
        row = index // 3
        duplicate.location = ((column - 1) * 2.20,
                              (row - 1) * 1.72, 0.0)
        tile_color = (0.075 + row * 0.012, 0.095 + column * 0.010,
                      0.090, 1.0)
        tile_material = bpy.data.materials.new(f"MAT_STAGE_{index}")
        tile_material.diffuse_color = tile_color
        tile_material.use_nodes = True
        tile_material.node_tree.nodes["Principled BSDF"].inputs[
            "Base Color"].default_value = tile_color
        bpy.ops.mesh.primitive_cube_add(
            location=(duplicate.location.x, duplicate.location.y, -0.045),
            scale=(0.96, 0.70, 0.04))
        tile = bpy.context.object
        tile.name = f"STAGE_{spec.role}"
        tile.data.materials.append(tile_material)
        move_to(tile, preview)

    bpy.ops.object.camera_add(location=(6.6, -11.4, 6.2))
    camera = bpy.context.object
    camera.name = "CAM_NPC_ArchetypeSheet"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 8.2
    look_at(camera, (0.0, 0.0, 1.0))
    bpy.context.scene.camera = camera
    move_to(camera, preview)

    bpy.ops.object.light_add(type="AREA", location=(-4.5, -5.0, 8.0))
    key = bpy.context.object
    key.name = "LIGHT_Key"
    key.data.energy = 1050.0
    key.data.shape = "DISK"
    key.data.size = 5.5
    key.data.color = (1.0, 0.78, 0.62)
    look_at(key, (0.0, 0.0, 1.0))
    move_to(key, preview)

    bpy.ops.object.light_add(type="AREA", location=(5.0, -1.0, 5.5))
    fill = bpy.context.object
    fill.name = "LIGHT_Fill"
    fill.data.energy = 760.0
    fill.data.size = 6.0
    fill.data.color = (0.48, 0.78, 1.0)
    look_at(fill, (0.0, 0.0, 1.2))
    move_to(fill, preview)

    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.scene.render.filepath = str(PREVIEW_PATH)
    bpy.ops.render.render(write_still=True)


def build() -> None:
    reset_scene()
    make_materials()
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    models: list[tuple[Archetype, bpy.types.Object]] = []
    manifest_entries: list[dict[str, object]] = []
    for base_spec in ARCHETYPES:
        for motion_pose in MOTION_POSES:
            spec = replace(base_spec, motion_pose=motion_pose)
            collection = new_collection(
                f"NPC_{spec.role.upper()}_{motion_pose.upper()}")
            collection["cc_asset_id"] = spec.asset_id
            collection["cc_role"] = spec.role
            collection["cc_role_index"] = spec.role_index
            collection["cc_motion_pose"] = motion_pose
            collection["cc_library_version"] = LIBRARY_VERSION
            build_body(spec, collection)
            build_head(spec, collection)
            model = consolidate(collection, spec)
            path = export_model(model, spec)
            if motion_pose == "idle":
                models.append((spec, model))
            else:
                model.hide_render = True
                model.hide_set(True)
            manifest_entries.append({
                **asdict(spec),
                "id": spec.asset_id,
                "export": str(path.relative_to(ROOT)),
                "material_order": list(MATERIAL_ORDER),
                "armor_geometry": "closed torso volume"
                if "armor" in spec.equipment or
                   "half_armor" in spec.equipment else "none",
            })

    manifest = {
        "library_version": LIBRARY_VERSION,
        "art_direction": "silhouette_first_pseudo_pixel_cast",
        "generation": "offline_curated_procedural_geometry",
        "runtime_strategy": "stepped static pose GLBs + deterministic indexed palette and scale",
        "coordinate_system": "glTF +Y up, +Z forward",
        "material_contract": "single indexed material; COLOR_0 stores palette, value, and fold",
        "material_order": list(MATERIAL_ORDER),
        "archetypes": manifest_entries,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8")
    add_preview_scene(models)
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    print(f"built {len(ARCHETYPES)} NPC archetypes in "
          f"{len(MOTION_POSES)} stepped poses")
    print(f"manifest: {MANIFEST_PATH}")
    print(f"preview: {PREVIEW_PATH}")


if __name__ == "__main__":
    build()
