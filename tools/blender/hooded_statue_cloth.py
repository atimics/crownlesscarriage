"""Organic cloth forms for the hooded limestone statue.

The open hood, hanging mantle, gown and sleeves remain separate editable meshes.
Fold placement uses smooth, fixed functions so each build has the same shape.
"""
import math

from mathutils import Vector


def _bell(value, center, width):
    return math.exp(-((value - center) / width) ** 2)


def _smooth(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def build_cloth(mesh, sphere, interp, grid_faces, stone, inner, skin):
    """Build the hood, weighted mantle, gown, neck and soft folded collar."""
    rows, cols = 172, 177
    verts = []
    for i in range(rows):
        z = 0.27 + 6.35 * i / (rows - 1)
        rx = interp([(0.27, 1.44), (0.70, 1.47), (1.55, 1.40), (2.55, 1.34),
                     (3.26, 1.47), (3.83, 1.33), (4.27, 1.10), (4.66, 0.88),
                     (5.30, 0.87), (5.91, 0.875), (6.62, 0.0)], z)
        ry = interp([(0.27, 0.68), (1.50, 0.67), (2.80, 0.70), (3.60, 0.70),
                     (4.23, 0.59), (4.80, 0.575), (5.91, 0.66), (6.62, 0.0)], z)
        center_y = interp([(0.27, 0.045), (3.60, 0.05), (4.68, 0.06),
                           (5.91, 0.06), (6.62, 0.03)], z)
        if z > 5.91:
            dome = math.sqrt(max(0.0, 1.0 - ((z - 5.91) / 0.71) ** 2))
            rx, ry = 0.875 * dome, 0.66 * dome
        limit = interp([(0.27, 2.43), (1.65, 2.43), (2.60, 2.47),
                        (3.35, 2.32), (4.15, 2.28), (4.70, 2.36),
                        (5.90, 2.337), (6.62, math.pi)], z)
        # A shallow arch spans the face. Its center flows into the crown.
        if 5.90 < z < 6.005:
            opening_x = 0.63 * math.sqrt(max(0.0, (6.005 - z) / 0.105))
            limit = math.pi - math.asin(min(1.0, opening_x / max(0.001, rx)))
        elif z >= 6.005:
            limit = math.pi
        falling = _smooth((4.90 - z) / 1.15)
        shoulder = _bell(z, 4.05, 0.48)
        crown = _smooth((6.62 - z) / 0.28)
        for j in range(cols):
            th = -limit + 2 * limit * j / (cols - 1)
            edge = (abs(th) / limit) ** 12
            phase = th + 0.20 * math.sin(z * 0.85 + 1.4 * th)
            phase += 0.075 * math.sin(z * 1.6 - 0.9 * th)
            # Deep uneven folds sweep around the hips and open toward the hem.
            fold = falling * (0.115 * math.cos(5 * phase + 0.30 * z)
                              + 0.046 * math.cos(9 * phase - 0.28 * z)
                              + 0.018 * math.sin(14 * phase + z * 0.30))
            fold += shoulder * 0.065 * math.cos(4 * th - 1.7 * z)
            fold += 0.012 * crown * math.cos(5 * th + z * 0.83)
            fold += edge * (0.060 + 0.045 * math.sin(1.55 * z + th)) * falling
            # The front edges turn out softly where the cloth crosses the arms.
            curl = edge * (0.090 * _bell(z, 3.23, 0.80)
                           + 0.075 * falling * math.sin(1.45 * z + th))
            x = (rx + fold) * math.sin(th)
            x += 0.070 * falling * math.sin(1.20 * z + 0.6 * th)
            x += edge * falling * 0.155 * math.sin(1.45 * z + 0.75 * th)
            y = center_y + (ry + fold * 0.90) * math.cos(th) - curl
            y += 0.050 * falling * math.sin(z * 1.15 + 2 * th)
            back = max(0.0, math.cos(th)) ** 4
            drop = _smooth((4.48 - z) / 1.65)
            # Long uneven back folds begin under the shoulder drape.
            drift = 0.075 * math.sin(1.3 * z)
            y += back * drop * (
                -0.075 * _bell(x, -0.10 + drift, 0.115)
                + 0.105 * _bell(x, 0.37 + drift, 0.145)
                - 0.070 * _bell(x, 0.58 + drift, 0.105)
                + 0.095 * _bell(x, -0.46 - drift, 0.17)
                - 0.065 * _bell(x, -0.73 - drift, 0.11))
            zz = z + 0.028 * falling * math.sin(5 * th + 0.7 * z)
            zz += 0.035 * _smooth((0.65 - z) / 0.38) * math.cos(3 * th + 0.6)
            # Cloth bridges the shoulders with shallow diagonal gathers.
            zz += shoulder * 0.026 * math.cos(4 * th - 1.8)
            verts.append((x, y, zz))
    mantle = mesh("01 | soft hood and weighted mantle", verts,
                  grid_faces(rows, cols), mat=stone, subdiv=1,
                  thickness=0.090, weather=0.014)
    mantle["Cloth form"] = "Deep sweeping folds, uneven front drape and a softly flared hem"

    rows, cols = 138, 160
    verts = []
    for i in range(rows):
        z = 0.29 + 4.22 * i / (rows - 1)
        rx = interp([(0.29, 1.10), (0.80, 1.07), (1.70, 0.94), (2.52, 0.79),
                     (3.35, 0.79), (3.94, 0.87), (4.15, 0.86), (4.51, 0.33)], z)
        ry = interp([(0.29, 0.62), (1.60, 0.59), (2.55, 0.53),
                     (3.43, 0.53), (4.06, 0.49), (4.51, 0.27)], z)
        depth = interp([(0.29, 0.13), (0.72, 0.115), (1.80, 0.10),
                        (2.90, 0.080), (3.53, 0.033), (4.51, 0.010)], z)
        chest = _bell(z, 3.96, 0.49)
        for j in range(cols):
            th = 2 * math.pi * j / cols
            phase = th + 0.15 * math.sin(1.12 * z + 2 * th) + 0.045 * z
            fold = depth * (0.76 + 0.24 * math.cos(3 * th + z * 0.77)) * (
                0.62 * math.cos(7 * phase + 0.20 * z)
                + 0.28 * math.cos(11 * phase - 0.23 * z))
            x = (rx + fold) * math.sin(th) + 0.070 * math.sin(z * 1.05) * _smooth((4.1 - z) / 1.3)
            y = (ry + fold * 0.92) * math.cos(th) - 0.04
            front = max(0.0, -math.cos(th)) ** 5
            fall = _smooth((3.65 - z) / 0.95)
            # Curved ridges run from the gathered waist into the loose skirt.
            for center, amplitude, width, offset in (
                    (-0.72, 0.135, 0.105, 0.2), (-0.34, 0.170, 0.115, 1.3),
                    (0.13, 0.165, 0.120, 2.2), (0.56, 0.135, 0.105, 3.4)):
                taper = 0.64 + 0.36 * _smooth((2.8 - z) / 2.5)
                line = center * taper + 0.115 * math.sin(1.25 * z + offset)
                y -= amplitude * fall * front * _bell(x, line, width)
                y += amplitude * 0.44 * fall * front * _bell(x, line + width * 1.45, width * 0.72)
            # Wide gathers flow from the neckline to the raised hands.
            distance = abs(x) - (0.20 + 0.48 * (4.46 - z))
            gathers = (0.062 * _bell(distance, 0.0, 0.065)
                       - 0.039 * _bell(distance, 0.105, 0.090)
                       + 0.028 * _bell(distance, 0.235, 0.066))
            y += gathers * chest * front
            y -= 0.022 * _bell(z, 3.64, 0.24) * front * math.cos(x * 5)
            hem = _smooth((0.62 - z) / 0.33)
            zz = z + 0.035 * hem * math.sin(5 * th + 0.2)
            zz += 0.009 * chest * front * math.cos(3 * th)
            verts.append((x, y, zz))
    gown = mesh("02 | flowing gown and neckline gathers", verts,
                grid_faces(rows, cols, True), mat=inner, subdiv=1,
                thickness=0.075, weather=0.010)
    gown["Cloth form"] = "Curved skirt folds, gentle body sway and deep neckline gathers"

    sphere("03 | neck", (0.005, -0.065, 4.72), (0.265, 0.255, 0.435), mat=skin)
    rows, cols = 36, 104
    verts = []
    for i in range(rows):
        t = i / (rows - 1)
        for j in range(cols):
            a = 2 * math.pi * j / cols
            front = max(0.0, -math.cos(a))
            r = 0.273 + (0.115 + 0.055 * front) * (1 - t) ** 1.6
            r += 0.018 * math.sin(11 * t + 2.2 * math.sin(a)) * math.sin(math.pi * t)
            r += 0.008 * math.sin(5 * a + 2.3 * t) * (1 - t)
            z = 4.305 + 0.315 * t - 0.095 * front * t - 0.065 * front * (1 - t)
            z += 0.018 * math.sin(a + 0.40) + 0.010 * math.sin(3 * a + 7 * t)
            verts.append((r * math.sin(a), r * math.cos(a) - 0.075, z))
    collar = mesh("03b | soft folded collar", verts, grid_faces(rows, cols, True),
                  mat=inner, subdiv=1, thickness=0.050, weather=0.004)
    collar["Cloth form"] = "Rounded neck wrap with a softly dipped front"
    return mantle, gown, collar


def build_sleeves(mesh, interp, grid_faces, bezier, inner):
    """Build raised sleeves with lower cloth weight and rounded open cuffs."""
    def sleeve(name, side, lift):
        points = bezier((side * 0.57, 0.09, 4.13),
                        (side * 1.36, -0.28, 2.51),
                        (side * 0.82, -0.73, 2.94),
                        (side * 0.26, -0.94, 3.44 + lift), 91)
        cols = 96
        verts = []
        final_u = final_v = None
        for i, p in enumerate(points):
            t = i / (len(points) - 1)
            tangent = (points[min(i + 1, len(points) - 1)]
                       - points[max(0, i - 1)]).normalized()
            u = tangent.cross(Vector((0, 1, 0))).normalized()
            v = tangent.cross(u).normalized()
            gravity = Vector((0, 0, -1)) - tangent * tangent.dot(Vector((0, 0, -1)))
            if gravity.length:
                gravity.normalize()
            radius = interp([(0.0, 0.315), (0.29, 0.350), (0.50, 0.350),
                             (0.70, 0.295), (0.89, 0.226), (0.97, 0.198),
                             (1.0, 0.198)], t)
            slack = math.sin(math.pi * t) ** 1.5
            p = p + Vector((0, 0, -0.025 * slack))
            for j in range(cols):
                th = 2 * math.pi * j / cols
                direction = u * math.cos(th) + v * math.sin(th)
                down = max(0.0, direction.dot(gravity))
                up = max(0.0, -direction.dot(gravity))
                front = max(0.0, -direction.y)
                rr = radius + 0.080 * slack * down ** 2
                # Broad folds fall along the lower sleeve, with small creases above.
                rr += 0.050 * slack * math.cos(4 * th + 3.5 * t + side * 0.7) * (0.28 + 0.72 * down)
                rr += 0.008 * math.sin(7 * th - 2.4 * t) * slack
                for center, strength in ((0.43, 0.045), (0.58, 0.055), (0.74, 0.038)):
                    at = t + 0.057 * math.sin(th + 0.8)
                    rr -= strength * _bell(at, center, 0.032) * (0.18 + 0.30 * up + 0.65 * front)
                    rr += strength * 0.70 * _bell(at, center + 0.050, 0.038) * (0.30 + 0.70 * front)
                # The cuff opens gently around the narrow wrist.
                rr += (0.012 * math.sin(3 * th + side) + 0.006 * math.cos(5 * th + side)) * _smooth((t - 0.84) / 0.16)
                verts.append(p + direction * rr)
            final_u, final_v = u, v
        obj = mesh(name, verts, grid_faces(len(points), cols, True), mat=inner,
                   subdiv=1, thickness=0.065, weather=0.008)
        obj["Cloth form"] = "Lower sleeve weight and soft elbow compression folds"

        # A small rolled edge gives the cuff a clear round rim.
        verts = []
        ring_rows = 20
        center = points[-1]
        tangent = (points[-1] - points[-2]).normalized()
        for i in range(ring_rows):
            a = 2 * math.pi * i / ring_rows
            for j in range(cols):
                th = 2 * math.pi * j / cols
                radius = 0.198 + 0.012 * math.sin(3 * th + side) + 0.006 * math.cos(5 * th + side)
                radial = final_u * math.cos(th) + final_v * math.sin(th)
                verts.append(center + radial * (radius + 0.026 * math.cos(a))
                             + tangent * (0.026 * math.sin(a)))
        faces = []
        for i in range(ring_rows):
            for j in range(cols):
                faces.append((i * cols + j, i * cols + (j + 1) % cols,
                              ((i + 1) % ring_rows) * cols + (j + 1) % cols,
                              ((i + 1) % ring_rows) * cols + j))
        mesh(name + " | rounded cuff rim", verts, faces, mat=inner,
             subdiv=1, weather=0.003)
        return obj

    left = sleeve("06 | draped left sleeve", -1, 0.02)
    right = sleeve("07 | draped right sleeve", 1, 0.0)
    return left, right
