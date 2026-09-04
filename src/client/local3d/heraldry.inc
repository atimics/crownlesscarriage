/* Procedural banners for kingdoms, factions, and other powers. */
static void DrawHeraldryTriangle(Vector3 first, Vector3 second,
                                 Vector3 third, Color color)
{
    DrawTriangle3D(first, second, third, color);
    DrawTriangle3D(third, second, first, ShadeColor(color, 0.92f));
}

static void DrawHeraldryQuad(Vector3 upper_left, Vector3 upper_right,
                             Vector3 lower_right, Vector3 lower_left,
                             Color color)
{
    DrawHeraldryTriangle(upper_left, lower_right, lower_left, color);
    DrawHeraldryTriangle(upper_left, upper_right, lower_right, color);
}

static void DrawHeraldryDiamond(float x, float y, float z, float radius,
                                Color color)
{
    Vector3 top = {x, y + radius, z};
    Vector3 right = {x + radius, y, z};
    Vector3 bottom = {x, y - radius, z};
    Vector3 left = {x - radius, y, z};
    DrawHeraldryTriangle(top, right, bottom, color);
    DrawHeraldryTriangle(top, bottom, left, color);
}

static void DrawHeraldryCloth(CcHeraldryStyle style, float tip_wave)
{
    const float left = 0.10f;
    const float top = 3.22f;
    const float bottom = 1.54f;
    const float front = 0.035f;
    Color fold = BlendColor(style.field, style.shadow, 0.20f);
    if (style.shape == CC_BANNER_COURT) {
        DrawHeraldryQuad(
            (Vector3){left, top, 0.0f}, (Vector3){0.96f, top, front},
            (Vector3){0.96f, bottom, front},
            (Vector3){left, bottom, 0.0f}, style.field);
        DrawHeraldryQuad(
            (Vector3){0.96f, top, front},
            (Vector3){1.82f, top, tip_wave * 0.45f},
            (Vector3){1.82f, bottom, tip_wave * 0.45f},
            (Vector3){0.96f, bottom, front}, fold);
        DrawHeraldryQuad(
            (Vector3){1.82f, top, tip_wave * 0.45f},
            (Vector3){2.66f, top, tip_wave},
            (Vector3){2.66f, bottom, tip_wave},
            (Vector3){1.82f, bottom, tip_wave * 0.45f}, style.field);
    } else if (style.shape == CC_BANNER_FACTORS) {
        DrawHeraldryQuad(
            (Vector3){left, top, 0.0f},
            (Vector3){1.76f, top, tip_wave * 0.40f},
            (Vector3){1.76f, bottom, tip_wave * 0.40f},
            (Vector3){left, bottom, 0.0f}, style.field);
        DrawHeraldryTriangle(
            (Vector3){1.76f, top, tip_wave * 0.40f},
            (Vector3){2.66f, top, tip_wave},
            (Vector3){2.12f, 2.38f, tip_wave * 0.74f}, fold);
        DrawHeraldryTriangle(
            (Vector3){1.76f, bottom, tip_wave * 0.40f},
            (Vector3){2.12f, 2.38f, tip_wave * 0.74f},
            (Vector3){2.66f, bottom, tip_wave}, style.field);
    } else if (style.shape == CC_BANNER_COMMONS) {
        DrawHeraldryQuad(
            (Vector3){left, top, 0.0f},
            (Vector3){1.78f, top, tip_wave * 0.42f},
            (Vector3){1.78f, bottom, tip_wave * 0.42f},
            (Vector3){left, bottom, 0.0f}, style.field);
        DrawHeraldryQuad(
            (Vector3){1.78f, top, tip_wave * 0.42f},
            (Vector3){2.66f, top, tip_wave},
            (Vector3){2.66f, 2.42f, tip_wave},
            (Vector3){1.78f, 2.42f, tip_wave * 0.42f}, fold);
        DrawHeraldryQuad(
            (Vector3){1.78f, 2.42f, tip_wave * 0.42f},
            (Vector3){2.30f, 2.42f, tip_wave * 0.76f},
            (Vector3){2.30f, bottom, tip_wave * 0.76f},
            (Vector3){1.78f, bottom, tip_wave * 0.42f}, style.field);
    } else {
        DrawHeraldryQuad(
            (Vector3){left, top, 0.0f},
            (Vector3){1.62f, top, tip_wave * 0.36f},
            (Vector3){1.62f, bottom, tip_wave * 0.36f},
            (Vector3){left, bottom, 0.0f}, style.field);
        DrawHeraldryTriangle(
            (Vector3){1.62f, top, tip_wave * 0.36f},
            (Vector3){2.58f, 3.10f, tip_wave},
            (Vector3){1.62f, 2.58f, tip_wave * 0.36f}, fold);
        DrawHeraldryTriangle(
            (Vector3){1.62f, 2.58f, tip_wave * 0.36f},
            (Vector3){2.32f, 2.30f, tip_wave * 0.78f},
            (Vector3){1.62f, 2.04f, tip_wave * 0.36f}, style.field);
        DrawHeraldryTriangle(
            (Vector3){1.62f, 2.04f, tip_wave * 0.36f},
            (Vector3){2.50f, 1.66f, tip_wave},
            (Vector3){1.62f, bottom, tip_wave * 0.36f}, fold);
    }
}

static void DrawHeraldryPattern(CcHeraldryStyle style)
{
    const float front = 0.072f;
    if (style.shape == CC_BANNER_COURT) {
        DrawCubeV((Vector3){0.19f, 2.38f, front},
                  (Vector3){0.16f, 1.50f, 0.045f}, style.accent);
        DrawCubeV((Vector3){1.28f, 3.12f, front},
                  (Vector3){2.04f, 0.10f, 0.045f}, style.accent);
        DrawCubeV((Vector3){1.28f, 1.64f, front},
                  (Vector3){2.04f, 0.10f, 0.045f}, style.accent);
    } else if (style.shape == CC_BANNER_FACTORS) {
        DrawCylinderEx((Vector3){0.30f, 1.70f, front},
                       (Vector3){1.72f, 3.02f, front},
                       0.105f, 0.105f, 5, style.accent);
        DrawCubeV((Vector3){0.22f, 2.38f, front},
                  (Vector3){0.14f, 1.48f, 0.045f}, style.accent);
    } else if (style.shape == CC_BANNER_COMMONS) {
        DrawCubeV((Vector3){0.42f, 2.38f, front},
                  (Vector3){0.48f, 1.52f, 0.045f}, style.accent);
        DrawCubeV((Vector3){1.48f, 1.78f, front},
                  (Vector3){0.40f, 0.26f, 0.045f}, style.accent);
        DrawCubeV((Vector3){1.72f, 2.92f, front},
                  (Vector3){0.28f, 0.22f, 0.045f}, style.shadow);
    } else {
        DrawCubeV((Vector3){0.23f, 2.38f, front},
                  (Vector3){0.18f, 1.48f, 0.045f}, style.accent);
        DrawCubeV((Vector3){1.72f, 2.77f, front},
                  (Vector3){0.56f, 0.25f, 0.045f}, style.accent);
    }
}

static void DrawRoadGrainMark(float x, float y, float z,
                              CcHeraldryStyle style)
{
    DrawCylinderEx((Vector3){x, y - 0.35f, z},
                   (Vector3){x, y + 0.08f, z},
                   0.055f, 0.055f, 5, style.device);
    DrawCylinderEx((Vector3){x, y - 0.02f, z},
                   (Vector3){x - 0.31f, y + 0.27f, z},
                   0.052f, 0.035f, 5, style.device);
    DrawCylinderEx((Vector3){x, y - 0.02f, z},
                   (Vector3){x + 0.31f, y + 0.27f, z},
                   0.052f, 0.035f, 5, style.device);
    DrawSphere((Vector3){x, y - 0.40f, z}, 0.105f, style.accent);
}

static void DrawIronWallMark(float x, float y, float z,
                             CcHeraldryStyle style)
{
    DrawCubeV((Vector3){x, y - 0.10f, z},
              (Vector3){0.82f, 0.38f, 0.055f}, style.device);
    for (int32_t merlon = -1; merlon <= 1; ++merlon) {
        DrawCubeV((Vector3){x + (float)merlon * 0.30f, y + 0.18f, z},
                  (Vector3){0.18f, 0.25f, 0.055f}, style.device);
    }
    DrawCubeV((Vector3){x, y - 0.12f, z + 0.035f},
              (Vector3){0.16f, 0.34f, 0.065f}, style.shadow);
}

static void DrawDeepStarMark(float x, float y, float z,
                             CcHeraldryStyle style)
{
    DrawHeraldryDiamond(x, y + 0.20f, z, 0.26f, style.device);
    for (int32_t step = 0; step < 3; ++step) {
        float width = 0.56f - (float)step * 0.15f;
        DrawCubeV((Vector3){x, y - 0.08f - (float)step * 0.16f, z},
                  (Vector3){width, 0.09f, 0.055f}, style.accent);
    }
}

static void DrawCinderMark(float x, float y, float z,
                           CcHeraldryStyle style)
{
    DrawCylinderEx((Vector3){x, y, z - 0.035f},
                   (Vector3){x, y, z + 0.035f},
                   0.31f, 0.31f, 9, style.device);
    DrawCylinderEx((Vector3){x, y, z - 0.070f},
                   (Vector3){x, y, z + 0.070f},
                   0.15f, 0.15f, 8, style.field);
    for (int32_t tooth = 0; tooth < 4; ++tooth) {
        float side = tooth < 2 ? -1.0f : 1.0f;
        float lift = (tooth & 1) == 0 ? -0.20f : 0.20f;
        DrawCubeV((Vector3){x + side * 0.36f, y + lift, z},
                  (Vector3){0.16f, 0.18f, 0.055f}, style.accent);
    }
}

static void DrawBanditPatchMark(float x, float y, float z,
                                CcHeraldryStyle style)
{
    DrawCubeV((Vector3){x, y, z - 0.015f},
              (Vector3){0.72f, 0.66f, 0.045f}, style.accent);
    DrawCylinderEx((Vector3){x - 0.30f, y - 0.28f, z + 0.035f},
                   (Vector3){x + 0.30f, y + 0.28f, z + 0.035f},
                   0.042f, 0.042f, 5, style.device);
    DrawCylinderEx((Vector3){x - 0.30f, y + 0.28f, z + 0.035f},
                   (Vector3){x + 0.30f, y - 0.28f, z + 0.035f},
                   0.042f, 0.042f, 5, style.device);
}

static void DrawLedgerMark(float x, float y, float z,
                           CcHeraldryStyle style)
{
    for (int32_t side = -1; side <= 1; side += 2) {
        rlPushMatrix();
        rlTranslatef(x + (float)side * 0.20f, y, z);
        rlRotatef((float)side * 9.0f, 0.0f, 0.0f, 1.0f);
        DrawCubeV((Vector3){0.0f, 0.0f, 0.0f},
                  (Vector3){0.38f, 0.62f, 0.055f}, style.device);
        rlPopMatrix();
    }
    DrawCubeV((Vector3){x, y, z + 0.045f},
              (Vector3){0.07f, 0.88f, 0.075f}, style.accent);
}

static void DrawDragonCrownMark(float x, float y, float z,
                                CcHeraldryStyle style)
{
    DrawCubeV((Vector3){x, y - 0.20f, z},
              (Vector3){0.84f, 0.13f, 0.055f}, style.accent);
    for (int32_t spike = -2; spike <= 2; ++spike) {
        float height = spike == 0 ? 0.48f :
                       abs(spike) == 1 ? 0.36f : 0.25f;
        DrawCylinderEx((Vector3){x + (float)spike * 0.18f,
                                 y - 0.15f, z},
                       (Vector3){x + (float)spike * 0.20f,
                                 y - 0.15f + height, z},
                       0.045f, 0.018f, 5, style.accent);
    }
    DrawSphere((Vector3){x, y - 0.02f, z + 0.035f},
               0.11f, style.device);
}

static void DrawHeraldryMark3D(CcHeraldryStyle style)
{
    const float x = 1.23f;
    const float y = 2.38f;
    const float z = 0.115f;
    switch (style.mark) {
        case CC_BANNER_MARK_ROAD_GRAIN:
            DrawRoadGrainMark(x, y, z, style);
            break;
        case CC_BANNER_MARK_IRON_WALL:
            DrawIronWallMark(x, y, z, style);
            break;
        case CC_BANNER_MARK_DEEP_STAR:
            DrawDeepStarMark(x, y, z, style);
            break;
        case CC_BANNER_MARK_CINDER_TITHE:
            DrawCinderMark(x, y, z, style);
            break;
        case CC_BANNER_MARK_BANDIT_PATCH:
            DrawBanditPatchMark(x, y, z, style);
            break;
        case CC_BANNER_MARK_IRON_LEDGER:
            DrawLedgerMark(x, y, z, style);
            break;
        case CC_BANNER_MARK_DRAGON_CROWN:
            DrawDragonCrownMark(x, y, z, style);
            break;
    }
}

static void DrawHeraldryWear(CcHeraldryStyle style)
{
    if (style.wear <= 0) return;
    DrawCubeV((Vector3){1.72f, 1.83f, 0.095f},
              (Vector3){0.38f, 0.27f, 0.045f}, style.shadow);
    DrawCylinderEx((Vector3){1.56f, 1.72f, 0.125f},
                   (Vector3){1.88f, 1.94f, 0.125f},
                   0.026f, 0.026f, 4, style.device);
    if (style.wear < 2) return;
    DrawCubeV((Vector3){0.66f, 2.93f, 0.095f},
              (Vector3){0.34f, 0.22f, 0.045f}, style.accent);
}

static void DrawHeraldryBanner3D(CcHeraldryStyle style, Vector3 base,
                                 float yaw_degrees, float scale, float clock)
{
    int32_t step = (int32_t)floorf(clock * 2.5f + (float)style.mark);
    float tip_wave = ((float)((step % 3 + 3) % 3) - 1.0f) * 0.075f;
    rlPushMatrix();
    rlTranslatef(base.x, base.y, base.z);
    rlRotatef(yaw_degrees, 0.0f, 1.0f, 0.0f);
    rlScalef(scale, scale, scale);
    DrawCylinderEx((Vector3){0.0f, 0.0f, 0.0f},
                   (Vector3){0.0f, 3.58f, 0.0f},
                   0.065f, 0.050f, 7, WORLD_WOOD_SHADOW);
    DrawCylinderEx((Vector3){0.0f, 3.25f, 0.0f},
                   (Vector3){2.12f, 3.25f, tip_wave * 0.62f},
                   0.042f, 0.032f, 6, WORLD_METAL_SHADOW);
    DrawSphere((Vector3){0.0f, 3.66f, 0.0f}, 0.10f, style.accent);
    DrawHeraldryCloth(style, tip_wave);
    DrawHeraldryPattern(style);
    DrawHeraldryMark3D(style);
    DrawHeraldryWear(style);
    rlPopMatrix();
}
