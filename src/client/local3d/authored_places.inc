static void DrawSphereModel(Model model, Vector3 center, float radius, Color color)
{
    DrawModelEx(model, center, (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                (Vector3){radius, radius, radius}, color);
}


static void DrawSmallSphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.small, center, radius, color);
}

static void DrawCharacterSphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.character, center, radius, color);
}

static void DrawScenerySphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.scenery, center, radius, color);
}

static void DrawBox(Vector3 center, Vector3 size, Color color)
{
    // rlgl shares the four corners of each face through its quad index buffer.
    // Add the center here so each box shares its parent's transform.
    Vector3 low = Vector3Subtract(center, Vector3Scale(size, 0.5f));
    Vector3 high = Vector3Add(center, Vector3Scale(size, 0.5f));
    // Batched vertices already include their transforms. Flush a full batch
    // with identity matrices so its lighting uses those world coordinates.
    rlPushMatrix();
    rlLoadIdentity();
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    (void)rlCheckRenderBatchLimit(24);
    rlPopMatrix();
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlVertex3f(low.x, high.y, high.z);
    rlVertex3f(low.x, low.y, high.z);
    rlVertex3f(high.x, low.y, high.z);
    rlVertex3f(high.x, high.y, high.z);
    rlNormal3f(0.0f, 0.0f, -1.0f);
    rlVertex3f(high.x, low.y, low.z);
    rlVertex3f(low.x, low.y, low.z);
    rlVertex3f(low.x, high.y, low.z);
    rlVertex3f(high.x, high.y, low.z);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlVertex3f(low.x, high.y, low.z);
    rlVertex3f(low.x, high.y, high.z);
    rlVertex3f(high.x, high.y, high.z);
    rlVertex3f(high.x, high.y, low.z);
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlVertex3f(high.x, low.y, high.z);
    rlVertex3f(low.x, low.y, high.z);
    rlVertex3f(low.x, low.y, low.z);
    rlVertex3f(high.x, low.y, low.z);
    rlNormal3f(1.0f, 0.0f, 0.0f);
    rlVertex3f(high.x, low.y, low.z);
    rlVertex3f(high.x, high.y, low.z);
    rlVertex3f(high.x, high.y, high.z);
    rlVertex3f(high.x, low.y, high.z);
    rlNormal3f(-1.0f, 0.0f, 0.0f);
    rlVertex3f(low.x, high.y, high.z);
    rlVertex3f(low.x, high.y, low.z);
    rlVertex3f(low.x, low.y, low.z);
    rlVertex3f(low.x, low.y, high.z);
    rlEnd();
}

static Color ShadeColor(Color color, float scale)
{
    return (Color){
        (unsigned char)fmaxf(0.0f, fminf(255.0f, color.r * scale)),
        (unsigned char)fmaxf(0.0f, fminf(255.0f, color.g * scale)),
        (unsigned char)fmaxf(0.0f, fminf(255.0f, color.b * scale)),
        color.a
    };
}

static Color BlendColor(Color from, Color to, float amount)
{
    amount = fmaxf(0.0f, fminf(1.0f, amount));
    return (Color){
        (unsigned char)lroundf(from.r + (to.r - from.r) * amount),
        (unsigned char)lroundf(from.g + (to.g - from.g) * amount),
        (unsigned char)lroundf(from.b + (to.b - from.b) * amount),
        (unsigned char)lroundf(from.a + (to.a - from.a) * amount)
    };
}

static void DrawTiltedBox(Vector3 center, Vector3 size, Vector3 axis,
                          float degrees, Color color)
{
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(degrees, axis.x, axis.y, axis.z);
    DrawBox((Vector3){0.0f, 0.0f, 0.0f}, size, color);
    rlPopMatrix();
}

static float DrawPitchedRoof(float x, float z, float width, float depth,
                             float wall_height, Color wall, Color roof)
{
    const float overhang = 0.42f;
    const float rise = 1.10f + fminf(width, depth) * 0.14f;
    bool ridge_along_x = width >= depth;
    Color shadow = ShadeColor(roof, 0.72f);


    DrawBox((Vector3){x + width * 0.5f, wall_height - 0.06f,
                      z + depth * 0.5f},
            (Vector3){width + overhang * 2.18f, 0.24f,
                      depth + overhang * 2.18f},
            BlendColor(ShadeColor(roof, 0.66f), wall, 0.16f));
    float half_span = (ridge_along_x ? depth : width) * 0.5f + overhang;
    float slope_length = sqrtf(half_span * half_span + rise * rise);
    float angle = atanf(rise / half_span) * RAD2DEG;
    for (int32_t side = -1; side <= 1; side += 2) {
        float sign = (float)side;
        Vector3 center = {
            x + width * 0.5f +
                (ridge_along_x ? 0.0f : sign * half_span * 0.5f),
            wall_height + rise * 0.5f,
            z + depth * 0.5f +
                (ridge_along_x ? sign * half_span * 0.5f : 0.0f),
        };
        Vector3 size = ridge_along_x ?
            (Vector3){width + overhang * 2.0f, 0.22f, slope_length} :
            (Vector3){slope_length, 0.22f, depth + overhang * 2.0f};
        Vector3 axis = ridge_along_x ? (Vector3){1.0f, 0.0f, 0.0f} :
                                          (Vector3){0.0f, 0.0f, 1.0f};
        float plane_angle = ridge_along_x ? sign * angle : -sign * angle;
        DrawTiltedBox(center, size, axis, plane_angle,
                      side > 0 ? roof : shadow);


        for (int32_t batten = 1; batten <= 3; ++batten) {
            float amount = (float)batten / 4.0f;
            float run = half_span * amount;
            float batten_height = rise * (1.0f - amount);
            Vector3 batten_center = {
                x + width * 0.5f +
                    (ridge_along_x ? 0.0f : sign * run),
                wall_height + batten_height + 0.10f,
                z + depth * 0.5f +
                    (ridge_along_x ? sign * run : 0.0f),
            };
            Vector3 batten_size = ridge_along_x ?
                (Vector3){width + overhang * 2.04f, 0.09f, 0.15f} :
                (Vector3){0.15f, 0.09f, depth + overhang * 2.04f};
            DrawBox(batten_center, batten_size,
                    ShadeColor(roof, side > 0 ? 0.74f : 0.58f));
        }
    }
    DrawBox((Vector3){x + width * 0.5f, wall_height + rise + 0.06f,
                      z + depth * 0.5f},
            ridge_along_x ?
                (Vector3){width + overhang * 2.10f, 0.18f, 0.24f} :
                (Vector3){0.24f, 0.18f, depth + overhang * 2.10f},
            ShadeColor(roof, 0.62f));
    return rise;
}

static void DrawConstructedWallShell(float x, float z, float width,
                                     float depth, float height,
                                     Color wall, Color trim, int32_t style)
{
    const float inset = 0.18f;
    const float base_height = fminf(1.06f, height * 0.20f);
    float upper_height = height - base_height;
    float corner = fmaxf(0.40f, fminf(0.58f,
        fminf(width, depth) * 0.072f));
    Color lower = ShadeColor(wall, 0.78f);
    Color side = ShadeColor(wall, 0.90f);


    DrawBox((Vector3){x + width * 0.5f, base_height * 0.5f,
                      z + depth * 0.5f},
            (Vector3){width, base_height, depth}, lower);
    DrawBox((Vector3){x + width * 0.5f,
                      base_height + upper_height * 0.5f,
                      z + depth * 0.5f},
            (Vector3){width - inset * 2.0f, upper_height,
                      depth - inset * 2.0f}, wall);

    for (int32_t corner_index = 0; corner_index < 4; ++corner_index) {
        float corner_x = (corner_index & 1) == 0 ?
            x + corner * 0.5f : x + width - corner * 0.5f;
        float corner_z = (corner_index & 2) == 0 ?
            z + corner * 0.5f : z + depth - corner * 0.5f;
        DrawBox((Vector3){corner_x, height * 0.50f, corner_z},
                (Vector3){corner, height, corner},
                corner_index == 3 ? trim : ShadeColor(trim, 0.90f));
    }

    int32_t front_bays = width >= 10.0f ? 3 : 2;
    float front_gap = 0.16f;
    float front_available = width - corner * 2.0f;
    float front_bay_width =
        (front_available - front_gap * (float)(front_bays - 1)) /
        (float)front_bays;
    for (int32_t bay = 0; bay < front_bays; ++bay) {
        float bay_x = x + corner + front_bay_width * ((float)bay + 0.5f) +
            front_gap * (float)bay;
        float projection = ((bay + style) & 1) != 0 ? 0.30f : 0.22f;
        DrawBox((Vector3){bay_x,
                          base_height + upper_height * 0.5f,
                          z + depth - inset * 0.30f + projection * 0.18f},
                (Vector3){front_bay_width, upper_height - 0.14f,
                          inset + projection},
                ((bay + style) & 1) != 0 ? wall : ShadeColor(wall, 0.96f));
    }

    int32_t side_bays = depth >= 9.0f ? 3 : 2;
    float side_available = depth - corner * 2.0f;
    float side_bay_depth =
        (side_available - front_gap * (float)(side_bays - 1)) /
        (float)side_bays;
    for (int32_t bay = 0; bay < side_bays; ++bay) {
        float bay_z = z + corner + side_bay_depth * ((float)bay + 0.5f) +
            front_gap * (float)bay;
        float projection = ((bay + style) & 1) != 0 ? 0.24f : 0.16f;
        DrawBox((Vector3){x + width - inset * 0.30f + projection * 0.18f,
                          base_height + upper_height * 0.5f, bay_z},
                (Vector3){inset + projection, upper_height - 0.14f,
                          side_bay_depth},
                ((bay + style) & 1) != 0 ? side : ShadeColor(side, 0.96f));
        DrawBox((Vector3){x + inset * 0.30f - projection * 0.18f,
                          base_height + upper_height * 0.5f, bay_z},
                (Vector3){inset + projection, upper_height - 0.14f,
                          side_bay_depth},
                ((bay + style) & 1) != 0 ? ShadeColor(side, 0.86f) :
                                           ShadeColor(side, 0.92f));
    }


    if (style == 0 || style == 3) {
        float storey = fminf(2.18f, height * 0.43f);
        DrawBox((Vector3){x + width * 0.5f, storey,
                          z + depth + 0.18f},
                (Vector3){width + 0.24f, 0.22f, 0.42f}, trim);
        DrawBox((Vector3){x + width + 0.18f, storey,
                          z + depth * 0.5f},
                (Vector3){0.42f, 0.22f, depth + 0.24f},
                ShadeColor(trim, 0.88f));
        DrawBox((Vector3){x - 0.18f, storey,
                          z + depth * 0.5f},
                (Vector3){0.42f, 0.22f, depth + 0.24f},
                ShadeColor(trim, 0.80f));
    }
}

static void DrawRoofDormer(float x, float z, float width, float depth,
                           float wall_height, float roof_rise,
                           Color wall, Color roof, Color trim)
{
    bool ridge_along_x = width >= depth;
    Color dormer_wall = BlendColor(wall, (Color){128, 116, 96, 255}, 0.16f);
    Color dormer_roof = ShadeColor(roof, 0.88f);
    Color glass = BlendColor((Color){31, 53, 57, 255}, WORLD_TEAL, 0.26f);
    if (ridge_along_x) {
        float center_x = x + width * 0.67f;
        float center_z = z + depth * 0.78f;
        float roof_surface = wall_height + roof_rise * 0.43f;
        DrawBox((Vector3){center_x, roof_surface + 0.37f, center_z},
                (Vector3){1.34f, 0.82f, 0.58f}, dormer_wall);
        DrawBox((Vector3){center_x, roof_surface + 0.40f,
                          center_z + 0.31f},
                (Vector3){0.52f, 0.46f, 0.045f}, glass);
        DrawBox((Vector3){center_x, roof_surface + 0.14f,
                          center_z + 0.34f},
                (Vector3){0.74f, 0.09f, 0.10f}, trim);
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawTiltedBox(
                (Vector3){center_x, roof_surface + 0.90f,
                          center_z + (float)side * 0.18f},
                (Vector3){1.62f, 0.10f, 0.50f},
                (Vector3){1.0f, 0.0f, 0.0f}, (float)side * 31.0f,
                side > 0 ? dormer_roof : ShadeColor(dormer_roof, 0.76f));
        }
    } else {
        float center_x = x + width * 0.78f;
        float center_z = z + depth * 0.67f;
        float roof_surface = wall_height + roof_rise * 0.43f;
        DrawBox((Vector3){center_x, roof_surface + 0.37f, center_z},
                (Vector3){0.58f, 0.82f, 1.34f}, dormer_wall);
        DrawBox((Vector3){center_x + 0.31f, roof_surface + 0.40f,
                          center_z},
                (Vector3){0.045f, 0.46f, 0.52f}, glass);
        DrawBox((Vector3){center_x + 0.34f, roof_surface + 0.14f,
                          center_z},
                (Vector3){0.10f, 0.09f, 0.74f}, trim);
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawTiltedBox(
                (Vector3){center_x + (float)side * 0.18f,
                          roof_surface + 0.90f, center_z},
                (Vector3){0.50f, 0.10f, 1.62f},
                (Vector3){0.0f, 0.0f, 1.0f}, (float)-side * 31.0f,
                side > 0 ? dormer_roof : ShadeColor(dormer_roof, 0.76f));
        }
    }
}

static void DrawFacadeBrace(Vector3 center, bool side_facing,
                            float length, float degrees, Color color)
{
    Vector3 size = side_facing ? (Vector3){0.11f, length, 0.16f} :
                                 (Vector3){0.16f, length, 0.11f};
    DrawTiltedBox(center, size,
                  side_facing ? (Vector3){1.0f, 0.0f, 0.0f} :
                                (Vector3){0.0f, 0.0f, 1.0f},
                  degrees, color);
}

static void DrawBuildingArchetypeDetails(float x, float z, float width,
                                         float depth, float height,
                                         Color wall, Color trim,
                                         int32_t style)
{
    float front_z = z + depth + 0.105f;
    float side_x = x + width + 0.105f;
    if (style == 1) {

        Color stone = ShadeColor(wall, 1.16f);
        for (int32_t course = 0; course < 5; ++course) {
            float y = 0.47f + (float)course * (height - 0.56f) / 4.0f;
            float inset = (course & 1) != 0 ? 0.08f : 0.0f;
            DrawBox((Vector3){x + width - 0.16f - inset, y, front_z},
                    (Vector3){0.48f, 0.52f, 0.15f}, stone);
            DrawBox((Vector3){side_x, y, z + depth - 0.16f - inset},
                    (Vector3){0.15f, 0.52f, 0.48f},
                    ShadeColor(stone, 0.90f));
        }
        DrawBox((Vector3){x + width * 0.50f, height * 0.70f, front_z},
                (Vector3){width * 0.50f, 0.16f, 0.15f}, trim);
        return;
    }

    if (style == 2) {

        Color pilaster = BlendColor(ShadeColor(wall, 0.62f), trim, 0.18f);
        for (int32_t bay = 0; bay < 3; ++bay) {
            float post_x = x + width * (0.18f + (float)bay * 0.32f);
            DrawBox((Vector3){post_x, height * 0.53f, front_z},
                    (Vector3){0.22f, height * 0.76f, 0.15f}, pilaster);
        }
        DrawBox((Vector3){x + width * 0.50f, height * 0.69f, front_z + 0.04f},
                (Vector3){width * 0.76f, 0.22f, 0.24f},
                ShadeColor(pilaster, 1.10f));
        DrawBox((Vector3){side_x, height * 0.69f, z + depth * 0.50f},
                (Vector3){0.24f, 0.22f, depth * 0.76f}, pilaster);
        DrawBox((Vector3){x + width * 0.50f, height * 0.69f + 0.13f,
                          front_z + 0.17f},
                (Vector3){width * 0.46f, 0.055f, 0.055f}, trim);
        return;
    }


    Color timber = style == 3 ? ShadeColor(trim, 0.76f) : trim;
    int32_t bay_count = style == 3 ? 3 : 2;
    for (int32_t bay = 1; bay <= bay_count; ++bay) {
        float amount = (float)bay / (float)(bay_count + 1);
        DrawBox((Vector3){x + width * amount, height * 0.51f, front_z},
                (Vector3){0.16f, height * 0.82f, 0.14f}, timber);
    }
    DrawBox((Vector3){side_x, height * 0.51f, z + depth * 0.44f},
            (Vector3){0.14f, height * 0.82f, 0.16f},
            ShadeColor(timber, 0.91f));
    float brace_y = fminf(height * 0.68f, 3.65f);
    float front_brace = fminf(width * 0.22f, 2.30f);
    DrawFacadeBrace((Vector3){x + width * 0.18f, brace_y, front_z + 0.01f},
                    false, front_brace, -42.0f, timber);
    DrawFacadeBrace((Vector3){x + width * 0.82f, brace_y, front_z + 0.01f},
                    false, front_brace, 42.0f, timber);
    if (style == 3) {
        float side_brace = fminf(depth * 0.24f, 2.15f);
        DrawFacadeBrace((Vector3){side_x + 0.01f, brace_y,
                                  z + depth * 0.76f},
                        true, side_brace, -40.0f,
                        ShadeColor(timber, 0.88f));
    }
}

static void DrawFacadeWindow(Vector3 center, int32_t side, Color trim,
                             Color glass)
{
    bool side_facing = side != 0;
    float side_sign = side < 0 ? -1.0f : 1.0f;
    Vector3 recess = side_facing ? (Vector3){0.075f, 1.34f, 1.02f} :
                                   (Vector3){1.02f, 1.34f, 0.075f};
    Vector3 pane = side_facing ? (Vector3){0.035f, 1.02f, 0.72f} :
                                 (Vector3){0.72f, 1.02f, 0.035f};
    DrawBox(center, recess, ShadeColor(trim, 0.48f));
    DrawBox(center, pane, glass);
    Vector3 vertical = side_facing ? (Vector3){0.022f, 1.06f, 0.055f} :
                                     (Vector3){0.055f, 1.06f, 0.022f};
    Vector3 horizontal = side_facing ? (Vector3){0.022f, 0.055f, 0.76f} :
                                       (Vector3){0.76f, 0.055f, 0.022f};
    DrawBox(center, vertical, trim);
    DrawBox(center, horizontal, trim);
    Vector3 sill_center = {center.x, center.y - 0.70f, center.z};
    Vector3 lintel_center = {center.x, center.y + 0.70f, center.z};
    if (side_facing) {
        sill_center.x += side_sign * 0.025f;
        lintel_center.x += side_sign * 0.025f;
    } else {
        sill_center.z += 0.025f;
        lintel_center.z += 0.025f;
    }
    Vector3 sill = side_facing ? (Vector3){0.10f, 0.11f, 1.12f} :
                                 (Vector3){1.12f, 0.11f, 0.10f};
    Vector3 lintel = side_facing ? (Vector3){0.08f, 0.15f, 1.04f} :
                                   (Vector3){1.04f, 0.15f, 0.08f};
    DrawBox(sill_center, sill, ShadeColor(trim, 1.08f));
    DrawBox(lintel_center, lintel, ShadeColor(trim, 0.82f));


    float outward = 0.10f;
    Vector3 jamb_a = center;
    Vector3 jamb_b = center;
    Vector3 jamb_size;
    if (side_facing) {
        jamb_a.x += side_sign * outward;
        jamb_b.x += side_sign * outward;
        jamb_a.z -= 0.49f;
        jamb_b.z += 0.49f;
        jamb_size = (Vector3){0.24f, 1.36f, 0.14f};
    } else {
        jamb_a.z += outward;
        jamb_b.z += outward;
        jamb_a.x -= 0.49f;
        jamb_b.x += 0.49f;
        jamb_size = (Vector3){0.14f, 1.36f, 0.24f};
    }
    DrawBox(jamb_a, jamb_size, ShadeColor(trim, 0.86f));
    DrawBox(jamb_b, jamb_size, ShadeColor(trim, 0.92f));
}

static void DrawBuildingContactShadow(float x, float z, float width,
                                      float depth, float height)
{
    float reach = 0.18f + fminf(height, 10.0f) * 0.045f;
    DrawCubeV((Vector3){x + width * 0.5f + reach * 0.42f, -0.012f,
                        z + depth * 0.5f + reach * 0.32f},
              (Vector3){width + reach, 0.025f, depth + reach * 0.82f},
              (Color){8, 13, 14, 78});
}

static void DrawBuildingFoundation(float x, float z, float width,
                                   float depth, float height, Color wall)
{
    DrawBuildingContactShadow(x, z, width, depth, height);
    Color footing = ShadeColor(wall, 0.48f);
    Color cap = ShadeColor(wall, 0.63f);

    DrawBox((Vector3){x + width * 0.5f, 0.18f,
                      z + depth * 0.5f},
            (Vector3){width + 0.20f, 0.52f, depth + 0.20f}, footing);
    DrawBox((Vector3){x + width * 0.5f, 0.47f,
                      z + depth * 0.5f},
            (Vector3){width + 0.30f, 0.12f, depth + 0.30f}, cap);


    for (int32_t course = 0; course < 2; ++course) {
        float y = 0.13f + (float)course * 0.25f;
        float block_width = 0.92f;
        int32_t front_blocks = (int32_t)ceilf((width + 0.20f) / block_width);
        for (int32_t block = 0; block < front_blocks; ++block) {
            float start = x - 0.10f - (course != 0 ? block_width * 0.5f : 0.0f);
            float block_x = start + ((float)block + 0.5f) * block_width;
            if (block_x < x - 0.12f || block_x > x + width + 0.12f) continue;
            DrawBox((Vector3){block_x, y, z + depth + 0.065f},
                    (Vector3){block_width - 0.045f, 0.22f, 0.17f},
                    course == 0 ? footing : cap);
        }
        int32_t side_blocks = (int32_t)ceilf((depth + 0.20f) / block_width);
        for (int32_t block = 0; block < side_blocks; ++block) {
            float start = z - 0.10f - (course == 0 ? block_width * 0.5f : 0.0f);
            float block_z = start + ((float)block + 0.5f) * block_width;
            if (block_z < z - 0.12f || block_z > z + depth + 0.12f) continue;
            DrawBox((Vector3){x + width + 0.065f, y, block_z},
                    (Vector3){0.17f, 0.22f, block_width - 0.045f},
                    course == 0 ? ShadeColor(footing, 0.92f) :
                                  ShadeColor(cap, 0.92f));
        }
    }
}

static void DrawBuildingRevealCap(float x, float z, float width, float depth,
                                  float height, float local_cut_height,
                                  Color wall, Color roof)
{
    if (local_cut_height < 0.58f || local_cut_height > height + 0.24f) return;
    float y = local_cut_height - 0.055f;
    Color cap = BlendColor(ShadeColor(wall, 0.54f), roof, 0.30f);
    Color ink = ShadeColor(cap, 0.58f);
    DrawBox((Vector3){x + width * 0.5f, y, z + 0.02f},
            (Vector3){width + 0.18f, 0.10f, 0.18f}, cap);
    DrawBox((Vector3){x + width * 0.5f, y, z + depth - 0.02f},
            (Vector3){width + 0.18f, 0.10f, 0.18f}, cap);
    DrawBox((Vector3){x + 0.02f, y, z + depth * 0.5f},
            (Vector3){0.18f, 0.10f, depth - 0.12f}, cap);
    DrawBox((Vector3){x + width - 0.02f, y, z + depth * 0.5f},
            (Vector3){0.18f, 0.10f, depth - 0.12f}, cap);
    DrawBox((Vector3){x + width * 0.5f, y + 0.052f, z + depth - 0.10f},
            (Vector3){width - 0.18f, 0.025f, 0.055f}, ink);
}

static void DrawBuilding(float x, float z, float width, float depth,
                         float height, Color wall, Color roof, bool door,
                         int32_t style)
{
    Vector3 center = {x + width * 0.5f, height * 0.5f, z + depth * 0.5f};
    if (distant_building_view) {
        // Keep the authored walls and roofline legible from the road.
        DrawCubeV(center, (Vector3){width, height, depth}, wall);
        (void)DrawPitchedRoof(x, z, width, depth, height, wall, roof);
        return;
    }
    Color trim = style == 2 ? WORLD_GOLD : ShadeColor(wall, 0.60f);
    Color glass = style == 1 ? (Color){148, 103, 55, 255} :
                  style == 3 ? Fade(WORLD_VIOLET, 0.84f) :
                               Fade(WORLD_TEAL, 0.78f);
    DrawConstructedWallShell(x, z, width, depth, height, wall, trim, style);


    for (int32_t corner = 0; corner < 2; ++corner) {
        float post_x = corner == 0 ? x + 0.18f : x + width - 0.18f;
        DrawBox((Vector3){post_x, height * 0.50f, z + depth + 0.045f},
                (Vector3){0.18f, height - 0.18f, 0.09f}, trim);
    }
    DrawBox((Vector3){center.x, height - 0.24f, z + depth + 0.045f},
            (Vector3){width - 0.18f, 0.18f, 0.09f}, trim);
    DrawBox((Vector3){x + width + 0.045f, height - 0.24f, center.z},
            (Vector3){0.09f, 0.18f, depth - 0.18f}, trim);
    DrawBox((Vector3){x - 0.045f, height - 0.24f, center.z},
            (Vector3){0.09f, 0.18f, depth - 0.18f},
            ShadeColor(trim, 0.86f));


    for (int32_t corner = 0; corner < 2; ++corner) {
        float post_x = corner == 0 ? x + 0.18f : x + width - 0.18f;
        DrawBox((Vector3){post_x, height * 0.50f, z - 0.045f},
                (Vector3){0.18f, height - 0.18f, 0.09f}, trim);
    }
    DrawBox((Vector3){center.x, height - 0.24f, z - 0.045f},
            (Vector3){width - 0.18f, 0.18f, 0.09f}, trim);
    DrawBox((Vector3){x - 0.045f, height - 0.24f, center.z},
            (Vector3){0.09f, 0.18f, depth - 0.18f}, trim);


    Color plinth = ShadeColor(wall, 0.50f);
    float story_course = fminf(2.16f, height * 0.42f);
    DrawBox((Vector3){center.x, 0.46f, z + depth + 0.052f},
            (Vector3){width - 0.14f, 0.42f, 0.105f}, plinth);
    DrawBox((Vector3){x + width + 0.052f, 0.46f, center.z},
            (Vector3){0.105f, 0.42f, depth - 0.14f}, plinth);
    DrawBox((Vector3){x - 0.052f, 0.46f, center.z},
            (Vector3){0.105f, 0.42f, depth - 0.14f},
            ShadeColor(plinth, 0.84f));
    DrawBox((Vector3){center.x, story_course, z + depth + 0.057f},
            (Vector3){width - 0.16f, 0.13f, 0.115f}, trim);
    DrawBox((Vector3){x + width + 0.057f, story_course, center.z},
            (Vector3){0.115f, 0.13f, depth - 0.16f}, trim);
    DrawBox((Vector3){center.x, 0.46f, z - 0.052f},
            (Vector3){width - 0.14f, 0.42f, 0.105f}, plinth);
    DrawBox((Vector3){center.x, story_course, z - 0.057f},
            (Vector3){width - 0.16f, 0.13f, 0.115f}, trim);
    DrawBox((Vector3){x - 0.057f, story_course, center.z},
            (Vector3){0.115f, 0.13f, depth - 0.16f},
            ShadeColor(trim, 0.86f));

    int32_t bays = width >= 10.0f ? 3 : 2;
    float window_y = fminf(height * 0.56f, 2.90f);
    for (int32_t bay = 0; bay < bays; ++bay) {
        float window_x = x + width * (float)(bay + 1) / (float)(bays + 1);
        if (door && fabsf(window_x - center.x) < 0.82f) continue;
        DrawFacadeWindow((Vector3){window_x, window_y,
                                   z + depth + 0.075f}, 0, trim, glass);
    }
    DrawFacadeWindow((Vector3){x + width + 0.075f, window_y,
                               center.z + depth * 0.12f}, 1, trim, glass);
    for (int32_t bay = 0; bay < bays; ++bay) {
        float window_x = x + width * (float)(bay + 1) / (float)(bays + 1);
        DrawFacadeWindow((Vector3){window_x, window_y, z - 0.075f},
                         0, trim, ShadeColor(glass, 0.88f));
    }
    DrawFacadeWindow((Vector3){x - 0.075f, window_y,
                               center.z - depth * 0.12f}, -1,
                     ShadeColor(trim, 0.88f), ShadeColor(glass, 0.82f));

    if (door) {
        DrawBox((Vector3){center.x, 0.10f, z + depth + 0.42f},
                (Vector3){1.58f, 0.20f, 0.76f},
                ShadeColor(wall, 0.54f));
        DrawBox((Vector3){center.x, 0.045f, z + depth + 0.84f},
                (Vector3){1.34f, 0.09f, 0.38f},
                ShadeColor(wall, 0.66f));
        DrawBox((Vector3){center.x, 1.10f, z + depth + 0.055f},
                (Vector3){1.02f, 2.10f, 0.055f},
                (Color){43, 34, 37, 255});
        DrawBox((Vector3){center.x - 0.67f, 1.18f,
                          z + depth + 0.24f},
                (Vector3){0.24f, 2.36f, 0.52f}, trim);
        DrawBox((Vector3){center.x + 0.67f, 1.18f,
                          z + depth + 0.24f},
                (Vector3){0.24f, 2.36f, 0.52f},
                ShadeColor(trim, 0.92f));
        DrawBox((Vector3){center.x, 2.30f, z + depth + 0.24f},
                (Vector3){1.58f, 0.24f, 0.52f}, trim);
        DrawSmallSphere((Vector3){center.x + 0.39f, 1.075f,
                                  z + depth + 0.11f},
                        0.035f, WORLD_GOLD);
        DrawBox((Vector3){center.x, 2.52f, z + depth + 0.48f},
                (Vector3){1.82f, 0.18f, 0.92f}, roof);
        float lantern_x = center.x + 0.94f;
        DrawBox((Vector3){lantern_x, 1.92f, z + depth + 0.12f},
                (Vector3){0.08f, 0.48f, 0.08f}, trim);
        DrawBox((Vector3){lantern_x, 2.15f, z + depth + 0.18f},
                (Vector3){0.30f, 0.08f, 0.20f}, trim);
        DrawSmallSphere((Vector3){lantern_x, 1.88f, z + depth + 0.22f},
                        0.11f, WORLD_GOLD);
    }

    DrawBuildingArchetypeDetails(x, z, width, depth, height, wall, trim,
                                 style);

    float roof_rise = DrawPitchedRoof(x, z, width, depth, height, wall, roof);
    if (style != 1 && width >= 7.5f && depth >= 6.5f) {
        DrawRoofDormer(x, z, width, depth, height, roof_rise,
                       wall, roof, trim);
    }
    if (style == 0 || style == 2) {
        DrawBox((Vector3){x + width * 0.24f,
                          height + roof_rise * 0.74f,
                          z + depth * 0.36f},
                (Vector3){0.48f, 1.28f, 0.48f},
                ShadeColor(wall, 0.52f));
        DrawBox((Vector3){x + width * 0.24f,
                          height + roof_rise * 0.74f + 0.67f,
                          z + depth * 0.36f},
                (Vector3){0.62f, 0.14f, 0.62f}, trim);
    }
    if (style == 2) {
        for (int32_t side = -1; side <= 1; side += 2) {
            float side_sign = (float)side;
            DrawBox((Vector3){center.x + side_sign * width * 0.27f,
                              height * 0.72f, z + depth + 0.12f},
                    (Vector3){0.72f, 1.75f, 0.06f},
                    side < 0 ? WORLD_GOLD : WORLD_TEAL);
        }
    }
}

static void DrawOpenRoofBay(float x, float z, float width, float depth,
                            float height, Color timber, Color roof)
{
    float post_inset = 0.24f;
    for (int32_t corner = 0; corner < 4; ++corner) {
        float post_x = (corner & 1) == 0 ? x + post_inset :
                                             x + width - post_inset;
        float post_z = (corner & 2) == 0 ? z + post_inset :
                                             z + depth - post_inset;
        DrawBox((Vector3){post_x, height * 0.50f, post_z},
                (Vector3){0.28f, height, 0.28f}, timber);
    }
    DrawBox((Vector3){x + width * 0.50f, height - 0.12f,
                      z + depth * 0.50f},
            (Vector3){width, 0.24f, depth}, ShadeColor(timber, 0.82f));
    DrawPitchedRoof(x, z, width, depth, height,
                    ShadeColor(timber, 0.86f), roof);
}

static void DrawRoundTower(float center_x, float center_z, float radius,
                           float height, Color wall, Color roof, bool door)
{
    Color trim = ShadeColor(wall, 0.62f);
    DrawCylinder((Vector3){center_x, 0.0f, center_z},
                 radius, radius * 1.03f, height, 12, wall);
    DrawCylinder((Vector3){center_x, height, center_z},
                 0.10f, radius * 1.22f, radius * 1.12f, 12, roof);
    DrawBox((Vector3){center_x, height * 0.58f,
                      center_z + radius + 0.025f},
            (Vector3){radius * 0.62f, 0.72f, 0.07f}, WORLD_INK);
    DrawBox((Vector3){center_x, height * 0.58f,
                      center_z + radius + 0.055f},
            (Vector3){0.08f, 0.78f, 0.11f}, trim);
    if (door) {
        DrawBox((Vector3){center_x, 1.05f,
                          center_z + radius + 0.035f},
                (Vector3){0.82f, 2.10f, 0.08f}, WORLD_WOOD_SHADOW);
    }
}


static void DrawSettlementBuilding(const WorldBuilding *building,
                                   Color wall, Color roof,
                                   const CcLocalPlaceProfile *profile,
                                   int32_t building_index)
{
    if (building == NULL || profile == NULL) return;
    float x = building->footprint.x;
    float z = building->footprint.y;
    float width = building->footprint.width;
    float depth = building->footprint.height;
    float height = building->height;
    bool mirror = (building_index & 1) != 0;

    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            if (building->style == CC_LOCAL_BUILDING_WORKSHOP ||
                building_index < 0) {

                float aisle = width * 0.20f;
                DrawBuilding(x + aisle, z, width - aisle * 2.0f, depth,
                             height, wall, roof, building->door,
                             building->style);
                DrawOpenRoofBay(x, z + depth * 0.12f, aisle * 1.18f,
                                depth * 0.78f, height * 0.48f,
                                WORLD_WOOD_SHADOW, ShadeColor(roof, 0.88f));
                DrawOpenRoofBay(x + width - aisle * 1.18f,
                                z + depth * 0.12f, aisle * 1.18f,
                                depth * 0.78f, height * 0.48f,
                                WORLD_WOOD_SHADOW, ShadeColor(roof, 0.92f));
            } else {
                float main_width = width * 0.70f;
                float main_x = mirror ? x + width - main_width : x;
                float annex_width = width * 0.38f;
                float annex_x = mirror ? x : x + width - annex_width;
                DrawBuilding(main_x, z, main_width, depth, height,
                             wall, roof, building->door, building->style);
                DrawBuilding(annex_x, z + depth * 0.24f,
                             annex_width, depth * 0.68f, height * 0.60f,
                             ShadeColor(wall, 0.94f), ShadeColor(roof, 0.90f),
                             false, building->style);
            }
            break;
        case CC_SETTLEMENT_MINING: {

            float lower_height = height * 0.48f;
            DrawBuilding(x, z, width, depth, lower_height,
                         ShadeColor(wall, 0.82f), ShadeColor(roof, 0.78f),
                         building->door, 1);
            float upper_width = width * 0.76f;
            float upper_x = x + (mirror ? width * 0.08f : width * 0.16f);
            rlPushMatrix();
            rlTranslatef(0.0f, height * 0.43f, 0.0f);
            DrawBuilding(upper_x, z + depth * 0.10f,
                         upper_width, depth * 0.80f, height * 0.54f,
                         wall, roof, false, building->style);
            rlPopMatrix();
            float gantry_x = mirror ? x + width * 0.16f :
                                      x + width * 0.84f;
            DrawBox((Vector3){gantry_x, height * 0.58f, z + depth * 0.50f},
                    (Vector3){0.28f, height * 1.08f, depth * 0.88f},
                    WORLD_WOOD_SHADOW);
            DrawBox((Vector3){gantry_x, height * 1.08f, z + depth * 0.50f},
                    (Vector3){width * 0.28f, 0.26f, depth * 0.88f},
                    WORLD_WOOD);
            break;
        }
        case CC_SETTLEMENT_MARKET: {

            float wing_width = width * 0.34f;
            DrawBuilding(x, z, wing_width, depth, height * 0.72f,
                         wall, roof, building->door, building->style);
            DrawBuilding(x + width - wing_width, z, wing_width, depth,
                         height * 0.72f, wall, roof, false, building->style);
            DrawBuilding(x + width * 0.31f, z,
                         width * 0.38f, depth * 0.52f, height,
                         ShadeColor(wall, 1.06f), roof, false, 2);
            int32_t bays = 3;
            for (int32_t bay = 0; bay < bays; ++bay) {
                float post_x = x + width * (0.37f + (float)bay * 0.13f);
                DrawBox((Vector3){post_x, height * 0.28f,
                                  z + depth * 0.88f},
                        (Vector3){0.24f, height * 0.56f, 0.30f},
                        WORLD_STONE_LIGHT);
            }
            DrawBox((Vector3){x + width * 0.50f, height * 0.57f,
                              z + depth * 0.88f},
                    (Vector3){width * 0.38f, 0.24f, 0.38f}, WORLD_GOLD);
            break;
        }
        case CC_SETTLEMENT_FORTRESS: {
            float tower_radius = fminf(width * 0.15f, depth * 0.24f);
            float tower_x = mirror ? x + tower_radius :
                                     x + width - tower_radius;
            float block_x = mirror ? x + width * 0.18f : x;
            DrawBuilding(block_x, z, width * 0.82f, depth,
                         height * 0.70f, wall, roof, building->door,
                         building->style);
            DrawRoundTower(tower_x, z + depth * 0.54f, tower_radius,
                           height * 1.06f, ShadeColor(wall, 0.88f),
                           roof, false);
            break;
        }
        case CC_SETTLEMENT_CAPITAL:

            DrawBuilding(x, z + depth * 0.10f, width * 0.31f,
                         depth * 0.90f, height * 0.62f,
                         wall, ShadeColor(roof, 0.92f), false,
                         building->style);
            DrawBuilding(x + width * 0.69f, z + depth * 0.10f,
                         width * 0.31f, depth * 0.90f, height * 0.62f,
                         wall, ShadeColor(roof, 0.92f), false,
                         building->style);
            DrawBuilding(x + width * 0.27f, z, width * 0.46f, depth,
                         height * 1.04f, ShadeColor(wall, 1.06f), roof,
                         building->door, 2);
            break;
        case CC_SETTLEMENT_DUNGEON_TOWN: {
            float main_width = width * 0.68f;
            float main_x = mirror ? x + width - main_width : x;
            float annex_x = mirror ? x : x + width * 0.54f;
            DrawBuilding(main_x, z, main_width, depth, height,
                         wall, roof, building->door, building->style);
            DrawBuilding(annex_x, z + depth * 0.32f,
                         width * 0.46f, depth * 0.60f, height * 0.58f,
                         ShadeColor(wall, 0.80f), ShadeColor(roof, 0.84f),
                         false, 3);
            float tower_x = mirror ? x + width * 0.26f :
                                     x + width * 0.74f;
            DrawRoundTower(tower_x, z + depth * 0.24f,
                           fminf(width, depth) * 0.15f,
                           height * 0.92f, ShadeColor(wall, 0.72f),
                           ShadeColor(roof, 0.76f), false);
            break;
        }
    }
}

static void DrawTerrainPatchAtTop(float x, float z, float width, float depth,
                                  float top, Color color)
{
    const float thickness = 0.060f;
    DrawCubeV((Vector3){x + width * 0.5f, top - thickness * 0.5f,
                        z + depth * 0.5f},
              (Vector3){width, thickness, depth}, color);
}


static bool SceneryFootprintVisibleWithin(Rectangle footprint, Vector3 focus,
                                          float visibility_radius)
{
    if (scenery_view.valid) {
        float ground = TerrainFootprintHeight(footprint);
        Vector3 center = {footprint.x + footprint.width * 0.5f,
                           ground + 12.0f,
                           footprint.y + footprint.height * 0.5f};
        Vector3 half = {footprint.width * 0.5f + 3.0f, 16.0f,
                        footprint.height * 0.5f + 3.0f};
        return CameraBoundsVisible(scenery_view.camera, center, half,
            (float)scenery_view.width / (float)scenery_view.height, 512.0f);
    }
    float nearest_x = fmaxf(footprint.x,
                            fminf(focus.x, footprint.x + footprint.width));
    float nearest_z = fmaxf(footprint.y,
                            fminf(focus.z, footprint.y + footprint.height));
    float delta_x = focus.x - nearest_x;
    float delta_z = focus.z - nearest_z;
    return delta_x * delta_x + delta_z * delta_z <=
           visibility_radius * visibility_radius;
}

static bool SceneryFootprintVisible(Rectangle footprint, Vector3 focus)
{
    return SceneryFootprintVisibleWithin(footprint, focus, 42.0f);
}

static bool SceneryPointVisible(float x, float z, Vector3 focus)
{
    return SceneryFootprintVisible((Rectangle){x, z, 0.0f, 0.0f}, focus);
}

static bool RoomDetailPointVisible(float x, float z, Vector3 focus)
{
    return SceneryFootprintVisibleWithin(
        (Rectangle){x, z, 0.0f, 0.0f}, focus, 23.0f);
}

static void DrawStreetLantern(float x, float z,
                              CcLocalRoadSurface surface)
{
    Color metal = (Color){43, 43, 39, 255};
    Color glow = WORLD_GOLD;
    float post_height = 2.14f;
    switch (surface) {
        case CC_LOCAL_ROAD_FARM_TRACK:
            metal = WORLD_WOOD;
            glow = WORLD_CROP_LIGHT;
            post_height = 1.78f;
            break;
        case CC_LOCAL_ROAD_INDUSTRIAL:
            metal = WORLD_METAL_SHADOW;
            glow = WORLD_EARTH_LIGHT;
            post_height = 1.92f;
            break;
        case CC_LOCAL_ROAD_TRADE:
            metal = WORLD_WOOD_SHADOW;
            glow = WORLD_GOLD;
            break;
        case CC_LOCAL_ROAD_MILITARY:
            metal = WORLD_STONE_SHADOW;
            glow = WORLD_DANGER;
            post_height = 2.30f;
            break;
        case CC_LOCAL_ROAD_PROCESSIONAL:
            metal = ShadeColor(WORLD_GOLD, 0.54f);
            glow = WORLD_GOLD;
            post_height = 2.48f;
            break;
        case CC_LOCAL_ROAD_EXPEDITION:
            metal = WORLD_WOOD_SHADOW;
            glow = WORLD_VIOLET;
            post_height = 1.88f;
            break;
    }
    float lantern_y = post_height - 0.16f;
    DrawCylinder((Vector3){x, 0.05f, z}, 0.23f, 0.28f, 0.10f, 8,
                 ShadeColor(metal, 0.72f));
    DrawCylinder((Vector3){x, post_height * 0.50f, z},
                 0.055f, 0.075f, post_height, 8, metal);
    DrawBox((Vector3){x, post_height + 0.04f, z},
            (Vector3){0.42f, 0.08f, 0.42f}, metal);
    DrawSmallSphere((Vector3){x, lantern_y, z}, 0.18f, glow);
    DrawBox((Vector3){x, lantern_y - 0.02f, z},
            (Vector3){0.27f, 0.30f, 0.27f}, Fade(glow, 0.82f));
    DrawBox((Vector3){x, lantern_y - 0.22f, z},
            (Vector3){0.34f, 0.07f, 0.34f}, metal);
    if (surface == CC_LOCAL_ROAD_PROCESSIONAL) {
        DrawSmallSphere((Vector3){x, post_height + 0.20f, z},
                        0.10f, WORLD_GOLD);
    } else if (surface == CC_LOCAL_ROAD_INDUSTRIAL) {
        DrawBox((Vector3){x, lantern_y - 0.02f, z},
                (Vector3){0.36f, 0.06f, 0.36f}, WORLD_METAL_LIGHT);
    }
}

static void DrawWayfarerGate(Color accent, float reveal)
{
    Color wood = WORLD_WOOD;
    float left = 8.66f;
    float right = 14.34f;
    float z = 10.56f;
    /* Keep the same pieces throughout the reveal. */
    float height = 2.32f + (0.12f - 2.32f) * reveal;
    float scale = height / 2.32f;
    DrawBox((Vector3){left, height * 0.5f, z},
            (Vector3){0.22f, height, 0.22f}, wood);
    DrawBox((Vector3){right, height * 0.5f, z},
            (Vector3){0.22f, height, 0.22f}, wood);
    DrawBox((Vector3){11.50f, 2.24f * scale, z},
            (Vector3){5.90f, 0.22f * scale, 0.24f}, wood);
    for (int32_t i = 0; i < 3; ++i) {
        float x = 10.05f + (float)i * 1.45f;
        DrawBox((Vector3){x, 1.91f * scale, z + 0.07f},
                (Vector3){0.56f, 0.48f * scale, 0.07f},
                i == 1 ? WORLD_GOLD : accent);
    }
}

static void DrawCroftScarecrow(float hunger)
{
    const float x = 8.10f;
    const float z = 25.00f;
    Color wood = WORLD_WOOD;
    Color cloth = BlendColor(WORLD_EARTH_LIGHT,
                             WORLD_ROAD_SHADOW, hunger);
    Color straw = BlendColor(WORLD_CROP_LIGHT, WORLD_WOOD_LIGHT, 0.34f);
    Vector3 post_base = {x - 0.06f, 0.02f, z};
    Vector3 post_top = {x + 0.13f, 1.97f, z + 0.02f};
    Vector3 arm_left = {x - 0.77f, 1.47f, z - 0.02f};
    Vector3 arm_right = {x + 0.70f, 1.35f, z + 0.03f};
    DrawCylinderEx(post_base, post_top, 0.060f, 0.045f, 7, wood);
    DrawCylinderEx(arm_left, arm_right, 0.052f, 0.045f, 7, wood);


    DrawTiltedBox((Vector3){x - 0.18f, 1.17f, z - 0.01f},
                  (Vector3){0.34f, 0.58f, 0.09f},
                  (Vector3){0.0f, 0.0f, 1.0f}, 10.0f, cloth);
    DrawTiltedBox((Vector3){x + 0.17f, 1.12f, z + 0.02f},
                  (Vector3){0.24f, 0.48f, 0.08f},
                  (Vector3){0.0f, 0.0f, 1.0f}, -14.0f,
                  ShadeColor(cloth, 0.78f));
    DrawCylinderEx(arm_left,
                   (Vector3){x - 0.58f, 1.19f, z - 0.01f},
                   0.020f, 0.012f, 5, straw);
    DrawCylinderEx(arm_right,
                   (Vector3){x + 0.58f, 1.08f, z + 0.04f},
                   0.020f, 0.012f, 5, straw);
    for (int32_t tuft = -1; tuft <= 1; ++tuft) {
        float spread = (float)tuft * 0.07f;
        DrawCylinderEx(arm_left,
                       (Vector3){arm_left.x - 0.17f,
                                 arm_left.y + spread, arm_left.z + spread},
                       0.014f, 0.004f, 5, straw);
        DrawCylinderEx(arm_right,
                       (Vector3){arm_right.x + 0.17f,
                                 arm_right.y + spread, arm_right.z - spread},
                       0.014f, 0.004f, 5, straw);
    }
    Vector3 head = {x + 0.14f, 1.82f, z + 0.01f};
    DrawSmallSphere(head, 0.19f, straw);
    DrawTiltedBox((Vector3){x + 0.17f, 2.01f, z + 0.01f},
                  (Vector3){0.63f, 0.065f, 0.38f},
                  (Vector3){0.0f, 0.0f, 1.0f}, -9.0f, wood);
    DrawTiltedBox((Vector3){x + 0.23f, 2.12f, z + 0.01f},
                  (Vector3){0.32f, 0.24f, 0.30f},
                  (Vector3){0.0f, 0.0f, 1.0f}, -9.0f, wood);
}

static void DrawMineWaystone(void)
{
    const float x = 18.0f;
    const float z = 54.72f;
    Color stone = WORLD_STONE;
    DrawBox((Vector3){x, 0.14f, z}, (Vector3){0.84f, 0.28f, 0.72f},
            ShadeColor(stone, 0.70f));
    DrawBox((Vector3){x, 0.82f, z}, (Vector3){0.56f, 1.38f, 0.46f}, stone);
    DrawBox((Vector3){x, 1.56f, z}, (Vector3){0.70f, 0.16f, 0.56f},
            ShadeColor(stone, 1.12f));
    DrawSmallSphere((Vector3){x, 1.03f, z + 0.27f}, 0.10f, WORLD_VIOLET);


    Color rail = WORLD_METAL_SHADOW;
    Color sleeper = WORLD_WOOD;
    DrawBox((Vector3){22.65f, 0.055f, 54.92f},
            (Vector3){8.20f, 0.075f, 0.09f}, rail);
    DrawBox((Vector3){22.65f, 0.055f, 55.72f},
            (Vector3){8.20f, 0.075f, 0.09f}, rail);
    for (int32_t sleeper_index = 0; sleeper_index < 10; ++sleeper_index) {
        DrawBox((Vector3){18.85f + (float)sleeper_index * 0.84f,
                          0.035f, 55.32f},
                (Vector3){0.14f, 0.055f, 1.18f}, sleeper);
    }

    DrawBox((Vector3){26.45f, 0.44f, 54.35f},
            (Vector3){1.45f, 0.88f, 1.05f},
            WORLD_WOOD_SHADOW);
    DrawBox((Vector3){26.45f, 0.91f, 54.35f},
            (Vector3){1.62f, 0.10f, 1.18f}, sleeper);
    for (int32_t ore = 0; ore < 4; ++ore) {
        float ore_x = 25.98f + (float)(ore & 1) * 0.64f;
        float ore_z = 54.02f + (float)(ore >> 1) * 0.54f;
        DrawTiltedBox((Vector3){ore_x, 1.02f, ore_z},
                      (Vector3){0.42f, 0.34f, 0.38f},
                      (Vector3){0.0f, 1.0f, 0.0f},
                      (float)ore * 19.0f, ShadeColor(stone, 0.78f));
    }
}

static void DrawArtisanSign(Color kingdom)
{
    const float x = 30.95f;
    const float z = 24.30f;
    Color wood = WORLD_WOOD_SHADOW;
    DrawBox((Vector3){x, 1.16f, z}, (Vector3){0.14f, 2.32f, 0.14f}, wood);
    DrawBox((Vector3){x + 0.48f, 2.12f, z},
            (Vector3){1.08f, 0.12f, 0.14f}, wood);
    DrawBox((Vector3){x + 0.86f, 1.72f, z + 0.04f},
            (Vector3){0.72f, 0.66f, 0.12f},
            BlendColor(WORLD_WOOD_LIGHT, kingdom, 0.26f));
    DrawSmallSphere((Vector3){x + 0.86f, 1.73f, z + 0.12f},
                    0.11f, WORLD_GOLD);
}

static void DrawWorkshopForge(Color kingdom,
                              const CcLocalPlaceProfile *profile)
{
    const float x = 37.85f;
    const float z = 23.08f;
    Color brick = BlendColor(WORLD_EARTH, kingdom, 0.12f);
    Color iron = WORLD_METAL_SHADOW;
    Color timber = WORLD_WOOD;
    DrawBox((Vector3){x, 0.48f, z},
            (Vector3){1.72f, 0.96f, 0.82f}, brick);
    DrawBox((Vector3){x, 1.10f, z - 0.10f},
            (Vector3){1.38f, 0.42f, 0.58f}, ShadeColor(brick, 0.78f));
    DrawBox((Vector3){x + 0.58f, 2.28f, z - 0.18f},
            (Vector3){0.46f, 2.80f, 0.46f}, iron);
    DrawBox((Vector3){x + 0.58f, 3.72f, z - 0.18f},
            (Vector3){0.62f, 0.18f, 0.62f}, ShadeColor(iron, 0.76f));
    DrawBox((Vector3){x - 0.14f, 0.70f, z + 0.44f},
            (Vector3){0.74f, 0.18f, 0.12f}, WORLD_GOLD);
    DrawSmallSphere((Vector3){x - 0.14f, 0.72f, z + 0.54f},
                    0.18f, BlendColor(WORLD_GOLD, WORLD_DANGER, 0.48f));
    DrawBox((Vector3){x - 1.45f, 0.30f, z + 0.05f},
            (Vector3){0.14f, 0.60f, 1.28f}, timber);
    for (int32_t billet = 0; billet < 3; ++billet) {
        DrawCylinderEx((Vector3){x - 1.72f, 0.16f + (float)billet * 0.15f,
                                 z - 0.40f},
                       (Vector3){x - 1.18f, 0.16f + (float)billet * 0.15f,
                                 z + 0.48f},
                       0.08f, 0.08f, 7, ShadeColor(timber, 1.08f));
    }
    if (profile == NULL) return;
    if (profile->function == CC_SETTLEMENT_MINING) {

        for (int32_t rail = -1; rail <= 1; rail += 2) {
            DrawBox((Vector3){x - 0.65f, 0.07f,
                              z + (float)rail * 0.52f},
                    (Vector3){2.50f, 0.09f, 0.09f}, iron);
        }
        DrawBox((Vector3){x - 0.70f, 0.52f, z},
                (Vector3){1.18f, 0.72f, 0.94f},
                ShadeColor(timber, 0.76f));
        for (int32_t ore = 0; ore < 3; ++ore) {
            DrawSmallSphere((Vector3){x - 1.02f + (float)ore * 0.32f,
                                      0.96f, z},
                            0.22f, ShadeColor(WORLD_STONE, 0.66f));
        }
    } else if (profile->function == CC_SETTLEMENT_FORTRESS) {

        DrawBox((Vector3){x - 1.55f, 1.05f, z},
                (Vector3){0.16f, 1.95f, 1.55f}, WORLD_WOOD_SHADOW);
        DrawBox((Vector3){x - 1.48f, 1.62f, z},
                (Vector3){0.12f, 0.16f, 1.62f}, kingdom);
        for (int32_t weapon = -1; weapon <= 1; ++weapon) {
            float weapon_z = z + (float)weapon * 0.48f;
            DrawBox((Vector3){x - 1.36f, 1.25f, weapon_z},
                    (Vector3){0.08f, 2.35f, 0.08f}, iron);
            DrawTiltedBox((Vector3){x - 1.36f, 2.43f, weapon_z},
                          (Vector3){0.10f, 0.34f, 0.16f},
                          (Vector3){0.0f, 0.0f, 1.0f}, 45.0f,
                          WORLD_METAL_LIGHT);
        }
    } else if (profile->function == CC_SETTLEMENT_CAPITAL) {

        DrawBox((Vector3){x - 1.48f, 0.58f, z},
                (Vector3){1.55f, 0.92f, 1.25f},
                ShadeColor(WORLD_WOOD_LIGHT, 0.86f));
        DrawBox((Vector3){x - 1.48f, 1.08f, z},
                (Vector3){1.72f, 0.12f, 1.38f}, WORLD_GOLD);
        for (int32_t work = 0; work < 3; ++work) {
            DrawSmallSphere((Vector3){x - 1.88f + (float)work * 0.40f,
                                      1.25f, z + 0.16f},
                            0.12f, work == 1 ? kingdom : WORLD_GOLD);
        }
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawBox((Vector3){x - 1.48f, 2.22f,
                              z + (float)side * 0.58f},
                    (Vector3){1.70f, 0.12f, 0.10f},
                    ShadeColor(WORLD_GOLD, 0.78f));
        }
    }
}

static Color PlaceIdentityAccent(const CcLocalPlaceProfile *profile,
                                 Color kingdom)
{
    if (profile == NULL) return kingdom;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING: return WORLD_CROP_LIGHT;
        case CC_SETTLEMENT_MINING: return WORLD_METAL_LIGHT;
        case CC_SETTLEMENT_MARKET: return WORLD_TEAL;
        case CC_SETTLEMENT_FORTRESS: return WORLD_DANGER;
        case CC_SETTLEMENT_CAPITAL: return WORLD_GOLD;
        case CC_SETTLEMENT_DUNGEON_TOWN: return WORLD_VIOLET;
    }
    return kingdom;
}

static void DrawTownSquareFocus(Color kingdom,
                                const CcLocalPlaceProfile *profile)
{
    const float x = CC_LOCAL_NOTICE_X;
    const float z = CC_LOCAL_NOTICE_Z;
    Color stone = WORLD_STONE_LIGHT;
    Color accent = PlaceIdentityAccent(profile, kingdom);

    DrawCylinder((Vector3){x, 0.015f, z}, 2.65f, 2.65f, 0.045f, 20,
                 ShadeColor(stone, 0.78f));
    DrawCylinder((Vector3){x, 0.052f, z}, 2.22f, 2.22f, 0.035f, 20,
                 stone);
    if (profile != NULL && profile->function == CC_SETTLEMENT_FARMING) {
        for (int32_t row = -1; row <= 1; ++row) {
            DrawTiltedBox((Vector3){x, 0.080f, z + (float)row * 0.48f},
                          (Vector3){3.62f, 0.018f, 0.12f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          (float)row * 4.0f, accent);
        }
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_MINING) {
        DrawBox((Vector3){x - 0.34f, 0.080f, z},
                (Vector3){0.12f, 0.018f, 3.70f}, accent);
        DrawBox((Vector3){x + 0.34f, 0.080f, z},
                (Vector3){0.12f, 0.018f, 3.70f}, accent);
        DrawTiltedBox((Vector3){x, 0.095f, z},
                      (Vector3){0.72f, 0.035f, 0.72f},
                      (Vector3){0.0f, 1.0f, 0.0f}, 45.0f,
                      WORLD_VIOLET);
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_FORTRESS) {
        DrawTiltedBox((Vector3){x, 0.080f, z},
                      (Vector3){3.30f, 0.018f, 0.15f},
                      (Vector3){0.0f, 1.0f, 0.0f}, 45.0f, accent);
        DrawTiltedBox((Vector3){x, 0.082f, z},
                      (Vector3){3.30f, 0.018f, 0.15f},
                      (Vector3){0.0f, 1.0f, 0.0f}, -45.0f, accent);
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_CAPITAL) {
        for (int32_t spoke = 0; spoke < 4; ++spoke) {
            DrawTiltedBox((Vector3){x, 0.080f, z},
                          (Vector3){3.52f, 0.018f, 0.11f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          (float)spoke * 45.0f, accent);
        }
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_DUNGEON_TOWN) {
        for (int32_t bar = -1; bar <= 1; ++bar) {
            DrawTiltedBox((Vector3){x + (float)bar * 0.44f, 0.080f, z},
                          (Vector3){0.12f, 0.018f, 3.45f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          (float)bar * 5.0f, accent);
        }
    } else {
        DrawBox((Vector3){x, 0.078f, z},
                (Vector3){3.70f, 0.018f, 0.13f}, accent);
        DrawBox((Vector3){x, 0.080f, z},
                (Vector3){0.13f, 0.018f, 3.70f}, accent);
    }
}

static void DrawCoachHitch(const CcSettlement *place)
{
    Color wood = WORLD_WOOD;
    Color sign = WORLD_WOOD_LIGHT;
    Color canopy = WORLD_EARTH_LIGHT;
    Color seal = WORLD_GOLD;
    switch (ActiveTownRoadSurface()) {
        case CC_LOCAL_ROAD_FARM_TRACK:
            sign = WORLD_CROP_LIGHT;
            canopy = WORLD_CROP;
            break;
        case CC_LOCAL_ROAD_INDUSTRIAL:
            wood = WORLD_METAL_SHADOW;
            sign = WORLD_METAL_LIGHT;
            canopy = WORLD_ROAD_SHADOW;
            break;
        case CC_LOCAL_ROAD_TRADE:
            sign = WORLD_GOLD;
            break;
        case CC_LOCAL_ROAD_MILITARY:
            wood = WORLD_STONE_SHADOW;
            sign = WORLD_STONE_LIGHT;
            canopy = WORLD_STONE;
            seal = WORLD_DANGER;
            break;
        case CC_LOCAL_ROAD_PROCESSIONAL:
            sign = WORLD_STONE_LIGHT;
            canopy = WORLD_GOLD;
            break;
        case CC_LOCAL_ROAD_EXPEDITION:
            sign = WORLD_VIOLET;
            canopy = WORLD_EARTH_SHADOW;
            seal = WORLD_VIOLET;
            break;
    }
    float z = 55.36f;
    DrawBox((Vector3){35.30f, 0.62f, z}, (Vector3){0.18f, 1.24f, 0.18f}, wood);
    DrawBox((Vector3){38.20f, 0.62f, z}, (Vector3){0.18f, 1.24f, 0.18f}, wood);
    DrawBox((Vector3){36.75f, 0.92f, z}, (Vector3){3.08f, 0.16f, 0.18f}, wood);
    DrawBox((Vector3){36.75f, 1.62f, z},
            (Vector3){0.14f, 1.40f, 0.14f}, wood);
    DrawBox((Vector3){36.75f, 2.18f, z + 0.05f},
            (Vector3){1.30f, 0.68f, 0.12f},
            sign);
    DrawBox((Vector3){36.75f, 2.70f, z - 0.02f},
            (Vector3){3.48f, 0.18f, 1.18f},
            ShadeColor(wood, 0.88f));
    DrawBox((Vector3){36.75f, 2.82f, z - 0.02f},
            (Vector3){3.08f, 0.12f, 1.44f},
            canopy);
    DrawCylinder((Vector3){36.75f, 2.03f, z + 0.13f},
                 0.19f, 0.19f, 0.08f, 10, seal);
    int32_t barrels = place != NULL ? place->stock[CC_GOOD_MATERIAL] / 24 : 1;
    if (barrels < 1) barrels = 1;
    if (barrels > 3) barrels = 3;
    for (int32_t i = 0; i < barrels; ++i) {
        DrawCylinder((Vector3){35.65f + (float)i * 0.58f, 0.05f, z + 0.42f},
                     0.25f, 0.25f, 0.58f, 10,
                     WORLD_WOOD_LIGHT);
    }
    for (int32_t wheel = 0; wheel < 2; ++wheel) {
        float wheel_z = z - 0.28f + (float)wheel * 0.58f;
        DrawCylinderEx((Vector3){38.50f, 0.62f, wheel_z},
                       (Vector3){38.58f, 0.62f, wheel_z},
                       0.48f, 0.48f, 12, ShadeColor(wood, 1.08f));
    }
}

static void DrawEastRoomFocus(float hunger, Color kingdom,
                              const CcLocalPlaceProfile *profile)
{
    const float x = 64.14f;
    const float z = 51.02f;
    Color timber = WORLD_WOOD_LIGHT;
    if (profile != NULL && profile->function == CC_SETTLEMENT_CAPITAL) {

        DrawCylinder((Vector3){x, 0.07f, z}, 0.90f, 0.90f, 0.14f, 14,
                     WORLD_STONE_LIGHT);
        for (int32_t corner = 0; corner < 4; ++corner) {
            float column_x = x + ((corner & 1) == 0 ? -0.68f : 0.68f);
            float column_z = z + ((corner & 2) == 0 ? -0.68f : 0.68f);
            DrawCylinder((Vector3){column_x, 0.16f, column_z},
                         0.12f, 0.16f, 2.35f, 8, WORLD_STONE_LIGHT);
            DrawSmallSphere((Vector3){column_x, 1.55f, column_z},
                            0.30f, BlendColor(kingdom, WORLD_FOLIAGE, 0.62f));
            DrawSmallSphere((Vector3){column_x, 1.78f, column_z},
                            0.11f, WORLD_DANGER);
        }
        DrawBox((Vector3){x, 2.48f, z - 0.68f},
                (Vector3){1.70f, 0.16f, 0.18f}, WORLD_GOLD);
        DrawBox((Vector3){x, 2.48f, z + 0.68f},
                (Vector3){1.70f, 0.16f, 0.18f}, WORLD_GOLD);
        DrawBox((Vector3){x - 0.68f, 2.48f, z},
                (Vector3){0.18f, 0.16f, 1.70f}, WORLD_GOLD);
        DrawBox((Vector3){x + 0.68f, 2.48f, z},
                (Vector3){0.18f, 0.16f, 1.70f}, WORLD_GOLD);
        return;
    }
    if (profile != NULL && profile->function == CC_SETTLEMENT_MINING) {
        DrawBox((Vector3){x, 0.78f, z},
                (Vector3){1.62f, 1.56f, 1.42f}, WORLD_WOOD_SHADOW);
        DrawBox((Vector3){x, 1.63f, z},
                (Vector3){1.88f, 0.16f, 1.68f}, WORLD_METAL_SHADOW);
        DrawBox((Vector3){x, 2.28f, z},
                (Vector3){0.26f, 1.46f, 0.26f}, WORLD_METAL_LIGHT);
        return;
    }
    if (profile != NULL && profile->function == CC_SETTLEMENT_MARKET) {
        DrawBox((Vector3){x, 0.80f, z},
                (Vector3){1.72f, 1.60f, 1.48f}, WORLD_WOOD_LIGHT);
        DrawBox((Vector3){x, 1.72f, z + 0.12f},
                (Vector3){2.10f, 0.18f, 1.84f}, kingdom);
        DrawBox((Vector3){x, 1.08f, z + 0.78f},
                (Vector3){1.24f, 0.12f, 0.22f}, WORLD_GOLD);
        return;
    }
    if (profile != NULL && profile->function == CC_SETTLEMENT_FORTRESS) {
        DrawBox((Vector3){x, 0.72f, z},
                (Vector3){1.72f, 1.44f, 1.52f}, WORLD_STONE_SHADOW);
        for (int32_t merlon = -1; merlon <= 1; ++merlon) {
            DrawBox((Vector3){x + (float)merlon * 0.58f, 1.72f, z},
                    (Vector3){0.34f, 0.58f, 1.62f}, kingdom);
        }
        return;
    }
    if (profile != NULL &&
        profile->function == CC_SETTLEMENT_DUNGEON_TOWN) {
        for (int32_t leg = -1; leg <= 1; leg += 2) {
            DrawTiltedBox((Vector3){x + (float)leg * 0.52f, 1.15f, z},
                          (Vector3){0.16f, 2.45f, 0.16f},
                          (Vector3){0.0f, 0.0f, 1.0f},
                          (float)-leg * 14.0f, WORLD_WOOD_SHADOW);
        }
        DrawBox((Vector3){x, 2.25f, z},
                (Vector3){1.62f, 0.16f, 0.18f}, WORLD_WOOD);
        DrawSmallSphere((Vector3){x, 1.42f, z}, 0.26f, WORLD_VIOLET);
        return;
    }
    Color plaster = BlendColor(WORLD_STONE_LIGHT,
                               WORLD_ROAD, hunger * 0.60f);
    DrawCylinder((Vector3){x, 0.08f, z}, 0.90f, 0.96f, 2.36f, 12, plaster);
    DrawCylinder((Vector3){x, 2.43f, z}, 0.10f, 1.10f, 0.88f, 12, timber);
    DrawBox((Vector3){x, 0.75f, z + 0.93f},
            (Vector3){0.58f, 1.20f, 0.08f}, timber);
    DrawBox((Vector3){x, 1.45f, z + 0.98f},
            (Vector3){0.72f, 0.10f, 0.12f}, WORLD_GOLD);
    int32_t boxes = hunger > 0.45f ? 2 : 5;
    for (int32_t box = 0; box < boxes; ++box) {
        float box_x = x - 1.28f + (float)(box % 3) * 0.48f;
        float box_z = z + 0.64f + (float)(box / 3) * 0.40f;
        DrawBox((Vector3){box_x, 0.24f, box_z},
                (Vector3){0.40f, 0.42f, 0.36f},
                (Color){177, 116, 55, 255});
        DrawBox((Vector3){box_x, 0.46f, box_z},
                (Vector3){0.42f, 0.05f, 0.38f}, WORLD_CROP_LIGHT);
    }
}

static void DrawEastWindmill(Color kingdom, float hunger)
{
    const float x = 81.4f;
    const float z = 47.0f;
    Color tower = BlendColor(WORLD_STONE_LIGHT,
                             WORLD_ROAD, hunger * 0.50f);
    Color timber = BlendColor(WORLD_WOOD, kingdom, 0.16f);
    DrawCylinder((Vector3){x, 0.05f, z}, 0.74f, 1.08f, 3.45f, 12, tower);
    DrawCylinder((Vector3){x, 3.50f, z}, 0.08f, 0.92f, 0.82f, 12, timber);
    DrawCylinderEx((Vector3){x, 3.28f, z + 0.36f},
                   (Vector3){x, 3.28f, z + 0.92f},
                   0.14f, 0.14f, 10, ShadeColor(timber, 0.72f));
    for (int32_t blade = 0; blade < 4; ++blade) {
        rlPushMatrix();
        rlTranslatef(x, 3.28f, z + 0.96f);
        rlRotatef(18.0f + (float)blade * 90.0f, 0.0f, 0.0f, 1.0f);
        DrawBox((Vector3){0.0f, 1.02f, 0.0f},
                (Vector3){0.18f, 2.04f, 0.10f}, timber);
        DrawBox((Vector3){0.20f, 1.38f, 0.0f},
                (Vector3){0.38f, 0.92f, 0.075f},
                Fade(BlendColor(WORLD_INK, WORLD_CROP_LIGHT, 0.42f), 0.78f));
        rlPopMatrix();
    }
    DrawSmallSphere((Vector3){x, 3.28f, z + 1.02f}, 0.20f, WORLD_GOLD);
}

static void DrawPlaceLandmark(const CcLocalPlaceLandmark *landmark,
                              Color kingdom)
{
    if (landmark == NULL) return;
    float center_x = landmark->x + landmark->width * 0.5f;
    float center_z = landmark->z + landmark->depth * 0.5f;
    float height = landmark->height;
    Color wall = WORLD_WOOD;
    Color roof = WORLD_WOOD_SHADOW;
    Color trim = WORLD_CROP_LIGHT;
    switch (landmark->family) {
        case CC_LOCAL_LANDMARK_AGRICULTURE:
            wall = BlendColor(WORLD_WOOD, WORLD_CROP, 0.30f);
            roof = WORLD_CROP_SHADOW;
            trim = WORLD_CROP_LIGHT;
            break;
        case CC_LOCAL_LANDMARK_INDUSTRY:
            wall = WORLD_STONE_SHADOW;
            roof = WORLD_METAL_SHADOW;
            trim = WORLD_METAL_LIGHT;
            break;
        case CC_LOCAL_LANDMARK_COMMERCE:
            wall = BlendColor(WORLD_STONE_LIGHT, kingdom, 0.16f);
            roof = BlendColor(WORLD_WOOD_SHADOW, kingdom, 0.22f);
            trim = WORLD_GOLD;
            break;
        case CC_LOCAL_LANDMARK_MILITARY:
            wall = BlendColor(WORLD_STONE, kingdom, 0.20f);
            roof = WORLD_METAL_SHADOW;
            trim = kingdom;
            break;
        case CC_LOCAL_LANDMARK_CIVIC:
            wall = WORLD_STONE_LIGHT;
            roof = BlendColor(WORLD_STONE, kingdom, 0.20f);
            trim = WORLD_GOLD;
            break;
        case CC_LOCAL_LANDMARK_EXPEDITION:
            wall = BlendColor(WORLD_STONE_SHADOW, WORLD_VIOLET, 0.16f);
            roof = WORLD_WOOD_SHADOW;
            trim = WORLD_VIOLET;
            break;
    }

    DrawBox((Vector3){center_x, 0.16f, center_z},
            (Vector3){landmark->width, 0.32f, landmark->depth},
            ShadeColor(wall, 0.62f));

    if (landmark->family == CC_LOCAL_LANDMARK_AGRICULTURE &&
        landmark->variant == 1) {
        float body_height = height * 0.72f;
        DrawBox((Vector3){center_x, body_height * 0.16f, center_z},
                (Vector3){landmark->width, body_height * 0.32f,
                          landmark->depth}, wall);
        for (int32_t silo = 0; silo < 2; ++silo) {
            float silo_x = landmark->x + landmark->width *
                           (silo == 0 ? 0.31f : 0.69f);
            float radius = fminf(landmark->width * 0.19f,
                                 landmark->depth * 0.34f);
            DrawCylinder((Vector3){silo_x, height * 0.20f, center_z},
                         radius * 0.88f, radius, height * 0.62f, 12, wall);
            DrawCylinder((Vector3){silo_x, height * 0.82f, center_z},
                         0.04f, radius * 1.06f, height * 0.18f, 12, roof);
        }
        return;
    }

    if (landmark->family == CC_LOCAL_LANDMARK_CIVIC &&
        landmark->variant == 1) {
        DrawBox((Vector3){center_x, height * 0.12f, center_z},
                (Vector3){landmark->width, height * 0.24f,
                          landmark->depth}, ShadeColor(wall, 0.78f));
        DrawBox((Vector3){center_x, height * 0.52f, center_z},
                (Vector3){landmark->width * 0.42f, height * 0.80f,
                          landmark->depth * 0.42f}, wall);
        DrawBox((Vector3){center_x, height * 0.94f, center_z},
                (Vector3){landmark->width * 0.58f, height * 0.14f,
                          landmark->depth * 0.58f}, trim);
        return;
    }

    float body_height = height * 0.78f;
    int32_t building_style = landmark->family ==
                                 CC_LOCAL_LANDMARK_COMMERCE ||
                             landmark->family == CC_LOCAL_LANDMARK_CIVIC ?
                                 CC_LOCAL_BUILDING_CIVIC :
                             landmark->family == CC_LOCAL_LANDMARK_MILITARY ?
                                 CC_LOCAL_BUILDING_WORKER_ROW :
                                 CC_LOCAL_BUILDING_WORKSHOP;
    WorldBuilding shell = {
        .footprint = {landmark->x, landmark->z,
                      landmark->width, landmark->depth},
        .height = body_height,
        .style = building_style,
        .door = true,
    };

    DrawSettlementBuilding(&shell, wall, roof, ActivePlaceProfile(),
                           landmark->variant);

    if (landmark->family == CC_LOCAL_LANDMARK_INDUSTRY) {
        int32_t stacks = landmark->variant == 1 ? 3 : 2;
        for (int32_t stack = 0; stack < stacks; ++stack) {
            float stack_x = landmark->x + landmark->width *
                ((float)(stack + 1) / (float)(stacks + 1));
            DrawCylinder((Vector3){stack_x, body_height * 0.82f,
                                   landmark->z + landmark->depth * 0.68f},
                         0.18f, 0.24f, height * 0.28f, 8,
                         WORLD_METAL_SHADOW);
        }
    } else if (landmark->family == CC_LOCAL_LANDMARK_COMMERCE) {
        for (int32_t panel = 0; panel < 4; ++panel) {
            float panel_x = landmark->x + landmark->width *
                ((float)panel + 0.5f) / 4.0f;
            DrawBox((Vector3){panel_x, body_height * 0.58f,
                              landmark->z + landmark->depth + 0.26f},
                    (Vector3){landmark->width * 0.22f, 0.12f, 0.52f},
                    (panel & 1) == 0 ? trim : roof);
        }
    } else if (landmark->family == CC_LOCAL_LANDMARK_MILITARY) {
        int32_t merlons = landmark->variant == 1 ? 3 : 5;
        for (int32_t merlon = 0; merlon < merlons; ++merlon) {
            float merlon_x = landmark->x + landmark->width *
                ((float)merlon + 0.5f) / (float)merlons;
            DrawBox((Vector3){merlon_x, body_height + height * 0.16f,
                              center_z},
                    (Vector3){landmark->width / (float)merlons * 0.52f,
                              height * 0.20f, landmark->depth * 0.18f},
                    trim);
        }
    } else if (landmark->family == CC_LOCAL_LANDMARK_CIVIC) {
        DrawBox((Vector3){center_x, body_height * 0.62f,
                          landmark->z + landmark->depth + 0.08f},
                (Vector3){landmark->width * 0.72f, height * 0.10f, 0.16f},
                trim);
    } else if (landmark->family == CC_LOCAL_LANDMARK_EXPEDITION) {
        DrawBox((Vector3){center_x, body_height + height * 0.20f, center_z},
                (Vector3){0.26f, height * 0.30f, 0.26f},
                WORLD_WOOD_SHADOW);
        DrawSmallSphere((Vector3){center_x, height * 1.02f, center_z},
                        0.22f, trim);
    }
}

static void DrawPlaceLandmarks(Color kingdom, Vector3 focus)
{
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        const CcLocalPlaceLandmark *landmark = ActivePlaceLandmarkAt(i);
        if (landmark == NULL) continue;
        Rectangle footprint = PlaceLandmarkFootprint(landmark);
        if (!SceneryFootprintVisible(footprint, focus)) continue;
        rlPushMatrix();
        rlTranslatef(0.0f, TerrainFootprintHeight(footprint), 0.0f);
        DrawPlaceLandmark(landmark, kingdom);
        rlPopMatrix();
    }
}

static void DrawRoomLandmarks(const CcSettlement *place, Color kingdom,
                              const CcLocalPlaceProfile *profile,
                              Vector3 focus, float wayfarer_gate_sightline_cut)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    if (RoomDetailPointVisible(11.50f, 10.56f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(11.50f, 10.56f), 0.0f);
        DrawWayfarerGate(kingdom, wayfarer_gate_sightline_cut);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(8.10f, 25.00f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(8.10f, 25.00f), 0.0f);
        DrawCroftScarecrow(hunger);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(18.0f, 54.72f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(18.0f, 54.72f), 0.0f);
        DrawMineWaystone();
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(30.95f, 24.30f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(30.95f, 24.30f), 0.0f);
        DrawArtisanSign(kingdom);
        DrawWorkshopForge(kingdom, profile);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f,
                     CcLocalTerrainHeightAt(CC_LOCAL_NOTICE_X,
                                            CC_LOCAL_NOTICE_Z), 0.0f);
        DrawTownSquareFocus(kingdom, profile);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(36.75f, 55.36f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(36.75f, 55.36f), 0.0f);
        DrawCoachHitch(place);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(64.14f, 51.02f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(64.14f, 51.02f), 0.0f);
        DrawEastRoomFocus(hunger, kingdom, profile);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(81.4f, 47.0f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(81.4f, 47.0f), 0.0f);
        DrawEastWindmill(kingdom, hunger);
        rlPopMatrix();
    }
}

static bool TerrainPointInRectangle(float x, float z, Rectangle rectangle)
{
    return x >= rectangle.x && x <= rectangle.x + rectangle.width &&
           z >= rectangle.y && z <= rectangle.y + rectangle.height;
}

static const Rectangle TERRAIN_FIELDS[] = {
    {3.0f, 17.0f, 13.0f, 9.0f}, {3.0f, 29.0f, 13.0f, 9.0f},
    {3.0f, 41.0f, 13.0f, 9.0f}, {59.0f, 43.0f, 14.0f, 10.0f},
    {76.0f, 43.0f, 15.0f, 10.0f},
};

static float TerrainRectangleInset(float x, float z, Rectangle rectangle)
{
    if (!TerrainPointInRectangle(x, z, rectangle)) return 0.0f;
    return fminf(fminf(x - rectangle.x,
                       rectangle.x + rectangle.width - x),
                 fminf(z - rectangle.y,
                       rectangle.y + rectangle.height - z));
}

static float TerrainRectangleSignedInset(float x, float z,
                                          Rectangle rectangle)
{
    if (TerrainPointInRectangle(x, z, rectangle)) {
        return TerrainRectangleInset(x, z, rectangle);
    }
    float closest_x = TerrainClamp(
        x, rectangle.x, rectangle.x + rectangle.width);
    float closest_z = TerrainClamp(
        z, rectangle.y, rectangle.y + rectangle.height);
    float dx = x - closest_x;
    float dz = z - closest_z;
    return -sqrtf(dx * dx + dz * dz);
}

static float TerrainRoadCenterline(const TerrainRoad *road,
                                   int32_t road_index, float along)
{
    float start = road->runs_east_west ? road->footprint.x :
                                         road->footprint.y;
    float length = road->runs_east_west ? road->footprint.width :
                                          road->footprint.height;
    float amount = TerrainClamp((along - start) / fmaxf(length, 0.01f),
                                0.0f, 1.0f);
    float cross = road->runs_east_west ?
        road->footprint.y + road->footprint.height * 0.5f :
        road->footprint.x + road->footprint.width * 0.5f;
    float cross_width = road->runs_east_west ? road->footprint.height :
                                               road->footprint.width;

    if (TerrainRoadIndexIsCarriageLane(road_index)) return cross;
    float bend = sinf(amount * PI) *
                 sinf(amount * 2.35f + (float)road_index * 1.71f) *
                 fminf(0.48f, cross_width * 0.105f);
    float wandering = TerrainValueNoise(
        along + (float)road_index * 7.0f,
        (float)road_index * 5.0f, 8.0f, 71U) *
        fminf(0.16f, cross_width * 0.035f) * sinf(amount * PI);
    return cross + bend + wandering;
}

static float TerrainRoadSignedInset(const TerrainRoad *road,
                                    int32_t road_index, float x, float z)
{
    float along = road->runs_east_west ? x : z;
    float cross = road->runs_east_west ? z : x;
    float start = road->runs_east_west ? road->footprint.x :
                                         road->footprint.y;
    float length = road->runs_east_west ? road->footprint.width :
                                          road->footprint.height;
    float cross_width = road->runs_east_west ? road->footprint.height :
                                               road->footprint.width;
    float amount = TerrainClamp((along - start) / fmaxf(length, 0.01f),
                                0.0f, 1.0f);
    float width_noise = TerrainValueNoise(
        along - (float)road_index * 4.0f,
        (float)road_index * 9.0f, 5.5f, 72U);

    float visible_scale = TerrainRoadIndexIsCarriageLane(road_index) ? 0.94f :
                          cross_width >= 4.50f ? 0.72f :
                          cross_width >= 3.40f ? 0.80f : 0.90f;
    float half_width = cross_width * 0.5f * visible_scale *
                       (0.94f + width_noise * 0.07f);
    float cross_inset = half_width - fabsf(
        cross - TerrainRoadCenterline(road, road_index, along));
    float end_inset = fminf(along - start, start + length - along);

    end_inset += sinf(amount * PI) * 0.16f;
    return fminf(cross_inset, end_inset);
}

static int32_t TerrainVisibleRoadCount(void)
{

    return (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0])) - 1;
}

static float TerrainPlaceRoadAmount(float x, float z)
{
    float amount = 0.0f;
    int32_t index_offset =
        (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0]));
    for (int32_t i = 0; i < CC_LOCAL_PLACE_ROAD_COUNT; ++i) {
        TerrainRoad road = PlaceTerrainRoad(ActivePlaceRoadAt(i));
        int32_t road_index = index_offset + i;
        float inset = TerrainRoadSignedInset(&road, road_index, x, z);
        float edge_breakup = TerrainValueNoise(
            x + (float)road_index * 3.7f,
            z - (float)road_index * 2.9f, 4.5f, 17U) * 0.24f;
        amount = fmaxf(amount,
                       TerrainSmooth01(inset / 0.68f + edge_breakup));
    }
    return amount;
}

static float TerrainCarriageRoadAmount(float x, float z)
{
    float amount = 0.0f;
    int32_t index_offset = TerrainCarriageRoadIndexOffset();
    for (int32_t i = 0; i < CC_LOCAL_CARRIAGE_ROUTE_COUNT; ++i) {
        TerrainRoad road = PlaceTerrainRoad(ActiveCarriageRouteAt(i));
        float inset = i < 3 ?
            TerrainRoadSignedInset(&road, index_offset + i, x, z) :
            TerrainRectangleSignedInset(x, z, road.footprint);
        float edge_breakup = TerrainValueNoise(
            x + (float)(index_offset + i) * 3.7f,
            z - (float)(index_offset + i) * 2.9f, 5.5f, 19U) * 0.12f;
        amount = fmaxf(amount,
                       TerrainSmooth01(inset / 0.72f + edge_breakup));
    }
    return amount;
}

static float TerrainRoadAmount(float x, float z)
{
    float amount = 0.0f;
    for (int32_t i = 0; i < TerrainVisibleRoadCount(); ++i) {
        float inset = TerrainRoadSignedInset(&TERRAIN_ROADS[i], i, x, z);
        float edge_breakup = TerrainValueNoise(
            x + (float)i * 3.7f, z - (float)i * 2.9f, 4.5f, 17U) * 0.24f;
        amount = fmaxf(amount,
                       TerrainSmooth01(inset / 0.68f + edge_breakup));
    }
    amount = fmaxf(amount, TerrainPlaceRoadAmount(x, z));
    return fmaxf(amount, TerrainCarriageRoadAmount(x, z));
}

CcSoundCue CcLocalFootstepSurfaceAt(CcLocalSceneKind scene, float x, float z)
{
    if (scene == CC_LOCAL_SCENE_MARKET) return CC_SOUND_STEP_WOOD;
    if (active_world_stream != NULL) {
        CcWorldSurfaceKind surface = CcWorldSurfaceAt(
            &active_world_stream->manifest, x, z);
        return surface == CC_WORLD_SURFACE_WILDERNESS ? CC_SOUND_STEP_GRASS :
               surface == CC_WORLD_SURFACE_ROAD ? CC_SOUND_STEP_DIRT : CC_SOUND_STEP_STONE;
    }
    if (scene == CC_LOCAL_SCENE_ROAD) {
        if (RoadUsesAuthoredCheckpoint() &&
            CcLocalRoadCheckpointSurfaceYInternal(x, z) > 0.0f)
            return CC_SOUND_STEP_STONE;
        return x >= 15.0f && x <= 81.0f && z >= 36.30f && z <= 43.70f ?
               CC_SOUND_STEP_DIRT : CC_SOUND_STEP_GRASS;
    }
    if (scene != CC_LOCAL_SCENE_STREET) return CC_SOUND_STEP_STONE;
    if (CourseWaterContains(scene, x, z)) return CC_SOUND_SPLASH;
    float terrain = CcLocalTerrainHeightAt(x, z);
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform != NULL && x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth &&
            PlatformTopHeight(platform) > terrain) {
            return platform->style == 0 ? CC_SOUND_STEP_STONE : CC_SOUND_STEP_WOOD;
        }
    }
    /* Use the same road edges and seed as the ground colors. */
    if (TerrainRoadAmount(x, z) < 0.5f) return CC_SOUND_STEP_GRASS;
    CcLocalRoadSurface surface = ActiveTownRoadSurface();
    return surface == CC_LOCAL_ROAD_FARM_TRACK || surface == CC_LOCAL_ROAD_EXPEDITION ?
           CC_SOUND_STEP_DIRT : CC_SOUND_STEP_STONE;
}

static Color TerrainPlaceRoadColor(CcLocalRoadSurface surface)
{
    switch (surface) {
        case CC_LOCAL_ROAD_FARM_TRACK:
            return BlendColor(WORLD_EARTH, WORLD_CROP_SHADOW, 0.24f);
        case CC_LOCAL_ROAD_INDUSTRIAL:
            return BlendColor(WORLD_ROAD_SHADOW,
                              WORLD_METAL_SHADOW, 0.30f);
        case CC_LOCAL_ROAD_TRADE:
            return BlendColor(WORLD_ROAD_LIGHT, WORLD_GOLD, 0.10f);
        case CC_LOCAL_ROAD_MILITARY:
            return BlendColor(WORLD_STONE, WORLD_ROAD_SHADOW, 0.28f);
        case CC_LOCAL_ROAD_PROCESSIONAL:
            return BlendColor(WORLD_STONE_LIGHT, WORLD_ROAD, 0.20f);
        case CC_LOCAL_ROAD_EXPEDITION:
            return BlendColor(WORLD_EARTH_SHADOW, WORLD_VIOLET, 0.12f);
    }
    return WORLD_ROAD;
}

static Color TerrainBrightRoadColor(CcLocalRoadSurface surface,
                                    float prosperity)
{
    float light_mix = 0.38f + prosperity * 0.12f;
    if (surface == CC_LOCAL_ROAD_TRADE) light_mix = 0.18f;
    if (surface == CC_LOCAL_ROAD_PROCESSIONAL) light_mix = 0.30f;
    return BlendColor(TerrainPlaceRoadColor(surface), WORLD_ROAD_LIGHT,
                      light_mix);
}

static Color TerrainRoadRutColor(CcLocalRoadSurface surface,
                                 float prosperity)
{
    Color rut = WORLD_ROAD_SHADOW;
    switch (surface) {
        case CC_LOCAL_ROAD_FARM_TRACK:
            rut = BlendColor(WORLD_EARTH_SHADOW, WORLD_CROP_SHADOW, 0.28f);
            break;
        case CC_LOCAL_ROAD_INDUSTRIAL:
            rut = BlendColor(WORLD_METAL_SHADOW, WORLD_ROAD_SHADOW, 0.34f);
            break;
        case CC_LOCAL_ROAD_TRADE:
            rut = BlendColor(WORLD_ROAD_SHADOW, WORLD_GOLD, 0.08f);
            break;
        case CC_LOCAL_ROAD_MILITARY:
            rut = BlendColor(WORLD_STONE_SHADOW, WORLD_ROAD_SHADOW, 0.28f);
            break;
        case CC_LOCAL_ROAD_PROCESSIONAL:
            rut = BlendColor(WORLD_STONE, WORLD_GOLD, 0.08f);
            break;
        case CC_LOCAL_ROAD_EXPEDITION:
            rut = BlendColor(WORLD_EARTH_SHADOW, WORLD_VIOLET, 0.12f);
            break;
    }
    return BlendColor(rut, TerrainPlaceRoadColor(surface),
                      prosperity * 0.10f);
}

static float TerrainFieldAmount(float x, float z)
{
    float amount = 0.0f;
    for (int32_t i = 0; i < (int32_t)(sizeof(TERRAIN_FIELDS) /
                                      sizeof(TERRAIN_FIELDS[0])); ++i) {
        Rectangle field = TERRAIN_FIELDS[i];
        float inset = TerrainRectangleSignedInset(x, z, field);
        float broad = TerrainValueNoise(
            x + (float)i * 8.2f, z - (float)i * 5.3f, 6.4f, 73U);
        float scallop = sinf((x - field.x) * 0.48f + (float)i) * 0.16f +
                        sinf((z - field.y) * 0.61f - (float)i) * 0.12f;
        amount = fmaxf(
            amount,
            TerrainSmooth01((inset + broad * 0.48f + scallop) / 0.80f));
    }
    return amount;
}

static bool TerrainPointInPlayableWorld(float x, float z)
{
    return x >= 0.0f && x <= CC_LOCAL_WORLD_WIDTH &&
           z >= 0.0f && z <= CC_LOCAL_WORLD_DEPTH;
}


static float TerrainVisualHeightAt(float x, float z)
{
    if (TerrainPointInPlayableWorld(x, z)) {
        return CcLocalTerrainHeightAt(x, z);
    }
    float edge_x = TerrainClamp(x, 0.0f, CC_LOCAL_WORLD_WIDTH);
    float edge_z = TerrainClamp(z, 0.0f, CC_LOCAL_WORLD_DEPTH);
    float dx = x - edge_x;
    float dz = z - edge_z;
    float distance = sqrtf(dx * dx + dz * dz);
    float edge_height = CcLocalTerrainHeightAt(edge_x, edge_z);
    float natural_delta = TerrainNaturalHeight(x, z) -
                          TerrainNaturalHeight(edge_x, edge_z);
    float natural_weight = TerrainSmooth01(distance / 5.0f);
    return edge_height + natural_delta * natural_weight - distance * 0.035f;
}

static Vector3 TerrainVisualNormalAt(float x, float z)
{
    const float offset = CC_TERRAIN_CELL_SIZE;
    float left = TerrainVisualHeightAt(x - offset, z);
    float right = TerrainVisualHeightAt(x + offset, z);
    float near_height = TerrainVisualHeightAt(x, z - offset);
    float far_height = TerrainVisualHeightAt(x, z + offset);
    Vector3 normal = {left - right, offset * 2.0f,
                      near_height - far_height};
    return Vector3Normalize(normal);
}

static Color TerrainSurfaceColor(const CcSettlement *place,
                                 float x, float z, Vector3 normal)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color grass = BlendColor(WORLD_GRASS,
                             WORLD_EARTH, hunger * 0.72f);
    float height = TerrainVisualHeightAt(x, z);
    float lowland = TerrainSmooth01((1.35f - height) / 2.40f);
    float highland = TerrainSmooth01((height - 4.20f) / 3.20f);
    grass = BlendColor(grass, WORLD_FOLIAGE_SHADOW, lowland * 0.34f);
    grass = BlendColor(grass, WORLD_GRASS_LIGHT, highland * 0.30f);
    Color color = grass;
    float field_amount = TerrainFieldAmount(x, z);
    Color field = BlendColor(WORLD_CROP,
                             WORLD_CROP_LIGHT, prosperity * 0.26f);
    field = BlendColor(field, grass, hunger * 0.28f);
    color = BlendColor(color, field, field_amount * 0.84f);

    float road_amount = TerrainRoadAmount(x, z);
    CcLocalRoadSurface road_surface = ActiveTownRoadSurface();
    Color road = TerrainBrightRoadColor(road_surface, prosperity);
    color = BlendColor(color, road, road_amount);
    const CcLocalPlaceRoad *place_road = ActivePlaceRoadAt(0);
    float place_road_amount = TerrainPlaceRoadAmount(x, z);
    if (place_road != NULL) {
        Color place_road_color = TerrainBrightRoadColor(
            place_road->surface, prosperity);
        color = BlendColor(color, place_road_color, place_road_amount);
    }
    const CcLocalPlaceRoad *carriage_road = ActiveCarriageRouteAt(0);
    float carriage_road_amount = TerrainCarriageRoadAmount(x, z);
    if (carriage_road != NULL) {
        Color carriage_road_color = TerrainPlaceRoadColor(
            carriage_road->surface);
        carriage_road_color = BlendColor(
            carriage_road_color, WORLD_ROAD_LIGHT,
            0.22f + prosperity * 0.12f);
        color = BlendColor(color, carriage_road_color,
                           carriage_road_amount);
    }

    Rectangle plaza = {37.6f, 25.6f, 18.8f, 8.8f};
    float plaza_amount = TerrainSmooth01(
        TerrainRectangleInset(x, z, plaza) / 0.55f);
    Color plaza_color = BlendColor(WORLD_STONE,
                                   WORLD_STONE_LIGHT,
                                   prosperity * 0.36f);
    color = BlendColor(color, plaza_color, plaza_amount);

    Rectangle wayfarer_yard = {1.0f, 0.0f, 14.6f, 11.1f};
    float yard_amount = TerrainSmooth01(
        TerrainRectangleInset(x, z, wayfarer_yard) / 0.52f);
    color = BlendColor(color, WORLD_FOLIAGE_SHADOW, yard_amount);

    float rock_amount = TerrainSmooth01((0.82f - normal.y) / 0.22f);
    Color stone = BlendColor(WORLD_STONE,
                             WORLD_ROAD, highland * 0.36f);
    color = BlendColor(color, stone, rock_amount * 0.78f);

    float broad = TerrainValueNoise(x, z, 11.0f, 21U);
    float detail = TerrainValueNoise(x + 5.0f, z - 9.0f, 3.4f, 22U);
    return ShadeColor(color, 1.0f + broad * 0.035f + detail * 0.014f);
}

typedef struct TerrainMeshWriter {
    Mesh mesh;
    int32_t cursor;
    bool count_only;
} TerrainMeshWriter;

typedef struct TerrainIndexedMeshWriter {
    Mesh mesh;
    int32_t vertex_cursor;
    int32_t index_cursor;
    bool failed;
} TerrainIndexedMeshWriter;

#define CC_TERRAIN_RENDER_MARGIN 28
#define CC_TERRAIN_RENDER_INNER_STEP 0.50f
#define CC_TERRAIN_RENDER_OUTER_STEP 1.0f

static TerrainMeshWriter *terrain_detail_mesh_writer = NULL;
static bool terrain_detail_cache_building = false;

static void TerrainRenderCacheClear(void)
{
    if (terrain_render_cache.ready) {
        UnloadModel(terrain_render_cache.model);
    }
    terrain_render_cache = (TerrainRenderCache){0};
}

static void TerrainDetailRenderCacheClear(void)
{
    if (terrain_detail_render_cache.ready) {
        UnloadModel(terrain_detail_render_cache.model);
    }
    terrain_detail_render_cache = (TerrainRenderCache){0};
}

static bool TerrainMeshWriterAllocate(TerrainMeshWriter *writer,
                                      int32_t vertex_count)
{
    if (writer == NULL || vertex_count <= 0) return false;
    *writer = (TerrainMeshWriter){0};
    writer->mesh.vertexCount = vertex_count;
    writer->mesh.triangleCount = vertex_count / 3;
    writer->mesh.vertices = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    writer->mesh.normals = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    writer->mesh.colors = MemAlloc(
        (unsigned int)((size_t)vertex_count * 4U * sizeof(unsigned char)));
    if (writer->mesh.vertices != NULL && writer->mesh.normals != NULL &&
        writer->mesh.colors != NULL) {
        return true;
    }
    if (writer->mesh.vertices != NULL) MemFree(writer->mesh.vertices);
    if (writer->mesh.normals != NULL) MemFree(writer->mesh.normals);
    if (writer->mesh.colors != NULL) MemFree(writer->mesh.colors);
    *writer = (TerrainMeshWriter){0};
    return false;
}

static void TerrainMeshWriteVertexWithNormal(TerrainMeshWriter *writer,
                                             Vector3 point, Vector3 normal,
                                             Color color)
{
    if (writer == NULL) return;
    if (writer->count_only) {
        writer->cursor += 1;
        return;
    }
    if (writer->cursor >= writer->mesh.vertexCount) return;
    int32_t vertex = writer->cursor;

    normal.y = fmaxf(normal.y, 0.24f);
    normal = Vector3Normalize(normal);
    writer->mesh.vertices[vertex * 3 + 0] = point.x;
    writer->mesh.vertices[vertex * 3 + 1] = point.y;
    writer->mesh.vertices[vertex * 3 + 2] = point.z;
    writer->mesh.normals[vertex * 3 + 0] = normal.x;
    writer->mesh.normals[vertex * 3 + 1] = normal.y;
    writer->mesh.normals[vertex * 3 + 2] = normal.z;
    writer->mesh.colors[vertex * 4 + 0] = color.r;
    writer->mesh.colors[vertex * 4 + 1] = color.g;
    writer->mesh.colors[vertex * 4 + 2] = color.b;
    writer->mesh.colors[vertex * 4 + 3] = color.a;
    writer->cursor += 1;
}

static int32_t TerrainGridVertexCount(int32_t columns, int32_t rows)
{
    return (columns + 1) * (rows + 1);
}

static int32_t TerrainGridIndexCount(int32_t columns, int32_t rows)
{
    return columns * rows * 6;
}

static CcLocalTerrainMeshStatsInternal TerrainMeshStats(void)
{
    const int32_t inner_columns =
        (int32_t)(CC_LOCAL_WORLD_WIDTH / CC_TERRAIN_RENDER_INNER_STEP);
    const int32_t inner_rows =
        (int32_t)(CC_LOCAL_WORLD_DEPTH / CC_TERRAIN_RENDER_INNER_STEP);
    const int32_t outer_columns =
        (int32_t)(CC_LOCAL_WORLD_WIDTH / CC_TERRAIN_RENDER_OUTER_STEP) +
        CC_TERRAIN_RENDER_MARGIN * 2;
    const int32_t outer_rows =
        (int32_t)(CC_LOCAL_WORLD_DEPTH / CC_TERRAIN_RENDER_OUTER_STEP);
    int32_t vertex_count =
        TerrainGridVertexCount(inner_columns, inner_rows) +
        TerrainGridVertexCount(outer_columns, CC_TERRAIN_RENDER_MARGIN) * 2 +
        TerrainGridVertexCount(CC_TERRAIN_RENDER_MARGIN, outer_rows) * 2;
    int32_t index_count =
        TerrainGridIndexCount(inner_columns, inner_rows) +
        TerrainGridIndexCount(outer_columns, CC_TERRAIN_RENDER_MARGIN) * 2 +
        TerrainGridIndexCount(CC_TERRAIN_RENDER_MARGIN, outer_rows) * 2;
    int32_t vertex_bytes = vertex_count *
        (int32_t)(6U * sizeof(float) + 4U * sizeof(unsigned char));
    int32_t index_bytes = index_count * (int32_t)sizeof(unsigned short);
    return (CcLocalTerrainMeshStatsInternal){
        .vertex_count = vertex_count,
        .index_count = index_count,
        .storage_bytes = vertex_bytes + index_bytes,
    };
}

CcLocalTerrainMeshStatsInternal CcLocalTerrainMeshStatsInternalGet(void)
{
    return TerrainMeshStats();
}

static bool TerrainIndexedMeshWriterAllocate(
    TerrainIndexedMeshWriter *writer,
    CcLocalTerrainMeshStatsInternal stats)
{
    if (writer == NULL || stats.vertex_count <= 0 ||
        stats.vertex_count > UINT16_MAX || stats.index_count <= 0 ||
        stats.index_count % 3 != 0) {
        return false;
    }
    *writer = (TerrainIndexedMeshWriter){0};
    writer->mesh.vertexCount = stats.vertex_count;
    writer->mesh.triangleCount = stats.index_count / 3;
    writer->mesh.vertices = MemAlloc(
        (unsigned int)((size_t)stats.vertex_count * 3U * sizeof(float)));
    writer->mesh.normals = MemAlloc(
        (unsigned int)((size_t)stats.vertex_count * 3U * sizeof(float)));
    writer->mesh.colors = MemAlloc(
        (unsigned int)((size_t)stats.vertex_count * 4U *
                       sizeof(unsigned char)));
    writer->mesh.indices = MemAlloc(
        (unsigned int)((size_t)stats.index_count * sizeof(unsigned short)));
    if (writer->mesh.vertices != NULL && writer->mesh.normals != NULL &&
        writer->mesh.colors != NULL && writer->mesh.indices != NULL) {
        return true;
    }
    if (writer->mesh.vertices != NULL) MemFree(writer->mesh.vertices);
    if (writer->mesh.normals != NULL) MemFree(writer->mesh.normals);
    if (writer->mesh.colors != NULL) MemFree(writer->mesh.colors);
    if (writer->mesh.indices != NULL) MemFree(writer->mesh.indices);
    *writer = (TerrainIndexedMeshWriter){0};
    return false;
}

static void TerrainIndexedMeshWriteVertex(
    TerrainIndexedMeshWriter *writer, const CcSettlement *place,
    float x, float z)
{
    if (writer == NULL || writer->failed ||
        writer->vertex_cursor >= writer->mesh.vertexCount) {
        if (writer != NULL) writer->failed = true;
        return;
    }
    Vector3 normal = TerrainVisualNormalAt(x, z);
    normal.y = fmaxf(normal.y, 0.24f);
    normal = Vector3Normalize(normal);
    Color color = TerrainSurfaceColor(place, x, z, normal);
    int32_t vertex = writer->vertex_cursor++;
    writer->mesh.vertices[vertex * 3 + 0] = x;
    writer->mesh.vertices[vertex * 3 + 1] = TerrainVisualHeightAt(x, z);
    writer->mesh.vertices[vertex * 3 + 2] = z;
    writer->mesh.normals[vertex * 3 + 0] = normal.x;
    writer->mesh.normals[vertex * 3 + 1] = normal.y;
    writer->mesh.normals[vertex * 3 + 2] = normal.z;
    writer->mesh.colors[vertex * 4 + 0] = color.r;
    writer->mesh.colors[vertex * 4 + 1] = color.g;
    writer->mesh.colors[vertex * 4 + 2] = color.b;
    writer->mesh.colors[vertex * 4 + 3] = color.a;
}

static void TerrainIndexedMeshWriteIndex(TerrainIndexedMeshWriter *writer,
                                         int32_t vertex)
{
    int32_t index_count = writer != NULL ?
        writer->mesh.triangleCount * 3 : 0;
    if (writer == NULL || writer->failed || vertex < 0 ||
        vertex > UINT16_MAX || writer->index_cursor >= index_count) {
        if (writer != NULL) writer->failed = true;
        return;
    }
    writer->mesh.indices[writer->index_cursor++] = (unsigned short)vertex;
}

static void TerrainIndexedMeshWriteGrid(
    TerrainIndexedMeshWriter *writer, const CcSettlement *place,
    float start_x, float start_z, float step,
    int32_t columns, int32_t rows)
{
    if (writer == NULL || columns <= 0 || rows <= 0 || step <= 0.0f) {
        if (writer != NULL) writer->failed = true;
        return;
    }
    int32_t base_vertex = writer->vertex_cursor;
    int32_t row_stride = columns + 1;
    for (int32_t row = 0; row <= rows; ++row) {
        float z = start_z + (float)row * step;
        for (int32_t column = 0; column <= columns; ++column) {
            float x = start_x + (float)column * step;
            TerrainIndexedMeshWriteVertex(writer, place, x, z);
        }
    }
    for (int32_t row = 0; row < rows; ++row) {
        for (int32_t column = 0; column < columns; ++column) {
            int32_t p00 = base_vertex + row * row_stride + column;
            int32_t p10 = p00 + 1;
            int32_t p01 = p00 + row_stride;
            int32_t p11 = p01 + 1;
            TerrainIndexedMeshWriteIndex(writer, p00);
            TerrainIndexedMeshWriteIndex(writer, p11);
            TerrainIndexedMeshWriteIndex(writer, p10);
            TerrainIndexedMeshWriteIndex(writer, p00);
            TerrainIndexedMeshWriteIndex(writer, p01);
            TerrainIndexedMeshWriteIndex(writer, p11);
        }
    }
}

static bool TerrainRenderCacheBuild(const CcSettlement *place)
{
    TerrainRenderCacheClear();
    TerrainEnsureReady();
    CcLocalTerrainMeshStatsInternal stats = TerrainMeshStats();
    TerrainIndexedMeshWriter writer;
    if (!TerrainIndexedMeshWriterAllocate(&writer, stats)) {
        TraceLog(LOG_WARNING,
                 "TERRAIN: could not allocate indexed cache (%d vertices)",
                 stats.vertex_count);
        return false;
    }

    const int32_t inner_columns =
        (int32_t)(CC_LOCAL_WORLD_WIDTH / CC_TERRAIN_RENDER_INNER_STEP);
    const int32_t inner_rows =
        (int32_t)(CC_LOCAL_WORLD_DEPTH / CC_TERRAIN_RENDER_INNER_STEP);
    const int32_t outer_columns =
        (int32_t)(CC_LOCAL_WORLD_WIDTH / CC_TERRAIN_RENDER_OUTER_STEP) +
        CC_TERRAIN_RENDER_MARGIN * 2;
    const int32_t outer_rows =
        (int32_t)(CC_LOCAL_WORLD_DEPTH / CC_TERRAIN_RENDER_OUTER_STEP);
    const float margin = (float)CC_TERRAIN_RENDER_MARGIN;
    TerrainIndexedMeshWriteGrid(
        &writer, place, -margin, -margin, CC_TERRAIN_RENDER_OUTER_STEP,
        outer_columns, CC_TERRAIN_RENDER_MARGIN);
    TerrainIndexedMeshWriteGrid(
        &writer, place, -margin, CC_LOCAL_WORLD_DEPTH,
        CC_TERRAIN_RENDER_OUTER_STEP,
        outer_columns, CC_TERRAIN_RENDER_MARGIN);
    TerrainIndexedMeshWriteGrid(
        &writer, place, -margin, 0.0f, CC_TERRAIN_RENDER_OUTER_STEP,
        CC_TERRAIN_RENDER_MARGIN, outer_rows);
    TerrainIndexedMeshWriteGrid(
        &writer, place, CC_LOCAL_WORLD_WIDTH, 0.0f,
        CC_TERRAIN_RENDER_OUTER_STEP,
        CC_TERRAIN_RENDER_MARGIN, outer_rows);
    TerrainIndexedMeshWriteGrid(
        &writer, place, 0.0f, 0.0f, CC_TERRAIN_RENDER_INNER_STEP,
        inner_columns, inner_rows);
    if (writer.failed || writer.vertex_cursor != stats.vertex_count ||
        writer.index_cursor != stats.index_count) {
        TraceLog(LOG_WARNING,
                 "TERRAIN: indexed cache count mismatch (%d/%d, %d/%d)",
                 writer.vertex_cursor, stats.vertex_count,
                 writer.index_cursor, stats.index_count);
        UnloadMesh(writer.mesh);
        return false;
    }
    UploadMesh(&writer.mesh, false);
    terrain_render_cache.model = LoadModelFromMesh(writer.mesh);
    ReleaseUploadedModelCpuData(&terrain_render_cache.model);
    ApplyWorldShader(&terrain_render_cache.model);
    terrain_render_cache.seed = street_terrain_seed;
    terrain_render_cache.place_id = place != NULL ? place->id : 0U;
    terrain_render_cache.hunger_band = place != NULL ? place->hunger / 5 : 0;
    terrain_render_cache.prosperity_band =
        place != NULL ? place->prosperity / 5 : 10;
    terrain_render_cache.vertex_count = stats.vertex_count;
    terrain_render_cache.ready = true;
    TraceLog(LOG_INFO,
             "TERRAIN: cached %d vertices and %d indices (%.1f MiB)",
             stats.vertex_count, stats.index_count,
             (double)stats.storage_bytes / (1024.0 * 1024.0));
    return true;
}

static void DrawCachedTerrain(const CcSettlement *place)
{
    int32_t hunger_band = place != NULL ? place->hunger / 5 : 0;
    int32_t prosperity_band = place != NULL ? place->prosperity / 5 : 10;
    bool stale = !terrain_render_cache.ready ||
                 terrain_render_cache.seed != street_terrain_seed ||
                 terrain_render_cache.place_id !=
                     (place != NULL ? place->id : 0U) ||
                 terrain_render_cache.hunger_band != hunger_band ||
                 terrain_render_cache.prosperity_band != prosperity_band;
    if (stale && !TerrainRenderCacheBuild(place)) return;
    DrawModel(terrain_render_cache.model, (Vector3){0.0f, 0.0f, 0.0f},
              1.0f, WHITE);
}

static void TerrainDetailVertex(Vector3 point, Color color)
{
    Vector3 normal = CcLocalTerrainNormalAt(point.x, point.z);
    normal.y = fmaxf(normal.y, 0.62f);
    normal = Vector3Normalize(normal);
    if (terrain_detail_mesh_writer != NULL) {
        TerrainMeshWriteVertexWithNormal(
            terrain_detail_mesh_writer, point, normal, color);
        return;
    }
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlVertex3f(point.x, point.y, point.z);
}

static void TerrainDetailBegin(void)
{
    if (terrain_detail_mesh_writer == NULL) rlBegin(RL_TRIANGLES);
}

static void TerrainDetailEnd(void)
{
    if (terrain_detail_mesh_writer == NULL) rlEnd();
}

static void TerrainRibbonSegment(Vector2 start, Vector2 end,
                                 float half_width, float lift, Color color)
{
    float dx = end.x - start.x;
    float dz = end.y - start.y;
    float length = sqrtf(dx * dx + dz * dz);
    if (length < 0.001f) return;
    float side_x = -dz / length * half_width;
    float side_z = dx / length * half_width;
    Vector3 right_start = {
        start.x - side_x, 0.0f, start.y - side_z,
    };
    Vector3 left_start = {
        start.x + side_x, 0.0f, start.y + side_z,
    };
    Vector3 right_end = {end.x - side_x, 0.0f, end.y - side_z};
    Vector3 left_end = {end.x + side_x, 0.0f, end.y + side_z};
    right_start.y = CcLocalTerrainHeightAt(right_start.x, right_start.z) +
                    lift;
    left_start.y = CcLocalTerrainHeightAt(left_start.x, left_start.z) + lift;
    right_end.y = CcLocalTerrainHeightAt(right_end.x, right_end.z) + lift;
    left_end.y = CcLocalTerrainHeightAt(left_end.x, left_end.z) + lift;
    TerrainDetailVertex(right_start, color);
    TerrainDetailVertex(left_end, color);
    TerrainDetailVertex(right_end, color);
    TerrainDetailVertex(right_start, color);
    TerrainDetailVertex(left_start, color);
    TerrainDetailVertex(left_end, color);
}

static Vector2 TerrainRoadDetailPoint(const TerrainRoad *road,
                                      int32_t road_index, float along,
                                      float cross_offset)
{
    float cross = TerrainRoadCenterline(road, road_index, along) + cross_offset;
    return road->runs_east_west ? (Vector2){along, cross} :
                                  (Vector2){cross, along};
}

static void DrawTerrainRoadIdentityMotif(const TerrainRoad *road,
                                         int32_t road_index,
                                         CcLocalRoadSurface surface,
                                         Vector3 focus)
{
    Rectangle footprint = road->footprint;
    float start = road->runs_east_west ? footprint.x : footprint.y;
    float finish = start + (road->runs_east_west ? footprint.width :
                                                      footprint.height);
    float cross_width = road->runs_east_west ? footprint.height :
                                               footprint.width;
    float visible_scale = cross_width >= 4.50f ? 0.72f :
                          cross_width >= 3.40f ? 0.80f : 0.90f;
    float edge = cross_width * 0.5f * visible_scale * 0.78f;
    float step = surface == CC_LOCAL_ROAD_EXPEDITION ? 1.42f : 1.08f;
    float segment_length = surface == CC_LOCAL_ROAD_EXPEDITION ? 0.72f :
                                                                  1.10f;
    Rectangle plaza = {37.6f, 25.6f, 18.8f, 8.8f};
    int32_t segment = 0;
    for (float along = start + 0.35f; along < finish - 0.35f;
         along += step, ++segment) {
        float next = fminf(along + segment_length, finish - 0.35f);
        Vector2 center_a = TerrainRoadDetailPoint(
            road, road_index, along, 0.0f);
        Vector2 center_b = TerrainRoadDetailPoint(
            road, road_index, next, 0.0f);
        Vector2 middle = {(center_a.x + center_b.x) * 0.5f,
                          (center_a.y + center_b.y) * 0.5f};
        float focus_x = middle.x - focus.x;
        float focus_z = middle.y - focus.z;
        if (!terrain_detail_cache_building &&
            focus_x * focus_x + focus_z * focus_z > 24.0f * 24.0f) {
            continue;
        }
        if (TerrainPointInRectangle(middle.x, middle.y, plaza)) continue;

        Vector2 left_a = TerrainRoadDetailPoint(
            road, road_index, along, -edge);
        Vector2 left_b = TerrainRoadDetailPoint(
            road, road_index, next, -edge);
        Vector2 right_a = TerrainRoadDetailPoint(
            road, road_index, along, edge);
        Vector2 right_b = TerrainRoadDetailPoint(
            road, road_index, next, edge);
        switch (surface) {
            case CC_LOCAL_ROAD_FARM_TRACK:
                if (segment % 3 != 2) {
                    TerrainRibbonSegment(center_a, center_b, 0.085f, 0.058f,
                                         WORLD_GRASS_SHADOW);
                }
                break;
            case CC_LOCAL_ROAD_INDUSTRIAL:
                TerrainRibbonSegment(left_a, left_b, 0.045f, 0.060f,
                                     WORLD_METAL_SHADOW);
                TerrainRibbonSegment(right_a, right_b, 0.045f, 0.060f,
                                     WORLD_METAL_SHADOW);
                if (segment % 2 == 0) {
                    Vector2 sleeper_a = TerrainRoadDetailPoint(
                        road, road_index, along, -edge * 0.82f);
                    Vector2 sleeper_b = TerrainRoadDetailPoint(
                        road, road_index, along, edge * 0.82f);
                    TerrainRibbonSegment(sleeper_a, sleeper_b, 0.035f,
                                         0.057f, WORLD_WOOD_SHADOW);
                }
                break;
            case CC_LOCAL_ROAD_TRADE:
                TerrainRibbonSegment(left_a, left_b, 0.055f, 0.060f,
                                     WORLD_STONE_LIGHT);
                TerrainRibbonSegment(right_a, right_b, 0.055f, 0.060f,
                                     WORLD_STONE_LIGHT);
                if (segment % 4 == 0) {
                    TerrainRibbonSegment(center_a, center_b, 0.035f, 0.059f,
                                         WORLD_GOLD);
                }
                break;
            case CC_LOCAL_ROAD_MILITARY:
                TerrainRibbonSegment(left_a, left_b, 0.070f, 0.059f,
                                     WORLD_STONE_SHADOW);
                TerrainRibbonSegment(right_a, right_b, 0.070f, 0.059f,
                                     WORLD_STONE_SHADOW);
                if (segment % 6 == 0) {
                    Vector2 stripe_a = TerrainRoadDetailPoint(
                        road, road_index, along, -edge * 0.72f);
                    Vector2 stripe_b = TerrainRoadDetailPoint(
                        road, road_index, along, edge * 0.72f);
                    TerrainRibbonSegment(stripe_a, stripe_b, 0.040f,
                                         0.058f, WORLD_STONE_LIGHT);
                }
                break;
            case CC_LOCAL_ROAD_PROCESSIONAL:
                TerrainRibbonSegment(center_a, center_b, 0.050f, 0.061f,
                                     WORLD_GOLD);
                TerrainRibbonSegment(left_a, left_b, 0.050f, 0.060f,
                                     WORLD_STONE_LIGHT);
                TerrainRibbonSegment(right_a, right_b, 0.050f, 0.060f,
                                     WORLD_STONE_LIGHT);
                break;
            case CC_LOCAL_ROAD_EXPEDITION:
                if (segment % 2 == 0) {
                    TerrainRibbonSegment(left_a, left_b, 0.065f, 0.060f,
                                         WORLD_VIOLET);
                } else {
                    TerrainRibbonSegment(right_a, right_b, 0.065f, 0.060f,
                                         WORLD_VIOLET);
                }
                break;
        }
    }
}

static void DrawTerrainRoadRuts(const CcSettlement *place, Vector3 focus)
{
    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    CcLocalRoadSurface active_surface = ActiveTownRoadSurface();
    Color paved_rut = TerrainRoadRutColor(active_surface, prosperity);
    Color field_track = BlendColor(WORLD_GRASS_SHADOW,
                                   WORLD_ROAD_SHADOW, 0.34f);
    Rectangle plaza = {37.6f, 25.6f, 18.8f, 8.8f};
    int32_t road_count =
        (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0]));
    TerrainDetailBegin();
    for (int32_t i = 0; i < road_count; ++i) {
        bool rural_track = i == 0 || i == 1 || i == 3 || i == 4 ||
                           i == 9 || i == 12 || i == 13 || i == 14;
        if (!rural_track) continue;
        const TerrainRoad *road = &TERRAIN_ROADS[i];
        Rectangle footprint = road->footprint;
        Color color = i < TerrainVisibleRoadCount() ? paved_rut : field_track;
        float lane_offset = (road->runs_east_west ? footprint.height :
                                                       footprint.width) *
                            0.17f;
        for (int32_t lane = -1; lane <= 1; lane += 2) {
            float start = road->runs_east_west ? footprint.x : footprint.y;
            float finish = start + (road->runs_east_west ? footprint.width :
                                                             footprint.height);
            for (float along = start + 0.35f; along < finish - 0.35f;
                 along += 0.72f) {
                float next = fminf(along + 0.74f, finish - 0.35f);
                float cross = TerrainRoadCenterline(road, i, along) +
                              (float)lane * lane_offset;
                float next_cross = TerrainRoadCenterline(road, i, next) +
                                   (float)lane * lane_offset;
                Vector2 a = road->runs_east_west ?
                    (Vector2){along, cross} : (Vector2){cross, along};
                Vector2 b = road->runs_east_west ?
                    (Vector2){next, next_cross} :
                    (Vector2){next_cross, next};
                Vector2 middle = {(a.x + b.x) * 0.5f,
                                  (a.y + b.y) * 0.5f};
                float focus_x = middle.x - focus.x;
                float focus_z = middle.y - focus.z;
                if (!terrain_detail_cache_building &&
                    focus_x * focus_x + focus_z * focus_z > 24.0f * 24.0f) {
                    continue;
                }
                if (TerrainPointInRectangle(middle.x, middle.y, plaza)) {
                    continue;
                }
                if (TerrainRoadAmount(middle.x, middle.y) < 0.34f) continue;
                TerrainRibbonSegment(a, b, 0.055f, 0.055f, color);
            }
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_ROAD_COUNT; ++i) {
        const CcLocalPlaceRoad *place_road = ActivePlaceRoadAt(i);
        if (place_road == NULL) continue;
        TerrainRoad road = PlaceTerrainRoad(place_road);
        Rectangle footprint = road.footprint;
        int32_t road_index = road_count + i;
        Color color = TerrainRoadRutColor(place_road->surface, prosperity);
        float lane_offset = (road.runs_east_west ? footprint.height :
                                                    footprint.width) * 0.17f;
        for (int32_t lane = -1; lane <= 1; lane += 2) {
            float start = road.runs_east_west ? footprint.x : footprint.y;
            float finish = start + (road.runs_east_west ? footprint.width :
                                                             footprint.height);
            for (float along = start + 0.35f; along < finish - 0.35f;
                 along += 0.72f) {
                float next = fminf(along + 0.74f, finish - 0.35f);
                float cross = TerrainRoadCenterline(
                    &road, road_index, along) + (float)lane * lane_offset;
                float next_cross = TerrainRoadCenterline(
                    &road, road_index, next) + (float)lane * lane_offset;
                Vector2 a = road.runs_east_west ?
                    (Vector2){along, cross} : (Vector2){cross, along};
                Vector2 b = road.runs_east_west ?
                    (Vector2){next, next_cross} :
                    (Vector2){next_cross, next};
                Vector2 middle = {(a.x + b.x) * 0.5f,
                                  (a.y + b.y) * 0.5f};
                float focus_x = middle.x - focus.x;
                float focus_z = middle.y - focus.z;
                if (!terrain_detail_cache_building &&
                    focus_x * focus_x + focus_z * focus_z >
                    24.0f * 24.0f) continue;
                if (TerrainPlaceRoadAmount(middle.x, middle.y) < 0.34f) {
                    continue;
                }
                TerrainRibbonSegment(a, b, 0.055f, 0.055f, color);
            }
        }
    }

    for (int32_t i = 0; i < 3; ++i) {
        const CcLocalPlaceRoad *carriage_road = ActiveCarriageRouteAt(i);
        if (carriage_road == NULL) continue;
        TerrainRoad road = PlaceTerrainRoad(carriage_road);
        Rectangle footprint = road.footprint;
        int32_t road_index = TerrainCarriageRoadIndexOffset() + i;
        Color color = ShadeColor(
            TerrainPlaceRoadColor(carriage_road->surface), 0.58f);
        float lane_offset = fminf(
            0.92f,
            (road.runs_east_west ? footprint.height : footprint.width) *
                0.18f);
        for (int32_t lane = -1; lane <= 1; lane += 2) {
            float start = road.runs_east_west ? footprint.x : footprint.y;
            float finish = start + (road.runs_east_west ? footprint.width :
                                                             footprint.height);
            for (float along = start + 0.35f; along < finish - 0.35f;
                 along += 0.72f) {
                float next = fminf(along + 0.74f, finish - 0.35f);
                float cross = TerrainRoadCenterline(
                    &road, road_index, along) + (float)lane * lane_offset;
                float next_cross = TerrainRoadCenterline(
                    &road, road_index, next) + (float)lane * lane_offset;
                Vector2 a = road.runs_east_west ?
                    (Vector2){along, cross} : (Vector2){cross, along};
                Vector2 b = road.runs_east_west ?
                    (Vector2){next, next_cross} :
                    (Vector2){next_cross, next};
                Vector2 middle = {(a.x + b.x) * 0.5f,
                                  (a.y + b.y) * 0.5f};
                float focus_x = middle.x - focus.x;
                float focus_z = middle.y - focus.z;
                if (!terrain_detail_cache_building &&
                    focus_x * focus_x + focus_z * focus_z >
                        24.0f * 24.0f) {
                    continue;
                }
                if (TerrainCarriageRoadAmount(middle.x, middle.y) < 0.34f) {
                    continue;
                }
                TerrainRibbonSegment(a, b, 0.065f, 0.060f, color);
            }
        }
    }
    for (int32_t i = 0; i < TerrainVisibleRoadCount(); ++i) {
        DrawTerrainRoadIdentityMotif(
            &TERRAIN_ROADS[i], i, active_surface, focus);
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_ROAD_COUNT; ++i) {
        const CcLocalPlaceRoad *place_road = ActivePlaceRoadAt(i);
        if (place_road == NULL) continue;
        TerrainRoad road = PlaceTerrainRoad(place_road);
        DrawTerrainRoadIdentityMotif(
            &road, road_count + i, place_road->surface, focus);
    }
    for (int32_t i = 0; i < 3; ++i) {
        const CcLocalPlaceRoad *carriage_road = ActiveCarriageRouteAt(i);
        if (carriage_road == NULL) continue;
        TerrainRoad road = PlaceTerrainRoad(carriage_road);
        DrawTerrainRoadIdentityMotif(
            &road, TerrainCarriageRoadIndexOffset() + i,
            carriage_road->surface, focus);
    }
    TerrainDetailEnd();
}

static void DrawTerrainFieldRows(const CcSettlement *place, Vector3 focus)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color crop = BlendColor(WORLD_CROP,
                            WORLD_CROP_LIGHT, prosperity * 0.44f);
    crop = BlendColor(crop, WORLD_CROP_SHADOW, hunger * 0.42f);
    Color furrow = BlendColor(WORLD_GRASS_SHADOW,
                              WORLD_EARTH_SHADOW, hunger * 0.30f);
    TerrainDetailBegin();
    for (int32_t i = 0; i < (int32_t)(sizeof(TERRAIN_FIELDS) /
                                      sizeof(TERRAIN_FIELDS[0])); ++i) {
        Rectangle field = TERRAIN_FIELDS[i];
        if (!terrain_detail_cache_building &&
            !SceneryFootprintVisible(field, focus)) continue;
        int32_t row = 0;
        for (float z = field.y + 0.78f;
             z < field.y + field.height - 0.45f; z += 1.32f, ++row) {
            Color color = BlendColor(crop, furrow,
                                     (row & 1) == 0 ? 0.12f : 0.28f);
            for (float x = field.x + 0.45f;
                 x < field.x + field.width - 0.45f; x += 0.72f) {
                float next = fminf(x + 0.74f,
                                   field.x + field.width - 0.45f);
                float row_bend = sinf(
                    (x - field.x) * 0.36f + (float)i * 1.2f) * 0.10f;
                float next_bend = sinf(
                    (next - field.x) * 0.36f + (float)i * 1.2f) * 0.10f;
                float middle_x = (x + next) * 0.5f;
                float middle_z = z + (row_bend + next_bend) * 0.5f;
                if (TerrainFieldAmount(middle_x, middle_z) < 0.30f) continue;
                TerrainRibbonSegment((Vector2){x, z + row_bend},
                                     (Vector2){next, z + next_bend},
                                     0.080f, 0.055f, color);
            }
        }
    }
    TerrainDetailEnd();
}

static float TerrainScatter01(int32_t column, int32_t row, uint32_t stream)
{
    uint32_t value = street_terrain_seed ^
        ((uint32_t)column * UINT32_C(0x9e3779b9)) ^
        ((uint32_t)row * UINT32_C(0x85ebca6b)) ^
        (stream * UINT32_C(0xc2b2ae35));
    return (float)(TerrainMix(value) & UINT32_C(0x00ffffff)) /
           (float)UINT32_C(0x00ffffff);
}

static void TerrainCoverVertex(Vector3 point, Color color)
{
    if (terrain_detail_mesh_writer != NULL) {
        TerrainMeshWriteVertexWithNormal(
            terrain_detail_mesh_writer, point,
            (Vector3){0.18f, 0.96f, 0.14f}, color);
        return;
    }
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(0.18f, 0.96f, 0.14f);
    rlVertex3f(point.x, point.y, point.z);
}

static void TerrainTuft(float x, float z, float height, float width,
                        float angle, Color bottom, Color top)
{
    float ground = CcLocalTerrainHeightAt(x, z) + 0.025f;
    for (int32_t blade = 0; blade < 2; ++blade) {
        float direction = angle + (float)blade * PI * 0.5f;
        float side_x = cosf(direction) * width;
        float side_z = sinf(direction) * width;
        Vector3 left = {x - side_x, ground, z - side_z};
        Vector3 right = {x + side_x, ground, z + side_z};
        Vector3 peak = {x + sinf(direction) * width * 0.20f,
                        ground + height,
                        z - cosf(direction) * width * 0.20f};
        TerrainCoverVertex(left, bottom);
        TerrainCoverVertex(right, bottom);
        TerrainCoverVertex(peak, top);

        TerrainCoverVertex(peak, top);
        TerrainCoverVertex(right, bottom);
        TerrainCoverVertex(left, bottom);
    }
}

static void DrawTerrainPlantCover(const CcSettlement *place, Vector3 focus)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    Color grass_bottom = BlendColor(WORLD_GRASS_SHADOW,
                                    WORLD_EARTH_SHADOW,
                                    hunger * 0.62f);
    Color grass_top = BlendColor(WORLD_GRASS_LIGHT,
                                 WORLD_EARTH,
                                 hunger * 0.58f);
    const float spacing = 1.65f;
    int32_t first_column = terrain_detail_cache_building ? 0 :
        (int32_t)floorf(fmaxf(0.0f, focus.x - 24.0f) / spacing);
    int32_t last_column = terrain_detail_cache_building ?
        (int32_t)ceilf(CC_LOCAL_WORLD_WIDTH / spacing) :
        (int32_t)ceilf(
            fminf(CC_LOCAL_WORLD_WIDTH, focus.x + 24.0f) / spacing);
    int32_t first_row = terrain_detail_cache_building ? 0 :
        (int32_t)floorf(fmaxf(0.0f, focus.z - 24.0f) / spacing);
    int32_t last_row = terrain_detail_cache_building ?
        (int32_t)ceilf(CC_LOCAL_WORLD_DEPTH / spacing) :
        (int32_t)ceilf(
            fminf(CC_LOCAL_WORLD_DEPTH, focus.z + 24.0f) / spacing);

    TerrainDetailBegin();
    for (int32_t row = first_row; row <= last_row; ++row) {
        for (int32_t column = first_column; column <= last_column; ++column) {
            float chance = TerrainScatter01(column, row, 31U);
            float cluster = TerrainValueNoise(
                (float)column * spacing, (float)row * spacing, 8.5f, 61U);
            float threshold = 0.50f - cluster * 0.20f;
            if (chance < threshold) continue;
            float x = ((float)column + 0.16f +
                       TerrainScatter01(column, row, 32U) * 0.68f) * spacing;
            float z = ((float)row + 0.16f +
                       TerrainScatter01(column, row, 33U) * 0.68f) * spacing;
            if (!TerrainPointInPlayableWorld(x, z) ||
                TerrainRoadAmount(x, z) > 0.06f ||
                TerrainFieldAmount(x, z) > 0.05f ||
                TerrainPointInsideMajorFoundation(x, z)) {
                continue;
            }
            Vector3 normal = CcLocalTerrainNormalAt(x, z);
            if (normal.y < 0.70f) continue;
            float scale = 0.78f +
                          TerrainScatter01(column, row, 34U) * 0.52f;
            float angle = TerrainScatter01(column, row, 35U) * PI * 2.0f;
            TerrainTuft(x, z, 0.24f * scale, 0.085f * scale, angle,
                         grass_bottom, grass_top);
        }
    }

    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color crop_bottom = BlendColor(WORLD_CROP_SHADOW,
                                   WORLD_EARTH, hunger * 0.38f);
    Color crop_top = BlendColor(WORLD_CROP,
                                WORLD_CROP_LIGHT,
                                prosperity * 0.42f);
    for (int32_t field_index = 0;
         field_index < (int32_t)(sizeof(TERRAIN_FIELDS) /
                                  sizeof(TERRAIN_FIELDS[0])); ++field_index) {
        Rectangle field = TERRAIN_FIELDS[field_index];
        if (!terrain_detail_cache_building &&
            !SceneryFootprintVisible(field, focus)) continue;
        int32_t row_index = 0;
        for (float z = field.y + 0.78f;
             z < field.y + field.height - 0.55f;
             z += 1.32f, ++row_index) {
            int32_t column_index = 0;
            for (float x = field.x + 0.72f;
                 x < field.x + field.width - 0.55f;
                 x += 1.05f, ++column_index) {
                float jitter = TerrainScatter01(
                    field_index * 31 + column_index,
                    row_index, 41U) - 0.5f;
                float scale = 0.82f + TerrainScatter01(
                    field_index * 37 + column_index,
                    row_index, 42U) * 0.38f;
                float crop_x = x + jitter * 0.22f;
                if (TerrainFieldAmount(crop_x, z) < 0.22f) continue;
                TerrainTuft(crop_x, z,
                             0.31f * scale, 0.070f * scale,
                             jitter * 0.55f, crop_bottom, crop_top);
            }
        }
    }
    TerrainDetailEnd();
}

static void DrawTerrainRocks(Vector3 focus)
{
    const float spacing = 4.80f;
    int32_t first_column = (int32_t)floorf(
        fmaxf(0.0f, focus.x - 24.0f) / spacing);
    int32_t last_column = (int32_t)ceilf(
        fminf(CC_LOCAL_WORLD_WIDTH, focus.x + 24.0f) / spacing);
    int32_t first_row = (int32_t)floorf(
        fmaxf(0.0f, focus.z - 24.0f) / spacing);
    int32_t last_row = (int32_t)ceilf(
        fminf(CC_LOCAL_WORLD_DEPTH, focus.z + 24.0f) / spacing);
    for (int32_t row = first_row; row <= last_row; ++row) {
        for (int32_t column = first_column; column <= last_column; ++column) {
            float chance = TerrainScatter01(column, row, 51U);
            if (chance < 0.74f) continue;
            float x = ((float)column + 0.18f +
                       TerrainScatter01(column, row, 52U) * 0.64f) * spacing;
            float z = ((float)row + 0.18f +
                       TerrainScatter01(column, row, 53U) * 0.64f) * spacing;
            if (!TerrainPointInPlayableWorld(x, z) ||
                TerrainRoadAmount(x, z) > 0.04f ||
                TerrainFieldAmount(x, z) > 0.04f ||
                TerrainPointInsideMajorFoundation(x, z)) {
                continue;
            }
            Vector3 normal = CcLocalTerrainNormalAt(x, z);
            if (normal.y > 0.94f &&
                TerrainScatter01(column, row, 54U) < 0.82f) {
                continue;
            }
            float radius = 0.16f +
                           TerrainScatter01(column, row, 55U) * 0.20f;
            float height = CcLocalTerrainHeightAt(x, z);
            Color rock = height > 4.0f ? WORLD_STONE_LIGHT : WORLD_STONE;
            DrawTiltedBox((Vector3){x, height + radius * 0.34f, z},
                          (Vector3){radius * 1.55f, radius * 0.72f,
                                    radius * 1.18f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          TerrainScatter01(column, row, 56U) * 42.0f,
                          rock);
            if (radius > 0.27f) {
                DrawTiltedBox(
                    (Vector3){x + radius * 0.32f,
                              height + radius * 0.62f,
                              z - radius * 0.14f},
                    (Vector3){radius * 0.82f, radius * 0.58f,
                              radius * 0.72f},
                    (Vector3){0.0f, 1.0f, 0.0f},
                    TerrainScatter01(column, row, 57U) * 55.0f,
                    ShadeColor(rock, 1.08f));
            }
        }
    }
}

static void EmitTerrainDetailMesh(const CcSettlement *place,
                                  TerrainMeshWriter *writer)
{
    Vector3 world_center = {
        CC_LOCAL_WORLD_WIDTH * 0.5f, 0.0f,
        CC_LOCAL_WORLD_DEPTH * 0.5f,
    };
    terrain_detail_mesh_writer = writer;
    terrain_detail_cache_building = true;
    DrawTerrainRoadRuts(place, world_center);
    DrawTerrainFieldRows(place, world_center);
    DrawTerrainPlantCover(place, world_center);
    terrain_detail_cache_building = false;
    terrain_detail_mesh_writer = NULL;
}

static bool TerrainDetailRenderCacheBuild(const CcSettlement *place)
{
    TerrainDetailRenderCacheClear();
    TerrainEnsureReady();

    TerrainMeshWriter counter = {.count_only = true};
    EmitTerrainDetailMesh(place, &counter);
    if (counter.cursor <= 0) return false;

    TerrainMeshWriter writer;
    if (!TerrainMeshWriterAllocate(&writer, counter.cursor)) {
        TraceLog(LOG_WARNING,
                 "TERRAIN: could not allocate detail cache (%d vertices)",
                 counter.cursor);
        return false;
    }
    EmitTerrainDetailMesh(place, &writer);
    if (writer.cursor != counter.cursor) {
        TraceLog(LOG_WARNING,
                 "TERRAIN: detail cache count mismatch (%d/%d)",
                 writer.cursor, counter.cursor);
        UnloadMesh(writer.mesh);
        return false;
    }

    UploadMesh(&writer.mesh, false);
    terrain_detail_render_cache.model = LoadModelFromMesh(writer.mesh);
    ReleaseUploadedModelCpuData(&terrain_detail_render_cache.model);
    ApplyWorldShader(&terrain_detail_render_cache.model);
    terrain_detail_render_cache.seed = street_terrain_seed;
    terrain_detail_render_cache.place_id = place != NULL ? place->id : 0U;
    terrain_detail_render_cache.hunger_band =
        place != NULL ? place->hunger / 5 : 0;
    terrain_detail_render_cache.prosperity_band =
        place != NULL ? place->prosperity / 5 : 10;
    terrain_detail_render_cache.vertex_count = counter.cursor;
    terrain_detail_render_cache.ready = true;
    TraceLog(LOG_INFO, "TERRAIN: cached %d detail vertices on the GPU",
             counter.cursor);
    return true;
}

static void DrawTerrainSurfaceDetails(const CcSettlement *place,
                                      Vector3 focus)
{
    int32_t hunger_band = place != NULL ? place->hunger / 5 : 0;
    int32_t prosperity_band = place != NULL ? place->prosperity / 5 : 10;
    bool stale = !terrain_detail_render_cache.ready ||
                 terrain_detail_render_cache.seed != street_terrain_seed ||
                 terrain_detail_render_cache.place_id !=
                     (place != NULL ? place->id : 0U) ||
                 terrain_detail_render_cache.hunger_band != hunger_band ||
                 terrain_detail_render_cache.prosperity_band !=
                     prosperity_band;
    if (stale && !TerrainDetailRenderCacheBuild(place)) {
        DrawTerrainRoadRuts(place, focus);
        DrawTerrainFieldRows(place, focus);
        DrawTerrainPlantCover(place, focus);
    } else {
        DrawModel(terrain_detail_render_cache.model,
                  (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    }
    DrawTerrainRocks(focus);
}

static void DrawTerrainPlacedLantern(float x, float z)
{
    rlPushMatrix();
    rlTranslatef(0.0f, CcLocalTerrainHeightAt(x, z), 0.0f);
    DrawStreetLantern(x, z, ActiveTownRoadSurface());
    rlPopMatrix();
}

static void DrawTerrainStageBox(float x, float z, float width, float depth,
                                float height, Color color)
{
    float ground = CcLocalTerrainHeightAt(x + width * 0.5f,
                                           z + depth * 0.5f);
    DrawBox((Vector3){x + width * 0.5f, ground + height * 0.5f,
                      z + depth * 0.5f},
            (Vector3){width, height, depth}, color);
}

static void DrawArrivalBridge(bool fortified)
{
    float ground = CcLocalTerrainHeightAt(91.0f, 34.0f);
    Color stone = fortified ? WORLD_STONE : WORLD_WOOD_LIGHT;
    Color shadow = ShadeColor(stone, 0.58f);
    DrawBox((Vector3){91.0f, ground - 0.10f, 34.0f},
            (Vector3){12.5f, 0.32f, 4.75f}, shadow);
    DrawBox((Vector3){91.0f, ground + 0.06f, 34.0f},
            (Vector3){12.5f, 0.10f, 4.45f}, stone);
    for (int32_t side = -1; side <= 1; side += 2) {
        float rail_z = 34.0f + (float)side * 2.12f;
        DrawBox((Vector3){91.0f, ground + 0.55f, rail_z},
                (Vector3){12.2f, fortified ? 1.05f : 0.72f, 0.28f},
                stone);
        for (int32_t post = 0; post < 5; ++post) {
            DrawBox((Vector3){86.2f + (float)post * 2.4f,
                              ground + 0.76f, rail_z},
                    (Vector3){0.34f, fortified ? 1.46f : 1.10f, 0.42f},
                    ShadeColor(stone, post % 2 == 0 ? 0.86f : 1.04f));
        }
    }
}

static void DrawFarmTerrainStage(Vector3 focus)
{
    Color water = BlendColor(WORLD_TEAL, WORLD_INK, 0.42f);
    for (float z = 2.0f; z < 72.0f; z += 4.0f) {
        float river_x = 91.0f + sinf(z * 0.082f) * 1.7f;
        if (!SceneryPointVisible(river_x, z, focus)) continue;
        float next_x = 91.0f + sinf((z + 4.0f) * 0.082f) * 1.7f;
        float angle = -atan2f(next_x - river_x, 4.0f) * RAD2DEG;

        float ground = TerrainSampleGrid(street_terrain_natural,
                                         river_x, z);
        DrawTiltedBox((Vector3){(river_x + next_x) * 0.5f,
                                ground + 0.045f, z + 2.0f},
                      (Vector3){7.2f, 0.07f, 4.5f},
                      (Vector3){0.0f, 1.0f, 0.0f}, angle, water);
    }
    if (SceneryPointVisible(91.0f, 34.0f, focus)) {
        DrawArrivalBridge(false);
    }
    if (SceneryPointVisible(45.0f, 29.0f, focus)) {
        for (int32_t terrace = 0; terrace < 3; ++terrace) {
            float radius = 4.7f - (float)terrace * 0.72f;
            float y = CcLocalTerrainHeightAt(45.0f, 29.5f) +
                      0.03f + (float)terrace * 0.035f;
            DrawCylinder((Vector3){45.0f, y, 29.5f},
                         radius, radius, 0.06f, 20,
                         terrace == 2 ? WORLD_CROP_LIGHT : WORLD_EARTH);
        }
    }
}

static void DrawMiningTerrainStage(Vector3 focus)
{
    Color iron = WORLD_METAL_SHADOW;
    Color timber = WORLD_WOOD_SHADOW;
    for (float x = 65.0f; x < 96.0f; x += 3.5f) {
        if (!SceneryPointVisible(x, 34.0f, focus)) continue;
        float ground = CcLocalTerrainHeightAt(x, 34.0f) + 0.055f;
        for (int32_t rail = -1; rail <= 1; rail += 2) {
            DrawBox((Vector3){x + 1.75f, ground, 34.0f + (float)rail * 0.62f},
                    (Vector3){3.65f, 0.10f, 0.10f}, iron);
        }
        DrawBox((Vector3){x + 1.75f, ground - 0.035f, 34.0f},
                (Vector3){0.18f, 0.10f, 2.10f}, timber);
    }
    if (SceneryPointVisible(75.0f, 42.0f, focus)) {
        DrawTerrainStageBox(69.0f, 40.8f, 18.0f, 0.65f, 2.25f,
                            WORLD_STONE_SHADOW);
        DrawTerrainStageBox(73.0f, 44.0f, 14.0f, 0.65f, 3.30f,
                            ShadeColor(WORLD_STONE_SHADOW, 0.84f));
    }
}

static void DrawMarketTerrainStage(Vector3 focus)
{
    if (SceneryPointVisible(46.0f, 29.5f, focus)) {
        float ground = CcLocalTerrainHeightAt(46.0f, 29.5f);
        for (int32_t ring = 0; ring < 4; ++ring) {
            float radius = 6.1f - (float)ring * 1.15f;
            DrawCylinder((Vector3){46.0f, ground + 0.025f +
                                   (float)ring * 0.025f, 29.5f},
                         radius, radius, 0.055f, 24,
                         ring == 3 ? WORLD_GOLD : WORLD_STONE);
        }
        DrawCylinder((Vector3){46.0f, ground + 0.26f, 29.5f},
                     0.62f, 0.78f, 0.48f, 14, WORLD_STONE_LIGHT);
    }
    for (float x = 66.0f; x < 96.0f; x += 5.0f) {
        if (!SceneryPointVisible(x, 34.0f, focus)) continue;
        float ground = CcLocalTerrainHeightAt(x, 34.0f);
        DrawBox((Vector3){x, ground + 0.10f, 31.95f},
                (Vector3){4.2f, 0.20f, 0.24f}, WORLD_STONE_LIGHT);
        DrawBox((Vector3){x, ground + 0.10f, 36.05f},
                (Vector3){4.2f, 0.20f, 0.24f}, WORLD_STONE_LIGHT);
    }
}

static void DrawFortressTerrainStage(Vector3 focus)
{
    for (float z = 2.0f; z < 72.0f; z += 4.0f) {
        float ravine_x = 91.0f + sinf(z * 0.052f) * 1.2f;
        if (!SceneryPointVisible(ravine_x, z, focus)) continue;
        float next_x = 91.0f + sinf((z + 4.0f) * 0.052f) * 1.2f;
        float floor = TerrainSampleGrid(street_terrain_natural,
                                        ravine_x, z);
        DrawTiltedBox((Vector3){(ravine_x + next_x) * 0.5f,
                                floor + 0.035f, z + 2.0f},
                      (Vector3){5.6f, 0.07f, 4.4f},
                      (Vector3){0.0f, 1.0f, 0.0f},
                      -atan2f(next_x - ravine_x, 4.0f) * RAD2DEG,
                      WORLD_INK);
    }
    if (SceneryPointVisible(91.0f, 34.0f, focus)) {
        DrawArrivalBridge(true);
        for (int32_t side = -1; side <= 1; side += 2) {
            float z = 34.0f + (float)side * 3.35f;
            DrawTerrainStageBox(86.0f, z - 0.65f, 2.4f, 1.3f, 4.8f,
                                WORLD_STONE_SHADOW);
            DrawTerrainStageBox(93.5f, z - 0.65f, 2.4f, 1.3f, 4.8f,
                                WORLD_STONE_SHADOW);
        }
    }
    if (SceneryPointVisible(46.0f, 29.5f, focus)) {
        for (int32_t step = 0; step < 5; ++step) {
            float x = 39.0f + (float)step * 3.2f;
            float ground = CcLocalTerrainHeightAt(x, 29.5f);
            DrawBox((Vector3){x, ground + 0.06f, 29.5f},
                    (Vector3){2.7f, 0.12f, 7.2f},
                    step % 2 == 0 ? WORLD_STONE : WORLD_ROAD_LIGHT);
        }
    }
}

static void DrawCapitalTerrainStage(Vector3 focus)
{
    for (int32_t stair = 0; stair < 8; ++stair) {
        float x = 58.0f + (float)stair * 4.1f;
        if (!SceneryPointVisible(x, 34.0f, focus)) continue;
        float ground = CcLocalTerrainHeightAt(x, 34.0f);
        DrawBox((Vector3){x, ground + 0.075f, 34.0f},
                (Vector3){3.7f, 0.15f, 5.2f},
                stair % 2 == 0 ? WORLD_STONE_LIGHT : WORLD_STONE);
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawSmallSphere((Vector3){x, ground + 0.22f,
                                      34.0f + (float)side * 2.45f},
                            0.18f, WORLD_GOLD);
        }
    }
    if (SceneryPointVisible(75.0f, 25.0f, focus)) {
        DrawTerrainStageBox(66.0f, 25.0f, 24.0f, 0.55f, 2.20f,
                            WORLD_STONE_LIGHT);
        DrawTerrainStageBox(69.0f, 21.0f, 20.0f, 0.55f, 3.20f,
                            WORLD_STONE);
    }
}

static void DrawDungeonTerrainStage(Vector3 focus)
{
    if (SceneryPointVisible(48.5f, 35.5f, focus)) {
        float ground = CcLocalTerrainHeightAt(48.5f, 35.5f);
        DrawCylinder((Vector3){48.5f, ground + 0.025f, 35.5f},
                     3.75f, 3.75f, 0.05f, 22, WORLD_INK);
        DrawCylinder((Vector3){48.5f, ground + 0.065f, 35.5f},
                     3.20f, 3.20f, 0.04f, 22,
                     BlendColor(WORLD_VIOLET, WORLD_INK, 0.72f));
        for (int32_t post = 0; post < 10; ++post) {
            float angle = (float)post / 10.0f * PI * 2.0f;
            DrawBox((Vector3){48.5f + cosf(angle) * 4.25f,
                              ground + 0.68f,
                              35.5f + sinf(angle) * 4.25f},
                    (Vector3){0.16f, 1.36f, 0.16f}, WORLD_WOOD_SHADOW);
        }
    }
    for (int32_t stone = 0; stone < 4; ++stone) {
        float x = 84.0f + (float)stone * 3.1f;
        float z = stone % 2 == 0 ? 29.8f : 38.2f;
        if (!SceneryPointVisible(x, z, focus)) continue;
        float ground = CcLocalTerrainHeightAt(x, z);
        DrawTiltedBox((Vector3){x, ground + 1.30f, z},
                      (Vector3){0.62f, 2.60f, 0.74f},
                      (Vector3){0.0f, 0.0f, 1.0f},
                      stone % 2 == 0 ? 7.0f : -9.0f,
                      WORLD_STONE_SHADOW);
    }
}

static void DrawBackdropMass(float x, float base_y, float z,
                             float radius_x, float radius_z,
                             float upper_ratio, float height,
                             int32_t sides, Color color)
{

    rlPushMatrix();
    rlTranslatef(x, base_y, z);
    rlScalef(radius_x, 1.0f, radius_z);
    DrawCylinder((Vector3){0.0f, 0.0f, 0.0f}, upper_ratio, 1.0f,
                 height, sides, color);
    rlPopMatrix();
}

static void DrawBackdropPeak(float x, float base_y, float z,
                             float radius_x, float radius_z,
                             float height, int32_t sides,
                             Color body, Color face)
{
    DrawBackdropMass(x, base_y, z, radius_x, radius_z, 0.24f,
                     height, sides, body);
    DrawBackdropMass(x - radius_x * 0.10f,
                     base_y + height * 0.47f,
                     z + radius_z * 0.02f,
                     radius_x * 0.54f, radius_z * 0.58f, 0.05f,
                     height * 0.56f, sides > 5 ? sides - 1 : sides,
                     face);
}

static void DrawBackdropTower(float x, float base_y, float z,
                              float width, float height,
                              Color stone, Color roof)
{
    DrawCubeV((Vector3){x, base_y + height * 0.5f, z},
              (Vector3){width, height, width}, stone);
    DrawCylinder((Vector3){x, base_y + height, z},
                 0.04f, width * 0.82f, width * 0.72f, 6, roof);
}

static void DrawBackdropWall(float start_x, float finish_x, float base_y,
                             float z, float height, int32_t tower_count,
                             Color stone, Color roof)
{
    float width = finish_x - start_x;
    DrawCubeV((Vector3){start_x + width * 0.5f,
                        base_y + height * 0.42f, z},
              (Vector3){width, height * 0.84f, 0.72f}, stone);
    for (int32_t tower = 0; tower < tower_count; ++tower) {
        float amount = tower_count > 1 ?
            (float)tower / (float)(tower_count - 1) : 0.5f;
        DrawBackdropTower(start_x + width * amount, base_y, z,
                          1.35f, height * 1.46f, stone, roof);
    }
}

static void DrawFarmingBackdrop(void)
{
    Color far = BlendColor(WORLD_GRASS_SHADOW, WORLD_INK, 0.62f);
    Color far_face = BlendColor(far, WORLD_STONE_SHADOW, 0.20f);
    Color middle = BlendColor(WORLD_GRASS, WORLD_STONE_SHADOW, 0.58f);
    for (int32_t peak = 0; peak < 7; ++peak) {
        float height = 31.0f + (float)((peak * 7) % 13);
        DrawBackdropPeak(-72.0f + (float)peak * 46.0f,
                         -48.0f - height,
                         -132.0f + (float)(peak % 2) * 9.0f,
                         45.0f, 31.0f, height, 11,
                         far, far_face);
    }
    for (int32_t shoulder = 0; shoulder < 7; ++shoulder) {
        float height = 17.0f + (float)(shoulder % 3) * 3.0f;
        DrawBackdropMass(-48.0f + (float)shoulder * 37.0f,
                         -25.0f - height,
                         -68.0f + (float)(shoulder % 2) * 4.0f,
                         31.0f, 18.0f, 0.56f, height,
                         12, middle);
    }
}

static void DrawMiningBackdrop(void)
{
    Color far = BlendColor(WORLD_METAL_SHADOW, WORLD_INK, 0.52f);
    Color far_face = BlendColor(WORLD_STONE, WORLD_INK, 0.56f);
    Color middle = BlendColor(WORLD_STONE_SHADOW,
                              WORLD_METAL_SHADOW, 0.48f);
    for (int32_t peak = 0; peak < 8; ++peak) {
        float height = 61.0f + (float)((peak * 13) % 31);
        DrawBackdropPeak(-70.0f + (float)peak * 39.0f,
                         -49.0f - height,
                         -138.0f + (float)(peak % 3) * 8.0f,
                         35.0f, 25.0f, height, 6,
                         far, far_face);
    }
    for (int32_t shelf = 0; shelf < 6; ++shelf) {
        float height = 27.0f + (float)(shelf % 3) * 4.0f;
        DrawBackdropMass(-45.0f + (float)shelf * 45.0f,
                         -26.0f - height,
                         -76.0f + (float)(shelf % 2) * 5.0f,
                         40.0f, 19.0f, 0.64f, height,
                         7, middle);
    }
}

static void DrawMarketBackdrop(void)
{
    Color far = BlendColor(WORLD_VIOLET, WORLD_INK, 0.76f);
    Color far_face = BlendColor(far, WORLD_STONE_SHADOW, 0.24f);
    Color middle = BlendColor(WORLD_STONE, WORLD_INK, 0.54f);
    for (int32_t peak = 0; peak < 7; ++peak) {
        float height = 38.0f + (float)((peak * 5) % 16);
        DrawBackdropPeak(-68.0f + (float)peak * 46.0f,
                         -48.0f - height,
                         -136.0f + (float)(peak % 2) * 7.0f,
                         43.0f, 27.0f,
                         height, 9,
                         far, far_face);
    }
    for (int32_t basin = 0; basin < 6; ++basin) {
        float height = 22.0f + (float)(basin % 3) * 3.0f;
        DrawBackdropMass(-42.0f + (float)basin * 44.0f,
                         -25.0f - height,
                         -72.0f + (float)(basin % 2) * 4.0f,
                         38.0f, 20.0f, 0.62f, height,
                         10, middle);
    }
}

static void DrawFortressBackdrop(void)
{
    Color far = BlendColor(WORLD_STONE_SHADOW, WORLD_INK, 0.42f);
    Color far_face = BlendColor(WORLD_STONE, WORLD_INK, 0.45f);
    Color middle = BlendColor(WORLD_STONE_SHADOW, WORLD_INK, 0.24f);
    Color wall = BlendColor(WORLD_STONE, WORLD_METAL_SHADOW, 0.30f);
    for (int32_t peak = 0; peak < 8; ++peak) {
        float height = 69.0f + (float)((peak * 17) % 35);
        DrawBackdropPeak(-72.0f + (float)peak * 40.0f,
                         -52.0f - height,
                         -142.0f + (float)(peak % 2) * 7.0f,
                         35.0f, 25.0f, height, 5,
                         far, far_face);
    }
    for (int32_t spur = 0; spur < 7; ++spur) {
        float height = 37.0f + (float)((spur * 7) % 15);
        DrawBackdropPeak(-55.0f + (float)spur * 38.0f,
                         -27.0f - height,
                         -78.0f + (float)(spur % 2) * 5.0f,
                         29.0f, 18.0f, height, 6,
                         middle, ShadeColor(middle, 1.10f));
    }
    Color roof = ShadeColor(WORLD_DANGER, 0.56f);
    float pass_z = -10.0f;
    float pass_ground = TerrainVisualHeightAt(62.0f, pass_z) + 0.12f;
    DrawBackdropWall(-1.0f, 130.0f, pass_ground, pass_z, 2.0f, 8,
                     wall, roof);
    DrawBackdropTower(64.0f, pass_ground, pass_z, 1.25f, 4.8f,
                      ShadeColor(wall, 1.06f), roof);
}

static void DrawCapitalBackdrop(void)
{
    Color far = BlendColor(WORLD_VIOLET, WORLD_STONE_SHADOW, 0.70f);
    Color far_face = BlendColor(WORLD_STONE_SHADOW, WORLD_INK, 0.40f);
    Color plateau = BlendColor(WORLD_STONE, WORLD_INK, 0.47f);
    for (int32_t peak = 0; peak < 7; ++peak) {
        float height = 46.0f + (float)((peak * 11) % 20);
        DrawBackdropPeak(-75.0f + (float)peak * 48.0f,
                         -48.0f - height,
                         -144.0f + (float)(peak % 2) * 8.0f,
                         46.0f, 30.0f, height, 9,
                         far, far_face);
    }
    for (int32_t shelf = 0; shelf < 6; ++shelf) {
        float height = 25.0f + (float)(shelf % 3) * 2.8f;
        DrawBackdropMass(-43.0f + (float)shelf * 45.0f,
                         -25.0f - height,
                         -76.0f + (float)(shelf % 2) * 4.0f,
                         41.0f, 20.0f, 0.72f, height,
                         10, plateau);
    }
}

static void DrawDungeonBackdrop(void)
{
    Color far = BlendColor(WORLD_VIOLET, WORLD_INK, 0.84f);
    Color far_face = BlendColor(WORLD_STONE_SHADOW, WORLD_INK, 0.68f);
    Color middle = BlendColor(WORLD_STONE_SHADOW, WORLD_INK, 0.50f);
    for (int32_t peak = 0; peak < 9; ++peak) {
        float height = 67.0f + (float)((peak * 19) % 41);
        DrawBackdropPeak(-70.0f + (float)peak * 36.0f,
                         -53.0f - height,
                         -142.0f + (float)(peak % 3) * 6.0f,
                         31.0f, 23.0f, height, 5,
                         far, far_face);
    }
    for (int32_t tooth = 0; tooth < 8; ++tooth) {
        float height = 38.0f + (float)((tooth * 11) % 23);
        DrawBackdropPeak(-50.0f + (float)tooth * 35.0f,
                         -28.0f - height,
                         -77.0f + (float)(tooth % 2) * 4.0f,
                         25.0f, 17.0f, height, 5,
                         middle, ShadeColor(middle, 1.08f));
    }
}

static void DrawTownBackdrop(const CcSettlement *place)
{
    if (place == NULL) return;


    switch (place->function) {
        case CC_SETTLEMENT_FARMING: DrawFarmingBackdrop(); break;
        case CC_SETTLEMENT_MINING: DrawMiningBackdrop(); break;
        case CC_SETTLEMENT_MARKET: DrawMarketBackdrop(); break;
        case CC_SETTLEMENT_FORTRESS: DrawFortressBackdrop(); break;
        case CC_SETTLEMENT_CAPITAL: DrawCapitalBackdrop(); break;
        case CC_SETTLEMENT_DUNGEON_TOWN: DrawDungeonBackdrop(); break;
    }
}

static void DrawTownTerrainStage(const CcSettlement *place, Vector3 focus)
{
    if (place == NULL) return;
    switch (place->function) {
        case CC_SETTLEMENT_FARMING: DrawFarmTerrainStage(focus); break;
        case CC_SETTLEMENT_MINING: DrawMiningTerrainStage(focus); break;
        case CC_SETTLEMENT_MARKET: DrawMarketTerrainStage(focus); break;
        case CC_SETTLEMENT_FORTRESS: DrawFortressTerrainStage(focus); break;
        case CC_SETTLEMENT_CAPITAL: DrawCapitalTerrainStage(focus); break;
        case CC_SETTLEMENT_DUNGEON_TOWN:
            DrawDungeonTerrainStage(focus);
            break;
    }
}

static void DrawExteriorTerrain(const CcSettlement *place, Vector3 focus)
{
    DrawTownBackdrop(place);
    SetWorldTerrainSurface(true);
    DrawCachedTerrain(place);
    DrawTerrainSurfaceDetails(place, focus);
    SetWorldTerrainSurface(false);
    DrawTownTerrainStage(place, focus);
    if (SceneryPointVisible(46.15f, 26.10f, focus)) {
        DrawTerrainPlacedLantern(46.15f, 26.10f);
    }
    if (SceneryPointVisible(53.85f, 26.10f, focus)) {
        DrawTerrainPlacedLantern(53.85f, 26.10f);
    }
}

static Color BuildingWallColor(int32_t style,
                               const CcLocalPlaceProfile *profile)
{
    Color wall;
    switch (style) {
        case 1: wall = WORLD_STONE; break;
        case 2:
            wall = BlendColor(WORLD_EARTH_LIGHT, WORLD_DANGER, 0.14f);
            break;
        case 3:
            wall = BlendColor(WORLD_STONE, WORLD_TEAL, 0.16f);
            break;
        default:
            wall = BlendColor(WORLD_STONE, WORLD_GRASS, 0.24f);
            break;
    }
    if (profile == NULL) return wall;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(wall, WORLD_CROP_LIGHT, 0.16f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(wall, WORLD_METAL_SHADOW, 0.22f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(wall, WORLD_STONE_LIGHT, 0.16f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(wall, WORLD_GOLD, 0.10f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(wall, WORLD_VIOLET, 0.14f);
        case CC_SETTLEMENT_MARKET: return wall;
    }
    return wall;
}

static Color BuildingRoofColor(int32_t style, Color kingdom,
                               const CcLocalPlaceProfile *profile)
{
    Color roof;
    switch (style) {
        case 1:
            roof = BlendColor(WORLD_ROAD_SHADOW, kingdom, 0.18f);
            break;
        case 2: roof = WORLD_EARTH; break;
        case 3:
            roof = BlendColor(WORLD_METAL_SHADOW, WORLD_VIOLET, 0.22f);
            break;
        default:
            roof = BlendColor(WORLD_FOLIAGE_SHADOW, kingdom, 0.16f);
            break;
    }
    if (profile == NULL) return roof;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(roof, WORLD_FOLIAGE, 0.18f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(roof, WORLD_METAL_SHADOW, 0.24f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(roof, WORLD_DANGER, 0.12f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(roof, WORLD_GOLD, 0.12f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(roof, WORLD_VIOLET, 0.16f);
        case CC_SETTLEMENT_MARKET: return roof;
    }
    return roof;
}

bool CcLocalRayMissesBoxParallelSlabInternal(Ray ray, BoundingBox box)
{
    return (fabsf(ray.direction.x) <= FLT_EPSILON &&
            (ray.position.x < box.min.x || ray.position.x > box.max.x)) ||
           (fabsf(ray.direction.y) <= FLT_EPSILON &&
            (ray.position.y < box.min.y || ray.position.y > box.max.y)) ||
           (fabsf(ray.direction.z) <= FLT_EPSILON &&
            (ray.position.z < box.min.z || ray.position.z > box.max.z));
}

bool CcLocalBuildingObscuresHeroInternal(Rectangle footprint, float height,
                                         Camera3D camera,
                                         Vector3 hero_center,
                                         int32_t render_width,
                                         int32_t render_height)
{
    if (render_width <= 0 || render_height <= 0 || height <= 0.0f) {
        return false;
    }
    Vector3 camera_forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    Vector3 camera_right = PhysicsNormalizeOr(
        Vector3CrossProduct(camera_forward, camera.up),
        (Vector3){1.0f, 0.0f, 0.0f});
    float base = TerrainFootprintHeight(footprint);
    BoundingBox visible_walls = {
        .min = {footprint.x, base, footprint.y},
        .max = {footprint.x + footprint.width, base + height,
                footprint.y + footprint.height},
    };
    Vector3 samples[] = {
        hero_center,
        Vector3Add(hero_center, (Vector3){0.0f, 0.78f, 0.0f}),
        Vector3Add(hero_center, (Vector3){0.0f, -0.72f, 0.0f}),
        Vector3Add(Vector3Add(hero_center,
                             Vector3Scale(camera_right, 0.38f)),
                   (Vector3){0.0f, 0.16f, 0.0f}),
        Vector3Add(Vector3Add(hero_center,
                             Vector3Scale(camera_right, -0.38f)),
                   (Vector3){0.0f, 0.16f, 0.0f}),
    };

    for (int32_t sample = 0;
         sample < (int32_t)(sizeof(samples) / sizeof(samples[0])); ++sample) {
        Vector2 screen = GetWorldToScreenEx(
            samples[sample], camera, render_width, render_height);
        Ray ray = GetScreenToWorldRayEx(screen, camera,
                                        render_width, render_height);
        if (CcLocalRayMissesBoxParallelSlabInternal(ray, visible_walls)) {
            continue;
        }
        RayCollision collision = GetRayCollisionBox(ray, visible_walls);
        if (!collision.hit) continue;
        float hero_depth = Vector3DotProduct(
            Vector3Subtract(samples[sample], camera.position),
            camera_forward);
        float collision_depth = Vector3DotProduct(
            Vector3Subtract(collision.point, camera.position),
            camera_forward);
        if (collision_depth + 0.18f < hero_depth) return true;
    }
    return false;
}

static Vector3 secondary_reveal_subject;
static bool secondary_reveal_valid;

static bool WorldBuildingObscuresReveal(const WorldBuilding *building,
                                        Camera3D camera,
                                        Vector3 reveal_world,
                                        int32_t render_width,
                                        int32_t render_height)
{
    if (building == NULL) return false;
    return CcLocalBuildingObscuresHeroInternal(
        building->footprint, building->height, camera, reveal_world,
        render_width, render_height) ||
        (secondary_reveal_valid && CcLocalBuildingObscuresHeroInternal(
            building->footprint, building->height, camera, secondary_reveal_subject,
            render_width, render_height));
}

typedef struct WorldBuildingRevealState {
    float amount;
    float pending_seconds;
    bool revealed;
} WorldBuildingRevealState;

static void UpdateBuildingReveal(WorldBuildingRevealState *state,
                                  bool occluded, bool reset, float delta_time,
                                  float clear_delay, float close_response)
{
    if (reset) {
        state->amount = occluded ? 1.0f : 0.0f;
        state->pending_seconds = 0.0f;
        state->revealed = occluded;
        return;
    }
    if (delta_time <= 0.0f) return;

    // Hold the current target until the opposite sightline lasts long enough.
    if (occluded == state->revealed) {
        state->pending_seconds = 0.0f;
    } else {
        state->pending_seconds += delta_time;
        float delay = occluded ? 0.06f : clear_delay;
        if (state->pending_seconds >= delay) {
            state->revealed = occluded;
            state->pending_seconds = 0.0f;
        }
    }

    float destination = state->revealed ? 1.0f : 0.0f;
    float response = state->revealed ? 8.0f : close_response;
    float blend = 1.0f - expf(-response * delta_time);
    state->amount += (destination - state->amount) * blend;
    if (fabsf(destination - state->amount) < 0.002f) {
        state->amount = destination;
    }
}

static WorldBuildingRevealState world_building_reveals[
    CC_LOCAL_PLACE_BUILDING_CAPACITY];
static uint32_t world_building_reveal_seed;
static float world_building_reveal_clock = 0.0f;
static bool world_building_reveals_initialized = false;
static CcSettlementFunction world_building_reveal_function =
    CC_SETTLEMENT_MARKET;

static void UpdateWorldBuildingReveals(Camera3D camera,
                                       Vector3 reveal_world,
                                       int32_t render_width,
                                       int32_t render_height,
                                       float clock)
{
    bool reset = !world_building_reveals_initialized ||
                 world_building_reveal_seed != street_terrain_seed ||
                 clock < world_building_reveal_clock ||
                 world_building_reveal_function != active_place_function;
    float delta_time = reset ? 0.0f :
        fmaxf(0.0f, fminf(clock - world_building_reveal_clock, 0.05f));
    world_building_reveal_clock = clock;
    world_building_reveal_seed = street_terrain_seed;
    world_building_reveals_initialized = true;
    world_building_reveal_function = active_place_function;

    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        WorldBuilding building = ActiveWorldBuildingAt(i);
        WorldBuildingRevealState *state = &world_building_reveals[i];
        bool occluded = WorldBuildingObscuresReveal(
            &building, camera, reveal_world,
            render_width, render_height);
        UpdateBuildingReveal(state, occluded, reset, delta_time, 0.14f, 5.0f);
    }
}

static void DrawWorldBuildings(Color kingdom, Vector3 focus,
                               const CcLocalPlaceProfile *profile,
                               Camera3D camera, Vector3 reveal_world,
                               int32_t render_width, int32_t render_height,
                               float clock)
{
    float reveal_cut_height = reveal_world.y - 0.30f;
    UpdateWorldBuildingReveals(camera, reveal_world,
                               render_width, render_height, clock);


    SetWorldForegroundReveal(0.0f, reveal_cut_height);
    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        WorldBuilding building = ActiveWorldBuildingAt(i);
        if (!SceneryFootprintVisible(building.footprint, focus)) continue;
        Color wall = BuildingWallColor(building.style, profile);
        rlPushMatrix();
        rlTranslatef(0.0f, TerrainFootprintHeight(building.footprint), 0.0f);
        DrawBuildingFoundation(
            building.footprint.x, building.footprint.y,
            building.footprint.width, building.footprint.height,
            building.height, wall);
        rlPopMatrix();
    }

    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        WorldBuilding building = ActiveWorldBuildingAt(i);
        if (!SceneryFootprintVisible(building.footprint, focus)) continue;

        if (profile->function == CC_SETTLEMENT_MARKET &&
            i == profile->primary_building &&
            runtime_assets[RUNTIME_ASSET_MARKET].ready) continue;
        float reveal = world_building_reveals[i].amount;
        float building_base = TerrainFootprintHeight(building.footprint);
        float active_cut_world = SetObjectForegroundReveal(reveal, reveal_cut_height,
            building_base + building.height + 3.5f);
        rlPushMatrix();
        rlTranslatef(0.0f, building_base, 0.0f);
        DrawSettlementBuilding(
            &building, BuildingWallColor(building.style, profile),
            BuildingRoofColor(building.style, kingdom, profile),
            profile, i);
        if (reveal > 0.05f) {
            DrawBuildingRevealCap(
                building.footprint.x, building.footprint.y,
                building.footprint.width, building.footprint.height,
                building.height, active_cut_world - building_base,
                BuildingWallColor(building.style, profile),
                BuildingRoofColor(building.style, kingdom, profile));
        }
        rlPopMatrix();
    }
    SetWorldForegroundReveal(0.0f, reveal_cut_height);
}

static bool DrawAuthoredMarket(const CcSettlement *place)
{
    RuntimeAsset *market = &runtime_assets[RUNTIME_ASSET_MARKET];
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(place);
    if (!market->ready || profile->function != CC_SETTLEMENT_MARKET) {
        return false;
    }
    const Vector3 origin = {50.0f, 0.0f, 21.0f};
    const float scale = 1.70f;
    WorldBuilding building = ActivePrimaryBuilding();
    rlPushMatrix();
    rlTranslatef(0.0f, TerrainFootprintHeight(building.footprint), 0.0f);
    DrawBuildingContactShadow(building.footprint.x, building.footprint.y,
                              building.footprint.width,
                              building.footprint.height, building.height);
    DrawModelEx(market->model, origin, (Vector3){0.0f, 1.0f, 0.0f},
                0.0f, (Vector3){scale, scale, scale}, WHITE);
    RuntimeAssetId state = RUNTIME_ASSET_COUNT;
    if (place != NULL && place->hunger >= 30) {
        state = RUNTIME_ASSET_SHORTAGE;
    } else if (place != NULL && place->security >= 70) {
        state = RUNTIME_ASSET_ENFORCEMENT;
    } else if (place != NULL && place->prosperity >= 60) {
        state = RUNTIME_ASSET_RECOVERY;
    }
    if (state < RUNTIME_ASSET_COUNT && runtime_assets[state].ready) {
        DrawModelEx(runtime_assets[state].model, origin,
                    (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                    (Vector3){scale, scale, scale}, WHITE);
    }
    rlPopMatrix();
    return true;
}

static WorldBuildingRevealState castle_structure_reveals[
    CC_LOCAL_PLACE_COMPOUND_CAPACITY];
static uint32_t castle_reveal_seed;
static float castle_reveal_clock = 0.0f;
static bool castle_reveals_initialized = false;
static CcSettlementFunction castle_reveal_function = CC_SETTLEMENT_MARKET;

static void UpdateCastleStructureReveals(Camera3D camera,
                                         Vector3 reveal_world,
                                         int32_t render_width,
                                         int32_t render_height,
                                         float clock)
{
    bool reset = !castle_reveals_initialized || clock < castle_reveal_clock ||
                 castle_reveal_seed != street_terrain_seed ||
                 castle_reveal_function != active_place_function;
    float delta_time = reset ? 0.0f :
        fmaxf(0.0f, fminf(clock - castle_reveal_clock, 0.05f));
    castle_reveal_clock = clock;
    castle_reveal_seed = street_terrain_seed;
    castle_reveals_initialized = true;
    castle_reveal_function = active_place_function;
    for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
        WorldStructure structure = ActiveCompoundStructureAt(i);
        WorldBuilding proxy = {
            structure.footprint, structure.height, 0, false,
        };
        WorldBuildingRevealState *state = &castle_structure_reveals[i];
        bool occluded = WorldBuildingObscuresReveal(
            &proxy, camera, reveal_world,
            render_width, render_height);
        UpdateBuildingReveal(state, occluded, reset, delta_time, 0.16f, 4.6f);
    }
}

static void DrawCastleBattlements(Rectangle footprint, float height,
                                  Color stone)
{
    const float spacing = 1.18f;
    int32_t front_count = (int32_t)fmaxf(1.0f,
        floorf(footprint.width / spacing));
    float front_step = footprint.width / (float)front_count;
    for (int32_t block = 0; block < front_count; ++block) {
        float block_x = footprint.x + front_step * ((float)block + 0.5f);
        DrawBox((Vector3){block_x, height + 0.42f,
                          footprint.y + footprint.height - 0.10f},
                (Vector3){fminf(0.66f, front_step * 0.62f), 0.78f, 0.56f},
                block != front_count - 1 ? stone : ShadeColor(stone, 0.88f));
    }
    int32_t side_count = (int32_t)fmaxf(1.0f,
        floorf(footprint.height / spacing));
    float side_step = footprint.height / (float)side_count;
    for (int32_t block = 0; block < side_count; ++block) {
        float block_z = footprint.y + side_step * ((float)block + 0.5f);
        DrawBox((Vector3){footprint.x + footprint.width - 0.10f,
                          height + 0.42f, block_z},
                (Vector3){0.56f, 0.78f,
                          fminf(0.66f, side_step * 0.62f)},
                ShadeColor(stone, block != side_count - 1 ? 0.94f : 0.84f));
    }
}

static bool CompoundUsesBattlements(const CcLocalPlaceProfile *profile)
{
    return profile != NULL &&
           (profile->function == CC_SETTLEMENT_MARKET ||
            profile->function == CC_SETTLEMENT_FORTRESS ||
            profile->function == CC_SETTLEMENT_CAPITAL);
}

static void DrawConstructedCompoundStructure(
    const WorldStructure *structure, Color stone, Color kingdom,
    const CcLocalPlaceProfile *profile)
{
    if (structure == NULL) return;
    Rectangle footprint = structure->footprint;
    float height = structure->height;
    float center_x = footprint.x + footprint.width * 0.5f;
    float center_z = footprint.y + footprint.height * 0.5f;
    bool wall_run = structure->kind == CC_LOCAL_COMPOUND_WALL;
    bool keep = structure->kind == CC_LOCAL_COMPOUND_HALL;
    bool tower = structure->kind == CC_LOCAL_COMPOUND_TOWER;
    Color base = ShadeColor(stone, 0.70f);

    if (structure->kind == CC_LOCAL_COMPOUND_HALL ||
        structure->kind == CC_LOCAL_COMPOUND_STOREHOUSE) {
        int32_t style = structure->kind == CC_LOCAL_COMPOUND_HALL ?
            (profile->function == CC_SETTLEMENT_CAPITAL ||
             profile->function == CC_SETTLEMENT_FORTRESS ? 2 : 1) : 1;
        Color roof = BuildingRoofColor(style, kingdom, profile);
        WorldBuilding building = {
            .footprint = footprint,
            .height = height,
            .style = style,
            .door = true,
        };
        DrawSettlementBuilding(&building, stone, roof, profile,
                               structure->kind == CC_LOCAL_COMPOUND_HALL ?
                                   -2 : -1);
        return;
    }

    if (structure->kind == CC_LOCAL_COMPOUND_SILO) {
        float radius = fminf(footprint.width, footprint.height) * 0.43f;
        DrawCylinder((Vector3){center_x, 0.0f, center_z},
                     radius, radius * 1.05f, height, 12, stone);
        DrawCylinder((Vector3){center_x, height, center_z},
                     0.08f, radius * 1.18f, radius * 0.90f, 12,
                     BuildingRoofColor(1, kingdom, profile));
        DrawBox((Vector3){center_x, height * 0.54f,
                          center_z + radius + 0.025f},
                (Vector3){radius * 1.15f, 0.18f, 0.08f},
                ShadeColor(stone, 0.68f));
        return;
    }

    if (tower && profile != NULL &&
        (profile->function == CC_SETTLEMENT_MARKET ||
         profile->function == CC_SETTLEMENT_CAPITAL ||
         profile->function == CC_SETTLEMENT_DUNGEON_TOWN)) {

        float radius = fminf(footprint.width, footprint.height) * 0.48f;
        Color tower_stone = profile->function ==
                CC_SETTLEMENT_DUNGEON_TOWN ? ShadeColor(stone, 0.76f) : stone;
        Color tower_roof = BuildingRoofColor(
            profile->function == CC_SETTLEMENT_CAPITAL ? 2 : 1,
            kingdom, profile);
        DrawBox((Vector3){center_x, 0.34f, center_z},
                (Vector3){footprint.width, 0.68f, footprint.height},
                ShadeColor(tower_stone, 0.66f));
        for (int32_t corner = 0; corner < 4; ++corner) {
            float buttress_x = (corner & 1) == 0 ?
                footprint.x + 0.24f :
                footprint.x + footprint.width - 0.24f;
            float buttress_z = (corner & 2) == 0 ?
                footprint.y + 0.24f :
                footprint.y + footprint.height - 0.24f;
            DrawBox((Vector3){buttress_x, height * 0.30f, buttress_z},
                    (Vector3){0.48f, height * 0.60f, 0.48f},
                    ShadeColor(tower_stone, 0.72f));
        }
        DrawRoundTower(center_x, center_z, radius, height,
                       tower_stone, tower_roof, false);
        return;
    }

    DrawBox((Vector3){center_x, 0.34f, center_z},
            (Vector3){footprint.width + 0.32f, 0.68f,
                      footprint.height + 0.32f}, base);
    DrawBox((Vector3){center_x, height * 0.50f, center_z},
            (Vector3){footprint.width, height, footprint.height}, stone);


    int32_t courses = wall_run ? 2 : 3;
    for (int32_t course = 1; course <= courses; ++course) {
        float y = height * (float)course / (float)(courses + 1);
        DrawBox((Vector3){center_x, y,
                          footprint.y + footprint.height + 0.055f},
                (Vector3){footprint.width + 0.12f, 0.16f, 0.15f},
                ShadeColor(stone, 0.82f));
        DrawBox((Vector3){footprint.x + footprint.width + 0.055f,
                          y, center_z},
                (Vector3){0.15f, 0.16f, footprint.height + 0.12f},
                ShadeColor(stone, 0.76f));
    }

    if (tower || keep) {
        float pier = tower ? 0.52f : 0.62f;
        for (int32_t corner = 0; corner < 4; ++corner) {
            float pier_x = (corner & 1) == 0 ?
                footprint.x + pier * 0.5f :
                footprint.x + footprint.width - pier * 0.5f;
            float pier_z = (corner & 2) == 0 ?
                footprint.y + pier * 0.5f :
                footprint.y + footprint.height - pier * 0.5f;
            DrawBox((Vector3){pier_x, height * 0.44f, pier_z},
                    (Vector3){pier, height * 0.88f, pier},
                    ShadeColor(stone, corner == 3 ? 0.76f : 0.86f));
            DrawBox((Vector3){pier_x, height * 0.89f, pier_z},
                    (Vector3){pier + 0.16f, 0.18f, pier + 0.16f},
                    keep && corner == 3 ? kingdom : ShadeColor(stone, 0.72f));
        }
    }

    DrawBox((Vector3){center_x, height + 0.10f, center_z},
            (Vector3){footprint.width + 0.30f, 0.22f,
                      footprint.height + 0.30f},
            tower ? BlendColor(ShadeColor(stone, 0.76f), kingdom, 0.18f) :
                    ShadeColor(stone, 0.70f));
    if (tower || CompoundUsesBattlements(profile)) {
        DrawCastleBattlements(footprint, height, stone);
    }
    if (wall_run) {
        bool horizontal = footprint.width >= footprint.height;
        float length = horizontal ? footprint.width : footprint.height;
        int32_t pier_count = (int32_t)fmaxf(1.0f, floorf(length / 3.2f));
        for (int32_t pier = 0; pier <= pier_count; ++pier) {
            float amount = (float)pier / (float)pier_count;
            float pier_x = horizontal ?
                footprint.x + footprint.width * amount :
                footprint.x + footprint.width + 0.13f;
            float pier_z = horizontal ?
                footprint.y + footprint.height + 0.13f :
                footprint.y + footprint.height * amount;
            DrawBox((Vector3){pier_x, height * 0.43f, pier_z},
                    horizontal ?
                        (Vector3){0.42f, height * 0.86f, 0.52f} :
                        (Vector3){0.52f, height * 0.86f, 0.42f},
                    ShadeColor(stone, 0.74f));
        }
    }
    if (tower && profile != NULL &&
        profile->function == CC_SETTLEMENT_FORTRESS) {
        DrawPitchedRoof(footprint.x + footprint.width * 0.10f,
                        footprint.y + footprint.height * 0.10f,
                        footprint.width * 0.80f,
                        footprint.height * 0.80f,
                        height + 0.35f, stone,
                        BuildingRoofColor(1, kingdom, profile));
    }
}

static Color CompoundStoneColor(const CcLocalPlaceProfile *profile,
                                Color kingdom, CcLocalCompoundKind kind)
{
    Color stone = kind == CC_LOCAL_COMPOUND_HALL ?
        WORLD_STONE_SHADOW : WORLD_STONE;
    if (profile == NULL) return stone;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(stone, WORLD_WOOD, 0.24f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(stone, WORLD_METAL_SHADOW, 0.28f);
        case CC_SETTLEMENT_MARKET:
            return BlendColor(stone, WORLD_TEAL, 0.10f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(stone, kingdom, 0.16f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(stone, WORLD_GOLD, 0.14f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(stone, WORLD_VIOLET, 0.18f);
    }
    return stone;
}

static void DrawCastle(const CcSim *sim, const CcSettlement *place,
                       Color kingdom, const CcLocalPlaceProfile *profile,
                       Vector3 focus, Camera3D camera,
                       Vector3 reveal_world,
                       int32_t render_width, int32_t render_height,
                       float clock)
{
    Rectangle keep_pad = ActiveCompoundBounds();
    float reveal_cut_height = reveal_world.y - 0.30f;
    float castle_base = TerrainFootprintHeight(keep_pad);
    UpdateCastleStructureReveals(camera, reveal_world,
                                 render_width, render_height, clock);
    if (!SceneryFootprintVisible(keep_pad, focus)) return;
    rlPushMatrix();
    rlTranslatef(0.0f, castle_base, 0.0f);
    SetWorldForegroundReveal(0.0f, reveal_cut_height);
    for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
        WorldStructure structure = ActiveCompoundStructureAt(i);
        Rectangle footprint = structure.footprint;
        Color stone = CompoundStoneColor(profile, kingdom, structure.kind);
        DrawBuildingFoundation(
            footprint.x, footprint.y, footprint.width, footprint.height,
            structure.height, stone);
    }
    for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
        WorldStructure structure = ActiveCompoundStructureAt(i);
        Color stone = CompoundStoneColor(profile, kingdom, structure.kind);
        float reveal = castle_structure_reveals[i].amount;
        (void)SetObjectForegroundReveal(reveal, reveal_cut_height,
            castle_base + structure.height + 3.5f);
        DrawConstructedCompoundStructure(&structure, stone, kingdom, profile);
    }
    SetWorldForegroundReveal(0.0f, reveal_cut_height);
    {
        WorldStructure compound_hall = ActiveCompoundStructureAt(5);
        float hall_door_x = compound_hall.footprint.x +
                            compound_hall.footprint.width * 0.5f;
        float hall_door_z = compound_hall.footprint.y +
                            compound_hall.footprint.height + 0.03f;
        float gate_x = 78.50f;
        float gate_z = ActiveCompoundStructureAt(3).footprint.y +
                       ActiveCompoundStructureAt(3).footprint.height * 0.5f;
        float gate_height = profile->function == CC_SETTLEMENT_FARMING ? 3.20f :
                            profile->function == CC_SETTLEMENT_MINING ? 5.40f :
                            profile->function == CC_SETTLEMENT_FORTRESS ? 8.40f :
                            profile->function == CC_SETTLEMENT_CAPITAL ? 7.60f :
                            profile->function == CC_SETTLEMENT_DUNGEON_TOWN ?
                                6.20f : 7.15f;
        DrawBox((Vector3){hall_door_x, 1.20f, hall_door_z},
                (Vector3){1.35f, 2.40f, 0.06f},
                WORLD_WOOD_SHADOW);
        CcId kingdom_id = place != NULL ? place->kingdom_id : 0U;
        CcFactionKind leading = CcHeraldryLeadingFaction(sim, kingdom_id);
        CcHeraldryStyle leading_style = CcHeraldryForFaction(
            sim, kingdom_id, leading);
        DrawHeraldryBanner3D(
            leading_style,
            (Vector3){76.30f, gate_height - 1.58f, gate_z - 0.42f},
            180.0f, 0.66f, clock);
        DrawHeraldryBanner3D(
            leading_style,
            (Vector3){80.70f, gate_height - 1.58f, gate_z - 0.42f},
            0.0f, 0.66f, clock + 0.6f);
        Color gate_stone = CompoundStoneColor(
            profile, kingdom, CC_LOCAL_COMPOUND_WALL);
        DrawBox((Vector3){gate_x, gate_height, gate_z},
                (Vector3){2.30f, 1.08f, 0.72f}, gate_stone);
        for (int32_t merlon = 0; merlon < 3; ++merlon) {
            DrawBox((Vector3){gate_x - 0.88f + (float)merlon * 0.88f,
                              gate_height + 0.87f, gate_z},
                    (Vector3){0.42f, 0.70f, 0.76f},
                    ShadeColor(gate_stone, 0.92f));
        }
        for (int32_t side = -1; side <= 1; side += 2) {
            float torch_x = gate_x + (float)side * 1.32f;
            DrawBox((Vector3){torch_x, 1.82f, gate_z + 0.36f},
                    (Vector3){0.10f, 0.86f, 0.10f},
                    WORLD_WOOD_SHADOW);
            DrawSmallSphere((Vector3){torch_x, 2.32f, gate_z + 0.36f},
                            0.18f, WORLD_GOLD);
        }
        for (int32_t faction = 0; faction < 3; ++faction) {
            DrawHeraldryBanner3D(
                CcHeraldryForFaction(
                    sim, kingdom_id, (CcFactionKind)faction),
                (Vector3){74.40f + (float)faction * 2.65f,
                          0.0f, gate_z + 2.35f},
                0.0f, 0.52f, clock + (float)faction * 0.35f);
        }
    }
    rlPopMatrix();
}
