#!/usr/bin/env python3

from __future__ import annotations

import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "asset_manifest.json"
THUMB_DIR = ROOT / "assets" / "previews" / "catalog" / "thumbs"


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def walk_layers(layer_collection: bpy.types.LayerCollection):
    yield layer_collection
    for child in layer_collection.children:
        yield from walk_layers(child)


def asset_bounds(collection_names: set[str]) -> tuple[Vector, Vector]:
    points: list[Vector] = []
    for collection_name in collection_names:
        collection = bpy.data.collections[collection_name]
        for obj in collection.all_objects:
            if obj.type != "MESH":
                continue
            points.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    if not points:
        raise RuntimeError(f"No mesh bounds found for {sorted(collection_names)}")
    minimum = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
    maximum = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
    return minimum, maximum


def render_thumbnail(
    asset_id: str,
    visible_collections: set[str],
    all_asset_collections: set[str],
    render_layer: bpy.types.ViewLayer,
) -> None:
    for collection_name in all_asset_collections:
        bpy.data.collections[collection_name].hide_render = collection_name not in visible_collections




    render_layer.update()
    minimum, maximum = asset_bounds(visible_collections)
    center = (minimum + maximum) * 0.5
    size = maximum - minimum

    ground = bpy.data.objects["STAGE_Ground"]
    ground.location.z = minimum.z - 0.18
    ground.dimensions.x = max(8.0, size.x * 1.8)
    ground.dimensions.y = max(8.0, size.y * 1.8)

    camera = bpy.data.objects["CAM_Isometric"]
    direction = Vector((1.15, -1.35, 1.0)).normalized()
    camera.location = center + direction * 12.0
    camera.data.ortho_scale = max(size.x * 1.30, size.y * 1.35, size.z * 1.75, 1.6)
    point_at(camera, center)
    print(
        f"Framing {asset_id}: center={tuple(round(v, 3) for v in center)} "
        f"size={tuple(round(v, 3) for v in size)} ortho={camera.data.ortho_scale:.3f}"
    )

    scene = bpy.context.scene
    scene.render.filepath = str(THUMB_DIR / f"{asset_id}.png")
    render_layer.update()
    bpy.ops.render.render(write_still=True, layer=render_layer.name)


def main() -> None:
    THUMB_DIR.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    collection_by_id = {asset["id"]: asset["collection"] for asset in manifest["assets"]}
    all_asset_collections = set(collection_by_id.values())

    scene = bpy.context.scene
    scene.render.resolution_x = 512
    scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    catalog_layer = scene.view_layers.get("CC_Catalog_All") or scene.view_layers.new("CC_Catalog_All")
    for layer_collection in walk_layers(catalog_layer.layer_collection):
        layer_collection.exclude = layer_collection.name == "00_GUIDES"
    bpy.context.window.view_layer = catalog_layer

    requested_ids: set[str] | None = None
    if "--" in sys.argv:
        requested_ids = set(sys.argv[sys.argv.index("--") + 1 :])

    state_context = {
        "state_food_shortage_v01": "environment_market_granary_v01",
        "state_harsh_enforcement_v01": "environment_market_granary_v01",
        "state_market_recovery_v01": "environment_market_granary_v01",
    }
    for asset in manifest["assets"]:
        asset_id = asset["id"]
        if requested_ids is not None and asset_id not in requested_ids:
            continue
        visible = {collection_by_id[asset_id]}
        if asset_id in state_context:
            visible.add(collection_by_id[state_context[asset_id]])
        render_thumbnail(asset_id, visible, all_asset_collections, catalog_layer)

    rendered_count = len(manifest["assets"]) if requested_ids is None else len(requested_ids)
    print(f"Rendered {rendered_count} catalog thumbnails to {THUMB_DIR}")


if __name__ == "__main__":
    main()
