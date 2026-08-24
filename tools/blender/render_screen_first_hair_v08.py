#!/usr/bin/env python3
"""Render the V08 clump hair on the production walk action."""

from __future__ import annotations

import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import export_screen_first_engine_hero as hero


ROOT = Path(__file__).resolve().parents[2]
FRAME_DIR = ROOT / "out" / "character-experiments" / "hair_v08" / "walk"
FRAMES = tuple(range(1, 49, 3))


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def add_area_light(name: str, location: tuple[float, float, float],
                   energy: float, color: tuple[float, float, float],
                   size: float) -> None:
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.color = color
    data.shape = "DISK"
    data.size = size
    light = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(light)
    light.location = location
    point_at(light, Vector((0.0, 0.0, 1.1)))


def build_character(rig: bpy.types.Object) -> bpy.types.Object:
    hero.remove_previous_export()
    hero.ensure_hair_bones(rig)
    materials = hero.build_materials()
    hero.build_body(materials)
    hero.build_limbs(materials)
    hero.build_head(materials)
    objects = [obj for obj in bpy.context.scene.objects
               if obj.type == "MESH" and obj.name.startswith(hero.PREFIX)]
    for obj in objects:
        hero.skin_object(obj, rig)
    return hero.consolidate(objects, rig)


def configure_scene(combined: bpy.types.Object, rig: bpy.types.Object) -> None:
    scene = bpy.context.scene
    for obj in scene.objects:
        obj.hide_render = obj != combined
    combined.hide_render = False
    rig.hide_render = True
    rig.data.pose_position = "POSE"

    camera_data = bpy.data.cameras.new("CAM_ScreenFirstHairWalkV08")
    camera = bpy.data.objects.new("CAM_ScreenFirstHairWalkV08", camera_data)
    scene.collection.objects.link(camera)
    camera.location = (4.8, -7.2, 3.4)
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 3.35
    point_at(camera, Vector((0.0, 0.0, 1.15)))
    scene.camera = camera

    add_area_light("KEY_HairWalkV08", (-4.0, -5.5, 7.0),
                   950.0, (1.0, 0.72, 0.52), 6.0)
    add_area_light("FILL_HairWalkV08", (4.0, 2.0, 5.0),
                   500.0, (0.48, 0.62, 0.74), 5.0)

    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 240
    scene.render.resolution_y = 240
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    scene.render.fps = 12
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.025, 0.030, 0.033, 1.0)
    background.inputs["Strength"].default_value = 0.32


def pose_hair(rig: bpy.types.Object, frame: int) -> None:
    """Give the weighted tips a small delayed response to the walk."""
    phase = math.tau * (frame - 1) / 47.0
    long_lock = rig.pose.bones["hair.long"]
    rear = rig.pose.bones["hair.rear"]
    for bone in (long_lock, rear):
        bone.rotation_mode = "XYZ"
    long_lock.rotation_euler = (
        math.sin(phase - 0.50) * 0.055,
        math.sin(phase - 0.35) * 0.025,
        math.sin(phase - 0.60) * 0.085,
    )
    rear.rotation_euler = (
        math.sin(phase - 0.75) * 0.040,
        0.0,
        math.sin(phase - 0.80) * 0.050,
    )
    bpy.context.view_layer.update()


def main() -> None:
    view_layer = bpy.context.scene.view_layers.get("CC_HairWalkV08")
    if view_layer is None:
        view_layer = bpy.context.scene.view_layers.new(name="CC_HairWalkV08")
    bpy.context.window.view_layer = view_layer
    rig = bpy.data.objects.get(hero.RIG_NAME)
    if rig is None or rig.type != "ARMATURE":
        raise RuntimeError(f"missing armature {hero.RIG_NAME}")
    if rig.animation_data is None or rig.animation_data.action is None:
        raise RuntimeError("the production walk action is not active")
    combined = build_character(rig)
    configure_scene(combined, rig)
    FRAME_DIR.mkdir(parents=True, exist_ok=True)
    for index, frame in enumerate(FRAMES):
        bpy.context.scene.frame_set(frame)
        pose_hair(rig, frame)
        path = FRAME_DIR / f"frame_{index:02d}.png"
        bpy.context.scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
    print(f"rendered {len(FRAMES)} V08 hair walk frames to {FRAME_DIR}")


if __name__ == "__main__":
    main()
