#!/usr/bin/env python3
"""Validate the generated Crownless Carriage Blender library."""

from __future__ import annotations

import json
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "asset_manifest.json"
EXPORT_DIR = ROOT / "assets" / "exports" / "glb"


def fail(message: str) -> None:
    raise RuntimeError(f"Asset validation failed: {message}")


manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
if manifest["library_version"] != bpy.context.scene.get("cc_library_version"):
    fail("manifest and .blend library versions do not match")

asset_ids: set[str] = set()
for asset in manifest["assets"]:
    asset_id = asset["id"]
    if asset_id in asset_ids:
        fail(f"duplicate asset id {asset_id}")
    asset_ids.add(asset_id)
    collection = bpy.data.collections.get(asset["collection"])
    if collection is None:
        fail(f"missing collection {asset['collection']}")
    if collection.get("cc_asset_id") != asset_id:
        fail(f"collection id mismatch for {asset['collection']}")
    export_path = ROOT / "assets" / asset["export"]
    if not export_path.exists() or export_path.stat().st_size < 1024:
        fail(f"missing or empty export {export_path.name}")
    mesh_count = sum(obj.type == "MESH" for obj in collection.all_objects)
    if mesh_count == 0:
        fail(f"collection {collection.name} has no meshes")

for socket_name, socket in manifest["sockets"].items():
    obj = bpy.data.objects.get(socket_name)
    if obj is None or obj.type != "EMPTY":
        fail(f"missing socket empty {socket_name}")
    if obj.get("cc_socket_type") != socket["type"]:
        fail(f"socket type mismatch for {socket_name}")

expected_layers = {
    "CC_Carriage_Cargo",
    "CC_Carriage_Armoured",
    "CC_Carriage_Medical",
    "CC_Market_Shortage",
    "CC_Market_Recovery",
    "CC_Bridge_Checkpoint",
    "CC_Mine_Entrance",
    "CC_Road",
}
actual_layers = {layer.name for layer in bpy.context.scene.view_layers}
if expected_layers - actual_layers:
    fail(f"missing view layers {sorted(expected_layers - actual_layers)}")

print(
    f"Validated {len(asset_ids)} assets, {len(manifest['sockets'])} sockets, "
    f"and {len(expected_layers)} view-layer presets."
)
