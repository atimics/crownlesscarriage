#!/usr/bin/env python3
"""Render a clean V09 adult-proportion silhouette blockout."""

from __future__ import annotations

from pathlib import Path
import sys

import bpy
from mathutils import Vector


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import export_screen_first_engine_hero as hero
import render_screen_first_character_experiments as shapes


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "out" / "character-experiments" / "redo_v09"
PREFIX = "REDO_V09_"
TARGET = Vector((0.0, 0.0, 1.08))
VIEWS = {
    "front": (0.0, -7.0, 2.75),
    "three_quarter": (4.5, -7.0, 2.85),
    "side": (7.0, 0.0, 2.75),
    "back": (0.0, 7.0, 2.75),
}


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def make_silhouette_material() -> bpy.types.Material:
    material = bpy.data.materials.new("MAT_RedoV09Silhouette")
    material.diffuse_color = (0.008, 0.010, 0.011, 1.0)
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (0.008, 0.010, 0.011, 1.0)
    principled.inputs["Roughness"].default_value = 1.0
    principled.inputs["Specular IOR Level"].default_value = 0.0
    return material


def add_head(material: bpy.types.Material) -> None:
    """A small six-head adult skull with a broad face plane."""
    profile = (
        (-0.068, 2.095), (-0.117, 2.072), (-0.138, 2.020),
        (-0.140, 1.935), (-0.122, 1.855), (-0.081, 1.805),
        (-0.045, 1.782), (0.045, 1.782), (0.081, 1.805),
        (0.122, 1.855), (0.140, 1.935), (0.138, 2.020),
        (0.117, 2.072), (0.068, 2.095),
    )
    center_z = 1.940
    layers = (
        (-0.142, 1.00, 1.00),
        (0.035, 1.02, 1.02),
        (0.137, 0.88, 0.95),
    )
    vertices = [
        (x * width_scale, y, center_z + (z - center_z) * height_scale)
        for y, width_scale, height_scale in layers
        for x, z in profile
    ]
    count = len(profile)
    faces = [tuple(reversed(range(count)))]
    for layer in range(len(layers) - 1):
        first = layer * count
        following = (layer + 1) * count
        for index in range(count):
            next_index = (index + 1) % count
            faces.append((
                first + index, first + next_index,
                following + next_index, following + index,
            ))
    back = (len(layers) - 1) * count
    faces.append(tuple(back + index for index in range(count)))
    mesh = bpy.data.meshes.new(f"MESH_{PREFIX}Head")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(f"{PREFIX}Head", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(material)


def add_hair(material: bpy.types.Material) -> None:
    """Six full-root clumps; no cap, crown, fringe strip, or loose strands."""
    bpy.ops.mesh.primitive_ico_sphere_add(
        subdivisions=1, radius=1.0, location=(0.0, 0.025, 2.075))
    core = bpy.context.object
    core.name = f"{PREFIX}HairCore"
    core.dimensions = (0.185, 0.175, 0.115)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    core.data.materials.append(material)

    hero.add_hair_clump(
        f"{PREFIX}HairBang_L",
        ((-0.047, -0.050, 2.125), (-0.038, -0.125, 2.085),
         (-0.020, -0.165, 2.020), (0.017, -0.170, 1.965)),
        (0.155, 0.145, 0.100, 0.010),
        (0.095, 0.080, 0.050, 0.010), material,
    )
    hero.add_hair_clump(
        f"{PREFIX}HairBang_R",
        ((0.047, -0.048, 2.122), (0.060, -0.125, 2.082),
         (0.068, -0.165, 2.025), (0.055, -0.170, 1.985)),
        (0.150, 0.140, 0.090, 0.010),
        (0.090, 0.075, 0.045, 0.010), material,
    )
    hero.add_hair_clump(
        f"{PREFIX}HairLongLock",
        ((-0.098, 0.000, 2.118), (-0.132, -0.040, 2.060),
         (-0.150, -0.055, 1.985), (-0.150, -0.030, 1.905),
         (-0.120, 0.000, 1.835)),
        (0.130, 0.120, 0.085, 0.045, 0.010),
        (0.110, 0.095, 0.068, 0.038, 0.010), material,
    )
    hero.add_hair_clump(
        f"{PREFIX}HairShortLock",
        ((0.098, 0.000, 2.115), (0.136, -0.035, 2.060),
         (0.150, -0.050, 2.000), (0.128, -0.022, 1.940)),
        (0.125, 0.112, 0.068, 0.010),
        (0.105, 0.090, 0.055, 0.010), material,
    )
    hero.add_hair_clump(
        f"{PREFIX}HairRear_L",
        ((-0.038, 0.100, 2.118), (-0.068, 0.148, 2.068),
         (-0.089, 0.162, 2.000), (-0.064, 0.162, 1.925),
         (-0.030, 0.148, 1.855)),
        (0.155, 0.165, 0.132, 0.085, 0.010),
        (0.060, 0.055, 0.047, 0.030, 0.010), material,
    )
    hero.add_hair_clump(
        f"{PREFIX}HairRear_R",
        ((0.038, 0.102, 2.115), (0.068, 0.150, 2.066),
         (0.089, 0.164, 2.002), (0.064, 0.164, 1.930),
         (0.030, 0.150, 1.862)),
        (0.150, 0.160, 0.128, 0.082, 0.010),
        (0.058, 0.053, 0.045, 0.028, 0.010), material,
    )


def add_body(material: bpy.types.Material) -> None:

    shapes.add_prism(
        f"{PREFIX}Torso",
        ((-0.195, 1.625), (-0.225, 1.475), (-0.200, 1.030),
         (-0.065, 0.990), (0.065, 0.990), (0.200, 1.030),
         (0.225, 1.475), (0.195, 1.625)),
        0.0, 0.0, 0.31, material,
    )
    shapes.add_prism(
        f"{PREFIX}ShoulderWrap",
        ((-0.270, 1.555), (-0.220, 1.665), (-0.070, 1.710),
         (0.065, 1.695), (0.225, 1.650), (0.270, 1.555),
         (0.185, 1.495), (0.0, 1.510), (-0.185, 1.495)),
        0.0, -0.005, 0.35, material,
    )
    shapes.add_beveled_box(
        f"{PREFIX}Neck", (0.0, 0.0, 1.750),
        (0.105, 0.115, 0.125), material, 0.014,
    )


def add_limbs(material: bpy.types.Material) -> None:
    for side in (-1.0, 1.0):
        suffix = "L" if side < 0.0 else "R"
        arm_y = 0.10 if side < 0.0 else -0.14
        shoulder = (side * 0.270, arm_y, 1.575)
        elbow = (side * 0.410, arm_y, 1.270)
        wrist = (side * 0.500, arm_y - 0.010, 0.970)
        shapes.add_beveled_beam(
            f"{PREFIX}UpperArm_{suffix}", shoulder, elbow,
            0.120, 0.145, material, 0.018,
        )
        shapes.add_beveled_beam(
            f"{PREFIX}Forearm_{suffix}", elbow, wrist,
            0.105, 0.125, material, 0.016,
        )
        shapes.add_beveled_box(
            f"{PREFIX}Hand_{suffix}",
            (side * 0.515, arm_y - 0.020, 0.905),
            (0.098, 0.120, 0.150), material, 0.018,
        )

        hip = (side * 0.125, 0.0, 0.980)
        knee = (side * 0.130, 0.0, 0.555)
        ankle = (side * 0.130, 0.0, 0.135)
        shapes.add_beveled_beam(
            f"{PREFIX}Thigh_{suffix}", hip, knee,
            0.135, 0.150, material, 0.018,
        )
        shapes.add_beveled_beam(
            f"{PREFIX}Shin_{suffix}", knee, ankle,
            0.125, 0.140, material, 0.016,
        )
        shapes.add_beveled_box(
            f"{PREFIX}Boot_{suffix}", (side * 0.130, -0.075, 0.085),
            (0.175, 0.245, 0.135), material, 0.022,
        )


def configure_scene(objects: list[bpy.types.Object]) -> tuple:
    scene = bpy.context.scene
    keep = set(objects)
    for obj in scene.objects:
        obj.hide_render = obj not in keep

    camera_data = bpy.data.cameras.new("CAM_RedoV09Silhouette")
    camera = bpy.data.objects.new("CAM_RedoV09Silhouette", camera_data)
    scene.collection.objects.link(camera)
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 2.35
    scene.camera = camera

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
    background.inputs["Color"].default_value = (0.72, 0.70, 0.66, 1.0)
    background.inputs["Strength"].default_value = 0.8
    return scene, camera


def main() -> None:
    for obj in tuple(bpy.data.objects):
        if obj.name.startswith(PREFIX):
            bpy.data.objects.remove(obj, do_unlink=True)
    material = make_silhouette_material()
    add_body(material)
    add_limbs(material)
    add_head(material)
    add_hair(material)
    objects = [
        obj for obj in bpy.context.scene.objects
        if obj.type == "MESH" and obj.name.startswith(PREFIX)
    ]
    scene, camera = configure_scene(objects)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for label, location in VIEWS.items():
        camera.location = location
        point_at(camera, TARGET)
        scene.render.filepath = str(OUT_DIR / f"{label}.png")
        bpy.ops.render.render(write_still=True)
    print(f"rendered V09 silhouette blockout to {OUT_DIR}")


if __name__ == "__main__":
    main()
