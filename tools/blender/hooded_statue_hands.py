"""Soft prayer hands for the hooded limestone statue."""

import math

import bpy
from mathutils import Matrix, Vector


def build_hands(mesh, sphere, tube, bezier, skin):
    """Build two sculpted hands with full palms and a gentle prayer pose."""
    hands = []

    def ellipsoid(name, center, scale, across, normal, length):
        obj = sphere(name, center, scale, mat=skin)
        basis = Matrix((across, normal, length)).transposed()
        obj.rotation_euler = basis.to_euler()
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
        return obj

    def join_forms(name, parts):
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
        obj.data.remesh_voxel_size = 0.007
        bpy.ops.object.voxel_remesh()
        soft = obj.modifiers.new("Soft palm and finger joins", "SMOOTH")
        soft.factor = 0.42
        soft.iterations = 6
        bpy.ops.object.modifier_apply(modifier=soft.name)
        for poly in obj.data.polygons:
            poly.use_smooth = True
        surface = obj.modifiers.new("Fine hand surface", "SUBSURF")
        surface.levels = 1
        surface.render_levels = 1
        obj["Pose"] = "Relaxed prayer, gently turned palms and rounded fingers"
        obj["Anatomy"] = "Four fingers, thumb, palm pads, wrist and soft knuckles"
        return obj

    for side, label in ((-1, "08 | left angelic prayer hand"),
                        (1, "09 | right angelic prayer hand")):
        lift = 0.009 if side == -1 else 0
        # The small turn exposes the rows of fingers in the portrait view.
        across = Vector((side * 0.56, 0.8285, 0))
        normal = Vector((side * 0.8285, -0.56, 0))
        up = Vector((0, 0, 1))
        center = Vector((side * 0.104, -1.012, 3.77 + lift))

        def point(width, depth, height):
            return center + across * width + normal * depth + up * height

        parts = [ellipsoid("hand | broad palm", center, (0.154, 0.071, 0.207),
                           across, normal, up)]
        parts.append(ellipsoid("hand | thumb pad", point(-0.095, 0.008, -0.063),
                               (0.092, 0.077, 0.128), across, normal, up))
        parts.append(ellipsoid("hand | outer palm pad", point(0.092, 0.002, -0.038),
                               (0.073, 0.062, 0.154), across, normal, up))

        # The wrist starts inside the cuff and blends into the palm heel.
        wrist = Vector((side * 0.26, -0.94, 3.445 + lift))
        pts = bezier(wrist + Vector((side * 0.035, 0.009, -0.052)),
                     wrist + Vector((-side * 0.018, -0.027, 0.072)),
                     point(0.025, 0, -0.133), point(0.0, 0, -0.042), 30)
        rr = []
        for i in range(30):
            t = i / 29
            rr.append(0.111 - 0.030 * t - 0.006 * math.sin(math.pi * t))
        parts.append(tube("hand | smooth wrist", pts, rr, mat=skin,
                          sides=24, flatten=0.85, subdivision=1))

        # Each finger has a separate rounded tip and a gentle bend towards its mate.
        specs = (("index", -0.119, 0.306, 0.036),
                 ("middle", -0.039, 0.348, 0.037),
                 ("ring", 0.041, 0.324, 0.034),
                 ("little", 0.115, 0.252, 0.029))
        for index, (digit, width, length, radius) in enumerate(specs):
            base_height = 0.132 - 0.014 * (index == 3)
            root = point(width, 0.005, base_height)
            tip = point(width - 0.007, -0.035, base_height + length)
            pts = bezier(root, point(width + 0.004, 0.003, base_height + length * 0.34),
                         point(width - 0.004, -0.026, base_height + length * 0.76), tip, 36)
            radii = []
            for i in range(36):
                t = i / 35
                r = radius * (1.0 - 0.24 * t)
                r += 0.0024 * math.exp(-((t - 0.38) / 0.075) ** 2)
                r += 0.0015 * math.exp(-((t - 0.71) / 0.055) ** 2)
                # This round end gives the distal phalanx a soft fingertip.
                if t > 0.89:
                    r *= math.sqrt(max(0.025, 1 - ((t - 0.89) / 0.115) ** 2))
                radii.append(r)
            parts.append(tube("hand | " + digit, pts, radii, mat=skin,
                              sides=24, flatten=0.93, subdivision=1))
            # Small metacarpal forms connect the digits to the palm.
            parts.append(ellipsoid("hand | " + digit + " knuckle", point(width, 0.019, 0.128),
                                   (radius * 1.09, radius * 0.89, radius * 1.30),
                                   across, normal, up))

        # The thumb follows the index edge with a relaxed, open first joint.
        pts = bezier(point(-0.127, 0.040, -0.123),
                     point(-0.214, 0.068, -0.048),
                     point(-0.217, 0.042, 0.089),
                     point(-0.171, 0.009, 0.178), 35)
        rr = []
        for i in range(35):
            t = i / 34
            r = 0.045 * (1 - 0.28 * t)
            r += 0.0024 * math.exp(-((t - 0.63) / 0.075) ** 2)
            if t > 0.87:
                r *= math.sqrt(max(0.025, 1 - ((t - 0.87) / 0.135) ** 2))
            rr.append(r)
        parts.append(tube("hand | rounded resting thumb", pts, rr, mat=skin,
                          sides=24, flatten=0.93, subdivision=1))
        hands.append(join_forms(label, parts))

    return tuple(hands)
