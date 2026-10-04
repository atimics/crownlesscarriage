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
    """Small connected forms give the mesh a soft classical expression."""
    depth = 0.0
    for side in (-1, 1):
        eye_x = side * (0.161 if side < 0 else 0.166)
        lift = 0.0025 if side > 0 else 0.0
        u = (x - eye_x) / 0.106
        # The closed lid sweeps down gently at the center of the eye.
        lid_z = 0.026 + 0.021 * u * u + lift
        eye_mask = math.exp(-0.5 * abs(u)**6)
        depth += 0.029 * _gauss(x, z, eye_x, 0.050 + lift, 0.110, 0.070)
        depth -= 0.027 * _gauss(x, z, eye_x, lid_z + 0.024, 0.089, 0.024)
        depth -= 0.011 * _gauss(x, z, eye_x, lid_z - 0.020, 0.098, 0.019)
        depth += 0.021 * eye_mask * math.exp(-0.5 * ((z - lid_z) / 0.0065)**2)
        # A shallow fold joins the upper lid to the brow plane.
        depth += 0.009 * eye_mask * math.exp(-0.5 * ((z - lid_z - 0.064) / 0.013)**2)
        brow_z = 0.149 + 0.030 * math.exp(-((x - eye_x + side * 0.021) / 0.091)**2)
        depth -= 0.026 * _gauss(x, z, eye_x, brow_z, 0.112, 0.043)
        depth -= 0.010 * _gauss(x, z, eye_x, brow_z + 0.011, 0.100, 0.014)
        # Cheekbones lead into full, relaxed cheeks and the upper lip.
        depth -= 0.048 * _gauss(x, z, side * 0.254, -0.107 + lift, 0.101, 0.100)
        depth -= 0.018 * _gauss(x, z, side * 0.205, -0.248, 0.099, 0.101)
        depth += 0.013 * _gauss(x, z, side * 0.330, 0.082, 0.048, 0.095)
        depth -= 0.019 * _gauss(x, z, side * 0.075, -0.199, 0.036, 0.039)
        # Nostrils sit under the rounded wings of a small nose.
        depth += 0.009 * _gauss(x, z, side * 0.061, -0.226, 0.017, 0.010)
        depth -= 0.006 * _gauss(x, z, side * 0.024, -0.279, 0.013, 0.039)

    # The bridge and tip blend into the forehead and cheeks.
    depth -= 0.041 * _gauss(x, z, 0.001, 0.025, 0.046, 0.143)
    depth -= 0.057 * _gauss(x, z, 0.001, -0.102, 0.041, 0.100)
    depth -= 0.101 * _gauss(x, z, 0.001, -0.170, 0.052, 0.046)
    depth += 0.011 * _gauss(x, z, 0.002, -0.239, 0.046, 0.013)
    depth += 0.006 * _gauss(x, z, 0.001, -0.281, 0.013, 0.028)

    # A small smile curves the mouth corners upwards.
    u = x / 0.137
    lip_z = -0.352 + 0.028 * u * u + 0.0015 * u
    mouth_mask = math.exp(-0.5 * abs(u)**6)
    cupid = 0.011 * math.exp(-((abs(x) - 0.036) / 0.025)**2)
    depth -= 0.019 * mouth_mask * math.exp(-0.5 * ((z - lip_z - 0.017 - cupid) / 0.014)**2)
    depth += 0.016 * mouth_mask * math.exp(-0.5 * ((z - lip_z) / 0.0055)**2)
    depth -= 0.027 * mouth_mask * math.exp(-0.5 * ((z - lip_z + 0.024) / 0.019)**2)
    depth += 0.006 * _gauss(x, z, 0, -0.414, 0.112, 0.020)
    depth -= 0.032 * _gauss(x, z, 0.001, -0.495, 0.126, 0.066)
    return depth


def build_face(mesh, sphere, skin):
    """Build the bowed head and return its single editable surface.

    ``mesh`` and ``sphere`` follow the main statue builder's helper API.
    The mesh uses the same center and bow as the hood and neck study.
    """
    center = (0.015, -0.17, 5.22)
    bow = math.radians(9)
    widths = [(-0.67, 0), (-0.61, 0.126), (-0.54, 0.208),
              (-0.42, 0.273), (-0.27, 0.327), (-0.10, 0.383),
              (0.12, 0.389), (0.31, 0.374), (0.47, 0.324),
              (0.59, 0.222), (0.67, 0)]
    fronts = [(-0.67, 0), (-0.61, 0.180), (-0.51, 0.271),
              (-0.35, 0.321), (-0.12, 0.334), (0.13, 0.349),
              (0.32, 0.344), (0.49, 0.289), (0.59, 0.191), (0.67, 0)]
    backs = [(-0.67, 0), (-0.56, 0.187), (-0.30, 0.310),
             (0.04, 0.372), (0.31, 0.366), (0.48, 0.311),
             (0.59, 0.208), (0.67, 0)]

    def head_point(x, y, z):
        return (center[0] + x,
                center[1] + y * math.cos(bow) - z * math.sin(bow),
                center[2] + y * math.sin(bow) + z * math.cos(bow))

    rows, cols = 222, 256
    verts = [head_point(0, 0, -0.67)]
    rings = []
    for i in range(1, rows):
        z = -0.67 + 1.34 * i / rows
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
            face_weight = max(0, -cosine)**4
            y += _face_depth(x, z) * face_weight
            x += 0.0022 * face_weight * math.sin(8.0 * z + 0.3)
            y += 0.0012 * face_weight * math.sin(12 * x + 6 * z)
            verts.append(head_point(x, y, z))
    top_index = len(verts)
    verts.append(head_point(0, 0, 0.67))
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
    head["Sculpt notes"] = "Closed lids, soft cheekbones, small smile, and slight natural asymmetry."
    head["Editable surface"] = "One continuous mesh. Facial forms are part of the head surface."
    return head
