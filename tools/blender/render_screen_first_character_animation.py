#!/usr/bin/env python3
"""Animate the painted screen-first character as rigid low-poly body masses."""

from __future__ import annotations

import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import render_screen_first_character_experiments as character


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "out" / "character-experiments" / "animation_v05"
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_screen_first_character_animation_v05.blend"
CHARACTER_X = 4.05

CLIPS = {
    "idle": (1, 24),
    "walk": (25, 48),
    "turn": (49, 80),
}


def parent_keep_world(child: bpy.types.Object, parent: bpy.types.Object) -> None:
    bpy.context.view_layer.update()
    child_world = child.matrix_world.copy()
    parent_world = parent.matrix_world.copy()
    child.parent = parent
    child.matrix_parent_inverse = Matrix.Identity(4)
    child.matrix_basis = parent_world.inverted() @ child_world
    bpy.context.view_layer.update()


def add_control(
    name: str,
    location: tuple[float, float, float],
    parent: bpy.types.Object | None = None,
) -> bpy.types.Object:
    control = bpy.data.objects.new(name, None)
    bpy.context.scene.collection.objects.link(control)
    control.empty_display_type = "PLAIN_AXES"
    control.empty_display_size = 0.12
    if parent is None:
        control.location = location
    else:
        bpy.context.view_layer.update()
        control.parent = parent
        control.location = parent.matrix_world.inverted() @ Vector(location)
    return control


def build_rigid_rig() -> dict[str, bpy.types.Object]:
    root = add_control("CTRL_Root", (CHARACTER_X, 0.0, 0.0))
    body = add_control("CTRL_Body", (CHARACTER_X, 0.0, 0.86), root)
    scarf = add_control("CTRL_Scarf", (CHARACTER_X, 0.0, 1.76), body)
    head = add_control("CTRL_Head", (CHARACTER_X, 0.0, 2.08), body)
    arm_l = add_control("CTRL_Arm_L", (CHARACTER_X - 0.43, 0.0, 1.76), body)
    arm_r = add_control("CTRL_Arm_R", (CHARACTER_X + 0.43, 0.0, 1.76), body)
    leg_l = add_control("CTRL_Leg_L", (CHARACTER_X - 0.205, 0.0, 0.86), root)
    leg_r = add_control("CTRL_Leg_R", (CHARACTER_X + 0.205, 0.0, 0.86), root)
    boot_l = add_control("CTRL_Boot_L", (CHARACTER_X - 0.205, 0.0, 0.47), leg_l)
    boot_r = add_control("CTRL_Boot_R", (CHARACTER_X + 0.205, 0.0, 0.47), leg_r)

    prefix = "PaintAccents_"
    for obj in tuple(bpy.context.scene.objects):
        if not obj.name.startswith(prefix):
            continue
        name = obj.name[len(prefix):]
        if any(token in name for token in ("Head", "Hair", "Eye", "Mouth", "Crown")):
            parent_keep_world(obj, head)
        elif "Scarf" in name:
            parent_keep_world(obj, scarf)
        elif any(token in name for token in ("Sleeve_L", "Forearm_L", "Hand_L", "HandShadow_L")):
            parent_keep_world(obj, arm_l)
        elif any(token in name for token in ("Sleeve_R", "Forearm_R", "Hand_R", "HandShadow_R")):
            parent_keep_world(obj, arm_r)
        elif "BootCuff_L" in name or "BootFoot_L" in name:
            parent_keep_world(obj, boot_l)
        elif "BootCuff_R" in name or "BootFoot_R" in name:
            parent_keep_world(obj, boot_r)
        elif "Leg_L" in name:
            parent_keep_world(obj, leg_l)
        elif "Leg_R" in name:
            parent_keep_world(obj, leg_r)
        else:
            parent_keep_world(obj, body)

    return {
        "root": root,
        "body": body,
        "scarf": scarf,
        "head": head,
        "arm_l": arm_l,
        "arm_r": arm_r,
        "leg_l": leg_l,
        "leg_r": leg_r,
        "boot_l": boot_l,
        "boot_r": boot_r,
    }


def reset_pose(controls: dict[str, bpy.types.Object]) -> None:
    controls["root"].location = (CHARACTER_X, 0.0, 0.0)
    for name, control in controls.items():
        if name != "root":


            pass
        control.rotation_mode = "XYZ"
        control.rotation_euler = (0.0, 0.0, 0.0)


def pose_idle(controls: dict[str, bpy.types.Object], phase: float) -> None:
    reset_pose(controls)
    breath = 0.5 - 0.5 * math.cos(phase)
    sway = math.sin(phase)
    controls["root"].location.z = breath * 0.012
    controls["body"].rotation_euler.z = sway * math.radians(0.8)
    controls["head"].rotation_euler.z = -sway * math.radians(0.9)
    controls["scarf"].rotation_euler.z = -sway * math.radians(0.45)
    controls["arm_l"].rotation_euler.x = sway * math.radians(1.4)
    controls["arm_r"].rotation_euler.x = -sway * math.radians(1.4)


def pose_walk(controls: dict[str, bpy.types.Object], phase: float) -> None:
    reset_pose(controls)
    stride = math.sin(phase)
    double_step = 0.5 - 0.5 * math.cos(phase * 2.0)
    controls["root"].location.z = double_step * 0.035
    controls["body"].rotation_euler.x = math.radians(2.0)
    controls["body"].rotation_euler.z = -stride * math.radians(1.8)
    controls["head"].rotation_euler.z = stride * math.radians(1.3)
    controls["scarf"].rotation_euler.x = -math.radians(1.2 + double_step * 1.8)
    controls["scarf"].rotation_euler.z = stride * math.radians(1.4)
    controls["arm_l"].rotation_euler.x = stride * math.radians(20.0)
    controls["arm_r"].rotation_euler.x = -stride * math.radians(20.0)
    controls["leg_l"].rotation_euler.x = -stride * math.radians(22.0)
    controls["leg_r"].rotation_euler.x = stride * math.radians(22.0)
    controls["boot_l"].rotation_euler.x = stride * math.radians(8.0)
    controls["boot_r"].rotation_euler.x = -stride * math.radians(8.0)


def pose_turn(controls: dict[str, bpy.types.Object], phase: float) -> None:
    reset_pose(controls)
    turn = math.sin(phase)
    settle = math.sin(phase * 2.0)
    controls["root"].rotation_euler.z = turn * math.radians(34.0)
    controls["body"].rotation_euler.z = -settle * math.radians(1.5)
    controls["head"].rotation_euler.z = -turn * math.radians(4.0)
    controls["scarf"].rotation_euler.z = -settle * math.radians(2.0)
    controls["arm_l"].rotation_euler.x = settle * math.radians(3.0)
    controls["arm_r"].rotation_euler.x = -settle * math.radians(3.0)


def key_pose(controls: dict[str, bpy.types.Object], frame: int) -> None:
    for control in controls.values():
        control.keyframe_insert(data_path="location", frame=frame)
        control.keyframe_insert(data_path="rotation_euler", frame=frame)


def author_animation(controls: dict[str, bpy.types.Object]) -> None:
    for clip, (start, end) in CLIPS.items():
        count = end - start + 1
        bpy.context.scene.timeline_markers.new(clip.upper(), frame=start)
        for index, frame in enumerate(range(start, end + 1)):
            phase = math.tau * float(index) / float(count)
            if clip == "idle":
                pose_idle(controls, phase)
            elif clip == "walk":
                pose_walk(controls, phase)
            else:
                pose_turn(controls, phase)
            key_pose(controls, frame)

def configure_animation_scene() -> None:
    scene = bpy.context.scene
    scene.name = "CC_SCREEN_FIRST_CHARACTER_ANIMATION_V05"
    for obj in scene.objects:
        if obj.name.startswith("LightOnly") or obj.name.startswith("BroadPaint"):
            obj.hide_render = True
            obj.hide_viewport = True
    camera = scene.camera
    camera.location = (CHARACTER_X + 5.4, -7.0, 3.55)
    camera.data.ortho_scale = 4.05
    character.point_at(camera, Vector((CHARACTER_X, 0.0, 1.38)))
    scene.render.resolution_x = 360
    scene.render.resolution_y = 360
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.frame_start = 1
    scene.frame_end = 80
    scene.render.fps = 12


def render_clips() -> None:
    scene = bpy.context.scene
    for clip, (start, end) in CLIPS.items():
        directory = OUT_DIR / clip
        directory.mkdir(parents=True, exist_ok=True)
        for frame in range(start, end + 1):
            scene.frame_set(frame)
            scene.render.filepath = str(directory / f"frame_{frame:03d}.png")
            bpy.ops.render.render(write_still=True)
        print(f"Rendered {clip} frames to {directory}")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    character.reset_scene()
    materials = {
        name: character.make_material(name, color)
        for name, color in character.PALETTE.items()
    }
    character.setup_scene(materials)
    configure_animation_scene()
    controls = build_rigid_rig()
    author_animation(controls)
    bpy.context.scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH), compress=True)
    render_clips()
    print(f"Saved editable animation scene to {BLEND_PATH}")


if __name__ == "__main__":
    main()
