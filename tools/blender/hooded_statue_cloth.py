"""Stone cloth shaped by head contact, shoulder support and bent arms."""
import math

from mathutils import Vector


def _bell(value, center, width):
    return math.exp(-((value - center) / width) ** 2)


def _smooth(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def _head_space(x, y, z):
    """Use the face's gentle bow and side tilt for cloth contact."""
    bow, tilt = math.radians(12), math.radians(-4)
    xx = x * math.cos(tilt) + z * math.sin(tilt)
    tilted_z = -x * math.sin(tilt) + z * math.cos(tilt)
    yy = y * math.cos(bow) - tilted_z * math.sin(bow)
    zz = y * math.sin(bow) + tilted_z * math.cos(bow)
    return Vector((0.015 + xx, -0.17 + yy, 5.22 + zz))


def _veil_point(th, t):
    """Follow the skull, with a free edge from the brow to each temple."""
    front = max(0.0, -math.cos(th))
    lower_z = -0.10 + 0.57 * _smooth((front - 0.20) / 0.52)
    z = lower_z + (0.674 - lower_z) * t
    dome = math.sqrt(max(0.0, 1.0 - (z / 0.677) ** 2))
    rx = 0.441 * dome
    ry = (0.394 if math.cos(th) < 0 else 0.440) * dome
    # One temple holds the cloth. The other has a deeper loose fold.
    left = _bell(th, -2.13 + 0.11 * t, 0.19)
    right = _bell(th, 2.12 - 0.06 * t, 0.24)
    fold = (0.050 * left - 0.007 * right) * (1 - t) ** 1.5
    fold -= 0.011 * _bell(th, -1.86 + 0.10 * t, 0.085) * (1 - t) ** 1.8
    # A small brow gather flattens as it crosses the crown.
    gather = _bell(th, 2.78, 0.27) * _bell(t, 0.14, 0.18)
    fold += 0.018 * gather
    fold -= 0.007 * _bell(th, 2.48, 0.11) * _bell(t, 0.12, 0.18)
    fold += 0.010 * math.sin(3 * th + 1.3 * t) * math.sin(math.pi * t)
    # A loose diagonal gather runs from the forehead toward the supported crown.
    sweep = -2.70 + 1.55 * t
    fold += 0.023 * _bell(th, sweep, 0.18) * math.sin(math.pi * (0.18 + 0.70 * t))
    fold -= 0.005 * _bell(th, sweep + 0.25, 0.10) * math.sin(math.pi * t)
    fold += 0.008 * front ** 3 * _bell(t, 0.040, 0.025)
    x = (rx + fold) * math.sin(th)
    y = (ry + fold * 0.65) * math.cos(th)
    z += 0.005 * math.sin(th + 0.8) * (1 - t) * front
    return _head_space(x, y, z)


def build_cloth(mesh, sphere, interp, grid_faces, stone, inner, skin):
    """Build a supported veil, weighted mantle, gown, neck and collar."""
    rows, cols = 164, 177
    top_limit = 2.16
    verts = []
    for i in range(rows):
        z = 0.27 + 5.13 * i / (rows - 1)
        rx = interp([(0.27, 1.44), (0.70, 1.47), (1.55, 1.40), (2.55, 1.34),
                     (3.26, 1.40), (3.83, 1.23), (4.27, 1.02), (4.60, 0.73),
                     (5.40, 0.43)], z)
        ry = interp([(0.27, 0.68), (1.50, 0.67), (2.80, 0.70), (3.60, 0.67),
                     (4.23, 0.55), (4.60, 0.48), (5.40, 0.39)], z)
        limit = interp([(0.27, 2.43), (1.65, 2.43), (2.60, 2.47),
                        (3.35, 2.31), (4.15, 2.25), (4.70, top_limit),
                        (5.40, top_limit)], z)
        falling = _smooth((4.75 - z) / 1.08)
        shoulder = _bell(z, 4.03, 0.43)
        fit = _smooth((z - 4.60) / 0.80)
        for j in range(cols):
            th = -limit + 2 * limit * j / (cols - 1)
            edge = (abs(th) / limit) ** 12
            phase = th + 0.15 * math.sin(z * 0.90 + 1.4 * th)
            # Wide crests and narrow valleys carry the mantle's weight.
            fold = falling * (0.098 * math.cos(5 * phase + 0.29 * z)
                              + 0.032 * math.cos(8 * phase - 0.24 * z))
            fold -= falling * 0.034 * _bell(th, -0.77 + 0.05 * math.sin(z), 0.075)
            fold += shoulder * 0.034 * math.cos(3 * th - 1.5 * z)
            # The front edge is pulled by the shoulder and clears the elbow.
            fold += edge * (0.042 + 0.036 * math.sin(1.47 * z + th)) * falling
            curl = edge * (0.071 * _bell(z, 3.12, 0.74)
                           + 0.052 * falling * math.sin(1.42 * z + th))
            x = (rx + fold) * math.sin(th)
            x += 0.052 * falling * math.sin(1.14 * z + 0.55 * th)
            x += edge * falling * 0.120 * math.sin(1.34 * z + 0.75 * th)
            y = 0.045 + (ry + fold * 0.88) * math.cos(th) - curl
            y += 0.027 * falling * math.sin(z * 1.11 + 2 * th)
            back = max(0.0, math.cos(th)) ** 4
            drop = _smooth((4.35 - z) / 1.4)
            drift = 0.067 * math.sin(1.2 * z)
            y += back * drop * (
                -0.071 * _bell(x, -0.10 + drift, 0.10)
                + 0.086 * _bell(x, 0.37 + drift, 0.16)
                - 0.052 * _bell(x, 0.60 + drift, 0.095)
                + 0.080 * _bell(x, -0.46 - drift, 0.19))
            zz = z + 0.024 * falling * math.sin(5 * th + 0.7 * z)
            zz += 0.025 * _smooth((0.65 - z) / 0.38) * math.cos(3 * th + 0.6)
            zz += shoulder * 0.037 * math.sin(th - 0.3)
            # The loose temple fold continues through the cheek into the shoulder.
            cheek = _bell(z, 4.90, 0.29) * _bell(th, -2.05, 0.16)
            x -= 0.035 * cheek
            y -= 0.047 * cheek
            p = Vector((x, y, zz))
            p = p.lerp(_veil_point(th, 0.0), fit)
            verts.append(p)
    faces = grid_faces(rows, cols)
    # The crown uses full rings. The brow edge stays open and follows an arch.
    # Sharing the temple/back row keeps the whole cloth one smooth sheet.
    front_cols = 64
    angles = [-top_limit + 2 * top_limit * j / (cols - 1) for j in range(cols)]
    angles += [top_limit + (2 * math.pi - 2 * top_limit) * j / front_cols
               for j in range(1, front_cols)]
    ring_cols = len(angles)
    bottom = list(range((rows - 1) * cols, rows * cols))
    for th in angles[cols:]:
        bottom.append(len(verts))
        verts.append(_veil_point(th, 0.0))
    previous = bottom
    cap_rows = 62
    for i in range(1, cap_rows):
        t = i / cap_rows
        current = []
        for th in angles:
            current.append(len(verts))
            verts.append(_veil_point(th, t))
        for j in range(ring_cols):
            faces.append((previous[j], previous[(j + 1) % ring_cols],
                          current[(j + 1) % ring_cols], current[j]))
        previous = current
    tip = len(verts)
    verts.append(_head_space(0, 0, 0.678))
    for j in range(ring_cols):
        faces.append((previous[j], previous[(j + 1) % ring_cols], tip))
    mantle = mesh("01 | supported veil and falling mantle", verts, faces,
                  mat=stone, subdiv=1, thickness=0.032, weather=0.006)
    mantle["Cloth form"] = "Thin arched brow edge, crown contact, one held temple and a loose cheek fold"

    rows, cols = 146, 160
    verts = []
    for i in range(rows):
        z = 0.29 + 4.22 * i / (rows - 1)
        rx = interp([(0.29, 1.10), (0.80, 1.07), (1.70, 0.94), (2.52, 0.76),
                     (3.35, 0.75), (3.94, 0.83), (4.15, 0.82), (4.51, 0.30)], z)
        ry = interp([(0.29, 0.62), (1.60, 0.59), (2.55, 0.51),
                     (3.43, 0.50), (4.06, 0.46), (4.51, 0.26)], z)
        depth = interp([(0.29, 0.105), (0.72, 0.092), (1.80, 0.085),
                        (2.90, 0.055), (3.53, 0.021), (4.51, 0.003)], z)
        chest = _bell(z, 3.92, 0.50)
        for j in range(cols):
            th = 2 * math.pi * j / cols
            phase = th + 0.12 * math.sin(1.02 * z + 2 * th) + 0.038 * z
            fold = depth * (0.80 + 0.20 * math.cos(3 * th + z * 0.7)) * (
                0.68 * math.cos(6 * phase + 0.18 * z)
                + 0.23 * math.cos(9 * phase - 0.20 * z))
            sway = -0.085 * _bell(z, 2.95, 1.15) + 0.024 * _bell(z, 4.15, 0.42)
            x = (rx + fold) * math.sin(th) + sway
            y = (ry + fold * 0.91) * math.cos(th) - 0.04
            front = max(0.0, -math.cos(th)) ** 5
            fall = _smooth((3.50 - z) / 0.80)
            # Unequal long folds open below the raised arms and split at the hem.
            for center, amplitude, width, offset in (
                    (-0.70, 0.102, 0.13, 0.2), (-0.31, 0.137, 0.14, 1.3),
                    (0.18, 0.151, 0.17, 2.2), (0.58, 0.094, 0.11, 3.4)):
                taper = 0.66 + 0.34 * _smooth((2.8 - z) / 2.5)
                line = center * taper + 0.09 * math.sin(1.12 * z + offset)
                y -= amplitude * fall * front * _bell(x, line, width)
                y += amplitude * 0.46 * fall * front * _bell(x, line + width * 1.45, width * 0.56)
            # The cloth stretches diagonally from a shoulder toward the gathered hands.
            # Broad planes separate two folds with different lengths and depths.
            left_line = 4.26 + 0.51 * (x + 0.50)
            right_line = 4.08 - 0.35 * (x - 0.38)
            y -= 0.047 * _bell(z, left_line, 0.078) * _bell(x, -0.33, 0.39) * front
            y += 0.024 * _bell(z, left_line - 0.105, 0.050) * _bell(x, -0.29, 0.34) * front
            y -= 0.029 * _bell(z, right_line, 0.11) * _bell(x, 0.35, 0.30) * front
            # A low slack pocket lies between the hands and waist.
            pocket_z = 3.43 + 0.28 * ((x + 0.06) / 0.65) ** 2
            y += 0.042 * _bell(z, pocket_z, 0.09) * _bell(x, -0.02, 0.54) * front
            y -= 0.028 * _bell(z, pocket_z - 0.13, 0.13) * _bell(x, -0.04, 0.48) * front
            hem = _smooth((0.62 - z) / 0.33)
            zz = z + 0.032 * hem * math.sin(5 * th + 0.2)
            zz += 0.028 * chest * math.sin(th - 0.2)
            verts.append((x, y, zz))
    gown = mesh("02 | weighted gown and diagonal chest gathers", verts,
                grid_faces(rows, cols, True), mat=inner, subdiv=1,
                thickness=0.060, weather=0.006)
    gown["Cloth form"] = "A gentle weight shift, broad chest tension planes and unequal skirt folds"

    sphere("03 | neck", (-0.004, -0.084, 4.72), (0.247, 0.237, 0.425), mat=skin)
    rows, cols = 32, 104
    verts = []
    for i in range(rows):
        t = i / (rows - 1)
        for j in range(cols):
            a = 2 * math.pi * j / cols
            front = max(0.0, -math.cos(a))
            r = 0.253 + (0.085 + 0.020 * front) * (1 - t) ** 1.6
            r += 0.010 * math.sin(7 * t + 1.6 * math.sin(a)) * math.sin(math.pi * t)
            z = 4.380 + 0.230 * t - 0.085 * front * t - 0.065 * front * (1 - t)
            z += 0.018 * math.sin(a + 0.40)
            verts.append((r * math.sin(a) - 0.008, r * math.cos(a) - 0.084, z))
    collar = mesh("03b | soft folded collar", verts, grid_faces(rows, cols, True),
                  mat=inner, subdiv=1, thickness=0.040, weather=0.003)
    collar["Cloth form"] = "A quiet neck wrap with a softly dipped front"
    return mantle, gown, collar


def build_sleeves(mesh, interp, grid_faces, bezier, inner):
    """Build sleeves from each shoulder through its bent elbow to the wrist."""
    def sleeve(name, side):
        lift = 0.075 if side < 0 else 0.0
        shoulder = (side * 0.57, 0.025, 4.07 + lift)
        elbow = (side * 0.85, -0.47, 2.88 + lift)
        wrist = (-0.40, -0.91, 3.45) if side < 0 else (0.38, -1.01, 3.31)
        upper = bezier(shoulder, (side * 0.84, -0.07, 3.86 + lift),
                       (side * 0.92, -0.30, 3.13 + lift), elbow, 61)
        forearm = bezier(elbow, (side * 0.83, -0.62, 2.98 + lift),
                         (side * 0.51, wrist[1] + 0.12, wrist[2] - 0.20), wrist, 61)
        points = upper + forearm[1:]
        cols = 96
        verts = []
        for i, p in enumerate(points):
            t = i / (len(points) - 1)
            tangent = (points[min(i + 1, len(points) - 1)]
                       - points[max(0, i - 1)]).normalized()
            u = tangent.cross(Vector((0, 1, 0))).normalized()
            v = tangent.cross(u).normalized()
            gravity = Vector((0, 0, -1)) - tangent * tangent.dot(Vector((0, 0, -1)))
            if gravity.length:
                gravity.normalize()
            radius = interp([(0.0, 0.258), (0.22, 0.278), (0.45, 0.269),
                             (0.61, 0.244), (0.78, 0.202), (0.91, 0.165),
                             (1.0, 0.163)], t)
            slack = math.sin(math.pi * t) ** 1.4
            for j in range(cols):
                th = 2 * math.pi * j / cols
                direction = u * math.cos(th) + v * math.sin(th)
                down = max(0.0, direction.dot(gravity))
                up = max(0.0, -direction.dot(gravity))
                front = max(0.0, -direction.y)
                rr = radius + 0.031 * slack * down ** 2
                # Two broad lower folds give the cloth a hanging direction.
                rr += 0.026 * slack * math.cos(3 * th + 1.5 * t + side * 0.55) * (0.18 + 0.82 * down)
                rr -= 0.017 * slack * _bell(math.sin(th + 0.7 * t), 0.1, 0.15) * down
                # Short valleys collect at the inside elbow and open over the forearm.
                for center, strength, width in ((0.46, 0.025, 0.024),
                                                (0.56, 0.034, 0.029),
                                                (0.69, 0.022, 0.032)):
                    at = t + 0.040 * math.sin(th + side * 0.4)
                    compress = 0.12 + 0.68 * up + 0.20 * front
                    rr -= strength * _bell(at, center, width) * compress
                    rr += strength * 0.68 * _bell(at, center + width * 1.6, width * 1.35) * compress
                cuff = _smooth((t - 0.86) / 0.14)
                rr += (0.008 * math.sin(2 * th + side) + 0.003 * math.cos(5 * th)) * cuff
                # A thin turned hem is part of the sleeve surface.
                rr += 0.007 * _bell(t, 0.984, 0.015)
                verts.append(p + direction * rr)
        obj = mesh(name, verts, grid_faces(len(points), cols, True), mat=inner,
                   subdiv=1, thickness=0.040, weather=0.005)
        obj["Cloth form"] = "Shoulder tension, inside-elbow compression, lower hanging folds and a thin turned cuff"
        obj["Wrist center"] = wrist
        return obj

    left = sleeve("06 | weighted left sleeve", -1)
    right = sleeve("07 | weighted right sleeve", 1)
    return left, right
