#!/usr/bin/env python3
"""Add the authored paint-channel contract to the shipped market GLB only."""

from __future__ import annotations

from pathlib import Path
import sys

import bpy


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import build_asset_library as library


ROOT = Path(__file__).resolve().parents[2]
MARKET_PATH = ROOT / "assets" / "exports" / "glb" / \
    "environment_market_granary_v01.glb"
ASSET_ID = "environment_market_granary_v01"


def main() -> None:
    if not MARKET_PATH.is_file():
        raise RuntimeError(f"market export is missing: {MARKET_PATH}")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(MARKET_PATH))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not meshes:
        raise RuntimeError("imported market contains no meshes")
    for obj in meshes:
        if obj.get("cc_asset_id") != ASSET_ID:
            raise RuntimeError(f"{obj.name} carries a foreign asset id")
        role = obj.get("cc_role")
        if not role:
            raise RuntimeError(f"{obj.name} has no cc_role")
        for existing in tuple(obj.data.color_attributes):
            obj.data.color_attributes.remove(existing)
        library.add_painted_environment_channels(obj, str(role))

    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.export_scene.gltf(
        filepath=str(MARKET_PATH),
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_extras=True,
        export_texcoords=False,
        export_materials="EXPORT",
        export_vertex_color="ACTIVE",
    )
    print(f"Painted market pilot exported with COLOR_0 on {len(meshes)} meshes")


if __name__ == "__main__":
    main()
