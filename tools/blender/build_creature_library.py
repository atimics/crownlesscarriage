#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import asdict, dataclass, replace
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import paint_channels


ROOT = Path(__file__).resolve().parents[2]
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_creature_library.blend"
EXPORT_DIR = ROOT / "assets" / "exports" / "creatures"
PREVIEW_PATH = ROOT / "assets" / "previews" / "creatures" / "creature_family_sheet.png"
MANIFEST_PATH = ROOT / "assets" / "creature_manifest.json"
LIBRARY_VERSION = "0.9.0"

DRAGON_POSES = ("idle", "stalk_a", "stalk_b", "threat", "rest")

QUADRUPED_BONES = (
    ("root", None),
    ("body", "root"),
    ("chest", "body"),
    ("neck", "chest"),
    ("head", "neck"),
    ("upper_leg.FL", "chest"),
    ("lower_leg.FL", "upper_leg.FL"),
    ("hoof.FL", "lower_leg.FL"),
    ("upper_leg.FR", "chest"),
    ("lower_leg.FR", "upper_leg.FR"),
    ("hoof.FR", "lower_leg.FR"),
    ("upper_leg.HL", "body"),
    ("lower_leg.HL", "upper_leg.HL"),
    ("hoof.HL", "lower_leg.HL"),
    ("upper_leg.HR", "body"),
    ("lower_leg.HR", "upper_leg.HR"),
    ("hoof.HR", "lower_leg.HR"),
    ("tail.root", "body"),
    ("tail", "tail.root"),
)

MATERIAL_ORDER = (
    "skin",
    "secondary",
    "hide",
    "cloth",
    "leather",
    "horn",
    "metal",
    "accent",
    "eye",
)

FAMILY_PALETTES = {
    "goblin": {
        "skin": (0.31, 0.46, 0.20, 1.0),
        "secondary": (0.16, 0.25, 0.12, 1.0),
        "hide": (0.22, 0.16, 0.10, 1.0),
        "cloth": (0.25, 0.18, 0.13, 1.0),
        "leather": (0.16, 0.085, 0.045, 1.0),
        "horn": (0.64, 0.55, 0.34, 1.0),
        "metal": (0.29, 0.34, 0.34, 1.0),
        "accent": (0.58, 0.18, 0.20, 1.0),
        "eye": (0.88, 0.66, 0.13, 1.0),
    },
    "horse": {
        "skin": (0.965, 0.592, 0.808, 1.0),
        "secondary": (0.729, 0.188, 0.545, 1.0),
        "hide": (0.976, 0.710, 0.863, 1.0),
        "cloth": (0.443, 0.906, 0.886, 1.0),
        "leather": (0.227, 0.145, 0.318, 1.0),
        "horn": (1.0, 0.984, 0.957, 1.0),
        "metal": (0.318, 0.545, 0.847, 1.0),
        "accent": (0.443, 0.906, 0.886, 1.0),
        "eye": (0.090, 0.067, 0.176, 1.0),
    },
    "cow": {
        "skin": (0.68, 0.61, 0.47, 1.0),
        "secondary": (0.20, 0.13, 0.085, 1.0),
        "hide": (0.76, 0.69, 0.55, 1.0),
        "cloth": (0.14, 0.31, 0.29, 1.0),
        "leather": (0.17, 0.085, 0.042, 1.0),
        "horn": (0.67, 0.59, 0.42, 1.0),
        "metal": (0.31, 0.36, 0.36, 1.0),
        "accent": (0.55, 0.18, 0.18, 1.0),
        "eye": (0.018, 0.016, 0.013, 1.0),
    },
    "sheep": {
        "skin": (0.15, 0.13, 0.11, 1.0),
        "secondary": (0.075, 0.065, 0.060, 1.0),
        "hide": (0.72, 0.68, 0.57, 1.0),
        "cloth": (0.89, 0.86, 0.72, 1.0),
        "leather": (0.10, 0.080, 0.065, 1.0),
        "horn": (0.62, 0.54, 0.38, 1.0),
        "metal": (0.31, 0.36, 0.36, 1.0),
        "accent": (0.56, 0.43, 0.32, 1.0),
        "eye": (0.012, 0.010, 0.009, 1.0),
    },
    "dragon": {
        "skin": (0.075, 0.16, 0.16, 1.0),
        "secondary": (0.30, 0.075, 0.070, 1.0),
        "hide": (0.12, 0.22, 0.20, 1.0),
        "cloth": (0.24, 0.08, 0.08, 1.0),
        "leather": (0.12, 0.065, 0.035, 1.0),
        "horn": (0.65, 0.52, 0.28, 1.0),
        "metal": (0.26, 0.31, 0.31, 1.0),
        "accent": (0.77, 0.35, 0.12, 1.0),
        "eye": (0.96, 0.70, 0.10, 1.0),
    },
}

PREVIEW_MATERIALS: dict[tuple[str, str], bpy.types.Material] = {}
INDEXED_MATERIAL: bpy.types.Material | None = None


@dataclass(frozen=True)
class CreatureSpec:
    family: str
    variant: str
    variant_index: int
    silhouette: str
    runtime_morphology: str
    gait_contract: str
    equipment: tuple[str, ...] = ()
    pose: str = "idle"

    @property
    def asset_id(self) -> str:
        suffix = "" if self.pose == "idle" else f"_{self.pose}"
        return f"creature_{self.variant}{suffix}_v01"


CREATURES = (
    CreatureSpec("goblin", "goblin_scavenger", 0,
                 "low narrow body, oversized ears and hands",
                 "biped", "humanoid_runtime_skin", ("pack", "hook")),
    CreatureSpec("goblin", "goblin_raider", 1,
                 "forward armored wedge with a high spear",
                 "biped", "humanoid_runtime_skin", ("helmet", "armor", "spear")),
    CreatureSpec("goblin", "goblin_tribute_bearer", 2,
                 "broad burdened carrier framing a bright offering",
                 "biped", "humanoid_runtime_skin", ("offering", "harness")),
    CreatureSpec("horse", "horse", 3,
                 "colorful storybook pony with big eyes and a ribbon mane",
                 "quadruped", "quadruped_runtime_skin",
                 ("thick_mane", "bushy_tail", "feathered_hooves")),
    CreatureSpec("cow", "cow", 4,
                 "deep barrel, low head, horned horizontal line",
                 "quadruped", "quadruped_runtime_skin", ("horns", "udder")),
    CreatureSpec("dragon", "dragon", 5,
                 "long grounded predator with crown horns and folded wings",
                 "quadruped", "dragon_authored",
                 ("wings", "neck", "tail", "jaw", "spines")),
    CreatureSpec("dragon", "dragon_whelp", 6,
                 "round big-eyed hatchling with ear fins and tiny wing buds",
                 "quadruped", "dragon_authored",
                 ("ear_fins", "wing_buds", "snub_jaw", "crest")),
    CreatureSpec("dragon", "dragon_wanderer", 7,
                 "giant long-bodied glider with a low chest and swept wings",
                 "quadruped", "dragon_authored",
                 ("glider_wings", "swept_horns", "long_tail", "back_blades")),
    CreatureSpec("dragon", "dragon_deep_wyrm", 8,
                 "ancient legless serpent with a narrow skull and broken sails",
                 "quadruped", "dragon_authored",
                 ("serpent_body", "broken_sails", "vestigial_claws", "horn_crown")),
    CreatureSpec("sheep", "sheep", 9,
                 "small round fleece with a dark narrow face and short legs",
                 "quadruped", "quadruped_runtime_skin",
                 ("fleece", "ears", "short_tail")),
)


def poses_for(spec: CreatureSpec) -> tuple[str, ...]:
    if spec.family == "dragon":
        return DRAGON_POSES
    return ("idle",)


def skeleton_for(spec: CreatureSpec) -> str:
    """The skeleton family of docs/design/unified-characters.md."""
    if spec.family == "goblin":
        return "humanoid"
    if spec.family in ("horse", "cow", "sheep"):
        return "quadruped"
    return "none"


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "CC_CREATURE_LIBRARY"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 2400
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene["cc_library_version"] = LIBRARY_VERSION
    scene["cc_art_direction"] = "silhouette_first_pseudo_pixel_creatures"
    scene["cc_forward_axis"] = "-Y"
    scene["cc_up_axis"] = "+Z"
    world = bpy.data.worlds.new("CC_CreatureWorld")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.012, 0.017, 0.018, 1.0)
    background.inputs["Strength"].default_value = 0.30
    scene.world = world


def make_materials() -> None:
    global INDEXED_MATERIAL
    for family, palette in FAMILY_PALETTES.items():
        for semantic in MATERIAL_ORDER:
            material = bpy.data.materials.new(
                f"MAT_CREATURE_PREVIEW_{family.upper()}_{semantic.upper()}")
            material.diffuse_color = palette[semantic]
            material.use_nodes = True
            material.use_backface_culling = False
            principled = material.node_tree.nodes.get("Principled BSDF")
            principled.inputs["Base Color"].default_value = palette[semantic]
            principled.inputs["Roughness"].default_value = 0.80
            if semantic == "metal":
                principled.inputs["Metallic"].default_value = 0.45
                principled.inputs["Roughness"].default_value = 0.48
            PREVIEW_MATERIALS[(family, semantic)] = material
    INDEXED_MATERIAL = bpy.data.materials.new("MAT_CREATURE_INDEXED")
    INDEXED_MATERIAL.diffuse_color = (1.0, 1.0, 1.0, 1.0)
    INDEXED_MATERIAL.use_nodes = True
    INDEXED_MATERIAL.use_backface_culling = False
    nodes = INDEXED_MATERIAL.node_tree.nodes
    principled = nodes.get("Principled BSDF")
    vertex_color = nodes.new("ShaderNodeVertexColor")
    vertex_color.layer_name = "COLOR_0"
    INDEXED_MATERIAL.node_tree.links.new(
        vertex_color.outputs["Color"], principled.inputs["Base Color"])
    principled.inputs["Roughness"].default_value = 0.80


def new_collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def move_to(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)


def tag(obj: bpy.types.Object, spec: CreatureSpec, part: str) -> None:
    obj["cc_asset_id"] = spec.asset_id
    obj["cc_family"] = spec.family
    obj["cc_variant"] = spec.variant
    obj["cc_pose"] = spec.pose
    obj["cc_part"] = part
    obj["cc_library_version"] = LIBRARY_VERSION


def assign(obj: bpy.types.Object, semantic: str, spec: CreatureSpec) -> None:
    obj.data.materials.append(PREVIEW_MATERIALS[(spec.family, semantic)])


def add_bevel(obj: bpy.types.Object, width: float, segments: int = 1) -> None:
    bevel = obj.modifiers.new("CC_SilhouetteBevel", "BEVEL")
    bevel.width = width
    bevel.segments = segments
    bevel.limit_method = "ANGLE"


def add_box(name: str, center: Vector | tuple[float, float, float],
            dimensions: tuple[float, float, float], semantic: str,
            collection: bpy.types.Collection, spec: CreatureSpec, part: str,
            *, rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
            bevel: float = 0.012) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=center, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0.0:
        add_bevel(obj, min(bevel, min(dimensions) * 0.20))
    assign(obj, semantic, spec)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_ellipsoid(name: str, center: Vector | tuple[float, float, float],
                  scale: tuple[float, float, float], semantic: str,
                  collection: bpy.types.Collection, spec: CreatureSpec,
                  part: str, *, subdivisions: int = 1) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(
        subdivisions=subdivisions, radius=1.0, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign(obj, semantic, spec)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_segment(name: str, start: Vector | tuple[float, float, float],
                end: Vector | tuple[float, float, float], start_radius: float,
                end_radius: float, semantic: str,
                collection: bpy.types.Collection, spec: CreatureSpec,
                part: str, *, sides: int = 8) -> bpy.types.Object:
    a = Vector(start)
    b = Vector(end)
    axis = b - a
    if axis.length < 0.0001:
        raise ValueError(f"{name} has no length")
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    tangent = axis.normalized()
    helper = Vector((0.0, 0.0, 1.0))
    if abs(tangent.dot(helper)) > 0.90:
        helper = Vector((1.0, 0.0, 0.0))
    right = tangent.cross(helper).normalized()
    up = right.cross(tangent).normalized()
    for point, radius in ((a, start_radius), (b, end_radius)):
        for side in range(sides):
            angle = math.tau * float(side) / float(sides)
            vertex = point + right * math.cos(angle) * radius
            vertex += up * math.sin(angle) * radius
            vertices.append(tuple(vertex))
    for side in range(sides):
        following = (side + 1) % sides
        faces.append((side, following, sides + following, sides + side))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple(range(sides, sides * 2)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def add_cone(name: str, center: Vector | tuple[float, float, float],
             radius: float, depth: float, semantic: str,
             collection: bpy.types.Collection, spec: CreatureSpec, part: str,
             *, rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
             vertices: int = 6) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices, radius1=radius, radius2=0.0, depth=depth,
        location=center, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name
    assign(obj, semantic, spec)
    move_to(obj, collection)
    tag(obj, spec, part)
    return obj


def add_prism(name: str, points: tuple[tuple[float, float, float], ...],
              thickness: float, semantic: str,
              collection: bpy.types.Collection, spec: CreatureSpec,
              part: str) -> bpy.types.Object:
    count = len(points)
    vertices = [(x, y, z - thickness * 0.5) for x, y, z in points]
    vertices += [(x, y, z + thickness * 0.5) for x, y, z in points]
    faces: list[tuple[int, ...]] = [
        tuple(reversed(range(count))),
        tuple(range(count, count * 2)),
    ]
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, following, count + following, count + index))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def add_lateral_prism(
    name: str,
    points: tuple[tuple[float, float, float], ...],
    thickness: float,
    semantic: str,
    collection: bpy.types.Collection,
    spec: CreatureSpec,
    part: str,
) -> bpy.types.Object:
    count = len(points)
    vertices = [(x - thickness * 0.5, y, z) for x, y, z in points]
    vertices += [(x + thickness * 0.5, y, z) for x, y, z in points]
    faces: list[tuple[int, ...]] = [
        tuple(reversed(range(count))),
        tuple(range(count, count * 2)),
    ]
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, following, count + following, count + index))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def pose_phase(pose: str) -> float:
    phases = {
        "idle": 0.0,
        "contact_a": 0.0,
        "down_a": 0.125,
        "passing_a": 0.25,
        "up_a": 0.375,
        "contact_b": 0.5,
        "down_b": 0.625,
        "passing_b": 0.75,
        "up_b": 0.875,
        "stalk_a": 0.12,
        "stalk_b": 0.62,
        "threat": 0.0,
        "rest": 0.0,
    }
    return phases[pose]


HUMANOID_BONES = (
    ("root", None),
    ("pelvis", "root"),
    ("spine", "pelvis"),
    ("chest", "spine"),
    ("neck", "chest"),
    ("head", "neck"),
    ("upper_arm.L", "chest"),
    ("forearm.L", "upper_arm.L"),
    ("hand.L", "forearm.L"),
    ("upper_arm.R", "chest"),
    ("forearm.R", "upper_arm.R"),
    ("hand.R", "forearm.R"),
    ("thigh.L", "pelvis"),
    ("shin.L", "thigh.L"),
    ("foot.L", "shin.L"),
    ("thigh.R", "pelvis"),
    ("shin.R", "thigh.R"),
    ("foot.R", "shin.R"),
)

# The idle output of CcHumanoidPoseFromBipedRig for the goblin body plans in
# src/locomotion/cc_character_skin.c, in glTF model space (+y up, +z
# forward). Regenerate with `character_skin_tests --print-bind`; the C test
# and the validator hold the two sides together.
GOBLIN_BIND = {
    "pelvis": (0.0000, 0.7800, 0.0000),
    "spine": (0.0000, 0.9600, 0.0000),
    "chest": (0.0000, 1.1400, 0.0000),
    "neck": (0.0000, 1.3200, 0.0000),
    "head": (0.0000, 1.4200, 0.0200),
    "hip.L": (-0.1400, 0.7800, 0.0200),
    "knee.L": (-0.1809, 0.4125, 0.2395),
    "ankle.L": (-0.1700, 0.0000, 0.0600),
    "toe.L": (-0.1700, 0.0000, 0.2600),
    "shoulder.L": (-0.2600, 1.1600, 0.0000),
    "elbow.L": (-0.3207, 0.9072, 0.0000),
    "hand.L": (-0.3207, 0.6604, 0.0398),
    "hip.R": (0.1400, 0.7800, 0.0200),
    "knee.R": (0.1809, 0.4125, 0.2395),
    "ankle.R": (0.1700, 0.0000, 0.0600),
    "toe.R": (0.1700, 0.0000, 0.2600),
    "shoulder.R": (0.2600, 1.1600, 0.0000),
    "elbow.R": (0.3207, 0.9072, 0.0000),
    "hand.R": (0.3207, 0.6604, 0.0398),
}
# The hand-drawn goblins read bright through the world shader. The skinned
# character shader is darker, so the goblin paint sits one value step up.
GOBLIN_VALUE_LIFT = 0.25
GOBLIN_CARRIER_ARMS = {
    "elbow.L": (-0.3400, 0.8300, 0.0800),
    "hand.L": (-0.2700, 0.8600, 0.3900),
    "elbow.R": (0.3400, 0.8300, 0.0800),
    "hand.R": (0.2700, 0.8600, 0.3900),
}


def gl(x: float, y: float, z: float) -> Vector:
    """A glTF model-space point (+y up, +z forward) in Blender axes."""
    return Vector((x, -z, y))


def goblin_joints(spec: CreatureSpec) -> dict[str, Vector]:
    joints = dict(GOBLIN_BIND)
    if spec.variant == "goblin_tribute_bearer":
        joints.update(GOBLIN_CARRIER_ARMS)
    return {name: gl(*point) for name, point in joints.items()}


def humanoid_bone_points(
    joints: dict[str, Vector],
) -> dict[str, tuple[Vector, Vector]]:
    def extend(start: Vector, end: Vector, length: float) -> Vector:
        return start + (start - end).normalized() * length

    points: dict[str, tuple[Vector, Vector]] = {
        "root": (Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, 0.18))),
        "pelvis": (joints["pelvis"], joints["spine"]),
        "spine": (joints["spine"], joints["chest"]),
        "chest": (joints["chest"], joints["neck"]),
        "neck": (joints["neck"], joints["head"]),
        "head": (joints["head"],
                 extend(joints["head"], joints["neck"], 0.18)),
    }
    for side in ("L", "R"):
        shoulder = joints[f"shoulder.{side}"]
        elbow = joints[f"elbow.{side}"]
        hand = joints[f"hand.{side}"]
        points[f"upper_arm.{side}"] = (shoulder, elbow)
        points[f"forearm.{side}"] = (elbow, hand)
        points[f"hand.{side}"] = (hand, extend(hand, elbow, 0.16))
        points[f"thigh.{side}"] = (joints[f"hip.{side}"],
                                   joints[f"knee.{side}"])
        points[f"shin.{side}"] = (joints[f"knee.{side}"],
                                  joints[f"ankle.{side}"])
        points[f"foot.{side}"] = (joints[f"ankle.{side}"],
                                  joints[f"toe.{side}"])
    return points


def bind_to(obj: bpy.types.Object, bone: str) -> bpy.types.Object:
    obj["cc_deform_bone"] = bone
    return obj


def add_tube(name: str, points: tuple[Vector, ...],
             radii: tuple[float, ...], semantic: str,
             collection: bpy.types.Collection, spec: CreatureSpec,
             part: str, *, sides: int = 6) -> bpy.types.Object:
    """One closed tube through shared rings, like DrawCreatureTube."""
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    helper = Vector((0.0, 0.0, 1.0))
    for index, (point, radius) in enumerate(zip(points, radii)):
        following = points[min(index + 1, len(points) - 1)]
        previous = points[max(index - 1, 0)]
        tangent = (following - previous).normalized()
        axis = helper if abs(tangent.dot(helper)) < 0.90 else \
            Vector((1.0, 0.0, 0.0))
        right = tangent.cross(axis).normalized()
        up = right.cross(tangent).normalized()
        for side in range(sides):
            angle = math.tau * float(side) / float(sides)
            vertex = point + right * math.cos(angle) * max(radius, 0.002)
            vertex += up * math.sin(angle) * max(radius, 0.002)
            vertices.append(tuple(vertex))
    for ring in range(len(points) - 1):
        for side in range(sides):
            following = (side + 1) % sides
            a = ring * sides
            b = (ring + 1) * sides
            faces.append((a + side, a + following, b + following, b + side))
    faces.append(tuple(reversed(range(sides))))
    last = (len(points) - 1) * sides
    faces.append(tuple(range(last, last + sides)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def add_panel(name: str, points: tuple[Vector, ...], thickness: float,
              semantic: str, collection: bpy.types.Collection,
              spec: CreatureSpec, part: str) -> bpy.types.Object:
    """A thin flat plate through three or more points (ears, pennant)."""
    normal = (points[1] - points[0]).cross(points[2] - points[0])
    normal = normal.normalized() * thickness * 0.5
    count = len(points)
    vertices = [tuple(point - normal) for point in points]
    vertices += [tuple(point + normal) for point in points]
    faces: list[tuple[int, ...]] = [
        tuple(reversed(range(count))),
        tuple(range(count, count * 2)),
    ]
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, following, count + following, count + index))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, semantic, spec)
    tag(obj, spec, part)
    return obj


def build_goblin(spec: CreatureSpec,
                 collection: bpy.types.Collection) -> None:
    """One skinned goblin in its bind pose.

    The shapes follow the hand-drawn C rig this mesh replaces (DrawGoblinRig
    before the unified-characters migration): same offsets from the pelvis,
    chest and head, same radii, same palette slots. Hair and the neck scarf
    use the accent slot, which the runtime fills with the dragon court color.
    """
    joints = goblin_joints(spec)
    pelvis = joints["pelvis"]
    chest = joints["chest"]
    neck_ring = pelvis + gl(0.0, 0.58, 0.0)
    head = pelvis + gl(0.0, 0.76, 0.025)

    def at(base: Vector, x: float, y: float, z: float) -> Vector:
        return base + gl(x, y, z)

    # Pelvis, torso, neck.
    bind_to(add_box("GOBLIN_Belt", at(pelvis, 0.0, 0.10, 0.0),
                    (0.38, 0.28, 0.22), "leather", collection, spec, "belt"),
            "pelvis")
    bind_to(add_box("GOBLIN_Buckle", at(pelvis, 0.0, 0.11, 0.158),
                    (0.09, 0.024, 0.085), "horn", collection, spec,
                    "buckle", bevel=0.004), "pelvis")
    bind_to(add_segment("GOBLIN_Torso", pelvis + gl(0.0, 0.06, 0.0), chest,
                        0.21, 0.26, "cloth", collection, spec, "torso",
                        sides=8), "spine")
    bind_to(add_segment("GOBLIN_Shoulders", chest, neck_ring, 0.21, 0.10,
                        "cloth", collection, spec, "shoulders", sides=8),
            "chest")
    bind_to(add_ellipsoid("GOBLIN_Scarf", neck_ring, (0.145, 0.13, 0.066),
                          "accent", collection, spec, "scarf"), "chest")

    # Head and face.
    face_parts: list[bpy.types.Object] = []
    face_parts.append(add_ellipsoid("GOBLIN_Head", head, (0.25, 0.22, 0.24),
                                    "skin", collection, spec, "head",
                                    subdivisions=2))
    face_parts.append(add_ellipsoid("GOBLIN_Jaw", at(head, 0.0, -0.105, 0.12),
                                    (0.19, 0.15, 0.115), "skin", collection,
                                    spec, "jaw"))
    face_parts.append(add_ellipsoid("GOBLIN_Nose", at(head, 0.0, -0.015, 0.235),
                                    (0.059, 0.08, 0.10), "skin", collection,
                                    spec, "nose"))
    face_parts.append(add_ellipsoid("GOBLIN_NoseTip", at(head, 0.0, -0.07, 0.30),
                                    (0.073, 0.076, 0.055), "skin",
                                    collection, spec, "nose_tip"))
    for side, label in ((-1.0, "L"), (1.0, "R")):
        ear_root = at(head, side * 0.18, 0.075, 0.0)
        ear_tip = at(head, side * 0.48, 0.12 if side < 0 else 0.18, -0.055)
        ear_bottom = at(head, side * 0.19, -0.09, -0.03)
        face_parts.append(add_panel(
            f"GOBLIN_Ear_{label}", (ear_root, ear_tip, ear_bottom), 0.018,
            "skin", collection, spec, f"ear_{label.lower()}"))
        face_parts.append(add_segment(
            f"GOBLIN_EarRim_{label}", ear_root, ear_tip, 0.036, 0.009,
            "skin", collection, spec, f"ear_rim_{label.lower()}", sides=5))
        ear_inner = at(head, side * 0.25, 0.025, 0.008)
        face_parts.append(add_panel(
            f"GOBLIN_EarInner_{label}",
            (ear_inner, ear_inner.lerp(ear_tip, 0.79),
             ear_inner.lerp(ear_bottom, 0.65)),
            0.022, "secondary", collection, spec,
            f"ear_inner_{label.lower()}"))
        eye = at(head, side * 0.104, 0.04, 0.206)
        face_parts.append(add_ellipsoid(
            f"GOBLIN_EyeSocket_{label}", eye, (0.061, 0.031, 0.048),
            "secondary", collection, spec, f"eye_socket_{label.lower()}"))
        iris = eye + gl(0.0, 0.0, 0.018)
        face_parts.append(add_ellipsoid(
            f"GOBLIN_Iris_{label}", iris, (0.048, 0.020, 0.035), "eye",
            collection, spec, f"iris_{label.lower()}"))
        pupil = iris + gl(0.0, 0.0, 0.018)
        pupil_obj = add_ellipsoid(
            f"GOBLIN_Pupil_{label}", pupil, (0.016, 0.008, 0.026),
            "leather", collection, spec, f"pupil_{label.lower()}")
        pupil_obj["cc_paint_value"] = 0.25
        face_parts.append(pupil_obj)
        face_parts.append(add_segment(
            f"GOBLIN_Brow_{label}", at(head, side * 0.05, 0.085, 0.226),
            at(head, side * 0.19, 0.12, 0.17), 0.029, 0.021, "secondary",
            collection, spec, f"brow_{label.lower()}", sides=5))
        mouth = add_segment(
            f"GOBLIN_Mouth_{label}", at(head, side * 0.14, -0.14, 0.218),
            at(head, 0.0, -0.158, 0.265), 0.014, 0.012, "leather",
            collection, spec, f"mouth_{label.lower()}", sides=5)
        mouth["cc_paint_value"] = 0.25
        face_parts.append(mouth)
        tusk = at(head, side * 0.098, -0.16, 0.258)
        face_parts.append(add_segment(
            f"GOBLIN_Tusk_{label}", tusk, tusk + gl(side * 0.013, 0.068, 0.016),
            0.021, 0.003, "horn", collection, spec,
            f"tusk_{label.lower()}", sides=5))
    add_goblin_hair(spec, collection, head, face_parts)
    for obj in face_parts:
        bind_to(obj, "head")

    # Arms.
    for side, label in ((-1.0, "L"), (1.0, "R")):
        shoulder = joints[f"shoulder.{label}"]
        elbow = joints[f"elbow.{label}"]
        hand = joints[f"hand.{label}"]
        lower = label.lower()
        bind_to(add_ellipsoid(f"GOBLIN_Shoulder_{label}", shoulder,
                              (0.085, 0.085, 0.085), "cloth", collection,
                              spec, f"shoulder_{lower}"),
                f"upper_arm.{label}")
        bind_to(add_segment(f"GOBLIN_UpperArm_{label}", shoulder, elbow,
                            0.085, 0.068, "cloth", collection, spec,
                            f"upper_arm_{lower}", sides=7),
                f"upper_arm.{label}")
        bind_to(add_ellipsoid(f"GOBLIN_Elbow_{label}", elbow,
                              (0.066, 0.066, 0.066), "skin", collection,
                              spec, f"elbow_{lower}"), f"forearm.{label}")
        bind_to(add_segment(f"GOBLIN_Forearm_{label}", elbow, hand,
                            0.068, 0.055, "skin", collection, spec,
                            f"forearm_{lower}", sides=7),
                f"forearm.{label}")
        bind_to(add_segment(f"GOBLIN_Cuff_{label}", elbow.lerp(hand, 0.78),
                            hand, 0.071, 0.065, "leather", collection, spec,
                            f"cuff_{lower}", sides=6), f"forearm.{label}")
        bind_to(add_ellipsoid(f"GOBLIN_Hand_{label}", hand,
                              (0.078, 0.078, 0.078), "skin", collection,
                              spec, f"hand_{lower}"), f"hand.{label}")

    # Legs, from DrawCreatureMuscleLimbs' goblin branch.
    for label in ("L", "R"):
        hip = joints[f"hip.{label}"]
        knee = joints[f"knee.{label}"]
        ankle = joints[f"ankle.{label}"]
        lower = label.lower()
        bind_to(add_segment(f"GOBLIN_Thigh_{label}", hip, knee, 0.098,
                            0.078, "cloth", collection, spec,
                            f"thigh_{lower}", sides=7), f"thigh.{label}")
        bind_to(add_ellipsoid(f"GOBLIN_Knee_{label}", knee,
                              (0.080, 0.080, 0.080), "cloth", collection,
                              spec, f"knee_{lower}"), f"shin.{label}")
        bind_to(add_segment(f"GOBLIN_Shin_{label}", knee,
                            ankle + gl(0.0, 0.04, 0.0), 0.076, 0.058,
                            "skin", collection, spec, f"shin_{lower}",
                            sides=7), f"shin.{label}")
        bind_to(add_box(f"GOBLIN_Boot_{label}", ankle + gl(0.0, 0.05, 0.06),
                        (0.22, 0.30, 0.10), "leather", collection, spec,
                        f"boot_{lower}"), f"foot.{label}")

    hand_r = joints["hand.R"]
    if spec.variant == "goblin_raider":
        bind_to(add_ellipsoid("GOBLIN_Helmet", at(head, 0.0, 0.21, -0.01),
                              (0.26, 0.245, 0.13), "metal", collection, spec,
                              "helmet"), "head")
        bind_to(add_box("GOBLIN_ChestArmor", at(chest, 0.0, 0.0, 0.17),
                        (0.38, 0.07, 0.30), "metal", collection, spec,
                        "armor"), "chest")
        bind_to(add_ellipsoid("GOBLIN_Pauldron", at(chest, -0.28, 0.025, 0.0),
                              (0.14, 0.16, 0.105), "metal", collection, spec,
                              "pauldron"), "chest")
        spear_bottom = at(hand_r, 0.0, -0.42, 0.0)
        spear_tip = at(hand_r, 0.0, 1.16, 0.0)
        bind_to(add_segment("GOBLIN_Spear", spear_bottom, spear_tip, 0.025,
                            0.018, "leather", collection, spec, "spear",
                            sides=6), "hand.R")
        bind_to(add_segment("GOBLIN_SpearHead", spear_tip,
                            at(spear_tip, 0.0, 0.24, 0.0), 0.075, 0.002,
                            "metal", collection, spec, "spear_head",
                            sides=5), "hand.R")
        bind_to(add_panel("GOBLIN_Pennant",
                          (spear_tip, at(spear_tip, -0.23, -0.10, 0.0),
                           at(spear_tip, 0.0, -0.21, 0.0)),
                          0.012, "accent", collection, spec, "pennant"),
                "hand.R")
    elif spec.variant == "goblin_tribute_bearer":
        bind_to(add_box("GOBLIN_Offering", at(chest, 0.0, -0.22, 0.30),
                        (0.48, 0.38, 0.38), "accent", collection, spec,
                        "offering", bevel=0.02), "chest")
        bind_to(add_box("GOBLIN_OfferingBand", at(chest, 0.0, -0.22, 0.505),
                        (0.10, 0.025, 0.40), "horn", collection, spec,
                        "offering_band", bevel=0.004), "chest")
    else:
        bind_to(add_box("GOBLIN_Pack", at(chest, 0.0, -0.04, -0.20),
                        (0.40, 0.18, 0.42), "secondary", collection, spec,
                        "pack"), "chest")
        bind_to(add_segment("GOBLIN_Bedroll", at(chest, -0.23, 0.20, -0.24),
                            at(chest, 0.23, 0.20, -0.24), 0.10, 0.10,
                            "hide", collection, spec, "bedroll", sides=8),
                "chest")
        bind_to(add_segment("GOBLIN_Strap", at(chest, -0.18, 0.19, 0.05),
                            at(pelvis, 0.14, 0.16, 0.17), 0.029, 0.029,
                            "leather", collection, spec, "strap", sides=5),
                "chest")
        hook_bottom = at(hand_r, 0.0, -0.25, 0.02)
        hook_bend = at(hook_bottom, 0.0, -0.08, 0.11)
        hook_tip = at(hook_bottom, 0.0, 0.04, 0.16)
        bind_to(add_segment("GOBLIN_HookHandle", hand_r, hook_bottom, 0.024,
                            0.022, "leather", collection, spec,
                            "hook_handle", sides=6), "hand.R")
        bind_to(add_segment("GOBLIN_HookBend", hook_bottom, hook_bend, 0.024,
                            0.020, "metal", collection, spec, "hook_bend",
                            sides=6), "hand.R")
        bind_to(add_segment("GOBLIN_HookTip", hook_bend, hook_tip, 0.020,
                            0.005, "metal", collection, spec, "hook_tip",
                            sides=6), "hand.R")


def add_goblin_hair(spec: CreatureSpec, collection: bpy.types.Collection,
                    head: Vector, parts: list[bpy.types.Object]) -> None:
    """Accent-slot hair, from the hand-drawn DrawGoblinHair shapes."""
    crest = spec.variant == "goblin_raider"
    bunches = spec.variant == "goblin_tribute_bearer"

    def at(x: float, y: float, z: float) -> Vector:
        return head + gl(x, y, z)

    parts.append(add_ellipsoid(
        "GOBLIN_HairCap", at(0.0, 0.20, -0.025),
        (0.12 if crest else 0.245, 0.20, 0.11 if bunches else 0.16),
        "accent", collection, spec, "hair_cap"))
    if bunches:
        for side, label in ((-1.0, "L"), (1.0, "R")):
            parts.append(add_ellipsoid(
                f"GOBLIN_HairBunch_{label}", at(side * 0.22, 0.37, -0.055),
                (0.20, 0.19, 0.23), "accent", collection, spec,
                f"hair_bunch_{label.lower()}"))
            local = ((side * 0.20, 0.37, -0.06), (side * 0.31, 0.60, -0.08),
                     (side * 0.21, 0.79, -0.07), (side * 0.09, 0.78, 0.005),
                     (side * 0.09, 0.67, 0.025))
            parts.append(add_tube(
                f"GOBLIN_HairTwist_{label}", tuple(at(*p) for p in local),
                (0.145, 0.16, 0.115, 0.060, 0.003), "accent", collection,
                spec, f"hair_twist_{label.lower()}", sides=7))
        return
    roots = ((-0.16, 0.15, -0.04), (0.16, 0.15, -0.04),
             (-0.10, 0.22, -0.04), (0.10, 0.22, -0.04),
             (0.00, 0.24, -0.06), (-0.10, 0.18, 0.10),
             (0.10, 0.18, 0.10))
    tips = ((-0.35, 0.49, -0.13), (0.40, 0.51, -0.12),
            (-0.02, 0.78, -0.10), (0.39, 0.67, -0.16),
            (0.23, 0.91, -0.13), (-0.13, 0.57, 0.10),
            (0.29, 0.59, 0.10))
    radii = (0.11, 0.13, 0.068, 0.003)
    lock_scale = 0.72 if crest else 1.0
    for lock in range(5 if crest else 7):
        root = Vector(roots[lock])
        tip = Vector(tips[lock])
        if crest:
            root = Vector((0.0, 0.27, 0.16 - lock * 0.085))
            tip = Vector((0.018, 1.02 - lock * 0.09, -0.13 - lock * 0.095))
        local = (root, root.lerp(tip, 0.34), root.lerp(tip, 0.72), tip)
        parts.append(add_tube(
            f"GOBLIN_HairLock_{lock}", tuple(at(*p) for p in local),
            tuple(radius * lock_scale for radius in radii), "accent",
            collection, spec, f"hair_lock_{lock}", sides=6))


def skin_humanoid(collection: bpy.types.Collection,
                  spec: CreatureSpec,
                  bone_points: dict[str, tuple[Vector, Vector]],
                  ) -> bpy.types.Object:
    """Rigid one-bone weights for every part, from its cc_deform_bone tag."""
    armature_data = bpy.data.armatures.new(f"RIG_{spec.variant}")
    rig = bpy.data.objects.new(f"RIG_{spec.variant}", armature_data)
    collection.objects.link(rig)
    rig.show_in_front = True
    rig["cc_asset_id"] = spec.asset_id
    rig["cc_rig_contract"] = "CcHumanoidSkinPose"
    rig["cc_skeleton"] = "humanoid"
    rig["cc_bone_count"] = len(HUMANOID_BONES)

    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    edit_bones: dict[str, bpy.types.EditBone] = {}
    for name, _parent in HUMANOID_BONES:
        bone = armature_data.edit_bones.new(name)
        bone.head, bone.tail = bone_points[name]
        bone.use_deform = True
        edit_bones[name] = bone
    for name, parent in HUMANOID_BONES:
        if parent is not None:
            edit_bones[name].parent = edit_bones[parent]
            edit_bones[name].use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.select_set(False)

    for obj in tuple(collection.objects):
        if obj.type != "MESH":
            continue
        bone_name = obj.get("cc_deform_bone")
        if bone_name not in edit_bones:
            raise RuntimeError(
                f"{spec.variant} part {obj.name} has no deform bone")
        apply_modifiers(obj)
        obj.vertex_groups.clear()
        group = obj.vertex_groups.new(name=bone_name)
        group.add(tuple(range(len(obj.data.vertices))), 1.0, "REPLACE")
        modifier = obj.modifiers.new("CC_HumanoidSkin", "ARMATURE")
        modifier.object = rig
        world = obj.matrix_world.copy()
        obj.parent = rig
        obj.matrix_parent_inverse = rig.matrix_world.inverted()
        obj.matrix_world = world
    return rig


def quadruped_stride(spec: CreatureSpec) -> dict[str, float]:
    phase = pose_phase(spec.pose)
    moving = spec.pose != "idle"
    if not moving:
        return {"fl": 0.0, "fr": 0.0, "hl": 0.0, "hr": 0.0,
                "lift_fl": 0.0, "lift_fr": 0.0,
                "lift_hl": 0.0, "lift_hr": 0.0, "bob": 0.0}
    offsets = {"fl": 0.0, "fr": 0.5, "hl": 0.5, "hr": 0.0}
    result: dict[str, float] = {}
    for name, offset in offsets.items():
        value = math.sin((phase + offset) * math.tau)
        result[name] = value
        result[f"lift_{name}"] = max(0.0, value) * 0.15
    result["bob"] = -0.030 * abs(math.sin(phase * math.tau * 2.0))
    return result


def paint_coat_patch(obj: bpy.types.Object, spec: CreatureSpec) -> None:
    """Put broad markings on the coat itself so they follow the skin exactly."""
    obj.data.materials.append(PREVIEW_MATERIALS[(spec.family, "secondary")])
    for face in obj.data.polygons:
        center = sum((obj.data.vertices[i].co for i in face.vertices), Vector()) / len(face.vertices)
        # Two offset patches on each flank, with a clear light band between.
        if abs(center.x) > 0.28 and (
            ((center.y + 0.38) / 0.32) ** 2 + ((center.z - 0.08) / 0.36) ** 2 < 1.0
            or ((center.y - 0.44) / 0.29) ** 2 + ((center.z + 0.02) / 0.32) ** 2 < 1.0
        ):
            face.material_index = len(obj.data.materials) - 1


def add_animal_ear(spec: CreatureSpec, collection: bpy.types.Collection,
                   head: Vector, sign: float, side: str) -> None:
    upright = spec.family == "horse"
    center = head + Vector((0.19 * sign, 0.015, 0.32)) if upright else head + Vector((0.34 * sign, 0.0, 0.10))
    size = (0.088, 0.070, 0.19) if upright else (0.22, 0.095, 0.085)
    ear = add_ellipsoid(f"{spec.family}_Ear_{side}", center, size,
                       "skin" if upright else "secondary", collection, spec,
                       f"ear_{side.lower()}", subdivisions=2 if upright else 1)
    # One curious ear and one relaxed ear give the pony an alert expression.
    ear.rotation_euler.y = (-0.12 if sign < 0.0 else 0.52) if upright else sign * -0.20
    inner = add_ellipsoid(f"{spec.family}_InnerEar_{side}",
                         center + Vector((0.0, -0.052, 0.0)),
                         (size[0] * 0.66, 0.022, size[2] * 0.66),
                         "accent", collection, spec, f"ear_{side.lower()}")
    inner.rotation_euler.y = ear.rotation_euler.y


def add_pony_lock(name: str, rings: tuple[tuple[float, ...], ...],
                  collection: bpy.types.Collection, spec: CreatureSpec,
                  part: str) -> bpy.types.Object:
    """Shape a continuous lock, with a narrow ribbon along its length."""
    sides = 8
    vertices = []
    faces = []
    for index, ring in enumerate(rings):
        center = Vector(ring[:3])
        before = Vector(rings[max(0, index - 1)][:3])
        after = Vector(rings[min(len(rings) - 1, index + 1)][:3])
        tangent = (after - before).normalized()
        right = Vector((1.0, 0.0, 0.0))
        right = (right - tangent * right.dot(tangent)).normalized()
        up = tangent.cross(right).normalized()
        for side in range(sides):
            angle = math.tau * side / sides
            vertices.append(tuple(center + right * math.cos(angle) * ring[3]
                                  + up * math.sin(angle) * ring[4]))
        if index:
            for side in range(sides):
                following = (side + 1) % sides
                previous = (index - 1) * sides
                current = index * sides
                faces.append((previous + side, previous + following,
                              current + following, current + side))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple((len(rings) - 1) * sides + side
                       for side in range(sides)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    assign(obj, "secondary", spec)
    assign(obj, "cloth", spec)
    for face in mesh.polygons:
        if face.index < (len(rings) - 1) * sides:
            face.material_index = 1 if face.index % sides == 1 else 0
    tag(obj, spec, part)
    return obj


def add_pony_eye(spec: CreatureSpec, collection: bpy.types.Collection,
                 head: Vector, sign: float, side: str) -> None:
    # Put every layer on the same cheek plane so the eye sits in its socket.
    normal = Vector((sign * 0.72, -0.69, 0.08)).normalized()
    right = Vector((0.0, 0.0, 1.0)).cross(normal).normalized()
    up = normal.cross(right)
    rotation = Matrix((right, up, normal)).transposed().to_euler()
    center = head + Vector((sign * 0.235, -0.235, 0.10))
    layers = (
        ("Lid", 0.0, (0.103, 0.130, 0.036), "secondary"),
        ("White", 0.022, (0.088, 0.112, 0.025), "horn"),
        ("Iris", 0.044, (0.060, 0.086, 0.015), "metal"),
        ("Pupil", 0.057, (0.035, 0.066, 0.008), "eye"),
    )
    for name, depth, size, semantic in layers:
        eye = add_ellipsoid(f"PONY_Eye{name}_{side}",
                            center + normal * depth, size, semantic,
                            collection, spec, f"eye_{side.lower()}",
                            subdivisions=2)
        eye.rotation_euler = rotation
    glint = add_ellipsoid(f"PONY_EyeGlint_{side}",
                          center + normal * 0.066 + up * 0.037 - right * 0.016,
                          (0.016, 0.021, 0.007), "horn", collection, spec,
                          f"eye_{side.lower()}")
    glint.rotation_euler = rotation


def add_pony_bridle(spec: CreatureSpec, collection: bpy.types.Collection,
                    head: Vector) -> None:
    """A simple, readable headstall: a browband, a noseband, and the cheek
    straps between them. It stays on the head bone with the eyes and ears
    so the later face exaggeration carries it along at the same scale."""
    for side, sign in (("L", -1.0), ("R", 1.0)):
        brow = head + Vector((0.275 * sign, -0.06, 0.285))
        nose = head + Vector((0.185 * sign, -0.495, -0.045))
        add_segment(f"PONY_BridleBrow_{side}", brow,
                    head + Vector((0.0, -0.03, 0.30)), 0.028, 0.026,
                    "leather", collection, spec, "bridle", sides=6)
        add_segment(f"PONY_BridleCheek_{side}", brow, nose, 0.026, 0.022,
                    "leather", collection, spec, "bridle", sides=6)
        add_segment(f"PONY_BridleNose_{side}", nose,
                    head + Vector((0.0, -0.535, -0.075)), 0.024, 0.024,
                    "leather", collection, spec, "bridle", sides=6)
        add_ellipsoid(f"PONY_BridleBuckle_{side}", brow,
                      (0.032, 0.028, 0.032), "metal", collection, spec,
                      "bridle", subdivisions=1)


def add_pony_collar(spec: CreatureSpec,
                    collection: bpy.types.Collection, body_z: float) -> None:
    """A padded breast collar built at the same model-space points the
    runtime harness in road_book.inc already reads off the neck and chest
    bones (RoadPonySkinPoint's CC_QUADRUPED_NECK/CC_QUADRUPED_CHEST calls),
    so the baked collar and the dynamic hitch trace line up on the nose."""
    top = Vector((0.0, -0.65, body_z + 0.59))
    bottom = Vector((0.0, -0.60, body_z - 0.05))
    for side in (-1.0, 1.0):
        tag_side = "L" if side < 0.0 else "R"
        collar_side = Vector((side * 0.44, -0.64, body_z + 0.21))
        # The trace socket: identical to the shoulder point road_book.inc
        # already uses for DrawRoadHorseHarness and the hitch trace end.
        trace_point = Vector((side * 0.47, -0.08, body_z + 0.17))
        add_segment(f"PONY_CollarCrest_{tag_side}", top, collar_side,
                    0.052, 0.048, "leather", collection, spec,
                    "collar_crest", sides=8)
        add_segment(f"PONY_CollarThroat_{tag_side}", collar_side, bottom,
                    0.046, 0.042, "leather", collection, spec,
                    "collar", sides=8)
        add_segment(f"PONY_TraceGuide_{tag_side}", collar_side, trace_point,
                    0.030, 0.026, "leather", collection, spec,
                    "collar", sides=6)
        add_ellipsoid(f"PONY_TraceRing_{tag_side}", trace_point,
                      (0.034, 0.034, 0.034), "metal", collection, spec,
                      "collar", subdivisions=1)


def add_pony_saddle_pad(spec: CreatureSpec,
                        collection: bpy.types.Collection,
                        body_z: float) -> None:
    """A back band across the withers, the third readable tack piece, plus
    a girth strap so it looks fastened rather than resting loose. It sits
    in the short clear saddle of the back between the shoulder bulge and
    the haunches, not on top of either one."""
    add_box("PONY_SaddlePad", (0.0, 0.02, body_z + 0.40),
            (0.56, 0.32, 0.12), "cloth", collection, spec, "saddle_pad",
            bevel=0.02)
    for side in (-1.0, 1.0):
        tag_side = "L" if side < 0.0 else "R"
        add_segment(f"PONY_Girth_{tag_side}",
                    (side * 0.28, 0.02, body_z + 0.34),
                    (side * 0.19, 0.02, body_z - 0.22), 0.026, 0.022,
                    "leather", collection, spec, "saddle_pad", sides=6)


def build_quadruped(spec: CreatureSpec,
                    collection: bpy.types.Collection) -> None:
    cow = spec.family == "cow"
    sheep = spec.family == "sheep"
    pony = spec.family == "horse"
    stride = quadruped_stride(spec)
    body_z = (0.76 if sheep else 1.08 if cow else 0.98) + stride["bob"]
    half_width = 0.30 if sheep else 0.38 if cow else 0.34
    front_y = -0.43 if sheep else -0.57 if cow else -0.52
    hind_y = 0.43 if sheep else 0.57 if cow else 0.52
    body_scale = ((0.50, 0.69, 0.43) if sheep else
                  (0.62, 0.93, 0.45) if cow else
                  (0.49, 0.70, 0.44))
    body_semantic = "cloth" if sheep else "hide" if cow else "skin"
    barrel = add_ellipsoid("CREATURE_Barrel", (0.0, 0.0, body_z), body_scale,
                  body_semantic, collection, spec, "barrel", subdivisions=2)
    if cow:
        paint_coat_patch(barrel, spec)
    chest_y = -0.38 if sheep else -0.53 if cow else -0.44
    add_ellipsoid("CREATURE_Chest", (0.0, chest_y, body_z + 0.06),
                  (body_scale[0] * 0.91,
                   0.34 if sheep else 0.43 if cow else 0.38,
                   body_scale[2] * 1.04),
                  body_semantic if sheep else "skin", collection, spec,
                  "chest", subdivisions=1)
    if sheep:
        for index, (x, y, z, sx, sy, sz, part) in enumerate((
            (0.0, -0.28, 0.91, 0.49, 0.40, 0.40, "chest"),
            (0.0, 0.04, 1.03, 0.52, 0.43, 0.39, "barrel"),
            (0.0, 0.35, 0.91, 0.48, 0.39, 0.38, "barrel"),
        )):
            add_ellipsoid(f"SHEEP_Fleece_{index}", (x, y, z),
                          (sx, sy, sz), "cloth", collection, spec, part,
                          subdivisions=1)
        for side in (-1.0, 1.0):
            for index, y in enumerate((-0.40, -0.08, 0.25, 0.49)):
                add_ellipsoid(f"SHEEP_FleeceFlank_{side}_{index}",
                              (side * 0.37, y, body_z + 0.13),
                              (0.23, 0.26, 0.28), "cloth", collection, spec,
                              "chest" if index == 0 else "barrel")
    elif pony:
        add_ellipsoid("PONY_RoundRump", (0.0, 0.38, body_z - 0.02),
                      (0.51, 0.44, 0.38), "skin", collection, spec,
                      "barrel", subdivisions=2)
        # Two haunch bulges break the rump into a readable pair of quarters
        # from behind instead of one smooth ball, and their outward-facing
        # normals pick up the darker side shading the paint contract already
        # gives skin, so the seam between them reads without a new material.
        for side in (-1.0, 1.0):
            add_ellipsoid(f"PONY_Haunch_{'L' if side < 0 else 'R'}",
                          (side * 0.28, 0.42, body_z - 0.09),
                          (0.23, 0.30, 0.28), "skin", collection, spec,
                          "barrel", subdivisions=1)
        add_ellipsoid("PONY_RoundShoulder", (0.0, -0.38, body_z + 0.12),
                      (0.47, 0.40, 0.45), "skin", collection, spec,
                      "chest", subdivisions=2)

    # A wider hind stance than fore reads as a draught animal set to pull,
    # and keeps the two hind legs from silhouetting into one mass from
    # behind once the haunches above them are bigger.
    hind_half_width = half_width + (0.055 if pony else 0.0)
    leg_roots = {
        "fl": Vector((-half_width, front_y, body_z - 0.08)),
        "fr": Vector((half_width, front_y, body_z - 0.08)),
        "hl": Vector((-hind_half_width, hind_y, body_z - 0.10)),
        "hr": Vector((hind_half_width, hind_y, body_z - 0.10)),
    }
    for name, root in leg_roots.items():
        travel = stride[name] * (0.18 if sheep else 0.20 if cow else 0.23)
        lift = stride[f"lift_{name}"]
        hoof = Vector((root.x, root.y - travel, 0.10 + lift))
        direction = -1.0 if name.startswith("f") else 1.0
        hind_leg = name.startswith("h")
        knee_kick = 0.15 if (pony and hind_leg) else 0.10
        knee = (root + hoof) * 0.5 + Vector((0.0, direction * knee_kick, 0.02))
        radius = 0.070 if sheep else 0.105 if cow else 0.092
        add_segment(f"CREATURE_UpperLeg_{name.upper()}", root, knee,
                    radius, radius * 0.82,
                    "secondary" if sheep else "skin", collection, spec,
                    f"upper_leg_{name}")
        add_segment(f"CREATURE_LowerLeg_{name.upper()}", knee, hoof,
                    radius * 0.78, radius * 0.58,
                    "hide" if pony else "secondary", collection,
                    spec, f"lower_leg_{name}")
        if pony and hind_leg:
            # The hock: a small joint mass right where the gaskin turns
            # into the cannon, so the hind leg reads as a bent, working
            # limb instead of a single straight taper under the rump.
            add_ellipsoid(f"PONY_Hock_{name.upper()}",
                          knee + Vector((0.0, direction * 0.02, -0.015)),
                          (radius * 0.92, radius * 0.80, radius * 0.72),
                          "hide", collection, spec, f"lower_leg_{name}",
                          subdivisions=1)
        hoof_size = ((0.12, 0.16, 0.09) if sheep else
                     (0.18, 0.23, 0.12) if cow else
                     (0.17, 0.20, 0.12))
        add_box(f"CREATURE_Hoof_{name.upper()}",
                hoof + Vector((0.0, -0.035, -0.015)), hoof_size,
                "leather" if pony else "secondary", collection, spec,
                f"hoof_{name}", bevel=0.025 if pony else 0.012)
        if pony:
            add_ellipsoid(
                f"PONY_HoofFeather_{name.upper()}",
                hoof + Vector((0.0, 0.0, 0.10)),
                (0.13, 0.14, 0.13), "hide", collection, spec,
                f"lower_leg_{name}")

    if sheep:
        neck_start = Vector((0.0, -0.36, body_z + 0.14))
        neck_end = Vector((0.0, -0.62, body_z + 0.08))
        head = Vector((0.0, -0.82, body_z + 0.04))
        add_segment("SHEEP_Neck", neck_start, neck_end, 0.21, 0.17,
                    "secondary", collection, spec, "neck", sides=8)
        add_ellipsoid("SHEEP_Head", head, (0.26, 0.35, 0.29),
                      "secondary", collection, spec, "head", subdivisions=2)
        add_ellipsoid("SHEEP_Muzzle",
                      head + Vector((0.0, -0.28, -0.07)),
                      (0.20, 0.17, 0.14), "skin", collection, spec,
                      "muzzle")
        add_ellipsoid("SHEEP_WoolCap",
                      head + Vector((0.0, 0.01, 0.20)),
                      (0.22, 0.18, 0.16), "cloth", collection, spec,
                      "head", subdivisions=1)
        for side, sign in (("L", -1.0), ("R", 1.0)):
            add_animal_ear(spec, collection, head, sign, side)
            add_ellipsoid(f"SHEEP_Eye_{side}",
                          head + Vector((0.20 * sign, -0.22, 0.08)),
                          (0.044, 0.025, 0.050), "eye", collection, spec,
                          f"eye_{side.lower()}")
    elif cow:
        neck_start = Vector((0.0, -0.52, body_z + 0.20))
        neck_end = Vector((0.0, -0.91, body_z + 0.08))
        head = Vector((0.07, -1.17, body_z - 0.01))
        add_segment("COW_Neck", neck_start, neck_end, 0.27, 0.23, "skin",
                    collection, spec, "neck", sides=9)
        add_ellipsoid("COW_Head", head, (0.35, 0.43, 0.30), "skin",
                      collection, spec, "head", subdivisions=2)
        add_ellipsoid("COW_Muzzle", head + Vector((0.0, -0.31, -0.06)),
                      (0.32, 0.22, 0.18), "secondary", collection, spec,
                      "muzzle")
        for side, sign in (("L", -1.0), ("R", 1.0)):
            eye = head + Vector((0.25 * sign, -0.20, 0.085))
            add_ellipsoid(f"COW_Eye_{side}", eye, (0.050, 0.027, 0.056),
                          "eye", collection, spec, f"eye_{side.lower()}")
            add_animal_ear(spec, collection, head, sign, side)
            horn_base = head + Vector((0.26 * sign, -0.02, 0.22))
            horn_mid = horn_base + Vector((0.19 * sign, 0.015, 0.045))
            horn_tip = horn_base + Vector((0.26 * sign, 0.045, 0.25))
            add_segment(f"COW_HornBase_{side}", horn_base, horn_mid,
                        0.082, 0.055, "horn", collection, spec,
                        f"horn_{side.lower()}", sides=6)
            add_segment(f"COW_HornTip_{side}", horn_mid, horn_tip,
                        0.055, 0.008, "horn", collection, spec,
                        f"horn_{side.lower()}", sides=6)
            add_ellipsoid(f"COW_Nostril_{side}",
                          head + Vector((0.16 * sign, -0.505, -0.025)),
                          (0.060, 0.026, 0.037), "eye", collection, spec, "muzzle")
        add_ellipsoid("COW_Udder", (0.0, 0.26, body_z - 0.47),
                      (0.24, 0.28, 0.16), "accent", collection, spec, "udder")
        for x in (-0.12, 0.12):
            add_segment("COW_Teat", (x, 0.18, body_z - 0.54),
                        (x, 0.18, body_z - 0.70), 0.035, 0.025, "accent",
                        collection, spec, "teat", sides=6)
    else:
        neck_start = Vector((0.0, -0.42, body_z + 0.22))
        neck_end = Vector((0.0, -0.70, body_z + 0.43))
        head = Vector((0.0, -0.93, body_z + 0.43))
        add_segment("HORSE_Neck", neck_start, neck_end, 0.30, 0.22, "skin",
                    collection, spec, "neck", sides=9)
        add_ellipsoid("HORSE_Head", head, (0.31, 0.37, 0.32), "skin",
                      collection, spec, "head", subdivisions=2)
        add_ellipsoid("HORSE_Muzzle", head + Vector((0.0, -0.32, -0.14)),
                      (0.24, 0.25, 0.18), "hide", collection, spec,
                      "muzzle", subdivisions=2)
        add_ellipsoid("HORSE_Blaze", head + Vector((0.0, -0.345, 0.075)),
                      (0.065, 0.038, 0.16), "horn", collection, spec, "head",
                      subdivisions=2)
        for side, sign in (("L", -1.0), ("R", 1.0)):
            add_animal_ear(spec, collection, head, sign, side)
            add_ellipsoid(f"HORSE_Nostril_{side}",
                          head + Vector((0.135 * sign, -0.522, -0.10)),
                          (0.037, 0.018, 0.028), "eye", collection, spec, "muzzle")
            add_pony_eye(spec, collection, head, sign, side)
        add_pony_bridle(spec, collection, head)
        add_pony_lock("PONY_SweptForelock", (
            (0.12, -0.85, body_z + 0.69, 0.07, 0.045),
            (0.10, -0.94, body_z + 0.73, 0.13, 0.055),
            (0.01, -1.06, body_z + 0.70, 0.14, 0.070),
            (-0.13, -1.14, body_z + 0.62, 0.12, 0.065),
            (-0.23, -1.12, body_z + 0.52, 0.065, 0.040),
            (-0.25, -1.06, body_z + 0.46, 0.012, 0.012),
        ), collection, spec, "head")
        add_pony_lock("PONY_DrapedMane", (
            (-0.015, -0.76, body_z + 0.68, 0.11, 0.060),
            (-0.20, -0.63, body_z + 0.70, 0.17, 0.075),
            (-0.34, -0.53, body_z + 0.62, 0.17, 0.085),
            (-0.43, -0.47, body_z + 0.48, 0.15, 0.085),
            (-0.48, -0.44, body_z + 0.32, 0.10, 0.065),
            (-0.46, -0.40, body_z + 0.17, 0.015, 0.018),
        ), collection, spec, "mane")

    if sheep:
        tail_base = Vector((0.0, 0.60, body_z + 0.14))
        tail_mid = Vector((0.0, 0.75, body_z + 0.10))
        tail_end = Vector((0.0, 0.86, body_z - 0.02))
        tail_semantic = "cloth"
        tail_radius = 0.10
    elif pony:
        tail_base = Vector((0.0, 0.68, body_z + 0.12))
        tail_mid = Vector((0.0, 0.94, body_z - 0.02))
        tail_end = Vector((0.0, 1.17, body_z - 0.48))
        tail_semantic = "secondary"
        tail_radius = 0.105
    else:
        tail_base = Vector((0.0, 0.82, body_z + 0.08))
        tail_mid = Vector((0.0, 1.10, body_z - 0.08))
        tail_end = Vector((0.0, 1.28,
                           body_z - (0.62 if cow else 0.48)))
        tail_semantic = "secondary"
        tail_radius = 0.075
    if not pony:
        add_segment("CREATURE_TailRoot", tail_base, tail_mid,
                    tail_radius, tail_radius * 0.73, tail_semantic,
                    collection, spec, "tail_root")
        add_segment("CREATURE_Tail", tail_mid, tail_end,
                    tail_radius * 0.78, tail_radius * 0.46, tail_semantic,
                    collection, spec, "tail")
    if pony:
        # A fuller dock that starts behind the haunch bulges (not inside
        # them) so the tail reads as its own shape even from close behind,
        # instead of the tip alone poking out past the rump.
        add_pony_lock("PONY_FlowingTail", (
            (0.0, 0.70, body_z + 0.10, 0.115, 0.095),
            (0.0, 0.84, body_z + 0.10, 0.18, 0.145),
            (-0.025, 0.97, body_z + 0.00, 0.20, 0.16),
            (-0.045, 1.07, body_z - 0.14, 0.20, 0.14),
            (-0.025, 1.14, body_z - 0.31, 0.18, 0.12),
            (0.025, 1.19, body_z - 0.47, 0.14, 0.10),
            (0.08, 1.17, body_z - 0.60, 0.085, 0.06),
            (0.10, 1.12, body_z - 0.66, 0.012, 0.012),
        ), collection, spec, "tail_flow")
        add_pony_collar(spec, collection, body_z)
        add_pony_saddle_pad(spec, collection, body_z)
        # Enlarge the whole face together, including its eyes and markings.
        # The head bone stays at the shared gait pivot.
        head_parts = {"head", "muzzle", "ear_l", "ear_r", "eye_l", "eye_r",
                      "bridle"}
        head_scale = Matrix.Diagonal((1.18, 1.12, 1.10, 1.0))
        exaggerate = Matrix.Translation(head) @ head_scale @ Matrix.Translation(-head)
        for obj in collection.objects:
            if obj.type == "MESH" and obj.get("cc_part") in head_parts:
                obj.matrix_world = exaggerate @ obj.matrix_world


def add_dragon_leg(spec: CreatureSpec, collection: bpy.types.Collection,
                   name: str, root: Vector, hoof: Vector, thickness: float,
                   foot: tuple[float, float, float], claw_count: int = 3,
                   bend: float = 0.0) -> None:
    elbow = (root + hoof) * 0.5 + Vector((root.x * 0.20, bend, 0.10))
    add_segment(f"DRAGON_UpperLeg_{name.upper()}", root, elbow,
                thickness, thickness * 0.72, "skin", collection, spec,
                f"upper_leg_{name}", sides=9)
    add_segment(f"DRAGON_LowerLeg_{name.upper()}", elbow, hoof,
                thickness * 0.72, thickness * 0.44, "secondary",
                collection, spec, f"lower_leg_{name}", sides=8)
    add_box(f"DRAGON_Foot_{name.upper()}",
            hoof + Vector((0.0, -foot[1] * 0.16, 0.0)), foot,
            "secondary", collection, spec, f"foot_{name}")
    spacing = foot[0] * 0.24
    for claw in range(claw_count):
        offset = (float(claw) - float(claw_count - 1) * 0.5) * spacing
        add_cone(f"DRAGON_Claw_{name.upper()}_{claw}",
                 hoof + Vector((offset, -foot[1] * 0.68, 0.012)),
                 max(0.022, thickness * 0.18), foot[1] * 0.42,
                 "horn", collection, spec, f"claw_{name}_{claw}",
                 rotation=(math.pi * 0.5, 0.0, 0.0), vertices=5)


def add_dragon_neck(spec: CreatureSpec, collection: bpy.types.Collection,
                    points: tuple[Vector, ...],
                    radii: tuple[float, ...]) -> None:
    for index in range(len(points) - 1):
        add_segment(f"DRAGON_Neck_{index}", points[index], points[index + 1],
                    radii[index], radii[index + 1], "skin", collection,
                    spec, f"neck_{index}", sides=10)


def add_dragon_tail(spec: CreatureSpec, collection: bpy.types.Collection,
                    points: tuple[Vector, ...],
                    radii: tuple[float, ...]) -> None:
    for index in range(len(points) - 1):
        add_segment(f"DRAGON_Tail_{index}", points[index], points[index + 1],
                    radii[index], radii[index + 1], "skin", collection,
                    spec, f"tail_{index}", sides=9)


def add_dragon_wing(
    spec: CreatureSpec, collection: bpy.types.Collection, side: str,
    shoulder: Vector, elbow: Vector, wrist: Vector,
    fingers: tuple[Vector, ...], trailing_anchor: Vector,
    arm_radius: float, finger_radius: float, membrane_thickness: float,
    *, fold_amount: float = 0.34, fold_depth: float = 0.10,
    membrane_semantic: str = "secondary", fold_semantic: str = "accent",
) -> None:
    """A strutted wing: an arm and forearm bone, a fan of finger struts,
    and membrane panels between them with a scalloped trailing edge
    folded out of the wing's own plane. Real strut volume plus a
    genuine fold keeps the wing reading as a structure, not one flat
    plane, and keeps it visible edge-on."""
    add_segment(f"WING_UpperArm_{side}", shoulder, elbow,
                arm_radius, arm_radius * 0.66, "skin", collection, spec,
                f"wing_arm_{side.lower()}", sides=8)
    add_segment(f"WING_Forearm_{side}", elbow, wrist,
                arm_radius * 0.66, arm_radius * 0.40, "skin", collection,
                spec, f"wing_arm_{side.lower()}", sides=7)
    for index, tip in enumerate(fingers):
        radius = finger_radius * (1.0 - index * 0.16)
        add_segment(f"WING_Finger_{side}_{index}", wrist, tip,
                    radius, radius * 0.20, "skin", collection, spec,
                    f"wing_finger_{side.lower()}_{index}", sides=5)
    span = fingers[-1] - wrist
    trail = trailing_anchor - wrist
    normal = span.cross(trail)
    normal = normal.normalized() if normal.length > 1e-6 else Vector((0.0, 0.0, 1.0))
    for index in range(len(fingers) - 1):
        tip_a = fingers[index]
        tip_b = fingers[index + 1]
        valley = tip_a.lerp(tip_b, 0.5).lerp(wrist, fold_amount)
        fold_sign = 1.0 if index % 2 == 0 else -1.0
        valley = valley + normal * (fold_depth * fold_sign)
        semantic = membrane_semantic if index % 2 == 0 else fold_semantic
        add_prism(f"WING_Membrane_{side}_{index}",
                  (wrist, tip_a, valley, tip_b), membrane_thickness,
                  semantic, collection, spec,
                  f"wing_membrane_{side.lower()}_{index}")
    trailing_valley = fingers[-1].lerp(trailing_anchor, 0.5).lerp(
        wrist, fold_amount * 0.7)
    trailing_valley = trailing_valley - normal * (fold_depth * 0.6)
    add_prism(f"WING_Membrane_{side}_trailing",
              (wrist, fingers[-1], trailing_valley, trailing_anchor),
              membrane_thickness, fold_semantic, collection, spec,
              f"wing_membrane_{side.lower()}_trailing")


def build_dragon_whelp(spec: CreatureSpec,
                       collection: bpy.types.Collection) -> None:
    phase = pose_phase(spec.pose)
    moving = spec.pose in ("stalk_a", "stalk_b")
    resting = spec.pose == "rest"
    threat = spec.pose == "threat"
    cycle = math.sin(phase * math.tau)
    fin_twitch = cycle * 0.10 if moving else 0.08 if threat else 0.0
    head_sway = cycle * 0.035 if moving else 0.0
    body_z = 0.52 if resting else 0.78
    add_ellipsoid("WHELP_RoundBody", (0.0, 0.10, body_z),
                  (0.48, 0.58, 0.44), "skin", collection, spec, "body",
                  subdivisions=2)
    add_ellipsoid("WHELP_Belly", (0.0, -0.30, body_z - 0.10),
                  (0.48, 0.42, 0.42), "hide", collection, spec, "chest",
                  subdivisions=2)
    add_ellipsoid("WHELP_Chest", (0.0, -0.36, body_z + 0.06),
                  (0.54, 0.40, 0.48), "skin", collection, spec, "chest",
                  subdivisions=2)
    add_ellipsoid("WHELP_Haunch", (0.0, 0.48, body_z - 0.04),
                  (0.48, 0.38, 0.42), "skin", collection, spec, "haunch",
                  subdivisions=2)
    for name, sign, longitudinal, offset in (
        ("fl", -1.0, -0.36, 0.0), ("fr", 1.0, -0.36, 0.5),
        ("hl", -1.0, 0.42, 0.5), ("hr", 1.0, 0.42, 0.0),
    ):
        leg_cycle = math.sin((phase + offset) * math.tau)
        root = Vector((0.38 * sign, longitudinal, body_z - 0.12))
        travel = leg_cycle * 0.11 if moving else 0.0
        lift = max(0.0, leg_cycle) * 0.08 if moving else 0.0
        hoof = Vector((0.50 * sign, longitudinal - travel,
                       0.12 + lift if not resting else 0.11))
        add_dragon_leg(spec, collection, name, root, hoof, 0.105,
                       (0.26, 0.30, 0.11), claw_count=2,
                       bend=-0.06 if longitudinal < 0.0 else 0.06)

    neck = (
        Vector((0.0, -0.42, body_z + 0.20)),
        Vector((0.0, -0.78, body_z + 0.34)),
    )
    add_dragon_neck(spec, collection, neck, (0.24, 0.18))
    head = Vector((head_sway, -1.03,
                   body_z + (0.48 if threat else 0.32)))
    add_ellipsoid("WHELP_OversizedHead", head, (0.47, 0.49, 0.38),
                  "skin", collection, spec, "head", subdivisions=2)
    add_box("WHELP_SnubJaw",
            head + Vector((0.0, -0.36, -0.15 if not threat else -0.22)),
            (0.54, 0.40, 0.16), "secondary", collection, spec, "jaw",
            rotation=(0.12 if threat else 0.0, 0.0, 0.0), bevel=0.035)
    for side, sign in (("L", -1.0), ("R", 1.0)):
        add_ellipsoid(f"WHELP_Eye_{side}",
                      head + Vector((0.32 * sign, -0.31, 0.12)),
                      (0.100, 0.040, 0.115), "eye", collection, spec,
                      f"eye_{side.lower()}")
        add_ellipsoid(f"WHELP_BrowRidge_{side}",
                      head + Vector((0.30 * sign, -0.22, 0.24)),
                      (0.15, 0.075, 0.10), "secondary", collection, spec,
                      f"eye_{side.lower()}")
        add_cone(f"WHELP_BrowNub_{side}",
                 head + Vector((0.20 * sign, -0.04, 0.34)),
                 0.045, 0.13, "horn", collection, spec,
                 f"brow_nub_{side.lower()}",
                 rotation=(0.30, 0.0, 0.0), vertices=5)
        ear_points = (
            (0.30 * sign, head.y + 0.01, head.z + 0.18),
            (0.64 * sign, head.y + 0.14,
             head.z + 0.30 + fin_twitch * sign),
            (0.34 * sign, head.y + 0.18, head.z - 0.02),
        )
        add_prism(f"WHELP_EarFin_{side}", ear_points, 0.035, "accent",
                  collection, spec, f"ear_fin_{side.lower()}")

    add_dragon_tail(
        spec, collection,
        (Vector((0.0, 0.58, body_z)), Vector((0.0, 1.10, body_z - 0.12)),
         Vector((0.24 * cycle, 1.62, body_z - 0.28)),
         Vector((0.34 * cycle, 1.98, body_z - 0.22))),
        (0.22, 0.15, 0.075, 0.018))
    for side, sign in (("L", -1.0), ("R", 1.0)):
        shoulder = Vector((0.34 * sign, -0.10, body_z + 0.32))
        elbow = Vector((0.62 * sign, 0.10 + cycle * 0.04 * sign,
                        body_z + 0.58))
        wrist = Vector((0.80 * sign, 0.22, body_z + 0.42))
        fingers = (
            Vector((0.98 * sign, 0.50, body_z + 0.20)),
            Vector((0.68 * sign, 0.58, body_z + 0.02)),
        )
        trailing_anchor = Vector((0.30 * sign, 0.30, body_z + 0.14))
        add_dragon_wing(spec, collection, side, shoulder, elbow, wrist,
                       fingers, trailing_anchor, 0.075, 0.050, 0.032,
                       fold_amount=0.30, fold_depth=0.055)
    for crest, y in enumerate((-0.74, -0.45, -0.10)):
        add_cone(f"WHELP_Crest_{crest}", (0.0, y, body_z + 0.58),
                 0.065, 0.15, "horn", collection, spec, f"crest_{crest}",
                 vertices=5)


def build_dragon_wanderer(spec: CreatureSpec,
                           collection: bpy.types.Collection) -> None:
    phase = pose_phase(spec.pose)
    moving = spec.pose in ("stalk_a", "stalk_b")
    resting = spec.pose == "rest"
    threat = spec.pose == "threat"
    cycle = math.sin(phase * math.tau)
    body_z = 0.90 if resting else 1.55
    add_ellipsoid("WANDERER_Chest", (0.0, -0.58, body_z + 0.10),
                  (0.56, 0.62, 0.48), "skin", collection, spec, "chest",
                  subdivisions=2)
    add_ellipsoid("WANDERER_Waist", (0.0, 0.10, body_z - 0.06),
                  (0.38, 0.62, 0.32), "skin", collection, spec, "body",
                  subdivisions=2)
    add_ellipsoid("WANDERER_Haunch", (0.0, 0.86, body_z + 0.02),
                  (0.50, 0.56, 0.42), "skin", collection, spec, "haunch",
                  subdivisions=2)
    add_ellipsoid("WANDERER_KeelChest", (0.0, -0.78, body_z + 0.06),
                  (0.48, 0.60, 0.56), "hide", collection, spec, "chest",
                  subdivisions=2)
    for name, sign, longitudinal, offset in (
        ("fl", -1.0, -0.76, 0.0), ("fr", 1.0, -0.76, 0.5),
        ("hl", -1.0, 0.88, 0.5), ("hr", 1.0, 0.88, 0.0),
    ):
        leg_cycle = math.sin((phase + offset) * math.tau)
        root = Vector((0.40 * sign, longitudinal, body_z - 0.08))
        travel = leg_cycle * 0.38 if moving else 0.0
        lift = max(0.0, leg_cycle) * 0.24 if moving else 0.0
        hoof = Vector((0.58 * sign, longitudinal - travel,
                       0.13 + lift if not resting else 0.15))
        add_dragon_leg(spec, collection, name, root, hoof, 0.135,
                       (0.27, 0.50, 0.12), claw_count=3,
                       bend=-0.24 if longitudinal < 0.0 else 0.24)

    neck = (
        Vector((0.0, -0.88, body_z + 0.26)),
        Vector((0.0, -1.30, body_z + 0.64)),
        Vector((0.0, -1.78, body_z + (1.05 if threat else 0.88))),
        Vector((0.0, -2.28, body_z + (1.26 if threat else 0.96))),
        Vector((0.0, -2.74, body_z + (1.17 if threat else 0.85))),
    )
    add_dragon_neck(spec, collection, neck, (0.27, 0.22, 0.18, 0.14, 0.11))
    head = neck[-1] + Vector((0.0, -0.20, 0.0))
    add_ellipsoid("WANDERER_NarrowHead", head, (0.29, 0.58, 0.24),
                  "skin", collection, spec, "head", subdivisions=2)
    add_box("WANDERER_BeakJaw",
            head + Vector((0.0, -0.46, -0.12 if not threat else -0.20)),
            (0.40, 0.62, 0.13), "secondary", collection, spec, "jaw",
            rotation=(0.12 if threat else 0.0, 0.0, 0.0), bevel=0.018)
    for side, sign in (("L", -1.0), ("R", 1.0)):
        add_ellipsoid(f"WANDERER_Eye_{side}",
                      head + Vector((0.21 * sign, -0.40, 0.09)),
                      (0.052, 0.028, 0.062), "eye", collection, spec,
                      f"eye_{side.lower()}")
        add_ellipsoid(f"WANDERER_BrowRidge_{side}",
                      head + Vector((0.19 * sign, -0.28, 0.17)),
                      (0.075, 0.040, 0.075), "secondary", collection, spec,
                      f"eye_{side.lower()}")
        add_cone(f"WANDERER_SweptHorn_{side}",
                 head + Vector((0.18 * sign, 0.02, 0.22)),
                 0.075, 0.72, "horn", collection, spec,
                 f"swept_horn_{side.lower()}",
                 rotation=(-0.65, math.pi * 0.18 * sign, 0.0), vertices=6)

    add_dragon_tail(
        spec, collection,
        (Vector((0.0, 1.30, body_z)), Vector((0.0, 2.12, body_z - 0.08)),
         Vector((0.30 * cycle, 2.98, body_z - 0.24)),
         Vector((0.55 * cycle, 3.88, body_z - 0.42)),
         Vector((0.70 * cycle, 4.72, body_z - 0.48))),
        (0.25, 0.19, 0.12, 0.065, 0.018))
    wing_height = body_z + (2.65 if threat else 1.45)
    wing_reach = 3.45 if threat else 2.75
    for side, sign in (("L", -1.0), ("R", 1.0)):
        shoulder = Vector((0.37 * sign, -0.56, body_z + 0.38))
        elbow = Vector((1.62 * sign, -0.18, wing_height))
        wrist = Vector((2.35 * sign, 0.55,
                        body_z + (0.98 if threat else 0.62)))
        fingers = (
            Vector((wing_reach * sign, 1.12, body_z + 0.92)),
            Vector((1.72 * sign, 1.88, body_z + 0.18)),
        )
        trailing_anchor = Vector((0.52 * sign, 0.72, body_z + 0.20))
        add_dragon_wing(spec, collection, side, shoulder, elbow, wrist,
                       fingers, trailing_anchor, 0.115, 0.062, 0.048,
                       fold_amount=0.32, fold_depth=0.14)
    for spine, y in enumerate((-1.15, -0.30, 0.60)):
        add_cone(f"WANDERER_BackBlade_{spine}",
                 (0.0, y, body_z + 0.48), 0.075, 0.34, "horn",
                 collection, spec, f"back_blade_{spine}", vertices=5)


def build_dragon_crowned(spec: CreatureSpec,
                          collection: bpy.types.Collection) -> None:
    phase = pose_phase(spec.pose)
    moving = spec.pose in ("stalk_a", "stalk_b")
    resting = spec.pose == "rest"
    threat = spec.pose == "threat"
    cycle = math.sin(phase * math.tau)
    body_z = 0.72 if resting else 1.05
    torso_points = (
        Vector((0.0, -1.65, body_z + 0.10)),
        Vector((0.12 * cycle, -0.30, body_z)),
        Vector((-0.18 * cycle, 1.30, body_z - 0.08)),
        Vector((0.10 * cycle, 2.65, body_z - 0.14)),
    )
    # Chest, then a cinched waist, then a haunch flare before the tail
    # taper: a deliberate mass contour instead of a near-uniform tube.
    torso_radii_start = (0.76, 0.50, 0.68)
    torso_radii_end = (0.50, 0.68, 0.42)
    for index in range(len(torso_points) - 1):
        add_segment(f"CROWNED_SerpentineTorso_{index}",
                    torso_points[index], torso_points[index + 1],
                    torso_radii_start[index], torso_radii_end[index],
                    "skin", collection, spec, f"serpentine_torso_{index}",
                    sides=12)
    add_ellipsoid("CROWNED_Ribcage", (0.0, -1.05, body_z + 0.08),
                  (0.88, 1.05, 0.78), "hide", collection, spec, "chest",
                  subdivisions=2)
    add_ellipsoid("CROWNED_Shoulder_L", (-0.62, -1.35, body_z + 0.20),
                  (0.42, 0.46, 0.44), "skin", collection, spec,
                  "shoulder_left", subdivisions=1)
    add_ellipsoid("CROWNED_Shoulder_R", (0.62, -1.35, body_z + 0.20),
                  (0.42, 0.46, 0.44), "skin", collection, spec,
                  "shoulder_right", subdivisions=1)
    add_ellipsoid("CROWNED_Haunch_L", (-0.64, 1.55, body_z + 0.02),
                  (0.44, 0.54, 0.48), "skin", collection, spec,
                  "haunch_left", subdivisions=1)
    add_ellipsoid("CROWNED_Haunch_R", (0.64, 1.55, body_z + 0.02),
                  (0.44, 0.54, 0.48), "skin", collection, spec,
                  "haunch_right", subdivisions=1)
    for name, sign, longitudinal, offset in (
        ("fl", -1.0, -1.35, 0.0), ("fr", 1.0, -1.35, 0.5),
        ("hl", -1.0, 1.72, 0.5), ("hr", 1.0, 1.72, 0.0),
    ):
        leg_cycle = math.sin((phase + offset) * math.tau)
        root = Vector((0.54 * sign, longitudinal, body_z - 0.12))
        travel = leg_cycle * 0.34 if moving else 0.0
        lift = max(0.0, leg_cycle) * 0.14 if moving else 0.0
        hoof = Vector((0.78 * sign, longitudinal - travel,
                       0.14 + lift if not resting else 0.15))
        add_dragon_leg(spec, collection, name, root, hoof, 0.17,
                       (0.32, 0.56, 0.14), claw_count=3,
                       bend=-0.22 if longitudinal < 0.0 else 0.22)

    neck = (
        Vector((0.0, -1.62, body_z + 0.22)),
        Vector((-0.12, -2.38, body_z + (0.68 if threat else 0.38))),
        Vector((0.16, -3.20, body_z + (1.06 if threat else 0.52))),
        Vector((-0.12, -4.08, body_z + (1.30 if threat else 0.58))),
        Vector((0.0, -4.92, body_z + (1.18 if threat else 0.46))),
    )
    add_dragon_neck(spec, collection, neck,
                    (0.50, 0.43, 0.35, 0.27, 0.18))
    head = neck[-1] + Vector((0.0, -0.36, 0.0))
    add_ellipsoid("CROWNED_Head", head, (0.44, 0.76, 0.34), "skin",
                  collection, spec, "head", subdivisions=2)
    add_ellipsoid("CROWNED_JawHinge",
                  head + Vector((0.0, -0.42, -0.04)),
                  (0.42, 0.32, 0.32), "secondary", collection, spec,
                  "jaw", subdivisions=1)
    add_box("CROWNED_Jaw",
            head + Vector((0.0, -0.56, -0.16 if not threat else -0.27)),
            (0.56, 0.78, 0.16), "secondary", collection, spec, "jaw",
            rotation=(0.12 if threat else 0.0, 0.0, 0.0), bevel=0.028)
    for side, sign in (("L", -1.0), ("R", 1.0)):
        add_ellipsoid(f"CROWNED_Eye_{side}",
                      head + Vector((0.29 * sign, -0.48, 0.10)),
                      (0.066, 0.032, 0.078), "eye", collection, spec,
                      f"eye_{side.lower()}")
        add_ellipsoid(f"CROWNED_BrowRidge_{side}",
                      head + Vector((0.27 * sign, -0.34, 0.22)),
                      (0.095, 0.050, 0.090), "secondary", collection, spec,
                      f"eye_{side.lower()}")
        add_cone(f"CROWNED_Horn_{side}",
                 head + Vector((0.24 * sign, 0.05, 0.29)),
                 0.105, 0.88, "horn", collection, spec,
                 f"horn_{side.lower()}",
                 rotation=(0.22, math.pi * 0.31 * sign, 0.0), vertices=7)
        add_cone(f"CROWNED_BrowHorn_{side}",
                 head + Vector((0.31 * sign, -0.22, 0.18)),
                 0.064, 0.44, "horn", collection, spec,
                 f"brow_horn_{side.lower()}",
                 rotation=(0.46, math.pi * 0.40 * sign, 0.0), vertices=6)

    add_dragon_tail(
        spec, collection,
        (Vector((0.0, 2.52, body_z - 0.10)),
         Vector((0.20, 3.68, body_z - 0.18)),
         Vector((-0.30 + 0.18 * cycle, 4.90, body_z - 0.28)),
         Vector((-0.56 + 0.34 * cycle, 6.20, body_z - 0.38)),
         Vector((0.18 + 0.48 * cycle, 7.48, body_z - 0.42)),
         Vector((0.50 + 0.58 * cycle, 8.68, body_z - 0.35)),
         Vector((0.18 + 0.62 * cycle, 9.76, body_z - 0.22))),
        (0.48, 0.42, 0.34, 0.25, 0.16, 0.085, 0.022))
    wing_height = body_z + (3.00 if threat else 1.18)
    wing_reach = 3.90 if threat else 2.45
    for side, sign in (("L", -1.0), ("R", 1.0)):
        shoulder = Vector((0.58 * sign, -1.18, body_z + 0.48))
        elbow = Vector((1.82 * sign, -0.36, wing_height))
        wrist = Vector((2.68 * sign, 0.42,
                        body_z + (1.05 if threat else 0.55)))
        fingers = (
            Vector((wing_reach * sign, 1.08,
                    body_z + (0.92 if threat else 0.34))),
            Vector((2.55 * sign, 1.65, body_z + 0.24)),
            Vector((2.05 * sign, 2.12, body_z + 0.18)),
        )
        trailing_anchor = Vector((0.72 * sign, 0.58, body_z + 0.22))
        add_dragon_wing(spec, collection, side, shoulder, elbow, wrist,
                       fingers, trailing_anchor, 0.155, 0.088, 0.062,
                       fold_amount=0.34, fold_depth=0.18)
    for spine, position in enumerate((
        (0.0, -4.18, body_z + 0.98), (0.0, -3.24, body_z + 0.96),
        (0.0, -2.28, body_z + 0.88), (0.0, -1.28, body_z + 0.78),
        (0.0, -0.20, body_z + 0.70), (0.0, 0.92, body_z + 0.60),
        (0.0, 2.04, body_z + 0.45), (0.0, 3.22, body_z + 0.28),
    )):
        add_cone(f"CROWNED_Spine_{spine}", position, 0.125,
                 max(0.22, 0.46 - spine * 0.038), "horn", collection,
                 spec, f"spine_{spine}", vertices=6)


def build_dragon_deep_wyrm(spec: CreatureSpec,
                            collection: bpy.types.Collection) -> None:
    phase = pose_phase(spec.pose)
    moving = spec.pose in ("stalk_a", "stalk_b")
    resting = spec.pose == "rest"
    threat = spec.pose == "threat"
    cycle = math.sin(phase * math.tau)
    body_z = 0.50 if resting else 0.78
    slither = cycle * (0.28 if moving else 0.10)
    serpent = (
        Vector((0.0, -3.72, body_z + 0.38)),
        Vector((0.34, -2.10, body_z + 0.20)),
        Vector((-0.46, -0.30, body_z + 0.08)),
        Vector((-0.72 - slither, 1.72, body_z)),
        Vector((0.16 - slither, 3.86, body_z - 0.08)),
        Vector((0.88 + slither, 6.04, body_z - 0.16)),
        Vector((0.28 + slither, 8.22, body_z - 0.24)),
        Vector((-0.72, 10.34, body_z - 0.30)),
        Vector((-0.46, 12.26, body_z - 0.34)),
        Vector((0.12, 13.96, body_z - 0.36)),
        Vector((0.34, 15.30, body_z - 0.36)),
    )
    serpent_radii = (0.82, 0.88, 0.84, 0.76, 0.66, 0.54,
                     0.42, 0.30, 0.20, 0.11, 0.025)
    for index in range(len(serpent) - 1):
        add_segment(f"ANCIENT_SerpentBody_{index}",
                    serpent[index], serpent[index + 1],
                    serpent_radii[index], serpent_radii[index + 1],
                    "skin" if index % 3 else "hide", collection, spec,
                    f"serpent_body_{index}", sides=12)
    # Even a legless, serpentine elder still needs a deep chest: a
    # distinct mass bulge right behind the neck, not just a smooth taper.
    add_ellipsoid("ANCIENT_ChestMass", serpent[1] + Vector((0.0, 0.0, 0.12)),
                  (1.08, 1.02, 0.96), "hide", collection, spec, "chest",
                  subdivisions=2)

    neck = (
        serpent[0],
        Vector((-0.22, -4.62, body_z + (1.00 if threat else 0.62))),
        Vector((0.18, -5.52, body_z + (1.82 if threat else 0.86))),
        Vector((0.0, -6.34, body_z + (2.18 if threat else 0.78))),
    )
    add_dragon_neck(spec, collection, neck, (0.82, 0.65, 0.46, 0.27))
    head = neck[-1] + Vector((0.0, -0.48, 0.0))
    add_ellipsoid("ANCIENT_NarrowSkull", head, (0.62, 1.05, 0.44), "skin",
                  collection, spec, "head", subdivisions=2)
    add_box("ANCIENT_LongJaw",
            head + Vector((0.0, -0.82, -0.24 if not threat else -0.40)),
            (0.84, 1.18, 0.24), "secondary", collection, spec, "jaw",
            rotation=(0.15 if threat else 0.0, 0.0, 0.0), bevel=0.055)
    for side, sign in (("L", -1.0), ("R", 1.0)):
        add_ellipsoid(f"ANCIENT_Eye_{side}",
                      head + Vector((0.44 * sign, -0.68, 0.14)),
                      (0.085, 0.040, 0.100), "eye", collection, spec,
                      f"eye_{side.lower()}")
        add_ellipsoid(f"ANCIENT_BrowRidge_{side}",
                      head + Vector((0.40 * sign, -0.50, 0.30)),
                      (0.13, 0.065, 0.12), "secondary", collection, spec,
                      f"eye_{side.lower()}")
        for crown, (x, y, z, length, angle) in enumerate((
            (0.26, 0.08, 0.38, 1.55, 0.12),
            (0.42, -0.08, 0.25, 1.12, 0.32),
            (0.48, -0.40, 0.10, 0.72, 0.52),
        )):
            add_cone(f"ANCIENT_Crown_{side}_{crown}",
                     head + Vector((x * sign, y, z)),
                     0.13 - crown * 0.022, length, "horn", collection,
                     spec, f"crown_{side.lower()}_{crown}",
                     rotation=(angle, math.pi * (0.20 + crown * 0.09) * sign,
                               0.0), vertices=8)
        whisker_root = head + Vector((0.38 * sign, -0.72, -0.02))
        whisker_tip = head + Vector((1.48 * sign, -1.72, -0.10))
        add_segment(f"ANCIENT_Whisker_{side}", whisker_root, whisker_tip,
                    0.035, 0.008, "horn", collection, spec,
                    f"whisker_{side.lower()}", sides=6)

        arm_root = Vector((0.58 * sign, -3.20, body_z + 0.18))
        arm_tip = Vector((1.06 * sign, -3.62, body_z - 0.32))
        add_segment(f"ANCIENT_VestigialArm_{side}", arm_root, arm_tip,
                    0.13, 0.065, "skin", collection, spec,
                    f"vestigial_arm_{side.lower()}", sides=7)
        for claw in range(2):
            add_cone(f"ANCIENT_Claw_{side}_{claw}",
                     arm_tip + Vector(((claw * 2 - 1) * 0.06 * sign,
                                      -0.08, -0.02)),
                     0.035, 0.24, "horn", collection, spec,
                     f"vestigial_claw_{side.lower()}_{claw}",
                     rotation=(math.pi * 0.55, 0.0, 0.0), vertices=5)

    wing_height = body_z + (4.40 if threat else 1.72)
    wing_reach = 6.20 if threat else 4.25
    for side, sign in (("L", -1.0), ("R", 1.0)):
        shoulder = Vector((0.72 * sign, -2.38, body_z + 0.74))
        elbow = Vector((2.46 * sign, -1.12, wing_height))
        wrist = Vector((3.65 * sign, -0.10,
                        body_z + (1.55 if threat else 0.95)))
        # Ancient, weathered "broken sails": the middle digit reads
        # shorter than its neighbours, like a tear that healed over.
        fingers = (
            Vector((wing_reach * sign, 0.42, body_z + 1.02)),
            Vector((3.55 * sign, 1.35, body_z + 0.55)),
            Vector((2.58 * sign, 2.40, body_z + 0.12)),
        )
        trailing_anchor = Vector((1.38 * sign, 0.82, body_z + 0.50))
        add_dragon_wing(spec, collection, side, shoulder, elbow, wrist,
                       fingers, trailing_anchor, 0.225, 0.115, 0.088,
                       fold_amount=0.30, fold_depth=0.22)
    for plate, point in enumerate(serpent[:-1]):
        add_cone(f"ANCIENT_Spine_{plate}",
                 (point.x, point.y, point.z + serpent_radii[plate] * 0.82),
                 max(0.055, 0.16 - plate * 0.009),
                 max(0.20, 0.62 - plate * 0.032), "horn", collection,
                 spec, f"serpent_spine_{plate}", vertices=7)


def build_dragon(spec: CreatureSpec,
                 collection: bpy.types.Collection) -> None:
    builders = {
        "dragon_whelp": build_dragon_whelp,
        "dragon_wanderer": build_dragon_wanderer,
        "dragon": build_dragon_crowned,
        "dragon_deep_wyrm": build_dragon_deep_wyrm,
    }
    builders[spec.variant](spec, collection)
    stage_size = {
        "dragon_whelp": 1.00,
        "dragon_wanderer": 1.90,
        "dragon": 2.50,
        "dragon_deep_wyrm": 3.90,
    }[spec.variant]
    authored_size = {
        "dragon_whelp": 0.36,
        "dragon_wanderer": 2.31,
        "dragon": 6.36,
        "dragon_deep_wyrm": 13.92,
    }[spec.variant]
    for obj in collection.objects:
        if obj.type == "MESH":
            obj.location *= stage_size
            obj.scale *= stage_size
            obj["cc_dragon_authored_size"] = authored_size


def build_creature(spec: CreatureSpec,
                   collection: bpy.types.Collection) -> None:
    if spec.family == "goblin":
        build_goblin(spec, collection)
    elif spec.family in ("horse", "cow", "sheep"):
        build_quadruped(spec, collection)
    elif spec.family == "dragon":
        build_dragon(spec, collection)
    else:
        raise ValueError(f"unsupported creature family {spec.family}")


def duplicate_preview_parts(collection: bpy.types.Collection,
                            spec: CreatureSpec) -> list[bpy.types.Object]:
    result: list[bpy.types.Object] = []
    for source in tuple(collection.objects):
        if source.type != "MESH":
            continue
        duplicate = source.copy()
        duplicate.data = source.data.copy()
        duplicate.name = f"PREVIEW_SOURCE_{spec.variant}_{source.name}"
        bpy.context.scene.collection.objects.link(duplicate)
        duplicate.hide_render = True
        duplicate.hide_set(True)
        result.append(duplicate)
    return result


def quadruped_bone_points(
    spec: CreatureSpec,
) -> dict[str, tuple[Vector, Vector]]:
    cow = spec.family == "cow"
    sheep = spec.family == "sheep"
    pony = spec.family == "horse"
    body_z = 0.76 if sheep else 1.08 if cow else 0.98
    half_width = 0.30 if sheep else 0.38 if cow else 0.34
    front_y = -0.43 if sheep else -0.57 if cow else -0.52
    hind_y = 0.43 if sheep else 0.57 if cow else 0.52
    points: dict[str, tuple[Vector, Vector]] = {
        "root": (Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, 0.20))),
        "body": (
            Vector((0.0, 0.34 if sheep else 0.42 if cow else 0.36, body_z)),
            Vector((0.0, -0.24 if sheep else -0.30 if cow else -0.28,
                    body_z)),
        ),
        "chest": (
            Vector((0.0, -0.12 if sheep else -0.16 if cow else -0.14,
                    body_z + 0.06)),
            Vector((0.0, -0.52 if sheep else -0.72 if cow else -0.60,
                    body_z + 0.06)),
        ),
    }
    if sheep:
        points["neck"] = (
            Vector((0.0, -0.36, body_z + 0.14)),
            Vector((0.0, -0.62, body_z + 0.08)),
        )
        points["head"] = (
            Vector((0.0, -0.82, body_z + 0.04)),
            Vector((0.0, -1.06, body_z - 0.02)),
        )
    elif cow:
        points["neck"] = (
            Vector((0.0, -0.50, body_z + 0.20)),
            Vector((0.0, -0.91, 1.16)),
        )
        points["head"] = (
            Vector((0.0, -1.15, 1.10)),
            Vector((0.0, -1.46, 1.04)),
        )
    elif pony:
        points["neck"] = (
            Vector((0.0, -0.42, body_z + 0.22)),
            Vector((0.0, -0.70, body_z + 0.43)),
        )
        points["head"] = (
            Vector((0.0, -0.93, body_z + 0.43)),
            Vector((0.0, -1.18, body_z + 0.35)),
        )

    for name, x, longitudinal, front in (
        ("FL", -half_width, front_y, True),
        ("FR", half_width, front_y, True),
        ("HL", -half_width, hind_y, False),
        ("HR", half_width, hind_y, False),
    ):
        root = Vector((x, longitudinal,
                       body_z - (0.08 if front else 0.10)))
        hoof = Vector((x, longitudinal, 0.10))
        knee = (root + hoof) * 0.5
        knee += Vector((0.0, -0.10 if front else 0.10, 0.02))
        hoof_tail = Vector((x, longitudinal - 0.20, 0.085))
        points[f"upper_leg.{name}"] = (root, knee)
        points[f"lower_leg.{name}"] = (knee, hoof)
        points[f"hoof.{name}"] = (hoof, hoof_tail)

    if sheep:
        tail_base = Vector((0.0, 0.60, body_z + 0.14))
        tail_mid = Vector((0.0, 0.75, body_z + 0.10))
        tail_end = Vector((0.0, 0.86, body_z - 0.02))
    elif pony:
        tail_base = Vector((0.0, 0.68, body_z + 0.12))
        tail_mid = Vector((0.0, 0.94, body_z - 0.02))
        tail_end = Vector((0.0, 1.17, body_z - 0.48))
    else:
        tail_base = Vector((0.0, 0.82, body_z + 0.08))
        tail_mid = Vector((0.0, 1.10, body_z - 0.08))
        tail_end = Vector((0.0, 1.28,
                           body_z - (0.62 if cow else 0.48)))
    points["tail.root"] = (tail_base, tail_mid)
    points["tail"] = (tail_mid, tail_end)
    return points


def quadruped_bone_for_part(part: str) -> str:
    direct = {
        "barrel": "body",
        "udder": "body",
        "teat": "body",
        "hide_patch": "chest",
        "chest": "chest",
        "neck": "neck",
        "mane": "neck",
        "head": "head",
        "muzzle": "head",
        "ear_l": "head",
        "ear_r": "head",
        "eye_l": "head",
        "eye_r": "head",
        "horn_l": "head",
        "horn_r": "head",
        "tail_root": "tail.root",
        "tail": "tail",
        "tail_flow": "tail.root",
        "bridle": "head",
        "collar_crest": "neck",
        "collar": "chest",
        "saddle_pad": "body",
    }
    if part in direct:
        return direct[part]
    for prefix, bone_prefix in (
        ("upper_leg_", "upper_leg."),
        ("lower_leg_", "lower_leg."),
        ("hoof_", "hoof."),
    ):
        if part.startswith(prefix):
            return bone_prefix + part.removeprefix(prefix).upper()
    raise RuntimeError(f"quadruped part {part!r} has no deform bone")


def apply_modifiers(obj: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for modifier in tuple(obj.modifiers):
        if modifier.type == "ARMATURE":
            continue
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def skin_quadruped(collection: bpy.types.Collection,
                    spec: CreatureSpec) -> bpy.types.Object:
    armature_data = bpy.data.armatures.new(f"RIG_{spec.variant}")
    rig = bpy.data.objects.new(f"RIG_{spec.variant}", armature_data)
    collection.objects.link(rig)
    rig.show_in_front = True
    rig["cc_asset_id"] = spec.asset_id
    rig["cc_rig_contract"] = "CcQuadrupedPose"
    rig["cc_bone_count"] = len(QUADRUPED_BONES)

    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    points = quadruped_bone_points(spec)
    edit_bones: dict[str, bpy.types.EditBone] = {}
    for name, _parent in QUADRUPED_BONES:
        bone = armature_data.edit_bones.new(name)
        bone.head, bone.tail = points[name]
        bone.use_deform = True
        edit_bones[name] = bone
    for name, parent in QUADRUPED_BONES:
        if parent is not None:
            edit_bones[name].parent = edit_bones[parent]
            edit_bones[name].use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.select_set(False)

    for obj in tuple(collection.objects):
        if obj.type != "MESH":
            continue
        apply_modifiers(obj)
        bone_name = quadruped_bone_for_part(str(obj["cc_part"]))
        obj.vertex_groups.clear()
        group = obj.vertex_groups.new(name=bone_name)
        group.add(tuple(range(len(obj.data.vertices))), 1.0, "REPLACE")
        if obj.get("cc_part") == "tail_flow":
            tip_group = obj.vertex_groups.new(name="tail")
            for vertex in obj.data.vertices:
                longitudinal = (obj.matrix_world @ vertex.co).y
                weight = max(0.0, min(1.0, (longitudinal - 0.80) / 0.30))
                weight = weight * weight * (3.0 - 2.0 * weight)
                group.add((vertex.index,), 1.0 - weight, "REPLACE")
                tip_group.add((vertex.index,), weight, "REPLACE")
        modifier = obj.modifiers.new("CC_QuadrupedSkin", "ARMATURE")
        modifier.object = rig
        world = obj.matrix_world.copy()
        obj.parent = rig
        obj.matrix_parent_inverse = rig.matrix_world.inverted()
        obj.matrix_world = world
        obj["cc_deform_bone"] = bone_name
    return rig


def consolidate(collection: bpy.types.Collection,
                spec: CreatureSpec,
                rig: bpy.types.Object | None = None) -> bpy.types.Object:
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
    if not objects:
        raise RuntimeError(f"{spec.variant} generated no meshes")
    for obj in objects:
        apply_modifiers(obj)
    # A part may pin its paint value (pupils, mouth lines stay dark). Carry
    # the pin through the join as a face attribute; 0 means "use normals".
    pinned_values = any("cc_paint_value" in obj for obj in objects)
    if pinned_values:
        for obj in objects:
            attribute = obj.data.attributes.new(
                "cc_paint_value", "FLOAT", "FACE")
            value = float(obj.get("cc_paint_value", 0.0))
            for index in range(len(obj.data.polygons)):
                attribute.data[index].value = value
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    joined = objects[0]
    value_overrides = None
    if pinned_values:
        attribute = joined.data.attributes["cc_paint_value"]
        value_overrides = [item.value for item in attribute.data]
        joined.data.attributes.remove(attribute)
    old_materials = list(joined.data.materials)
    old_names = [material.name if material else "" for material in old_materials]
    canonical: dict[str, int] = {}
    for family in FAMILY_PALETTES:
        for index, semantic in enumerate(MATERIAL_ORDER):
            canonical[PREVIEW_MATERIALS[(family, semantic)].name] = index
    polygon_materials = [canonical[old_names[polygon.material_index]]
                         for polygon in joined.data.polygons]
    paint_channels.add_indexed_paint_channels(
        joined, polygon_materials, MATERIAL_ORDER,
        value_offset=GOBLIN_VALUE_LIFT if spec.family == "goblin" else 0.0,
        value_overrides=value_overrides)
    joined.data.materials.clear()
    if INDEXED_MATERIAL is None:
        raise RuntimeError("indexed creature material was not initialized")
    joined.data.materials.append(INDEXED_MATERIAL)
    for polygon in joined.data.polygons:
        polygon.material_index = 0
    joined.name = f"GEO_{spec.asset_id}"
    joined.data.name = joined.name
    tag(joined, spec, "assembled_creature")
    joined["cc_material_contract"] = "COLOR_0:palette,value,fold"
    if rig is not None:
        joined.parent = rig
        armatures = [modifier for modifier in joined.modifiers
                     if modifier.type == "ARMATURE"]
        if not armatures:
            armature = joined.modifiers.new("CC_QuadrupedSkin", "ARMATURE")
            armature.object = rig
        else:
            armatures[0].object = rig
            for redundant in armatures[1:]:
                joined.modifiers.remove(redundant)
        joined["cc_skin_contract"] = str(
            rig.get("cc_rig_contract", "CcQuadrupedPose"))
    return joined


def export_model(model: bpy.types.Object, spec: CreatureSpec,
                 rig: bpy.types.Object | None = None) -> Path:
    path = EXPORT_DIR / f"{spec.asset_id}.glb"
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    model.hide_set(False)
    model.select_set(True)
    if rig is not None:
        rig.hide_set(False)
        rig.select_set(True)
        bpy.context.view_layer.objects.active = rig
    else:
        bpy.context.view_layer.objects.active = model
    bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_animations=False,
        export_skins=rig is not None,
        export_morph=False,
        export_extras=True,
        export_materials="EXPORT",
    )
    model.select_set(False)
    if rig is not None:
        rig.select_set(False)
    return path


def look_at(obj: bpy.types.Object,
            target: tuple[float, float, float]) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_stage(collection: bpy.types.Collection, z: float,
              row_name: str) -> None:
    material = bpy.data.materials.new("MAT_CREATURE_STAGE")
    material.diffuse_color = (0.060, 0.085, 0.080, 1.0)
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = material.diffuse_color
    principled.inputs["Roughness"].default_value = 0.92
    bpy.ops.mesh.primitive_cube_add(location=(0.0, 0.45, z - 0.10))
    stage = bpy.context.object
    stage.name = f"STAGE_CreatureFamilySheet_{row_name}"
    stage.dimensions = (112.0, 16.0, 0.16)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    stage.data.materials.append(material)
    move_to(stage, collection)


def add_preview_scene(
    preview_sources: list[tuple[CreatureSpec, list[bpy.types.Object]]],
) -> None:
    preview = new_collection("90_PREVIEW")
    placements = {


        "horse": (-12.00, 0.20, 9.0, 2.4),
        "cow": (2.00, 0.40, 9.0, 2.4),
        "sheep": (14.00, 0.62, 9.0, 2.4),

        "dragon_whelp": (-43.00, 0.75, 0.0, 0.35),
        "dragon_wanderer": (-36.50, 0.95, 0.0, 0.35),
        "dragon": (-13.50, 1.20, 0.0, 0.35),
        "dragon_deep_wyrm": (5.00, 1.50, 0.0, 0.35),
    }
    for spec, sources in preview_sources:
        if spec.family == "goblin":
            for source in sources:
                source.hide_render = True
                source.hide_set(True)
            continue
        x, y, z, scale = placements[spec.variant]
        for source in sources:
            source.hide_render = True
            source.hide_set(True)
            duplicate = source.copy()
            duplicate.data = source.data
            duplicate.name = f"PREVIEW_{spec.variant}_{source.name}"
            preview.objects.link(duplicate)
            duplicate.hide_render = False
            duplicate.hide_viewport = False
            duplicate.hide_set(False)
            stage_scale = Matrix.Diagonal((scale, scale, scale, 1.0))
            preview_turn = (
                math.pi * 0.5 if spec.family in ("dragon", "horse") else
                math.pi * 0.22 if spec.family in ("cow", "sheep") else
                0.0)
            side_turn = Matrix.Rotation(preview_turn, 4, "Z")
            duplicate.matrix_world = (
                Matrix.Translation(Vector((x, y, z))) @
                side_turn @ stage_scale @ source.matrix_world)
    add_stage(preview, 0.0, "Dragons")
    add_stage(preview, 9.0, "Characters")

    bpy.ops.object.camera_add(location=(0.0, -75.0, 22.0))
    camera = bpy.context.object
    camera.name = "CAM_CreatureFamilySheet"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 45.0
    look_at(camera, (0.0, 0.55, 5.10))
    bpy.context.scene.camera = camera
    move_to(camera, preview)

    bpy.ops.object.light_add(type="SUN", location=(-18.0, -16.0, 24.0))
    sun = bpy.context.object
    sun.name = "LIGHT_Sun"
    sun.data.energy = 2.7
    sun.data.angle = math.radians(18.0)
    sun.rotation_euler = (math.radians(28.0), math.radians(-18.0),
                          math.radians(-25.0))
    move_to(sun, preview)

    bpy.ops.object.light_add(type="AREA", location=(-10.0, -14.0, 21.0))
    key = bpy.context.object
    key.name = "LIGHT_Key"
    key.data.energy = 4200.0
    key.data.shape = "DISK"
    key.data.size = 32.0
    key.data.color = (1.0, 0.77, 0.59)
    look_at(key, (0.0, 0.5, 4.5))
    move_to(key, preview)

    bpy.ops.object.light_add(type="AREA", location=(18.0, -8.0, 18.0))
    fill = bpy.context.object
    fill.name = "LIGHT_Fill"
    fill.data.energy = 3000.0
    fill.data.size = 30.0
    fill.data.color = (0.43, 0.75, 1.0)
    look_at(fill, (0.0, 0.8, 4.5))
    move_to(fill, preview)

    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.scene.render.filepath = str(PREVIEW_PATH)
    bpy.ops.render.render(write_still=True)


def build() -> None:
    reset_scene()
    make_materials()
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    preview_sources: list[tuple[CreatureSpec, list[bpy.types.Object]]] = []
    manifest_entries: list[dict[str, object]] = []
    for base_spec in CREATURES:
        for pose in poses_for(base_spec):
            spec = replace(base_spec, pose=pose)
            collection = new_collection(
                f"CREATURE_{spec.variant.upper()}_{pose.upper()}")
            collection["cc_asset_id"] = spec.asset_id
            collection["cc_family"] = spec.family
            collection["cc_variant"] = spec.variant
            collection["cc_variant_index"] = spec.variant_index
            collection["cc_pose"] = pose
            collection["cc_runtime_morphology"] = spec.runtime_morphology
            collection["cc_library_version"] = LIBRARY_VERSION
            build_creature(spec, collection)
            if pose == "idle":
                preview_sources.append(
                    (spec, duplicate_preview_parts(collection, spec)))
            skeleton = skeleton_for(spec)
            if skeleton == "quadruped":
                rig = skin_quadruped(collection, spec)
            elif skeleton == "humanoid":
                rig = skin_humanoid(
                    collection, spec,
                    humanoid_bone_points(goblin_joints(spec)))
            else:
                rig = None
            model = consolidate(collection, spec, rig)
            path = export_model(model, spec, rig)
            model.hide_render = True
            model.hide_set(True)
            entry: dict[str, object] = {
                **asdict(spec),
                "id": spec.asset_id,
                "export": str(path.relative_to(ROOT)),
                "material_order": list(MATERIAL_ORDER),
            }
            if rig is not None:
                rig.hide_render = True
                rig.hide_set(True)
                bones = HUMANOID_BONES if skeleton == "humanoid" else \
                    QUADRUPED_BONES
                entry["skinned"] = True
                entry["armature"] = rig.name
                entry["bones"] = [name for name, _parent in bones]
            entry["skeleton"] = skeleton
            manifest_entries.append(entry)

    referenced = {ROOT / str(entry["export"]) for entry in manifest_entries}
    for stale in EXPORT_DIR.glob("*.glb"):
        if stale not in referenced:
            stale.unlink()

    manifest = {
        "library_version": LIBRARY_VERSION,
        "art_direction": "silhouette_first_pseudo_pixel_creatures",
        "generation": "offline_curated_procedural_geometry",
        "runtime_strategy": "one skinned GLB per character posed at runtime (goblins: humanoid skeleton; horse, cow, sheep: quadruped skeleton); dragons still use held poses until their migration (docs/design/unified-characters.md)",
        "coordinate_system": "glTF +Y up, +Z forward",
        "material_contract": "single indexed material; COLOR_0 stores palette, value, and fold",
        "material_order": list(MATERIAL_ORDER),
        "families": ["goblin", "dragon", "animal"],
        "variants": [spec.variant for spec in CREATURES],
        "archetypes": manifest_entries,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8")
    from generate_creature_catalog import generate
    generate(MANIFEST_PATH)
    add_preview_scene(preview_sources)
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    print(f"built {len(CREATURES)} creature variants / "
          f"{len(manifest_entries)} pose assets")
    print(f"manifest: {MANIFEST_PATH}")
    print(f"preview: {PREVIEW_PATH}")


if __name__ == "__main__":
    build()
