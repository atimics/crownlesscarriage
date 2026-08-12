#!/usr/bin/env python3
"""Build and animate a self-contained Crownless hero action reel in Blender."""

from __future__ import annotations

import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_hero_actions.blend"
FRAMES_DIR = Path("/tmp/crownless_hero_action_frames")
PREVIEW_DIR = Path("/tmp/crownless_hero_action_previews")
FRAME_END = 240
MATERIALS: dict[str, bpy.types.Material] = {}

BONES = (
    ("root", None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.18)),
    ("pelvis", "root", (0.0, 0.0, 0.90), (0.0, 0.0, 1.10)),
    ("spine", "pelvis", (0.0, 0.0, 1.08), (0.0, 0.0, 1.38)),
    ("chest", "spine", (0.0, 0.0, 1.36), (0.0, 0.0, 1.62)),
    ("neck", "chest", (0.0, 0.0, 1.60), (0.0, 0.0, 1.76)),
    ("head", "neck", (0.0, 0.0, 1.74), (0.0, 0.0, 2.08)),
    ("upper_arm.L", "chest", (-0.29, 0.0, 1.58), (-0.46, 0.0, 1.26)),
    ("forearm.L", "upper_arm.L", (-0.46, 0.0, 1.26), (-0.57, -0.015, 0.94)),
    ("hand.L", "forearm.L", (-0.57, -0.015, 0.94), (-0.58, -0.045, 0.78)),
    ("upper_arm.R", "chest", (0.29, 0.0, 1.58), (0.46, 0.0, 1.26)),
    ("forearm.R", "upper_arm.R", (0.46, 0.0, 1.26), (0.57, -0.015, 0.94)),
    ("hand.R", "forearm.R", (0.57, -0.015, 0.94), (0.58, -0.045, 0.78)),
    ("thigh.L", "pelvis", (-0.14, 0.0, 1.00), (-0.15, 0.0, 0.56)),
    ("shin.L", "thigh.L", (-0.15, 0.0, 0.56), (-0.15, 0.0, 0.13)),
    ("foot.L", "shin.L", (-0.15, 0.0, 0.13), (-0.15, -0.27, 0.08)),
    ("thigh.R", "pelvis", (0.14, 0.0, 1.00), (0.15, 0.0, 0.56)),
    ("shin.R", "thigh.R", (0.15, 0.0, 0.56), (0.15, 0.0, 0.13)),
    ("foot.R", "shin.R", (0.15, 0.0, 0.13), (0.15, -0.27, 0.08)),
    ("cape.0", "chest", (0.0, 0.205, 1.64), (0.0, 0.245, 1.37)),
    ("cape.1", "cape.0", (0.0, 0.245, 1.37), (0.0, 0.290, 1.10)),
    ("cape.2", "cape.1", (0.0, 0.290, 1.10), (0.0, 0.340, 0.82)),
    ("cape.3", "cape.2", (0.0, 0.340, 0.82), (0.0, 0.390, 0.54)),
)

CONTROL_BONES = tuple(name for name, _parent, _head, _tail in BONES if name != "root")


def reset_scene() -> bpy.types.Collection:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_HERO_ACTION_REEL"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 512
    scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.fps = 24
    scene.frame_start = 1
    scene.frame_end = FRAME_END
    scene.view_settings.look = "AgX - Medium High Contrast"
    world = bpy.data.worlds.new("CC_ActionWorld")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.012, 0.016, 0.020, 1.0)
    background.inputs["Strength"].default_value = 0.50
    scene.world = world
    root = bpy.data.collections.new("CC_HERO_ACTION_LIBRARY")
    scene.collection.children.link(root)
    return root


def make_material(name: str, color: tuple[float, float, float, float],
                  metallic: float = 0.0, roughness: float = 0.62,
                  emission: float = 0.0) -> None:
    mat = bpy.data.materials.new(f"MAT_{name.upper()}")
    mat.diffuse_color = color
    mat.use_nodes = True
    node = mat.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value = color
    node.inputs["Metallic"].default_value = metallic
    node.inputs["Roughness"].default_value = roughness
    if emission:
        node.inputs["Emission Color"].default_value = color
        node.inputs["Emission Strength"].default_value = emission
    MATERIALS[name] = mat


def make_palette() -> None:
    make_material("skin", (0.55, 0.27, 0.14, 1.0))
    make_material("skin_light", (0.72, 0.40, 0.23, 1.0))
    make_material("muscle", (0.31, 0.085, 0.055, 1.0), roughness=0.76)
    make_material("hair", (0.10, 0.04, 0.02, 1.0))
    make_material("eye", (0.01, 0.015, 0.018, 1.0))
    make_material("teal", (0.025, 0.24, 0.28, 1.0))
    make_material("teal_light", (0.055, 0.39, 0.42, 1.0))
    make_material("teal_dark", (0.012, 0.11, 0.14, 1.0))
    make_material("padding", (0.73, 0.65, 0.50, 1.0))
    make_material("padding_dark", (0.25, 0.20, 0.15, 1.0))
    make_material("cape", (0.25, 0.10, 0.34, 1.0))
    make_material("cape_light", (0.47, 0.26, 0.58, 1.0))
    make_material("leather", (0.22, 0.08, 0.03, 1.0))
    make_material("leather_light", (0.45, 0.20, 0.075, 1.0))
    make_material("steel", (0.10, 0.13, 0.145, 1.0), 0.80, 0.27)
    make_material("steel_light", (0.30, 0.36, 0.37, 1.0), 0.75, 0.24)
    make_material("steel_dark", (0.04, 0.055, 0.06, 1.0), 0.65, 0.30)
    make_material("brass", (0.70, 0.43, 0.075, 1.0), 0.78, 0.26)
    make_material("water", (0.03, 0.34, 0.52, 1.0), 0.10, 0.20)
    make_material("cyan", (0.06, 0.85, 1.0, 1.0), emission=0.8)
    make_material("stage", (0.13, 0.15, 0.16, 1.0))


def finish(obj: bpy.types.Object, collection: bpy.types.Collection,
           mat: str, component: str) -> bpy.types.Object:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[mat])
    obj["cc_component"] = component
    return obj


def bevel(obj: bpy.types.Object, width: float) -> None:
    modifier = obj.modifiers.new("CC_Bevel", "BEVEL")
    modifier.width = width
    modifier.segments = 2
    modifier.limit_method = "ANGLE"


def cube(name: str, location: tuple[float, float, float],
         dimensions: tuple[float, float, float], mat: str,
         collection: bpy.types.Collection, component: str,
         rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
         bevel_width: float = 0.018) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel_width:
        bevel(obj, min(bevel_width, min(dimensions) * 0.22))
    return finish(obj, collection, mat, component)


def ico(name: str, location: tuple[float, float, float],
        scale: tuple[float, float, float], mat: str,
        collection: bpy.types.Collection, component: str,
        subdivisions: int = 1) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(
        subdivisions=subdivisions, radius=1.0, location=location,
    )
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, collection, mat, component)


def cylinder_between(name: str, start: tuple[float, float, float],
                     end: tuple[float, float, float], radius1: float,
                     radius2: float, mat: str, collection: bpy.types.Collection,
                     component: str, vertices: int = 8) -> bpy.types.Object:
    a, b = Vector(start), Vector(end)
    delta = b - a
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices, radius1=radius1, radius2=radius2,
        depth=delta.length, location=(a + b) * 0.5,
    )
    obj = bpy.context.object
    obj.name = name
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = delta.to_track_quat("Z", "Y")
    bevel(obj, 0.008)
    return finish(obj, collection, mat, component)


def torus(name: str, location: tuple[float, float, float], major: float,
          minor: float, mat: str, collection: bpy.types.Collection,
          component: str, rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
          scale: tuple[float, float, float] = (1.0, 1.0, 1.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_torus_add(
        major_segments=16, minor_segments=6, major_radius=major,
        minor_radius=minor, location=location, rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, collection, mat, component)


def loft(name: str, rings: list[tuple[float, float, float]], mat: str,
         collection: bpy.types.Collection, component: str,
         segments: int = 8) -> bpy.types.Object:
    vertices: list[tuple[float, float, float]] = []
    for z, rx, ry in rings:
        for index in range(segments):
            angle = math.tau * index / segments
            vertices.append((math.cos(angle) * rx, math.sin(angle) * ry, z))
    faces: list[tuple[int, ...]] = []
    for ring in range(len(rings) - 1):
        for index in range(segments):
            nxt = (index + 1) % segments
            a = ring * segments + index
            b = ring * segments + nxt
            c = (ring + 1) * segments + nxt
            d = (ring + 1) * segments + index
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(segments))))
    top = (len(rings) - 1) * segments
    faces.append(tuple(top + index for index in range(segments)))
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[mat])
    obj["cc_component"] = component
    bevel(obj, 0.014)
    return obj


def panel(name: str, vertices: list[tuple[float, float, float]], mat: str,
          collection: bpy.types.Collection, component: str,
          thickness: float = 0.018) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], [tuple(range(len(vertices)))])
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS[mat])
    obj["cc_component"] = component
    solidify = obj.modifiers.new("CC_Solidify", "SOLIDIFY")
    solidify.thickness = thickness
    bevel(obj, 0.008)
    return obj


def parent_to_bone(obj: bpy.types.Object, armature: bpy.types.Object,
                   bone_name: str) -> None:
    bpy.context.view_layer.update()
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.parent_type = "BONE"
    obj.parent_bone = bone_name
    obj.matrix_world = world


def smooth_skin(obj: bpy.types.Object, armature: bpy.types.Object,
                vertex_weights: list[dict[str, float]]) -> None:
    if len(vertex_weights) != len(obj.data.vertices):
        raise ValueError(f"weight count mismatch for {obj.name}")
    for vertex_index, weights in enumerate(vertex_weights):
        total = sum(weights.values())
        if total <= 0.0:
            raise ValueError(f"vertex {vertex_index} has no skin weights")
        for bone_name, amount in weights.items():
            if amount <= 0.0:
                continue
            group = obj.vertex_groups.get(bone_name)
            if group is None:
                group = obj.vertex_groups.new(name=bone_name)
            group.add((vertex_index,), amount / total, "REPLACE")
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.parent_type = "OBJECT"
    obj.matrix_parent_inverse = armature.matrix_world.inverted()
    obj.matrix_world = world
    modifier = obj.modifiers.new("CC_SmoothEngineSkin", "ARMATURE")
    modifier.object = armature
    obj["cc_smooth_skin"] = True


def height_skin(obj: bpy.types.Object, armature: bpy.types.Object,
                controls: list[tuple[float, str]]) -> None:
    weights: list[dict[str, float]] = []
    for vertex in obj.data.vertices:
        z = vertex.co.z
        if z <= controls[0][0]:
            weights.append({controls[0][1]: 1.0})
            continue
        if z >= controls[-1][0]:
            weights.append({controls[-1][1]: 1.0})
            continue
        for index in range(len(controls) - 1):
            lower_z, lower_bone = controls[index]
            upper_z, upper_bone = controls[index + 1]
            if lower_z <= z <= upper_z:
                blend = (z - lower_z) / (upper_z - lower_z)
                weights.append({lower_bone: 1.0 - blend, upper_bone: blend})
                break
    smooth_skin(obj, armature, weights)


def chain_tube(name: str, points: list[tuple[float, float, float]],
               radii: list[float], bones: tuple[str, str],
               collection: bpy.types.Collection,
               armature: bpy.types.Object) -> bpy.types.Object:
    if len(points) != 5 or len(radii) != 5:
        raise ValueError("smooth chain tubes use five control rings")
    segments = 8
    vertices: list[tuple[float, float, float]] = []
    ring_weights = ((1.0, 0.0), (0.78, 0.22), (0.5, 0.5),
                    (0.22, 0.78), (0.0, 1.0))
    weights: list[dict[str, float]] = []
    centers = [Vector(point) for point in points]
    for ring, center in enumerate(centers):
        before = centers[max(0, ring - 1)]
        after = centers[min(len(centers) - 1, ring + 1)]
        tangent = (after - before).normalized()
        reference = Vector((0.0, 1.0, 0.0))
        if abs(tangent.dot(reference)) > 0.92:
            reference = Vector((1.0, 0.0, 0.0))
        axis_a = tangent.cross(reference).normalized()
        axis_b = tangent.cross(axis_a).normalized()
        for segment in range(segments):
            angle = math.tau * segment / segments
            point = center + (axis_a * math.cos(angle) +
                              axis_b * math.sin(angle)) * radii[ring]
            vertices.append(tuple(point))
            first, second = ring_weights[ring]
            weights.append({bones[0]: first, bones[1]: second})
    faces: list[tuple[int, ...]] = []
    for ring in range(len(centers) - 1):
        for segment in range(segments):
            nxt = (segment + 1) % segments
            a = ring * segments + segment
            b = ring * segments + nxt
            c = (ring + 1) * segments + nxt
            d = (ring + 1) * segments + segment
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(segments))))
    top = (len(centers) - 1) * segments
    faces.append(tuple(top + index for index in range(segments)))
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(MATERIALS["muscle"])
    obj["cc_component"] = "muscle_underlayer"
    smooth_skin(obj, armature, weights)
    return obj


def armature(collection: bpy.types.Collection) -> bpy.types.Object:
    data = bpy.data.armatures.new("ARM_CrownlessHero")
    obj = bpy.data.objects.new("ARM_CrownlessHero", data)
    collection.objects.link(obj)
    obj.show_in_front = True
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    created = {}
    for name, parent_name, head, tail in BONES:
        bone = data.edit_bones.new(name)
        bone.head, bone.tail = head, tail
        if parent_name:
            bone.parent = created[parent_name]
        created[name] = bone
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.select_set(False)
    return obj


def build_hero(collection: bpy.types.Collection, rig: bpy.types.Object) -> list[bpy.types.Object]:
    cape_objects: list[bpy.types.Object] = []
    muscle_torso = loft(
        "GEO_MuscleTorso",
        [(.92, .16, .12), (1.08, .20, .14), (1.30, .25, .16),
         (1.50, .29, .18), (1.60, .18, .11)],
        "muscle", collection, "muscle_underlayer",
    )
    height_skin(muscle_torso, rig,
                [(.96, "pelvis"), (1.22, "spine"), (1.50, "chest")])
    torso = loft("GEO_TunicTorso", [(1.03, .27, .17), (1.25, .31, .19),
                 (1.48, .37, .22), (1.62, .405, .23), (1.67, .14, .11)],
                 "teal", collection, "tunic")
    parent_to_bone(torso, rig, "chest")
    cuirass = loft("GEO_Cuirass", [(1.14, .285, .195), (1.31, .33, .215),
                  (1.50, .39, .235), (1.61, .41, .24), (1.65, .15, .12)],
                  "steel", collection, "cuirass")
    parent_to_bone(cuirass, rig, "chest")
    plate = panel("GEO_CuirassPlate", [(-.245, -.247, 1.59), (.245, -.247, 1.59),
                  (.225, -.25, 1.35), (.145, -.24, 1.22),
                  (-.145, -.24, 1.22), (-.225, -.25, 1.35)],
                  "steel_light", collection, "cuirass", .035)
    parent_to_bone(plate, rig, "chest")
    for name, z, width in (("Upper", 1.585, .46), ("Lower", 1.225, .29)):
        trim = cube(f"GEO_CuirassTrim{name}", (0.0, -.275, z),
                    (width, .026, .026), "brass", collection, "cuirass", bevel_width=.006)
        parent_to_bone(trim, rig, "chest")
    emblem = ico("GEO_Emblem", (0.0, -.292, 1.42), (.045, .014, .065),
                  "brass", collection, "cuirass")
    parent_to_bone(emblem, rig, "chest")
    neck = cylinder_between("GEO_Neck", (0, 0, 1.61), (0, 0, 1.77),
                            .085, .09, "skin", collection, "body")
    parent_to_bone(neck, rig, "neck")
    head = ico("GEO_Head", (0, 0, 1.92), (.14, .122, .18),
               "skin", collection, "body", 2)
    parent_to_bone(head, rig, "head")
    jaw = ico("GEO_Jaw", (0, -.01, 1.845), (.115, .105, .09),
              "skin", collection, "body")
    parent_to_bone(jaw, rig, "head")
    nose = ico("GEO_Nose", (0, -.128, 1.915), (.025, .03, .04),
               "skin_light", collection, "body")
    parent_to_bone(nose, rig, "head")
    for suffix, x in (("L", -.052), ("R", .052)):
        eye = ico(f"GEO_Eye{suffix}", (x, -.121, 1.95), (.017, .009, .014),
                  "eye", collection, "body")
        parent_to_bone(eye, rig, "head")
    hair_cap = ico("GEO_HairCap", (0, .012, 2.025), (.158, .137, .125),
                   "hair", collection, "hair", 2)
    parent_to_bone(hair_cap, rig, "head")
    bun = ico("GEO_HairBun", (0, .15, 1.965), (.105, .085, .09),
              "hair", collection, "hair")
    parent_to_bone(bun, rig, "head")
    for index, (x, top, bottom) in enumerate(((-.115, 2.075, 1.985),
            (-.06, 2.12, 2.015), (0, 2.125, 2.045),
            (.06, 2.115, 2.01), (.115, 2.065, 1.975))):
        lock = cylinder_between(f"GEO_HairLock{index}", (x*.82, -.105, top),
                                (x, -.137, bottom), .029, .016,
                                "hair", collection, "hair", 6)
        parent_to_bone(lock, rig, "head")

    for suffix, sign in (("L", -1.0), ("R", 1.0)):
        shoulder = Vector((sign*.29, 0.0, 1.57))
        elbow = Vector((sign*.46, 0.0, 1.26))
        hand = Vector((sign*.57, -.015, .94))
        chain_tube(
            f"GEO_MuscleArm{suffix}",
            [tuple(shoulder), tuple(shoulder.lerp(elbow, .78)), tuple(elbow),
             tuple(elbow.lerp(hand, .22)), tuple(hand)],
            [.083, .073, .069, .062, .052],
            (f"upper_arm.{suffix}", f"forearm.{suffix}"), collection, rig,
        )
        hip = Vector((sign*.14, 0.0, 1.0))
        knee_point = Vector((sign*.15, 0.0, .56))
        ankle = Vector((sign*.15, 0.0, .13))
        chain_tube(
            f"GEO_MuscleLeg{suffix}",
            [tuple(hip), tuple(hip.lerp(knee_point, .78)), tuple(knee_point),
             tuple(knee_point.lerp(ankle, .22)), tuple(ankle)],
            [.102, .092, .086, .075, .064],
            (f"thigh.{suffix}", f"shin.{suffix}"), collection, rig,
        )
        upper = cylinder_between(f"GEO_UpperArm{suffix}", (sign*.29, 0, 1.57),
                    (sign*.46, 0, 1.26), .105, .086, "padding", collection, "padding")
        parent_to_bone(upper, rig, f"upper_arm.{suffix}")
        sleeve = cylinder_between(f"GEO_Sleeve{suffix}", (sign*.30, -.002, 1.57),
                    (sign*.405, -.005, 1.38), .137, .116,
                    "teal_light", collection, "tunic")
        parent_to_bone(sleeve, rig, f"upper_arm.{suffix}")
        forearm = cylinder_between(f"GEO_Forearm{suffix}", (sign*.46, 0, 1.26),
                    (sign*.57, -.015, .94), .083, .063,
                    "padding", collection, "padding")
        parent_to_bone(forearm, rig, f"forearm.{suffix}")
        bracer = cylinder_between(f"GEO_Bracer{suffix}", (sign*.49, -.015, 1.18),
                    (sign*.555, -.025, .98), .095, .075,
                    "leather_light", collection, "bracer")
        parent_to_bone(bracer, rig, f"forearm.{suffix}")
        splint = cylinder_between(f"GEO_BracerSplint{suffix}",
                    (sign*.495, -.09, 1.17), (sign*.555, -.085, 1.00),
                    .018, .014, "steel_light", collection, "bracer", 6)
        parent_to_bone(splint, rig, f"forearm.{suffix}")
        glove = ico(f"GEO_Glove{suffix}", (sign*.58, -.025, .85),
                    (.08, .065, .115), "leather", collection, "glove")
        parent_to_bone(glove, rig, f"hand.{suffix}")
        cuff = cube(f"GEO_GloveCuff{suffix}", (sign*.565, -.02, .94),
                    (.15, .12, .055), "steel_light", collection, "glove", bevel_width=.012)
        parent_to_bone(cuff, rig, f"hand.{suffix}")
        pauldron = ico(f"GEO_Pauldron{suffix}", (sign*.355, 0, 1.57),
                       (.21, .175, .125), "steel", collection, "pauldron")
        parent_to_bone(pauldron, rig, f"upper_arm.{suffix}")
        ridge = ico(f"GEO_PauldronRidge{suffix}", (sign*.36, -.02, 1.62),
                    (.185, .155, .045), "brass", collection, "pauldron")
        parent_to_bone(ridge, rig, f"upper_arm.{suffix}")
        thigh = cylinder_between(f"GEO_Thigh{suffix}", (sign*.14, 0, 1.0),
                    (sign*.15, 0, .56), .125, .095,
                    "padding_dark", collection, "body")
        parent_to_bone(thigh, rig, f"thigh.{suffix}")
        shin = cylinder_between(f"GEO_Shin{suffix}", (sign*.15, 0, .56),
                    (sign*.15, 0, .13), .10, .075,
                    "steel", collection, "greave")
        parent_to_bone(shin, rig, f"shin.{suffix}")
        knee = ico(f"GEO_Knee{suffix}", (sign*.15, -.075, .56),
                   (.115, .06, .105), "steel_light", collection, "greave")
        parent_to_bone(knee, rig, f"shin.{suffix}")
        boot = cube(f"GEO_Boot{suffix}", (sign*.15, -.105, .115),
                    (.225, .365, .19), "leather_light", collection, "boot", bevel_width=.04)
        parent_to_bone(boot, rig, f"foot.{suffix}")
        sole = cube(f"GEO_Sole{suffix}", (sign*.15, -.12, .045),
                    (.235, .38, .055), "padding_dark", collection, "boot", bevel_width=.012)
        parent_to_bone(sole, rig, f"foot.{suffix}")

    panels = (("Front", [(-.24,-.205,1.08),(.24,-.205,1.08),(.20,-.215,.70),(-.20,-.215,.70)]),
              ("Back", [( .24,.19,1.08),(-.24,.19,1.08),(-.20,.21,.72),(.20,.21,.72)]),
              ("Left", [(-.27,.14,1.08),(-.27,-.14,1.08),(-.24,-.15,.74),(-.24,.15,.74)]),
              ("Right", [(.27,-.14,1.08),(.27,.14,1.08),(.24,.15,.74),(.24,-.15,.74)]))
    for name, vertices in panels:
        piece = panel(f"GEO_Tunic{name}", vertices, "teal", collection, "tunic")
        parent_to_bone(piece, rig, "pelvis")
    for name, loc, dims in (("Front", (0,-.19,1.02),(.52,.06,.075)),
                             ("Back", (0,.19,1.02),(.52,.06,.075)),
                             ("Left", (-.275,0,1.02),(.06,.35,.075)),
                             ("Right", (.275,0,1.02),(.06,.35,.075))):
        belt = cube(f"GEO_Belt{name}", loc, dims, "leather", collection, "belt")
        parent_to_bone(belt, rig, "pelvis")
    buckle = torus("GEO_Buckle", (0,-.23,1.02), .055, .012, "brass", collection,
                   "belt", (math.radians(90),0,0), (1.2,1,.85))
    parent_to_bone(buckle, rig, "pelvis")
    satchel = cube("GEO_Satchel", (.38,-.18,.84), (.30,.13,.34),
                   "leather_light", collection, "satchel", bevel_width=.04)
    parent_to_bone(satchel, rig, "pelvis")
    flap = cube("GEO_SatchelFlap", (.38,-.255,.91), (.28,.035,.17),
                "padding", collection, "satchel", bevel_width=.02)
    parent_to_bone(flap, rig, "pelvis")
    strap = panel("GEO_SatchelStrap", [(-.315,-.27,1.59),(-.245,-.27,1.61),
                  (.37,-.27,.93),(.30,-.27,.88)], "leather_light",
                  collection, "satchel", .022)
    parent_to_bone(strap, rig, "chest")

    columns, rows = 6, 7
    vertices = []
    for row in range(rows):
        v = row / (rows - 1)
        half = .37 + v*.28
        z = 1.64 - v*1.10
        for column in range(columns):
            u = column / (columns - 1)
            vertices.append(((u*2-1)*half, .205 + v*.17 + math.sin(u*math.pi)*v*.05, z))
    faces = []
    for row in range(rows-1):
        for column in range(columns-1):
            a = row*columns+column
            faces.append((a,a+1,a+1+columns,a+columns))
    mesh = bpy.data.meshes.new("MESH_Cape")
    mesh.from_pydata(vertices, [], faces)
    cape = bpy.data.objects.new("GEO_Cape", mesh)
    collection.objects.link(cape)
    cape.data.materials.append(MATERIALS["cape"])
    cape["cc_component"] = "cape_cloth"
    solidify = cape.modifiers.new("CC_CapeThickness", "SOLIDIFY")
    solidify.thickness = .022
    cape_weights: list[dict[str, float]] = []
    for row in range(rows):
        chain = row / (rows - 1) * 3.0
        first = min(3, int(math.floor(chain)))
        second = min(3, first + 1)
        blend = chain - first
        for _column in range(columns):
            if first == second:
                cape_weights.append({f"cape.{first}": 1.0})
            else:
                cape_weights.append({f"cape.{first}": 1.0 - blend,
                                     f"cape.{second}": blend})
    smooth_skin(cape, rig, cape_weights)
    cape_objects.append(cape)
    collar = torus("GEO_CapeCollar", (0,.015,1.635), .255,.032,
                   "cape_light", collection,"cape", scale=(1.28,.86,1))
    parent_to_bone(collar, rig, "chest")
    cape_objects.append(collar)
    for suffix, sign in (("L",-1.0),("R",1.0)):
        mantle = panel(f"GEO_CapeMantle{suffix}",
                       [(sign*.03,-.175,1.645),(sign*.34,-.145,1.61),
                        (sign*.39,.04,1.545),(sign*.12,.08,1.57)],
                       "cape_light", collection,"cape",.027)
        parent_to_bone(mantle,rig,"chest")
        cape_objects.append(mantle)
        pin = torus(f"GEO_CapePin{suffix}",(sign*.29,-.18,1.59),.05,.015,
                    "brass",collection,"cape",(math.radians(90),0,0))
        parent_to_bone(pin,rig,"chest")
        cape_objects.append(pin)
    return cape_objects


def presentation(root: bpy.types.Collection) -> None:
    stage = cube("STAGE_Ground", (0,0,-.08), (6.0,5.0,.14),
                 "stage", root, "presentation", bevel_width=.08)
    stage["cc_presentation_only"] = True
    camera_data = bpy.data.cameras.new("CAM_HeroActions")
    camera = bpy.data.objects.new("CAM_HeroActions", camera_data)
    root.objects.link(camera)
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 2.90
    camera.location = (3.7,-6.2,3.05)
    camera.rotation_euler = (Vector((0,0,1.08))-camera.location).to_track_quat("-Z","Y").to_euler()
    bpy.context.scene.camera = camera
    key_data = bpy.data.lights.new("KEY_Action", "AREA")
    key_data.energy, key_data.size = 950, 4.0
    key = bpy.data.objects.new("KEY_Action", key_data)
    key.location = (-3.8,-4.5,6.0)
    key.rotation_euler = (Vector((0,0,1.1))-key.location).to_track_quat("-Z","Y").to_euler()
    root.objects.link(key)
    fill_data = bpy.data.lights.new("FILL_Action", "AREA")
    fill_data.energy, fill_data.size = 650, 3.5
    fill = bpy.data.objects.new("FILL_Action", fill_data)
    fill.location = (4.0,-2.8,4.5)
    fill.rotation_euler = (Vector((0,0,1.1))-fill.location).to_track_quat("-Z","Y").to_euler()
    root.objects.link(fill)
    rim_data = bpy.data.lights.new("RIM_Action", "AREA")
    rim_data.energy, rim_data.size = 750, 3.0
    rim = bpy.data.objects.new("RIM_Action", rim_data)
    rim.location = (0,3.5,4.8)
    rim.rotation_euler = (Vector((0,0,1.1))-rim.location).to_track_quat("-Z","Y").to_euler()
    root.objects.link(rim)


def key_scale(objects: list[bpy.types.Object], start: int, end: int) -> None:
    for obj in objects:
        base = tuple(obj.scale)
        keys = [(1,(0,0,0)), (max(1,start-1),(0,0,0)), (start,base), (end,base)]
        if end < FRAME_END:
            keys.append((end+1,(0,0,0)))
        for frame, scale in keys:
            obj.scale = scale
            obj.keyframe_insert("scale",frame=frame)


def action_props(root: bpy.types.Collection, rig: bpy.types.Object,
                 cape_objects: list[bpy.types.Object]) -> None:
    climb = [cube("ACTION_ClimbWall",(0,.50,1.30),(1.35,.12,2.60),
                  "steel_dark",root,"action_prop",bevel_width=.025)]
    for index,(x,z) in enumerate(((-.32,.42),(.28,.64),(-.20,.90),(.34,1.14),
                                  (-.30,1.40),(.22,1.66),(-.15,1.92),(.32,2.18))):
        bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=.075,depth=.07,
                location=(x,.405,z),rotation=(math.radians(90),0,0))
        hold=bpy.context.object; hold.name=f"ACTION_ClimbHold{index:02d}"
        climb.append(finish(hold,root,"brass" if index%2 else "cape_light","action_prop"))
    key_scale(climb,97,144)
    water = [cube("ACTION_Water",(0,0,.82),(4.5,4.5,.06),"water",root,"action_prop",bevel_width=.005)]
    for index,(y,x) in enumerate(((-.65,-.5),(-.10,.35),(.48,-.20))):
        water.append(cube(f"ACTION_Wave{index}",(x,y,.94),(1.25,.035,.035),
                          "cyan",root,"action_prop",(0,0,math.radians(8-index*8)),.005))
    key_scale(water,145,192)
    weapons=[]
    blade=cube("ACTION_SwordBlade",(.59,-.055,1.15),(.055,.028,.62),"steel_light",root,"weapon",bevel_width=.006)
    guard=cube("ACTION_SwordGuard",(.59,-.055,.86),(.22,.045,.045),"brass",root,"weapon",bevel_width=.008)
    grip=cylinder_between("ACTION_SwordGrip",(.59,-.055,.68),(.59,-.055,.87),.028,.028,"leather",root,"weapon",8)
    for obj in (blade,guard,grip):
        parent_to_bone(obj,rig,"hand.R"); weapons.append(obj)
    bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.19,depth=.055,
            location=(-.62,-.11,.98),rotation=(math.radians(90),0,0))
    shield=bpy.context.object; shield.name="ACTION_Shield"
    shield=finish(shield,root,"steel","weapon")
    bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=.065,depth=.075,
            location=(-.62,-.145,.98),rotation=(math.radians(90),0,0))
    boss=bpy.context.object; boss.name="ACTION_ShieldBoss"
    boss=finish(boss,root,"brass","weapon")
    for obj in (shield,boss):
        parent_to_bone(obj,rig,"hand.L"); weapons.append(obj)
    key_scale(weapons,193,240)
    for obj in cape_objects:
        base=tuple(obj.scale)
        for frame,scale in ((1,base),(96,base),(97,(0,0,0)),(192,(0,0,0)),(193,base)):
            obj.scale=scale; obj.keyframe_insert("scale",frame=frame)


def key_pose(rig: bpy.types.Object, frame: int,
             rotations: dict[str,tuple[float,float,float]]|None=None,
             pelvis_z: float=0.0,
             location: tuple[float,float,float]=(0,0,0),
             rotation: tuple[float,float,float]=(0,0,0)) -> None:
    rotations=rotations or {}
    for name in CONTROL_BONES:
        bone=rig.pose.bones[name]
        bone.rotation_mode="XYZ"
        bone.rotation_euler=rotations.get(name,(0,0,0))
        bone.location=(0,0,pelvis_z if name=="pelvis" else 0)
        bone.keyframe_insert("rotation_euler",frame=frame)
        bone.keyframe_insert("location",frame=frame)
    rig.rotation_mode="XYZ"; rig.location=location; rig.rotation_euler=rotation
    rig.keyframe_insert("location",frame=frame); rig.keyframe_insert("rotation_euler",frame=frame)


def animate(rig: bpy.types.Object) -> None:
    left={"upper_arm.L":(-.42,0,0),"forearm.L":(-.18,0,0),"upper_arm.R":(.42,0,0),"forearm.R":(-.35,0,0),
          "thigh.L":(.48,0,0),"shin.L":(.10,0,0),"thigh.R":(-.48,0,0),"shin.R":(.60,0,0),
          "chest":(0,0,.055),"head":(0,0,-.035)}
    right={"upper_arm.L":(.42,0,0),"forearm.L":(-.35,0,0),"upper_arm.R":(-.42,0,0),"forearm.R":(-.18,0,0),
           "thigh.L":(-.48,0,0),"shin.L":(.60,0,0),"thigh.R":(.48,0,0),"shin.R":(.10,0,0),
           "chest":(0,0,-.055),"head":(0,0,.035)}
    key_pose(rig,1,left); key_pose(rig,13,{"shin.L":(.72,0,0)},.03); key_pose(rig,25,right)
    key_pose(rig,37,{"shin.R":(.72,0,0)},.03); key_pose(rig,48,left)
    crouch={"upper_arm.L":(.38,0,0),"upper_arm.R":(.38,0,0),"thigh.L":(-.62,0,0),"thigh.R":(-.62,0,0),
            "shin.L":(1.05,0,0),"shin.R":(1.05,0,0),"chest":(.16,0,0)}
    launch={"upper_arm.L":(0,0,2.50),"upper_arm.R":(0,0,-2.50),"forearm.L":(0,0,-.20),"forearm.R":(0,0,.20)}
    apex={"upper_arm.L":(0,0,2.25),"upper_arm.R":(0,0,-2.25),"forearm.L":(0,0,-.30),"forearm.R":(0,0,.30),"thigh.L":(.48,0,0),"thigh.R":(.48,0,0),
          "shin.L":(-.72,0,0),"shin.R":(-.72,0,0)}
    key_pose(rig,49,crouch,-.105); key_pose(rig,61,launch,.20); key_pose(rig,73,apex,.46)
    key_pose(rig,85,crouch,-.055); key_pose(rig,96)
    climb_l={"upper_arm.L":(0,0,2.55),"forearm.L":(0,0,-.20),"upper_arm.R":(0,0,-1.20),"forearm.R":(0,0,.55),
             "thigh.L":(-.65,0,0),"shin.L":(1.05,0,0),"chest":(.12,0,-.08)}
    climb_r={"upper_arm.L":(0,0,1.20),"forearm.L":(0,0,-.55),"upper_arm.R":(0,0,-2.55),"forearm.R":(0,0,.20),
             "thigh.R":(-.65,0,0),"shin.R":(1.05,0,0),"chest":(.12,0,.08)}
    for frame,pose,z in ((97,climb_l,0),(109,climb_r,.16),(121,climb_l,.31),(133,climb_r,.46),(144,climb_l,.58)):
        key_pose(rig,frame,pose,location=(0,.20,z),rotation=(0,0,math.pi))
    horizontal=(math.radians(90),0,0)
    extend={"upper_arm.L":(0,0,1.10),"upper_arm.R":(0,0,-1.10),"forearm.L":(0,0,-.20),"forearm.R":(0,0,.20),
            "thigh.L":(.20,0,0),"thigh.R":(-.20,0,0)}
    stroke_l={"upper_arm.L":(-1.10,0,.55),"forearm.L":(-.75,0,0),"upper_arm.R":(.70,0,-.45),"forearm.R":(-.30,0,0),
              "thigh.L":(-.35,0,0),"shin.L":(.35,0,0),"thigh.R":(.35,0,0),"shin.R":(-.35,0,0)}
    stroke_r={"upper_arm.L":(.70,0,.45),"forearm.L":(-.30,0,0),"upper_arm.R":(-1.10,0,-.55),"forearm.R":(-.75,0,0),
              "thigh.L":(.35,0,0),"shin.L":(-.35,0,0),"thigh.R":(-.35,0,0),"shin.R":(.35,0,0)}
    key_pose(rig,145,extend,location=(0,.10,1.12),rotation=horizontal)
    key_pose(rig,157,stroke_l,location=(-.08,.02,1.13),rotation=horizontal)
    key_pose(rig,169,extend,location=(0,-.05,1.12),rotation=horizontal)
    key_pose(rig,181,stroke_r,location=(.08,.02,1.13),rotation=horizontal)
    key_pose(rig,192,extend,location=(0,.10,1.12),rotation=horizontal)
    guard_pose={"upper_arm.L":(-.55,0,.45),"forearm.L":(-.85,0,-.15),"upper_arm.R":(-.55,0,-.35),"forearm.R":(-.95,0,0),
                "thigh.L":(.22,0,0),"thigh.R":(-.22,0,0),"chest":(0,0,-.14),"head":(0,0,.10)}
    high={"upper_arm.L":(-.35,0,.75),"forearm.L":(-.70,0,-.10),"upper_arm.R":(-1.05,0,.75),"forearm.R":(-.35,0,.35),
          "chest":(0,0,.30),"head":(0,0,-.18),"thigh.L":(.38,0,0),"shin.L":(.35,0,0)}
    cross={"upper_arm.L":(-.25,0,.60),"forearm.L":(-.60,0,0),"upper_arm.R":(.35,0,-1.05),"forearm.R":(-.15,0,-.25),
           "chest":(0,0,-.38),"head":(0,0,.22),"thigh.R":(.34,0,0),"shin.R":(.30,0,0)}
    key_pose(rig,193,guard_pose); key_pose(rig,205,high,location=(-.08,-.05,.04))
    key_pose(rig,217,cross,location=(.10,-.10,.02)); key_pose(rig,229,high,location=(-.04,-.04,.03))
    key_pose(rig,240,guard_pose)


def render(preview: bool) -> None:
    scene=bpy.context.scene
    if preview:
        PREVIEW_DIR.mkdir(parents=True,exist_ok=True)
        for frame,label in ((1,"walk"),(73,"jump"),(121,"climb"),(169,"swim"),(217,"fight")):
            scene.frame_set(frame); scene.render.filepath=str(PREVIEW_DIR/f"{label}.png")
            bpy.ops.render.render(write_still=True)
        print(f"Rendered action previews to {PREVIEW_DIR}")
    else:
        FRAMES_DIR.mkdir(parents=True,exist_ok=True)
        scene.render.filepath=str(FRAMES_DIR/"action_")
        bpy.ops.render.render(animation=True)
        print(f"Rendered {FRAME_END} action frames to {FRAMES_DIR}")


def main() -> None:
    root=reset_scene(); make_palette(); rig=armature(root)
    cape_objects=build_hero(root,rig); presentation(root); action_props(root,rig,cape_objects); animate(rig)
    BLEND_PATH.parent.mkdir(parents=True,exist_ok=True)
    bpy.context.scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH),compress=True)
    if "--save-only" not in sys.argv:
        render("--preview" in sys.argv)


if __name__=="__main__":
    main()
