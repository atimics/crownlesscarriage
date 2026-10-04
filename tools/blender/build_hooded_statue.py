"""Build an editable stone statue study from the supplied photo.

Run with Blender 5: blender --background --python build_hooded_statue.py -- --output DIR
The photo supplies the front view. The back and full hem are artistic extensions.
"""
import argparse
import math
from pathlib import Path
import sys

import bpy
import bmesh
from mathutils import Vector

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from hooded_statue_face import build_face
from hooded_statue_hands import build_hands
from hooded_statue_cloth import build_cloth, build_sleeves


def args():
    tail = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("/private/tmp/hooded-statue"))
    parser.add_argument("--samples", type=int, default=32)
    return parser.parse_args(tail)


opt = args()
opt.output.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
sculpt = bpy.data.collections.new("STATUE | sculpt and cloth")
scene.collection.children.link(sculpt)
studio = bpy.data.collections.new("STUDIO | camera and lighting")
scene.collection.children.link(studio)


def place(obj, collection=sculpt):
    for coll in list(obj.users_collection):
        coll.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


def stone_material(name, color, delicate=False):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 0.77 if delicate else 0.85
    position = nodes.new("ShaderNodeNewGeometry")
    tex = nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = 2.6
    tex.inputs["Detail"].default_value = 4
    tex.inputs["Roughness"].default_value = 0.72
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.24
    ramp.color_ramp.elements[0].color = (*(v * 0.82 for v in color), 1)
    ramp.color_ramp.elements[1].position = 0.77
    ramp.color_ramp.elements[1].color = (*(min(1, v * 1.08) for v in color), 1)
    links.new(position.outputs["Position"], tex.inputs["Vector"])
    links.new(tex.outputs["Fac"], ramp.inputs[0])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    grain = nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 180
    grain.inputs["Detail"].default_value = 3
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.12 if delicate else 0.22
    bump.inputs["Distance"].default_value = 0.007 if delicate else 0.018
    links.new(position.outputs["Position"], grain.inputs["Vector"])
    links.new(grain.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


stone = stone_material("Warm limestone | cloak", (0.60, 0.535, 0.438))
skin = stone_material("Warm limestone | face and hands", (0.62, 0.552, 0.463), delicate=True)
inner = stone_material("Warm limestone | inner robe", (0.565, 0.510, 0.435))


def finish(obj, mat, subdiv=0, thickness=0, weather=0):
    obj.data.materials.append(mat)
    if obj.type == "MESH":
        for poly in obj.data.polygons:
            poly.use_smooth = True
    if subdiv:
        mod = obj.modifiers.new("Soft sculpt surface", "SUBSURF")
        mod.levels = subdiv
        mod.render_levels = subdiv
    if thickness:
        mod = obj.modifiers.new("Carved cloth thickness", "SOLIDIFY")
        mod.thickness = thickness
        mod.offset = 0
    if weather:
        texture = bpy.data.textures.get("Subtle stone wear")
        if texture is None:
            texture = bpy.data.textures.new("Subtle stone wear", type="CLOUDS")
            texture.noise_scale = 0.19
            texture.noise_depth = 2
        mod = obj.modifiers.new("Small worn irregularities", "DISPLACE")
        mod.texture = texture
        mod.strength = weather
        mod.mid_level = 0.5
        mod.texture_coords = "GLOBAL"
    place(obj)
    return obj


def mesh(name, verts, faces, mat=stone, **kwargs):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=0.0005)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    obj = bpy.data.objects.new(name, data)
    sculpt.objects.link(obj)
    return finish(obj, mat, **kwargs)


def sphere(name, center, scale, mat=skin):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=32, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, mat)


def interp(points, t):
    for a, b in zip(points, points[1:]):
        if t <= b[0]:
            f = max(0, (t - a[0]) / (b[0] - a[0]))
            f = f * f * (3 - 2 * f)
            return a[1] * (1 - f) + b[1] * f
    return points[-1][1]


def grid_faces(rows, cols, closed=False):
    return [(i * cols + j, i * cols + (j + 1) % cols,
             (i + 1) * cols + (j + 1) % cols, (i + 1) * cols + j)
            for i in range(rows - 1) for j in range(cols if closed else cols - 1)]


mantle, gown, collar = build_cloth(mesh, sphere, interp, grid_faces, stone, inner, skin)


head = build_face(mesh, sphere, skin)


def tube(name, points, radii, mat=skin, sides=16, flatten=1, subdivision=1, closed_caps=True):
    points = [Vector(p) for p in points]
    verts = []
    for i, (p, r) in enumerate(zip(points, radii)):
        tangent = (points[min(i + 1, len(points) - 1)] - points[max(0, i - 1)]).normalized()
        u = tangent.cross(Vector((0, 1, 0)))
        if u.length < 0.01:
            u = tangent.cross(Vector((1, 0, 0)))
        u.normalize()
        v = tangent.cross(u).normalized()
        for j in range(sides):
            a = 2 * math.pi * j / sides
            verts.append(p + r * (u * math.cos(a) + v * math.sin(a) * flatten))
    faces = grid_faces(len(points), sides, True)
    if closed_caps:
        faces.extend([tuple(reversed(range(sides))),
                      tuple((len(points) - 1) * sides + j for j in range(sides))])
    return mesh(name, verts, faces, mat=mat, subdiv=subdivision)


def bezier(a, b, c, d, count):
    a, b, c, d = map(Vector, (a, b, c, d))
    return [(1-t)**3*a + 3*(1-t)**2*t*b + 3*(1-t)*t*t*c + t**3*d
            for t in (i / (count - 1) for i in range(count))]


build_sleeves(mesh, interp, grid_faces, bezier, inner)
left_hand, right_hand = build_hands(mesh, sphere, tube, bezier, skin)


# A gently worn base supports the inferred full hem.
base_profile = [(0.045, 0.952), (0.061, 0.983), (0.084, 0.998), (0.119, 1.0),
                (0.183, 0.999), (0.221, 0.987), (0.247, 0.958), (0.259, 0.948)]
verts = []
for z, radius in base_profile:
    for j in range(128):
        th = j * 2 * math.pi / 128
        wear = 0.005 * math.sin(7 * th + z * 3) + 0.003 * math.cos(13 * th)
        verts.append(((1.63 * radius + wear) * math.sin(th),
                      (1.01 * radius + wear) * math.cos(th), z + 0.002 * math.sin(5 * th)))
faces = grid_faces(len(base_profile), 128, True)
faces.extend([tuple(reversed(range(128))), tuple((len(base_profile) - 1) * 128 + j for j in range(128))])
base = mesh("10 | softly worn oval stone base", verts, faces, subdiv=1, weather=0.005)


def track(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def area(name, location, energy, size, color):
    data = bpy.data.lights.new(name, "AREA")
    data.energy, data.shape, data.size, data.color = energy, "DISK", size, color
    obj = bpy.data.objects.new(name, data)
    studio.objects.link(obj)
    obj.location = location
    track(obj, (0, 0, 3.7))


area("Large soft key", (-3.8, -5.0, 8.4), 720, 3.4, (1, 0.91, 0.79))
area("Soft front fill", (4, -5, 5.2), 230, 4.5, (0.89, 0.92, 1))
area("Mantle edge light", (1.5, 3.5, 7.8), 850, 3.5, (1, 0.94, 0.84))
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, 0.025))
floor = place(bpy.context.object, studio)
floor.name = "Studio floor"
floor_mat = bpy.data.materials.new("Charcoal backdrop")
floor_mat.diffuse_color = (0.035, 0.039, 0.043, 1)
floor_mat.use_nodes = True
floor_mat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (0.035, 0.039, 0.043, 1)
floor_mat.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.9
floor.data.materials.append(floor_mat)

cam_data = bpy.data.cameras.new("Portrait camera")
cam = bpy.data.objects.new("Portrait camera", cam_data)
studio.objects.link(cam)
cam.location = (0.65, -16, 5.7)
track(cam, (0, -0.05, 3.15))
cam_data.type = "ORTHO"
cam_data.ortho_scale = 6.90
scene.camera = cam
scene.world.use_nodes = True
scene.world.node_tree.nodes.get("Background").inputs["Color"].default_value = (0.28, 0.31, 0.35, 1)
scene.world.node_tree.nodes.get("Background").inputs["Strength"].default_value = 0.14
scene.render.engine = "CYCLES"
scene.cycles.samples = opt.samples
scene.cycles.use_denoising = True
scene.render.resolution_x = 1100
scene.render.resolution_y = 1500
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.view_settings.view_transform = "AgX"

scene["Reference"] = "User supplied photo of a hooded stone figure with bowed head and gathered hands"
scene["Study notes"] = "Front view follows the photo. Back, hem and base are an artistic extension."
scene["Build"] = "Separate editable veil, robe, face, sleeves and hands. Stone nodes and studio are included."
readme = bpy.data.texts.new("READ ME | hooded stone study")
readme.write("HOODED STONE FIGURE\n\nAn organic study with a soft gathered veil, sweeping folds and gentle prayer hands.\n"
             "The back, full hem and base are inferred.\n\n"
             "STATUE contains the editable mesh pieces. STUDIO contains the camera, floor and lights.\n"
             "The warm limestone materials include mottling and fine grain.\n"
             "The robe modifiers control thickness and soft wear.\n")

bpy.ops.object.select_all(action="DESELECT")
for obj in sculpt.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = head
for screen in bpy.data.screens:
    for space_area in screen.areas:
        if space_area.type == "VIEW_3D":
            space_area.spaces.active.region_3d.view_distance = 9
            space_area.spaces.active.region_3d.view_location = (0, 0, 3.5)
            space_area.spaces.active.region_3d.view_rotation = cam.rotation_euler.to_quaternion()
            space_area.spaces.active.shading.type = "MATERIAL"
            space_area.spaces.active.overlay.show_overlays = False

blend_path = opt.output / "hooded_stone_statue.blend"
bpy.ops.wm.save_as_mainfile(filepath=str(blend_path), compress=True)
# A portable mesh copy keeps the sculpture separate from the render studio.
bpy.ops.wm.obj_export(filepath=str(opt.output / "hooded_stone_statue.obj"),
                      export_selected_objects=True, apply_modifiers=True)
scene.render.filepath = str(opt.output / "hooded_stone_statue.png")
bpy.ops.render.render(write_still=True)

# Two closer views make the anatomy and fabric easy to inspect.
portrait_state = (cam.location.copy(), cam.rotation_euler.copy(), cam_data.ortho_scale,
                  scene.render.resolution_x, scene.render.resolution_y)
cam.location = (3.8, -11.0, 5.6)
track(cam, (0, -0.1, 3.17))
cam_data.ortho_scale = 6.85
scene.render.filepath = str(opt.output / "hooded_stone_statue_three_quarter.png")
bpy.ops.render.render(write_still=True)
cam.location = (1.05, -12, 5.9)
track(cam, (0, -0.2, 4.73))
cam_data.ortho_scale = 2.75
scene.render.resolution_x = 1400
scene.render.resolution_y = 1600
scene.render.filepath = str(opt.output / "hooded_stone_statue_detail.png")
bpy.ops.render.render(write_still=True)
cam.location = (0.7, -12, 5.0)
track(cam, (0, -1.0, 3.88))
cam_data.ortho_scale = 1.58
scene.render.resolution_x = 1100
scene.render.resolution_y = 1100
scene.render.filepath = str(opt.output / "hooded_stone_statue_hands.png")
bpy.ops.render.render(write_still=True)
cam.location, cam.rotation_euler, cam_data.ortho_scale, scene.render.resolution_x, scene.render.resolution_y = portrait_state
scene.render.filepath = str(opt.output / "hooded_stone_statue.png")
print("STATUE_READY", blend_path, flush=True)
