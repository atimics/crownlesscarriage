#!/usr/bin/env python3
"""Build the Crownless Carriage modular hero prototype in Blender.

Run from the repository root:
    blender --background --factory-startup \
        --python tools/blender/build_hero_component_library.py

The generated blockout follows the concept image while keeping every garment,
cloth panel, armor piece, and accessory independently toggleable and exportable.
"""

from __future__ import annotations

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

from procedural_character import (
    CUIRASS_SHELL,
    FITTED_BRACER,
    FITTED_GREAVE,
    FITTED_TUNIC,
    PADDED_UNDERLAYER,
    WAYFARER_RECIPE,
    body_profiles,
    clip_profile,
    derive_shell,
    loft_rows,
    sweep_rows,
)


ROOT = Path(__file__).resolve().parents[2]
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_hero_components.blend"
EXPORT_DIR = ROOT / "assets" / "exports" / "hero_glb"
PREVIEW_DIR = ROOT / "assets" / "previews" / "hero"
MANIFEST_PATH = ROOT / "assets" / "hero_component_manifest.json"
LIBRARY_VERSION = "0.11.0"
ART_DIRECTION = "silhouette_first_pixel_wayfarer"

HERO_BODY_PROFILES = body_profiles(WAYFARER_RECIPE.body)

MATERIALS: dict[str, bpy.types.Material] = {}
COMPONENTS: dict[str, bpy.types.Collection] = {}
COMPONENT_RECORDS: list[dict[str, object]] = []
SOCKET_RECORDS: list[dict[str, object]] = []


PALETTE = {
    "skin": (0.48, 0.235, 0.125, 1.0),
    "skin_light": (0.69, 0.385, 0.215, 1.0),
    "hair": (0.040, 0.014, 0.010, 1.0),
    "body_neutral": (0.16, 0.19, 0.19, 1.0),
    "muscle_blue": (0.08, 0.45, 0.65, 1.0),
    "muscle_green": (0.12, 0.55, 0.31, 1.0),
    "muscle_purple": (0.43, 0.20, 0.62, 1.0),
    "muscle_gold": (0.74, 0.49, 0.10, 1.0),
    "rig_cyan": (0.08, 0.82, 1.0, 1.0),
    "teal": (0.015, 0.105, 0.112, 1.0),
    "teal_light": (0.030, 0.190, 0.198, 1.0),
    "teal_dark": (0.008, 0.050, 0.056, 1.0),
    "padding": (0.34, 0.255, 0.155, 1.0),
    "padding_dark": (0.150, 0.100, 0.055, 1.0),
    "cape": (0.125, 0.018, 0.028, 1.0),
    "cape_light": (0.295, 0.043, 0.055, 1.0),
    "brigandine": (0.095, 0.015, 0.020, 1.0),
    "brigandine_edge": (0.205, 0.032, 0.038, 1.0),
    "leather": (0.135, 0.044, 0.015, 1.0),
    "leather_light": (0.315, 0.120, 0.035, 1.0),
    "steel": (0.135, 0.165, 0.165, 1.0),
    "steel_light": (0.285, 0.335, 0.330, 1.0),
    "steel_dark": (0.034, 0.047, 0.048, 1.0),
    "brass": (0.51, 0.285, 0.040, 1.0),
    "eye": (0.015, 0.02, 0.022, 1.0),
    "ghost": (0.22, 0.27, 0.29, 1.0),
    "stage": (0.12, 0.14, 0.15, 1.0),
}


BONES = [
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
]

CAPE_BONES = [
    ("cape.0", "chest", (0.0, 0.205, 1.64), (0.0, 0.245, 1.37)),
    ("cape.1", "cape.0", (0.0, 0.245, 1.37), (0.0, 0.290, 1.10)),
    ("cape.2", "cape.1", (0.0, 0.290, 1.10), (0.0, 0.340, 0.82)),
    ("cape.3", "cape.2", (0.0, 0.340, 0.82), (0.0, 0.390, 0.54)),
]


ASSEMBLED_COMPONENTS = {
    "CC_HERO_BODY_BASE",
    "CC_HERO_HAIR",
    "CC_HERO_PADDED_UNDERLAYER",
    "CC_HERO_TUNIC",
    "CC_HERO_CAPE",
    "CC_HERO_CUIRASS",
    "CC_HERO_PAULDRON_L",
    "CC_HERO_PAULDRON_R",
    "CC_HERO_BRACER_L",
    "CC_HERO_BRACER_R",
    "CC_HERO_GREAVE_L",
    "CC_HERO_GREAVE_R",
    "CC_HERO_GLOVE_L",
    "CC_HERO_GLOVE_R",
    "CC_HERO_BOOT_L",
    "CC_HERO_BOOT_R",
    "CC_HERO_BELT_SATCHEL",
}


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_HERO_COMPONENT_LIBRARY"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 900
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene["cc_library_version"] = LIBRARY_VERSION
    scene["cc_unit"] = "meter"
    scene["cc_forward_axis"] = "-Y"
    scene["cc_up_axis"] = "+Z"
    scene["cc_art_direction"] = ART_DIRECTION
    scene["cc_procedural_schema_version"] = WAYFARER_RECIPE.schema_version
    scene["cc_body_preset"] = WAYFARER_RECIPE.body.name

    world = bpy.data.worlds.new("CC_HeroWorld")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.018, 0.022, 0.026, 1.0)
    background.inputs["Strength"].default_value = 0.55
    scene.world = world


def make_material(
    name: str,
    color: tuple[float, float, float, float],
    *,
    metallic: float = 0.0,
    roughness: float = 0.62,
    emission: float = 0.0,
) -> bpy.types.Material:
    mat = bpy.data.materials.new(f"MAT_{name.upper()}")
    mat.diffuse_color = color
    mat.use_nodes = True
    node = mat.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value = color
    node.inputs["Metallic"].default_value = metallic
    node.inputs["Roughness"].default_value = roughness
    if emission > 0.0:
        node.inputs["Emission Color"].default_value = color
        node.inputs["Emission Strength"].default_value = emission
    MATERIALS[name] = mat
    return mat


def make_palette() -> None:
    for name, color in PALETTE.items():
        metallic = 0.72 if name in {"steel", "steel_light", "steel_dark", "brass"} else 0.0
        if name in {"teal", "teal_light", "teal_dark", "padding", "padding_dark",
                    "cape", "cape_light", "brigandine", "brigandine_edge"}:
            roughness = 0.84
        elif name in {"leather", "leather_light"}:
            roughness = 0.62
        elif metallic:
            roughness = 0.42 if name == "steel_dark" else 0.34
        else:
            roughness = 0.70
        emission = 1.2 if name == "rig_cyan" else 0.0
        make_material(name, color, metallic=metallic, roughness=roughness, emission=emission)


def new_group(name: str, parent: bpy.types.Collection) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    parent.children.link(collection)
    return collection


def new_component(
    name: str,
    parent: bpy.types.Collection,
    *,
    component_id: str,
    kind: str,
    slot: str,
    layer: str,
    anchor: str,
    coverage: tuple[str, ...],
) -> bpy.types.Collection:
    collection = new_group(name, parent)
    collection["cc_component_id"] = component_id
    collection["cc_component_kind"] = kind
    collection["cc_slot"] = slot
    collection["cc_layer"] = layer
    collection["cc_anchor_bone"] = anchor
    collection["cc_coverage"] = ",".join(coverage)
    collection["cc_library_version"] = LIBRARY_VERSION
    COMPONENTS[name] = collection
    COMPONENT_RECORDS.append(
        {
            "id": component_id,
            "collection": name,
            "kind": kind,
            "slot": slot,
            "layer": layer,
            "anchor_bone": anchor,
            "coverage": list(coverage),
            "export": f"exports/hero_glb/{component_id}.glb",
        }
    )
    return collection


def move_to_collection(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)


def tag_object(obj: bpy.types.Object, collection: bpy.types.Collection, role: str) -> None:
    obj["cc_component_id"] = collection.get("cc_component_id", "guide")
    obj["cc_role"] = role
    obj["cc_layer"] = collection.get("cc_layer", "guide")
    obj["cc_library_version"] = LIBRARY_VERSION


def add_bevel(obj: bpy.types.Object, width: float, segments: int = 2) -> None:
    modifier = obj.modifiers.new("CC_Bevel", "BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"


def assign_material(obj: bpy.types.Object, mat: str) -> None:
    obj.data.materials.append(MATERIALS[mat])


def add_cube(
    name: str,
    location: tuple[float, float, float],
    dimensions: tuple[float, float, float],
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    bevel_width: float = 0.025,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel_width:
        add_bevel(obj, min(bevel_width, min(dimensions) * 0.24))
    assign_material(obj, mat)
    move_to_collection(obj, collection)
    tag_object(obj, collection, role)
    return obj


def add_ico(
    name: str,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    subdivisions: int = 2,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign_material(obj, mat)
    move_to_collection(obj, collection)
    tag_object(obj, collection, role)
    return obj


def add_cylinder_between(
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    radius_start: float,
    radius_end: float,
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    vertices: int = 8,
    bevel_width: float = 0.012,
) -> bpy.types.Object:
    a = Vector(start)
    b = Vector(end)
    delta = b - a
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices,
        radius1=radius_start,
        radius2=radius_end,
        depth=delta.length,
        location=(a + b) * 0.5,
    )
    obj = bpy.context.object
    obj.name = name
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = delta.to_track_quat("Z", "Y")
    if bevel_width:
        add_bevel(obj, bevel_width)
    assign_material(obj, mat)
    move_to_collection(obj, collection)
    tag_object(obj, collection, role)
    return obj


def add_box_between(
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    width: float,
    depth: float,
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    bevel_width: float = 0.008,
) -> bpy.types.Object:
    """Create a beveled rectangular strip aligned between two points."""
    a = Vector(start)
    b = Vector(end)
    delta = b - a
    rotation = delta.to_track_quat("Z", "Y").to_euler()
    obj = add_cube(
        name, tuple((a + b) * 0.5), (width, depth, delta.length),
        mat, collection, role, rotation=tuple(rotation),
        bevel_width=bevel_width,
    )
    return obj


def add_torus(
    name: str,
    location: tuple[float, float, float],
    major_radius: float,
    minor_radius: float,
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_torus_add(
        major_segments=16,
        minor_segments=6,
        major_radius=major_radius,
        minor_radius=minor_radius,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign_material(obj, mat)
    move_to_collection(obj, collection)
    tag_object(obj, collection, role)
    return obj


def add_loft(
    name: str,
    rings: list[tuple[float, float, float] | tuple[float, float, float, float]],
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    segments: int = 8,
) -> bpy.types.Object:
    vertices: list[tuple[float, float, float]] = []
    for ring in rings:
        z, radius_x, radius_y = ring[:3]
        center_y = ring[3] if len(ring) == 4 else 0.0
        for segment in range(segments):
            angle = math.tau * segment / segments
            vertices.append((math.cos(angle) * radius_x,
                             center_y + math.sin(angle) * radius_y, z))
    faces: list[tuple[int, ...]] = []
    for ring in range(len(rings) - 1):
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            a = ring * segments + segment
            b = ring * segments + next_segment
            c = (ring + 1) * segments + next_segment
            d = (ring + 1) * segments + segment
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(segments))))
    top = (len(rings) - 1) * segments
    faces.append(tuple(top + segment for segment in range(segments)))
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign_material(obj, mat)
    add_bevel(obj, 0.018)
    tag_object(obj, collection, role)
    return obj


def add_limb_loft(
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    profiles: list[tuple[float, float, float] | tuple[float, float, float, float]],
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    segments: int = 10,
    bevel_width: float = 0.010,
) -> bpy.types.Object:
    """Create an elliptical, muscle-shaped volume along an arbitrary bone."""
    a = Vector(start)
    b = Vector(end)
    axis = (b - a).normalized()
    depth_axis = Vector((0.0, 1.0, 0.0))
    depth_axis = (depth_axis - axis * depth_axis.dot(axis)).normalized()
    width_axis = depth_axis.cross(axis).normalized()
    vertices: list[tuple[float, float, float]] = []
    for profile in profiles:
        amount, radius_width, radius_depth = profile[:3]
        center_depth = profile[3] if len(profile) == 4 else 0.0
        center = a.lerp(b, amount) + depth_axis * center_depth
        for segment in range(segments):
            angle = math.tau * segment / segments
            point = (center + width_axis * math.cos(angle) * radius_width +
                     depth_axis * math.sin(angle) * radius_depth)
            vertices.append(tuple(point))
    faces: list[tuple[int, ...]] = []
    for ring in range(len(profiles) - 1):
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            a_index = ring * segments + segment
            b_index = ring * segments + next_segment
            c_index = (ring + 1) * segments + next_segment
            d_index = (ring + 1) * segments + segment
            faces.append((a_index, b_index, c_index, d_index))
    faces.append(tuple(reversed(range(segments))))
    top = (len(profiles) - 1) * segments
    faces.append(tuple(top + segment for segment in range(segments)))
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign_material(obj, mat)
    if bevel_width:
        add_bevel(obj, bevel_width, segments=2)
    tag_object(obj, collection, role)
    return obj


def add_boot_loft(
    name: str,
    center_x: float,
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    scale: float = 1.0,
) -> bpy.types.Object:
    """Create a tapered ankle-to-toe boot shell instead of a block foot."""
    rings = (
        (0.055, 0.080, 0.135, 0.075),
        (-0.035, 0.092, 0.150, 0.095),
        (-0.145, 0.102, 0.125, 0.075),
        (-0.265, 0.094, 0.095, 0.052),
    )
    vertices: list[tuple[float, float, float]] = []
    segments = 10
    for y, radius_x, center_z, radius_z in rings:
        for segment in range(segments):
            angle = math.tau * segment / segments
            vertices.append((center_x + math.cos(angle) * radius_x * scale,
                             y, center_z + math.sin(angle) * radius_z * scale))
    faces: list[tuple[int, ...]] = []
    for ring in range(len(rings) - 1):
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            a = ring * segments + segment
            b = ring * segments + next_segment
            c = (ring + 1) * segments + next_segment
            d = (ring + 1) * segments + segment
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(segments))))
    top = (len(rings) - 1) * segments
    faces.append(tuple(top + segment for segment in range(segments)))
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign_material(obj, mat)
    add_bevel(obj, 0.016, segments=2)
    tag_object(obj, collection, role)
    return obj


def add_boot_sole(
    name: str,
    center_x: float,
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    scale: float = 1.0,
) -> bpy.types.Object:
    """Create a close-cut outsole that follows the boot, not a toy-like slab."""
    outline = (
        (-0.292, -0.076), (-0.282, 0.076), (-0.180, 0.103),
        (-0.060, 0.101), (0.065, 0.079), (0.075, -0.079),
        (-0.060, -0.101), (-0.180, -0.103),
    )
    bottom_z = 0.035
    top_z = 0.066
    vertices = [
        (center_x + x * scale, y * scale, z)
        for z in (bottom_z, top_z)
        for y, x in outline
    ]
    count = len(outline)
    faces: list[tuple[int, ...]] = [
        tuple(reversed(range(count))),
        tuple(count + index for index in range(count)),
    ]
    for index in range(count):
        next_index = (index + 1) % count
        faces.append((index, next_index, count + next_index, count + index))
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign_material(obj, mat)
    add_bevel(obj, 0.010, segments=2)
    tag_object(obj, collection, role)
    return obj


def add_panel(
    name: str,
    vertices: list[tuple[float, float, float]],
    mat: str,
    collection: bpy.types.Collection,
    role: str,
    *,
    thickness: float = 0.018,
) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], [tuple(range(len(vertices)))])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign_material(obj, mat)
    solidify = obj.modifiers.new("CC_Solidify", "SOLIDIFY")
    solidify.thickness = thickness
    add_bevel(obj, 0.012)
    tag_object(obj, collection, role)
    return obj


def parent_to_bone(obj: bpy.types.Object, armature: bpy.types.Object, bone_name: str) -> None:
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.parent_type = "BONE"
    obj.parent_bone = bone_name
    obj.matrix_world = world
    obj["cc_anchor_bone"] = bone_name


def skin_to_armature(obj: bpy.types.Object, armature: bpy.types.Object) -> None:
    obj.parent = armature
    obj.matrix_parent_inverse = armature.matrix_world.inverted()
    modifier = obj.modifiers.new("CC_Armature", "ARMATURE")
    modifier.object = armature
    pelvis = obj.vertex_groups.new(name="pelvis")
    spine = obj.vertex_groups.new(name="spine")
    chest = obj.vertex_groups.new(name="chest")
    for vertex in obj.data.vertices:
        z = vertex.co.z
        if z <= 1.14:
            pelvis.add([vertex.index], 1.0, "REPLACE")
        elif z <= 1.39:
            amount = (z - 1.14) / 0.25
            pelvis.add([vertex.index], 1.0 - amount, "REPLACE")
            spine.add([vertex.index], amount, "REPLACE")
        elif z <= 1.52:
            amount = (z - 1.39) / 0.13
            spine.add([vertex.index], 1.0 - amount, "REPLACE")
            chest.add([vertex.index], amount, "REPLACE")
        else:
            chest.add([vertex.index], 1.0, "REPLACE")
    obj["cc_smooth_skin"] = True


def skin_cape_to_armature(
    cape: bpy.types.Object,
    armature: bpy.types.Object,
    columns: int,
    rows: int,
) -> None:
    """Blend the authored cloth rows across the four runtime cape bones."""
    cape.parent = armature
    cape.matrix_parent_inverse = armature.matrix_world.inverted()
    modifier = cape.modifiers.new("CC_Armature", "ARMATURE")
    modifier.object = armature
    groups = [cape.vertex_groups.new(name=f"cape.{index}") for index in range(4)]
    for row in range(rows):
        chain = row / (rows - 1) * 3.0
        first = min(3, int(math.floor(chain)))
        second = min(3, first + 1)
        blend = chain - first
        indices = [row * columns + column for column in range(columns)]
        groups[first].add(indices, 1.0 - blend, "REPLACE")
        if second != first:
            groups[second].add(indices, blend, "REPLACE")
    cape["cc_smooth_skin"] = True
    cape["cc_runtime_bones"] = ",".join(f"cape.{index}" for index in range(4))


def build_structure() -> dict[str, bpy.types.Collection]:
    root = bpy.context.scene.collection
    library = new_group("CC_HERO_LIBRARY", root)
    groups = {
        "library": library,
        "guides": new_group("00_GUIDES", library),
        "rig": new_group("10_RIG", library),
        "anatomy": new_group("20_ANATOMY", library),
        "garments": new_group("30_GARMENTS", library),
        "cloth": new_group("40_CLOTH", library),
        "armor": new_group("50_ARMOR", library),
        "accessories": new_group("60_ACCESSORIES", library),
        "presentation": new_group("90_PRESENTATION", root),
    }
    groups["rig_guides"] = new_group("CC_HERO_RIG_GUIDES", groups["guides"])
    groups["cape_guides"] = new_group("CC_HERO_CAPE_CAGE_GUIDES", groups["guides"])
    groups["exploded"] = new_group("CC_HERO_EXPLODED_DISPLAY", groups["presentation"])
    return groups


def build_armature(parent: bpy.types.Collection) -> bpy.types.Object:
    collection = new_component(
        "CC_HERO_SKELETON", parent,
        component_id="hero_skeleton_v01", kind="skeleton", slot="skeleton",
        layer="anatomy", anchor="root", coverage=("whole_body",),
    )
    armature_data = bpy.data.armatures.new("ARM_CrownlessHero")
    armature = bpy.data.objects.new("ARM_CrownlessHero", armature_data)
    collection.objects.link(armature)
    armature.show_in_front = True
    armature_data.display_type = "OCTAHEDRAL"
    armature["cc_skeleton_id"] = "crownless_humanoid_v01"
    armature["cc_forward_axis"] = "-Y"
    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    created: dict[str, bpy.types.EditBone] = {}
    for name, parent_name, head, tail in BONES + CAPE_BONES:
        bone = armature_data.edit_bones.new(name)
        bone.head = head
        bone.tail = tail
        bone.roll = 0.0
        if parent_name:
            bone.parent = created[parent_name]
        created[name] = bone
    bpy.ops.object.mode_set(mode="OBJECT")
    armature.select_set(False)
    tag_object(armature, collection, "canonical_armature")
    return armature


def build_rig_guides(groups: dict[str, bpy.types.Collection]) -> None:
    collection = groups["rig_guides"]
    collection["cc_layer"] = "guide"
    for name, _parent, head, tail in BONES:
        if name == "root":
            continue
        add_cylinder_between(f"GUIDE_Bone_{name}", head, tail, 0.018, 0.018,
                             "rig_cyan", collection, "bone_guide", vertices=6,
                             bevel_width=0.0)
        add_ico(f"GUIDE_Joint_{name}", head, (0.032, 0.032, 0.032),
                "rig_cyan", collection, "joint_guide", subdivisions=1)


def add_socket(
    name: str,
    location: tuple[float, float, float],
    bone: str,
    armature: bpy.types.Object,
    collection: bpy.types.Collection,
) -> bpy.types.Object:
    obj = bpy.data.objects.new(name, None)
    obj.empty_display_type = "SPHERE"
    obj.empty_display_size = 0.065
    obj.location = location
    collection.objects.link(obj)
    parent_to_bone(obj, armature, bone)
    obj["cc_socket_type"] = name.removeprefix("SOCKET_").lower()
    SOCKET_RECORDS.append(
        {"name": name, "bone": bone, "position_m": list(location)}
    )
    return obj


def build_sockets(armature: bpy.types.Object, skeleton: bpy.types.Collection) -> None:
    sockets = (
        ("SOCKET_HEAD", (0.0, 0.0, 2.04), "head"),
        ("SOCKET_FACE", (0.0, -0.19, 1.90), "head"),
        ("SOCKET_CHEST_FRONT", (0.0, -0.20, 1.47), "chest"),
        ("SOCKET_BACK", (0.0, 0.20, 1.50), "chest"),
        ("SOCKET_SHOULDER_L", (-0.32, 0.0, 1.58), "upper_arm.L"),
        ("SOCKET_SHOULDER_R", (0.32, 0.0, 1.58), "upper_arm.R"),
        ("SOCKET_FOREARM_L", (-0.52, 0.0, 1.08), "forearm.L"),
        ("SOCKET_FOREARM_R", (0.52, 0.0, 1.08), "forearm.R"),
        ("SOCKET_HAND_L", (-0.58, -0.03, 0.85), "hand.L"),
        ("SOCKET_HAND_R", (0.58, -0.03, 0.85), "hand.R"),
        ("SOCKET_BELT", (0.0, 0.0, 1.02), "pelvis"),
        ("SOCKET_SHIN_L", (-0.15, -0.04, 0.35), "shin.L"),
        ("SOCKET_SHIN_R", (0.15, -0.04, 0.35), "shin.R"),
        ("SOCKET_FOOT_L", (-0.15, -0.14, 0.10), "foot.L"),
        ("SOCKET_FOOT_R", (0.15, -0.14, 0.10), "foot.R"),
    )
    for name, location, bone in sockets:
        add_socket(name, location, bone, armature, skeleton)


def build_body(parent: bpy.types.Collection, armature: bpy.types.Object) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_BODY_BASE", parent,
        component_id="hero_body_base_v01", kind="body", slot="body",
        layer="skin", anchor="root", coverage=("whole_body",),
    )
    torso = add_loft(
        "GEO_BodyTorso", loft_rows(HERO_BODY_PROFILES["torso"]),
        "body_neutral", collection, "athletic_torso", segments=12,
    )
    skin_to_armature(torso, armature)
    pelvis = add_loft(
        "GEO_BodyPelvis", loft_rows(HERO_BODY_PROFILES["pelvis"]),
        "body_neutral", collection, "athletic_pelvis", segments=12,
    )
    parent_to_bone(pelvis, armature, "pelvis")

    limb_segments = [
        ("UpperArmL", (-0.29, 0.0, 1.56), (-0.46, 0.0, 1.26),
         sweep_rows(HERO_BODY_PROFILES["upper_arm"]), "upper_arm.L"),
        ("ForearmL", (-0.46, 0.0, 1.26), (-0.57, -0.015, 0.94),
         sweep_rows(HERO_BODY_PROFILES["forearm"]), "forearm.L"),
        ("UpperArmR", (0.29, 0.0, 1.56), (0.46, 0.0, 1.26),
         sweep_rows(HERO_BODY_PROFILES["upper_arm"]), "upper_arm.R"),
        ("ForearmR", (0.46, 0.0, 1.26), (0.57, -0.015, 0.94),
         sweep_rows(HERO_BODY_PROFILES["forearm"]), "forearm.R"),
        ("ThighL", (-0.14, 0.0, 1.00), (-0.15, 0.0, 0.56),
         sweep_rows(HERO_BODY_PROFILES["thigh"]), "thigh.L"),
        ("ShinL", (-0.15, 0.0, 0.56), (-0.15, 0.0, 0.13),
         sweep_rows(HERO_BODY_PROFILES["shin"]), "shin.L"),
        ("ThighR", (0.14, 0.0, 1.00), (0.15, 0.0, 0.56),
         sweep_rows(HERO_BODY_PROFILES["thigh"]), "thigh.R"),
        ("ShinR", (0.15, 0.0, 0.56), (0.15, 0.0, 0.13),
         sweep_rows(HERO_BODY_PROFILES["shin"]), "shin.R"),
    ]
    for name, start, end, profiles, bone in limb_segments:
        obj = add_limb_loft(f"GEO_Body{name}", start, end, profiles,
                            "body_neutral", collection, "sculpted_limb")
        parent_to_bone(obj, armature, bone)

    for side, x in (("L", -0.58), ("R", 0.58)):
        hand_scale = WAYFARER_RECIPE.body.hand_scale
        hand = add_ico(f"GEO_BodyHand{side}", (x, -0.040, 0.85),
                       (0.064 * hand_scale, 0.050 * hand_scale,
                        0.092 * hand_scale), "skin", collection,
                       "sculpted_hand", subdivisions=2)
        parent_to_bone(hand, armature, f"hand.{side}")
        foot = add_boot_loft(f"GEO_BodyFoot{side}", x * 0.2586,
                             "body_neutral", collection, "sculpted_foot",
                             scale=0.86 * WAYFARER_RECIPE.body.foot_scale)
        parent_to_bone(foot, armature, f"foot.{side}")

    neck = add_limb_loft("GEO_BodyNeck", (0.0, 0.0, 1.61),
                         (0.0, 0.0, 1.77),
                         sweep_rows(HERO_BODY_PROFILES["neck"]),
                         "skin", collection, "neck", segments=12)
    parent_to_bone(neck, armature, "neck")



    head = add_ico("GEO_BodyHead", (0.0, 0.0, 1.92), (0.132, 0.116, 0.172),
                   "skin", collection, "head")
    parent_to_bone(head, armature, "head")
    nose = add_ico("GEO_BodyNose", (0.0, -0.118, 1.915), (0.015, 0.018, 0.030),
                   "skin_light", collection, "face", subdivisions=1)
    parent_to_bone(nose, armature, "head")
    for side, x in (("L", -0.040), ("R", 0.040)):


        eye = add_ico(f"GEO_BodyEye{side}", (x, -0.116, 1.950),
                      (0.015, 0.008, 0.010), "eye", collection, "face", subdivisions=1)
        parent_to_bone(eye, armature, "head")
        outer_x = x - 0.020 if side == "L" else x + 0.020
        inner_x = x + 0.020 if side == "L" else x - 0.020
        brow = add_box_between(
            f"GEO_BodyBrow{side}", (outer_x, -0.116, 1.980),
            (inner_x, -0.119, 1.976), 0.009, 0.006,
            "hair", collection, "face", bevel_width=0.002,
        )
        parent_to_bone(brow, armature, "head")
        ear = add_ico(f"GEO_BodyEar{side}", ((-1 if side == "L" else 1) * 0.125,
                      -0.005, 1.925), (0.018, 0.017, 0.034), "skin_light",
                      collection, "face", subdivisions=1)
        parent_to_bone(ear, armature, "head")
    mouth = add_box_between("GEO_BodyMouth", (-0.028, -0.112, 1.865),
                            (0.028, -0.112, 1.865), 0.007, 0.006,
                            "hair", collection, "face", bevel_width=0.002)
    parent_to_bone(mouth, armature, "head")
    return collection


def build_hair(parent: bpy.types.Collection, armature: bpy.types.Object) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_HAIR", parent,
        component_id="hero_hair_v01", kind="hair", slot="head",
        layer="accessory", anchor="head", coverage=("head",),
    )
    cap = add_ico("GEO_HairCap", (0.0, 0.018, 2.020), (0.137, 0.119, 0.105),
                  "hair", collection, "hair_cap")
    parent_to_bone(cap, armature, "head")
    bun = add_ico("GEO_HairBun", (0.050, 0.128, 1.970), (0.065, 0.058, 0.062),
                  "hair", collection, "hair_bun", subdivisions=1)
    parent_to_bone(bun, armature, "head")


    for suffix, vertices in (
        ("L", [(-0.118, -0.124, 2.055), (0.010, -0.128, 2.100),
               (-0.020, -0.133, 2.015), (-0.108, -0.132, 1.998)]),
        ("R", [(0.004, -0.128, 2.100), (0.118, -0.124, 2.060),
               (0.096, -0.132, 2.010), (0.034, -0.133, 2.026)]),
    ):
        fringe = add_panel(f"GEO_HairFringe{suffix}", vertices, "hair",
                           collection, "hair_fringe", thickness=0.018)
        parent_to_bone(fringe, armature, "head")
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        bottom = 1.905 if suffix == "L" else 1.932
        temple = add_cylinder_between(
            f"GEO_HairTemple{suffix}", (sign * 0.122, -0.012, 2.020),
            (sign * 0.128, -0.064, bottom), 0.019, 0.009,
            "hair", collection, "hair_lock", vertices=6, bevel_width=0.006,
        )
        parent_to_bone(temple, armature, "head")





    circlet = add_torus(
        "GEO_BrokenCrownCirclet", (0.0, -0.002, 2.058), 0.143, 0.014,
        "brass", collection, "broken_crown", scale=(1.0, 0.88, 1.0),
    )
    parent_to_bone(circlet, armature, "head")
    for suffix, start, end in (
        ("L", (-0.066, -0.129, 2.060), (-0.074, -0.134, 2.132)),
        ("C", (0.000, -0.133, 2.060), (0.000, -0.138, 2.166)),
        ("R", (0.066, -0.129, 2.060), (0.083, -0.132, 2.112)),
    ):
        tine = add_box_between(
            f"GEO_BrokenCrownTine{suffix}", start, end, 0.032, 0.020,
            "brass", collection, "broken_crown", bevel_width=0.005,
        )
        parent_to_bone(tine, armature, "head")
    return collection


def add_muscle(
    collection: bpy.types.Collection,
    armature: bpy.types.Object,
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    radius: float,
    mat: str,
    bone: str,
    joint: str,
) -> None:
    obj = add_limb_loft(
        f"GEO_Muscle_{name}", start, end,
        [(0.0, radius * 0.66, radius * 0.60),
         (0.30, radius * 1.02, radius * 0.92),
         (0.58, radius * 1.12, radius),
         (1.0, radius * 0.62, radius * 0.56)],
        mat, collection, "muscle_belly", segments=10,
        bevel_width=0.008,
    )
    obj["cc_muscle_name"] = name
    obj["cc_driven_joint"] = joint
    parent_to_bone(obj, armature, bone)


def build_muscles(parent: bpy.types.Collection, armature: bpy.types.Object) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_MUSCLE_GUIDES", parent,
        component_id="hero_muscle_guides_v01", kind="anatomy_guide",
        slot="muscles", layer="anatomy", anchor="root",
        coverage=("torso", "arms", "legs"),
    )
    for side, x, suffix in ((-1.0, -0.12, "L"), (1.0, 0.12, "R")):
        pec = add_limb_loft(
            f"GEO_Muscle_Pectoral{suffix}",
            (side * 0.020, -0.170, 1.505), (side * 0.255, -0.165, 1.465),
            [(0.0, 0.048, 0.024), (0.28, 0.086, 0.040),
             (0.70, 0.095, 0.046), (1.0, 0.040, 0.022)],
            "muscle_blue", collection, "muscle_belly", segments=12,
            bevel_width=0.008,
        )
        pec["cc_muscle_name"] = f"pectoral.{suffix}"
        pec["cc_driven_joint"] = f"shoulder.{suffix}"
        parent_to_bone(pec, armature, "chest")
        deltoid = add_ico(f"GEO_Muscle_Deltoid{suffix}",
                          (side * 0.335, -0.020, 1.525),
                          (0.090, 0.080, 0.112), "muscle_purple",
                          collection, "muscle_belly", subdivisions=2)
        deltoid["cc_muscle_name"] = f"deltoid.{suffix}"
        deltoid["cc_driven_joint"] = f"shoulder.{suffix}"
        parent_to_bone(deltoid, armature, f"upper_arm.{suffix}")
        add_muscle(collection, armature, f"Biceps{suffix}",
                   (side * 0.38, -0.075, 1.42), (side * 0.47, -0.060, 1.20),
                   0.058, "muscle_purple", f"upper_arm.{suffix}", f"elbow.{suffix}")
        add_muscle(collection, armature, f"Forearm{suffix}",
                   (side * 0.48, -0.055, 1.19), (side * 0.56, -0.050, 0.98),
                   0.050, "muscle_gold", f"forearm.{suffix}", f"wrist.{suffix}")
        add_muscle(collection, armature, f"Quadriceps{suffix}",
                   (side * 0.14, -0.090, 0.94), (side * 0.15, -0.075, 0.62),
                   0.090, "muscle_purple", f"thigh.{suffix}", f"knee.{suffix}")
        add_muscle(collection, armature, f"Calf{suffix}",
                   (side * 0.15, 0.060, 0.51), (side * 0.15, 0.040, 0.19),
                   0.070, "muscle_blue", f"shin.{suffix}", f"ankle.{suffix}")
        lat = add_ico(f"GEO_Muscle_Latissimus{suffix}",
                      (side * 0.255, 0.055, 1.37),
                      (0.095, 0.075, 0.185), "muscle_blue", collection,
                      "muscle_belly", subdivisions=2)
        lat["cc_muscle_name"] = f"latissimus.{suffix}"
        lat["cc_driven_joint"] = "spine_pitch"
        parent_to_bone(lat, armature, "spine")
        oblique = add_limb_loft(
            f"GEO_Muscle_Oblique{suffix}",
            (side * 0.215, -0.120, 1.38), (side * 0.170, -0.150, 1.12),
            [(0.0, 0.040, 0.026), (0.45, 0.060, 0.035),
             (1.0, 0.034, 0.024)],
            "muscle_gold", collection, "muscle_belly", segments=8,
            bevel_width=0.006,
        )
        oblique["cc_muscle_name"] = f"oblique.{suffix}"
        oblique["cc_driven_joint"] = "spine_pitch"
        parent_to_bone(oblique, armature, "spine")
    for suffix, x in (("L", -0.060), ("R", 0.060)):
        abdomen = add_limb_loft(
            f"GEO_Muscle_RectusAbdominis{suffix}",
            (x, -0.158, 1.425), (x, -0.150, 1.095),
            [(0.0, 0.030, 0.020), (0.14, 0.052, 0.033),
             (0.30, 0.041, 0.027), (0.46, 0.055, 0.035),
             (0.62, 0.041, 0.027), (0.79, 0.050, 0.032),
             (1.0, 0.028, 0.020)],
            "muscle_green", collection, "muscle_volume", segments=10,
            bevel_width=0.006,
        )
        abdomen["cc_muscle_name"] = f"rectus_abdominis.{suffix}"
        abdomen["cc_driven_joint"] = "spine_pitch"
        parent_to_bone(abdomen, armature, "spine")
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        trap = add_limb_loft(
            f"GEO_Muscle_Trapezius{suffix}",
            (sign * 0.045, 0.025, 1.64), (sign * 0.255, 0.035, 1.56),
            [(0.0, 0.032, 0.026), (0.45, 0.060, 0.040),
             (1.0, 0.038, 0.030)],
            "muscle_purple", collection, "muscle_belly", segments=8,
            bevel_width=0.006,
        )
        trap["cc_muscle_name"] = f"trapezius.{suffix}"
        trap["cc_driven_joint"] = f"shoulder.{suffix}"
        parent_to_bone(trap, armature, "chest")
    return collection


def build_padding(parent: bpy.types.Collection, armature: bpy.types.Object) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_PADDED_UNDERLAYER", parent,
        component_id="hero_padded_underlayer_v01", kind="garment",
        slot="underlayer", layer="padding", anchor="chest",
        coverage=("torso", "upper_arms", "forearms"),
    )
    padded_torso = derive_shell(HERO_BODY_PROFILES["torso"], PADDED_UNDERLAYER)
    padded_upper_arm = derive_shell(HERO_BODY_PROFILES["upper_arm"], PADDED_UNDERLAYER)
    padded_forearm = derive_shell(HERO_BODY_PROFILES["forearm"], PADDED_UNDERLAYER)
    torso = add_loft(
        "GEO_PaddedTorso", loft_rows(padded_torso),
        "padding", collection, "padded_torso", segments=12,
    )
    skin_to_armature(torso, armature)
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        upper = add_limb_loft(
            f"GEO_PaddedUpperSleeve{suffix}",
            (sign * 0.29, 0.0, 1.57), (sign * 0.46, 0.0, 1.26),
            sweep_rows(padded_upper_arm),
            "padding", collection, "fitted_padded_sleeve", segments=12,
        )
        parent_to_bone(upper, armature, f"upper_arm.{suffix}")
        lower = add_limb_loft(
            f"GEO_PaddedLowerSleeve{suffix}",
            (sign * 0.46, 0.0, 1.26), (sign * 0.56, -0.01, 0.96),
            sweep_rows(padded_forearm),
            "padding", collection, "fitted_padded_sleeve", segments=12,
        )
        parent_to_bone(lower, armature, f"forearm.{suffix}")
        for amount in (0.25, 0.50, 0.75):
            a = Vector((sign * 0.46, 0.0, 1.26)).lerp(
                Vector((sign * 0.56, -0.01, 0.96)), amount)
            ring = add_torus(f"GEO_PaddingStitch{suffix}", tuple(a), 0.072, 0.008,
                             "padding_dark", collection, "quilt_stitch",
                             rotation=(math.radians(72), 0.0, sign * math.radians(18)),
                             scale=(1.0, 0.78, 1.0))
            parent_to_bone(ring, armature, f"forearm.{suffix}")
        for index, y_offset in enumerate((-0.068, 0.068)):
            seam = add_box_between(
                f"GEO_PaddingSeam{suffix}_{index}",
                (sign * 0.475, y_offset, 1.205),
                (sign * 0.545, -y_offset, 1.005),
                0.010, 0.009, "padding_dark", collection, "quilt_stitch",
                bevel_width=0.003,
            )
            parent_to_bone(seam, armature, f"forearm.{suffix}")
    return collection


def build_tunic(parent: bpy.types.Collection, armature: bpy.types.Object) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_TUNIC", parent,
        component_id="hero_tunic_v01", kind="garment", slot="torso_garment",
        layer="garment", anchor="chest",
        coverage=("torso", "upper_arms", "hips"),
    )
    tunic_torso = clip_profile(
        derive_shell(HERO_BODY_PROFILES["torso"], FITTED_TUNIC),
        1.04, 1.67,
    )
    tunic_sleeve = clip_profile(
        derive_shell(HERO_BODY_PROFILES["upper_arm"], FITTED_TUNIC),
        0.0, 0.62, normalize=True,
    )
    torso = add_loft(
        "GEO_TunicTorso", loft_rows(tunic_torso),
        "teal_dark", collection, "tunic_torso", segments=12,
    )
    skin_to_armature(torso, armature)
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        sleeve = add_limb_loft(
            f"GEO_TunicSleeve{suffix}",
            (sign * 0.30, 0.0, 1.57), (sign * 0.405, 0.0, 1.38),
            sweep_rows(tunic_sleeve),
            "teal", collection, "sculpted_short_sleeve", segments=12,
        )
        parent_to_bone(sleeve, armature, f"upper_arm.{suffix}")
    panels = [
        ("FrontL", [(-0.24, -0.185, 1.08), (-0.035, -0.185, 1.08),
                    (-0.045, -0.20, 0.73), (-0.20, -0.20, 0.70)]),
        ("FrontR", [(0.035, -0.185, 1.08), (0.24, -0.185, 1.08),
                    (0.20, -0.20, 0.70), (0.045, -0.20, 0.73)]),
        ("Back", [(0.24, 0.18, 1.08), (-0.24, 0.18, 1.08),
                  (-0.20, 0.20, 0.72), (0.20, 0.20, 0.72)]),
        ("Left", [(-0.27, 0.14, 1.08), (-0.27, -0.14, 1.08),
                  (-0.24, -0.15, 0.74), (-0.24, 0.15, 0.74)]),
        ("Right", [(0.27, -0.14, 1.08), (0.27, 0.14, 1.08),
                   (0.24, 0.15, 0.74), (0.24, -0.15, 0.74)]),
    ]
    for name, vertices in panels:
        panel = add_panel(f"GEO_TunicPanel{name}", vertices, "teal", collection,
                          "tunic_skirt_panel")
        parent_to_bone(panel, armature, "pelvis")
    neckline = add_torus("GEO_TunicNeckline", (0.0, -0.015, 1.655),
                         0.128, 0.014, "teal_dark", collection,
                         "tunic_neckline", scale=(1.0, 0.78, 1.0))
    parent_to_bone(neckline, armature, "chest")
    placket = add_panel(
        "GEO_TunicPlacket",
        [(-0.030, -0.232, 1.62), (0.030, -0.232, 1.62),
         (0.030, -0.222, 1.39), (-0.030, -0.222, 1.39)],
        "padding", collection, "tunic_placket", thickness=0.012,
    )
    parent_to_bone(placket, armature, "chest")
    clasp = add_ico("GEO_TunicClasp", (0.0, -0.246, 1.505),
                    (0.018, 0.009, 0.018), "brass", collection,
                    "tunic_clasp", subdivisions=1)
    parent_to_bone(clasp, armature, "chest")
    for suffix, start, end in (
        ("L", (-0.20, -0.214, 0.735), (-0.050, -0.214, 0.758)),
        ("R", (0.050, -0.214, 0.758), (0.20, -0.214, 0.735)),
    ):
        hem = add_box_between(
            f"GEO_TunicHemTrim{suffix}", start, end, 0.025, 0.018,
            "teal_dark", collection, "tunic_trim", bevel_width=0.006,
        )
        parent_to_bone(hem, armature, "pelvis")
        slit_edge = add_box_between(
            f"GEO_TunicSlitEdge{suffix}",
            (start[0] if suffix == "R" else end[0], -0.214, 1.055),
            (start[0] if suffix == "R" else end[0], -0.214, 0.765),
            0.018, 0.016, "teal_dark", collection, "tunic_trim",
            bevel_width=0.004,
        )
        parent_to_bone(slit_edge, armature, "pelvis")
    return collection


def create_cape_mesh(
    collection: bpy.types.Collection,
    armature: bpy.types.Object,
    cage_collection: bpy.types.Collection,
) -> bpy.types.Object:
    columns = 6
    rows = 7
    vertices: list[tuple[float, float, float]] = []
    for row in range(rows):
        v = row / (rows - 1)


        half_width = 0.25 + math.sin(v * math.pi) * 0.060 + v * 0.020
        for column in range(columns):
            u = column / (columns - 1)
            signed = u * 2.0 - 1.0
            side_scale = 1.20 if signed < 0.0 else 0.76
            x = signed * half_width * side_scale
            center = abs(u - 0.5) / 0.5
            forked_hem = (1.0 - center) ** 2 * (v ** 6) * 0.17
            asymmetric_wear = (u - 0.5) * (v ** 5) * 0.050
            z = 1.64 - v * 0.86 + forked_hem + asymmetric_wear
            fold = math.sin(u * math.pi * 3.0) * math.sin(v * math.pi) * 0.032
            y = 0.205 + v * 0.13 + math.sin(u * math.pi) * v * 0.038 + fold
            vertices.append((x, y, z))
    faces: list[tuple[int, int, int, int]] = []
    for row in range(rows - 1):
        for column in range(columns - 1):
            a = row * columns + column
            b = a + 1
            c = (row + 1) * columns + column + 1
            d = (row + 1) * columns + column
            faces.append((a, b, c, d))
    mesh = bpy.data.meshes.new("MESH_HeroCape")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    cape = bpy.data.objects.new("GEO_HeroCape", mesh)
    collection.objects.link(cape)
    assign_material(cape, "cape")
    tag_object(cape, collection, "cloth_panel")
    pin = cape.vertex_groups.new(name="PIN_COLLAR")
    pin.add(list(range(columns)), 1.0, "REPLACE")
    cape["cc_cloth_solver"] = "coarse_pinned_cage"
    cape["cc_pin_group"] = "PIN_COLLAR"
    cape["cc_control_columns"] = columns
    cape["cc_control_rows"] = rows
    cloth = cape.modifiers.new("CC_Cloth", "CLOTH")
    cloth.settings.vertex_group_mass = "PIN_COLLAR"
    cloth.settings.quality = 5
    cloth.settings.tension_stiffness = 18.0
    cloth.settings.compression_stiffness = 12.0
    cloth.settings.shear_stiffness = 8.0
    cloth.settings.bending_stiffness = 0.45
    solidify = cape.modifiers.new("CC_Solidify", "SOLIDIFY")
    solidify.thickness = 0.025
    add_bevel(cape, 0.012)
    skin_cape_to_armature(cape, armature, columns, rows)

    cage_collection["cc_layer"] = "cloth_guide"
    for index, vertex in enumerate(vertices):
        node = add_ico(f"GUIDE_CapeNode_{index:02d}", vertex,
                       (0.022, 0.022, 0.022), "cape_light", cage_collection,
                       "cloth_control_node", subdivisions=1)
        node["cc_cloth_pin"] = index < columns
        parent_to_bone(node, armature, "chest")
    edges: set[tuple[int, int]] = set()
    for face in faces:
        for a, b in zip(face, face[1:] + face[:1]):
            edges.add(tuple(sorted((a, b))))
    for index, (a, b) in enumerate(sorted(edges)):
        edge = add_cylinder_between(f"GUIDE_CapeEdge_{index:02d}", vertices[a],
                                    vertices[b], 0.006, 0.006, "cape_light",
                                    cage_collection, "cloth_constraint",
                                    vertices=5, bevel_width=0.0)
        parent_to_bone(edge, armature, "chest")
    return cape


def build_cape(parent: bpy.types.Collection, armature: bpy.types.Object,
               cage_collection: bpy.types.Collection) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_CAPE", parent,
        component_id="hero_cape_v01", kind="cloth", slot="back_cloth",
        layer="cloth", anchor="chest",
        coverage=("back", "shoulders"),
    )
    create_cape_mesh(collection, armature, cage_collection)
    cowl = add_ico("GEO_CapeTravelCowl", (0.0, 0.145, 1.665),
                   (0.175, 0.078, 0.092), "cape", collection,
                   "travel_cowl", subdivisions=2)
    parent_to_bone(cowl, armature, "chest")
    collar = add_torus("GEO_CapeCollar", (0.0, 0.020, 1.635), 0.180, 0.018,
                       "cape", collection, "cape_collar",
                       scale=(1.22, 0.76, 1.0))
    parent_to_bone(collar, armature, "chest")
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        outer = 0.320 if suffix == "L" else 0.175
        inner = 0.090 if suffix == "L" else 0.055
        mantle = add_panel(
            f"GEO_CapeMantle{suffix}",
            [(sign * 0.025, -0.175, 1.642), (sign * outer, -0.142, 1.602),
             (sign * (outer + 0.025), 0.035, 1.552),
             (sign * inner, 0.070, 1.578)],
            "cape", collection, "cape_mantle", thickness=0.025,
        )
        parent_to_bone(mantle, armature, "chest")
        mantle_trim = add_box_between(
            f"GEO_CapeMantleTrim{suffix}",
            (sign * 0.050, -0.190, 1.635),
            (sign * (outer - 0.018), -0.158, 1.600),
            0.024, 0.016, "cape_light", collection, "cape_mantle_trim",
            bevel_width=0.005,
        )
        parent_to_bone(mantle_trim, armature, "chest")
    for suffix, x, radius in (("L", -0.275, 0.046), ("R", 0.235, 0.030)):
        pin = add_torus(f"GEO_CapePin{suffix}", (x, -0.175, 1.58),
                        radius, 0.012,
                        "brass" if suffix == "L" else "steel_dark",
                        collection, "cape_pin",
                        rotation=(math.radians(90), 0.0, 0.0))
        parent_to_bone(pin, armature, "chest")
    return collection


def build_cuirass(parent: bpy.types.Collection, armature: bpy.types.Object) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_CUIRASS", parent,
        component_id="hero_cuirass_v01", kind="rigid_armor", slot="torso_armor",
        layer="rigid_armor", anchor="chest",
        coverage=("chest", "abdomen", "back"),
    )
    cuirass_profile = clip_profile(
        derive_shell(HERO_BODY_PROFILES["torso"], CUIRASS_SHELL),
        1.15, 1.63,
    )
    shell = add_loft(
        "GEO_CuirassShell", loft_rows(cuirass_profile),
        "brigandine", collection, "brigandine_shell", segments=12,
    )
    skin_to_armature(shell, armature)
    chest_plate = add_panel(
        "GEO_CuirassBrigandineFront",
        [(-0.190, -0.220, 1.565), (0.190, -0.220, 1.565),
         (0.170, -0.230, 1.405), (0.108, -0.238, 1.350),
         (-0.108, -0.238, 1.350), (-0.170, -0.230, 1.405)],
        "brigandine_edge", collection, "brigandine_facing", thickness=0.026,
    )
    parent_to_bone(chest_plate, armature, "chest")


    fauld = add_panel(
        "GEO_CuirassFauld",
        [(-0.170, -0.257, 1.380), (0.170, -0.257, 1.380),
         (0.140, -0.260, 1.255), (-0.140, -0.260, 1.255)],
        "steel_dark", collection, "plackart_lame", thickness=0.030,
    )
    parent_to_bone(fauld, armature, "chest")
    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        upper_trim = add_box_between(
            f"GEO_CuirassUpperTrim{suffix}",
            (0.0, -0.258, 1.535), (sign * 0.210, -0.252, 1.565),
            0.022, 0.016, "leather", collection, "armor_trim",
            bevel_width=0.007,
        )
        parent_to_bone(upper_trim, armature, "chest")
    lower_trim = add_box_between("GEO_CuirassLowerTrim",
                                 (-0.142, -0.282, 1.245),
                                 (0.142, -0.282, 1.245), 0.022, 0.016,
                                 "steel", collection, "armor_trim",
                                 bevel_width=0.007)
    parent_to_bone(lower_trim, armature, "chest")
    emblem = add_box_between(
        "GEO_CuirassBrokenMarkL", (-0.018, -0.306, 1.475),
        (-0.070, -0.306, 1.405), 0.024, 0.016, "brass", collection,
        "armor_emblem", bevel_width=0.004,
    )
    parent_to_bone(emblem, armature, "chest")
    return collection


def build_pauldron(parent: bpy.types.Collection, armature: bpy.types.Object,
                    suffix: str, sign: float) -> bpy.types.Collection:
    collection = new_component(
        f"CC_HERO_PAULDRON_{suffix}", parent,
        component_id=f"hero_pauldron_{suffix.lower()}_v01",
        kind="rigid_armor", slot=f"shoulder_{suffix.lower()}",
        layer="rigid_armor", anchor=f"upper_arm.{suffix}",
        coverage=(f"shoulder_{suffix.lower()}",),
    )

    scale = 1.12 if suffix == "L" else 0.62
    plate = add_ico(f"GEO_Pauldron{suffix}", (sign * 0.330, 0.0, 1.565),
                    (0.150 * scale, 0.128 * scale, 0.085 * scale), "steel", collection,
                    "pauldron_shell", subdivisions=1)
    plate.rotation_euler[1] = sign * math.radians(12)
    parent_to_bone(plate, armature, f"upper_arm.{suffix}")
    if suffix == "L":
        waymark = add_box_between(
            "GEO_PauldronWaymarkL", (-0.285, -0.126, 1.590),
            (-0.350, -0.137, 1.545), 0.018, 0.012, "brass", collection,
            "wayfarer_mark", bevel_width=0.004,
        )
        parent_to_bone(waymark, armature, "upper_arm.L")
    return collection


def build_bracer(parent: bpy.types.Collection, armature: bpy.types.Object,
                  suffix: str, sign: float) -> bpy.types.Collection:
    collection = new_component(
        f"CC_HERO_BRACER_{suffix}", parent,
        component_id=f"hero_bracer_{suffix.lower()}_v01",
        kind="rigid_armor", slot=f"forearm_{suffix.lower()}",
        layer="rigid_armor", anchor=f"forearm.{suffix}",
        coverage=(f"forearm_{suffix.lower()}",),
    )
    start = (sign * 0.49, -0.012, 1.19)
    end = (sign * 0.56, -0.020, 0.98)
    bracer_profile = clip_profile(
        derive_shell(HERO_BODY_PROFILES["forearm"], FITTED_BRACER),
        0.10, 0.90, normalize=True,
    )
    bracer = add_limb_loft(
        f"GEO_Bracer{suffix}", start, end,
        sweep_rows(bracer_profile),
        "leather", collection, "fitted_bracer", segments=12,
    )
    parent_to_bone(bracer, armature, f"forearm.{suffix}")
    splint = add_box_between(f"GEO_BracerSplint{suffix}",
                             (sign * 0.495, -0.088, 1.175),
                             (sign * 0.558, -0.082, 0.995),
                             0.040, 0.026, "steel_light", collection,
                             "bracer_splint", bevel_width=0.007)
    parent_to_bone(splint, armature, f"forearm.{suffix}")
    for amount in (0.50,):
        center = Vector(start).lerp(Vector(end), amount)
        band = add_torus(f"GEO_BracerBand{suffix}", tuple(center), 0.078, 0.012,
                         "leather_light", collection, "bracer_band",
                         rotation=(math.radians(72), 0.0, sign * math.radians(18)),
                         scale=(1.0, 0.82, 1.0))
        parent_to_bone(band, armature, f"forearm.{suffix}")
    return collection


def build_greave(parent: bpy.types.Collection, armature: bpy.types.Object,
                  suffix: str, sign: float) -> bpy.types.Collection:
    collection = new_component(
        f"CC_HERO_GREAVE_{suffix}", parent,
        component_id=f"hero_greave_{suffix.lower()}_v01",
        kind="rigid_armor", slot=f"shin_{suffix.lower()}",
        layer="rigid_armor", anchor=f"shin.{suffix}",
        coverage=(f"shin_{suffix.lower()}", "knee"),
    )
    greave_profile = clip_profile(
        derive_shell(HERO_BODY_PROFILES["shin"], FITTED_GREAVE),
        0.07, 0.84, normalize=True,
    )
    greave = add_limb_loft(
        f"GEO_Greave{suffix}", (sign * 0.15, 0.0, 0.53),
        (sign * 0.15, -0.01, 0.20),
        sweep_rows(greave_profile),
        "steel_dark", collection, "fitted_greave", segments=12,
    )
    parent_to_bone(greave, armature, f"shin.{suffix}")
    knee = add_ico(f"GEO_KneePlate{suffix}", (sign * 0.15, -0.075, 0.56),
                   (0.078, 0.040, 0.066), "steel", collection,
                   "knee_plate", subdivisions=2)
    parent_to_bone(knee, armature, f"shin.{suffix}")
    return collection


def build_glove(parent: bpy.types.Collection, armature: bpy.types.Object,
                 suffix: str, sign: float) -> bpy.types.Collection:
    collection = new_component(
        f"CC_HERO_GLOVE_{suffix}", parent,
        component_id=f"hero_glove_{suffix.lower()}_v01",
        kind="accessory", slot=f"hand_{suffix.lower()}", layer="accessory",
        anchor=f"hand.{suffix}", coverage=(f"hand_{suffix.lower()}",),
    )
    hand_scale = WAYFARER_RECIPE.body.hand_scale
    glove = add_ico(f"GEO_Glove{suffix}", (sign * 0.58, -0.045, 0.855),
                    (0.061 * hand_scale, 0.050 * hand_scale,
                     0.083 * hand_scale), "leather", collection,
                    "fist_palm", subdivisions=2)
    parent_to_bone(glove, armature, f"hand.{suffix}")
    thumb = add_ico(f"GEO_GloveThumb{suffix}",
                    (sign * 0.545, -0.070, 0.830),
                    (0.025 * hand_scale, 0.029 * hand_scale,
                     0.042 * hand_scale), "leather_light", collection,
                    "fist_thumb", subdivisions=2)
    parent_to_bone(thumb, armature, f"hand.{suffix}")
    cuff = add_torus(f"GEO_GloveCuff{suffix}", (sign * 0.565, -0.015, 0.94),
                     0.062 * hand_scale, 0.010, "leather_light", collection,
                     "glove_cuff",
                     rotation=(math.radians(72), 0.0, sign * math.radians(18)),
                     scale=(1.0, 0.82, 1.0))
    parent_to_bone(cuff, armature, f"hand.{suffix}")
    return collection


def build_boot(parent: bpy.types.Collection, armature: bpy.types.Object,
                suffix: str, sign: float) -> bpy.types.Collection:
    collection = new_component(
        f"CC_HERO_BOOT_{suffix}", parent,
        component_id=f"hero_boot_{suffix.lower()}_v01",
        kind="accessory", slot=f"foot_{suffix.lower()}", layer="accessory",
        anchor=f"foot.{suffix}", coverage=(f"foot_{suffix.lower()}", "ankle"),
    )
    foot_scale = WAYFARER_RECIPE.body.foot_scale
    boot = add_boot_loft(f"GEO_Boot{suffix}", sign * 0.15,
                         "leather", collection, "tapered_boot_shell",
                         scale=foot_scale)
    parent_to_bone(boot, armature, f"foot.{suffix}")
    cuff = add_limb_loft(
        f"GEO_BootCuff{suffix}", (sign * 0.15, 0.0, 0.30),
        (sign * 0.15, 0.0, 0.15),
        [(0.0, 0.092 * foot_scale, 0.082 * foot_scale),
         (0.55, 0.102 * foot_scale, 0.088 * foot_scale),
         (1.0, 0.095 * foot_scale, 0.082 * foot_scale)],
        "leather", collection, "fitted_boot_cuff", segments=12,
    )
    parent_to_bone(cuff, armature, f"foot.{suffix}")
    sole = add_boot_sole(f"GEO_BootSole{suffix}", sign * 0.15,
                         "padding_dark", collection, "fitted_boot_sole",
                         scale=foot_scale)
    parent_to_bone(sole, armature, f"foot.{suffix}")
    return collection


def build_belt_satchel(parent: bpy.types.Collection,
                       armature: bpy.types.Object) -> bpy.types.Collection:
    collection = new_component(
        "CC_HERO_BELT_SATCHEL", parent,
        component_id="hero_belt_satchel_v01", kind="accessory",
        slot="belt", layer="accessory", anchor="pelvis",
        coverage=("waist", "right_hip", "chest_strap"),
    )
    for name, location, dimensions in (
        ("Front", (0.0, -0.165, 1.02), (0.43, 0.050, 0.065)),
        ("Back", (0.0, 0.165, 1.02), (0.43, 0.050, 0.065)),
        ("Left", (-0.230, 0.0, 1.02), (0.050, 0.29, 0.065)),
        ("Right", (0.230, 0.0, 1.02), (0.050, 0.29, 0.065)),
    ):
        piece = add_cube(f"GEO_Belt{name}", location, dimensions, "leather",
                         collection, "belt", bevel_width=0.018)
        parent_to_bone(piece, armature, "pelvis")
    buckle = add_torus("GEO_BeltBuckle", (0.0, -0.205, 1.02), 0.045, 0.010,
                       "brass", collection, "buckle",
                       rotation=(math.radians(90), 0.0, 0.0),
                       scale=(1.25, 1.0, 0.85))
    parent_to_bone(buckle, armature, "pelvis")
    strap = add_panel(
        "GEO_SatchelStrap",
        [(-0.310, -0.270, 1.585), (-0.255, -0.270, 1.600),
         (0.352, -0.270, 0.945), (0.302, -0.270, 0.905)],
        "leather", collection, "satchel_strap", thickness=0.020,
    )
    parent_to_bone(strap, armature, "chest")
    satchel = add_cube("GEO_Satchel", (0.315, -0.15, 0.885),
                       (0.180, 0.082, 0.185), "leather", collection,
                       "satchel", bevel_width=0.032)
    parent_to_bone(satchel, armature, "pelvis")
    flap = add_panel(
        "GEO_SatchelFlap",
        [(0.232, -0.205, 0.955), (0.398, -0.205, 0.955),
         (0.386, -0.207, 0.895), (0.244, -0.207, 0.895)],
        "leather_light", collection, "satchel_flap", thickness=0.026,
    )
    parent_to_bone(flap, armature, "pelvis")
    waymark = add_torus("GEO_SatchelWaymark", (0.315, -0.228, 0.910),
                        0.022, 0.006, "brass", collection, "travel_token",
                        rotation=(math.radians(90), 0.0, 0.0),
                        scale=(1.0, 1.0, 0.82))
    parent_to_bone(waymark, armature, "pelvis")
    return collection


def add_presentation(groups: dict[str, bpy.types.Collection]) -> None:
    collection = groups["presentation"]
    camera_data = bpy.data.cameras.new("CAM_HeroIsometric")
    camera = bpy.data.objects.new("CAM_HeroIsometric", camera_data)
    collection.objects.link(camera)
    camera.data.type = "ORTHO"
    bpy.context.scene.camera = camera

    key_data = bpy.data.lights.new("KEY_Hero", "AREA")
    key_data.energy = 900
    key_data.shape = "DISK"
    key_data.size = 4.5
    key_data.color = (1.0, 0.82, 0.68)
    key = bpy.data.objects.new("KEY_Hero", key_data)
    key.location = (-3.5, -4.5, 6.0)
    collection.objects.link(key)
    point_at(key, Vector((0.0, 0.0, 1.1)))

    rim_data = bpy.data.lights.new("RIM_Hero", "AREA")
    rim_data.energy = 700
    rim_data.size = 3.5
    rim_data.color = (0.48, 0.68, 1.0)
    rim = bpy.data.objects.new("RIM_Hero", rim_data)
    rim.location = (4.0, 3.0, 4.5)
    collection.objects.link(rim)
    point_at(rim, Vector((0.0, 0.0, 1.2)))

    fill_data = bpy.data.lights.new("FILL_Hero", "AREA")
    fill_data.energy = 280
    fill_data.size = 5.0
    fill_data.color = (0.72, 0.88, 1.0)
    fill = bpy.data.objects.new("FILL_Hero", fill_data)
    fill.location = (2.5, -4.0, 2.2)
    collection.objects.link(fill)
    point_at(fill, Vector((0.0, 0.0, 1.15)))

    stage = add_cube("STAGE_HeroGround", (0.0, 0.0, -0.08),
                     (6.0, 5.0, 0.14), "stage", collection, "presentation_ground",
                     bevel_width=0.10)
    stage["cc_export"] = False


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def collection_bounds(collection: bpy.types.Collection) -> tuple[Vector, Vector]:
    bpy.context.view_layer.update()
    points: list[Vector] = []
    for obj in collection.all_objects:
        if obj.type != "MESH":
            continue
        points.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    if not points:
        return Vector((-0.1, -0.1, -0.1)), Vector((0.1, 0.1, 0.1))
    return (
        Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))),
        Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points))),
    )


def duplicate_component_to(
    source: bpy.types.Collection,
    destination: bpy.types.Collection,
    target_center: tuple[float, float, float],
    *,
    material_override: str | None = None,
) -> None:
    minimum, maximum = collection_bounds(source)
    center = (minimum + maximum) * 0.5
    offset = Vector(target_center) - center
    for obj in source.all_objects:
        if obj.type != "MESH":
            continue
        duplicate = obj.copy()
        duplicate.data = obj.data.copy()
        world = obj.matrix_world.copy()
        duplicate.parent = None
        duplicate.parent_type = "OBJECT"
        duplicate.matrix_world = world
        duplicate.matrix_world.translation += offset
        duplicate.name = f"EXP_{source.name}_{obj.name}"
        destination.objects.link(duplicate)
        for modifier in tuple(duplicate.modifiers):
            if modifier.type in {"ARMATURE", "CLOTH"}:
                duplicate.modifiers.remove(modifier)
        if material_override:
            duplicate.data.materials.clear()
            duplicate.data.materials.append(MATERIALS[material_override])
        duplicate["cc_presentation_only"] = True


def build_exploded_display(groups: dict[str, bpy.types.Collection]) -> None:
    destination = groups["exploded"]
    duplicate_component_to(COMPONENTS["CC_HERO_BODY_BASE"], destination,
                           (0.0, 0.0, 1.10), material_override="ghost")
    targets = {
        "CC_HERO_HAIR": (0.0, 0.0, 2.55),
        "CC_HERO_CAPE": (-1.75, 0.0, 1.75),
        "CC_HERO_TUNIC": (-1.75, 0.0, 1.05),
        "CC_HERO_PADDED_UNDERLAYER": (-1.75, 0.0, 0.35),
        "CC_HERO_CUIRASS": (1.65, 0.0, 1.48),
        "CC_HERO_PAULDRON_L": (0.95, 0.0, 2.15),
        "CC_HERO_PAULDRON_R": (2.35, 0.0, 2.15),
        "CC_HERO_BRACER_L": (0.95, 0.0, 1.22),
        "CC_HERO_BRACER_R": (2.35, 0.0, 1.22),
        "CC_HERO_GLOVE_L": (0.95, 0.0, 0.75),
        "CC_HERO_GLOVE_R": (2.35, 0.0, 0.75),
        "CC_HERO_BELT_SATCHEL": (0.0, 0.0, 0.50),
        "CC_HERO_GREAVE_L": (1.15, 0.0, 0.18),
        "CC_HERO_GREAVE_R": (2.15, 0.0, 0.18),
        "CC_HERO_BOOT_L": (1.15, 0.0, -0.28),
        "CC_HERO_BOOT_R": (2.15, 0.0, -0.28),
    }
    for name, target in targets.items():
        duplicate_component_to(COMPONENTS[name], destination, target)

    cape_min, cape_max = collection_bounds(COMPONENTS["CC_HERO_CAPE"])
    cape_center = (cape_min + cape_max) * 0.5
    cage_min, cage_max = collection_bounds(groups["cape_guides"])
    cage_center = (cage_min + cage_max) * 0.5
    cage_target = Vector(targets["CC_HERO_CAPE"]) + (cage_center - cape_center)
    duplicate_component_to(groups["cape_guides"], destination, tuple(cage_target))

    for name, location in (
        ("EXP_SOCKET_HEAD", (0.0, -0.10, 2.15)),
        ("EXP_SOCKET_SHOULDER_L", (-0.35, -0.10, 1.62)),
        ("EXP_SOCKET_SHOULDER_R", (0.35, -0.10, 1.62)),
        ("EXP_SOCKET_BELT", (0.0, -0.10, 1.03)),
        ("EXP_SOCKET_SHIN_L", (-0.15, -0.10, 0.35)),
        ("EXP_SOCKET_SHIN_R", (0.15, -0.10, 0.35)),
    ):
        add_ico(name, location, (0.040, 0.040, 0.040), "rig_cyan",
                destination, "socket_marker", subdivisions=1)


def set_render_visibility(
    visible_components: set[str],
    groups: dict[str, bpy.types.Collection],
    *,
    show_rig: bool = False,
    show_cape_cage: bool = False,
    show_exploded: bool = False,
) -> None:
    for name, collection in COMPONENTS.items():
        collection.hide_render = name not in visible_components
    groups["rig_guides"].hide_render = not show_rig
    groups["cape_guides"].hide_render = not show_cape_cage
    groups["exploded"].hide_render = not show_exploded


def render_preview(
    filename: str,
    visible_components: set[str],
    groups: dict[str, bpy.types.Collection],
    *,
    show_rig: bool = False,
    show_cape_cage: bool = False,
    show_exploded: bool = False,
    camera_location: tuple[float, float, float] = (3.4, -5.3, 3.0),
    target: tuple[float, float, float] = (0.0, 0.0, 1.05),
    ortho_scale: float = 2.65,
    resolution: tuple[int, int] = (900, 900),
) -> None:
    set_render_visibility(visible_components, groups, show_rig=show_rig,
                          show_cape_cage=show_cape_cage,
                          show_exploded=show_exploded)
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = resolution
    camera = bpy.data.objects["CAM_HeroIsometric"]
    camera.location = camera_location
    camera.data.ortho_scale = ortho_scale
    point_at(camera, Vector(target))
    stage = bpy.data.objects["STAGE_HeroGround"]
    if show_exploded:
        stage.dimensions = (7.4, 5.2, 0.14)
        stage.location = (0.20, 0.0, -0.58)
    else:
        stage.dimensions = (6.0, 5.0, 0.14)
        stage.location = (0.0, 0.0, -0.08)
    scene.render.filepath = str(PREVIEW_DIR / filename)
    bpy.ops.render.render(write_still=True)


def export_objects(objects: Iterable[bpy.types.Object], filepath: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    selected = [obj for obj in objects if obj.type in {"MESH", "EMPTY", "ARMATURE"}]
    for obj in selected:
        obj.hide_set(False)
        obj.select_set(True)
    if selected:
        bpy.context.view_layer.objects.active = selected[0]
    bpy.ops.export_scene.gltf(
        filepath=str(filepath), export_format="GLB", use_selection=True,
        export_apply=True, export_extras=True, export_yup=True,
        export_materials="EXPORT",
    )


def export_components(armature: bpy.types.Object) -> None:
    for record in COMPONENT_RECORDS:
        collection = COMPONENTS[str(record["collection"])]
        objects = list(collection.all_objects)
        if armature not in objects:
            objects.append(armature)
        export_objects(objects, EXPORT_DIR / f"{record['id']}.glb")
    assembled: list[bpy.types.Object] = [armature]
    for name in ASSEMBLED_COMPONENTS:
        assembled.extend(COMPONENTS[name].all_objects)
    export_objects(dict.fromkeys(assembled), EXPORT_DIR / "hero_wayfarer_assembled_v01.glb")


def write_manifest() -> None:
    manifest = {
        "schema_version": 1,
        "library_version": LIBRARY_VERSION,
        "art_direction": ART_DIRECTION,
        "source": "blender/crownless_hero_components.blend",
        "coordinate_system": {"unit": "meter", "forward": "-Y", "up": "+Z"},
        "procedural_generation": WAYFARER_RECIPE.to_manifest(),
        "skeleton": {
            "id": "crownless_humanoid_v01",
            "bones": [name for name, _parent, _head, _tail in BONES],
            "cloth_bones": [name for name, _parent, _head, _tail in CAPE_BONES],
        },
        "sockets": SOCKET_RECORDS,
        "components": sorted(COMPONENT_RECORDS, key=lambda item: str(item["id"])),
        "assemblies": {
            "wayfarer_prototype_v01": {
                "components": sorted(
                    COMPONENTS[name]["cc_component_id"] for name in ASSEMBLED_COMPONENTS
                ),
                "export": "exports/hero_glb/hero_wayfarer_assembled_v01.glb",
            }
        },
        "review_previews": {
            "assembled": "previews/hero/hero_assembled.png",
            "gameplay_read": "previews/hero/hero_gameplay_read.png",
            "component_triptych": "previews/hero/hero_component_triptych.png",
        },
        "cloth": {
            "hero_cape_v01": {
                "pin_group": "PIN_COLLAR",
                "control_grid": [6, 7],
                "simulation_authority": "presentation_only",
            }
        },
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def find_layer_collection(layer_collection: bpy.types.LayerCollection,
                          name: str) -> bpy.types.LayerCollection | None:
    if layer_collection.name == name:
        return layer_collection
    for child in layer_collection.children:
        found = find_layer_collection(child, name)
        if found:
            return found
    return None


def create_view_layers(groups: dict[str, bpy.types.Collection]) -> None:
    scene = bpy.context.scene
    default = scene.view_layers[0]
    default.name = "CC_Hero_Assembled"
    presets = {
        "CC_Hero_Assembled": ASSEMBLED_COMPONENTS,
        "CC_Hero_Anatomy": {"CC_HERO_BODY_BASE", "CC_HERO_MUSCLE_GUIDES"},
        "CC_Hero_Exploded": set(),
    }
    for name in presets:
        if name != default.name:
            scene.view_layers.new(name=name)
    for name, included in presets.items():
        layer = scene.view_layers[name]
        for component_name in COMPONENTS:
            layer_collection = find_layer_collection(layer.layer_collection,
                                                     component_name)
            if layer_collection:
                layer_collection.exclude = component_name not in included
        rig = find_layer_collection(layer.layer_collection, groups["rig_guides"].name)
        cage = find_layer_collection(layer.layer_collection, groups["cape_guides"].name)
        exploded = find_layer_collection(layer.layer_collection, groups["exploded"].name)
        if rig:
            rig.exclude = name != "CC_Hero_Anatomy"
        if cage:
            cage.exclude = name != "CC_Hero_Exploded"
        if exploded:
            exploded.exclude = name != "CC_Hero_Exploded"


def validate(armature: bpy.types.Object) -> None:
    expected_bones = {
        name for name, _parent, _head, _tail in BONES + CAPE_BONES
    }
    actual_bones = {bone.name for bone in armature.data.bones}
    if expected_bones != actual_bones:
        raise RuntimeError(f"Skeleton mismatch: {sorted(expected_bones ^ actual_bones)}")
    if len(COMPONENTS) != 19:
        raise RuntimeError(f"Expected 19 components, found {len(COMPONENTS)}")
    ids = [str(record["id"]) for record in COMPONENT_RECORDS]
    if len(ids) != len(set(ids)):
        raise RuntimeError("Duplicate component IDs")
    for name, collection in COMPONENTS.items():
        renderables = [obj for obj in collection.all_objects
                       if obj.type in {"MESH", "ARMATURE"}]
        if not renderables:
            raise RuntimeError(f"{name} has no renderable/exportable objects")
    cape = bpy.data.objects.get("GEO_HeroCape")
    if cape is None or cape.vertex_groups.get("PIN_COLLAR") is None:
        raise RuntimeError("Cape pin group is missing")
    if not any(modifier.type == "CLOTH" for modifier in cape.modifiers):
        raise RuntimeError("Cape cloth modifier is missing")


def main() -> None:
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    reset_scene()
    make_palette()
    groups = build_structure()
    armature = build_armature(groups["rig"])
    build_sockets(armature, COMPONENTS["CC_HERO_SKELETON"])
    build_rig_guides(groups)
    build_body(groups["anatomy"], armature)
    build_muscles(groups["anatomy"], armature)
    build_hair(groups["accessories"], armature)
    build_padding(groups["garments"], armature)
    build_tunic(groups["garments"], armature)
    build_cape(groups["cloth"], armature, groups["cape_guides"])
    build_cuirass(groups["armor"], armature)
    build_pauldron(groups["armor"], armature, "L", -1.0)
    build_pauldron(groups["armor"], armature, "R", 1.0)
    build_bracer(groups["armor"], armature, "L", -1.0)
    build_bracer(groups["armor"], armature, "R", 1.0)
    build_greave(groups["armor"], armature, "L", -1.0)
    build_greave(groups["armor"], armature, "R", 1.0)
    build_glove(groups["accessories"], armature, "L", -1.0)
    build_glove(groups["accessories"], armature, "R", 1.0)
    build_boot(groups["accessories"], armature, "L", -1.0)
    build_boot(groups["accessories"], armature, "R", 1.0)
    build_belt_satchel(groups["accessories"], armature)
    add_presentation(groups)
    bpy.context.view_layer.update()
    build_exploded_display(groups)
    validate(armature)
    export_components(armature)
    write_manifest()

    render_preview(
        "hero_assembled.png", ASSEMBLED_COMPONENTS, groups,
        camera_location=(3.3, -5.3, 2.85), target=(0.0, 0.0, 1.05),
        ortho_scale=2.55,
    )
    render_preview(
        "hero_gameplay_read.png", ASSEMBLED_COMPONENTS, groups,
        camera_location=(3.3, -5.3, 2.85), target=(0.0, 0.0, 1.05),
        ortho_scale=4.80, resolution=(320, 320),
    )
    render_preview(
        "hero_anatomy.png", {"CC_HERO_BODY_BASE", "CC_HERO_MUSCLE_GUIDES"},
        groups, show_rig=True,
        camera_location=(3.3, -5.3, 2.85), target=(0.0, 0.0, 1.05),
        ortho_scale=2.55,
    )
    render_preview(
        "hero_exploded.png", set(), groups, show_exploded=True,
        camera_location=(5.2, -12.0, 4.8), target=(0.15, 0.0, 1.02),
        ortho_scale=5.00, resolution=(1400, 900),
    )

    create_view_layers(groups)
    set_render_visibility(ASSEMBLED_COMPONENTS, groups)
    bpy.data.objects["STAGE_HeroGround"].location = (0.0, 0.0, -0.08)
    bpy.data.objects["STAGE_HeroGround"].dimensions = (6.0, 5.0, 0.14)
    bpy.context.scene.render.filepath = "//../previews/hero/hero_assembled.png"
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH), compress=True)
    print(f"Built {len(COMPONENTS)} hero components at {BLEND_PATH}")


if __name__ == "__main__":
    main()
