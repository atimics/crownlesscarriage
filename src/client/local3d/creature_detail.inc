static void DrawCreatureEllipsoid(Vector3 center, Vector3 radius, float yaw,
                                   Color color)
{
    DrawModelEx(sphere_models.character, center, (Vector3){0, 1, 0},
                yaw * RAD2DEG, radius, color);
}

static void DrawDoubleSidedTriangle(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a),
                                                         Vector3Subtract(c, a)));
    rlBegin(RL_TRIANGLES);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
    rlVertex3f(c.x, c.y, c.z);
    rlNormal3f(-normal.x, -normal.y, -normal.z);
    rlVertex3f(c.x, c.y, c.z);
    rlVertex3f(b.x, b.y, b.z);
    rlVertex3f(a.x, a.y, a.z);
    rlEnd();
    rlNormal3f(0, 1, 0);
}

/* Shared rings keep a continuous surface through the neck and tail bends. */
static void DrawCreatureTube(const Vector3 *points, const float *radii,
                              int32_t count, float scale, Color coat, Color belly)
{
    const int32_t sides = 10;
    Vector3 rings[2][10];
    Vector3 normals[2][10];
    rlBegin(RL_TRIANGLES);
    for (int32_t point = 0; point < count; ++point) {
        int32_t current = point % 2;
        Vector3 tangent = Vector3Normalize(Vector3Subtract(
            points[point + 1 < count ? point + 1 : point],
            points[point > 0 ? point - 1 : point]));
        Vector3 reference = fabsf(tangent.y) > 0.92f ? (Vector3){0, 0, 1} : (Vector3){0, 1, 0};
        Vector3 right = Vector3Normalize(Vector3CrossProduct(reference, tangent));
        Vector3 up = Vector3CrossProduct(tangent, right);
        for (int32_t side = 0; side < sides; ++side) {
            float angle = (float)side * 2.0f * PI / (float)sides;
            Vector3 normal = Vector3Add(Vector3Scale(right, cosf(angle)),
                                        Vector3Scale(up, sinf(angle)));
            normals[current][side] = normal;
            rings[current][side] = Vector3Add(points[point],
                Vector3Scale(normal, radii[point] * scale));
        }
        if (point == 0) continue;
        int32_t previous = 1 - current;
        for (int32_t side = 0; side < sides; ++side) {
            int32_t next = (side + 1) % sides;
            const int32_t rows[] = {previous, previous, current, previous, current, current};
            const int32_t columns[] = {side, next, next, side, next, side};
            for (int32_t vertex = 0; vertex < 6; ++vertex) {
                Vector3 normal = normals[rows[vertex]][columns[vertex]];
                Vector3 position = rings[rows[vertex]][columns[vertex]];
                Color color = normal.y < -0.25f ? belly : coat;
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlNormal3f(normal.x, normal.y, normal.z);
                rlVertex3f(position.x, position.y, position.z);
            }
        }
    }
    rlEnd();
    rlNormal3f(0, 1, 0);
}

static void DrawGoblinHair(CcCreatureVariant variant, Vector3 head, float yaw,
                            float scale, float sway, Color color)
{
    bool crest = variant == CC_CREATURE_GOBLIN_RAIDER;
    bool bunches = variant == CC_CREATURE_GOBLIN_TRIBUTE_BEARER;
    DrawCreatureEllipsoid(LocalPoint(head, 0, 0.20f * scale, -0.025f * scale, yaw),
        (Vector3){(crest ? 0.12f : 0.245f) * scale,
                  (bunches ? 0.11f : 0.16f) * scale, 0.20f * scale}, yaw, color);
    if (bunches) {
        for (int32_t side = -1; side <= 1; side += 2) {
            float sign = (float)side;
            Vector3 bunch = LocalPoint(head, sign * 0.22f * scale,
                0.37f * scale, -0.055f * scale, yaw);
            DrawCreatureEllipsoid(bunch,
                (Vector3){0.20f * scale, 0.23f * scale, 0.19f * scale}, yaw, color);
            Vector3 local[] = {
                {sign * 0.20f, 0.37f, -0.06f},
                {sign * 0.31f + sway, 0.60f, -0.08f},
                {sign * 0.21f + sway, 0.79f, -0.07f},
                {sign * 0.09f + sway, 0.78f, 0.005f},
                {sign * 0.09f + sway, 0.67f, 0.025f},
            };
            Vector3 points[5];
            for (int32_t point = 0; point < 5; ++point) {
                points[point] = LocalPoint(head, local[point].x * scale,
                    local[point].y * scale, local[point].z * scale, yaw);
            }
            const float radii[] = {0.145f, 0.16f, 0.115f, 0.060f, 0.003f};
            DrawCreatureTube(points, radii, 5, scale,
                BlendColor(color, WHITE, side < 0 ? 0.18f : 0.09f),
                ShadeColor(color, 0.75f));
        }
        return;
    }
    static const Vector3 roots[] = {
        {-0.16f, 0.15f, -0.04f}, {0.16f, 0.15f, -0.04f},
        {-0.10f, 0.22f, -0.04f}, {0.10f, 0.22f, -0.04f},
        {0.00f, 0.24f, -0.06f}, {-0.10f, 0.18f, 0.10f},
        {0.10f, 0.18f, 0.10f},
    };
    static const Vector3 tips[] = {
        {-0.35f, 0.49f, -0.13f}, {0.40f, 0.51f, -0.12f},
        {-0.02f, 0.78f, -0.10f}, {0.39f, 0.67f, -0.16f},
        {0.23f, 0.91f, -0.13f}, {-0.13f, 0.57f, 0.10f},
        {0.29f, 0.59f, 0.10f},
    };
    const float radii[] = {0.11f, 0.13f, 0.068f, 0.003f};
    int32_t lock_count = crest ? 5 : 7;
    for (int32_t lock = 0; lock < lock_count; ++lock) {
        Vector3 root = roots[lock];
        Vector3 tip = tips[lock];
        if (crest) {
            float row = (float)lock;
            root = (Vector3){0, 0.27f, 0.16f - row * 0.085f};
            tip = (Vector3){0.018f, 1.02f - row * 0.09f,
                             -0.13f - row * 0.095f};
        }
        tip.x += sway;
        Vector3 local[] = {root, Vector3Lerp(root, tip, 0.34f),
                           Vector3Lerp(root, tip, 0.72f), tip};
        Vector3 points[4];
        for (int32_t point = 0; point < 4; ++point) {
            points[point] = LocalPoint(head, local[point].x * scale,
                                       local[point].y * scale, local[point].z * scale, yaw);
        }
        Color highlight = BlendColor(color, WHITE,
                                      lock % 2 == 0 ? 0.23f : 0.08f);
        DrawCreatureTube(points, radii, 4, scale * (crest ? 0.72f : 1.0f),
                          highlight, ShadeColor(color, 0.75f));
    }
}

static void DrawDragonWing(Vector3 root, Vector3 joint, Vector3 tip, Vector3 back,
                            float scale, const CreatureRenderPalette *palette,
                            bool weathered)
{
    Vector3 fingers[] = {
        tip,
        Vector3Lerp(tip, back, 0.54f),
        back,
        Vector3Lerp(back, root, 0.72f),
    };
    fingers[1].y -= 0.12f * scale;
    Color membrane = BlendColor(palette->accent, palette->secondary, 0.28f);
    for (int32_t panel = 0; panel < 3; ++panel) {
        Vector3 scallop = Vector3Lerp(fingers[panel], fingers[panel + 1], 0.5f);
        scallop = Vector3Lerp(scallop, joint, weathered ? 0.38f : 0.20f);
        Color shade = ShadeColor(membrane, 1.0f - (float)panel * 0.08f);
        DrawDoubleSidedTriangle(joint, fingers[panel], scallop, shade);
        DrawDoubleSidedTriangle(joint, scallop, fingers[panel + 1], shade);
        DrawCylinderEx(joint, fingers[panel], 0.036f * scale, 0.010f * scale,
                       6, palette->hide);
    }
    DrawDoubleSidedTriangle(root, joint, fingers[3], membrane);
    DrawCylinderEx(root, joint, 0.10f * scale, 0.056f * scale, 7, palette->skin);
    DrawCylinderEx(joint, tip, 0.058f * scale, 0.016f * scale, 6, palette->skin);
    Vector3 thumb = Vector3Add(joint, Vector3Scale(Vector3Normalize(
        Vector3Subtract(joint, root)), 0.20f * scale));
    DrawCylinderEx(joint, thumb, 0.055f * scale, 0.0f, 6, palette->horn);
}

static void DrawDragonMane(Vector3 head, Vector3 radii, float yaw, Color hair)
{
    DrawCreatureEllipsoid(LocalPoint(head, 0, radii.y * 0.69f, -radii.z * 0.35f, yaw),
        (Vector3){radii.x * 0.86f, radii.y * 0.43f, radii.z * 0.62f}, yaw, hair);
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t side = -1; side <= 1; ++side) {
            float sign = (float)side;
            Vector3 root = {sign * radii.x * 0.46f, radii.y * 0.73f,
                            radii.z * (0.08f - (float)row * 0.35f)};
            Vector3 tip = {sign * radii.x * 1.04f,
                           radii.y * (side == 0 ? 2.15f : 1.66f),
                           root.z - radii.z * (0.62f + (float)row * 0.13f)};
            Vector3 local[] = {root, Vector3Lerp(root, tip, 0.35f),
                               Vector3Lerp(root, tip, 0.74f), tip};
            Vector3 points[4];
            for (int32_t point = 0; point < 4; ++point) {
                points[point] = LocalPoint(head, local[point].x,
                                           local[point].y, local[point].z, yaw);
            }
            const float widths[] = {0.26f, 0.30f, 0.15f, 0.006f};
            DrawCreatureTube(points, widths, 4, radii.x,
                BlendColor(hair, WHITE, side == 0 ? 0.20f : 0.08f),
                ShadeColor(hair, 0.75f));
        }
    }
}

static void DrawDragonFace(Vector3 head, Vector3 radii, float yaw, float scale,
                            const CreatureRenderPalette *palette, bool young)
{
    DrawCreatureEllipsoid(head, radii, yaw, palette->skin);
    DrawDragonMane(head, radii, yaw, palette->hair);
    Vector3 muzzle = LocalPoint(head, 0.0f, -radii.y * 0.24f, radii.z * 0.64f, yaw);
    DrawCreatureEllipsoid(muzzle,
        (Vector3){radii.x * 0.80f, radii.y * 0.57f, radii.z * 0.65f}, yaw,
        BlendColor(palette->skin, palette->hide, 0.45f));
    Vector3 jaw = LocalPoint(head, 0.0f, -radii.y * 0.63f, radii.z * 0.66f, yaw);
    DrawCreatureEllipsoid(jaw,
        (Vector3){radii.x * 0.76f, radii.y * 0.23f, radii.z * 0.65f}, yaw, palette->hide);
    Color dark = {15, 23, 22, 255};
    for (int32_t side = -1; side <= 1; side += 2) {
        float sign = (float)side;
        Vector3 eye = LocalPoint(head, sign * radii.x * 0.78f,
                                 radii.y * 0.25f, radii.z * 0.61f, yaw);
        float eye_size = radii.x * (young ? 0.24f : 0.22f);
        DrawCreatureEllipsoid(eye, (Vector3){eye_size, eye_size * 1.08f, eye_size * 0.62f},
                               yaw + sign * 0.62f, dark);
        Vector3 iris = LocalPoint(eye, sign * eye_size * 0.24f, 0.0f, eye_size * 0.44f, yaw);
        DrawCreatureEllipsoid(iris, (Vector3){eye_size * 0.75f, eye_size * 0.80f, eye_size * 0.33f},
                               yaw + sign * 0.62f, palette->eye);
        Vector3 pupil = LocalPoint(iris, sign * eye_size * 0.17f, 0.0f, eye_size * 0.27f, yaw);
        DrawCreatureEllipsoid(pupil,
            (Vector3){eye_size * (young ? 0.32f : 0.16f), eye_size * 0.63f, eye_size * 0.16f},
            yaw + sign * 0.62f, dark);
        Vector3 brow_in = LocalPoint(head, sign * radii.x * 0.48f,
                                     radii.y * 0.48f, radii.z * 0.67f, yaw);
        Vector3 brow_out = LocalPoint(head, sign * radii.x * 0.96f,
                                      radii.y * 0.57f, radii.z * 0.35f, yaw);
        DrawCylinderEx(brow_in, brow_out, eye_size * 0.58f, eye_size * 0.32f, 6, palette->hide);
        Vector3 nostril = LocalPoint(head, sign * radii.x * 0.43f,
                                     -radii.y * 0.07f, radii.z * 1.19f, yaw);
        DrawCreatureEllipsoid(nostril,
            (Vector3){radii.x * 0.105f, radii.y * 0.09f, radii.z * 0.04f}, yaw, dark);
        Vector3 mouth_back = LocalPoint(head, sign * radii.x * 0.72f,
                                        -radii.y * 0.44f, radii.z * 0.46f, yaw);
        Vector3 mouth_front = LocalPoint(head, sign * radii.x * 0.52f,
                                         -radii.y * 0.47f, radii.z * 1.17f, yaw);
        DrawCylinderEx(mouth_back, mouth_front, 0.012f * scale, 0.010f * scale, 5, dark);
        for (int32_t tooth = 0; tooth < 2; ++tooth) {
            Vector3 root = Vector3Lerp(mouth_back, mouth_front, 0.36f + (float)tooth * 0.38f);
            Vector3 tip = root;
            tip.y -= radii.y * (young ? 0.18f : 0.28f);
            DrawCylinderEx(root, tip, radii.x * 0.05f, 0.0f, 5, palette->horn);
        }
    }
}

static void DrawDragonClaws(Vector3 foot, float yaw, float scale,
                             const CreatureRenderPalette *palette)
{
    DrawCreatureEllipsoid(LocalPoint(foot, 0, 0.03f * scale, 0.05f * scale, yaw),
        (Vector3){0.16f * scale, 0.075f * scale, 0.22f * scale}, yaw, palette->skin);
    for (int32_t toe = -1; toe <= 1; ++toe) {
        Vector3 root = LocalPoint(foot, (float)toe * 0.10f * scale,
                                  0.035f * scale, 0.17f * scale, yaw);
        Vector3 tip = LocalPoint(foot, (float)toe * 0.14f * scale,
                                 0.008f * scale, (toe == 0 ? 0.35f : 0.31f) * scale, yaw);
        DrawCylinderEx(root, tip, 0.045f * scale, 0.0f, 6, palette->horn);
    }
}
