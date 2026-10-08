#!/usr/bin/env python3
"""Render the wolf and rabbit from the saved creature library."""

from pathlib import Path
import math

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[2]
LIBRARY = ROOT / "assets/blender/crownless_creature_library.blend"
OUTPUT = ROOT / "docs/art/wolf-rabbit/wolf-rabbit-sheet.png"


def look_at(obj, point):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat("-Z", "Y").to_euler()


def text_label(label, x, z, size, material):
    bpy.ops.object.text_add(location=(x, -0.7, z), rotation=(math.pi / 2, 0.0, 0.0))
    obj = bpy.context.object
    obj.data.body = label
    obj.data.size = size
    obj.data.align_x = "CENTER"
    obj.data.materials.append(material)


def render():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with bpy.data.libraries.load(str(LIBRARY), link=False) as (source, target):
        target.objects = [name for name in source.objects
                          if name.startswith(("PREVIEW_SOURCE_wolf_", "PREVIEW_SOURCE_rabbit_"))]
    parts = [obj for obj in target.objects if obj is not None]
    scene = bpy.context.scene
    for part in parts:
        scene.collection.objects.link(part)
        part.hide_render = True
    bpy.context.view_layer.update()
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1800
    scene.render.resolution_y = 1200
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.view_settings.exposure = -0.75
    scene.world = bpy.data.worlds.new("Wildlife studio")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.026, 0.034, 0.040, 1.0)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.5

    ink = bpy.data.materials.new("Sheet labels")
    ink.diffuse_color = (0.73, 0.76, 0.73, 1.0)
    ink.use_nodes = True
    bsdf = ink.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = ink.diffuse_color
    bsdf.inputs["Emission Color"].default_value = ink.diffuse_color
    bsdf.inputs["Emission Strength"].default_value = 0.4

    for variant, row_z in (("wolf", 2.3), ("rabbit", 0.25)):
        for column, angle in enumerate((math.radians(-32), math.pi / 2, math.radians(145))):
            x = (column - 1) * 3.0
            for part in parts:
                if not part.name.startswith(f"PREVIEW_SOURCE_{variant}_"):
                    continue
                obj = part.copy()
                obj.data = part.data.copy()
                obj.parent = None
                scene.collection.objects.link(obj)
                obj.hide_render = False
                obj.hide_viewport = False
                obj.hide_set(False)
                obj.matrix_world = (Matrix.Translation((x, 0.0, row_z)) @
                                    Matrix.Rotation(angle, 4, "Z") @ part.matrix_world)
            text_label(("FRONT", "SIDE", "REAR")[column], x, row_z - 0.19, 0.115, ink)
        text_label(variant.upper(), 0.0, row_z + (1.65 if variant == "wolf" else 1.35), 0.22, ink)
    text_label("CROWNLESS  /  WOODLAND CREATURES", 0.0, 4.35, 0.20, ink)
    text_label("METRE SCALE  /  19 BONES  /  PALETTE COLOUR", 0.0, -0.35, 0.115, ink)

    bpy.ops.object.camera_add(location=(0.0, -13.0, 6.0))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 9.2
    look_at(camera, (0.0, 0.0, 2.05))
    scene.camera = camera
    for name, position, energy, color, size in (
        ("Key", (-3.0, -5.0, 8.0), 1800, (1.0, 0.88, 0.74), 7.0),
        ("Fill", (4.0, -2.0, 6.0), 1100, (0.67, 0.80, 1.0), 6.0),
        ("Rim", (0.0, 4.0, 7.0), 1800, (0.84, 0.94, 1.0), 5.0),
    ):
        bpy.ops.object.light_add(type="AREA", location=position)
        light = bpy.context.object
        light.name = name
        light.data.energy = energy
        light.data.color = color
        light.data.size = size
        look_at(light, (0.0, 0.0, 2.0))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(OUTPUT)
    bpy.ops.render.render(write_still=True)
    print(f"wildlife sheet: {OUTPUT}")


if __name__ == "__main__":
    render()
