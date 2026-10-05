"""An editable stone face with a calm, closed-eye expression."""

import math


def _gauss(x, z, cx, cz, sx, sz):
    return math.exp(-0.5 * (((x - cx) / sx) ** 2 + ((z - cz) / sz) ** 2))


def _profile(points, z):
    """Interpolate the head sections with a smooth cubic curve."""
    if z <= points[0][0]:
        return points[0][1]
    if z >= points[-1][0]:
        return points[-1][1]
    for i in range(len(points) - 1):
        a, b = points[i], points[i + 1]
        if z <= b[0]:
            previous = points[max(0, i - 1)]
            following = points[min(len(points) - 1, i + 2)]
            slope_a = (b[1] - previous[1]) / (b[0] - previous[0])
            slope_b = (following[1] - a[1]) / (following[0] - a[0])
            span = b[0] - a[0]
            t = (z - a[0]) / span
            return ((2 * t**3 - 3 * t**2 + 1) * a[1]
                    + (t**3 - 2 * t**2 + t) * span * slope_a
                    + (-2 * t**3 + 3 * t**2) * b[1]
                    + (t**3 - t**2) * span * slope_b)


def _face_depth(x, z):
    """Shape connected eyelids, cheek planes, nose wings and relaxed lips."""
    z -= 0.045
    depth = 0.0
    for side in (-1, 1):
        eye_x = side * (0.161 if side < 0 else 0.166)
        lift = 0.0025 if side > 0 else 0.0
        u = (x - eye_x) / 0.106
        # The closed lid sweeps down gently at the center of the eye.
        lid_z = 0.026 + 0.021 * u * u + lift
        eye_mask = math.exp(-0.5 * abs(u)**6)
        depth += 0.028 * _gauss(x, z, eye_x, 0.066 + lift, 0.107, 0.072)
        depth -= 0.019 * _gauss(x, z, eye_x, lid_z + 0.020, 0.091, 0.024)
        depth -= 0.008 * _gauss(x, z, eye_x, lid_z - 0.020, 0.095, 0.024)
        depth += 0.015 * eye_mask * math.exp(-0.5 * ((z - lid_z) / 0.0055)**2)
        # A shallow fold joins the upper lid to the brow plane.
        depth += 0.005 * eye_mask * math.exp(-0.5 * ((z - lid_z - 0.060) / 0.016)**2)
        brow_z = 0.146 + 0.018 * math.exp(-((x - eye_x + side * 0.021) / 0.091)**2)
        depth -= 0.023 * _gauss(x, z, eye_x, brow_z, 0.114, 0.057)
        # Cheekbones lead into full, relaxed cheeks and the upper lip.
        depth -= 0.067 * _gauss(x, z, side * 0.249, -0.105 + lift, 0.097, 0.069)
        depth -= 0.023 * _gauss(x, z, side * 0.204, -0.204, 0.098, 0.088)
        depth += 0.019 * _gauss(x, z, side * 0.275, -0.281, 0.062, 0.090)
        depth += 0.017 * _gauss(x, z, side * 0.329, 0.087, 0.046, 0.090)
        depth += 0.006 * _gauss(x, z, side * 0.094, -0.274, 0.022, 0.058)
        depth -= 0.031 * _gauss(x, z, side * 0.068, -0.181, 0.037, 0.037)
        # Nostrils sit under the rounded wings of a small nose.
        depth += 0.023 * _gauss(x, z, side * 0.060, -0.204, 0.017, 0.009)
        depth += 0.009 * _gauss(x, z, side * 0.091, -0.174, 0.012, 0.025)
        depth -= 0.007 * _gauss(x, z, side * 0.023, -0.269, 0.012, 0.034)

    # The bridge and tip blend into the forehead and cheeks.
    depth -= 0.036 * _gauss(x, z, 0.001, 0.040, 0.053, 0.134)
    depth -= 0.060 * _gauss(x, z, 0.001, -0.069, 0.045, 0.090)
    depth -= 0.102 * _gauss(x, z, 0.001, -0.151, 0.050, 0.040)
    depth -= 0.014 * _gauss(x, z, 0.002, -0.206, 0.014, 0.025)
    depth += 0.013 * _gauss(x, z, 0.002, -0.228, 0.043, 0.013)
    depth += 0.007 * _gauss(x, z, 0.001, -0.269, 0.012, 0.026)

    # A small smile curves the mouth corners upwards.
    u = x / 0.133
    lip_z = -0.333 + 0.017 * u * u + 0.0015 * u
    mouth_mask = math.exp(-0.5 * abs(u)**6)
    cupid = 0.011 * math.exp(-((abs(x) - 0.036) / 0.025)**2)
    depth -= 0.023 * mouth_mask * math.exp(-0.5 * ((z - lip_z - 0.015 - cupid) / 0.014)**2)
    depth += 0.020 * mouth_mask * math.exp(-0.5 * ((z - lip_z) / 0.0045)**2)
    depth -= 0.028 * mouth_mask * math.exp(-0.5 * ((z - lip_z + 0.023) / 0.020)**2)
    for side in (-1, 1):
        depth += 0.015 * _gauss(x, z, side * 0.132, -0.314 + side * 0.0015, 0.016, 0.014)
        depth -= 0.008 * _gauss(x, z, side * 0.149, -0.343, 0.027, 0.027)
    depth += 0.010 * _gauss(x, z, 0, -0.397, 0.100, 0.022)
    depth -= 0.035 * _gauss(x, z, 0.001, -0.483, 0.131, 0.068)
    return depth


def build_face(mesh, sphere, skin):
    """Build the bowed head and return its single editable surface.

    ``mesh`` and ``sphere`` follow the main statue builder's helper API.
    A slight bow and side tilt give the quiet expression a natural posture.
    """
    center = (0.015, -0.17, 5.22)
    bow = math.radians(12)
    tilt = math.radians(-4)
    widths = [(-0.67, 0), (-0.61, 0.126), (-0.54, 0.208),
              (-0.42, 0.281), (-0.27, 0.326), (-0.10, 0.387),
              (0.12, 0.384), (0.29, 0.367), (0.44, 0.312),
              (0.55, 0.214), (0.63, 0)]
    fronts = [(-0.67, 0), (-0.61, 0.180), (-0.51, 0.271),
              (-0.35, 0.325), (-0.12, 0.331), (0.13, 0.333),
              (0.30, 0.323), (0.45, 0.266), (0.55, 0.180), (0.63, 0)]
    backs = [(-0.67, 0), (-0.56, 0.187), (-0.30, 0.310),
             (0.04, 0.372), (0.29, 0.361), (0.45, 0.297),
             (0.55, 0.199), (0.63, 0)]

    def head_point(x, y, z):
        tilted_x = x * math.cos(tilt) + z * math.sin(tilt)
        tilted_z = -x * math.sin(tilt) + z * math.cos(tilt)
        return (center[0] + tilted_x,
                center[1] + y * math.cos(bow) - tilted_z * math.sin(bow),
                center[2] + y * math.sin(bow) + tilted_z * math.cos(bow))

    rows, cols = 222, 256
    verts = [head_point(0, 0, -0.67)]
    rings = []
    for i in range(1, rows):
        z = -0.67 + 1.30 * i / rows
        width = _profile(widths, z)
        front_depth = _profile(fronts, z)
        back_depth = _profile(backs, z)
        # Wider ring spacing at the tips keeps the sculpt surface watertight.
        ring_cols = min(cols, max(16, int(2 * math.pi * width / 0.0018)))
        rings.append((len(verts), ring_cols))
        for j in range(ring_cols):
            phi = 2 * math.pi * j / ring_cols
            sine, cosine = math.sin(phi), math.cos(phi)
            x = width * sine
            y = (front_depth if cosine < 0 else back_depth) * cosine
            face_weight = max(0, -cosine)**2
            y += _face_depth(x, z) * face_weight
            x += 0.0022 * face_weight * math.sin(8.0 * z + 0.3)
            y += 0.0012 * face_weight * math.sin(12 * x + 6 * z)
            verts.append(head_point(x, y, z))
    top_index = len(verts)
    verts.append(head_point(0, 0, 0.63))
    faces = []
    first, first_cols = rings[0]
    for j in range(first_cols):
        faces.append((0, first + (j + 1) % first_cols, first + j))
    for (a, a_cols), (b, b_cols) in zip(rings, rings[1:]):
        j, k = 0, 0
        while j < a_cols or k < b_cols:
            next_a, next_b = (j + 1) / a_cols, (k + 1) / b_cols
            aj, bk = a + j % a_cols, b + k % b_cols
            if abs(next_a - next_b) < 1e-8:
                faces.append((aj, a + (j + 1) % a_cols,
                              b + (k + 1) % b_cols, bk))
                j, k = j + 1, k + 1
            elif next_a < next_b:
                faces.append((aj, a + (j + 1) % a_cols, bk))
                j += 1
            else:
                faces.append((aj, b + (k + 1) % b_cols, bk))
                k += 1
    last, last_cols = rings[-1]
    for j in range(last_cols):
        faces.append((last + j, last + (j + 1) % last_cols, top_index))
    head = mesh("04 | angelic bowed face", verts, faces, mat=skin, weather=0.0015)
    head["Sculpt notes"] = "Soft closed lids, clear cheek planes, rounded nose wings, lip corners, and a gentle bowed tilt."
    head["Editable surface"] = "One continuous mesh. Facial forms are part of the head surface."
    return head
