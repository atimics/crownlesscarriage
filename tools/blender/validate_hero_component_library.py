#!/usr/bin/env python3
"""Validate the generated modular hero Blender library."""

from __future__ import annotations

import json
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "assets" / "hero_component_manifest.json"


def fail(message: str) -> None:
    raise RuntimeError(f"Hero component validation failed: {message}")


manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
if manifest["library_version"] != bpy.context.scene.get("cc_library_version"):
    fail("manifest and blend versions differ")

armature = bpy.data.objects.get("ARM_CrownlessHero")
if armature is None or armature.type != "ARMATURE":
    fail("canonical armature is missing")
expected_bones = set(manifest["skeleton"]["bones"])
expected_bones.update(manifest["skeleton"].get("cloth_bones", []))
actual_bones = {bone.name for bone in armature.data.bones}
if expected_bones != actual_bones:
    fail(f"bone mismatch: {sorted(expected_bones ^ actual_bones)}")

component_ids: set[str] = set()
for component in manifest["components"]:
    component_id = component["id"]
    if component_id in component_ids:
        fail(f"duplicate component id {component_id}")
    component_ids.add(component_id)
    collection = bpy.data.collections.get(component["collection"])
    if collection is None:
        fail(f"missing collection {component['collection']}")
    if collection.get("cc_component_id") != component_id:
        fail(f"component metadata mismatch for {component_id}")
    export = ROOT / "assets" / component["export"]
    if not export.exists() or export.stat().st_size < 1024:
        fail(f"missing export {export.name}")

for socket in manifest["sockets"]:
    obj = bpy.data.objects.get(socket["name"])
    if obj is None or obj.type != "EMPTY":
        fail(f"missing socket {socket['name']}")
    if obj.parent_bone != socket["bone"]:
        fail(f"socket bone mismatch for {socket['name']}")

cape = bpy.data.objects.get("GEO_HeroCape")
if cape is None or cape.vertex_groups.get("PIN_COLLAR") is None:
    fail("cape pin group is missing")
if not any(modifier.type == "CLOTH" for modifier in cape.modifiers):
    fail("cape cloth modifier is missing")
if not cape.get("cc_smooth_skin"):
    fail("cape runtime skin metadata is missing")
for bone_name in manifest["skeleton"].get("cloth_bones", []):
    if cape.vertex_groups.get(bone_name) is None:
        fail(f"cape weight group {bone_name} is missing")

required_layers = {"CC_Hero_Assembled", "CC_Hero_Anatomy", "CC_Hero_Exploded"}
actual_layers = {layer.name for layer in bpy.context.scene.view_layers}
if required_layers - actual_layers:
    fail(f"missing view layers {sorted(required_layers - actual_layers)}")

assembly_export = ROOT / "assets" / manifest["assemblies"]["wayfarer_prototype_v01"]["export"]
if not assembly_export.exists() or assembly_export.stat().st_size < 1024:
    fail("assembled hero export is missing")

print(
    f"Validated {len(component_ids)} hero components, {len(expected_bones)} bones, "
    f"{len(manifest['sockets'])} sockets, cape cloth, and 3 view layers."
)
