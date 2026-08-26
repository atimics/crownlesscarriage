#!/usr/bin/env python3
"""Render the V08 production hero from four review angles."""

from __future__ import annotations

from pathlib import Path
import sys

import bpy
from mathutils import Vector


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import export_screen_first_engine_hero as hero


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "out" / "character-experiments" / "hair_v08" / "sheet"
VIEWS = {
    "front": (0.0, -7.0, 2.75),
    "three_quarter": (4.5, -7.0, 3.0),
    "side": (7.0, 0.0, 2.75),
    "back": (0.0, 7.0, 2.75),
}
TARGET = Vector((0.0, 0.0, 1.15))


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def add_area_light(name: str, energy: float, size: float) -> bpy.types.Object:
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.color = (1.0, 0.90, 0.80)
    data.shape = "DISK"
    data.size = size
    light = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(light)
    return light


def build_character(rig: bpy.types.Object) -> bpy.types.Object:
    hero.reset_rig(rig)
    hero.remove_previous_export()
    hero.ensure_hair_bones(rig)
    materials = hero.build_materials()
    hero.build_body(materials)
    hero.build_limbs(materials)
    hero.build_head(materials)
    objects = [
        obj for obj in bpy.context.scene.objects
        if obj.type == "MESH" and obj.name.startswith(hero.PREFIX)
    ]
    for obj in objects:
        hero.skin_object(obj, rig)
    return hero.consolidate(objects, rig)


def configure_scene(combined: bpy.types.Object, rig: bpy.types.Object) -> tuple:
    scene = bpy.context.scene
    for obj in scene.objects:
        obj.hide_render = obj != combined
    combined.hide_render = False
    rig.hide_render = True
    rig.data.pose_position = "REST"

    camera_data = bpy.data.cameras.new("CAM_ScreenFirstSheetV08")
    camera = bpy.data.objects.new("CAM_ScreenFirstSheetV08", camera_data)
    scene.collection.objects.link(camera)
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 2.65
    scene.camera = camera

    key = add_area_light("KEY_ScreenFirstSheetV08", 430.0, 5.0)
    fill = add_area_light("FILL_ScreenFirstSheetV08", 110.0, 4.0)

    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 240
    scene.render.resolution_y = 320
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.025, 0.030, 0.033, 1.0)
    background.inputs["Strength"].default_value = 0.20
    return scene, camera, key, fill


def place_lights(camera: bpy.types.Object,
                 key: bpy.types.Object,
                 fill: bpy.types.Object) -> None:
    view = (camera.location - TARGET).normalized()
    right = Vector((0.0, 0.0, 1.0)).cross(view).normalized()
    key.location = TARGET + view * 4.2 - right * 2.8 + Vector((0.0, 0.0, 3.7))
    fill.location = TARGET + view * 3.0 + right * 2.8 + Vector((0.0, 0.0, 2.3))
    point_at(key, TARGET)
    point_at(fill, TARGET)


def main() -> None:
    view_layer = bpy.context.scene.view_layers.get("CC_CharacterSheetV08")
    if view_layer is None:
        view_layer = bpy.context.scene.view_layers.new(name="CC_CharacterSheetV08")
    bpy.context.window.view_layer = view_layer
    rig = bpy.data.objects.get(hero.RIG_NAME)
    if rig is None or rig.type != "ARMATURE":
        raise RuntimeError(f"missing armature {hero.RIG_NAME}")
    combined = build_character(rig)
    scene, camera, key, fill = configure_scene(combined, rig)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for label, location in VIEWS.items():
        camera.location = location
        point_at(camera, TARGET)
        place_lights(camera, key, fill)
        scene.render.filepath = str(OUT_DIR / f"{label}.png")
        bpy.ops.render.render(write_still=True)
    print(f"rendered V08 character sheet views to {OUT_DIR}")


if __name__ == "__main__":
    main()
