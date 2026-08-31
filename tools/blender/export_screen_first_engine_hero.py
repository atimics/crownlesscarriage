#!/usr/bin/env python3
"""Export the screen-first character on the production engine skeleton."""

from __future__ import annotations

import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import render_screen_first_character_experiments as character
import paint_channels


ROOT = Path(__file__).resolve().parents[2]
EXPORT_PATH = (
    ROOT / "assets" / "exports" / "hero" /
    "crownless_screen_first_engine_rig_v08.glb"
)
MANIFEST_PATH = EXPORT_PATH.with_suffix(".json")
SILHOUETTE_PATH = (
    ROOT / "assets" / "previews" / "experiments" /
    "screen_first_hair_silhouette_v08.png"
)
RIG_NAME = "ARM_CrownlessHero"
PREFIX = "SCREEN_FIRST_"
HAIR_BONES = {
    "hair.long": ((-0.18, -0.08, 2.13), (-0.23, -0.04, 1.76)),
    "hair.rear": ((0.00, 0.12, 2.12), (0.00, 0.18, 1.83)),
}


def reset_rig(rig: bpy.types.Object) -> None:
    rig.animation_data_clear()
    rig.data.pose_position = "REST"
    for bone in rig.pose.bones:
        bone.location = (0.0, 0.0, 0.0)
        bone.rotation_mode = "QUATERNION"
        bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        bone.scale = (1.0, 1.0, 1.0)
    bpy.context.view_layer.update()


def remove_previous_export() -> None:
    for obj in tuple(bpy.data.objects):
        if obj.name.startswith((PREFIX, "SKIN_ScreenFirst")):
            bpy.data.objects.remove(obj, do_unlink=True)


def ensure_hair_bones(rig: bpy.types.Object) -> None:
    """Add two small secondary controls without changing the body rig."""
    bpy.ops.object.select_all(action="DESELECT")
    rig.hide_set(False)
    rig.hide_viewport = False
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    head = rig.data.edit_bones.get("head")
    if head is None:
        raise RuntimeError("screen-first hair requires the head bone")
    for name, (start, end) in HAIR_BONES.items():
        bone = rig.data.edit_bones.get(name)
        if bone is None:
            bone = rig.data.edit_bones.new(name)
        bone.head = start
        bone.tail = end
        bone.roll = 0.0
        bone.parent = head
        bone.use_connect = False
        bone.use_deform = True
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.select_set(False)


def build_materials() -> dict[str, bpy.types.Material]:
    materials: dict[str, bpy.types.Material] = {}
    palette = dict(character.PALETTE)


    engine_colors = {



        "skin": (126, 78, 75),
        "skin_light": (172, 108, 105),
        "hair": (24, 15, 14),
        "hair_mid": (42, 24, 18),
        "hair_highlight": (78, 42, 26),
        "eye": (15, 16, 18),
        "oxblood": (94, 44, 53),
        "oxblood_dark": (66, 36, 43),
        "teal": (39, 104, 101),
        "teal_dark": (27, 63, 64),
        "teal_light": (57, 133, 125),
        "teal_shadow": (27, 63, 64),
        "dark_cloth": (27, 31, 32),
        "leather": (94, 62, 43),
        "gold": (190, 142, 53),
    }
    for name, color in engine_colors.items():
        palette[name] = tuple(channel / 255.0 for channel in color) + (1.0,)
    for name, color in palette.items():
        if name in ("background", "ground"):
            continue
        material = bpy.data.materials.get(f"MAT_screen_first_{name}")
        if material is None:
            material = character.make_material(f"screen_first_{name}", color)
        else:



            material.diffuse_color = color
            material.use_nodes = True
            principled = material.node_tree.nodes.get("Principled BSDF")
            if principled is not None:
                principled.inputs["Base Color"].default_value = color
                principled.inputs["Roughness"].default_value = 0.94
                principled.inputs["Specular IOR Level"].default_value = 0.12
        materials[name] = material
    return materials


def add_segmented_tunic(materials: dict[str, bpy.types.Material]) -> None:
    """Build one tunic volume with non-overlapping authored front values."""
    points = (
        (-0.28, 0.83), (-0.34, 1.49), (-0.28, 1.67), (0.28, 1.67),
        (0.34, 1.49), (0.28, 0.83), (0.07, 0.80), (-0.08, 0.82),
    )
    depth = 0.42
    front_y = -depth * 0.5
    back_y = depth * 0.5
    vertices = [(x, front_y, z) for x, z in points]
    vertices += [(x, back_y, z) for x, z in points]
    center = len(vertices)
    vertices.append((0.0, front_y, 1.20))
    count = len(points)
    faces = [tuple(range(count, count * 2))]
    material_indices = [0]
    for index in range(count):
        next_index = (index + 1) % count
        faces.append((index, next_index, count + next_index, count + index))
        material_indices.append(0)
    front_materials = (1, 1, 0, 2, 2, 2, 0, 1)
    for index, material_index in enumerate(front_materials):
        next_index = (index + 1) % count
        faces.append((center, next_index, index))
        material_indices.append(material_index)
    mesh = bpy.data.meshes.new(f"MESH_{PREFIX}TunicMass")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(f"{PREFIX}TunicMass", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(materials["teal"])
    obj.data.materials.append(materials["teal_light"])
    obj.data.materials.append(materials["teal_shadow"])
    for polygon, material_index in zip(obj.data.polygons, material_indices):
        polygon.material_index = material_index


def build_body(materials: dict[str, bpy.types.Material]) -> None:
    add_segmented_tunic(materials)
    character.add_prism(
        f"{PREFIX}ScarfShoulderWrap",
        ((-0.385, 1.55), (-0.35, 1.66), (-0.17, 1.745), (0.0, 1.71),
         (0.17, 1.745), (0.35, 1.66), (0.385, 1.55), (0.27, 1.455),
         (0.0, 1.43), (-0.27, 1.455)),
        0.0, 0.01, 0.50, materials["oxblood"],
    )
    character.add_prism(
        f"{PREFIX}ScarfFrontFold",
        ((-0.31, 1.56), (-0.20, 1.68), (0.0, 1.63), (0.20, 1.68),
         (0.31, 1.56), (0.18, 1.46), (0.0, 1.43), (-0.18, 1.46)),
        0.0, -0.264, 0.040, materials["oxblood_dark"],
    )


def build_limbs(materials: dict[str, bpy.types.Material]) -> None:
    for side in (-1.0, 1.0):
        suffix = "L" if side < 0.0 else "R"
        shoulder = (side * 0.29, 0.0, 1.58)
        elbow = (side * 0.46, 0.0, 1.26)
        wrist = (side * 0.57, -0.015, 0.94)
        character.add_beveled_beam(
            f"{PREFIX}Sleeve_{suffix}", shoulder, elbow,
            0.175, 0.235, materials["teal_dark"], 0.025,
        )
        character.add_beveled_beam(
            f"{PREFIX}Forearm_{suffix}", elbow, wrist,
            0.145, 0.195, materials["skin_light"], 0.022,
        )
        character.add_beveled_box(
            f"{PREFIX}Hand_{suffix}",
            (side * 0.585, -0.035, 0.84),
            (0.180, 0.225, 0.205), materials["skin_light"], 0.034,
        )

        hip = (side * 0.14, 0.0, 1.0)
        knee = (side * 0.15, 0.0, 0.56)
        ankle = (side * 0.15, 0.0, 0.13)
        character.add_beveled_beam(
            f"{PREFIX}Thigh_{suffix}", hip, knee,
            0.175, 0.220, materials["dark_cloth"], 0.024,
        )
        character.add_beveled_beam(
            f"{PREFIX}Shin_{suffix}", knee, ankle,
            0.160, 0.205, materials["dark_cloth"], 0.022,
        )
        character.add_beveled_box(
            f"{PREFIX}BootCuff_{suffix}",
            (side * 0.15, -0.015, 0.235),
            (0.250, 0.285, 0.180), materials["leather"], 0.032,
        )
        character.add_beveled_box(
            f"{PREFIX}BootFoot_{suffix}",
            (side * 0.15, -0.135, 0.090),
            (0.280, 0.405, 0.170), materials["leather"], 0.036,
        )


def add_hair_clump(
    name: str,
    path: tuple[tuple[float, float, float], ...],
    widths: tuple[float, ...],
    depths: tuple[float, ...],
    material: bpy.types.Material,
    secondary_bone: str | None = None,
) -> bpy.types.Object:
    """Build one flat-shaded diamond clump along a root-to-tip path."""
    if not (3 <= len(path) <= 5):
        raise ValueError(f"{name}: hair clumps need 3 to 5 cross-sections")
    if len(path) != len(widths) or len(path) != len(depths):
        raise ValueError(f"{name}: path, width, and depth counts must match")

    centers = [Vector(point) for point in path]
    vertices: list[tuple[float, float, float]] = []
    for index, center in enumerate(centers):
        before = centers[max(0, index - 1)]
        after = centers[min(len(centers) - 1, index + 1)]
        tangent = (after - before).normalized()
        side = Vector((0.0, 1.0, 0.0)).cross(tangent)
        if side.length_squared < 1.0e-8:
            side = Vector((1.0, 0.0, 0.0))
        side.normalize()
        thickness = tangent.cross(side).normalized()
        half_width = widths[index] * 0.5
        half_depth = depths[index] * 0.5
        vertices.extend((
            tuple(center - side * half_width),
            tuple(center - thickness * half_depth),
            tuple(center + side * half_width),
            tuple(center + thickness * half_depth),
        ))

    faces: list[tuple[int, ...]] = [(3, 2, 1, 0)]
    for section in range(len(path) - 1):
        first = section * 4
        following = (section + 1) * 4
        for edge in range(4):
            next_edge = (edge + 1) % 4
            faces.append((
                first + edge,
                first + next_edge,
                following + next_edge,
                following + edge,
            ))
    final = (len(path) - 1) * 4
    faces.append((final, final + 1, final + 2, final + 3))

    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(material)
    obj["cc_hair_form"] = "tapered_diamond_clump"
    obj["cc_hair_sections"] = len(path)
    if secondary_bone is not None:
        obj["cc_hair_secondary_bone"] = secondary_bone
    return obj


def add_scalp_core(material: bpy.types.Material) -> bpy.types.Object:
    """Make a small core that is covered by the six clump roots."""
    bpy.ops.mesh.primitive_ico_sphere_add(
        subdivisions=2, radius=1.0, location=(0.0, 0.080, 2.110))
    obj = bpy.context.object
    obj.name = f"{PREFIX}HairScalpCore"
    obj.dimensions = (0.26, 0.24, 0.18)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    obj["cc_hair_form"] = "hidden_scalp_core"
    return obj


def add_hair_highlight(material: bpy.types.Material) -> bpy.types.Object:
    """Add one small opaque highlight on the long lock, away from its edge."""
    vertices = (
        (-0.224, -0.132, 2.090),
        (-0.190, -0.138, 2.115),
        (-0.202, -0.151, 2.045),
        (-0.222, -0.145, 2.025),
    )
    mesh = bpy.data.meshes.new(f"MESH_{PREFIX}HairHighlightPlane")
    mesh.from_pydata(vertices, [], ((0, 1, 2, 3),))
    mesh.update()
    obj = bpy.data.objects.new(f"{PREFIX}HairHighlightPlane", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(material)
    obj["cc_hair_form"] = "single_highlight_plane"
    return obj


def add_faceted_head(skin_material: bpy.types.Material) -> bpy.types.Object:
    """Build a clean anime face plane with a rounded rear skull."""



    profile = (
        (-0.095, 2.150), (-0.165, 2.115), (-0.195, 2.045),
        (-0.198, 1.940), (-0.170, 1.840), (-0.115, 1.780),
        (-0.065, 1.755), (0.065, 1.755), (0.115, 1.780),
        (0.170, 1.840),
        (0.198, 1.940), (0.195, 2.045), (0.165, 2.115),
        (0.095, 2.150),
    )
    center_z = 1.950
    layers = (
        (-0.215, 1.00, 1.00),
        (0.055, 1.03, 1.02),
        (0.180, 0.88, 0.94),
    )
    vertices: list[tuple[float, float, float]] = []
    for y, width_scale, height_scale in layers:
        vertices.extend((
            x * width_scale,
            y,
            center_z + (z - center_z) * height_scale,
        ) for x, z in profile)

    count = len(profile)
    faces: list[tuple[int, ...]] = [tuple(reversed(range(count)))]
    for layer in range(len(layers) - 1):
        first = layer * count
        following = (layer + 1) * count
        for side in range(count):
            next_side = (side + 1) % count
            faces.append((
                first + side,
                first + next_side,
                following + next_side,
                following + side,
            ))
    back = (len(layers) - 1) * count
    faces.append(tuple(back + side for side in range(count)))

    mesh = bpy.data.meshes.new(f"MESH_{PREFIX}Head")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(f"{PREFIX}Head", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(skin_material)
    obj["cc_head_form"] = "faceted_oval_tapered_jaw"
    return obj


def build_hair(materials: dict[str, bpy.types.Material]) -> None:
    """Build the V08 silhouette from six broad, separated hair clumps."""
    add_scalp_core(materials["hair"])
    add_hair_clump(
        f"{PREFIX}HairBang_L",
        ((-0.070, -0.080, 2.210), (-0.060, -0.180, 2.160),
         (-0.040, -0.238, 2.075), (0.020, -0.248, 1.990)),
        (0.220, 0.215, 0.175, 0.016),
        (0.140, 0.120, 0.080, 0.014),
        materials["hair"],
    )
    add_hair_clump(
        f"{PREFIX}HairBang_R",
        ((0.070, -0.075, 2.200), (0.090, -0.180, 2.145),
         (0.090, -0.238, 2.070), (0.070, -0.248, 2.005)),
        (0.210, 0.200, 0.155, 0.016),
        (0.135, 0.115, 0.075, 0.014),
        materials["hair_mid"],
    )
    add_hair_clump(
        f"{PREFIX}HairLongLock",
        ((-0.145, -0.010, 2.185), (-0.205, -0.065, 2.115),
         (-0.235, -0.090, 2.020), (-0.235, -0.060, 1.915),
         (-0.185, -0.005, 1.815)),
        (0.190, 0.175, 0.130, 0.075, 0.015),
        (0.165, 0.145, 0.105, 0.065, 0.014),
        materials["hair"],
        "hair.long",
    )
    add_hair_clump(
        f"{PREFIX}HairShortLock",
        ((0.145, -0.005, 2.180), (0.200, -0.055, 2.115),
         (0.225, -0.080, 2.035), (0.190, -0.035, 1.955)),
        (0.180, 0.160, 0.095, 0.014),
        (0.155, 0.135, 0.075, 0.012),
        materials["hair_mid"],
    )
    add_hair_clump(
        f"{PREFIX}HairRearWedge_L",
        ((-0.055, 0.205, 2.165), (-0.100, 0.220, 2.085),
         (-0.130, 0.230, 1.995), (-0.090, 0.230, 1.900),
         (-0.045, 0.215, 1.810)),
        (0.220, 0.250, 0.240, 0.180, 0.012),
        (0.080, 0.075, 0.060, 0.040, 0.014),
        materials["hair"],
        "hair.rear",
    )
    add_hair_clump(
        f"{PREFIX}HairRearWedge_R",
        ((0.055, 0.207, 2.160), (0.100, 0.222, 2.085),
         (0.130, 0.232, 2.000), (0.090, 0.232, 1.910),
         (0.045, 0.218, 1.825)),
        (0.215, 0.245, 0.235, 0.175, 0.012),
        (0.078, 0.072, 0.058, 0.038, 0.014),
        materials["hair_mid"],
        "hair.rear",
    )
    add_hair_highlight(materials["hair_highlight"])


def build_head(materials: dict[str, bpy.types.Material]) -> None:
    add_faceted_head(materials["skin_light"])
    build_hair(materials)
    for side in (-1.0, 1.0):
        character.add_beveled_box(
            f"{PREFIX}Eye_{'L' if side < 0.0 else 'R'}",
            (side * 0.075, -0.222, 1.95),
            (0.035, 0.016, 0.052), materials["eye"], 0.006,
        )
        character.add_box(
            f"{PREFIX}Brow_{'L' if side < 0.0 else 'R'}",
            (side * 0.075, -0.224, 1.995),
            (0.058, 0.012, 0.012), materials["hair"],
        )
    character.add_box(
        f"{PREFIX}Mouth", (0.0, -0.223, 1.855),
        (0.060, 0.014, 0.020), materials["skin"],
    )
    character.add_beveled_box(
        f"{PREFIX}HeadEar_R", (0.203, -0.002, 1.955),
        (0.045, 0.145, 0.090), materials["skin"], 0.014,
    )
    crown_x = 0.060
    character.add_box(
        f"{PREFIX}CrownBand", (crown_x, 0.035, 2.215),
        (0.130, 0.090, 0.025), materials["gold"],
    )
    for index, (offset, height) in enumerate(
        ((-0.040, 0.050), (0.0, 0.075), (0.040, 0.045))
    ):
        character.add_box(
            f"{PREFIX}CrownProng_{index}",
            (crown_x + offset, 0.035, 2.228 + height * 0.5),
            (0.016, 0.090, height), materials["gold"],
        )


def bone_for_object(name: str) -> str:
    if "HairLongLock" in name:
        return "hair.long"
    if "HairRearWedge" in name:
        return "hair.rear"
    if any(token in name for token in
           ("Head", "Hair", "Eye", "Brow", "Mouth", "Crown")):
        return "head"
    if "Scarf" in name:
        return "chest"
    if "Sleeve_L" in name:
        return "upper_arm.L"
    if "Sleeve_R" in name:
        return "upper_arm.R"
    if "Forearm_L" in name:
        return "forearm.L"
    if "Forearm_R" in name:
        return "forearm.R"
    if "Hand_L" in name or "HandShadow_L" in name:
        return "hand.L"
    if "Hand_R" in name or "HandShadow_R" in name:
        return "hand.R"
    if "Thigh_L" in name:
        return "thigh.L"
    if "Thigh_R" in name:
        return "thigh.R"
    if "Shin_L" in name or "BootCuff_L" in name:
        return "shin.L"
    if "Shin_R" in name or "BootCuff_R" in name:
        return "shin.R"
    if "BootFoot_L" in name:
        return "foot.L"
    if "BootFoot_R" in name:
        return "foot.R"
    return "spine"


def apply_shape_modifiers(obj: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.hide_viewport = False
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for modifier in tuple(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def skin_object(obj: bpy.types.Object, rig: bpy.types.Object) -> str:
    bone_name = bone_for_object(obj.name)
    apply_shape_modifiers(obj)
    obj.vertex_groups.clear()
    secondary_bone = obj.get("cc_hair_secondary_bone")
    section_count = int(obj.get("cc_hair_sections", 0))
    if secondary_bone and section_count >= 3:
        head_group = obj.vertex_groups.new(name="head")
        tip_group = obj.vertex_groups.new(name=str(secondary_bone))
        for section in range(section_count):
            amount = section / (section_count - 1)
            tip_weight = 0.92 * amount * amount
            indices = tuple(range(section * 4, section * 4 + 4))
            head_group.add(indices, 1.0 - tip_weight, "REPLACE")
            if tip_weight > 0.0:
                tip_group.add(indices, tip_weight, "REPLACE")
        weight_label = f"head->{secondary_bone}"
    else:
        group = obj.vertex_groups.new(name=bone_name)
        group.add(tuple(range(len(obj.data.vertices))), 1.0, "REPLACE")
        weight_label = bone_name
    armature = obj.modifiers.new("CC_ScreenFirstSkin", "ARMATURE")
    armature.object = rig
    world = obj.matrix_world.copy()
    obj.parent = rig
    obj.matrix_parent_inverse = rig.matrix_world.inverted()
    obj.matrix_world = world
    obj["cc_engine_bone"] = weight_label
    return weight_label


def consolidate(objects: list[bpy.types.Object],
                rig: bpy.types.Object) -> bpy.types.Object:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    combined = objects[0]
    bpy.context.view_layer.objects.active = combined
    bpy.ops.object.join()
    combined.name = "SKIN_ScreenFirst_Runtime"
    material_names = [material.name if material is not None else "neutral"
                      for material in combined.data.materials]
    polygon_materials = [polygon.material_index
                         for polygon in combined.data.polygons]
    paint_channels.add_indexed_paint_channels(
        combined, polygon_materials, material_names)
    combined.parent = rig
    armatures = [modifier for modifier in combined.modifiers
                 if modifier.type == "ARMATURE"]
    if not armatures:
        armature = combined.modifiers.new("CC_ScreenFirstSkin", "ARMATURE")
        armature.object = rig
    else:
        armatures[0].object = rig
        for redundant in armatures[1:]:
            combined.modifiers.remove(redundant)
    return combined


def render_silhouette(combined: bpy.types.Object,
                      rig: bpy.types.Object) -> None:
    """Render the whole hero as a 60-pixel black silhouette."""
    scene = bpy.context.scene
    hidden = {obj: obj.hide_render for obj in scene.objects}
    for obj in scene.objects:
        obj.hide_render = obj != combined
    rig.hide_render = True

    camera_data = bpy.data.cameras.new("CAM_ScreenFirstHairSilhouetteV08")
    camera = bpy.data.objects.new("CAM_ScreenFirstHairSilhouetteV08", camera_data)
    scene.collection.objects.link(camera)
    camera.location = (4.8, -7.2, 3.4)
    camera.rotation_euler = (
        Vector((0.0, 0.0, 1.15)) - camera.location
    ).to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 9.20
    scene.camera = camera

    silhouette = bpy.data.materials.new("MAT_screen_first_silhouette_black")
    silhouette.diffuse_color = (0.0, 0.0, 0.0, 1.0)
    silhouette.use_nodes = True
    principled = silhouette.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (0.0, 0.0, 0.0, 1.0)
    principled.inputs["Roughness"].default_value = 1.0
    combined.data.materials.clear()
    combined.data.materials.append(silhouette)
    for polygon in combined.data.polygons:
        polygon.material_index = 0

    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 240
    scene.render.resolution_y = 240
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.78, 0.77, 0.72, 1.0)
    background.inputs["Strength"].default_value = 0.8
    SILHOUETTE_PATH.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(SILHOUETTE_PATH)
    bpy.ops.render.render(write_still=True)

    for obj, was_hidden in hidden.items():
        if obj.name in bpy.data.objects:
            obj.hide_render = was_hidden
    bpy.data.objects.remove(camera, do_unlink=True)
    print(f"rendered 60-pixel silhouette to {SILHOUETTE_PATH}")


def export() -> None:
    export_layer = bpy.context.scene.view_layers.get("CC_EngineExport")
    if export_layer is None:
        export_layer = bpy.context.scene.view_layers.new(name="CC_EngineExport")
    bpy.context.window.view_layer = export_layer
    rig = bpy.data.objects.get(RIG_NAME)
    if rig is None or rig.type != "ARMATURE":
        raise RuntimeError(f"missing armature {RIG_NAME}")
    reset_rig(rig)
    ensure_hair_bones(rig)
    remove_previous_export()
    materials = build_materials()
    build_body(materials)
    build_limbs(materials)
    build_head(materials)

    objects = [obj for obj in bpy.context.scene.objects
               if obj.type == "MESH" and obj.name.startswith(PREFIX)]
    if not objects:
        raise RuntimeError("screen-first geometry was not created")
    entries = [{"name": obj.name, "bone": skin_object(obj, rig)}
               for obj in objects]
    combined = consolidate(objects, rig)

    EXPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    rig.hide_set(False)
    rig.hide_viewport = False
    rig.select_set(True)
    combined.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.gltf(
        filepath=str(EXPORT_PATH),
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_animations=False,
        export_skins=True,
        export_morph=False,
        export_extras=True,
        export_materials="EXPORT",
        export_vertex_color="ACTIVE",
    )
    render_silhouette(combined, rig)

    manifest = {
        "asset": str(EXPORT_PATH.relative_to(ROOT)),
        "armature": RIG_NAME,
        "art_direction": "screen_first_tapered_hair_clumps_v08",
        "motion_source": "CcHumanoidSkinPoseResolve plus cape-delayed hair tips (runtime only)",
        "runtime_layout": {
            "authored_objects": len(entries),
            "skinned_objects": 1,
            "strategy": "joined skin with rigid body weights and quadratic hair tip blends",
        },
        "paint_contract": "COLOR_0 stores material index, authored value, and fold strength",
        "hair_contract": {
            "scalp_cores": 1,
            "opaque_clumps": 6,
            "cross_sections": "3-5 per clump",
            "highlight_planes": 1,
            "secondary_bones": list(HAIR_BONES),
            "root_weights": "rigid head roots with quadratic tip blend",
        },
        "bones": [bone.name for bone in rig.data.bones],
        "components": entries,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"exported {len(entries)} screen-first pieces to {EXPORT_PATH}")


if __name__ == "__main__":
    export()
