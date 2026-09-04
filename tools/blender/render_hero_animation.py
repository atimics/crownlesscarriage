#!/usr/bin/env python3

from __future__ import annotations

import json
import math
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "hero_component_manifest.json"
ANIMATION_BLEND = ROOT / "assets" / "blender" / "crownless_hero_animation.blend"
FRAMES_DIR = Path("/tmp/crownless_hero_turntable_frames")
START_FRAME = 1
LOOP_FRAME = 97
END_FRAME = LOOP_FRAME - 1


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def configure_visibility() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    assembled_ids = set(
        manifest["assemblies"]["wayfarer_prototype_v01"]["components"]
    )
    for component in manifest["components"]:
        collection = bpy.data.collections.get(component["collection"])
        if collection is not None:
            collection.hide_render = component["id"] not in assembled_ids
    for name in (
        "CC_HERO_RIG_GUIDES",
        "CC_HERO_CAPE_CAGE_GUIDES",
        "CC_HERO_EXPLODED_DISPLAY",
    ):
        collection = bpy.data.collections.get(name)
        if collection is not None:
            collection.hide_render = True
    for layer in bpy.context.scene.view_layers:
        layer.use = layer.name == "CC_Hero_Assembled"


def animate_camera() -> None:
    scene = bpy.context.scene
    camera = bpy.data.objects["CAM_HeroIsometric"]
    camera.animation_data_clear()
    camera.parent = None
    for constraint in list(camera.constraints):
        if constraint.name.startswith("ANIM_"):
            camera.constraints.remove(constraint)
    camera.location = (3.55, -5.75, 2.90)
    camera.data.ortho_scale = 2.72
    target = Vector((0.0, 0.0, 1.05))
    point_at(camera, target)
    bpy.context.view_layer.update()

    target_object = bpy.data.objects.get("ANIM_HeroTarget")
    if target_object is None:
        target_object = bpy.data.objects.new("ANIM_HeroTarget", None)
        bpy.data.collections["90_PRESENTATION"].objects.link(target_object)
    target_object.location = target

    orbit = bpy.data.objects.get("ANIM_HeroOrbit")
    if orbit is None:
        orbit = bpy.data.objects.new("ANIM_HeroOrbit", None)
        bpy.data.collections["90_PRESENTATION"].objects.link(orbit)
    orbit.animation_data_clear()
    orbit.location = (0.0, 0.0, 0.0)
    orbit.rotation_mode = "XYZ"
    world = camera.matrix_world.copy()
    camera.parent = orbit
    camera.matrix_world = world
    tracking = camera.constraints.new("TRACK_TO")
    tracking.name = "ANIM_TrackHero"
    tracking.target = target_object
    tracking.track_axis = "TRACK_NEGATIVE_Z"
    tracking.up_axis = "UP_Y"
    orbit.rotation_euler = (0.0, 0.0, 0.0)
    try:
        orbit.driver_remove("rotation_euler", 2)
    except TypeError:
        pass
    driver = orbit.driver_add("rotation_euler", 2).driver
    driver.type = "SCRIPTED"
    driver.expression = f"{math.tau} * (frame - {START_FRAME}) / {LOOP_FRAME - START_FRAME}"

    light_data = bpy.data.lights.get("ANIM_CameraFill")
    if light_data is None:
        light_data = bpy.data.lights.new("ANIM_CameraFill", "AREA")
    light_data.energy = 520
    light_data.size = 4.0
    fill = bpy.data.objects.get("ANIM_CameraFill")
    if fill is None:
        fill = bpy.data.objects.new("ANIM_CameraFill", light_data)
        bpy.data.collections["90_PRESENTATION"].objects.link(fill)
    fill.parent = None
    fill.location = (2.8, -4.5, 4.0)
    point_at(fill, target)
    bpy.context.view_layer.update()
    fill_world = fill.matrix_world.copy()
    fill.parent = orbit
    fill.matrix_world = fill_world
    scene.camera = camera


def animate_idle() -> None:
    armature = bpy.data.objects["ARM_CrownlessHero"]
    armature.animation_data_clear()
    pelvis = armature.pose.bones["pelvis"]
    chest = armature.pose.bones["chest"]
    head = armature.pose.bones["head"]
    for bone in (pelvis, chest, head):
        bone.rotation_mode = "XYZ"

    frames = (START_FRAME, 25, 49, 73, LOOP_FRAME)
    pelvis_heights = (0.0, 0.010, 0.0, -0.006, 0.0)
    chest_twists = (0.0, math.radians(1.2), 0.0, math.radians(-1.2), 0.0)
    head_twists = (0.0, math.radians(-1.8), 0.0, math.radians(1.8), 0.0)
    for frame, height, chest_twist, head_twist in zip(
        frames, pelvis_heights, chest_twists, head_twists
    ):
        pelvis.location.z = height
        pelvis.keyframe_insert("location", frame=frame, index=2)
        chest.rotation_euler.z = chest_twist
        chest.keyframe_insert("rotation_euler", frame=frame, index=2)
        head.rotation_euler.z = head_twist
        head.keyframe_insert("rotation_euler", frame=frame, index=2)


def animate_cape() -> None:
    cape = bpy.data.objects["GEO_HeroCape"]
    for modifier in cape.modifiers:
        if modifier.type == "CLOTH":
            modifier.show_viewport = False
            modifier.show_render = False
    if cape.data.shape_keys is not None:
        for key in list(cape.data.shape_keys.key_blocks)[1:]:
            cape.shape_key_remove(key)
    if cape.data.shape_keys is None:
        cape.shape_key_add(name="Basis")
    breeze = cape.shape_key_add(name="Breeze")
    breeze.slider_min = -1.0
    breeze.slider_max = 1.0
    columns = int(cape.get("cc_control_columns", 6))
    rows = int(cape.get("cc_control_rows", 7))
    for index, point in enumerate(breeze.data):
        row = index // columns
        column = index % columns
        vertical = row / max(rows - 1, 1)
        horizontal = column / max(columns - 1, 1)
        point.co.x += math.sin(horizontal * math.pi) * 0.035 * vertical
        point.co.y += math.sin(horizontal * math.tau) * 0.045 * vertical
    for frame, value in (
        (START_FRAME, 0.0), (25, 1.0), (49, 0.0),
        (73, -1.0), (LOOP_FRAME, 0.0),
    ):
        breeze.value = value
        breeze.keyframe_insert("value", frame=frame)


def configure_render() -> None:
    scene = bpy.context.scene
    FRAMES_DIR.mkdir(parents=True, exist_ok=True)
    scene.frame_start = START_FRAME
    scene.frame_end = END_FRAME
    scene.render.fps = 24
    scene.render.resolution_x = 640
    scene.render.resolution_y = 640
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(FRAMES_DIR / "frame_")
    scene.render.film_transparent = False
    scene.frame_set(START_FRAME)


def main() -> None:
    ANIMATION_BLEND.parent.mkdir(parents=True, exist_ok=True)
    configure_visibility()
    animate_camera()
    animate_idle()
    animate_cape()
    configure_render()
    bpy.ops.wm.save_as_mainfile(filepath=str(ANIMATION_BLEND), compress=True)
    bpy.ops.render.render(animation=True)
    print(f"Rendered looping hero frames to {FRAMES_DIR}")


if __name__ == "__main__":
    main()
