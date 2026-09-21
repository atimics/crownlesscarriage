/* Four hand-authored building families. Included after the shared construction
 * primitives, before DrawSettlementBuilding. The existing plot/yaw, foundations,
 * service-door anchors, reveal masks and town-condition layer remain authoritative.
 * Keep solid ground-floor mass inside its plot and roof peaks below height + 3.0.
 * Details here are architecture, not simulated stock or new interaction targets. */

static void DrawTownPortal(float x, float front, float width, float height,
                           Color frame, bool door)
{
    float half = fminf(width * 0.15f, 1.15f);
    float top = fminf(height * 0.64f, 2.70f);
    if (!door) return;
    DrawBox((Vector3){x, top * 0.5f, front + 0.018f},
            (Vector3){half * 2.0f, top, 0.07f}, WORLD_WOOD_SHADOW);
    for (int32_t side = -1; side <= 1; side += 2) {
        DrawBox((Vector3){x + (float)side * (half + 0.10f), top * 0.5f, front},
                (Vector3){0.22f, top + 0.30f, 0.24f}, frame);
        DrawBox((Vector3){x + (float)side * half * 0.52f, top * 0.46f,
                          front + 0.065f},
                (Vector3){0.11f, 0.15f, 0.08f}, WORLD_METAL_SHADOW);
    }
    DrawBox((Vector3){x, top + 0.12f, front},
            (Vector3){half * 2.0f + 0.40f, 0.26f, 0.26f}, frame);
}

static void DrawTownGableRoof(float x, float z, float width, float depth,
                              float eaves, float rise, Color wall, Color roof)
{
    /* A deliberate ridge along depth, unlike the shared longest-axis cottage roof.
       Market gables are tall; farm galleries are broad; no additional collider. */
    const float half = width * 0.5f + 0.22f;
    const float length = sqrtf(half * half + rise * rise);
    float angle = atanf(rise / half) * RAD2DEG;
    for (int32_t side = -1; side <= 1; side += 2) {
        DrawTiltedBox((Vector3){x + width * 0.5f + (float)side * half * 0.5f,
                                eaves + rise * 0.5f, z + depth * 0.5f},
                      (Vector3){length, 0.20f, depth + 0.44f},
                      (Vector3){0, 0, 1}, -(float)side * angle,
                      ShadeColor(roof, side < 0 ? 0.76f : 1.0f));
    }
    /* Stacked broad gable infill avoids hollow end faces without new mesh assets. */
    for (int32_t step = 0; step < 6; ++step) {
        float t = ((float)step + 0.5f) / 6.0f;
        float band = width * (1.0f - t);
        for (int32_t end = 0; end < 2; ++end) {
            DrawBox((Vector3){x + width * 0.5f, eaves + rise * t,
                              z + (end == 0 ? 0.10f : depth - 0.10f)},
                    (Vector3){band, rise / 6.0f + 0.025f, 0.20f}, wall);
        }
    }
    DrawBox((Vector3){x + width * 0.5f, eaves + rise, z + depth * 0.5f},
            (Vector3){0.22f, 0.18f, depth + 0.50f}, ShadeColor(roof, 0.58f));
}

static void DrawTownTimberFront(float x, float z, float width, float height,
                                int32_t bays, Color timber)
{
    float bay = width / (float)bays;
    for (int32_t post = 0; post <= bays; ++post) {
        float px = x + 0.16f + (width - 0.32f) * (float)post / (float)bays;
        DrawBox((Vector3){px, height * 0.53f, z - 0.02f},
                (Vector3){0.20f, height * 0.94f, 0.20f}, timber);
    }
    for (int32_t floor = 1; floor <= 2; ++floor) {
        float y = height * (float)floor / 3.0f;
        DrawBox((Vector3){x + width * 0.5f, y, z},
                (Vector3){width, 0.20f, 0.24f}, timber);
    }
    for (int32_t pane = 0; pane < bays; ++pane) {
        float px = x + ((float)pane + 0.5f) * bay;
        DrawBox((Vector3){px, height * 0.72f, z + 0.01f},
                (Vector3){bay * 0.40f, height * 0.22f, 0.07f}, WORLD_INK);
        DrawBox((Vector3){px, height * 0.72f, z + 0.06f},
                (Vector3){0.10f, height * 0.24f, 0.08f}, timber);
    }
}

static void DrawThornfordArchitecture(const WorldBuilding *b, Color wall,
                                      Color roof, int32_t index, bool burnt)
{
    float x = b->footprint.x, z = b->footprint.y;
    float w = b->footprint.width, d = b->footprint.height, h = b->height;
    Color timber = burnt ? ShadeColor(wall, 0.62f) : WORLD_WOOD_SHADOW;
    bool granary = index == 2 || index < 0;
    bool shed = index == 0 || index == 5;
    if (granary) {
        /* A wide raised food store, cross-braced gallery and an unloading hatch.
           The lower stone store is solid: the gallery does not imply a shortcut. */
        DrawBox((Vector3){x + w * 0.5f, 0.65f, z + d * 0.5f},
                (Vector3){w, 1.30f, d}, ShadeColor(wall, 0.72f));
        DrawBox((Vector3){x + w * 0.5f, (h + 1.3f) * 0.5f, z + d * 0.5f},
                (Vector3){w - 0.20f, h - 1.3f, d - 0.16f}, wall);
        DrawTownTimberFront(x, z + d, w, h, 4, timber);
        DrawTownGableRoof(x, z, w, d, h, 2.25f, wall, roof);
        DrawBox((Vector3){x + w * 0.5f, h * 0.52f, z + d - 0.18f},
                (Vector3){w - 0.4f, 0.30f, 0.66f}, timber);
        for (int32_t peg = 0; peg < 7; ++peg) {
            DrawBox((Vector3){x + 0.55f + (w - 1.10f) * (float)peg / 6.0f,
                              h * 0.52f + 0.40f, z + d},
                    (Vector3){0.10f, 0.78f, 0.12f}, timber);
        }
        DrawBox((Vector3){x + w * 0.5f, h * 0.52f + 0.79f, z + d},
                (Vector3){w - 0.60f, 0.12f, 0.14f}, timber);
        DrawBox((Vector3){x + w * 0.5f, h + 0.65f, z + d - 0.24f},
                (Vector3){0.22f, 0.24f, 1.3f}, timber);
    } else if (shed) {
        /* Continuous low barn roof and solid plank bays. No false walk-through
           openings inside the existing solid workshop collision footprint. */
        float eaves = h * 0.73f;
        DrawBox((Vector3){x + w * 0.5f, eaves * 0.5f, z + d * 0.5f},
                (Vector3){w, eaves, d}, ShadeColor(wall, 0.88f));
        DrawTownTimberFront(x, z + d, w, eaves, 4, timber);
        DrawPitchedRoof(x, z, w, d, eaves, wall, roof);
        if (index == 5) {
            /* Two wheels fixed to the shed face, entirely above the walkway. */
            for (int32_t wheel = 0; wheel < 2; ++wheel) {
                Vector3 hub = {x + w * (wheel == 0 ? 0.22f : 0.78f),
                               1.48f, z + d + 0.10f};
                for (int32_t spoke = 0; spoke < 8; ++spoke) {
                    float a = (float)spoke * PI / 4.0f;
                    float n = a + PI / 4.0f;
                    Vector3 p = Vector3Add(hub, (Vector3){cosf(a), sinf(a), 0});
                    Vector3 q = Vector3Add(hub, (Vector3){cosf(n), sinf(n), 0});
                    DrawCylinderEx(hub, p, 0.045f, 0.045f, 4, timber);
                    DrawCylinderEx(p, q, 0.09f, 0.09f, 4, timber);
                }
            }
        }
    } else {
        /* Crofts stay low. The mill alone has a tall framed hoist gable; its
           existing turning wheel and water channel are drawn by the river stage. */
        float eaves = index == 6 ? h * 0.90f : h * 0.72f;
        DrawBox((Vector3){x + w * 0.5f, eaves * 0.5f, z + d * 0.5f},
                (Vector3){w, eaves, d}, wall);
        DrawTownTimberFront(x, z + d, w, eaves, index == 6 ? 3 : 2, timber);
        DrawTownGableRoof(x, z, w, d, eaves,
                          index == 6 ? 2.25f : 1.65f, wall, roof);
    }
    DrawTownPortal(x + w * 0.5f, z + d, w, h, timber, b->door);
}

static void DrawSilverwickArchitecture(const WorldBuilding *b, Color wall,
                                       Color roof, int32_t index, bool burnt)
{
    float x = b->footprint.x, z = b->footprint.y;
    float w = b->footprint.width, d = b->footprint.height, h = b->height;
    Color iron = burnt ? ShadeColor(wall, 0.65f) : WORLD_METAL_SHADOW;
    Color masonry = ShadeColor(wall, 0.86f);
    if (index == 2) {
        /* The company facade is squat and severe. A stopped clock, not a church
           spire, rises above the counter building. The centre door stays put. */
        float eaves = h * 0.57f;
        DrawBox((Vector3){x + w * 0.5f, eaves * 0.5f, z + d * 0.5f},
                (Vector3){w, eaves, d}, wall);
        DrawPitchedRoof(x, z, w, d, eaves, wall, roof);
        DrawBox((Vector3){x + w * 0.5f, h * 0.68f, z + d - 1.2f},
                (Vector3){w * 0.25f, h * 0.64f, 2.4f}, masonry);
        DrawBox((Vector3){x + w * 0.5f, h + 0.15f, z + d - 1.2f},
                (Vector3){w * 0.30f, 0.30f, 2.8f}, iron);
        Vector3 dial = {x + w * 0.5f, h * 0.84f, z + d + 0.04f};
        DrawCylinderEx(dial, Vector3Add(dial, (Vector3){0,0,0.1f}),
                        1.04f, 1.04f, 16, iron);
        dial.z += 0.11f;
        DrawCylinderEx(dial, Vector3Add(dial, (Vector3){0,0,0.05f}),
                        0.87f, 0.87f, 16,
                        burnt ? masonry : CC_VISUAL_PALETTE.parchment.light);
        DrawBox(Vector3Add(dial, (Vector3){0,0.33f,0.09f}),
                (Vector3){0.09f,0.66f,0.08f}, iron);
        DrawTiltedBox(Vector3Add(dial, (Vector3){-0.22f,-0.13f,0.09f}),
                      (Vector3){0.55f,0.11f,0.08f}, (Vector3){0,0,1}, 30, iron);
        DrawBox((Vector3){x + w * 0.5f, 3.25f, z + d + 0.07f},
                (Vector3){w * 0.80f, 0.42f, 0.18f}, iron);
    } else if (b->style == CC_LOCAL_BUILDING_WORKER_ROW) {
        int32_t bays = w >= 9.0f ? 3 : 2;
        float bay = w / (float)bays;
        DrawBox((Vector3){x + w * 0.5f, h * 0.38f, z + d * 0.5f},
                (Vector3){w, h * 0.76f, d}, masonry);
        for (int32_t home = 0; home < bays; ++home) {
            float px = x + (float)home * bay;
            float eaves = h * (home % 2 == 0 ? 0.78f : 0.88f);
            DrawBox((Vector3){px + bay * 0.5f, eaves * 0.5f, z + d * 0.5f},
                    (Vector3){bay - 0.10f, eaves, d - 0.06f},
                    ShadeColor(wall, home % 2 == 0 ? 0.93f : 1.08f));
            DrawTownGableRoof(px, z, bay, d, eaves, 1.18f, wall, roof);
            DrawTownTimberFront(px + 0.12f, z + d, bay - 0.24f, eaves, 2, iron);
            DrawBox((Vector3){px + bay * 0.76f, eaves + 0.75f, z + d * 0.30f},
                    (Vector3){0.54f, 2.20f, 0.70f}, masonry);
        }
        /* Laundry is pinned to the facade, not a new line across a walking lane. */
        for (int32_t cloth = 0; cloth < 4; ++cloth) {
            DrawBox((Vector3){x + w * (0.16f + (float)cloth * 0.22f),
                              h * 0.53f, z + d + 0.05f},
                    (Vector3){w * 0.12f, 0.70f, 0.035f},
                    burnt ? iron : BlendColor(wall, roof, (float)(cloth % 3) * 0.25f));
        }
    } else {
        /* Repeating sawtooth shed roofs, clerestory glazing and a headframe on
           the existing company plot: an industrial skyline without new blockers. */
        float eaves = h * 0.62f;
        DrawBox((Vector3){x + w * 0.5f, eaves * 0.5f, z + d * 0.5f},
                (Vector3){w, eaves, d}, masonry);
        for (int32_t tooth = 0; tooth < 3; ++tooth) {
            float run = d / 3.0f;
            float sz = z + (float)tooth * run;
            float rise = 1.15f;
            DrawTiltedBox((Vector3){x + w * 0.5f, eaves + rise * 0.5f,
                                    sz + run * 0.5f},
                          (Vector3){w + 0.24f,0.18f,sqrtf(run * run + rise * rise)},
                          (Vector3){1,0,0}, atanf(rise / run) * RAD2DEG, roof);
            DrawBox((Vector3){x + w * 0.5f, eaves + rise * 0.48f, sz + 0.06f},
                    (Vector3){w - 0.25f, rise * 0.94f, 0.12f}, iron);
            for (int32_t pane = 0; pane < 4; ++pane) {
                DrawBox((Vector3){x + w * (0.13f + (float)pane * 0.245f),
                                  eaves + rise * 0.50f, sz + 0.14f},
                        (Vector3){w * 0.17f, rise * 0.58f, 0.05f},
                        burnt ? masonry : BlendColor(WORLD_TEAL, wall, 0.50f));
            }
        }
        for (int32_t post = 0; post < 4; ++post) {
            float px = x + 0.20f + (w - 0.40f) * (float)post / 3.0f;
            DrawBox((Vector3){px, eaves * 0.5f, z + d - 0.10f},
                    (Vector3){0.32f, eaves + 0.20f, 0.32f}, iron);
        }
        if (index < 0) {
            for (int32_t side = -1; side <= 1; side += 2) {
                DrawTiltedBox((Vector3){x + w * 0.5f + (float)side * w * 0.19f,
                                        h * 0.63f, z + d * 0.45f},
                              (Vector3){0.32f,h * 0.78f,0.38f},
                              (Vector3){0,0,1}, (float)side * 12.0f, iron);
            }
            DrawBox((Vector3){x + w * 0.5f,h * 1.01f,z + d * 0.45f},
                    (Vector3){w * 0.56f,0.45f,0.75f}, iron);
        }
    }
    DrawTownPortal(x + w * 0.5f, z + d, w, h, iron, b->door);
}

static void DrawGloamgateArchitecture(const WorldBuilding *b, Color wall,
                                      Color roof, int32_t index, bool burnt)
{
    float x = b->footprint.x, z = b->footprint.y;
    float w = b->footprint.width, d = b->footprint.height, h = b->height;
    Color trim = burnt ? ShadeColor(wall, 0.64f) : WORLD_WOOD_SHADOW;
    Color cloth = burnt ? roof : BlendColor(WORLD_TEAL, wall, 0.15f);
    if (index == 1) {
        /* The archive is the only tall stepped facade: paired lancet windows,
           stone stairs within the existing solid plot and a central record door. */
        DrawBox((Vector3){x + w * 0.5f,h * 0.5f,z + d * 0.5f},
                (Vector3){w,h,d}, wall);
        DrawTownGableRoof(x,z,w,d,h,2.6f,wall,roof);
        for (int32_t tier = 0; tier < 4; ++tier) {
            float tw = w * (1.0f - (float)tier * 0.23f);
            DrawBox((Vector3){x + w * 0.5f,h + (float)tier * 0.60f + 0.20f,
                              z + d - 0.12f},
                    (Vector3){tw,0.60f,0.35f}, ShadeColor(wall,1.06f));
        }
        for (int32_t side = -1; side <= 1; side += 2) {
            float px = x + w * 0.5f + (float)side * w * 0.27f;
            DrawBox((Vector3){px,h * 0.62f,z + d + 0.03f},
                    (Vector3){w * 0.16f,h * 0.48f,0.08f}, WORLD_INK);
            DrawBox((Vector3){px,h * 0.62f,z + d + 0.08f},
                    (Vector3){0.10f,h * 0.50f,0.12f}, trim);
            DrawTiltedBox((Vector3){px - 0.19f,h * 0.90f,z + d + 0.06f},
                          (Vector3){0.16f,0.62f,0.18f},(Vector3){0,0,1},-36,wall);
            DrawTiltedBox((Vector3){px + 0.19f,h * 0.90f,z + d + 0.06f},
                          (Vector3){0.16f,0.62f,0.18f},(Vector3){0,0,1},36,wall);
        }
    } else if (index == 2) {
        /* An octagonal exchange above a wide, low market hall. */
        float eaves = h * 0.50f;
        DrawBox((Vector3){x + w * 0.5f,eaves * 0.5f,z + d * 0.5f},
                (Vector3){w,eaves,d}, wall);
        DrawPitchedRoof(x,z,w,d,eaves,wall,roof);
        rlPushMatrix();
        rlTranslatef(0,eaves,0);
        DrawCylinder((Vector3){x + w * 0.5f,0,z + d * 0.40f},
                     w * 0.23f,w * 0.25f,h * 0.62f,8,wall);
        DrawCylinder((Vector3){x + w * 0.5f,h * 0.62f,z + d * 0.40f},
                     w * 0.10f,w * 0.31f,1.05f,8,cloth);
        rlPopMatrix();
        DrawTownTimberFront(x,z + d,w,eaves,5,trim);
    } else if (index < 0) {
        /* The customs compound is a counting house, not another frontier keep. */
        DrawBox((Vector3){x + w * 0.5f,h * 0.38f,z + d * 0.5f},
                (Vector3){w,h * 0.76f,d}, wall);
        DrawPitchedRoof(x,z,w,d,h * 0.76f,wall,roof);
        DrawTownTimberFront(x,z + d,w,h * 0.76f,4,trim);
    } else {
        /* Close-set merchant gables with jettied upper storeys and fabric faces. */
        int32_t bays = w >= 9.0f ? 3 : 2;
        float bw = w / (float)bays;
        for (int32_t bay = 0; bay < bays; ++bay) {
            float px = x + (float)bay * bw;
            float eaves = h * (bay % 2 == 0 ? 0.82f : 0.94f);
            DrawBox((Vector3){px + bw * 0.5f,eaves * 0.5f,z + d * 0.5f},
                    (Vector3){bw,eaves,d}, ShadeColor(wall,0.94f + (float)bay * 0.05f));
            DrawBox((Vector3){px + bw * 0.5f,eaves * 0.71f,z + d - 0.30f},
                    (Vector3){bw - 0.12f,eaves * 0.53f,0.72f}, wall);
            DrawTownTimberFront(px + 0.08f,z + d,bw - 0.16f,eaves,2,trim);
            DrawTownGableRoof(px,z,bw,d,eaves,1.75f,wall,roof);
            DrawTiltedBox((Vector3){px + bw * 0.5f,2.72f,z + d - 0.20f},
                          (Vector3){bw - 0.22f,0.08f,1.15f},
                          (Vector3){1,0,0},12,
                          ShadeColor(cloth,bay % 2 == 0 ? 1.0f : 0.77f));
            DrawBox((Vector3){px + bw * 0.5f,2.40f,z + d + 0.12f},
                    (Vector3){bw - 0.22f,0.45f,0.045f}, cloth);
        }
    }
    DrawTownPortal(x + w * 0.5f,z + d,w,h,trim,b->door);
}

static void DrawAlderwatchArchitecture(const WorldBuilding *b, Color wall,
                                       Color roof, int32_t index, bool burnt)
{
    float x = b->footprint.x, z = b->footprint.y;
    float w = b->footprint.width, d = b->footprint.height, h = b->height;
    Color stone = ShadeColor(wall,0.90f);
    Color iron = burnt ? ShadeColor(wall,0.60f) : WORLD_METAL_SHADOW;
    bool keep = index == -2;
    float eaves = keep ? h : h * 0.62f;
    DrawBox((Vector3){x + w * 0.5f,eaves * 0.5f,z + d * 0.5f},
            (Vector3){w,eaves,d}, stone);
    if (keep || index == 2) {
        /* Only the keep and toll hall are crenellated. Ordinary barracks do not
           sprout identical turrets: this leaves one legible dominant silhouette. */
        DrawBox((Vector3){x + w * 0.5f,eaves - 0.08f,z + d * 0.5f},
                (Vector3){w,0.32f,d}, ShadeColor(wall,1.10f));
        for (int32_t edge = 0; edge < 4; ++edge) {
            int32_t count = edge < 2 ? 7 : 5;
            for (int32_t tooth = 0; tooth < count; ++tooth) {
                float t = ((float)tooth + 0.5f) / (float)count;
                float px = edge < 2 ? x + w * t : x + (edge == 2 ? 0.30f : w - 0.30f);
                float pz = edge < 2 ? z + (edge == 0 ? 0.30f : d - 0.30f) : z + d * t;
                DrawBox((Vector3){px,eaves + 0.38f,pz},
                        (Vector3){0.62f,0.78f,0.62f}, wall);
            }
        }
        if (keep) {
            DrawBox((Vector3){x + w * 0.68f,h + 1.15f,z + d * 0.36f},
                    (Vector3){w * 0.26f,2.30f,d * 0.28f}, wall);
            DrawBox((Vector3){x + w * 0.68f,h + 2.40f,z + d * 0.36f},
                    (Vector3){w * 0.29f,0.22f,d * 0.31f}, iron);
        }
    } else {
        DrawPitchedRoof(x,z,w,d,eaves,stone,roof);
    }
    for (int32_t side = 0; side < 4; ++side) {
        float px = x + 0.25f + (w - 0.50f) * (float)side / 3.0f;
        DrawBox((Vector3){px,eaves * 0.42f,z + d - 0.18f},
                (Vector3){0.50f,eaves * 0.84f,0.40f}, ShadeColor(wall,0.76f));
        DrawBox((Vector3){px + 0.46f,eaves * 0.68f,z + d + 0.015f},
                (Vector3){0.17f,0.83f,0.04f}, iron);
    }
    if (index == 2 || keep) {
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawBox((Vector3){x + w * (0.5f + (float)side * 0.29f),
                              eaves * 0.62f,z + d + 0.06f},
                    (Vector3){w * 0.11f,eaves * 0.43f,0.035f},
                    burnt ? iron : WORLD_DANGER);
        }
    }
    DrawTownPortal(x + w * 0.5f,z + d,w,eaves,iron,b->door);
}
