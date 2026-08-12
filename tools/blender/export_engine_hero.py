#!/usr/bin/env python3
"""Export the modular Crownless hero as a rigid-weighted engine skin."""

from __future__ import annotations

import json
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[2]
EXPORT_PATH = ROOT / "assets" / "exports" / "hero" / "crownless_hero_engine_rig_v01.glb"
MANIFEST_PATH = EXPORT_PATH.with_suffix(".json")
COMPONENT_MANIFEST_PATH = ROOT / "assets" / "hero_component_manifest.json"
RIG_NAME = "ARM_CrownlessHero"
EXCLUDED_COMPONENTS = {"action_prop", "presentation", "weapon", "guide"}


def reset_rig(rig: bpy.types.Object) -> None:
    rig.animation_data_clear()
    rig.data.pose_position = "REST"
    for bone in rig.pose.bones:
        bone.location = (0.0, 0.0, 0.0)
        bone.rotation_mode = "QUATERNION"
        bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        bone.scale = (1.0, 1.0, 1.0)
    bpy.context.view_layer.update()


def apply_shape_modifiers(obj: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.hide_viewport = False
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for modifier in tuple(obj.modifiers):
        if modifier.type == "ARMATURE":
            continue
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def duplicate_component(source: bpy.types.Object, rig: bpy.types.Object,
                        collection: bpy.types.Collection) -> bpy.types.Object:
    smooth = bool(source.get("cc_smooth_skin"))
    bone_name = ("smooth" if smooth else
                 source.parent_bone if source.parent_type == "BONE" else "root")
    world = source.matrix_world.copy()
    duplicate = source.copy()
    duplicate.data = source.data.copy()
    duplicate.animation_data_clear()
    duplicate.name = f"SKIN_{source.name}"
    collection.objects.link(duplicate)
    duplicate.parent = None
    duplicate.parent_type = "OBJECT"
    duplicate.parent_bone = ""
    duplicate.matrix_world = world
    apply_shape_modifiers(duplicate)

    if smooth:
        armatures = [modifier for modifier in duplicate.modifiers
                     if modifier.type == "ARMATURE"]
        if not armatures:
            raise RuntimeError(f"smooth mesh {source.name} has no armature modifier")
        for armature in armatures:
            armature.object = rig
    else:
        duplicate.vertex_groups.clear()
        group = duplicate.vertex_groups.new(name=bone_name)
        group.add(tuple(range(len(duplicate.data.vertices))), 1.0, "REPLACE")
        armature = duplicate.modifiers.new("CC_EngineSkin", "ARMATURE")
        armature.object = rig
    skinned_world = duplicate.matrix_world.copy()
    duplicate.parent = rig
    duplicate.matrix_parent_inverse = rig.matrix_world.inverted()
    duplicate.matrix_world = skinned_world
    duplicate["cc_engine_bone"] = bone_name
    return duplicate


def export() -> None:
    export_layer = bpy.context.scene.view_layers.get("CC_EngineExport")
    if export_layer is None:
        export_layer = bpy.context.scene.view_layers.new(name="CC_EngineExport")
    bpy.context.window.view_layer = export_layer
    rig = bpy.data.objects.get(RIG_NAME)
    if rig is None or rig.type != "ARMATURE":
        raise RuntimeError(f"missing armature {RIG_NAME}")
    reset_rig(rig)
    component_manifest = json.loads(COMPONENT_MANIFEST_PATH.read_text())
    assembled_ids = set(
        component_manifest["assemblies"]["wayfarer_prototype_v01"]["components"]
    )

    export_collection = bpy.data.collections.new("CC_ENGINE_EXPORT")
    bpy.context.scene.collection.children.link(export_collection)
    components: list[bpy.types.Object] = []
    source_objects = tuple(bpy.context.scene.objects)
    for source in source_objects:
        component = source.get("cc_component_id", source.get("cc_component"))
        if source.type != "MESH" or component is None:
            continue
        if (component in EXCLUDED_COMPONENTS or
                component not in assembled_ids or
                source.name.startswith(("ACTION_", "EXP_"))):
            continue
        components.append(duplicate_component(source, rig, export_collection))

    if not components:
        raise RuntimeError("no modular hero components were found")

    EXPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    rig.hide_set(False)
    rig.hide_viewport = False
    rig.select_set(True)
    for component in components:
        component.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.gltf(
        filepath=str(EXPORT_PATH),
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_animations=False,
        export_skins=True,
        export_morph=False,
        export_extras=True,
        export_materials="EXPORT",
    )

    manifest = {
        "asset": str(EXPORT_PATH.relative_to(ROOT)),
        "armature": RIG_NAME,
        "coordinate_system": "glTF +Y up, +Z forward",
        "motion_source": "CcHumanoidSkinPoseResolve (runtime only)",
        "bones": [bone.name for bone in rig.data.bones],
        "components": [
            {
                "name": component.name,
                "component": component.get(
                    "cc_component_id", component.get("cc_component")
                ),
                "bone": component.get("cc_engine_bone"),
            }
            for component in components
        ],
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"exported {len(components)} modular meshes to {EXPORT_PATH}")


if __name__ == "__main__":
    export()
