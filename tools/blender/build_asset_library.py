#!/usr/bin/env python3
"""Build the Crownless Carriage modular Blender asset starter library.

Run with:
    blender --background --factory-startup --python tools/blender/build_asset_library.py

The script is intentionally self-contained. It creates source collections, view
layer presets, GLB exports, a machine-readable manifest, and preview renders.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_asset_library.blend"
EXPORT_DIR = ROOT / "assets" / "exports" / "glb"
PREVIEW_DIR = ROOT / "assets" / "previews"
MANIFEST_PATH = ROOT / "assets" / "asset_manifest.json"
LIBRARY_VERSION = "0.1.0"

MATERIALS: dict[str, bpy.types.Material] = {}
LEAF_COLLECTIONS: dict[str, bpy.types.Collection] = {}
ASSET_RECORDS: list[dict[str, object]] = []


PALETTE = {
    "wood_dark": (0.17, 0.075, 0.035, 1.0),
    "wood": (0.38, 0.16, 0.065, 1.0),
    "wood_light": (0.60, 0.31, 0.12, 1.0),
    "iron": (0.075, 0.09, 0.10, 1.0),
    "steel": (0.25, 0.31, 0.32, 1.0),
    "brass": (0.66, 0.39, 0.085, 1.0),
    "leather": (0.26, 0.09, 0.045, 1.0),
    "canvas": (0.72, 0.62, 0.43, 1.0),
    "cream": (0.84, 0.76, 0.58, 1.0),
    "red": (0.47, 0.055, 0.045, 1.0),
    "blue": (0.055, 0.20, 0.29, 1.0),
    "teal": (0.055, 0.31, 0.28, 1.0),
    "purple": (0.29, 0.12, 0.35, 1.0),
    "green": (0.14, 0.31, 0.12, 1.0),
    "grass": (0.21, 0.32, 0.13, 1.0),
    "dry_grass": (0.40, 0.36, 0.17, 1.0),
    "road": (0.30, 0.25, 0.18, 1.0),
    "stone": (0.36, 0.37, 0.34, 1.0),
    "stone_light": (0.52, 0.50, 0.43, 1.0),
    "stone_dark": (0.18, 0.20, 0.19, 1.0),
    "moss": (0.16, 0.27, 0.10, 1.0),
    "water": (0.025, 0.16, 0.20, 1.0),
    "water_light": (0.08, 0.34, 0.38, 1.0),
    "cloth_white": (0.72, 0.73, 0.67, 1.0),
    "glass": (0.035, 0.12, 0.16, 1.0),
    "food": (0.55, 0.23, 0.055, 1.0),
    "black": (0.012, 0.014, 0.015, 1.0),
}


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_ASSET_LIBRARY"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 768
    scene.render.resolution_y = 768
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.resolution_percentage = 100
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene.unit_settings.scale_length = 1.0
    scene["cc_library_version"] = LIBRARY_VERSION
    scene["cc_forward_axis"] = "+X"
    scene["cc_up_axis"] = "+Z"
    scene["cc_unit"] = "meter"

    world = bpy.data.worlds.new("CC_World")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.055, 0.067, 0.075, 1.0)
    background.inputs["Strength"].default_value = 0.55
    scene.world = world


def material(
    name: str,
    color: tuple[float, float, float, float],
    *,
    metallic: float = 0.0,
    roughness: float = 0.65,
) -> bpy.types.Material:
    if name in MATERIALS:
        return MATERIALS[name]
    mat = bpy.data.materials.new(name=f"MAT_{name.upper()}")
    mat.diffuse_color = color
    mat.use_nodes = True
    principled = mat.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = color
    principled.inputs["Metallic"].default_value = metallic
    principled.inputs["Roughness"].default_value = roughness
    MATERIALS[name] = mat
    return mat


def make_palette() -> None:
    for name, color in PALETTE.items():
        metallic = 0.75 if name in {"iron", "steel", "brass"} else 0.0
        roughness = 0.3 if metallic else 0.68
        if name == "glass":
            roughness = 0.22
            metallic = 0.1
        elif name in {"water", "water_light"}:
            roughness = 0.18
            metallic = 0.08
        material(name, color, metallic=metallic, roughness=roughness)


def new_collection(
    name: str,
    parent: bpy.types.Collection,
    *,
    asset_id: str | None = None,
    kind: str | None = None,
    layer_group: str | None = None,
    sockets: tuple[str, ...] = (),
) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    parent.children.link(collection)
    if asset_id:
        collection["cc_asset_id"] = asset_id
        collection["cc_asset_kind"] = kind or "unspecified"
        collection["cc_layer_group"] = layer_group or "unspecified"
        collection["cc_library_version"] = LIBRARY_VERSION
        collection["cc_compatible_sockets"] = ",".join(sockets)
        LEAF_COLLECTIONS[name] = collection
        ASSET_RECORDS.append(
            {
                "id": asset_id,
                "collection": name,
                "kind": kind,
                "layer_group": layer_group,
                "compatible_sockets": list(sockets),
                "export": f"exports/glb/{asset_id}.glb",
            }
        )
    return collection


def move_to_collection(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)


def tag_object(
    obj: bpy.types.Object,
    *,
    asset_id: str,
    role: str,
    layer_group: str,
) -> None:
    obj["cc_asset_id"] = asset_id
    obj["cc_role"] = role
    obj["cc_layer_group"] = layer_group
    obj["cc_library_version"] = LIBRARY_VERSION


def bevel(obj: bpy.types.Object, width: float = 0.05, segments: int = 2) -> None:
    modifier = obj.modifiers.new(name="CC_Bevel", type="BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"


def add_cube(
    name: str,
    location: tuple[float, float, float],
    dimensions: tuple[float, float, float],
    mat: str,
    collection: bpy.types.Collection,
    asset_id: str,
    role: str,
    *,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    bevel_width: float = 0.04,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel_width:
        bevel(obj, min(bevel_width, min(dimensions) * 0.25))
    obj.data.materials.append(MATERIALS[mat])
    move_to_collection(obj, collection)
    tag_object(obj, asset_id=asset_id, role=role, layer_group=collection.get("cc_layer_group", "presentation"))
    return obj


def add_cylinder(
    name: str,
    location: tuple[float, float, float],
    radius: float,
    depth: float,
    mat: str,
    collection: bpy.types.Collection,
    asset_id: str,
    role: str,
    *,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    vertices: int = 16,
    bevel_width: float = 0.025,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices,
        radius=radius,
        depth=depth,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    if bevel_width:
        bevel(obj, bevel_width, 2)
    obj.data.materials.append(MATERIALS[mat])
    move_to_collection(obj, collection)
    tag_object(obj, asset_id=asset_id, role=role, layer_group=collection.get("cc_layer_group", "presentation"))
    return obj


def add_torus(
    name: str,
    location: tuple[float, float, float],
    major_radius: float,
    minor_radius: float,
    mat: str,
    collection: bpy.types.Collection,
    asset_id: str,
    role: str,
    *,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
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
    obj.data.materials.append(MATERIALS[mat])
    move_to_collection(obj, collection)
    tag_object(obj, asset_id=asset_id, role=role, layer_group=collection.get("cc_layer_group", "presentation"))
    return obj


def add_uv_sphere(
    name: str,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
    mat: str,
    collection: bpy.types.Collection,
    asset_id: str,
    role: str,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(MATERIALS[mat])
    move_to_collection(obj, collection)
    tag_object(obj, asset_id=asset_id, role=role, layer_group=collection.get("cc_layer_group", "presentation"))
    return obj


def add_empty(
    name: str,
    location: tuple[float, float, float],
    collection: bpy.types.Collection,
    socket_type: str,
) -> bpy.types.Object:
    obj = bpy.data.objects.new(name, None)
    obj.empty_display_type = "ARROWS"
    obj.empty_display_size = 0.26
    obj.location = location
    obj["cc_socket_type"] = socket_type
    obj["cc_forward_axis"] = "+X"
    collection.objects.link(obj)
    return obj


def add_beam_between(
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    width: float,
    mat: str,
    collection: bpy.types.Collection,
    asset_id: str,
    role: str,
) -> bpy.types.Object:
    a = Vector(start)
    b = Vector(end)
    delta = b - a
    midpoint = (a + b) * 0.5
    obj = add_cube(
        name,
        tuple(midpoint),
        (width, width, delta.length),
        mat,
        collection,
        asset_id,
        role,
        bevel_width=width * 0.18,
    )
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = delta.to_track_quat("Z", "Y")
    return obj


def add_wheel(
    prefix: str,
    x: float,
    y: float,
    z: float,
    collection: bpy.types.Collection,
    asset_id: str,
    radius: float = 0.72,
) -> None:
    add_torus(
        f"GEO_{prefix}_Rim",
        (x, y, z),
        radius - 0.075,
        0.075,
        "wood_dark",
        collection,
        asset_id,
        "wheel_rim",
        rotation=(math.radians(90), 0.0, 0.0),
    )
    add_cylinder(
        f"GEO_{prefix}_Hub",
        (x, y, z),
        0.14,
        0.26,
        "brass",
        collection,
        asset_id,
        "wheel_hub",
        rotation=(math.radians(90), 0.0, 0.0),
        vertices=12,
    )
    for index in range(8):
        angle = index * math.tau / 8.0
        dx = math.cos(angle) * radius * 0.45
        dz = math.sin(angle) * radius * 0.45
        add_beam_between(
            f"GEO_{prefix}_Spoke_{index:02d}",
            (x, y, z),
            (x + dx * 1.9, y, z + dz * 1.9),
            0.045,
            "wood_light",
            collection,
            asset_id,
            "wheel_spoke",
        )


def build_carriage_base(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "carriage_base_v01"
    col = new_collection(
        "CC_CARRIAGE_BASE",
        parent,
        asset_id=asset_id,
        kind="vehicle_core",
        layer_group="carriage_core",
        sockets=("roof", "rear", "side_left", "side_right", "underbody", "interior"),
    )

    add_cube("GEO_Chassis", (0.0, 0.0, 0.88), (3.9, 1.45, 0.18), "iron", col, asset_id, "chassis")
    add_cube("GEO_Deck", (-0.15, 0.0, 1.06), (3.35, 1.72, 0.18), "wood_dark", col, asset_id, "deck")
    add_cube("GEO_Cabin", (-0.3, 0.0, 1.55), (2.75, 1.68, 0.95), "wood", col, asset_id, "body", bevel_width=0.12)
    add_cube("GEO_Roof", (-0.3, 0.0, 2.16), (2.98, 1.92, 0.25), "canvas", col, asset_id, "roof", bevel_width=0.12)
    add_cube("GEO_DriverDeck", (1.55, 0.0, 1.12), (0.85, 1.62, 0.18), "wood_dark", col, asset_id, "driver_deck")
    add_cube("GEO_DriverSeat", (1.45, 0.0, 1.45), (0.34, 1.3, 0.28), "leather", col, asset_id, "driver_seat", bevel_width=0.08)
    add_cube("GEO_DriverBack", (1.20, 0.0, 1.72), (0.15, 1.3, 0.65), "leather", col, asset_id, "driver_seat")

    for y in (-0.855, 0.855):
        add_cube(
            f"GEO_Window_{'L' if y < 0 else 'R'}",
            (-0.35, y, 1.69),
            (1.25, 0.035, 0.43),
            "glass",
            col,
            asset_id,
            "window",
            bevel_width=0.035,
        )
        add_cube(
            f"GEO_WindowFrame_{'L' if y < 0 else 'R'}",
            (-0.35, y * 1.008, 1.69),
            (1.40, 0.05, 0.06),
            "brass",
            col,
            asset_id,
            "window_frame",
        )

    for x in (-1.22, 1.05):
        for y in (-0.94, 0.94):
            add_wheel(f"Wheel_{x:+.0f}_{y:+.0f}", x, y, 0.72, col, asset_id)

    add_cube("GEO_FrontAxle", (1.05, 0.0, 0.75), (0.16, 2.05, 0.16), "iron", col, asset_id, "axle")
    add_cube("GEO_RearAxle", (-1.22, 0.0, 0.75), (0.16, 2.05, 0.16), "iron", col, asset_id, "axle")
    add_beam_between("GEO_HitchLeft", (1.65, -0.43, 0.95), (3.35, -0.28, 0.72), 0.10, "wood_dark", col, asset_id, "hitch")
    add_beam_between("GEO_HitchRight", (1.65, 0.43, 0.95), (3.35, 0.28, 0.72), 0.10, "wood_dark", col, asset_id, "hitch")
    add_cube("GEO_HitchBar", (3.36, 0.0, 0.72), (0.14, 0.72, 0.14), "iron", col, asset_id, "hitch")

    for y in (-0.77, 0.77):
        add_cube("GEO_LanternBracket", (1.58, y, 1.72), (0.10, 0.10, 0.42), "iron", col, asset_id, "lantern")
        add_cube("GEO_Lantern", (1.58, y, 1.95), (0.18, 0.18, 0.28), "brass", col, asset_id, "lantern", bevel_width=0.04)
        add_cube("GEO_LanternGlass", (1.58, y, 1.96), (0.12, 0.12, 0.15), "cream", col, asset_id, "lantern_glass", bevel_width=0.025)

    sockets = {
        "SOCKET_Roof": ((-0.3, 0.0, 2.34), "roof"),
        "SOCKET_Rear": ((-1.82, 0.0, 1.28), "rear"),
        "SOCKET_SideLeft": ((-0.3, -0.97, 1.42), "side_left"),
        "SOCKET_SideRight": ((-0.3, 0.97, 1.42), "side_right"),
        "SOCKET_Underbody": ((-0.15, 0.0, 0.74), "underbody"),
        "SOCKET_Interior": ((-0.3, 0.0, 1.52), "interior"),
    }
    for name, (location, socket_type) in sockets.items():
        add_empty(name, location, col, socket_type)
    return col


def build_cargo_module(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_cargo_rack_v01"
    col = new_collection("CC_MOD_CARGO_RACK", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("roof",))
    for y in (-0.68, 0.68):
        add_cube("GEO_CargoRail", (-0.3, y, 2.52), (2.45, 0.07, 0.18), "iron", col, asset_id, "rack_rail")
    for x in (-1.25, -0.65, -0.05, 0.55):
        add_cube("GEO_CargoSlat", (x, 0.0, 2.43), (0.08, 1.42, 0.10), "wood_light", col, asset_id, "rack_slat")
    add_cube("GEO_CargoCrate_A", (-0.72, -0.30, 2.76), (0.72, 0.58, 0.50), "wood_light", col, asset_id, "cargo")
    add_cube("GEO_CargoCrate_B", (0.10, 0.28, 2.72), (0.62, 0.52, 0.42), "wood", col, asset_id, "cargo")
    add_uv_sphere("GEO_CargoRoll", (0.68, -0.27, 2.71), (0.38, 0.25, 0.22), "canvas", col, asset_id, "cargo")
    return col


def build_armour_module(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_armoured_body_v01"
    col = new_collection("CC_MOD_ARMOURED_BODY", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("side_left", "side_right"))
    for y in (-0.895, 0.895):
        side = "L" if y < 0 else "R"
        add_cube(f"GEO_Armour_{side}_A", (-0.88, y, 1.34), (1.02, 0.09, 0.52), "steel", col, asset_id, "armour_panel", bevel_width=0.025)
        add_cube(f"GEO_Armour_{side}_B", (0.28, y, 1.34), (1.02, 0.09, 0.52), "steel", col, asset_id, "armour_panel", bevel_width=0.025)
        for x in (-1.30, -0.46, 0.70):
            add_cylinder(f"GEO_Rivet_{side}", (x, y * 1.006, 1.34), 0.035, 0.035, "brass", col, asset_id, "rivet", rotation=(math.radians(90), 0.0, 0.0), vertices=8, bevel_width=0.0)
    add_cube("GEO_ArmourFront", (1.10, 0.0, 1.40), (0.10, 1.48, 0.58), "steel", col, asset_id, "armour_panel")
    return col


def build_medical_module(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_medical_bunk_v01"
    col = new_collection("CC_MOD_MEDICAL_BUNK", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("interior", "rear"))
    add_cube("GEO_MedicalBunk", (-0.55, 0.0, 1.38), (1.72, 0.76, 0.16), "cloth_white", col, asset_id, "bunk", bevel_width=0.06)
    add_cube("GEO_MedicalBlanket", (-0.62, 0.0, 1.49), (1.05, 0.69, 0.08), "teal", col, asset_id, "blanket", bevel_width=0.035)
    add_cube("GEO_MedicalCabinet", (-1.55, 0.0, 1.44), (0.42, 0.70, 0.66), "cream", col, asset_id, "medical_storage", bevel_width=0.05)
    add_cube("GEO_MedicalMark_V", (-1.765, 0.0, 1.50), (0.035, 0.32, 0.09), "red", col, asset_id, "medical_mark")
    add_cube("GEO_MedicalMark_H", (-1.77, 0.0, 1.50), (0.035, 0.09, 0.32), "red", col, asset_id, "medical_mark")
    return col


def build_hidden_compartment(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_hidden_compartment_v01"
    col = new_collection("CC_MOD_HIDDEN_COMPARTMENT", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("underbody",))
    add_cube("GEO_HiddenBox", (-0.18, 0.0, 0.68), (1.52, 0.82, 0.28), "wood_dark", col, asset_id, "hidden_compartment", bevel_width=0.045)
    add_cube("GEO_HiddenLatch", (-0.18, -0.42, 0.68), (0.28, 0.035, 0.12), "iron", col, asset_id, "hidden_latch", bevel_width=0.02)
    return col


def build_scout_perch(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_scout_perch_v01"
    col = new_collection("CC_MOD_SCOUT_PERCH", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("roof",))
    add_cube("GEO_ScoutPlatform", (-0.35, 0.0, 2.50), (0.82, 0.82, 0.12), "wood_light", col, asset_id, "platform")
    for x in (-0.70, 0.0):
        for y in (-0.35, 0.35):
            add_cube("GEO_ScoutPost", (x, y, 2.80), (0.055, 0.055, 0.62), "iron", col, asset_id, "guard_rail")
    for y in (-0.35, 0.35):
        add_cube("GEO_ScoutRail", (-0.35, y, 3.08), (0.78, 0.055, 0.055), "iron", col, asset_id, "guard_rail")
    add_cube("GEO_ScoutSeat", (-0.35, 0.0, 2.75), (0.42, 0.48, 0.18), "leather", col, asset_id, "seat", bevel_width=0.05)
    return col


def build_monster_cage(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_monster_cage_v01"
    col = new_collection("CC_MOD_MONSTER_CAGE", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("rear",))
    cx = -2.04
    add_cube("GEO_CageFloor", (cx, 0.0, 1.02), (0.74, 1.28, 0.12), "iron", col, asset_id, "cage_floor")
    for x in (cx - 0.31, cx + 0.31):
        for y in (-0.56, -0.28, 0.0, 0.28, 0.56):
            add_cube("GEO_CageBar", (x, y, 1.62), (0.055, 0.055, 1.12), "steel", col, asset_id, "cage_bar", bevel_width=0.015)
    for y in (-0.59, 0.59):
        for x in (cx - 0.25, cx, cx + 0.25):
            add_cube("GEO_CageBar", (x, y, 1.62), (0.055, 0.055, 1.12), "steel", col, asset_id, "cage_bar", bevel_width=0.015)
    add_cube("GEO_CageRoof", (cx, 0.0, 2.18), (0.74, 1.28, 0.12), "iron", col, asset_id, "cage_roof")
    return col


def build_relic_module(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_relic_containment_v01"
    col = new_collection("CC_MOD_RELIC_CONTAINMENT", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("rear", "interior"))
    add_cube("GEO_RelicCase", (-1.88, 0.0, 1.26), (0.74, 1.10, 0.72), "purple", col, asset_id, "containment_case", bevel_width=0.08)
    for z in (1.00, 1.52):
        add_cube("GEO_RelicBand", (-1.88, 0.0, z), (0.78, 1.14, 0.08), "brass", col, asset_id, "containment_band", bevel_width=0.025)
    add_uv_sphere("GEO_RelicSeal", (-2.27, 0.0, 1.27), (0.045, 0.19, 0.19), "brass", col, asset_id, "containment_seal")
    return col


def build_document_safe(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_document_safe_v01"
    col = new_collection("CC_MOD_DOCUMENT_SAFE", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("side_right", "interior"))
    add_cube("GEO_DocumentSafe", (-0.75, 0.83, 1.38), (0.68, 0.16, 0.54), "blue", col, asset_id, "document_safe", bevel_width=0.055)
    add_cylinder("GEO_DocumentDial", (-0.75, 0.93, 1.38), 0.11, 0.06, "brass", col, asset_id, "safe_dial", rotation=(math.radians(90), 0.0, 0.0), vertices=12)
    return col


def build_passenger_bench(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "module_passenger_bench_v01"
    col = new_collection("CC_MOD_PASSENGER_BENCH", parent, asset_id=asset_id, kind="carriage_module", layer_group="carriage_module", sockets=("interior",))
    add_cube("GEO_PassengerBench", (-0.35, 0.0, 1.33), (1.74, 0.74, 0.20), "leather", col, asset_id, "passenger_bench", bevel_width=0.07)
    add_cube("GEO_PassengerBack", (-0.35, 0.30, 1.64), (1.74, 0.16, 0.64), "leather", col, asset_id, "passenger_bench", bevel_width=0.07)
    return col


def build_road_kit(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "environment_road_straight_v01"
    col = new_collection("CC_ENV_ROAD_STRAIGHT", parent, asset_id=asset_id, kind="environment_kit", layer_group="environment_base")
    add_cube("GEO_RoadGround", (0.0, 0.0, -0.12), (7.0, 6.0, 0.22), "grass", col, asset_id, "terrain", bevel_width=0.10)
    add_cube("GEO_RoadBed", (0.0, 0.0, 0.01), (7.0, 2.45, 0.16), "road", col, asset_id, "road", bevel_width=0.12)
    for x, y, size in ((-2.5, -2.0, 0.32), (2.2, 1.9, 0.26), (1.4, -2.1, 0.18), (-1.0, 2.25, 0.22)):
        add_uv_sphere("GEO_RoadRock", (x, y, size * 0.45), (size, size * 0.75, size * 0.55), "stone", col, asset_id, "rock")
    add_cube("GEO_SignPost", (1.9, -1.70, 0.76), (0.13, 0.13, 1.52), "wood_dark", col, asset_id, "signpost")
    add_cube("GEO_SignBoard", (1.72, -1.70, 1.34), (0.72, 0.12, 0.30), "wood_light", col, asset_id, "signpost", rotation=(0.0, math.radians(-5), 0.0))
    return col


def build_bridge_checkpoint(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "environment_bridge_checkpoint_v01"
    col = new_collection("CC_ENV_BRIDGE_CHECKPOINT", parent, asset_id=asset_id, kind="environment_kit", layer_group="environment_base")

    # Establish the crossing before adding checkpoint furniture. The water and
    # raised banks make the bridge legible from an isometric gameplay camera
    # instead of reading as an isolated stone tray.
    add_cube("GEO_River", (0.0, 0.0, -0.15), (8.8, 6.6, 0.10), "water", col, asset_id, "water", bevel_width=0.04)
    for index, (x, y, length, angle) in enumerate(((-2.45, -2.42, 1.35, 4), (1.48, -2.68, 1.75, -3), (-1.25, 2.62, 1.25, -5), (2.62, 2.30, 1.48, 3))):
        add_cube(f"GEO_WaterRipple_{index:02d}", (x, y, -0.092), (length, 0.045, 0.018), "water_light", col, asset_id, "water_ripple", rotation=(0.0, 0.0, math.radians(angle)), bevel_width=0.012)
    for x in (-4.22, 4.22):
        add_cube("GEO_RiverBank", (x, 0.0, -0.17), (0.92, 6.6, 0.24), "grass", col, asset_id, "terrain", bevel_width=0.12)
        add_cube("GEO_Causeway", (x, 0.0, 0.00), (0.96, 2.38, 0.20), "road", col, asset_id, "road", bevel_width=0.09)

    add_cube("GEO_BridgeDeck", (0.0, 0.0, 0.30), (7.6, 2.55, 0.52), "stone_light", col, asset_id, "bridge_deck", bevel_width=0.12)
    for x in (-3.15, -2.10, -1.05, 0.0, 1.05, 2.10, 3.15):
        add_cube("GEO_DeckJoint", (x, 0.0, 0.57), (0.035, 2.18, 0.022), "stone_dark", col, asset_id, "masonry_joint", bevel_width=0.0)

    for y in (-1.40, 1.40):
        side = "L" if y < 0.0 else "R"
        add_cube(f"GEO_BridgeParapet_{side}", (0.0, y, 0.74), (7.6, 0.30, 0.76), "stone", col, asset_id, "parapet", bevel_width=0.07)
        add_cube(f"GEO_ParapetCap_{side}", (0.0, y, 1.15), (7.82, 0.38, 0.16), "stone_light", col, asset_id, "parapet_cap", bevel_width=0.055)
        for x in (-3.15, -1.60, 0.0, 1.60, 3.15):
            add_cube("GEO_ParapetButtress", (x, y * 1.075, 0.66), (0.24, 0.22, 0.84), "stone_dark", col, asset_id, "bridge_buttress", bevel_width=0.035)

    # Bridge supports remain visible through the river from the presentation
    # angle and give the span useful vertical rhythm.
    for x in (-2.45, 0.0, 2.45):
        for y in (-0.88, 0.88):
            add_cylinder("GEO_BridgePier", (x, y, -0.02), 0.30, 0.72, "stone_dark", col, asset_id, "bridge_support", vertices=8, bevel_width=0.04)

    # A compact timber toll house with a pitched roof, foundation, door, and
    # glazed side window. It intentionally sits outside the travel lane.
    add_cube("GEO_CheckpointFoundation", (-1.45, 2.12, 0.25), (1.72, 1.36, 0.50), "stone_dark", col, asset_id, "guard_hut_foundation", bevel_width=0.06)
    add_cube("GEO_CheckpointHut", (-1.45, 2.12, 1.34), (1.50, 1.18, 1.72), "wood", col, asset_id, "guard_hut", bevel_width=0.08)
    for x in (-2.13, -0.77):
        add_cube("GEO_CheckpointCornerPost", (x, 2.12, 1.40), (0.13, 1.24, 1.84), "wood_dark", col, asset_id, "guard_hut_frame", bevel_width=0.025)
    add_cube("GEO_CheckpointDoor", (-1.45, 1.515, 1.06), (0.62, 0.065, 1.16), "wood_dark", col, asset_id, "guard_hut_door", bevel_width=0.035)
    add_torus("GEO_CheckpointDoorRing", (-1.22, 1.47, 1.09), 0.075, 0.014, "brass", col, asset_id, "door_hardware", rotation=(math.radians(90), 0.0, 0.0))
    add_cube("GEO_CheckpointWindow", (-0.685, 2.12, 1.50), (0.045, 0.52, 0.48), "glass", col, asset_id, "guard_hut_window", bevel_width=0.018)
    for y, pitch in ((1.79, math.radians(27)), (2.45, math.radians(-27))):
        add_cube("GEO_CheckpointRoofSlope", (-1.45, y, 2.40), (1.90, 0.86, 0.16), "red", col, asset_id, "guard_hut_roof", rotation=(pitch, 0.0, 0.0), bevel_width=0.045)
    add_cube("GEO_CheckpointChimney", (-1.88, 2.39, 2.65), (0.24, 0.24, 0.62), "stone_dark", col, asset_id, "guard_hut_chimney", bevel_width=0.035)
    add_cube("GEO_CheckpointChimneyCap", (-1.88, 2.39, 2.97), (0.32, 0.32, 0.12), "stone_light", col, asset_id, "guard_hut_chimney", bevel_width=0.03)

    # The gate is a high-contrast read from a distance. Alternating pale bands
    # preserve the visual language of a controlled crossing without textures.
    for y in (-1.0, 1.0):
        add_cube("GEO_GatePost", (0.65, y, 1.14), (0.28, 0.28, 2.28), "iron", col, asset_id, "gate", bevel_width=0.035)
        add_cube("GEO_GatePostCap", (0.65, y, 2.32), (0.40, 0.40, 0.18), "brass", col, asset_id, "gate", bevel_width=0.045)
    add_cube("GEO_GateBeam", (0.65, 0.0, 2.12), (0.26, 2.18, 0.24), "iron", col, asset_id, "gate", bevel_width=0.035)
    add_cube("GEO_Barrier", (0.24, 0.0, 1.18), (0.18, 2.12, 0.18), "red", col, asset_id, "barrier", rotation=(math.radians(10), 0.0, 0.0), bevel_width=0.025)
    for y in (-0.68, 0.0, 0.68):
        add_cube("GEO_BarrierStripe", (0.24, y, 1.18 + math.sin(math.radians(10)) * y), (0.195, 0.28, 0.195), "cream", col, asset_id, "barrier_stripe", rotation=(math.radians(10), 0.0, 0.0), bevel_width=0.018)
    add_cube("GEO_GatePlacard", (0.48, 0.0, 2.13), (0.18, 0.72, 0.44), "blue", col, asset_id, "checkpoint_sign", bevel_width=0.045)
    add_cube("GEO_GatePlacardMark", (0.375, 0.0, 2.13), (0.025, 0.38, 0.10), "brass", col, asset_id, "checkpoint_sign", bevel_width=0.008)

    # Inspection furniture and road-side detail support the simulation role of
    # this asset: cargo is stopped, checked, documented, and sometimes seized.
    add_cube("GEO_InspectionTable", (-0.55, -0.72, 0.78), (1.05, 0.55, 0.12), "wood_light", col, asset_id, "inspection_table")
    for x in (-0.98, -0.12):
        for y in (-0.94, -0.50):
            add_cube("GEO_InspectionTableLeg", (x, y, 0.43), (0.09, 0.09, 0.66), "wood_dark", col, asset_id, "inspection_table", bevel_width=0.018)
    add_cube("GEO_InspectionLedger", (-0.58, -0.72, 0.86), (0.34, 0.24, 0.045), "cream", col, asset_id, "inspection_ledger", rotation=(0.0, 0.0, math.radians(8)), bevel_width=0.012)
    add_cube("GEO_InspectionCrate", (-1.13, -0.30, 0.40), (0.56, 0.52, 0.68), "wood", col, asset_id, "inspection_cargo", bevel_width=0.055)

    for index, (x, y, height) in enumerate(((-4.02, -2.25, 0.48), (-4.12, -1.95, 0.62), (4.08, 2.12, 0.55), (4.18, 2.43, 0.42))):
        add_cube(f"GEO_Reed_{index:02d}", (x, y, 0.02), (0.055, 0.055, height), "moss", col, asset_id, "river_growth", rotation=(math.radians(5), math.radians(index * 3 - 4), 0.0), bevel_width=0.01)
    return col


def build_mine_entrance(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "environment_mine_entrance_v01"
    col = new_collection("CC_ENV_MINE_ENTRANCE", parent, asset_id=asset_id, kind="environment_kit", layer_group="environment_base")
    add_cube("GEO_MineGround", (0.0, 0.0, -0.11), (6.4, 5.4, 0.20), "stone_dark", col, asset_id, "terrain", bevel_width=0.10)
    add_cube("GEO_MineCliff", (0.0, 1.70, 1.35), (6.1, 1.30, 2.70), "stone", col, asset_id, "cliff", bevel_width=0.20)
    add_cube("GEO_MineVoid", (0.0, 1.00, 1.00), (1.75, 0.24, 1.90), "black", col, asset_id, "entrance_void", bevel_width=0.18)
    for x in (-1.02, 1.02):
        add_cube("GEO_MineSupport", (x, 0.78, 1.02), (0.22, 0.28, 2.04), "wood_dark", col, asset_id, "timber_support")
    add_cube("GEO_MineLintel", (0.0, 0.78, 1.93), (2.28, 0.28, 0.24), "wood_dark", col, asset_id, "timber_support")
    for y in (-0.28, 0.28):
        add_cube("GEO_Rail", (0.0, y, 0.08), (4.5, 0.075, 0.075), "steel", col, asset_id, "mine_rail")
    for x in (-1.8, -0.9, 0.0, 0.9, 1.8):
        add_cube("GEO_RailTie", (x, 0.0, 0.025), (0.16, 0.78, 0.08), "wood_dark", col, asset_id, "mine_rail")
    add_cube("GEO_MineCart", (-1.15, -0.05, 0.52), (0.92, 0.72, 0.64), "iron", col, asset_id, "mine_cart", bevel_width=0.06)
    for x in (-1.42, -0.88):
        add_cylinder("GEO_CartWheel", (x, -0.42, 0.27), 0.17, 0.10, "steel", col, asset_id, "mine_cart", rotation=(math.radians(90), 0.0, 0.0), vertices=12)
    return col


def build_market(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "environment_market_granary_v01"
    col = new_collection("CC_ENV_MARKET_GRANARY", parent, asset_id=asset_id, kind="environment_kit", layer_group="environment_base")
    add_cube("GEO_MarketGround", (0.0, 0.0, -0.10), (7.0, 6.0, 0.20), "stone", col, asset_id, "market_ground", bevel_width=0.10)
    add_cube("GEO_Granary", (1.55, 1.63, 1.15), (3.15, 1.72, 2.30), "wood", col, asset_id, "granary", bevel_width=0.10)
    add_cube("GEO_GranaryRoof", (1.55, 1.63, 2.40), (3.45, 2.04, 0.25), "red", col, asset_id, "granary_roof", bevel_width=0.10)
    add_cube("GEO_GranaryDoor", (1.55, 0.75, 0.88), (0.90, 0.10, 1.58), "wood_dark", col, asset_id, "granary_door", bevel_width=0.06)
    for x in (-2.25, -0.55):
        add_cube("GEO_StallCounter", (x, -0.35, 0.76), (1.28, 0.72, 0.18), "wood_light", col, asset_id, "market_stall")
        for dx in (-0.52, 0.52):
            add_cube("GEO_StallPost", (x + dx, -0.35, 1.38), (0.10, 0.10, 1.40), "wood_dark", col, asset_id, "market_stall")
        add_cube("GEO_StallAwning", (x, -0.35, 2.04), (1.46, 0.98, 0.14), "canvas", col, asset_id, "market_stall", rotation=(0.0, math.radians(-4), 0.0), bevel_width=0.07)
    add_cube("GEO_WellBase", (-1.15, 1.55, 0.30), (1.18, 1.18, 0.60), "stone_dark", col, asset_id, "well", bevel_width=0.18)
    add_cylinder("GEO_WellOpening", (-1.15, 1.55, 0.63), 0.38, 0.10, "black", col, asset_id, "well", vertices=16, bevel_width=0.02)
    return col


def build_shortage_layer(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "state_food_shortage_v01"
    col = new_collection("CC_STATE_FOOD_SHORTAGE", parent, asset_id=asset_id, kind="state_dressing", layer_group="state_layer")
    for x, y, rot in ((-2.35, -0.46, 0.25), (-0.62, -0.48, -0.18), (0.42, -1.85, 0.12)):
        add_cylinder("GEO_EmptyBasket", (x, y, 0.24), 0.28, 0.36, "wood_light", col, asset_id, "empty_container", rotation=(0.0, rot, 0.0), vertices=12, bevel_width=0.025)
    for x in (0.63, 1.08, 1.53, 1.98):
        add_cube("GEO_RationPost", (x, -0.58, 0.45), (0.08, 0.08, 0.90), "iron", col, asset_id, "queue_control")
    add_cube("GEO_RationRope", (1.30, -0.58, 0.65), (1.46, 0.04, 0.04), "red", col, asset_id, "queue_control")
    add_cube("GEO_GranaryGuardBar", (1.55, 0.58, 1.02), (0.12, 1.56, 0.12), "iron", col, asset_id, "granary_security", rotation=(math.radians(4), 0.0, 0.0))
    add_cube("GEO_RationNotice", (2.25, 0.58, 1.37), (0.08, 0.62, 0.62), "cream", col, asset_id, "notice", bevel_width=0.025)
    return col


def build_security_layer(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "state_harsh_enforcement_v01"
    col = new_collection("CC_STATE_HARSH_ENFORCEMENT", parent, asset_id=asset_id, kind="state_dressing", layer_group="state_layer")
    for x in (-1.15, 0.0, 1.15):
        add_cube("GEO_BarricadeLeg", (x, -1.55, 0.35), (0.18, 0.78, 0.18), "iron", col, asset_id, "barricade")
    add_cube("GEO_BarricadeBeam", (0.0, -1.55, 0.72), (2.72, 0.18, 0.22), "red", col, asset_id, "barricade", rotation=(0.0, math.radians(4), 0.0))
    add_cube("GEO_SearchTable", (0.0, -0.55, 0.72), (1.35, 0.72, 0.16), "wood_dark", col, asset_id, "search_table")
    add_cube("GEO_ConfiscationCrate", (0.92, -0.43, 0.35), (0.62, 0.62, 0.62), "wood_light", col, asset_id, "confiscation")
    return col


def build_recovery_layer(parent: bpy.types.Collection) -> bpy.types.Collection:
    asset_id = "state_market_recovery_v01"
    col = new_collection("CC_STATE_MARKET_RECOVERY", parent, asset_id=asset_id, kind="state_dressing", layer_group="state_layer")
    for x, y, mat in ((-2.45, -0.42, "food"), (-2.05, -0.42, "green"), (-0.72, -0.42, "food"), (-0.30, -0.42, "cream")):
        add_uv_sphere("GEO_Produce", (x, y, 0.93), (0.16, 0.16, 0.12), mat, col, asset_id, "market_stock")
    for x, y in ((1.02, 0.48), (1.58, 0.48), (2.14, 0.48)):
        add_cube("GEO_FullSack", (x, y, 0.38), (0.46, 0.42, 0.72), "canvas", col, asset_id, "market_stock", bevel_width=0.12)
    for index, x in enumerate((-1.75, -0.9, -0.05, 0.8, 1.65)):
        add_uv_sphere("GEO_Bunting", (x, -1.88, 2.18 - (index % 2) * 0.18), (0.13, 0.06, 0.18), "teal" if index % 2 else "cream", col, asset_id, "celebration")
    return col


def build_scene_structure() -> dict[str, bpy.types.Collection]:
    scene_root = bpy.context.scene.collection
    library = bpy.data.collections.new("CC_LIBRARY")
    scene_root.children.link(library)
    guides = bpy.data.collections.new("00_GUIDES")
    core = bpy.data.collections.new("10_CARRIAGE_CORE")
    modules = bpy.data.collections.new("20_CARRIAGE_MODULES")
    environments = bpy.data.collections.new("30_ENVIRONMENT_KITS")
    states = bpy.data.collections.new("40_STATE_LAYERS")
    for collection in (guides, core, modules, environments, states):
        library.children.link(collection)

    staging = bpy.data.collections.new("90_PRESENTATION")
    scene_root.children.link(staging)
    return {"guides": guides, "core": core, "modules": modules, "environments": environments, "states": states, "staging": staging}


def build_assets(groups: dict[str, bpy.types.Collection]) -> None:
    base = build_carriage_base(groups["core"])
    build_cargo_module(groups["modules"])
    build_armour_module(groups["modules"])
    build_medical_module(groups["modules"])
    build_passenger_bench(groups["modules"])
    build_hidden_compartment(groups["modules"])
    build_scout_perch(groups["modules"])
    build_monster_cage(groups["modules"])
    build_relic_module(groups["modules"])
    build_document_safe(groups["modules"])
    build_road_kit(groups["environments"])
    build_bridge_checkpoint(groups["environments"])
    build_mine_entrance(groups["environments"])
    build_market(groups["environments"])
    build_shortage_layer(groups["states"])
    build_security_layer(groups["states"])
    build_recovery_layer(groups["states"])

    # Socket objects live in the base asset for export, and are cross-linked to
    # the guides collection for quick authoring visibility control.
    for obj in base.objects:
        if obj.name.startswith("SOCKET_"):
            groups["guides"].objects.link(obj)


def add_presentation_rig(collection: bpy.types.Collection) -> None:
    scene = bpy.context.scene
    camera_data = bpy.data.cameras.new("CAM_Isometric")
    camera = bpy.data.objects.new("CAM_Isometric", camera_data)
    collection.objects.link(camera)
    scene.camera = camera
    camera.data.type = "ORTHO"
    camera.data.lens = 50

    sun_data = bpy.data.lights.new("KEY_Sun", type="SUN")
    sun_data.energy = 3.0
    sun_data.angle = math.radians(18)
    sun = bpy.data.objects.new("KEY_Sun", sun_data)
    sun.rotation_euler = (math.radians(28), math.radians(-18), math.radians(-38))
    collection.objects.link(sun)

    fill_data = bpy.data.lights.new("FILL_Area", type="AREA")
    fill_data.energy = 850
    fill_data.shape = "DISK"
    fill_data.size = 6.0
    fill = bpy.data.objects.new("FILL_Area", fill_data)
    fill.location = (-4.0, -5.0, 7.0)
    collection.objects.link(fill)
    point_camera(fill, (0.0, 0.0, 1.0), track="-Z", up="Y")

    add_cube("STAGE_Ground", (0.0, 0.0, -0.30), (12.0, 12.0, 0.20), "stone_dark", collection, "presentation_only", "ground", bevel_width=0.18)


def point_camera(
    obj: bpy.types.Object,
    target: tuple[float, float, float],
    *,
    track: str = "-Z",
    up: str = "Y",
) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat(track, up).to_euler()


def find_layer_collection(layer_collection: bpy.types.LayerCollection, name: str) -> bpy.types.LayerCollection | None:
    if layer_collection.name == name:
        return layer_collection
    for child in layer_collection.children:
        found = find_layer_collection(child, name)
        if found:
            return found
    return None


def create_view_layers() -> None:
    scene = bpy.context.scene
    default = scene.view_layers[0]
    default.name = "CC_Carriage_Cargo"
    presets = {
        "CC_Carriage_Cargo": {"CC_CARRIAGE_BASE", "CC_MOD_CARGO_RACK"},
        "CC_Carriage_Armoured": {"CC_CARRIAGE_BASE", "CC_MOD_ARMOURED_BODY"},
        "CC_Carriage_Medical": {"CC_CARRIAGE_BASE", "CC_MOD_MEDICAL_BUNK"},
        "CC_Market_Shortage": {"CC_ENV_MARKET_GRANARY", "CC_STATE_FOOD_SHORTAGE"},
        "CC_Market_Recovery": {"CC_ENV_MARKET_GRANARY", "CC_STATE_MARKET_RECOVERY"},
        "CC_Bridge_Checkpoint": {"CC_ENV_BRIDGE_CHECKPOINT"},
        "CC_Mine_Entrance": {"CC_ENV_MINE_ENTRANCE"},
        "CC_Road": {"CC_ENV_ROAD_STRAIGHT"},
    }
    for name in presets:
        if name != default.name:
            scene.view_layers.new(name=name)
    for name, included in presets.items():
        view_layer = scene.view_layers[name]
        for leaf_name in LEAF_COLLECTIONS:
            layer_col = find_layer_collection(view_layer.layer_collection, leaf_name)
            if layer_col:
                layer_col.exclude = leaf_name not in included
        guides = find_layer_collection(view_layer.layer_collection, "00_GUIDES")
        if guides:
            guides.exclude = True


def visible_assets(names: set[str]) -> None:
    for name, collection in LEAF_COLLECTIONS.items():
        collection.hide_render = name not in names


def render_preview(
    filename: str,
    visible: set[str],
    *,
    camera_location: tuple[float, float, float],
    target: tuple[float, float, float],
    ortho_scale: float,
) -> None:
    scene = bpy.context.scene
    visible_assets(visible)
    camera = bpy.data.objects["CAM_Isometric"]
    camera.location = camera_location
    camera.data.ortho_scale = ortho_scale
    point_camera(camera, target)
    scene.render.filepath = str(PREVIEW_DIR / filename)
    bpy.ops.render.render(write_still=True)


def export_collection(collection: bpy.types.Collection, filepath: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    export_objects = [obj for obj in collection.all_objects if obj.type in {"MESH", "EMPTY"}]
    for obj in export_objects:
        obj.hide_set(False)
        obj.select_set(True)
    if export_objects:
        bpy.context.view_layer.objects.active = next((obj for obj in export_objects if obj.type == "MESH"), export_objects[0])
    bpy.ops.export_scene.gltf(
        filepath=str(filepath),
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_extras=True,
        export_yup=True,
        export_materials="EXPORT",
    )


def write_manifest() -> None:
    sockets = {}
    for obj in bpy.data.objects:
        if obj.name.startswith("SOCKET_"):
            sockets[obj.name] = {
                "type": obj["cc_socket_type"],
                "position_m": [round(value, 4) for value in obj.location],
                "forward": "+X",
                "up": "+Z",
            }
    manifest = {
        "schema_version": 1,
        "library_version": LIBRARY_VERSION,
        "coordinate_system": {"unit": "meter", "forward": "+X", "up": "+Z"},
        "source": "blender/crownless_asset_library.blend",
        "assets": sorted(ASSET_RECORDS, key=lambda item: str(item["id"])),
        "sockets": sockets,
        "state_composition": {
            "market_shortage": ["environment_market_granary_v01", "state_food_shortage_v01"],
            "market_enforced": ["environment_market_granary_v01", "state_harsh_enforcement_v01"],
            "market_recovery": ["environment_market_granary_v01", "state_market_recovery_v01"],
        },
        "carriage_examples": {
            "cargo": ["carriage_base_v01", "module_cargo_rack_v01"],
            "armoured": ["carriage_base_v01", "module_armoured_body_v01"],
            "medical": ["carriage_base_v01", "module_medical_bunk_v01"],
        },
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def validate() -> None:
    required = {
        "CC_CARRIAGE_BASE",
        "CC_MOD_CARGO_RACK",
        "CC_MOD_ARMOURED_BODY",
        "CC_MOD_MEDICAL_BUNK",
        "CC_ENV_ROAD_STRAIGHT",
        "CC_ENV_BRIDGE_CHECKPOINT",
        "CC_ENV_MINE_ENTRANCE",
        "CC_ENV_MARKET_GRANARY",
        "CC_STATE_FOOD_SHORTAGE",
        "CC_STATE_HARSH_ENFORCEMENT",
        "CC_STATE_MARKET_RECOVERY",
    }
    missing = required - set(LEAF_COLLECTIONS)
    if missing:
        raise RuntimeError(f"Missing required asset collections: {sorted(missing)}")
    ids = [record["id"] for record in ASSET_RECORDS]
    if len(ids) != len(set(ids)):
        raise RuntimeError("Duplicate cc_asset_id values detected")
    for name, collection in LEAF_COLLECTIONS.items():
        meshes = [obj for obj in collection.all_objects if obj.type == "MESH"]
        if not meshes:
            raise RuntimeError(f"{name} contains no exportable mesh")
        for obj in meshes:
            if not obj.data.materials:
                raise RuntimeError(f"{obj.name} has no material")
            if "cc_asset_id" not in obj:
                raise RuntimeError(f"{obj.name} is missing cc_asset_id")


def main() -> None:
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    reset_scene()
    make_palette()
    groups = build_scene_structure()
    build_assets(groups)
    add_presentation_rig(groups["staging"])
    validate()

    for record in ASSET_RECORDS:
        collection = LEAF_COLLECTIONS[str(record["collection"])]
        export_collection(collection, EXPORT_DIR / f"{record['id']}.glb")
    write_manifest()

    render_preview(
        "carriage_cargo.png",
        {"CC_CARRIAGE_BASE", "CC_MOD_CARGO_RACK"},
        camera_location=(7.6, -8.8, 6.7),
        target=(0.0, 0.0, 1.25),
        ortho_scale=6.2,
    )
    render_preview(
        "carriage_armoured.png",
        {"CC_CARRIAGE_BASE", "CC_MOD_ARMOURED_BODY"},
        camera_location=(7.6, -8.8, 6.7),
        target=(0.0, 0.0, 1.25),
        ortho_scale=6.2,
    )
    render_preview(
        "market_shortage.png",
        {"CC_ENV_MARKET_GRANARY", "CC_STATE_FOOD_SHORTAGE"},
        camera_location=(8.6, -9.8, 9.0),
        target=(0.0, 0.0, 0.85),
        ortho_scale=8.3,
    )
    render_preview(
        "bridge_checkpoint.png",
        {"CC_ENV_BRIDGE_CHECKPOINT"},
        camera_location=(9.2, -10.6, 8.5),
        target=(0.0, 0.0, 0.70),
        ortho_scale=8.8,
    )
    render_preview(
        "mine_entrance.png",
        {"CC_ENV_MINE_ENTRANCE"},
        camera_location=(8.0, -9.6, 7.4),
        target=(0.0, 0.55, 0.90),
        ortho_scale=7.6,
    )

    create_view_layers()
    visible_assets({"CC_CARRIAGE_BASE", "CC_MOD_CARGO_RACK"})
    bpy.context.scene.render.filepath = "//../previews/carriage_cargo.png"
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH), compress=True)
    print(f"Built {len(ASSET_RECORDS)} assets at {BLEND_PATH}")


if __name__ == "__main__":
    main()
