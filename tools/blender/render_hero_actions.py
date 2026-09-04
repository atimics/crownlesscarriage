#!/usr/bin/env python3

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "hero_component_manifest.json"
ACTION_BLEND = ROOT / "assets" / "blender" / "crownless_hero_actions.blend"
FRAMES_DIR = Path("/tmp/crownless_hero_action_frames")
PREVIEW_DIR = Path("/tmp/crownless_hero_action_previews")
FRAME_END = 240
CONTROL_BONES = (
    "pelvis", "chest", "head",
    "upper_arm.L", "forearm.L", "hand.L",
    "upper_arm.R", "forearm.R", "hand.R",
    "thigh.L", "shin.L", "foot.L",
    "thigh.R", "shin.R", "foot.R",
)


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


def configure_camera() -> None:
    camera = bpy.data.objects["CAM_HeroIsometric"]
    camera.animation_data_clear()
    camera.parent = None
    for constraint in list(camera.constraints):
        camera.constraints.remove(constraint)
    camera.location = (3.7, -6.2, 3.05)
    camera.data.ortho_scale = 2.90
    point_at(camera, Vector((0.0, 0.0, 1.08)))
    bpy.context.scene.camera = camera


def material(name: str) -> bpy.types.Material:
    return bpy.data.materials[f"MAT_{name.upper()}"]


def finish_prop(obj: bpy.types.Object, collection: bpy.types.Collection,
                material_name: str) -> bpy.types.Object:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)
    obj.data.materials.append(material(material_name))
    obj["cc_presentation_only"] = True
    return obj


def add_prop_cube(name: str, location: tuple[float, float, float],
                  dimensions: tuple[float, float, float], material_name: str,
                  collection: bpy.types.Collection,
                  rotation: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel = obj.modifiers.new("CC_ActionBevel", "BEVEL")
    bevel.width = min(dimensions) * 0.12
    bevel.segments = 2
    return finish_prop(obj, collection, material_name)


def add_prop_cylinder(name: str, location: tuple[float, float, float],
                      radius: float, depth: float, material_name: str,
                      collection: bpy.types.Collection,
                      rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
                      vertices: int = 12) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth,
        location=location, rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    return finish_prop(obj, collection, material_name)


def parent_to_bone(obj: bpy.types.Object, armature: bpy.types.Object,
                   bone_name: str) -> None:
    bpy.context.view_layer.update()
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.parent_type = "BONE"
    obj.parent_bone = bone_name
    obj.matrix_world = world


def key_prop_scale(objects: list[bpy.types.Object], start: int, end: int) -> None:
    for obj in objects:
        base = obj.scale.copy()
        for frame, scale in (
            (1, (0.0, 0.0, 0.0)),
            (max(1, start - 1), (0.0, 0.0, 0.0)),
            (start, tuple(base)),
            (end, tuple(base)),
            (min(FRAME_END, end + 1), (0.0, 0.0, 0.0)),
        ):
            obj.scale = scale
            obj.keyframe_insert("scale", frame=frame)


def build_action_props(armature: bpy.types.Object) -> None:
    root = bpy.data.collections.get("CC_HERO_ACTION_PROPS")
    if root is None:
        root = bpy.data.collections.new("CC_HERO_ACTION_PROPS")
        bpy.context.scene.collection.children.link(root)

    climb: list[bpy.types.Object] = []
    wall = add_prop_cube("ACTION_ClimbWall", (0.0, 0.47, 1.30),
                         (1.35, 0.12, 2.60), "steel_dark", root)
    climb.append(wall)
    for index, (x, z) in enumerate((
        (-0.32, 0.42), (0.28, 0.64), (-0.20, 0.90),
        (0.34, 1.14), (-0.30, 1.40), (0.22, 1.66),
        (-0.15, 1.92), (0.32, 2.18),
    )):
        hold = add_prop_cylinder(
            f"ACTION_ClimbHold_{index:02d}", (x, 0.375, z),
            0.075, 0.07, "brass" if index % 2 else "cape_light", root,
            rotation=(math.radians(90), 0.0, 0.0), vertices=8,
        )
        climb.append(hold)
    key_prop_scale(climb, 97, 144)

    water: list[bpy.types.Object] = []
    water.append(add_prop_cube("ACTION_WaterPlane", (0.0, 0.0, 0.82),
                               (4.5, 4.5, 0.06), "muscle_blue", root))
    for index, (y, x) in enumerate(((-0.65, -0.5), (-0.10, 0.35), (0.48, -0.20))):
        water.append(add_prop_cube(
            f"ACTION_WaterWave_{index}", (x, y, 0.93),
            (1.25, 0.035, 0.035), "rig_cyan", root,
            rotation=(0.0, 0.0, math.radians(8 - index * 8)),
        ))
    key_prop_scale(water, 145, 192)

    weapons: list[bpy.types.Object] = []
    blade = add_prop_cube("ACTION_SwordBlade", (0.59, -0.055, 1.15),
                          (0.055, 0.028, 0.62), "steel_light", root)
    guard = add_prop_cube("ACTION_SwordGuard", (0.59, -0.055, 0.86),
                          (0.22, 0.045, 0.045), "brass", root)
    grip = add_prop_cylinder("ACTION_SwordGrip", (0.59, -0.055, 0.77),
                             0.028, 0.20, "leather", root, vertices=8)
    for obj in (blade, guard, grip):
        parent_to_bone(obj, armature, "hand.R")
        weapons.append(obj)
    shield = add_prop_cylinder("ACTION_Shield", (-0.62, -0.11, 0.98),
                               0.19, 0.055, "steel", root,
                               rotation=(math.radians(90), 0.0, 0.0), vertices=12)
    shield_boss = add_prop_cylinder("ACTION_ShieldBoss", (-0.62, -0.145, 0.98),
                                    0.065, 0.075, "brass", root,
                                    rotation=(math.radians(90), 0.0, 0.0), vertices=8)
    for obj in (shield, shield_boss):
        parent_to_bone(obj, armature, "hand.L")
        weapons.append(obj)
    key_prop_scale(weapons, 193, 240)

    cape = bpy.data.collections["CC_HERO_CAPE"]
    for obj in cape.all_objects:
        if obj.type != "MESH":
            continue
        base = obj.scale.copy()
        for frame, scale in ((1, tuple(base)), (144, tuple(base)),
                             (145, (0.0, 0.0, 0.0)),
                             (192, (0.0, 0.0, 0.0)), (193, tuple(base))):
            obj.scale = scale
            obj.keyframe_insert("scale", frame=frame)


def key_pose(armature: bpy.types.Object, frame: int,
             rotations: dict[str, tuple[float, float, float]] | None = None,
             *, pelvis_z: float = 0.0,
             object_location: tuple[float, float, float] = (0.0, 0.0, 0.0),
             object_rotation: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> None:
    rotations = rotations or {}
    for name in CONTROL_BONES:
        bone = armature.pose.bones[name]
        bone.rotation_mode = "XYZ"
        bone.rotation_euler = rotations.get(name, (0.0, 0.0, 0.0))
        bone.location = (0.0, 0.0, pelvis_z if name == "pelvis" else 0.0)
        bone.keyframe_insert("rotation_euler", frame=frame)
        bone.keyframe_insert("location", frame=frame)
    armature.rotation_mode = "XYZ"
    armature.location = object_location
    armature.rotation_euler = object_rotation
    armature.keyframe_insert("location", frame=frame)
    armature.keyframe_insert("rotation_euler", frame=frame)


def author_walk(armature: bpy.types.Object) -> None:
    left = {
        "upper_arm.L": (-0.42, 0.0, 0.0), "forearm.L": (-0.18, 0.0, 0.0),
        "upper_arm.R": (0.42, 0.0, 0.0), "forearm.R": (-0.35, 0.0, 0.0),
        "thigh.L": (0.48, 0.0, 0.0), "shin.L": (0.10, 0.0, 0.0),
        "thigh.R": (-0.48, 0.0, 0.0), "shin.R": (0.60, 0.0, 0.0),
        "chest": (0.0, 0.0, 0.055), "head": (0.0, 0.0, -0.035),
    }
    right = {
        "upper_arm.L": (0.42, 0.0, 0.0), "forearm.L": (-0.35, 0.0, 0.0),
        "upper_arm.R": (-0.42, 0.0, 0.0), "forearm.R": (-0.18, 0.0, 0.0),
        "thigh.L": (-0.48, 0.0, 0.0), "shin.L": (0.60, 0.0, 0.0),
        "thigh.R": (0.48, 0.0, 0.0), "shin.R": (0.10, 0.0, 0.0),
        "chest": (0.0, 0.0, -0.055), "head": (0.0, 0.0, 0.035),
    }
    passing_l = {"shin.L": (0.72, 0.0, 0.0), "chest": (0.0, 0.0, -0.02)}
    passing_r = {"shin.R": (0.72, 0.0, 0.0), "chest": (0.0, 0.0, 0.02)}
    key_pose(armature, 1, left)
    key_pose(armature, 13, passing_l, pelvis_z=0.030)
    key_pose(armature, 25, right)
    key_pose(armature, 37, passing_r, pelvis_z=0.030)
    key_pose(armature, 48, left)


def author_jump(armature: bpy.types.Object) -> None:
    crouch = {
        "upper_arm.L": (0.38, 0.0, 0.0), "upper_arm.R": (0.38, 0.0, 0.0),
        "thigh.L": (-0.62, 0.0, 0.0), "thigh.R": (-0.62, 0.0, 0.0),
        "shin.L": (1.05, 0.0, 0.0), "shin.R": (1.05, 0.0, 0.0),
        "chest": (0.16, 0.0, 0.0),
    }
    launch = {
        "upper_arm.L": (-1.18, 0.0, 0.0), "upper_arm.R": (-1.18, 0.0, 0.0),
        "forearm.L": (-0.25, 0.0, 0.0), "forearm.R": (-0.25, 0.0, 0.0),
        "thigh.L": (0.10, 0.0, 0.0), "thigh.R": (0.10, 0.0, 0.0),
    }
    apex = {
        "upper_arm.L": (-0.92, 0.0, 0.0), "upper_arm.R": (-0.92, 0.0, 0.0),
        "thigh.L": (0.48, 0.0, 0.0), "thigh.R": (0.48, 0.0, 0.0),
        "shin.L": (-0.72, 0.0, 0.0), "shin.R": (-0.72, 0.0, 0.0),
    }
    key_pose(armature, 49, crouch, pelvis_z=-0.105)
    key_pose(armature, 61, launch, pelvis_z=0.20)
    key_pose(armature, 73, apex, pelvis_z=0.46)
    key_pose(armature, 85, crouch, pelvis_z=-0.055)
    key_pose(armature, 96)


def author_climb(armature: bpy.types.Object) -> None:
    left_reach = {
        "upper_arm.L": (0.0, 0.0, 1.42), "forearm.L": (0.0, 0.0, -0.35),
        "upper_arm.R": (0.0, 0.0, -0.70), "forearm.R": (0.0, 0.0, 0.65),
        "thigh.L": (-0.65, 0.0, 0.0), "shin.L": (1.05, 0.0, 0.0),
        "chest": (0.12, 0.0, -0.08),
    }
    right_reach = {
        "upper_arm.L": (0.0, 0.0, 0.70), "forearm.L": (0.0, 0.0, -0.65),
        "upper_arm.R": (0.0, 0.0, -1.42), "forearm.R": (0.0, 0.0, 0.35),
        "thigh.R": (-0.65, 0.0, 0.0), "shin.R": (1.05, 0.0, 0.0),
        "chest": (0.12, 0.0, 0.08),
    }
    key_pose(armature, 97, left_reach, object_location=(0.0, 0.02, 0.0))
    key_pose(armature, 109, right_reach, object_location=(0.0, 0.02, 0.16))
    key_pose(armature, 121, left_reach, object_location=(0.0, 0.02, 0.31))
    key_pose(armature, 133, right_reach, object_location=(0.0, 0.02, 0.46))
    key_pose(armature, 144, left_reach, object_location=(0.0, 0.02, 0.58))


def author_swim(armature: bpy.types.Object) -> None:
    horizontal = (math.radians(90), 0.0, 0.0)
    extension = {
        "upper_arm.L": (0.0, 0.0, 1.10), "upper_arm.R": (0.0, 0.0, -1.10),
        "forearm.L": (0.0, 0.0, -0.20), "forearm.R": (0.0, 0.0, 0.20),
        "thigh.L": (0.20, 0.0, 0.0), "thigh.R": (-0.20, 0.0, 0.0),
    }
    stroke_l = {
        "upper_arm.L": (-1.10, 0.0, 0.55), "forearm.L": (-0.75, 0.0, 0.0),
        "upper_arm.R": (0.70, 0.0, -0.45), "forearm.R": (-0.30, 0.0, 0.0),
        "thigh.L": (-0.35, 0.0, 0.0), "shin.L": (0.35, 0.0, 0.0),
        "thigh.R": (0.35, 0.0, 0.0), "shin.R": (-0.35, 0.0, 0.0),
    }
    stroke_r = {
        "upper_arm.L": (0.70, 0.0, 0.45), "forearm.L": (-0.30, 0.0, 0.0),
        "upper_arm.R": (-1.10, 0.0, -0.55), "forearm.R": (-0.75, 0.0, 0.0),
        "thigh.L": (0.35, 0.0, 0.0), "shin.L": (-0.35, 0.0, 0.0),
        "thigh.R": (-0.35, 0.0, 0.0), "shin.R": (0.35, 0.0, 0.0),
    }
    key_pose(armature, 145, extension, object_location=(0.0, 0.10, 1.12),
             object_rotation=horizontal)
    key_pose(armature, 157, stroke_l, object_location=(-0.08, 0.02, 1.13),
             object_rotation=horizontal)
    key_pose(armature, 169, extension, object_location=(0.0, -0.05, 1.12),
             object_rotation=horizontal)
    key_pose(armature, 181, stroke_r, object_location=(0.08, 0.02, 1.13),
             object_rotation=horizontal)
    key_pose(armature, 192, extension, object_location=(0.0, 0.10, 1.12),
             object_rotation=horizontal)


def author_fight(armature: bpy.types.Object) -> None:
    guard = {
        "upper_arm.L": (-0.55, 0.0, 0.45), "forearm.L": (-0.85, 0.0, -0.15),
        "upper_arm.R": (-0.55, 0.0, -0.35), "forearm.R": (-0.95, 0.0, 0.0),
        "thigh.L": (0.22, 0.0, 0.0), "thigh.R": (-0.22, 0.0, 0.0),
        "chest": (0.0, 0.0, -0.14), "head": (0.0, 0.0, 0.10),
    }
    high_slash = {
        "upper_arm.L": (-0.35, 0.0, 0.75), "forearm.L": (-0.70, 0.0, -0.10),
        "upper_arm.R": (-1.05, 0.0, 0.75), "forearm.R": (-0.35, 0.0, 0.35),
        "chest": (0.0, 0.0, 0.30), "head": (0.0, 0.0, -0.18),
        "thigh.L": (0.38, 0.0, 0.0), "shin.L": (0.35, 0.0, 0.0),
    }
    cross_slash = {
        "upper_arm.L": (-0.25, 0.0, 0.60), "forearm.L": (-0.60, 0.0, 0.0),
        "upper_arm.R": (0.35, 0.0, -1.05), "forearm.R": (-0.15, 0.0, -0.25),
        "chest": (0.0, 0.0, -0.38), "head": (0.0, 0.0, 0.22),
        "thigh.R": (0.34, 0.0, 0.0), "shin.R": (0.30, 0.0, 0.0),
    }
    key_pose(armature, 193, guard)
    key_pose(armature, 205, high_slash, object_location=(-0.08, -0.05, 0.04))
    key_pose(armature, 217, cross_slash, object_location=(0.10, -0.10, 0.02))
    key_pose(armature, 229, high_slash, object_location=(-0.04, -0.04, 0.03))
    key_pose(armature, 240, guard)


def configure_render() -> None:
    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = FRAME_END
    scene.render.fps = 24
    scene.render.resolution_x = 512
    scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False


def render(preview_only: bool) -> None:
    scene = bpy.context.scene
    if preview_only:
        PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
        for frame, label in ((1, "walk"), (73, "jump"), (121, "climb"),
                             (169, "swim"), (217, "fight")):
            scene.frame_set(frame)
            scene.render.filepath = str(PREVIEW_DIR / f"{label}.png")
            bpy.ops.render.render(write_still=True)
        print(f"Rendered action pose previews to {PREVIEW_DIR}")
        return
    FRAMES_DIR.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(FRAMES_DIR / "action_")
    bpy.ops.render.render(animation=True)
    print(f"Rendered {FRAME_END} action frames to {FRAMES_DIR}")


def main() -> None:
    configure_visibility()
    configure_camera()
    armature = bpy.data.objects["ARM_CrownlessHero"]
    armature.animation_data_clear()
    for bone in armature.pose.bones:
        bone.rotation_mode = "XYZ"
        bone.rotation_euler = (0.0, 0.0, 0.0)
        bone.location = (0.0, 0.0, 0.0)
    build_action_props(armature)
    author_walk(armature)
    author_jump(armature)
    author_climb(armature)
    author_swim(armature)
    author_fight(armature)
    configure_render()
    ACTION_BLEND.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(ACTION_BLEND), compress=True)
    render("--preview" in sys.argv)


if __name__ == "__main__":
    main()
