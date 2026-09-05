#!/usr/bin/env python3
"""Render the shipped hair and head meshes from three angles."""
from pathlib import Path
import argparse
import math
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
STYLES = ("cropped", "swept", "bob", "crest", "braided", "rear_lock")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--export-dir", type=Path,
                        default=ROOT / "assets/exports/world_kit")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    label_material = bpy.data.materials.new("ReviewLabel")
    label_material.diffuse_color = (0.8, 0.75, 0.65, 1)
    for column, style in enumerate(STYLES):
        font = bpy.data.curves.new(style, "FONT")
        font.body = style.replace("_", " ").upper()
        font.align_x, font.size = "CENTER", 0.034
        label = bpy.data.objects.new(style, font)
        scene.collection.objects.link(label)
        label.location = ((column - 2.5) * 0.49, -0.2, 1.15)
        label.rotation_euler.x = math.pi / 2
        font.materials.append(label_material)
    for row, angle in enumerate((0.0, math.pi * 0.32, math.pi)):
        for column, style in enumerate(STYLES):
            for asset in ("wk_head_square_v01", f"wk_hair_{style}_v01"):
                bpy.ops.import_scene.gltf(filepath=str(args.export_dir / f"{asset}.glb"))
                for obj in bpy.context.selected_objects:
                    if obj.parent is None:
                        obj.rotation_mode = "XYZ"
                        obj.rotation_euler.z += angle
                        obj.location += Vector(((column - 2.5) * 0.49, 0, (1 - row) * 0.75))
                    if obj.type == "MESH":
                        # Review shape and material color under a fixed light.
                        for material in obj.data.materials:
                            bsdf = material.node_tree.nodes.get("Principled BSDF")
                            for link in tuple(bsdf.inputs["Base Color"].links):
                                material.node_tree.links.remove(link)
                            bsdf.inputs["Base Color"].default_value = (
                                (0.065, 0.035, 0.022, 1) if "hair" in asset
                                else (0.58, 0.33, 0.22, 1))
    for name, location, energy, size in (
        ("Key", (-3, -4, 6), 450, 4), ("Fill", (4, -2, 3), 180, 3),
    ):
        data = bpy.data.lights.new(name, "AREA")
        light = bpy.data.objects.new(name, data)
        scene.collection.objects.link(light)
        light.location = location
        light.rotation_euler = (-light.location).to_track_quat("-Z", "Y").to_euler()
        data.energy, data.size = energy, size
    camera = bpy.data.objects.new("HairReview", bpy.data.cameras.new("HairReview"))
    scene.collection.objects.link(camera)
    camera.location = (0, -8, 2.0)
    camera.rotation_euler = (-camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 3.05
    scene.camera = camera
    scene.world = bpy.data.worlds.new("HairReview")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.045, 0.050, 0.060, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.5
    scene.view_settings.view_transform = "Standard"
    scene.render.resolution_x, scene.render.resolution_y = 1200, 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(args.output.resolve())
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
