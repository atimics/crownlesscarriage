#!/usr/bin/env python3
"""Render small, screen-first character prototypes for the pixel pipeline.

The production hero is intentionally not changed by this experiment.  These
proxies test how few large masses are needed for Crownless identity to survive
at roughly 60 art pixels tall.
"""

from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "out" / "character-experiments"
HIGH_PATH = OUT_DIR / "screen_first_character_v04_high.png"
LOW_PATH = OUT_DIR / "screen_first_character_v04_low.png"
BLEND_PATH = ROOT / "assets" / "blender" / "crownless_screen_first_character_experiments_v04.blend"
FRONT_PATH = OUT_DIR / "screen_first_character_v04_front.png"
THREE_QUARTER_PATH = OUT_DIR / "screen_first_character_v04_three_quarter.png"
SIDE_PATH = OUT_DIR / "screen_first_character_v04_side.png"


PALETTE = {
    "background": (0.018, 0.022, 0.026, 1.0),
    "ground": (0.105, 0.125, 0.105, 1.0),
    "skin": (0.62, 0.34, 0.18, 1.0),
    "skin_light": (0.82, 0.50, 0.27, 1.0),
    "hair": (0.075, 0.046, 0.032, 1.0),
    "eye": (0.012, 0.010, 0.009, 1.0),
    "oxblood": (0.36, 0.075, 0.060, 1.0),
    "oxblood_dark": (0.22, 0.040, 0.035, 1.0),
    "oxblood_light": (0.415, 0.098, 0.073, 1.0),
    "teal": (0.075, 0.205, 0.195, 1.0),
    "teal_dark": (0.050, 0.115, 0.110, 1.0),
    "teal_light": (0.100, 0.245, 0.220, 1.0),
    "teal_shadow": (0.038, 0.095, 0.090, 1.0),
    "dark_cloth": (0.045, 0.054, 0.052, 1.0),
    "leather": (0.20, 0.105, 0.052, 1.0),
    "gold": (0.62, 0.36, 0.065, 1.0),
}


def reset_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (
        bpy.data.meshes,
        bpy.data.curves,
        bpy.data.materials,
        bpy.data.cameras,
        bpy.data.lights,
    ):
        for datablock in tuple(datablocks):
            datablocks.remove(datablock)


def make_material(name: str, color: tuple[float, float, float, float]) -> bpy.types.Material:
    material = bpy.data.materials.new(f"MAT_{name}")
    material.diffuse_color = color
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = color
    principled.inputs["Roughness"].default_value = 0.94
    principled.inputs["Specular IOR Level"].default_value = 0.12
    return material


def add_box(
    name: str,
    location: tuple[float, float, float],
    dimensions: tuple[float, float, float],
    material: bpy.types.Material,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    return obj


def bevel_object(obj: bpy.types.Object, width: float) -> bpy.types.Object:
    modifier = obj.modifiers.new(name="LowPolyBevel", type="BEVEL")
    modifier.width = width
    modifier.segments = 1
    return obj


def add_beveled_box(
    name: str,
    location: tuple[float, float, float],
    dimensions: tuple[float, float, float],
    material: bpy.types.Material,
    bevel: float,
) -> bpy.types.Object:
    return bevel_object(add_box(name, location, dimensions, material), bevel)


def add_beam(
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    width: float,
    depth: float,
    material: bpy.types.Material,
) -> bpy.types.Object:
    start_vector = Vector(start)
    end_vector = Vector(end)
    direction = end_vector - start_vector
    obj = add_box(
        name,
        tuple((start_vector + end_vector) * 0.5),
        (width, depth, direction.length),
        material,
    )
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = direction.to_track_quat("Z", "Y")
    return obj


def add_beveled_beam(
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    width: float,
    depth: float,
    material: bpy.types.Material,
    bevel: float,
) -> bpy.types.Object:
    return bevel_object(add_beam(name, start, end, width, depth, material), bevel)


def add_prism(
    name: str,
    points: tuple[tuple[float, float], ...],
    center_x: float,
    center_y: float,
    depth: float,
    material: bpy.types.Material,
) -> bpy.types.Object:
    """Extrude a front-facing x/z polygon along y."""
    front_y = center_y - depth * 0.5
    back_y = center_y + depth * 0.5
    vertices = [
        (center_x + x, front_y, z) for x, z in points
    ] + [
        (center_x + x, back_y, z) for x, z in points
    ]
    count = len(points)
    faces = [tuple(reversed(range(count))), tuple(range(count, count * 2))]
    for index in range(count):
        next_index = (index + 1) % count
        faces.append((index, next_index, count + next_index, count + index))
    mesh = bpy.data.meshes.new(f"MESH_{name}")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def add_head(prefix: str, x: float, materials: dict[str, bpy.types.Material]) -> None:
    # Start from a large warm face, then wrap it with a few asymmetric hair
    # masses.  This is closer to the concept and avoids the helmet-like inset
    # face used by the first proxy.
    add_beveled_box(
        f"{prefix}_Head", (x, -0.015, 2.29), (0.46, 0.48, 0.48),
        materials["skin_light"], 0.055,
    )
    add_beveled_box(
        f"{prefix}_HairTop", (x - 0.035, 0.005, 2.515), (0.55, 0.50, 0.21),
        materials["hair"], 0.055,
    )
    add_beveled_box(
        f"{prefix}_HairLongSide", (x - 0.235, -0.005, 2.33), (0.15, 0.47, 0.42),
        materials["hair"], 0.045,
    )
    add_beveled_box(
        f"{prefix}_HairShortSide", (x + 0.225, 0.005, 2.40), (0.105, 0.45, 0.27),
        materials["hair"], 0.035,
    )
    add_prism(
        f"{prefix}_HairFringe",
        ((-0.24, 2.48), (-0.24, 2.38), (-0.13, 2.38), (-0.13, 2.29),
         (-0.025, 2.29), (-0.025, 2.43), (0.13, 2.43), (0.13, 2.52)),
        x, -0.285, 0.045, materials["hair"],
    )
    for side in (-1.0, 1.0):
        add_beveled_box(
            f"{prefix}_Eye_{'L' if side < 0 else 'R'}",
            (x + side * 0.082, -0.267, 2.30),
            (0.046, 0.020, 0.058),
            materials["eye"], 0.008,
        )
    add_box(f"{prefix}_Mouth", (x, -0.269, 2.195), (0.070, 0.018, 0.026), materials["skin"])
    # Keep the crown as a small broken accent sitting to one side of the hair.
    crown_x = x + 0.115
    add_box(f"{prefix}_CrownBand", (crown_x, 0.005, 2.625), (0.22, 0.17, 0.042), materials["gold"])
    for index, (offset, height) in enumerate(((-0.070, 0.080), (0.0, 0.115), (0.070, 0.075))):
        add_box(
            f"{prefix}_CrownProng_{index}",
            (crown_x + offset, 0.005, 2.655 + height * 0.5),
            (0.034, 0.17, height),
            materials["gold"],
        )


def add_body(prefix: str, x: float, materials: dict[str, bpy.types.Material]) -> None:
    add_prism(
        f"{prefix}_TunicMass",
        ((-0.37, 0.90), (-0.43, 1.73), (-0.34, 1.90), (0.34, 1.90),
         (0.43, 1.73), (0.37, 0.90), (0.09, 0.86), (-0.11, 0.89)),
        x,
        0.0,
        0.50,
        materials["teal"],
    )
    # Two broad pieces read as a wrapped scarf rather than a flat shoulder plate.
    add_prism(
        f"{prefix}_ScarfShoulderWrap",
        ((-0.55, 1.77), (-0.46, 1.95), (-0.21, 2.05), (0.0, 1.97),
         (0.21, 2.05), (0.46, 1.95), (0.55, 1.77), (0.35, 1.60),
         (0.0, 1.55), (-0.35, 1.60)),
        x,
        0.01,
        0.58,
        materials["oxblood"],
    )
    add_prism(
        f"{prefix}_ScarfFrontFold",
        ((-0.39, 1.77), (-0.25, 1.90), (0.0, 1.84), (0.25, 1.90),
         (0.39, 1.77), (0.23, 1.64), (0.0, 1.60), (-0.23, 1.64)),
        x, -0.315, 0.065, materials["oxblood_dark"],
    )

    for side in (-1.0, 1.0):
        suffix = "L" if side < 0 else "R"
        shoulder = (x + side * 0.43, 0.0, 1.76)
        elbow = (x + side * 0.47, -0.015, 1.43)
        wrist = (x + side * 0.49, -0.025, 1.16)
        add_beveled_beam(
            f"{prefix}_Sleeve_{suffix}", shoulder, elbow, 0.22, 0.30,
            materials["teal_dark"], 0.035,
        )
        add_beveled_beam(
            f"{prefix}_Forearm_{suffix}", elbow, wrist, 0.19, 0.26,
            materials["skin_light"], 0.030,
        )
        add_beveled_box(
            f"{prefix}_Hand_{suffix}",
            (x + side * 0.495, -0.035, 1.06),
            (0.22, 0.30, 0.25),
            materials["skin_light"], 0.045,
        )
        leg_x = x + side * 0.205
        add_beveled_box(
            f"{prefix}_Leg_{suffix}",
            (leg_x, 0.02 + (0.025 if side < 0 else -0.02), 0.52),
            (0.27, 0.32, 0.66),
            materials["dark_cloth"], 0.035,
        )
        add_beveled_box(
            f"{prefix}_BootCuff_{suffix}",
            (leg_x, -0.015, 0.34),
            (0.33, 0.35, 0.27),
            materials["leather"], 0.045,
        )
        add_beveled_box(
            f"{prefix}_BootFoot_{suffix}",
            (leg_x, -0.11 + (0.025 if side < 0 else 0.0), 0.14),
            (0.36, 0.50, 0.25),
            materials["leather"], 0.055,
        )


def add_belt_and_satchel(prefix: str, x: float, materials: dict[str, bpy.types.Material]) -> None:
    add_beveled_beam(
        f"{prefix}_TravelStrap", (x - 0.25, -0.305, 1.74),
        (x + 0.22, -0.305, 1.02), 0.080, 0.050, materials["leather"], 0.014,
    )
    add_beveled_box(
        f"{prefix}_Belt", (x, -0.285, 1.10), (0.68, 0.075, 0.110),
        materials["leather"], 0.020,
    )
    add_beveled_box(
        f"{prefix}_Satchel", (x + 0.43, -0.305, 1.02), (0.30, 0.18, 0.34),
        materials["leather"], 0.050,
    )


def add_cape(prefix: str, x: float, materials: dict[str, bpy.types.Material]) -> None:
    # The cape is one broad asymmetrical silhouette shape behind the torso.
    add_prism(
        f"{prefix}_CapeMass",
        ((0.08, 1.92), (0.52, 1.88), (0.70, 1.67), (0.74, 0.96),
         (0.58, 0.64), (0.30, 0.76), (0.26, 1.53)),
        x,
        0.18,
        0.16,
        materials["oxblood"],
    )


def add_painted_definition(
    prefix: str,
    x: float,
    materials: dict[str, bpy.types.Material],
    accent_level: int,
) -> None:
    """Attach a few broad, local paint shapes to the model surface.

    These stand in for production vertex paint.  They move with the character
    and deliberately describe value grouping that normal-based lighting cannot
    infer, such as the reference's diagonal tunic wedge and scarf lip.
    """
    add_prism(
        f"{prefix}_Paint_TunicLightWedge",
        ((-0.365, 0.91), (-0.365, 1.69), (-0.22, 1.77),
         (0.11, 1.29), (0.11, 0.89)),
        x, -0.258, 0.012, materials["teal_light"],
    )
    add_prism(
        f"{prefix}_Paint_ScarfLightLip",
        ((-0.50, 1.80), (-0.43, 1.94), (-0.20, 2.01),
         (0.0, 1.94), (-0.12, 1.88), (-0.37, 1.83)),
        x, -0.292, 0.012, materials["oxblood_light"],
    )
    for side in (-1.0, 1.0):
        suffix = "L" if side < 0 else "R"
        add_beveled_box(
            f"{prefix}_Paint_Sleeve_{suffix}",
            (x + side * 0.445, -0.172, 1.64),
            (0.145, 0.018, 0.21), materials["teal_light"], 0.012,
        )

    if accent_level < 2:
        return
    add_prism(
        f"{prefix}_Paint_TunicShadowWedge",
        ((0.10, 1.28), (0.35, 1.66), (0.365, 1.70),
         (0.365, 0.91), (0.12, 0.88)),
        x, -0.272, 0.012, materials["teal_shadow"],
    )
    for side in (-1.0, 1.0):
        suffix = "L" if side < 0 else "R"
        inner_x = x + side * 0.45
        add_beveled_box(
            f"{prefix}_Paint_HandShadow_{suffix}",
            (inner_x, -0.192, 1.00),
            (0.070, 0.018, 0.115), materials["skin"], 0.010,
        )


def add_character(name: str, x: float, variant: str, materials: dict[str, bpy.types.Material]) -> None:
    add_body(name, x, materials)
    if variant == "paint":
        add_painted_definition(name, x, materials, 1)
    elif variant == "accents":
        add_painted_definition(name, x, materials, 2)
    add_head(name, x, materials)


def point_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def setup_scene(materials: dict[str, bpy.types.Material]) -> None:
    scene = bpy.context.scene
    scene.name = "CC_SCREEN_FIRST_CHARACTER_EXPERIMENTS_V04"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.resolution_percentage = 100
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.world.color = PALETTE["background"][:3]
    world_background = scene.world.node_tree.nodes.get("Background") if scene.world.use_nodes else None
    if world_background is None:
        scene.world.use_nodes = True
        world_background = scene.world.node_tree.nodes.get("Background")
    world_background.inputs["Color"].default_value = PALETTE["background"]
    world_background.inputs["Strength"].default_value = 0.22

    add_box("Ground", (0.0, 0.35, -0.07), (16.2, 4.2, 0.14), materials["ground"])
    add_character("LightOnly", -4.05, "light", materials)
    add_character("BroadPaint", 0.0, "paint", materials)
    add_character("PaintAccents", 4.05, "accents", materials)

    camera_data = bpy.data.cameras.new("CAM_ScreenFirst")
    camera = bpy.data.objects.new("CAM_ScreenFirst", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = (0.0, -12.0, 4.05)
    camera.data.type = "ORTHO"
    # Blender's orthographic scale covers the horizontal field.  At 480 x 270
    # this makes the 2.75-unit figure about 60 pixels tall: the top end of the
    # production 35--60 art-pixel distance contract.
    camera.data.ortho_scale = 21.70
    point_at(camera, Vector((0.0, 0.0, 1.35)))
    scene.camera = camera

    key_data = bpy.data.lights.new("KEY_Warm", "AREA")
    key_data.energy = 1800.0
    key_data.shape = "DISK"
    key_data.size = 12.0
    key_data.color = (1.0, 0.71, 0.48)
    key = bpy.data.objects.new("KEY_Warm", key_data)
    key.location = (0.0, -6.0, 8.0)
    bpy.context.scene.collection.objects.link(key)
    point_at(key, Vector((0.0, 0.0, 1.1)))

    fill_data = bpy.data.lights.new("FILL_Cool", "AREA")
    fill_data.energy = 700.0
    fill_data.shape = "DISK"
    fill_data.size = 12.0
    fill_data.color = (0.42, 0.58, 0.67)
    fill = bpy.data.objects.new("FILL_Cool", fill_data)
    fill.location = (0.0, 3.0, 6.0)
    bpy.context.scene.collection.objects.link(fill)
    point_at(fill, Vector((0.0, 0.0, 1.2)))


def render(path: Path, width: int, height: int, percentage: int = 100) -> None:
    scene = bpy.context.scene
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    scene.render.resolution_percentage = percentage
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)


def render_model_views() -> None:
    """Render one unchanged mesh from three angles to prove its 3D read."""
    scene = bpy.context.scene
    camera = scene.camera
    original_location = camera.location.copy()
    original_rotation = camera.rotation_euler.copy()
    original_scale = camera.data.ortho_scale
    hidden = []
    for obj in scene.objects:
        if obj.name.startswith("LightOnly") or obj.name.startswith("PaintAccents"):
            obj.hide_render = True
            hidden.append(obj)

    center_x = 0.0
    target = Vector((center_x, 0.0, 1.38))
    camera.data.ortho_scale = 4.05
    views = (
        (FRONT_PATH, Vector((center_x, -8.0, 3.55))),
        (THREE_QUARTER_PATH, Vector((center_x + 5.4, -7.0, 3.55))),
        (SIDE_PATH, Vector((center_x + 8.0, 0.0, 3.25))),
    )
    for path, location in views:
        camera.location = location
        point_at(camera, target)
        render(path, 480, 480)

    for obj in hidden:
        obj.hide_render = False
    camera.location = original_location
    camera.rotation_euler = original_rotation
    camera.data.ortho_scale = original_scale


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    reset_scene()
    materials = {name: make_material(name, color) for name, color in PALETTE.items()}
    setup_scene(materials)
    render(HIGH_PATH, 1440, 810)
    render(LOW_PATH, 480, 270)
    render_model_views()
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH), compress=True)
    print(f"Rendered high-resolution experiment to {HIGH_PATH}")
    print(f"Rendered art-grid experiment to {LOW_PATH}")
    print(f"Rendered model views to {FRONT_PATH}, {THREE_QUARTER_PATH}, and {SIDE_PATH}")
    print(f"Saved editable scene to {BLEND_PATH}")


if __name__ == "__main__":
    main()
