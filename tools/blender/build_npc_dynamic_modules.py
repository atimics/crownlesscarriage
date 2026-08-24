#!/usr/bin/env python3
"""Build rigid NPC modules for physics-driven humanoids.

The runtime attaches these unskinned GLBs to the authoritative biomechanical
skeleton one bone at a time.  This preserves contacts, climbing, combat, and
ragdolls without a CPU skin/deformed-vertex upload for every dynamic actor.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
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
LIBRARY_VERSION = "0.2.0"
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
    vertex_color = material.node_tree.nodes.new("ShaderNodeVertexColor")
    vertex_color.layer_name = "COLOR_0"
    material.node_tree.links.new(
        vertex_color.outputs["Color"], principled.inputs["Base Color"])
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
    """Create a +Z unit module from (height, x radius, y radius) rings."""
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


def build_geometry(slot: str, collection: bpy.types.Collection,
                   material: bpy.types.Material) -> None:
    if slot == "torso":
        add_loft("GEO_ModuleTorso", (
            (0.00, 0.36, 0.30), (0.36, 0.43, 0.34),
            (0.78, 0.53, 0.36), (1.00, 0.48, 0.32),
        ), collection, material, sides=10)
    elif slot == "pelvis":
        add_loft("GEO_ModulePelvis", (
            (0.00, 0.44, 0.34), (0.50, 0.50, 0.37),
            (1.00, 0.43, 0.32),
        ), collection, material, sides=10)
    elif slot == "upper_arm":
        add_loft("GEO_ModuleUpperArm", (
            (0.00, 0.54, 0.50), (0.28, 0.61, 0.57),
            (0.72, 0.48, 0.46), (1.00, 0.38, 0.37),
        ), collection, material)
    elif slot == "forearm":
        add_loft("GEO_ModuleForearm", (
            (0.00, 0.44, 0.42), (0.32, 0.56, 0.50),
            (0.72, 0.43, 0.39), (1.00, 0.31, 0.29),
        ), collection, material)
    elif slot == "thigh":
        add_loft("GEO_ModuleThigh", (
            (0.00, 0.55, 0.51), (0.28, 0.66, 0.58),
            (0.70, 0.52, 0.47), (1.00, 0.39, 0.37),
        ), collection, material, sides=9)
    elif slot == "shin":
        add_loft("GEO_ModuleShin", (
            (0.00, 0.43, 0.40), (0.26, 0.53, 0.47),
            (0.58, 0.47, 0.44), (1.00, 0.31, 0.29),
        ), collection, material, sides=9)
    elif slot == "hand":
        add_ico("GEO_ModuleHand", (0.0, 0.0, 0.0),
                (0.50, 0.42, 0.58), collection, material, subdivisions=1)
    elif slot == "foot":
        # The runtime maps local +Y to ankle->toe, making this a rigid boot.
        add_box("GEO_ModuleFoot", (0.0, -0.02, 0.48),
                (0.78, 0.74, 1.08), collection, material, width=0.10)
        add_box("GEO_ModuleFootSole", (0.0, -0.22, 0.53),
                (0.84, 0.23, 1.15), collection, material, width=0.05)
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
    elif slot == "coat_tail":
        add_box("GEO_ModuleCoatTail", (0.0, 0.0, -0.50),
                (0.78, 0.20, 1.0), collection, material, width=0.08,
                rotation=(0.05, 0.0, 0.0))
    elif slot == "chest_plate":
        add_box("GEO_ModuleChestPlate", (0.0, 0.0, 0.0),
                (1.0, 0.22, 1.0), collection, material, width=0.12)
        add_box("GEO_ModuleChestRidge", (0.0, -0.16, 0.10),
                (0.16, 0.10, 0.78), collection, material, width=0.035)
    elif slot == "pauldron":
        add_ico("GEO_ModulePauldron", (0.0, 0.0, 0.0),
                (0.58, 0.48, 0.50), collection, material, subdivisions=1)
        add_box("GEO_ModulePauldronLip", (0.0, -0.26, -0.18),
                (0.94, 0.14, 0.22), collection, material, width=0.05)
    elif slot == "apron":
        add_panel("GEO_ModuleApron", (
            (-0.48, 0.0, 0.45), (0.48, 0.0, 0.45),
            (0.42, 0.04, -0.55), (-0.42, 0.04, -0.55),
        ), collection, material, thickness=0.05)
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
        elif style == 3:
            add_box("GEO_ModuleHairSweep", (-0.18, -0.32, 0.28),
                    (0.70, 0.28, 0.42), collection, material, width=0.10,
                    rotation=(0.0, -0.18, -0.12))
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
                palette_slot: str) -> bpy.types.Object:
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
    if not objects:
        raise RuntimeError(f"{asset_id} generated no mesh")
    for obj in objects:
        apply_modifiers(obj)
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
    semantic_index = PALETTE_INDEX[palette_slot]
    paint_channels.add_indexed_paint_channels(
        joined, [semantic_index] * len(joined.data.polygons),
        PAINT_SEMANTICS)
    for polygon in joined.data.polygons:
        polygon.material_index = 0
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    joined["cc_asset_id"] = asset_id
    joined["cc_rigid_module"] = True
    joined["cc_material_contract"] = "COLOR_0:palette,value,fold"
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
        export_morph=False, export_extras=True, export_materials="EXPORT")
    model.select_set(False)
    model.hide_render = True
    model.hide_set(True)
    return path


def build() -> None:
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
        ("mantle", "mantle", "back_socket", "outer_shadow"),
        ("coat_tail", "coat_tail", "pelvis", "outer_shadow"),
        ("chest_plate", "chest_plate", "chest_front_socket", "metal"),
        ("pauldron", "pauldron", "shoulder_socket", "metal"),
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
        collection = collection_for(asset_id)
        build_geometry(slot, collection, material)
        model = consolidate(collection, asset_id, material, palette)
        path = export_model(model, asset_id)
        records.append(ModuleRecord(
            id=asset_id, slot=slot, anchor=anchor, material=palette,
            export=str(path.relative_to(ROOT))))
    manifest = {
        "library_version": LIBRARY_VERSION,
        "generation": "offline procedural rigid character modules",
        "runtime_strategy": "bone-frame instancing without skins or animations",
        "coordinate_system": "glTF +Y up, +Z forward",
        "material_contract": "single indexed material; COLOR_0 stores palette, value, and fold",
        "modules": [asdict(record) for record in records],
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8")
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    print(f"built {len(records)} rigid NPC modules")
    print(f"manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    build()
