#!/usr/bin/env python3
"""Render the economic cargo models for the fixed-pixel icon atlas."""

from __future__ import annotations

import json
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "asset_manifest.json"
OUTPUT_DIR = ROOT / "assets" / "previews" / "economic-icons"


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def asset_bounds(collection: bpy.types.Collection) -> tuple[Vector, Vector]:
    points: list[Vector] = []
    for obj in collection.all_objects:
        if obj.type != "MESH":
            continue
        points.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    if not points:
        raise RuntimeError(f"No mesh bounds found for {collection.name}")
    minimum = Vector(
        (min(point.x for point in points),
         min(point.y for point in points),
         min(point.z for point in points)))
    maximum = Vector(
        (max(point.x for point in points),
         max(point.y for point in points),
         max(point.z for point in points)))
    return minimum, maximum


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    collection_by_id = {
        asset["id"]: bpy.data.collections[asset["collection"]]
        for asset in manifest["assets"]
    }
    cargo_ids = [entry["cargo_asset"] for entry in manifest["economic_goods"]]

    scene = bpy.context.scene
    scene.render.resolution_x = 128
    scene.render.resolution_y = 128
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 15
    render_layer = scene.view_layers["CC_Economy_Cargo"]
    bpy.context.window.view_layer = render_layer

    ground = bpy.data.objects.get("STAGE_Ground")
    if ground is not None:
        ground.hide_render = True

    camera = bpy.data.objects["CAM_Isometric"]
    direction = Vector((1.15, -1.35, 1.0)).normalized()
    for asset_id in cargo_ids:
        for collection in collection_by_id.values():
            collection.hide_render = True
        collection = collection_by_id[asset_id]
        collection.hide_render = False
        minimum, maximum = asset_bounds(collection)
        center = (minimum + maximum) * 0.5
        size = maximum - minimum
        camera.location = center + direction * 8.0
        camera.data.ortho_scale = max(
            size.x * 1.55, size.y * 1.70, size.z * 1.42, 0.92)
        point_at(camera, center)
        scene.render.filepath = str(OUTPUT_DIR / f"{asset_id}.png")
        render_layer.update()
        bpy.ops.render.render(write_still=True, layer=render_layer.name)
        print(
            f"Rendered {asset_id}: size={tuple(round(value, 3) for value in size)} "
            f"ortho={camera.data.ortho_scale:.3f}")


if __name__ == "__main__":
    main()
