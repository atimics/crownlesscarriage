"""Gently gathered hands for the limestone statue."""

import math

import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree


def build_hands(mesh, sphere, tube, bezier, skin):
    """Build two continuous hands with a soft diagonal gesture."""
    hands = []

    def bell(value, center, width):
        return math.exp(-((value - center) / width) ** 2)

    def ellipsoid(name, center, scale, across, normal, length):
        obj = sphere(name, center, scale, mat=skin)
        obj.rotation_euler = Matrix((across, normal, length)).transposed().to_euler()
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
        return obj

    def sculpt_surface(obj, paths, point, normal, across, length):
        # Shallow cuts keep the nails and joint creases part of the hand mesh.
        prepared = []
        for path, radii in paths:
            tree = KDTree(len(path))
            frames = []
            for i, sample in enumerate(path):
                tree.insert(sample, i)
                tangent = (path[min(i + 1, len(path) - 1)] -
                           path[max(i - 1, 0)]).normalized()
                face = (normal - tangent * normal.dot(tangent)).normalized()
                frames.append((face, face.cross(tangent).normalized()))
            tree.balance()
            prepared.append((path, radii, tree, frames))
        for vert in obj.data.vertices:
            p = obj.matrix_world @ vert.co
            delta = Vector((0, 0, 0))
            for path, radii, tree, frames in prepared:
                _, nearest, distance = tree.find(p)
                t = nearest / (len(path) - 1)
                radius = radii[nearest]
                if t < 0.20 or distance > radius * 1.16:
                    continue
                face, width_axis = frames[nearest]
                offset = p - path[nearest]
                width = offset.dot(width_axis)
                depth = offset.dot(face)
                if depth < radius * 0.43 or abs(width) > radius * 1.05:
                    continue
                top = min(1.0, max(0.0, (depth / radius - 0.43) / 0.38))
                crease = (0.0019 * bell(t, 0.41, 0.024) +
                          0.0014 * bell(t, 0.68, 0.020))
                crease *= bell(width, 0, radius * 0.92)
                # A U-shaped nail bed opens towards the rounded fingertip.
                nail_width = radius * 0.62
                nail_side = bell(abs(width), nail_width, 0.0027)
                nail_side *= bell(t, 0.84, 0.11)
                nail_root = bell(t, 0.755 + 0.045 * (width / nail_width) ** 2, 0.016)
                nail_root *= bell(width, 0, nail_width * 1.1)
                delta -= face * top * (crease + 0.0027 * (nail_side + nail_root))
            # Subtle folds at the wrist and the root of the thumb.
            local = p - point(0, 0, 0)
            u, d, h = local.dot(across), local.dot(normal), local.dot(length)
            if d > 0.029 and abs(u) < 0.095:
                delta -= normal * 0.0018 * bell(h, -0.185 + u * 0.12, 0.008)
            vert.co += obj.matrix_world.inverted().to_3x3() @ delta

    def join_forms(name, parts, paths, point, normal, across, length):
        bpy.ops.object.select_all(action="DESELECT")
        for obj in parts:
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            for mod in list(obj.modifiers):
                bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.context.view_layer.objects.active = parts[0]
        bpy.ops.object.join()
        obj = bpy.context.object
        obj.name = name
        obj.data.remesh_voxel_size = 0.0045
        bpy.ops.object.voxel_remesh()
        soft = obj.modifiers.new("Soft hand joins", "SMOOTH")
        soft.factor = 0.42
        soft.iterations = 6
        bpy.ops.object.modifier_apply(modifier=soft.name)
        sculpt_surface(obj, paths, point, normal, across, length)
        for poly in obj.data.polygons:
            poly.use_smooth = True
        surface = obj.modifiers.new("Fine hand surface", "SUBSURF")
        surface.levels = 1
        surface.render_levels = 1
        obj["Pose"] = "Loose hands across the chest, one resting gently over the other"
        obj["Anatomy"] = "Four tapered fingers, a slender thumb, soft knuckles, nails and wrist folds"
        return obj

    poses = (
        ("08 | left gathered hand", (-0.15, -1.135, 3.80),
         (0.80, 0.0, 0.60), (-0.40, -0.91, 3.45), 1.0),
        ("09 | right gathered hand", (0.16, -1.035, 3.61),
         (-0.61, 0.0, 0.7924), (0.38, -1.01, 3.31), 0.94),
    )
    for label, palm_center, palm_axis, cuff_center, size in poses:
        center = Vector(palm_center)
        length = Vector(palm_axis).normalized()
        normal = Vector((0, -1, 0))
        across = normal.cross(length).normalized()

        def point(width, depth, height):
            return center + across * width + normal * depth + length * height

        # Oval cross-sections give a broad, thin palm and a narrower heel.
        verts, faces = [], []
        rows, sides = 31, 48
        for i in range(rows):
            t = i / (rows - 1)
            height = -0.19 + 0.35 * t
            edge = math.sin(math.pi * t) ** 0.32
            width = size * (0.074 + 0.058 * t) * edge
            thick = (0.042 + 0.008 * bell(t, 0.38, 0.23)) * edge
            for j in range(sides):
                angle = 2 * math.pi * j / sides
                w = width * math.cos(angle)
                d = thick * math.sin(angle)
                d += 0.004 * bell(w, -0.065, 0.04) * bell(t, 0.42, 0.22)
                if math.sin(angle) > 0:
                    for tendon_width in (-0.030, 0.035):
                        ridge_width = tendon_width * (0.45 + 0.55 * t)
                        d += 0.0025 * bell(w, ridge_width, 0.012) * bell(t, 0.55, 0.25)
                verts.append(point(w, d, height))
        for i in range(rows - 1):
            for j in range(sides):
                faces.append((i * sides + j, i * sides + (j + 1) % sides,
                              (i + 1) * sides + (j + 1) % sides, (i + 1) * sides + j))
        faces.extend((tuple(reversed(range(sides))),
                      tuple((rows - 1) * sides + j for j in range(sides))))
        parts = [mesh("hand | thin palm", verts, faces, mat=skin, subdiv=1)]
        parts.append(ellipsoid("hand | soft thumb root", point(-0.069, -0.006, -0.085),
                               (0.052, 0.040, 0.088), across, normal, length))

        wrist = Vector(cuff_center)
        start = wrist - length * 0.060 + Vector((0, 0.018, -0.016))
        pts = bezier(start, wrist + length * 0.078,
                     point(0.008, -0.003, -0.120), point(0, 0, -0.012), 32)
        rr = [0.081 - 0.038 * i / 31 for i in range(32)]
        parts.append(tube("hand | narrow wrist", pts, rr, mat=skin,
                          sides=28, flatten=0.72, subdivision=1))

        finger_paths = []
        specs = (("index", -0.092, 0.273, 0.032),
                 ("middle", -0.030, 0.304, 0.032),
                 ("ring", 0.035, 0.279, 0.030),
                 ("little", 0.092, 0.219, 0.025))
        for index, (digit, width, digit_length, radius) in enumerate(specs):
            width *= size
            root_height = -0.027 - 0.012 * (index == 3)
            tip_height = 0.096 - 0.012 * (index == 3) + digit_length
            bend = (0.028 + 0.008 * index) * (1 if label.startswith("08") else -1)
            end_width = width - (0.037 - 0.004 * index) if bend > 0 else width - 0.011 + 0.007 * index
            pts = bezier(point(width * 0.62, 0.001, root_height),
                         point(width + 0.025, 0.013, 0.164),
                         point(end_width, -0.018 if bend > 0 else 0.028,
                               tip_height - digit_length * 0.21),
                         point(end_width, -bend, tip_height), 42)
            rr = []
            for i in range(42):
                t = i / 41
                r = radius * size * (1 - 0.28 * t)
                r += 0.0024 * bell(t, 0.64, 0.065)
                r += 0.0015 * bell(t, 0.83, 0.050)
                if t > 0.89:
                    r *= math.sqrt(max(0.02, 1 - ((t - 0.89) / 0.112) ** 2))
                rr.append(r)
            parts.append(tube("hand | " + digit, pts, rr, mat=skin,
                              sides=28, flatten=0.80, subdivision=1))
            finger_paths.append((pts[12:], rr[12:]))

        # The thumb curves alongside the index finger with a small open web.
        if label.startswith("08"):
            pts = bezier(point(-0.074, 0.001, -0.135),
                         point(-0.163, 0.015, -0.059),
                         point(-0.174, 0.014, 0.068),
                         point(-0.138, -0.005, 0.157), 38)
        else:
            pts = bezier(point(-0.071, 0.001, -0.134),
                         point(-0.159, 0.017, -0.059),
                         point(-0.122, 0.031, 0.125),
                         point(-0.035, 0.047, 0.225), 38)
        rr = []
        for i in range(38):
            t = i / 37
            r = 0.036 * size * (1 - 0.26 * t) + 0.0021 * bell(t, 0.56, 0.065)
            if t > 0.89:
                r *= math.sqrt(max(0.02, 1 - ((t - 0.89) / 0.112) ** 2))
            rr.append(r)
        parts.append(tube("hand | relaxed thumb", pts, rr, mat=skin,
                          sides=28, flatten=0.83, subdivision=1))
        finger_paths.append((pts, rr))
        hands.append(join_forms(label, parts, finger_paths, point, normal, across, length))

    return tuple(hands)
