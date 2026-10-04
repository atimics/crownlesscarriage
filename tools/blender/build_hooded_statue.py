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


def stone_material(name, color):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 0.83
    tex = nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = 4.0
    tex.inputs["Detail"].default_value = 5
    tex.inputs["Roughness"].default_value = 0.72
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.24
    ramp.color_ramp.elements[0].color = (*(v * 0.60 for v in color), 1)
    ramp.color_ramp.elements[1].position = 0.77
    ramp.color_ramp.elements[1].color = (*(min(1, v * 1.17) for v in color), 1)
    links.new(tex.outputs["Fac"], ramp.inputs[0])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    grain = nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 135
    grain.inputs["Detail"].default_value = 3
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.28
    bump.inputs["Distance"].default_value = 0.026
    links.new(grain.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


stone = stone_material("Warm limestone | cloak", (0.57, 0.49, 0.38))
skin = stone_material("Warm limestone | face and hands", (0.52, 0.455, 0.36))
inner = stone_material("Warm limestone | inner robe", (0.53, 0.47, 0.39))


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


# The open mantle becomes a closed hood above the brow.
rows, cols = 146, 161
verts = []
for i in range(rows):
    z = 0.27 + 6.35 * i / (rows - 1)
    rx = interp([(0.27, 1.26), (1.2, 1.32), (2.65, 1.40), (3.45, 1.40),
                 (4.05, 1.18), (4.75, 0.84), (5.45, 0.82), (5.91, 0.83),
                 (6.16, 0.76), (6.40, 0.52), (6.62, 0.0)], z)
    ry = interp([(0.27, 0.64), (3.3, 0.69), (4.15, 0.55), (4.9, 0.54),
                 (5.87, 0.60), (6.2, 0.57), (6.62, 0.0)], z)
    if z > 5.94:
        dome = math.sqrt(max(0, 1 - ((z - 5.94) / 0.68) ** 2))
        rx, ry = 0.83 * dome, 0.60 * dome
    limit = interp([(0.27, 2.49), (2.1, 2.48), (3.6, 2.25), (4.75, 2.32),
                    (5.78, 2.30), (5.97, math.pi), (6.62, math.pi)], z)
    for j in range(cols):
        th = -limit + 2 * limit * j / (cols - 1)
        below = max(0, min(1, (4.9 - z) / 1.25))
        fold = below * (0.070 * math.sin(8 * th + 0.62 * z)
                       + 0.042 * math.sin(13 * th - 0.38 * z)
                       + 0.026 * math.cos(19 * th + z))
        fold += 0.012 * math.sin(6 * th + z * 1.1) * min(1, max(0, (6.62-z)/0.22))
        x = (rx + fold) * math.sin(th)
        y = (ry + fold) * math.cos(th)
        # Gentle unevenness gives the long front edges a natural fall.
        x += 0.025 * below * math.sin(z * 1.5 + th)
        y += 0.020 * below * math.cos(z * 2 + th * 3)
        zz = z + 0.018 * below * math.sin(th * 4 + z)
        verts.append((x, y, zz))
mantle = mesh("01 | hood and falling mantle", verts, grid_faces(rows, cols),
              subdiv=1, thickness=0.095, weather=0.020)

# The underdress carries long, rounded folds with changing depth.
rows, cols = 116, 144
verts = []
for i in range(rows):
    z = 0.29 + 4.22 * i / (rows - 1)
    rx = interp([(0.29, 1.04), (1.2, 0.99), (2.1, 0.81), (2.9, 0.75),
                 (3.7, 0.81), (4.13, 0.86), (4.51, 0.34)], z)
    ry = interp([(0.29, 0.62), (2.1, 0.50), (3.4, 0.53), (4.1, 0.46), (4.51, 0.27)], z)
    for j in range(cols):
        th = 2 * math.pi * j / cols
        deep = interp([(0.29, 0.10), (2.25, 0.095), (3.3, 0.038), (4.51, 0.012)], z)
        phase = th + 0.10 * math.sin(z * 1.3 + th * 2) + 0.045 * z
        fold = deep * (0.76 + 0.24 * math.sin(3 * th - z)) * (
            math.cos(11 * phase + 0.34 * z) + 0.40 * math.cos(17 * phase - z * 0.23))
        verts.append(((rx + fold) * math.sin(th), (ry + fold) * math.cos(th) - 0.04,
                      z + 0.02 * math.sin(th * 5) * max(0, 1 - z)))
gown = mesh("02 | inner robe with long folds", verts, grid_faces(rows, cols, True),
            mat=inner, subdiv=1, thickness=0.075, weather=0.016)
sphere("03 | neck", (0, -0.065, 4.70), (0.285, 0.28, 0.49))

# Soft gathered fabric wraps the base of the neck.
verts = []
for i in range(28):
    z = 4.34 + i * 0.0125
    t = i / 27
    for j in range(80):
        a = 2 * math.pi * j / 80
        r = 0.287 + 0.095 * (1 - t) ** 2 + 0.009 * math.sin(t * 7 + 2.0 * math.sin(a))
        verts.append((r * math.sin(a), r * math.cos(a) - 0.075, z + 0.030 * math.sin(a + 0.4)))
mesh("03b | gathered neck cloth", verts, grid_faces(28, 80, True),
     mat=inner, subdiv=1, thickness=0.025)


def gauss(x, z, cx, cz, sx, sz):
    return math.exp(-0.5 * (((x - cx) / sx) ** 2 + ((z - cz) / sz) ** 2))


# Facial forms are built into a single dense surface.
head_center = Vector((0.015, -0.17, 5.22))
bow = math.radians(9)


def head_point(p):
    x, y, z = p
    return head_center + Vector((x, y * math.cos(bow) - z * math.sin(bow),
                                 y * math.sin(bow) + z * math.cos(bow)))


rows, cols = 150, 192
verts = []
for i in range(rows):
    lat = math.pi * (i + 0.001) / (rows - 1 + 0.002)
    z = 0.67 * math.cos(lat)
    jaw = interp([(-0.67, 0.74), (-0.43, 0.86), (-0.15, 1.00), (0.22, 0.98), (0.67, 1)], z)
    for j in range(cols):
        phi = 2 * math.pi * j / cols
        x = 0.455 * math.sin(lat) * math.sin(phi) * jaw
        y = 0.365 * math.sin(lat) * math.cos(phi)
        front = max(0, -math.cos(phi)) ** 3
        d = 0
        for side in (-1, 1):
            # Relaxed brows, full cheeks and softly closed eyes.
            d += 0.041 * gauss(x, z, side * 0.173, 0.004, 0.106, 0.083)
            brow_z = 0.141 - 0.055 * (abs(x) - 0.10)
            d -= 0.019 * gauss(x, z, side * 0.16, brow_z, 0.124, 0.053)
            d -= 0.038 * gauss(x, z, side * 0.230, -0.144, 0.131, 0.121)
            eye_z = -0.006 - 0.027 * math.exp(-((abs(x) - 0.174) / 0.083) ** 2)
            d -= 0.033 * gauss(x, z, side * 0.171, eye_z + 0.020, 0.083, 0.031)
            d += 0.036 * gauss(x, z, side * 0.171, eye_z, 0.090, 0.009)
            d -= 0.019 * gauss(x, z, side * 0.050, -0.188, 0.041, 0.045)
            d += 0.009 * gauss(x, z, side * 0.052, -0.215, 0.018, 0.012)
        d -= 0.051 * gauss(x, z, 0, 0.007, 0.055, 0.155)
        d -= 0.089 * gauss(x, z, 0, -0.154, 0.063, 0.066)
        d += 0.008 * gauss(x, z, 0, -0.260, 0.022, 0.042)
        lip_z = -0.333 + 0.043 * (abs(x) / 0.16) ** 2
        d -= 0.024 * gauss(x, z, 0, lip_z + 0.018, 0.116, 0.023)
        d += 0.018 * gauss(x, z, 0, lip_z, 0.138, 0.009)
        d -= 0.031 * gauss(x, z, 0, lip_z - 0.028, 0.113, 0.024)
        d += 0.010 * gauss(x, z, 0, -0.400, 0.125, 0.028)
        d -= 0.025 * gauss(x, z, 0, -0.464, 0.168, 0.073)
        y += d * front
        # Low amplitude asymmetry keeps the face soft.
        x += 0.003 * front * math.sin(z * 13)
        verts.append(head_point((x, y, z)))
head = mesh("04 | serene bowed face", verts, grid_faces(rows, cols, True), mat=skin, weather=0.003)


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


def sleeve(name, a, b, c, d):
    pts = bezier(a, b, c, d, 65)
    verts = []
    for i, p in enumerate(pts):
        t = i / (len(pts) - 1)
        tangent = (pts[min(i + 1, len(pts)-1)] - pts[max(0, i-1)]).normalized()
        u = tangent.cross(Vector((0, 1, 0))).normalized()
        v = tangent.cross(u).normalized()
        r = interp([(0, 0.39), (0.5, 0.32), (0.78, 0.275), (1, 0.20)], t)
        for j in range(64):
            th = 2 * math.pi * j / 64
            rr = r + 0.028 * math.sin(7 * th + 5 * t) + 0.018 * math.cos(11 * th - 4*t)
            # Compression folds gather at the inner elbow and cuff.
            rr += 0.024 * math.sin(28 * t + 2 * math.sin(th)) * math.sin(math.pi*t)**2
            verts.append(p + rr * (u * math.cos(th) + v * math.sin(th) * 0.84))
    return mesh(name, verts, grid_faces(65, 64, True), mat=inner, subdiv=1, thickness=0.075, weather=0.012)


sleeve("06 | left sleeve, raised forearm", (-0.77, 0.02, 4.12), (-1.40, -0.28, 2.42),
       (-0.79, -0.73, 2.96), (-0.26, -0.94, 3.46))
sleeve("07 | right sleeve, gathered forearm", (0.77, 0.02, 4.12), (1.36, -0.24, 2.43),
       (0.79, -0.73, 2.94), (0.26, -0.94, 3.44))


def union_hand(name, pieces):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in pieces:
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        for mod in list(obj.modifiers):
            bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.context.view_layer.objects.active = pieces[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    obj.data.remesh_voxel_size = 0.018
    bpy.ops.object.voxel_remesh()
    mod = obj.modifiers.new("Blend the hand forms", "SMOOTH")
    mod.factor = 0.56
    mod.iterations = 4
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for poly in obj.data.polygons:
        poly.use_smooth = True
    obj.modifiers.new("Soft hand surface", "SUBSURF").levels = 1
    return obj


# Tapered fingers rest together in a calm prayer pose.
for side, label in ((-1, "08 | left prayer hand"), (1, "09 | right prayer hand")):
    lift = 0.012 if side == -1 else 0
    parts = [sphere("prayer palm", (side * 0.086, -1.015, 3.704 + lift), (0.083, 0.146, 0.187)),
             sphere("prayer wrist", (side * 0.255, -0.943, 3.446 + lift), (0.119, 0.106, 0.145))]
    pts = bezier((side * 0.254, -0.943, 3.442 + lift), (side * 0.207, -0.976, 3.531 + lift),
                 (side * 0.134, -1.002, 3.617 + lift), (side * 0.090, -1.015, 3.704 + lift), 16)
    parts.append(tube("gentle wrist transition", pts, [0.105 - 0.018 * i / 15 for i in range(16)],
                      sides=16, flatten=0.9))
    for k in range(4):
        y = -1.116 + k * 0.071
        length = [0.30, 0.39, 0.375, 0.292][k]
        pts = bezier((side * 0.086, y, 3.819 + lift), (side * 0.062, y - 0.010, 3.960 + lift),
                     (side * 0.045, y + 0.005, 3.819 + length + lift),
                     (side * 0.025, y + 0.018, 3.843 + length + lift), 20)
        rr = [0.036 * (1 - 0.32 * i / 19) for i in range(20)]
        rr[-1] = 0.014
        parts.append(tube("relaxed prayer finger", pts, rr, sides=14))
    pts = bezier((side * 0.136, -1.145, 3.620 + lift), (side * 0.092, -1.193, 3.695 + lift),
                 (side * 0.053, -1.197, 3.805 + lift), (side * 0.036, -1.155, 3.857 + lift), 20)
    parts.append(tube("resting prayer thumb", pts,
                      [0.051 * (1 - 0.45 * i / 19) for i in range(20)], sides=16))
    union_hand(label, parts)

# A small stone base completes the inferred lower portion.
bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=1.36, depth=0.22, location=(0, 0, 0.15))
base = bpy.context.object
base.name = "10 | low oval stone base"
base.scale.y = 0.65
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
finish(base, stone)
mod = base.modifiers.new("Worn base edge", "BEVEL")
mod.width = 0.06
mod.segments = 4
base.modifiers.new("Base normals", "WEIGHTED_NORMAL")


def track(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def area(name, location, energy, size, color):
    data = bpy.data.lights.new(name, "AREA")
    data.energy, data.shape, data.size, data.color = energy, "DISK", size, color
    obj = bpy.data.objects.new(name, data)
    studio.objects.link(obj)
    obj.location = location
    track(obj, (0, 0, 3.7))


area("Large soft key", (-4.5, -6, 9.3), 950, 5, (1, 0.87, 0.72))
area("Soft front fill", (4, -4, 5), 320, 4, (0.84, 0.88, 1))
area("Mantle edge light", (1.5, 3.5, 8), 1050, 4, (1, 0.90, 0.76))
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
cam.location = (0.65, -16, 6.0)
track(cam, (0, -0.05, 3.42))
cam_data.type = "ORTHO"
cam_data.ortho_scale = 7.35
scene.camera = cam
scene.world.color = (0.18, 0.18, 0.18)
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
scene["Build"] = "Separate editable hood, robe, face, sleeves and hands. Stone nodes and studio are included."
readme = bpy.data.texts.new("READ ME | hooded stone study")
readme.write("HOODED STONE FIGURE\n\nA quick organic study from the supplied front-view photo.\n"
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
print("STATUE_READY", blend_path, flush=True)
