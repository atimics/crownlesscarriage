/* Hero, NPC, creature, carriage, and combat presentation. */
static Vector3 Add3(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 LocalPoint(Vector3 base, float x, float y, float z, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (Vector3){base.x + x * cosine + z * sine,
                     base.y + y,
                     base.z - x * sine + z * cosine};
}

static void DrawOrientedBox(Vector3 base, Vector3 local_center, Vector3 size,
                            float yaw, Color color)
{
    rlPushMatrix();
    rlTranslatef(base.x, base.y, base.z);
    rlRotatef(yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawBox(local_center, size, color);
    rlPopMatrix();
}

static void DrawBoneSegment(Vector3 a, Vector3 b, Color color)
{
    DrawCylinderEx(a, b, 0.028f, 0.028f, 7, color);
}

static void DrawBoneJoint(Vector3 point, Color color)
{
    DrawSmallSphere(point, 0.045f, color);
}

static void DrawCharacterEllipsoid(Vector3 center, Vector3 radius, Color color)
{
    DrawModelEx(sphere_models.character, center,
                (Vector3){0.0f, 1.0f, 0.0f}, 0.0f, radius, color);
}

static void SetFaceRenderContext(Camera3D camera, int32_t width,
                                 int32_t height)
{
    face_render_context.camera = camera;
    face_render_context.width = width;
    face_render_context.height = height;
    face_render_context.valid = width > 0 && height > 0;
}

static float FaceProjectedHeight(Vector3 eye_center, Vector3 up,
                                 float half_height)
{
    if (!face_render_context.valid) return 0.0f;
    Vector2 top = GetWorldToScreenEx(
        Vector3Add(eye_center, Vector3Scale(up, half_height)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    Vector2 bottom = GetWorldToScreenEx(
        Vector3Subtract(eye_center, Vector3Scale(up, half_height)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    return Vector2Distance(top, bottom);
}

static float FaceProjectedSpan(Vector3 center, Vector3 axis,
                               float half_span)
{
    if (!face_render_context.valid || half_span <= 0.0f) return 0.0f;
    Vector2 first = GetWorldToScreenEx(
        Vector3Subtract(center, Vector3Scale(axis, half_span)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    Vector2 second = GetWorldToScreenEx(
        Vector3Add(center, Vector3Scale(axis, half_span)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    return Vector2Distance(first, second);
}

CcLocalFaceLodInternal CcLocalFaceLodForProjectedHeightInternal(
    float projected_face_height)
{

    if (projected_face_height < 4.0f) {
        return CC_LOCAL_FACE_LOD_SILHOUETTE;
    }
    if (projected_face_height < 12.0f) return CC_LOCAL_FACE_LOD_READABLE;
    return CC_LOCAL_FACE_LOD_CLOSE;
}

CcLocalFaceViewInternal CcLocalFaceViewForFrontAmountInternal(
    float front_amount)
{
    if (front_amount >= 0.82f) return CC_LOCAL_FACE_VIEW_FRONT;
    if (front_amount >= 0.28f) return CC_LOCAL_FACE_VIEW_THREE_QUARTER;
    return CC_LOCAL_FACE_VIEW_PROFILE;
}

CcLocalFaceAnchorInternal CcLocalFaceAnchorForCameraInternal(
    float front_amount, float side_amount)
{
    CcLocalFaceViewInternal view =
        CcLocalFaceViewForFrontAmountInternal(front_amount);
    float turn = view == CC_LOCAL_FACE_VIEW_FRONT ? 0.0f :
                 view == CC_LOCAL_FACE_VIEW_THREE_QUARTER ? 0.52f : 1.22f;
    float side = side_amount < 0.0f ? -1.0f : 1.0f;
    return (CcLocalFaceAnchorInternal){
        .forward = cosf(turn),
        .right = side * sinf(turn),
    };
}

static void DrawFaceQuad(Vector3 origin, Vector3 right, Vector3 up,
                         Vector3 normal, float x, float y, float width,
                         float height, float lift, Color color)
{
    Vector3 center = Vector3Add(
        origin, Vector3Add(Vector3Scale(right, x), Vector3Scale(up, y)));
    center = Vector3Add(center, Vector3Scale(normal, lift));
    Vector3 across = Vector3Scale(right, width * 0.5f);
    Vector3 rise = Vector3Scale(up, height * 0.5f);
    Vector3 lower_left = Vector3Subtract(Vector3Subtract(center, across), rise);
    Vector3 lower_right = Vector3Add(Vector3Subtract(center, rise), across);
    Vector3 upper_right = Vector3Add(Vector3Add(center, across), rise);
    Vector3 upper_left = Vector3Add(Vector3Subtract(center, across), rise);
    DrawTriangle3D(lower_left, lower_right, upper_right, color);
    DrawTriangle3D(lower_left, upper_right, upper_left, color);

    DrawTriangle3D(lower_left, upper_right, lower_right, color);
    DrawTriangle3D(lower_left, upper_left, upper_right, color);
}

typedef struct WorldFaceCanvas {
    Vector3 origin;
    Vector3 right;
    Vector3 up;
    Vector3 normal;
    float cell_width;
    float cell_height;
    float horizontal_scale;
    CcLocalFaceLodInternal lod;
    bool always_paint;
    int32_t layer;
} WorldFaceCanvas;

static bool IsPortraitEyeBlock(int32_t grid_x, int32_t grid_y,
                               int32_t grid_width, int32_t grid_height)
{
    return grid_width == 1 && grid_height == 1 &&
           grid_x >= 6 && grid_x <= 13 &&
           (grid_y == 8 || grid_y == 9);
}

static bool IsPortraitMouthBlock(int32_t grid_x, int32_t grid_y,
                                 int32_t grid_width,
                                 int32_t grid_height)
{
    (void)grid_width;
    (void)grid_height;
    return grid_x >= 7 && grid_x <= 10 &&
           (grid_y >= 13 && grid_y <= 15);
}

static void PaintWorldFaceBlock(void *context, int32_t grid_x,
                                int32_t grid_y, int32_t grid_width,
                                int32_t grid_height, Color color)
{
    WorldFaceCanvas *canvas = context;
    if (!canvas->always_paint &&
        canvas->lod == CC_LOCAL_FACE_LOD_READABLE &&
        !IsPortraitEyeBlock(grid_x, grid_y, grid_width, grid_height) &&
        !IsPortraitMouthBlock(grid_x, grid_y, grid_width, grid_height)) {
        return;
    }

    float grid_center_x = (float)grid_x + (float)grid_width * 0.5f;
    float grid_center_y = (float)grid_y + (float)grid_height * 0.5f;

    float x = (grid_center_x - 10.0f) * canvas->cell_width;
    float width = (float)grid_width * canvas->cell_width;
    x *= canvas->horizontal_scale;
    width *= canvas->horizontal_scale;
    float y = (8.5f - grid_center_y) * canvas->cell_height;
    float height = (float)grid_height * canvas->cell_height;
    float lift = 0.0030f + (float)canvas->layer * 0.00045f;
    DrawFaceQuad(canvas->origin, canvas->right, canvas->up, canvas->normal,
                 x, y, width, height, lift, color);
    canvas->layer += 1;
}

static void DrawWorldFace(Vector3 eye_center, Vector3 head_right,
                          Vector3 head_up, Vector3 head_forward,
                          float half_width, float half_height,
                          float half_depth, const CcFaceRecipe *face,
                          CcNpcPortraitExpression expression,
                          bool priority_face)
{
    if (!face_render_context.valid || face == NULL) return;
    head_right = PhysicsNormalizeOr(head_right,
                                    (Vector3){1.0f, 0.0f, 0.0f});
    head_up = PhysicsNormalizeOr(head_up,
                                 (Vector3){0.0f, 1.0f, 0.0f});
    head_forward = PhysicsNormalizeOr(head_forward,
                                      (Vector3){0.0f, 0.0f, 1.0f});
    Vector3 to_camera = Vector3Subtract(face_render_context.camera.position,
                                        eye_center);
    to_camera = Vector3Subtract(
        to_camera, Vector3Scale(head_up,
                               Vector3DotProduct(to_camera, head_up)));
    to_camera = PhysicsNormalizeOr(to_camera, head_forward);
    float front_amount = Vector3DotProduct(head_forward, to_camera);
    float side_amount = Vector3DotProduct(head_right, to_camera);

    float hidden_face_cutoff = priority_face ? -0.08f : -0.12f;
    if (front_amount < hidden_face_cutoff) return;

    CcLocalFaceAnchorInternal anchor =
        CcLocalFaceAnchorForCameraInternal(front_amount, side_amount);
    Vector3 normal = PhysicsNormalizeOr(
        Vector3Add(Vector3Scale(head_forward, anchor.forward),
                   Vector3Scale(head_right, anchor.right)),
        head_forward);
    Vector3 feature_right = PhysicsNormalizeOr(
        Vector3Subtract(Vector3Scale(head_right, anchor.forward),
                        Vector3Scale(head_forward, anchor.right)),
        head_right);
    float surface_cosine = anchor.forward;
    float surface_sine = anchor.right;
    float surface_radius = 1.0f / sqrtf(
        (surface_cosine * surface_cosine) / (half_depth * half_depth) +
        (surface_sine * surface_sine) / (half_width * half_width));
    Vector3 surface = Vector3Add(
        eye_center, Vector3Scale(
            normal, surface_radius * (priority_face ? 1.15f : 1.03f)));

    float projected_face_height = FaceProjectedHeight(
        eye_center, head_up, half_height);
    CcLocalFaceLodInternal lod = CcLocalFaceLodForProjectedHeightInternal(
        projected_face_height);
    if (lod == CC_LOCAL_FACE_LOD_SILHOUETTE) return;


    EndShaderMode();
    float projected_face_width = FaceProjectedSpan(
        eye_center, feature_right, half_width);
    float horizontal_pixels_per_unit = projected_face_width /
                                       fmaxf(half_width * 2.0f, 0.0001f);
    float vertical_pixels_per_unit = projected_face_height /
                                     fmaxf(half_height * 2.0f, 0.0001f);

    float cell_pixels = fminf(projected_face_width / 10.0f,
                              projected_face_height / 15.0f);
    WorldFaceCanvas canvas = {
        .origin = surface,
        .right = feature_right,
        .up = head_up,
        .normal = normal,

        .cell_width = cell_pixels /
                      fmaxf(horizontal_pixels_per_unit, 0.0001f),
        .cell_height = cell_pixels /
                       fmaxf(vertical_pixels_per_unit, 0.0001f),
        .horizontal_scale = 1.0f,
        .lod = lod,
        .always_paint = false,
        .layer = 0,
    };
    if (lod == CC_LOCAL_FACE_LOD_CLOSE || priority_face) {
        canvas.always_paint = true;
        CcNpcPaintFaceBeard(face, &canvas, PaintWorldFaceBlock);
    }
    canvas.always_paint = priority_face;
    CcNpcPaintFaceFeatures(face, expression, &canvas,
                           PaintWorldFaceBlock);
    EndShaderMode();
}

static void DrawNpcHead(const CcNpcAppearance *appearance, Vector3 head,
                        float yaw, float scale)
{
    float head_width = 0.175f * appearance->head_width * scale;
    float head_depth = 0.158f * appearance->head_depth * scale;
    DrawCharacterEllipsoid(head,
        (Vector3){head_width, 0.195f * scale, head_depth}, appearance->skin);
    Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    CcNpcPortraitExpression expression =
        appearance->role == CC_NPC_ROLE_GUARD ||
        appearance->role == CC_NPC_ROLE_RAIDER ? CC_NPC_PORTRAIT_FOCUSED :
                                                 CC_NPC_PORTRAIT_NEUTRAL;
    CcFaceRecipe face = CcNpcFaceRecipe(appearance);
    DrawWorldFace(PhysicsAdd(head, (Vector3){0.0f, 0.045f * scale, 0.0f}),
                  right, (Vector3){0.0f, 1.0f, 0.0f}, forward,
                  head_width, 0.195f * scale, head_depth, &face, expression,
                  false);
    UseCharacterLighting();

    Vector3 crown = PhysicsAdd(head, (Vector3){0.0f, 0.115f * scale, 0.0f});
    switch (appearance->hair_style) {
        case 0:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width * 1.02f, 0.105f * scale,
                          head_depth * 1.04f}, appearance->hair);
            break;
        case 1:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width * 1.04f, 0.125f * scale,
                          head_depth * 1.02f}, appearance->hair);
            DrawCylinderEx(PhysicsAdd(head, PhysicsScale(forward,
                                      -head_depth * 0.80f)),
                           PhysicsAdd(head, (Vector3){0.0f, -0.18f * scale, 0.0f}),
                           0.075f * scale, 0.045f * scale, 7,
                           appearance->hair);
            break;
        case 2:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width, 0.105f * scale, head_depth},
                appearance->hair);
            DrawCharacterSphere(
                PhysicsAdd(PhysicsAdd(head, (Vector3){0.0f, 0.12f * scale, 0.0f}),
                           PhysicsScale(forward, -head_depth)),
                0.070f * scale, appearance->hair);
            break;
        case 3:
            DrawOrientedBox(crown, (Vector3){0.0f, 0.02f * scale, 0.0f},
                            (Vector3){head_width * 0.72f, 0.16f * scale,
                                      head_depth * 1.65f}, yaw,
                            appearance->hair);
            break;
        case 4:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width, 0.105f * scale, head_depth},
                appearance->hair);
            for (int32_t side = -1; side <= 1; side += 2) {
                Vector3 braid = PhysicsAdd(head, PhysicsScale(right,
                    (float)side * head_width * 0.78f));
                DrawCylinderEx(braid,
                    PhysicsAdd(braid, (Vector3){0.0f, -0.24f * scale, 0.0f}),
                    0.027f * scale, 0.018f * scale, 6, appearance->hair);
            }
            break;
        case 5:
        default:
            DrawCharacterEllipsoid(
                PhysicsAdd(crown, PhysicsScale(forward, -0.018f * scale)),
                (Vector3){head_width * 0.88f, 0.060f * scale,
                          head_depth * 0.92f}, appearance->hair);
            break;
    }
    if (appearance->beard_style != 0U) {
        Vector3 beard = PhysicsAdd(
            PhysicsAdd(head, PhysicsScale(forward, head_depth * 0.72f)),
            (Vector3){0.0f, -0.105f * scale, 0.0f});
        float length = (0.045f + 0.035f * (float)appearance->beard_style) * scale;
        DrawCharacterEllipsoid(beard,
            (Vector3){head_width * 0.68f, length,
                      head_depth * 0.34f}, appearance->hair);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) {
        Color headwear = appearance->role == CC_NPC_ROLE_GUARD ?
                         appearance->metal : appearance->outer;
        DrawCylinder(PhysicsAdd(head, (Vector3){0.0f, 0.18f * scale, 0.0f}),
                     0.19f * scale, 0.15f * scale, 0.09f * scale, 10,
                     headwear);
        if (appearance->role != CC_NPC_ROLE_GUARD) {
            DrawCylinder(PhysicsAdd(head,
                         (Vector3){0.0f, 0.145f * scale, 0.0f}),
                         0.24f * scale, 0.24f * scale, 0.025f * scale, 12,
                         ShadeColor(headwear, 0.72f));
        }
    }
}

static void DrawNpcEquipment(const CcNpcAppearance *appearance,
                             Vector3 position, Vector3 hip, Vector3 chest,
                             Vector3 shoulder_l, Vector3 shoulder_r,
                             Vector3 hand_l, float yaw, float scale)
{
    if ((appearance->equipment & CC_NPC_EQUIPMENT_MANTLE) != 0U) {
        DrawOrientedBox(position, (Vector3){0.0f, 1.12f * scale,
                                            -0.16f * scale},
                        (Vector3){0.48f * scale, 0.58f * scale,
                                  0.055f * scale}, yaw,
                        ShadeColor(appearance->outer, 0.68f));
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {

        DrawCharacterSphere(shoulder_l, 0.098f * scale, appearance->metal);
        DrawCharacterSphere(shoulder_r, 0.098f * scale, appearance->metal);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_APRON) != 0U) {
        DrawOrientedBox(hip, (Vector3){0.0f, 0.26f * scale,
                                       0.18f * scale},
                        (Vector3){0.27f * scale, 0.22f * scale,
                                  0.06f * scale}, yaw,
                        appearance->underlayer);
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawOrientedBox(
                hip, (Vector3){(float)side * 0.085f * scale,
                               0.02f * scale, 0.18f * scale},
                (Vector3){0.145f * scale, 0.38f * scale,
                          0.065f * scale}, yaw,
                appearance->underlayer);
        }
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_SATCHEL) != 0U) {
        Vector3 bag = LocalPoint(hip, 0.25f * scale, -0.02f * scale,
                                 0.02f * scale, yaw);
        DrawOrientedBox(bag, (Vector3){0},
                        (Vector3){0.20f * scale, 0.24f * scale,
                                  0.11f * scale}, yaw, appearance->leather);
        DrawCylinderEx(shoulder_l, bag, 0.018f * scale, 0.018f * scale,
                       5, ShadeColor(appearance->leather, 0.82f));
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_PACK) != 0U) {
        DrawOrientedBox(chest, (Vector3){0.0f, -0.04f * scale,
                                         -0.20f * scale},
                        (Vector3){0.36f * scale, 0.42f * scale,
                                  0.18f * scale}, yaw,
                        appearance->leather);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_TOOL) != 0U &&
        appearance->role != CC_NPC_ROLE_GUARD &&
        appearance->role != CC_NPC_ROLE_RAIDER) {
        Vector3 tool_end = PhysicsAdd(hand_l,
            (Vector3){0.0f, 0.62f * scale, 0.0f});
        DrawCylinderEx(hand_l, tool_end, 0.022f * scale, 0.016f * scale,
                       6, appearance->leather);
        DrawSmallSphere(tool_end, 0.045f * scale, appearance->metal);
    }
}

static void DrawNpcGarmentCut(const CcNpcAppearance *appearance,
                              Vector3 hip, Vector3 chest,
                              Vector3 shoulder_l, float yaw, float scale)
{
    Color shadow = ShadeColor(appearance->outer, 0.72f);
    switch (appearance->garment_style) {
        case 0:

            DrawOrientedBox(hip, (Vector3){0.0f, -0.10f * scale, 0.0f},
                            (Vector3){0.38f * scale, 0.28f * scale,
                                      0.28f * scale}, yaw, shadow);
            break;
        case 1:

            for (int32_t side = -1; side <= 1; side += 2) {
                DrawOrientedBox(hip,
                    (Vector3){(float)side * 0.105f * scale,
                              -0.19f * scale, -0.13f * scale},
                    (Vector3){0.17f * scale, 0.42f * scale,
                              0.055f * scale}, yaw, shadow);
            }
            break;
        case 2: {

            Vector3 sash_end = LocalPoint(hip, 0.14f * scale,
                                           0.08f * scale, 0.17f * scale, yaw);
            DrawCylinderEx(shoulder_l, sash_end, 0.025f * scale,
                           0.025f * scale, 5, appearance->leather);
            break;
        }
        case 3:

            DrawOrientedBox(chest,
                            (Vector3){0.0f, -0.035f * scale,
                                      0.16f * scale},
                            (Vector3){0.31f * scale, 0.34f * scale,
                                      0.035f * scale}, yaw,
                            ShadeColor(appearance->accent, 0.82f));
            break;
        case 4:
        default:

            DrawCylinder(PhysicsAdd(hip,
                         (Vector3){0.0f, -0.03f * scale, 0.0f}),
                         0.22f * scale, 0.205f * scale, 0.15f * scale, 10,
                         shadow);
            break;
    }
}

static bool DrawNpcArchetypePose3D(
    Vector3 position, float size_hint, float yaw, float phase,
    CcTraversalMode mode, int32_t fixed_pose,
    CcNpcPortraitExpression expression, const CcNpcAppearance *appearance)
{
    if (appearance == NULL || appearance->role < CC_NPC_ROLE_WAYFARER ||
        appearance->role >= CC_NPC_ROLE_COUNT) {
        return false;
    }
    float pulse = sinf(phase * appearance->gait_cadence_scale);
    int32_t pose = fixed_pose >= 0 &&
        fixed_pose < CC_NPC_ARCHETYPE_POSE_COUNT ? fixed_pose : 0;
    if (fixed_pose < 0 && mode != CC_TRAVERSAL_IDLE) {
        float cycle = fmodf(phase * appearance->gait_cadence_scale /
                                (2.0f * PI),
                            1.0f);
        if (cycle < 0.0f) cycle += 1.0f;
        pose = 1 + (int32_t)floorf(
            cycle * (float)CC_NPC_ARCHETYPE_LOCOMOTION_POSE_COUNT);
        if (pose >= CC_NPC_ARCHETYPE_POSE_COUNT) {
            pose = CC_NPC_ARCHETYPE_POSE_COUNT - 1;
        }
    }
    NpcArchetypeCache *archetype = &npc_archetypes[appearance->role][pose];
    if (!archetype->ready && pose != 0) {
        archetype = &npc_archetypes[appearance->role][0];
    }
    if (!archetype->ready || archetype->model.meshCount != 1 ||
        archetype->model.materialCount < 1) {
        return false;
    }

    float scale = appearance->stature * (0.82f + size_hint * 0.08f);
    float width = 0.96f + (appearance->body_mass - 1.0f) * 0.22f +
                  (appearance->shoulder_scale - 1.0f) * 0.18f;
    width = fmaxf(0.90f, fminf(width, 1.08f));
    float movement = mode == CC_TRAVERSAL_IDLE ? 0.35f : 1.0f;
    position.y += fabsf(pulse) * 0.010f * movement *
                  appearance->bob_scale;
    float presentation_yaw = WrapAngle(yaw + pulse * 0.012f * movement);

    if (!npc_review_exact_yaw) {
        const float camera_read_yaw = 0.20f;
        presentation_yaw = WrapAngle(
            presentation_yaw +
            WrapAngle(camera_read_yaw - presentation_yaw) * 0.68f);
    }

    const float silhouette_gain = 1.18f;
    const float contact_grid = 0.125f;
    Vector3 contact_shadow = {
        roundf(position.x / contact_grid) * contact_grid,
        0.006f,
        roundf(position.z / contact_grid) * contact_grid,
    };
    DrawContactShadow(
        contact_shadow, 0.62f * scale * width * silhouette_gain,
        0.43f * scale * width * silhouette_gain, presentation_yaw,
        (Color){2, 7, 10, 98});
    SetNpcPalette(appearance, 0.50f, false, (Vector3){0});
    DrawModelEx(archetype->model, position,
                (Vector3){0.0f, 1.0f, 0.0f},
                presentation_yaw * RAD2DEG,
                (Vector3){scale * width * silhouette_gain, scale,
                          scale * width * silhouette_gain}, WHITE);

    Vector3 head_right = {cosf(presentation_yaw), 0.0f,
                          -sinf(presentation_yaw)};
    Vector3 head_up = {0.0f, 1.0f, 0.0f};
    Vector3 head_forward = {sinf(presentation_yaw), 0.0f,
                            cosf(presentation_yaw)};
    Vector3 head = LocalPoint(position, 0.0f, 1.84f * scale,
                              0.01f * scale, presentation_yaw);
    Matrix identity_head = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){0.36f * appearance->head_width * scale * width *
                      silhouette_gain,
                  0.40f * scale,
                  0.33f * appearance->head_depth * scale * width *
                      silhouette_gain});
    Matrix molded_hair = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){scale * width * silhouette_gain,
                  scale,
                  scale * width * silhouette_gain});
    (void)DrawNpcHairFamily(appearance, molded_hair, identity_head);
    if ((appearance->equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) {
        NpcDynamicModuleId headwear = NpcHeadwearModule(
            appearance->headwear_style);
        Color headwear_color = appearance->headwear_style == 0U ?
            appearance->metal : appearance->outer;
        (void)DrawNpcDynamicModule(headwear, identity_head, headwear_color);
    }
    CcFaceRecipe face = CcNpcFaceRecipe(appearance);
    DrawWorldFace(PhysicsAdd(head, Vector3Scale(head_up, 0.025f * scale)),
                  head_right, head_up, head_forward,
                  0.18f * appearance->head_width * scale * width *
                      silhouette_gain,
                  0.20f * scale,
                  0.165f * appearance->head_depth * scale * width *
                      silhouette_gain,
                  &face, expression, false);
    RestoreWorldLighting();
    if (visual_style.hero_ready) {
        float ink_strength = CC_HERO_INK_STRENGTH;
        SetShaderValue(visual_style.hero,
                       visual_style.hero_ink_strength_location,
                       &ink_strength, SHADER_UNIFORM_FLOAT);
    }
    return true;
}

static bool DrawNpcArchetype3D(Vector3 position, float size_hint, float yaw,
                               float phase, CcTraversalMode mode,
                               CcNpcPortraitExpression expression,
                               const CcNpcAppearance *appearance)
{
    return DrawNpcArchetypePose3D(position, size_hint, yaw, phase, mode, -1,
                                  expression, appearance);
}

static void DrawNpcAppearancePoseFigure3D(
    Vector3 position, float size_hint, float yaw,
    const CcNpcAppearance *identity, float phase, CcTraversalMode mode,
    int32_t fixed_pose)
{
    if (identity == NULL) return;
    CcNpcAppearance appearance = *identity;
    uint32_t seed = appearance.seed;
    CcNpcPortraitExpression expression =
        appearance.role == CC_NPC_ROLE_GUARD ||
        appearance.role == CC_NPC_ROLE_RAIDER ? CC_NPC_PORTRAIT_FOCUSED :
                                               CC_NPC_PORTRAIT_NEUTRAL;
    if (!draw_hero_rig_debug && DrawNpcArchetypePose3D(
            position, size_hint, yaw, phase, mode, fixed_pose, expression,
            &appearance)) {
        return;
    }
    UseCharacterLighting();
    float scale = appearance.stature * (0.90f + size_hint * 0.16f);
    float mass = appearance.body_mass;
    float muscle = appearance.muscularity;
    float active = mode == CC_TRAVERSAL_IDLE ? 0.0f : 1.0f;
    float walk_phase = phase * appearance.gait_cadence_scale * 2.0f * PI;
    float walk_wave = sinf(walk_phase);
    float idle_wave = sinf(phase * 0.55f +
                           (float)(seed & UINT32_C(255)) * 0.011f);
    float stride = walk_wave * 0.13f * scale * active *
                   appearance.stride_scale;
    float arm_stride = stride * appearance.arm_swing_scale;
    float bob = mode == CC_TRAVERSAL_WALK ?
        fabsf(walk_wave) * 0.035f * scale * appearance.bob_scale :
        idle_wave * 0.010f * scale;
    float sway = mode == CC_TRAVERSAL_IDLE ? idle_wave * 0.014f * scale :
                                             cosf(walk_phase) * 0.018f * scale;
    float lean = appearance.idle_lean * scale;
    Vector3 hip = LocalPoint(position, sway * 0.30f,
                             0.73f * scale + bob * 0.35f,
                             lean * 0.20f, yaw);
    Vector3 chest = LocalPoint(position, sway, 1.15f * scale + bob,
                               lean, yaw);
    Vector3 neck = LocalPoint(position, sway * 1.08f,
                              1.43f * scale + bob, lean * 1.24f, yaw);
    Vector3 shoulder_l = LocalPoint(position, -0.225f * scale + sway,
                                    1.29f * scale + bob, lean, yaw);
    Vector3 shoulder_r = LocalPoint(position, 0.225f * scale + sway,
                                    1.29f * scale + bob, lean, yaw);
    Vector3 elbow_l = LocalPoint(position, -0.31f * scale + sway,
                                 1.01f * scale + bob,
                                 lean + arm_stride, yaw);
    Vector3 elbow_r = LocalPoint(position, 0.31f * scale + sway,
                                 1.01f * scale + bob,
                                 lean - arm_stride, yaw);
    Vector3 hand_l = LocalPoint(position, -0.275f * scale + sway,
                                0.78f * scale + bob,
                                lean + arm_stride, yaw);
    Vector3 hand_r = LocalPoint(position, 0.275f * scale + sway,
                                0.78f * scale + bob,
                                lean - arm_stride, yaw);
    Vector3 knee_l = LocalPoint(position, -0.12f * scale, 0.40f * scale,
                                stride, yaw);
    Vector3 knee_r = LocalPoint(position, 0.12f * scale, 0.40f * scale,
                                -stride, yaw);
    Vector3 foot_l = LocalPoint(position, -0.13f * scale, 0.08f,
                                0.06f + stride, yaw);
    Vector3 foot_r = LocalPoint(position, 0.13f * scale, 0.08f,
                                0.06f - stride, yaw);
    if (mode == CC_TRAVERSAL_CLIMB) {
        elbow_l = LocalPoint(position, -0.25f * scale, 1.50f * scale,
                             0.10f, yaw);
        hand_l = LocalPoint(position, -0.18f * scale, 1.70f * scale,
                            0.15f, yaw);
        knee_r = LocalPoint(position, 0.13f * scale, 0.58f * scale,
                            0.18f, yaw);
    } else if (mode == CC_TRAVERSAL_DROP) {
        elbow_l = LocalPoint(position, -0.28f * scale, 1.48f * scale,
                             0.02f, yaw);
        elbow_r = LocalPoint(position, 0.28f * scale, 1.48f * scale,
                             0.02f, yaw);
        hand_l = LocalPoint(position, -0.20f * scale, 1.68f * scale,
                            0.04f, yaw);
        hand_r = LocalPoint(position, 0.20f * scale, 1.68f * scale,
                            0.04f, yaw);
        knee_l = LocalPoint(position, -0.13f * scale, 0.56f * scale,
                            0.17f, yaw);
        knee_r = LocalPoint(position, 0.13f * scale, 0.56f * scale,
                            0.17f, yaw);
        foot_l = LocalPoint(position, -0.14f * scale, 0.30f * scale,
                            0.03f, yaw);
        foot_r = LocalPoint(position, 0.14f * scale, 0.30f * scale,
                            0.03f, yaw);
    }

    DrawContactShadow(
        (Vector3){position.x, position.y + 0.006f, position.z},
        0.62f * scale, 0.43f * scale, yaw, (Color){2, 7, 10, 115});
    float thigh = (0.084f + muscle * 0.024f) * mass * scale;
    DrawCylinderEx(hip, knee_l, thigh, thigh * 0.82f, 9,
                   appearance.trousers);
    DrawCylinderEx(hip, knee_r, thigh, thigh * 0.82f, 9,
                   appearance.trousers);
    DrawCylinderEx(knee_l, foot_l, thigh * 0.82f, thigh * 0.62f, 9,
                   ShadeColor(appearance.trousers, 0.82f));
    DrawCylinderEx(knee_r, foot_r, thigh * 0.82f, thigh * 0.62f, 9,
                   ShadeColor(appearance.trousers, 0.82f));
    DrawOrientedBox(foot_l, (Vector3){0.0f, 0.02f * scale, 0.07f * scale},
                    (Vector3){0.19f * scale, 0.11f * scale, 0.32f * scale},
                    yaw, appearance.leather);
    DrawOrientedBox(foot_r, (Vector3){0.0f, 0.02f * scale, 0.07f * scale},
                    (Vector3){0.19f * scale, 0.11f * scale, 0.32f * scale},
                    yaw, appearance.leather);
    DrawCylinderEx(hip, chest, 0.18f * mass * scale,
                   0.24f * appearance.shoulder_scale * mass * scale, 10,
                   appearance.underlayer);
    Vector3 waist = LocalPoint(position, sway * 0.48f,
                               0.88f * scale + bob * 0.62f,
                               lean * 0.46f, yaw);
    Vector3 upper_chest = LocalPoint(position, sway,
                                     1.29f * scale + bob, lean, yaw);
    DrawCylinderEx(waist, upper_chest, 0.195f * mass * scale,
                   0.25f * appearance.shoulder_scale * mass * scale, 10,
                   appearance.outer);
    DrawCylinder(waist, 0.205f * mass * scale, 0.205f * mass * scale,
                 0.065f * scale, 10, appearance.leather);
    DrawNpcGarmentCut(&appearance, hip, chest, shoulder_l, yaw, scale);
    DrawCylinderEx(shoulder_l, elbow_l, 0.077f * mass * scale,
                   0.063f * mass * scale, 8, appearance.outer);
    DrawCylinderEx(elbow_l, hand_l, 0.063f * mass * scale,
                   0.050f * mass * scale, 8, appearance.underlayer);
    DrawCylinderEx(shoulder_r, elbow_r, 0.077f * mass * scale,
                   0.063f * mass * scale, 8, appearance.outer);
    DrawCylinderEx(elbow_r, hand_r, 0.063f * mass * scale,
                   0.050f * mass * scale, 8, appearance.underlayer);
    DrawSmallSphere(hand_l, 0.060f * scale, appearance.skin);
    DrawSmallSphere(hand_r, 0.060f * scale, appearance.skin);
    Vector3 head = LocalPoint(position, sway * 1.12f,
                              1.62f * scale + bob, lean * 1.34f, yaw);
    DrawNpcHead(&appearance, head, yaw, scale);
    DrawNpcEquipment(&appearance, position, hip, chest,
                     shoulder_l, shoulder_r, hand_l, yaw, scale);

    if (draw_hero_rig_debug) {
        Vector3 offset = LocalPoint((Vector3){0.0f, 0.0f, 0.0f},
                                    0.025f, 0.012f, 0.050f, yaw);
        hip = Add3(hip, offset);
        chest = Add3(chest, offset);
        neck = Add3(neck, offset);
        shoulder_l = Add3(shoulder_l, offset);
        shoulder_r = Add3(shoulder_r, offset);
        elbow_l = Add3(elbow_l, offset);
        elbow_r = Add3(elbow_r, offset);
        hand_l = Add3(hand_l, offset);
        hand_r = Add3(hand_r, offset);
        knee_l = Add3(knee_l, offset);
        knee_r = Add3(knee_r, offset);
        foot_l = Add3(foot_l, offset);
        foot_r = Add3(foot_r, offset);
        DrawBoneSegment(hip, chest, appearance.accent);
        DrawBoneSegment(chest, neck, appearance.accent);
        DrawBoneSegment(shoulder_l, shoulder_r, appearance.accent);
        DrawBoneSegment(shoulder_l, elbow_l, appearance.accent);
        DrawBoneSegment(elbow_l, hand_l, appearance.accent);
        DrawBoneSegment(shoulder_r, elbow_r, appearance.accent);
        DrawBoneSegment(elbow_r, hand_r, appearance.accent);
        DrawBoneSegment(hip, knee_l, appearance.accent);
        DrawBoneSegment(knee_l, foot_l, appearance.accent);
        DrawBoneSegment(hip, knee_r, appearance.accent);
        DrawBoneSegment(knee_r, foot_r, appearance.accent);
        const Vector3 joints[] = {
            hip, chest, neck, shoulder_l, shoulder_r, elbow_l, elbow_r,
            hand_l, hand_r, knee_l, knee_r, foot_l, foot_r
        };
        for (int32_t joint = 0;
             joint < (int32_t)(sizeof(joints) / sizeof(joints[0])); ++joint) {
            DrawBoneJoint(joints[joint], appearance.accent);
        }
    }
    RestoreWorldLighting();
}

static void DrawNpcAppearanceFigure3D(
    Vector3 position, float size_hint, float yaw,
    const CcNpcAppearance *identity, float phase, CcTraversalMode mode)
{
    DrawNpcAppearancePoseFigure3D(position, size_hint, yaw, identity, phase,
                                  mode, -1);
}

static void DrawNpcFigurePose3D(Vector3 position, float size_hint, float yaw,
                                uint32_t seed, CcNpcRole role, Color accent,
                                float phase, CcTraversalMode mode,
                                int32_t fixed_pose)
{
    enum { NPC_APPEARANCE_CACHE_CAPACITY = 64 };
    typedef struct NpcAppearanceCacheEntry {
        bool valid;
        uint32_t seed;
        CcNpcRole role;
        Color accent;
        CcNpcAppearance appearance;
    } NpcAppearanceCacheEntry;
    static NpcAppearanceCacheEntry cache[NPC_APPEARANCE_CACHE_CAPACITY];

    uint32_t accent_key = (uint32_t)accent.r |
        ((uint32_t)accent.g << 8U) |
        ((uint32_t)accent.b << 16U) |
        ((uint32_t)accent.a << 24U);
    uint32_t hash = seed ^ accent_key ^
        ((uint32_t)role * UINT32_C(0x9e3779b9));
    hash ^= hash >> 16U;
    NpcAppearanceCacheEntry *entry =
        &cache[hash % NPC_APPEARANCE_CACHE_CAPACITY];
    bool cache_hit = entry->valid && entry->seed == seed &&
        entry->role == role && entry->accent.r == accent.r &&
        entry->accent.g == accent.g && entry->accent.b == accent.b &&
        entry->accent.a == accent.a;
    if (!cache_hit) {
        entry->valid = true;
        entry->seed = seed;
        entry->role = role;
        entry->accent = accent;
        entry->appearance = CcNpcAppearanceGenerate(seed, role, accent);
    }
    DrawNpcAppearancePoseFigure3D(position, size_hint, yaw,
                                  &entry->appearance, phase, mode,
                                  fixed_pose);
}

static void DrawNpcFigure3D(Vector3 position, float size_hint, float yaw,
                            uint32_t seed, CcNpcRole role, Color accent,
                            float phase, CcTraversalMode mode)
{
    DrawNpcFigurePose3D(position, size_hint, yaw, seed, role, accent, phase,
                        mode, -1);
}

static void DrawVisibleNpcFigure3D(Vector3 position, float size_hint,
                                   float yaw, uint32_t seed, CcNpcRole role,
                                   Color accent, float phase,
                                   CcTraversalMode mode, Vector3 focus)
{
    if (!SceneryPointVisible(position.x, position.z, focus)) return;
    DrawNpcFigure3D(position, size_hint, yaw, seed, role, accent, phase, mode);
}

static void DrawVisibleNpcPoseFigure3D(
    Vector3 position, float size_hint, float yaw, uint32_t seed,
    CcNpcRole role, Color accent, float phase, CcTraversalMode mode,
    int32_t fixed_pose, Vector3 focus)
{
    if (!SceneryPointVisible(position.x, position.z, focus)) return;
    DrawNpcFigurePose3D(position, size_hint, yaw, seed, role, accent, phase,
                        mode, fixed_pose);
}

static void DrawAmbientNpcRoute3D(Vector2 start, Vector2 end,
                                  float size_hint, uint32_t seed,
                                  CcNpcRole role, Color accent, float clock,
                                  float route_offset, float world_pressure,
                                  Vector3 focus)
{
    float route_speed = 0.10f + Clamp(world_pressure, 0.0f, 1.0f) * 0.08f;
    float cycle = fmodf(clock * route_speed + route_offset, 1.0f);
    if (cycle < 0.0f) cycle += 1.0f;
    bool moving_forward = cycle < 0.40f;
    bool resting_at_end = cycle >= 0.40f && cycle < 0.50f;
    bool moving_back = cycle >= 0.50f && cycle < 0.90f;
    float progress = 0.0f;
    if (moving_forward) {
        progress = SmoothStep01(cycle / 0.40f);
    } else if (resting_at_end) {
        progress = 1.0f;
    } else if (moving_back) {
        progress = 1.0f - SmoothStep01((cycle - 0.50f) / 0.40f);
    }
    Vector2 point = {
        start.x + (end.x - start.x) * progress,
        start.y + (end.y - start.y) * progress,
    };
    float route_yaw = atan2f(end.x - start.x, end.y - start.y);
    if (moving_back || cycle >= 0.90f) route_yaw = WrapAngle(route_yaw + PI);
    CcTraversalMode mode = moving_forward || moving_back ?
        CC_TRAVERSAL_WALK : CC_TRAVERSAL_IDLE;
    int32_t fixed_pose = -1;
    if (resting_at_end) {
        fixed_pose = CC_NPC_ARCHETYPE_WORK_POSE;
    } else if (cycle >= 0.90f && world_pressure >= 0.35f) {
        fixed_pose = CC_NPC_ARCHETYPE_REACT_POSE;
    }
    DrawVisibleNpcPoseFigure3D(
        TerrainWorldPoint(point.x, point.y), size_hint, route_yaw,
        seed, role, accent, clock * 4.2f + route_offset * 2.0f * PI,
        mode, fixed_pose, focus);
}

static void DrawPitchedFoot(Vector3 heel, Vector3 toe, float yaw, Color color)
{
    Vector3 center = PhysicsScale(PhysicsAdd(heel, toe), 0.5f);
    Vector3 difference = PhysicsSubtract(toe, heel);
    float horizontal = sqrtf(difference.x * difference.x +
                             difference.z * difference.z);
    Vector3 fallback_forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 horizontal_forward = horizontal > 0.0001f ?
        (Vector3){difference.x / horizontal, 0.0f,
                  difference.z / horizontal} : fallback_forward;
    horizontal_forward = PhysicsNormalizeOr(
        PhysicsLerp(fallback_forward, horizontal_forward,
                    SmoothStep01(horizontal / 0.08f)),
        fallback_forward);
    float solved_yaw = atan2f(horizontal_forward.x, horizontal_forward.z);
    float pitch = atan2f(difference.y, fmaxf(0.0001f, horizontal));
    center.y += 0.045f;
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(solved_yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(-pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    DrawBox((Vector3){0.0f, 0.0f, 0.0f}, (Vector3){0.20f, 0.11f, 0.34f},
            color);
    rlPopMatrix();
}

static Color HumanoidContactColor(CcHumanoidContact contact)
{
    switch (contact) {
        case CC_HUMANOID_CONTACT_HEEL: return (Color){116, 224, 197, 255};
        case CC_HUMANOID_CONTACT_FLAT: return WORLD_GOLD;
        case CC_HUMANOID_CONTACT_TOE: return (Color){245, 151, 66, 255};
        case CC_HUMANOID_CONTACT_SWING: return WORLD_VIOLET;
        case CC_HUMANOID_CONTACT_AIR:
        default: return WORLD_MUTED;
    }
}

static float PoseAxisYaw(Vector3 left, Vector3 right, float fallback)
{
    Vector3 axis = PhysicsSubtract(right, left);
    float horizontal = sqrtf(axis.x * axis.x + axis.z * axis.z);
    return horizontal > 0.0001f ? atan2f(-axis.z, axis.x) : fallback;
}

static void DrawHeroSkinRigOverlay(const CcHumanoidGait *gait,
                                   const CcHumanoidSkinPose *skin,
                                   const CcLocalCapeState *cape)
{
    Vector3 offset = {0.018f, 0.010f, 0.018f};
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        Color color = Fade(WORLD_GOLD, 0.78f);
        if (bone == CC_HUMANOID_SKIN_ROOT) color = Fade(WORLD_TEAL, 0.85f);
        else if (bone >= CC_HUMANOID_SKIN_THIGH_LEFT &&
                 bone <= CC_HUMANOID_SKIN_FOOT_LEFT) {
            color = Fade(HumanoidContactColor(gait->feet[0].contact), 0.82f);
        } else if (bone >= CC_HUMANOID_SKIN_THIGH_RIGHT) {
            color = Fade(HumanoidContactColor(gait->feet[1].contact), 0.82f);
        }
        Vector3 head = Add3(FromLimbVector(skin->bones[bone].head), offset);
        Vector3 tail = Add3(FromLimbVector(skin->bones[bone].tail), offset);
        DrawBoneSegment(head, tail, color);
        DrawBoneJoint(head, color);
    }
    if (cape != NULL && cape->initialized) {
        for (int32_t point = 0; point < CC_LOCAL_CAPE_POINT_COUNT - 1;
             ++point) {
            Vector3 head = Add3(cape->point[point], offset);
            Vector3 tail = Add3(cape->point[point + 1], offset);
            DrawBoneSegment(head, tail, Fade(WORLD_VIOLET, 0.86f));
            DrawBoneJoint(head, Fade(WORLD_VIOLET, 0.92f));
        }
    }
}

static Vector3 NpcModuleLocalPoint(Vector3 origin, Vector3 right, Vector3 up,
                                   Vector3 forward, Vector3 local)
{
    Vector3 result = PhysicsAdd(origin, PhysicsScale(right, local.x));
    result = PhysicsAdd(result, PhysicsScale(up, local.y));
    return PhysicsAdd(result, PhysicsScale(forward, local.z));
}

static Matrix NpcModuleTransform(Vector3 origin, Vector3 right, Vector3 up,
                                 Vector3 forward, Vector3 scale)
{
    return (Matrix){
        .m0 = right.x * scale.x,
        .m1 = right.y * scale.x,
        .m2 = right.z * scale.x,
        .m3 = 0.0f,
        .m4 = up.x * scale.y,
        .m5 = up.y * scale.y,
        .m6 = up.z * scale.y,
        .m7 = 0.0f,
        .m8 = forward.x * scale.z,
        .m9 = forward.y * scale.z,
        .m10 = forward.z * scale.z,
        .m11 = 0.0f,
        .m12 = origin.x,
        .m13 = origin.y,
        .m14 = origin.z,
        .m15 = 1.0f,
    };
}


static void DrawWayfarerHeroDetails(const CcHumanoidSkinPose *skin)
{
    if (skin == NULL || !skin->valid) return;
    Vector3 up = PhysicsNormalizeOr(FromLimbVector(skin->body_up),
                                    (Vector3){0.0f, 1.0f, 0.0f});
    Vector3 right = PhysicsNormalizeOr(FromLimbVector(skin->body_right),
                                       (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 forward = PhysicsNormalizeOr(FromLimbVector(skin->body_forward),
                                         (Vector3){0.0f, 0.0f, 1.0f});
    Vector3 chest = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_CHEST_FRONT].position);
    chest = PhysicsAdd(chest, PhysicsScale(forward, 0.032f));

    Color portrait_burgundy = CC_STYLE_HERO_OUTER;
    Color broken_gold = CC_STYLE_HERO_ACCENT;
    Vector3 panel = PhysicsAdd(chest, PhysicsScale(up, -0.018f));


    static const CcHumanoidSkinBone shoulder_bones[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    for (int32_t side = 0; side < 2; ++side) {
        Vector3 shoulder = FromLimbVector(
            skin->bones[shoulder_bones[side]].head);
        Vector3 upper_arm = FromLimbVector(
            skin->bones[shoulder_bones[side]].tail);
        Vector3 tab_end = Vector3Lerp(shoulder, upper_arm, 0.24f);
        shoulder = PhysicsAdd(shoulder, PhysicsScale(forward, 0.014f));
        tab_end = PhysicsAdd(tab_end, PhysicsScale(forward, 0.014f));
        DrawCylinderEx(shoulder, tab_end, 0.108f, 0.084f, 6,
                       portrait_burgundy);
    }

    Vector3 left_top = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){-0.112f, 0.076f, 0.022f});
    Vector3 left_low = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){-0.020f, -0.030f, 0.022f});
    Vector3 right_low = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){0.026f, -0.014f, 0.022f});
    Vector3 right_top = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){0.116f, 0.064f, 0.022f});
    DrawCylinderEx(left_top, left_low, 0.020f, 0.016f, 5, broken_gold);
    DrawCylinderEx(right_low, right_top, 0.016f, 0.020f, 5, broken_gold);
}

static bool DrawNpcDynamicModule(NpcDynamicModuleId id, Matrix transform,
                                 Color color)
{
    (void)color;
    if (id < 0 || id >= NPC_DYNAMIC_MODULE_COUNT) return false;
    NpcDynamicModuleCache *module = &npc_dynamic_modules[id];
    if (!module->ready || module->model.meshCount != 1 ||
        module->model.materialCount < 1) return false;
    for (int32_t mesh = 0; mesh < module->model.meshCount; ++mesh) {
        int32_t material = module->model.meshMaterial[mesh];
        if (material < 0 || material >= module->model.materialCount) {
            material = 0;
        }
        DrawMesh(module->model.meshes[mesh],
                 module->model.materials[material], transform);
    }
    return true;
}

static int32_t NpcHeadFamilyForAppearance(
    const CcNpcAppearance *appearance)
{
    if (appearance == NULL ||
        appearance->head_family >= CC_NPC_HEAD_FAMILY_COUNT) {
        return CC_NPC_HEAD_FAMILY_SQUARE;
    }
    return appearance->head_family;
}

static bool DrawNpcHeadFamily(const CcNpcAppearance *appearance,
                              Matrix transform, Matrix fallback_transform)
{
    int32_t family = NpcHeadFamilyForAppearance(appearance);
    NpcHeadFamilyCache *head = &npc_head_families[family];
    if (!head->ready || head->model.materialCount < 1) {
        return DrawNpcDynamicModule(NPC_DYNAMIC_HEAD, fallback_transform,
                                    appearance != NULL ? appearance->skin :
                                                         WHITE);
    }
    for (int32_t mesh = 0; mesh < head->model.meshCount; ++mesh) {
        int32_t material = head->model.meshMaterial[mesh];
        if (material < 0 || material >= head->model.materialCount) {
            material = 0;
        }
        DrawMesh(head->model.meshes[mesh], head->model.materials[material],
                 transform);
    }
    return true;
}

static int32_t NpcHairFamilyForAppearance(
    const CcNpcAppearance *appearance)
{
    if (appearance == NULL ||
        appearance->hair_family >= CC_NPC_HAIR_FAMILY_COUNT) {
        return CC_NPC_HAIR_FAMILY_CROPPED;
    }
    return appearance->hair_family;
}

static bool DrawNpcHairFamily(const CcNpcAppearance *appearance,
                              Matrix transform, Matrix fallback_transform)
{
    int32_t family = NpcHairFamilyForAppearance(appearance);
    NpcHairFamilyCache *hair = &npc_hair_families[family];
    if (!hair->ready || hair->model.materialCount < 1) {
        int32_t fallback = appearance != NULL ?
            (int32_t)appearance->hair_style % CC_NPC_DYNAMIC_HAIR_COUNT : 0;
        if (fallback < 0) fallback += CC_NPC_DYNAMIC_HAIR_COUNT;
        return DrawNpcDynamicModule(
            (NpcDynamicModuleId)(NPC_DYNAMIC_HAIR_0 + fallback),
            fallback_transform,
            appearance != NULL ? appearance->hair : WHITE);
    }
    for (int32_t mesh = 0; mesh < hair->model.meshCount; ++mesh) {
        int32_t material = hair->model.meshMaterial[mesh];
        if (material < 0 || material >= hair->model.materialCount) {
            material = 0;
        }
        DrawMesh(hair->model.meshes[mesh], hair->model.materials[material],
                 transform);
    }
    return true;
}

static bool DrawNpcDynamicBoneModule(NpcDynamicModuleId id,
                                     const CcHumanoidSkinBonePose *bone,
                                     float width, float depth, Color color)
{
    if (bone == NULL) return false;
    Vector3 head = FromLimbVector(bone->head);
    Vector3 tail = FromLimbVector(bone->tail);
    float length = PhysicsLength(PhysicsSubtract(tail, head));
    if (!isfinite(length) || length <= 0.001f) return false;
    return DrawNpcDynamicModule(
        id, NpcModuleTransform(head, FromLimbVector(bone->right),
                               FromLimbVector(bone->up),
                               FromLimbVector(bone->forward),
                               (Vector3){width, length, depth}), color);
}

static bool NpcDynamicCoreReady(const CcNpcAppearance *appearance)
{
    static const NpcDynamicModuleId core[] = {
        NPC_DYNAMIC_TORSO, NPC_DYNAMIC_PELVIS, NPC_DYNAMIC_UPPER_ARM,
        NPC_DYNAMIC_FOREARM, NPC_DYNAMIC_THIGH, NPC_DYNAMIC_SHIN,
        NPC_DYNAMIC_HAND, NPC_DYNAMIC_FOOT,
    };
    for (int32_t i = 0;
        i < (int32_t)(sizeof(core) / sizeof(core[0])); ++i) {
        if (!npc_dynamic_modules[core[i]].ready) return false;
    }
    int32_t family = NpcHeadFamilyForAppearance(appearance);
    if (!npc_head_families[family].ready &&
        !npc_dynamic_modules[NPC_DYNAMIC_HEAD].ready) return false;
    int32_t hair_family = NpcHairFamilyForAppearance(appearance);
    if (npc_hair_families[hair_family].ready) return true;
    int32_t hair = (int32_t)appearance->hair_style %
                   CC_NPC_DYNAMIC_HAIR_COUNT;
    if (hair < 0) hair += CC_NPC_DYNAMIC_HAIR_COUNT;
    return npc_dynamic_modules[NPC_DYNAMIC_HAIR_0 + hair].ready;
}

static NpcDynamicModuleId NpcChestArmorModule(
    const CcNpcAppearance *appearance, bool featured_hero)
{
    if (featured_hero) return NPC_DYNAMIC_CHEST_PLATE_HERO;
    if (appearance != NULL && appearance->role == CC_NPC_ROLE_RAIDER) {
        return NPC_DYNAMIC_CHEST_PLATE_RAIDER;
    }
    return NPC_DYNAMIC_CHEST_PLATE;
}

static NpcDynamicModuleId NpcShoulderArmorModule(
    const CcNpcAppearance *appearance, bool featured_hero, int32_t side)
{
    if (featured_hero) {
        return side == 0 ? NPC_DYNAMIC_PAULDRON_HERO :
                           NPC_DYNAMIC_PAULDRON_RAIDER;
    }
    if (appearance != NULL && appearance->role == CC_NPC_ROLE_RAIDER) {
        return NPC_DYNAMIC_PAULDRON_RAIDER;
    }
    return NPC_DYNAMIC_PAULDRON;
}

static bool DrawDynamicNpcModules(const CcLocalAgent *agent,
                                  const CcHumanoidSkinPose *skin,
                                  const CcNpcAppearance *appearance)
{
    if (agent == NULL || skin == NULL || appearance == NULL || !skin->valid ||
        !NpcDynamicCoreReady(appearance)) return false;

    Vector3 body_right = FromLimbVector(skin->body_right);
    Vector3 body_up = FromLimbVector(skin->body_up);
    Vector3 body_forward = FromLimbVector(skin->body_forward);
    float movement_weight = agent->humanoid.ragdoll.active ? 0.0f :
        SmoothStep01(fabsf(agent->humanoid.speed.value) / 0.90f);
    static const float stepped_wave[8] = {
        0.0f, 0.70f, 1.0f, 0.70f, 0.0f, -0.70f, -1.0f, -0.70f,
    };
    float secondary_wave = 0.0f;
    if (agent->stepped_pose.initialized &&
        agent->stepped_pose.locomotion_bin >= 0 &&
        agent->stepped_pose.locomotion_bin < 8) {
        secondary_wave = stepped_wave[agent->stepped_pose.locomotion_bin];
    }
    Vector3 gear_up = PhysicsNormalizeOr(
        PhysicsAdd(body_up,
                   PhysicsAdd(PhysicsScale(body_right,
                                           secondary_wave * 0.075f *
                                               movement_weight),
                              PhysicsScale(body_forward,
                                           0.045f * movement_weight))),
        body_up);
    Vector3 gear_right = PhysicsNormalizeOr(
        PhysicsCross(gear_up, body_forward), body_right);
    Vector3 gear_forward = PhysicsNormalizeOr(
        PhysicsCross(gear_right, gear_up), body_forward);
    float mass = appearance->body_mass;
    float muscle = appearance->muscularity;
    bool featured_hero = agent->crowned;
    Color outer = appearance->outer;
    Color trousers = appearance->trousers;
    if (CombatIsDefeated(&agent->combat)) {
        float defeat_drain = featured_hero ? 0.22f : 0.72f;
        outer = BlendColor(outer, (Color){50, 49, 52, 255}, defeat_drain);
        trousers = BlendColor(trousers, (Color){42, 42, 44, 255},
                               defeat_drain);
    } else if (agent->combat.hit_flash_seconds > 0.0f) {
        outer = BlendColor(outer, WORLD_INK, 0.72f);
    }
    CcNpcAppearance palette_appearance = *appearance;
    palette_appearance.outer = outer;
    palette_appearance.trousers = trousers;
    Vector3 presentation_head = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_HEAD].position);
    SetNpcPalette(&palette_appearance, featured_hero ? 0.56f : 0.68f,
                  featured_hero, presentation_head);


    bool drew = DrawNpcBodySkin(skin, appearance, featured_hero);
    if (!drew) return false;

    const CcHumanoidSkinBonePose *spine =
        &skin->bones[CC_HUMANOID_SKIN_SPINE];
    Vector3 torso_base = FromLimbVector(spine->head);
    Vector3 torso_top = FromLimbVector(
        skin->bones[CC_HUMANOID_SKIN_CHEST].tail);
    float torso_length = PhysicsLength(
        PhysicsSubtract(torso_top, torso_base));
    drew = DrawNpcDynamicModule(
        NPC_DYNAMIC_TORSO,
        NpcModuleTransform(torso_base, body_right, body_up, body_forward,
                           (Vector3){(featured_hero ? 0.56f : 0.62f) * mass *
                                         appearance->shoulder_scale,
                                     torso_length,
                                     (featured_hero ? 0.54f : 0.55f) * mass}),
        outer);

    const CcHumanoidSkinBonePose *pelvis =
        &skin->bones[CC_HUMANOID_SKIN_PELVIS];
    float pelvis_length = PhysicsLength(PhysicsSubtract(
        FromLimbVector(pelvis->tail), FromLimbVector(pelvis->head)));
    pelvis_length = fmaxf(pelvis_length, 0.16f);
    drew = DrawNpcDynamicModule(
        NPC_DYNAMIC_PELVIS,
        NpcModuleTransform(FromLimbVector(pelvis->head),
                           FromLimbVector(pelvis->right),
                           FromLimbVector(pelvis->up),
                           FromLimbVector(pelvis->forward),
                           (Vector3){(featured_hero ? 0.46f : 0.55f) * mass,
                                     pelvis_length,
                                     (featured_hero ? 0.42f : 0.54f) * mass}),
        trousers) && drew;

    static const CcHumanoidSkinBone upper_arms[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    static const CcHumanoidSkinBone forearms[] = {
        CC_HUMANOID_SKIN_FOREARM_LEFT,
        CC_HUMANOID_SKIN_FOREARM_RIGHT,
    };
    static const CcHumanoidSkinBone thighs[] = {
        CC_HUMANOID_SKIN_THIGH_LEFT,
        CC_HUMANOID_SKIN_THIGH_RIGHT,
    };
    static const CcHumanoidSkinBone shins[] = {
        CC_HUMANOID_SKIN_SHIN_LEFT,
        CC_HUMANOID_SKIN_SHIN_RIGHT,
    };
    static const CcHumanoidSkinBone feet[] = {
        CC_HUMANOID_SKIN_FOOT_LEFT,
        CC_HUMANOID_SKIN_FOOT_RIGHT,
    };
    float arm_width = featured_hero ? 0.152f * mass :
        (0.135f + muscle * 0.020f) * mass;
    float leg_width = featured_hero ? 0.215f * mass :
        (0.175f + muscle * 0.025f) * mass;
    for (int32_t side = 0; side < 2; ++side) {
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_UPPER_ARM, &skin->bones[upper_arms[side]],
            featured_hero ? 0.150f * mass : arm_width,
            featured_hero ? 0.158f * mass : arm_width * 0.92f,
            outer) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_FOREARM, &skin->bones[forearms[side]],
            featured_hero ? 0.136f * mass : arm_width * 0.86f,
            featured_hero ? 0.146f * mass : arm_width * 0.80f,
            appearance->underlayer) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_THIGH, &skin->bones[thighs[side]],
            leg_width, leg_width * (featured_hero ? 1.00f : 0.94f),
            trousers) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_SHIN, &skin->bones[shins[side]],
            leg_width * 0.86f, leg_width * 0.82f,
            ShadeColor(trousers, 0.84f)) && drew;
        const CcHumanoidSkinBonePose *foot = &skin->bones[feet[side]];
        Vector3 foot_head = FromLimbVector(foot->head);
        float foot_length = PhysicsLength(PhysicsSubtract(
            FromLimbVector(foot->tail), foot_head));
        drew = DrawNpcDynamicModule(
            NPC_DYNAMIC_FOOT,
            NpcModuleTransform(foot_head, FromLimbVector(foot->right),
                               FromLimbVector(foot->up),
                               FromLimbVector(foot->forward),
                               (Vector3){(featured_hero ? 0.195f : 0.23f) *
                                             mass,
                                         foot_length,
                                         (featured_hero ? 0.175f : 0.20f) *
                                             mass}),
            appearance->leather) && drew;
    }

    const CcHumanoidSkinBonePose *head_bone =
        &skin->bones[CC_HUMANOID_SKIN_HEAD];
    Vector3 head = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_HEAD].position);
    Vector3 head_right = FromLimbVector(head_bone->right);
    Vector3 head_up = FromLimbVector(head_bone->up);
    Vector3 head_forward = FromLimbVector(head_bone->forward);
    float hero_head_scale = featured_hero ? 1.04f : 1.0f;
    Vector3 head_scale = {appearance->head_width * hero_head_scale,
                          hero_head_scale,
                          appearance->head_depth * hero_head_scale};
    Matrix head_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward, head_scale);
    Matrix fallback_head_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){0.30f * appearance->head_width * hero_head_scale,
                  0.34f * hero_head_scale,
                  0.28f * appearance->head_depth * hero_head_scale});
    drew = DrawNpcHeadFamily(appearance, head_transform,
                             fallback_head_transform) && drew;
    Matrix hair_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward, head_scale);
    Matrix fallback_hair_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){0.25f * appearance->head_width * hero_head_scale,
                  0.29f * hero_head_scale,
                  0.33f * appearance->head_depth * hero_head_scale});
    drew = DrawNpcHairFamily(appearance, hair_transform,
                             fallback_hair_transform) && drew;

    Vector3 back = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_BACK].position);
    if ((appearance->equipment & CC_NPC_EQUIPMENT_MANTLE) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_MANTLE].ready) {
        Vector3 mantle_root = NpcModuleLocalPoint(
            back, body_right, body_up, body_forward,
            (Vector3){-0.06f + secondary_wave * 0.020f * movement_weight,
                      0.12f + fabsf(secondary_wave) * 0.010f *
                                  movement_weight,
                      -0.015f - 0.018f * movement_weight});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_MANTLE,
            NpcModuleTransform(mantle_root, gear_right, gear_up,
                               gear_forward,
                               (Vector3){(featured_hero ? 0.43f : 0.62f) *
                                             mass,
                                         featured_hero ? 0.50f : 0.64f,
                                         featured_hero ? 0.34f : 0.50f}),
            ShadeColor(appearance->outer, 0.68f));
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
        Vector3 chest_front = FromLimbVector(
            skin->sockets[CC_HUMANOID_SOCKET_CHEST_FRONT].position);
        (void)DrawNpcDynamicModule(
            NpcChestArmorModule(appearance, featured_hero),
            NpcModuleTransform(chest_front, gear_right, gear_up,
                               gear_forward,
                               (Vector3){(featured_hero ? 0.34f : 0.46f) *
                                             mass,
                                         featured_hero ? 0.43f : 0.50f,
                                         (featured_hero ? 0.24f : 0.30f) *
                                             mass}),
            appearance->metal);
        for (int32_t side = 0; side < 2; ++side) {
            if (!featured_hero && appearance->role == CC_NPC_ROLE_RAIDER &&
                side != (int32_t)(appearance->seed & UINT32_C(1))) {
                continue;
            }
            const CcHumanoidSkinBonePose *shoulder =
                &skin->bones[upper_arms[side]];
            float shoulder_scale = featured_hero && side == 1 ? 0.78f : 1.0f;
            (void)DrawNpcDynamicModule(
                NpcShoulderArmorModule(appearance, featured_hero, side),
                NpcModuleTransform(FromLimbVector(shoulder->head),
                                   FromLimbVector(shoulder->right),
                                   FromLimbVector(shoulder->up),
                                   FromLimbVector(shoulder->forward),
                                   (Vector3){(featured_hero ? 0.15f : 0.22f) *
                                                 mass * shoulder_scale,
                                             (featured_hero ? 0.15f : 0.20f) *
                                                 shoulder_scale,
                                             (featured_hero ? 0.14f : 0.21f) *
                                                 mass * shoulder_scale}),
                appearance->metal);
        }
    }

    Vector3 pelvis_center = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_BELT].position);
    if (appearance->garment_style == 1U &&
        npc_dynamic_modules[NPC_DYNAMIC_COAT_TAIL].ready) {
        for (int32_t side = -1; side <= 1; side += 2) {
            Vector3 tail = NpcModuleLocalPoint(
                pelvis_center, body_right, body_up, body_forward,
                (Vector3){(float)side * 0.11f, -0.08f, -0.09f});
            (void)DrawNpcDynamicModule(
                NPC_DYNAMIC_COAT_TAIL,
                NpcModuleTransform(tail, body_right, body_up, body_forward,
                                   (Vector3){0.22f * mass, 0.40f, 0.28f}),
                ShadeColor(appearance->outer, 0.72f));
        }
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_APRON) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_APRON].ready) {
        Vector3 apron = NpcModuleLocalPoint(
            pelvis_center, body_right, body_up, body_forward,
            (Vector3){0.0f, -0.04f, 0.18f});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_APRON,
            NpcModuleTransform(apron, body_right, body_up, body_forward,
                               (Vector3){0.42f * mass, 0.62f, 0.28f}),
            appearance->underlayer);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_PACK) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_PACK].ready) {
        Vector3 pack = NpcModuleLocalPoint(
            back, body_right, body_up, body_forward,
            (Vector3){secondary_wave * 0.016f * movement_weight,
                      -0.16f + fabsf(secondary_wave) * 0.012f *
                                   movement_weight,
                      -0.13f - 0.012f * movement_weight});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_PACK,
            NpcModuleTransform(pack, gear_right, gear_up, gear_forward,
                               (Vector3){0.42f * mass, 0.48f, 0.30f}),
            appearance->leather);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_SATCHEL) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_SATCHEL].ready) {
        Vector3 satchel = NpcModuleLocalPoint(
            pelvis_center, body_right, body_up, body_forward,
            (Vector3){0.29f * mass + secondary_wave * 0.026f *
                                         movement_weight,
                      -0.13f - fabsf(secondary_wave) * 0.010f *
                                   movement_weight,
                      -0.08f});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_SATCHEL,
            NpcModuleTransform(satchel, gear_right, gear_up, gear_forward,
                               (Vector3){0.24f, 0.28f, 0.20f}),
            appearance->leather);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_TOOL) != 0U &&
        appearance->role != CC_NPC_ROLE_GUARD &&
        appearance->role != CC_NPC_ROLE_RAIDER &&
        npc_dynamic_modules[NPC_DYNAMIC_TOOL_SHAFT].ready &&
        npc_dynamic_modules[NPC_DYNAMIC_TOOL_HEAD].ready) {
        Vector3 tool_root = FromLimbVector(
            skin->bones[CC_HUMANOID_SKIN_HAND_LEFT].tail);
        Vector3 tool_axis = PhysicsNormalizeOr(
            PhysicsAdd(body_up,
                       PhysicsAdd(PhysicsScale(
                                      body_right,
                                      secondary_wave * 0.12f *
                                          movement_weight),
                                  PhysicsScale(body_forward, 0.12f))),
            body_up);
        Vector3 tool_right = PhysicsNormalizeOr(
            PhysicsCross(tool_axis, body_forward), body_right);
        Vector3 tool_forward = PhysicsNormalizeOr(
            PhysicsCross(tool_right, tool_axis), body_forward);
        const float tool_length = 0.68f;
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_TOOL_SHAFT,
            NpcModuleTransform(tool_root, tool_right, tool_axis,
                               tool_forward,
                               (Vector3){0.042f, tool_length, 0.042f}),
            appearance->leather);
        Vector3 tool_tip = PhysicsAdd(
            tool_root, PhysicsScale(tool_axis, tool_length));
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_TOOL_HEAD,
            NpcModuleTransform(tool_tip, tool_right, tool_axis,
                               tool_forward,
                               (Vector3){0.28f, 0.14f, 0.15f}),
            appearance->metal);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) {
        NpcDynamicModuleId headwear = NpcHeadwearModule(
            appearance->headwear_style);
        Color headwear_color = appearance->headwear_style == 0U ?
            appearance->metal : appearance->outer;
        Matrix headwear_transform = NpcModuleTransform(
            head, head_right, head_up, head_forward,
            (Vector3){0.26f * appearance->head_width, 0.30f,
                      0.25f * appearance->head_depth});
        (void)DrawNpcDynamicModule(
            headwear, headwear_transform, headwear_color);
    }

    CcNpcPortraitExpression expression =
        appearance->role == CC_NPC_ROLE_GUARD ||
        appearance->role == CC_NPC_ROLE_RAIDER ? CC_NPC_PORTRAIT_FOCUSED :
                                                CC_NPC_PORTRAIT_NEUTRAL;
    if (agent->combat.hit_flash_seconds > 0.0f) {
        expression = CC_NPC_PORTRAIT_HURT;
    } else if (agent->combat.focus_valid ||
               agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
               agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        expression = CC_NPC_PORTRAIT_FOCUSED;
    }

    CcFaceRecipe face = CcNpcFaceRecipe(appearance);
    float face_half_width = featured_hero ?
        0.138f * appearance->head_width :
        0.105f * appearance->head_width;
    float face_half_height = featured_hero ? 0.145f : 0.158f;
    float face_half_depth = featured_hero ?
        0.142f * appearance->head_depth :
        0.125f * appearance->head_depth;
    DrawWorldFace(PhysicsAdd(head, Vector3Scale(head_up, 0.025f)),
                  head_right, head_up, head_forward,
                  face_half_width, face_half_height, face_half_depth,
                  &face, expression, featured_hero);
    return drew;
}

static CcNpcAppearance ProceduralHeroAppearance(const CcLocalAgent *agent)
{
    return agent != NULL ? agent->appearance : CcNpcCrownlessAppearance();
}

static void DrawGroundBrushStroke(Vector3 center, Vector3 along,
                                  float length, float width, Color color);

static void DrawBiomechanicalBiped(const CcLocalAgent *agent)
{
    const CcHumanoidGait *gait = &agent->humanoid;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(pose, &skin);
    if (!skin.valid) return;
    bool modular_hero = agent->crowned;
    CcNpcAppearance procedural_hero = ProceduralHeroAppearance(agent);
    if (modular_hero && screen_first_hero_active) {
        UseCharacterLighting();
        bool procedural_hero_updated = DrawDynamicNpcModules(
            agent, &skin, &procedural_hero);
        if (procedural_hero_updated) {
            DrawWayfarerHeroDetails(&skin);
        }
        RestoreWorldLighting();
        if (procedural_hero_updated) {
            CcLocalRendererRecordBiped(true);
            return;
        }
    }
    bool hero_skin_updated = modular_hero &&
        DrawHeroSkin(&skin, &agent->render_cape, WHITE, true, true);
    if (hero_skin_updated) {
        CcLocalRendererRecordBiped(true);
        if (draw_hero_rig_debug) {
            DrawHeroSkinRigOverlay(gait, &skin, &agent->render_cape);
        }
        UseCharacterLighting();
        DrawWayfarerHeroDetails(&skin);
        RestoreWorldLighting();

        return;
    } else {
        CcLocalRendererRecordBiped(false);
    }
    UseCharacterLighting();
    if (!agent->crowned &&
        DrawDynamicNpcModules(agent, &skin, &agent->appearance)) {
        RestoreWorldLighting();
        return;
    }
    Vector3 pelvis = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_PELVIS].head);
    Vector3 spine = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_SPINE].head);
    Vector3 chest = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_CHEST].head);
    Vector3 neck = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_NECK].head);
    Vector3 head = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HEAD].position);
    float fallen_weight = fmaxf(0.0f, fminf(agent->ragdoll_visual_blend, 1.0f));
    float upright_weight = 1.0f - fallen_weight;
    Vector3 shoulder_left = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_SHOULDER_LEFT].position);
    Vector3 shoulder_right = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_SHOULDER_RIGHT].position);
    Vector3 hip_left = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HIP_LEFT].position);
    Vector3 hip_right = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HIP_RIGHT].position);
    float upper_yaw = PoseAxisYaw(shoulder_left, shoulder_right,
                                  agent->facing_yaw + pose->chest_yaw);
    float pelvis_yaw = PoseAxisYaw(hip_left, hip_right,
                                   agent->facing_yaw + pose->pelvis_yaw);
    Vector3 body_up = FromLimbVector(skin.body_up);
    Vector3 cape_center = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(body_up, -0.24f));
    CcNpcAppearance gameplay_appearance = agent->crowned ?
        procedural_hero : agent->appearance;
    const CcNpcAppearance *appearance = &gameplay_appearance;
    float movement_weight = SmoothStep01(fabsf(gait->speed.value) / 0.90f);
    float movement_wave = sinf(gait->phase * 2.0f * PI);
    float signature_lean = appearance->idle_lean *
        (1.0f - movement_weight * 0.35f);
    float signature_sway = cosf(gait->phase * 2.0f * PI) * 0.018f *
        (1.0f - movement_weight) * appearance->bob_scale;
    float signature_bob = fabsf(movement_wave) * 0.014f * movement_weight *
        (appearance->bob_scale - 0.70f);
    Vector3 signature_forward = {sinf(upper_yaw), 0.0f, cosf(upper_yaw)};
    Vector3 signature_right = {cosf(upper_yaw), 0.0f, -sinf(upper_yaw)};
    Vector3 posture_offset = PhysicsAdd(
        PhysicsScale(signature_forward, signature_lean),
        PhysicsScale(signature_right, signature_sway));
    posture_offset.y += signature_bob;
    chest = PhysicsAdd(chest, posture_offset);
    shoulder_left = PhysicsAdd(shoulder_left, posture_offset);
    shoulder_right = PhysicsAdd(shoulder_right, posture_offset);
    neck = PhysicsAdd(neck, PhysicsScale(posture_offset, 1.18f));
    head = PhysicsAdd(head, PhysicsScale(posture_offset, 1.32f));
    cape_center = PhysicsAdd(cape_center, posture_offset);
    Color tunic = appearance->outer;
    if (CombatIsDefeated(&agent->combat)) tunic = (Color){61, 57, 62, 255};
    else if (agent->combat.hit_flash_seconds > 0.0f) tunic = WORLD_INK;
    Color trousers = CombatIsDefeated(&agent->combat) ?
                      (Color){50, 49, 51, 255} : appearance->trousers;
    Color leather = appearance->leather;
    float mass = appearance->body_mass;
    float muscle = appearance->muscularity;

    if (fallen_weight > 0.01f) {
        Vector3 fallen_back = PhysicsScale(
            (Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)}, -0.12f);
        DrawCylinderEx(PhysicsAdd(chest, fallen_back),
                       PhysicsAdd(pelvis, fallen_back),
                       0.17f, 0.25f, 5,
                       Fade((Color){73, 55, 91, 255}, fallen_weight));
        DrawCharacterSphere(pelvis, 0.18f * mass,
                            Fade(trousers, fallen_weight));
        DrawCylinderEx(pelvis, spine,
                       0.18f, 0.22f, 10,
                       Fade(appearance->underlayer, fallen_weight));
        DrawCylinderEx(spine, chest,
                       0.23f, 0.27f, 10,
                       Fade(tunic, fallen_weight));
        DrawCylinderEx(chest, neck, 0.18f, 0.09f, 9,
                       Fade(tunic, fallen_weight));
    }
    if (upright_weight > 0.01f) {
        if ((appearance->equipment & CC_NPC_EQUIPMENT_MANTLE) != 0U) {
            Color mantle = agent->crowned ? (Color){91, 46, 54, 255} :
                ShadeColor(appearance->outer, 0.68f);
            DrawOrientedBox(cape_center, (Vector3){0.0f, 0.04f, 0.0f},
                            (Vector3){0.46f * mass, 0.62f, 0.045f}, upper_yaw,
                            Fade(mantle, upright_weight));
        }
        DrawOrientedBox(pelvis, (Vector3){0.0f, 0.02f, 0.0f},
                        (Vector3){0.40f * mass, 0.18f, 0.27f * mass},
                        pelvis_yaw,
                        Fade(trousers, upright_weight));
        DrawCylinderEx(spine, chest,
                       0.27f * mass, 0.31f * appearance->shoulder_scale,
                       10,
                       Fade(tunic, upright_weight));
        DrawNpcGarmentCut(appearance, pelvis, chest, shoulder_left,
                          upper_yaw, 1.0f);
        if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
            Vector3 plate = FromLimbVector(
                skin.sockets[CC_HUMANOID_SOCKET_CHEST_FRONT].position);
            DrawOrientedBox(plate, (Vector3){0.0f, -0.04f, 0.0f},
                            (Vector3){0.34f * mass, 0.28f, 0.045f}, upper_yaw,
                            Fade(appearance->metal, upright_weight));
        }
    }

    const CcHumanoidSkinBone thigh_bones[] = {
        CC_HUMANOID_SKIN_THIGH_LEFT,
        CC_HUMANOID_SKIN_THIGH_RIGHT,
    };
    const CcHumanoidSkinBone shin_bones[] = {
        CC_HUMANOID_SKIN_SHIN_LEFT,
        CC_HUMANOID_SKIN_SHIN_RIGHT,
    };
    const CcHumanoidSkinBone foot_bones[] = {
        CC_HUMANOID_SKIN_FOOT_LEFT,
        CC_HUMANOID_SKIN_FOOT_RIGHT,
    };
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Vector3 hip = FromLimbVector(skin.bones[thigh_bones[leg]].head);
        Vector3 knee = FromLimbVector(skin.bones[thigh_bones[leg]].tail);
        Vector3 ankle = FromLimbVector(skin.bones[shin_bones[leg]].tail);
        Vector3 heel = FromLimbVector(pose->heel[leg]);
        Vector3 toe = FromLimbVector(skin.bones[foot_bones[leg]].tail);
        float thigh_radius = (0.084f + muscle * 0.018f) * mass;
        DrawCylinderEx(hip, knee, thigh_radius, thigh_radius * 0.80f, 9,
                       trousers);
        DrawCylinderEx(knee, ankle, thigh_radius * 0.78f,
                       thigh_radius * 0.58f, 9, ShadeColor(trousers, 0.84f));
        DrawPitchedFoot(heel, toe, agent->facing_yaw, leather);
    }

    const CcHumanoidSkinBone upper_arm_bones[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    const CcHumanoidSkinBone forearm_bones[] = {
        CC_HUMANOID_SKIN_FOREARM_LEFT,
        CC_HUMANOID_SKIN_FOREARM_RIGHT,
    };
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        Vector3 shoulder = FromLimbVector(
            skin.bones[upper_arm_bones[arm]].head);
        Vector3 elbow = FromLimbVector(
            skin.bones[upper_arm_bones[arm]].tail);
        Vector3 hand = FromLimbVector(
            skin.bones[forearm_bones[arm]].tail);
        float arm_direction = arm == 0 ? -1.0f : 1.0f;
        float signature_swing = movement_wave * 0.045f * movement_weight *
            appearance->arm_swing_scale * arm_direction;
        Vector3 arm_offset = PhysicsAdd(
            posture_offset, PhysicsScale(signature_forward,
                                          signature_swing));
        shoulder = PhysicsAdd(shoulder, posture_offset);
        elbow = PhysicsAdd(elbow, PhysicsScale(arm_offset, 0.72f));
        hand = PhysicsAdd(hand, arm_offset);
        DrawCylinderEx(shoulder, elbow, 0.074f * mass, 0.061f * mass, 8,
                       tunic);
        DrawCylinderEx(elbow, hand, 0.061f * mass, 0.050f * mass, 8,
                       appearance->underlayer);
        if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
            DrawCharacterSphere(shoulder, 0.092f * mass,
                                appearance->metal);
        }
        DrawSmallSphere(hand, 0.062f, appearance->skin);
    }

    Vector3 head_up = PhysicsNormalizeOr(PhysicsSubtract(head, neck),
                                         (Vector3){0.0f, 1.0f, 0.0f});
    Vector3 fallback_right = {cosf(upper_yaw), 0.0f, -sinf(upper_yaw)};
    Vector3 shoulder_axis = PhysicsSubtract(shoulder_right, shoulder_left);
    Vector3 head_right = PhysicsSubtract(
        shoulder_axis, PhysicsScale(head_up,
                                    PhysicsDot(shoulder_axis, head_up)));
    head_right = PhysicsNormalizeOr(head_right, fallback_right);
    Vector3 head_forward = PhysicsNormalizeOr(PhysicsCross(head_right, head_up),
        (Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)});
    (void)head_forward;
    (void)head_right;
    DrawNpcHead(appearance, head, upper_yaw, 1.0f);
    CcNpcAppearance accessories = *appearance;
    accessories.equipment &= ~(uint32_t)(CC_NPC_EQUIPMENT_ARMOR |
                                         CC_NPC_EQUIPMENT_MANTLE);
    DrawNpcEquipment(&accessories, agent->position, pelvis, chest,
                     shoulder_left, shoulder_right,
                     FromLimbVector(skin.bones[CC_HUMANOID_SKIN_FOREARM_LEFT].tail),
                     upper_yaw, 1.0f);
    if (draw_hero_rig_debug) {
        Vector3 rig_offset = LocalPoint((Vector3){0}, 0.022f, 0.010f, 0.040f,
                                        upper_yaw);
        Vector3 physical_root = {gait->body.root.position.x,
                                 gait->body.root.position.y,
                                 gait->body.root.position.z};
        Vector3 reaction_end = physical_root;
        if (gait->body.total_mass > 0.0f) {
            reaction_end.x += gait->ground_reaction.x /
                              gait->body.total_mass * 0.012f;
            reaction_end.y += gait->ground_reaction.y /
                              gait->body.total_mass * 0.012f;
            reaction_end.z += gait->ground_reaction.z /
                              gait->body.total_mass * 0.012f;
        }
        if (upright_weight > 0.01f) {
            DrawSmallSphere(Add3(physical_root, rig_offset), 0.052f,
                            Fade(WORLD_TEAL, upright_weight));
            DrawCylinderEx(Add3(physical_root, rig_offset),
                           Add3(reaction_end, rig_offset), 0.014f, 0.008f, 6,
                           Fade(WORLD_TEAL, 0.82f * upright_weight));
        }
        Vector3 rig_pelvis = Add3(pelvis, rig_offset);
        Vector3 rig_spine = Add3(spine, rig_offset);
        Vector3 rig_chest = Add3(chest, rig_offset);
        Vector3 rig_neck = Add3(neck, rig_offset);
        DrawBoneSegment(rig_pelvis, rig_spine, WORLD_GOLD);
        DrawBoneSegment(rig_spine, rig_chest, WORLD_GOLD);
        DrawBoneSegment(rig_chest, rig_neck, WORLD_GOLD);
        DrawBoneJoint(rig_pelvis, WORLD_GOLD);
        DrawBoneJoint(rig_spine, WORLD_GOLD);
        DrawBoneJoint(rig_chest, WORLD_GOLD);
        DrawBoneJoint(rig_neck, WORLD_GOLD);
        for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
            Color bone = HumanoidContactColor(gait->feet[leg].contact);
            Vector3 hip = Add3(FromLimbVector(
                skin.bones[thigh_bones[leg]].head), rig_offset);
            Vector3 knee = Add3(FromLimbVector(
                skin.bones[thigh_bones[leg]].tail), rig_offset);
            Vector3 ankle = Add3(FromLimbVector(
                skin.bones[shin_bones[leg]].tail), rig_offset);
            Vector3 heel = Add3(FromLimbVector(pose->heel[leg]), rig_offset);
            Vector3 ball = Add3(FromLimbVector(pose->ball[leg]), rig_offset);
            Vector3 toe = Add3(FromLimbVector(pose->toe[leg]), rig_offset);
            DrawBoneSegment(hip, knee, bone);
            DrawBoneSegment(knee, ankle, bone);
            DrawBoneSegment(ankle, heel, bone);
            DrawBoneSegment(heel, ball, bone);
            DrawBoneSegment(ball, toe, bone);
            DrawBoneJoint(hip, bone);
            DrawBoneJoint(knee, bone);
            DrawBoneJoint(ankle, bone);
            DrawBoneJoint(heel, bone);
            DrawBoneJoint(ball, bone);
            DrawBoneJoint(toe, bone);
        }
        for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
            Vector3 shoulder = Add3(FromLimbVector(
                skin.bones[upper_arm_bones[arm]].head), rig_offset);
            Vector3 elbow = Add3(FromLimbVector(
                skin.bones[upper_arm_bones[arm]].tail), rig_offset);
            Vector3 hand = Add3(FromLimbVector(
                skin.bones[forearm_bones[arm]].tail), rig_offset);
            DrawBoneSegment(shoulder, elbow, WORLD_GOLD);
            DrawBoneSegment(elbow, hand, WORLD_GOLD);
            DrawBoneJoint(shoulder, WORLD_GOLD);
            DrawBoneJoint(elbow, WORLD_GOLD);
            DrawBoneJoint(hand, WORLD_GOLD);
        }
    }
    RestoreWorldLighting();
}

static Vector3 SolveVisualTwoBone(Vector3 root, Vector3 target,
                                  Vector3 bend_direction,
                                  float upper_length, float lower_length)
{
    Vector3 delta = PhysicsSubtract(target, root);
    float raw_distance = PhysicsLength(delta);
    Vector3 direction = raw_distance > 0.0001f ?
                        PhysicsScale(delta, 1.0f / raw_distance) :
                        (Vector3){0.0f, -1.0f, 0.0f};
    float distance = fmaxf(fabsf(upper_length - lower_length) + 0.0001f,
                           fminf(raw_distance,
                                 upper_length + lower_length - 0.0001f));
    Vector3 pole = PhysicsSubtract(
        bend_direction, PhysicsScale(direction,
                                     PhysicsDot(bend_direction, direction)));
    float pole_length = PhysicsLength(pole);
    if (pole_length <= 0.0001f) pole = (Vector3){1.0f, 0.0f, 0.0f};
    else pole = PhysicsScale(pole, 1.0f / pole_length);
    float along = (upper_length * upper_length - lower_length * lower_length +
                   distance * distance) / (2.0f * distance);
    float height = sqrtf(fmaxf(0.0f,
                               upper_length * upper_length - along * along));
    return PhysicsAdd(PhysicsAdd(root, PhysicsScale(direction, along)),
                      PhysicsScale(pole, height));
}

static float SnapContactCoordinate(float value)
{
    const float contact_grid = 0.125f;
    return roundf(value / contact_grid) * contact_grid;
}

static void DrawCharacterContactEffects(const CcLocalAgent *agent)
{
    if (agent == NULL) return;
    bool fallen = agent->humanoid.ragdoll.active ||
                  agent->ragdoll_visual_blend > 0.42f;
    float surface = SurfaceHeightAt(agent->scene, agent->position.x,
                                    agent->position.z);
    Vector3 shadow = {
        SnapContactCoordinate(agent->position.x),
        fmaxf(0.006f, surface + 0.008f),
        SnapContactCoordinate(agent->position.z),
    };
    float vertical_gap = fmaxf(0.0f, agent->position.y - surface);
    float lift = fminf(vertical_gap / 1.60f, 1.0f);
    float shadow_scale = fallen ? 1.0f : 1.0f - lift * 0.38f;
    float shadow_visibility = 1.0f - lift * 0.72f;
    Vector2 shadow_size = fallen ? (Vector2){1.20f, 0.56f} :
                                   agent->crowned ?
                                       (Vector2){0.82f, 0.52f} :
                                       (Vector2){0.70f, 0.46f};
    Color shadow_color = agent->crowned && !fallen ?
        (Color){15, 38, 38, 255} : (Color){3, 8, 10, 255};
    float base_opacity = fallen ? 102.0f / 255.0f :
                         agent->crowned ? 108.0f / 255.0f :
                                          82.0f / 255.0f;

    if (surface <= agent->position.y + 0.12f) {
        DrawContactShadow(shadow, shadow_size.x * shadow_scale,
                          shadow_size.y * shadow_scale, agent->facing_yaw,
                          Fade(shadow_color,
                               base_opacity * shadow_visibility));
    }

    if (agent->swimming || agent->immersion > 0.08f) {
        float phase = agent->humanoid.swim_phase -
                      floorf(agent->humanoid.swim_phase);
        float pulse = 0.34f + phase * 0.30f;
        Vector3 water = {shadow.x, COURSE_WATER_SURFACE + 0.022f, shadow.z};
        DrawCylinderWires(water, pulse, pulse, 0.018f, 16,
                          Fade((Color){126, 220, 222, 255},
                               0.62f * (1.0f - phase)));
        DrawCylinderWires((Vector3){water.x, water.y + 0.006f, water.z},
                          pulse * 0.62f, pulse * 0.62f, 0.018f, 12,
                          Fade(WORLD_TEAL, 0.48f));
        return;
    }

    bool stepping = agent->morphology == CC_MORPHOLOGY_BIPED &&
                    agent->grounded && !agent->climbing &&
                    !agent->humanoid.ragdoll.active &&
                    agent->humanoid.speed.value > 0.16f &&
                    agent->stepped_pose.initialized;
    if (!stepping) return;
    int32_t bin = agent->stepped_pose.locomotion_bin;
    if (bin != 0 && bin != 4) return;
    int32_t leg = bin == 0 ? 0 : 1;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    Vector3 heel = FromLimbVector(pose->heel[leg]);
    Vector3 toe = FromLimbVector(pose->toe[leg]);
    heel.x = SnapContactCoordinate(heel.x);
    heel.z = SnapContactCoordinate(heel.z);
    toe.x = SnapContactCoordinate(toe.x);
    toe.z = SnapContactCoordinate(toe.z);
    heel.y = fmaxf(0.016f, agent->position.y + 0.014f);
    toe.y = heel.y;
    Color print = (Color){67, 59, 48, 116};
    DrawLine3D(heel, toe, print);
    DrawBox(heel, (Vector3){0.16f, 0.014f, 0.11f}, print);

    float within = agent->humanoid.phase * 8.0f;
    within -= floorf(within);
    float dust = 1.0f - SmoothStep01(within / 0.72f);
    Vector3 back = {-sinf(agent->facing_yaw), 0.0f,
                    -cosf(agent->facing_yaw)};
    Vector3 dust_smear = PhysicsAdd(heel, PhysicsScale(back, 0.08f));
    dust_smear.y = shadow.y + 0.006f;
    DrawGroundBrushStroke(dust_smear, back, 0.32f * dust,
                          0.12f * dust,
                          Fade((Color){150, 125, 86, 255}, dust * 0.42f));
}

static void DrawRobotShell(const CcLocalAgent *agent)
{
    DrawCharacterContactEffects(agent);
    Vector3 body = agent->multileg_ragdoll.active ?
        FromLimbVector(agent->multileg_ragdoll.body_center) :
        ShellPodCenter(agent);
    const CcLimbRig *rig = &agent->limb_rig;
    bool biped = rig->morphology.preset == CC_MORPHOLOGY_BIPED;
    if (biped && agent->humanoid.initialized) {
        DrawBiomechanicalBiped(agent);
        return;
    }
    if (!biped) {
        float body_length = rig->morphology.limb_count >= 6 ? 0.72f : 0.54f;
        DrawOrientedBox(body, (Vector3){0.0f, 0.0f, 0.0f},
                        (Vector3){0.54f, 0.38f, body_length}, agent->facing_yaw,
                        (Color){42, 128, 136, 255});
        DrawSmallSphere(body, 0.085f, WORLD_GOLD);
        DrawOrientedBox(body, (Vector3){0.0f, 0.02f, 0.22f},
                        (Vector3){0.24f, 0.18f, 0.28f}, agent->facing_yaw,
                        (Color){225, 177, 68, 255});
        DrawSmallSphere(
            LocalPoint(body, -0.085f, 0.06f, 0.34f, agent->facing_yaw),
            0.040f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.085f, 0.06f, 0.34f, agent->facing_yaw),
            0.040f, WORLD_VOID);
    }
    Vector3 rig_offset = LocalPoint((Vector3){0}, 0.022f, 0.010f, 0.040f,
                                    agent->facing_yaw);
    for (int32_t limb_index = 0; limb_index < rig->morphology.limb_count;
         ++limb_index) {
        const CcLimbRuntime *limb = &rig->limbs[limb_index];
        const CcLimbSpec *spec = &rig->morphology.limbs[limb_index];
        Color bone = limb->state == CC_LIMB_SWING ? WORLD_VIOLET :
                     limb->state == CC_LIMB_DISABLED ? WORLD_DANGER : WORLD_GOLD;
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            Vector3 a = FromLimbVector(limb->joints[segment]);
            Vector3 b = FromLimbVector(limb->joints[segment + 1]);
            float skin = segment == 0 ? 0.072f : 0.058f;
            DrawCylinderEx(a, b, skin, skin * 0.82f, 8,
                           limb->state == CC_LIMB_DISABLED ?
                           (Color){67, 48, 51, 255} : (Color){54, 66, 71, 255});
            Vector3 rig_a = Add3(a, rig_offset);
            Vector3 rig_b = Add3(b, rig_offset);
            DrawCylinderEx(rig_a, rig_b, 0.022f, 0.022f, 7, bone);
            DrawSmallSphere(rig_a, 0.046f, bone);
        }
        Vector3 foot = FromLimbVector(limb->joints[spec->segment_count]);
        DrawSmallSphere(foot, 0.076f, bone);
        Color foot_color = limb->state == CC_LIMB_SWING ?
                           Fade(WORLD_VIOLET, 0.72f) :
                           limb->state == CC_LIMB_DISABLED ?
                           Fade(WORLD_DANGER, 0.72f) : (Color){37, 62, 67, 255};
        if (biped) {
            if (agent->traversal == CC_TRAVERSAL_CLIMB &&
                fabsf(limb->contact_normal.y) < 0.5f) {
                Vector3 size = fabsf(limb->contact_normal.x) > 0.5f ?
                               (Vector3){0.09f, 0.28f, 0.17f} :
                               (Vector3){0.17f, 0.28f, 0.09f};
                Vector3 center = PhysicsAdd(foot,
                                            PhysicsScale(FromLimbVector(
                                                limb->contact_normal), 0.035f));
                DrawBox(center, size, foot_color);
            } else {
                DrawOrientedBox(foot, (Vector3){0.0f, 0.025f, 0.075f},
                                (Vector3){0.17f, 0.09f, 0.30f}, agent->facing_yaw,
                                foot_color);
            }
        } else {
            DrawCylinder((Vector3){foot.x, foot.y - 0.02f, foot.z},
                         0.11f, 0.09f, 0.04f, 8, foot_color);
        }
    }
    if (biped) {
        float arm_swing = 0.0f;
        float upper_yaw = agent->facing_yaw;
        float climb_lean = agent->traversal == CC_TRAVERSAL_CLIMB ?
                           sinf(agent->climb_progress * PI) * 0.11f : 0.0f;
        float hero_lean = climb_lean;
        Vector3 spine_base = LocalPoint(body, 0.0f, 0.03f, 0.010f,
                                        upper_yaw);
        Vector3 chest = LocalPoint(body, 0.0f, 0.41f, 0.020f + hero_lean,
                                   upper_yaw);
        Vector3 neck = LocalPoint(body, 0.0f, 0.66f,
                                  0.020f + hero_lean * 1.20f, upper_yaw);
        Vector3 shoulder_l = LocalPoint(body, -0.285f, 0.50f,
                                        hero_lean * 0.92f, upper_yaw);
        Vector3 shoulder_r = LocalPoint(body, 0.285f, 0.50f,
                                        hero_lean * 0.92f, upper_yaw);
        Vector3 elbow_l = LocalPoint(body, -0.34f, 0.27f,
                                     -arm_swing * 0.68f, upper_yaw);
        Vector3 elbow_r = LocalPoint(body, 0.34f, 0.27f,
                                     arm_swing * 0.68f, upper_yaw);
        Vector3 hand_l = LocalPoint(body, -0.30f, 0.04f,
                                    -arm_swing * 1.35f, upper_yaw);
        Vector3 hand_r = LocalPoint(body, 0.30f, 0.04f,
                                    arm_swing * 1.35f, upper_yaw);
        if (agent->traversal == CC_TRAVERSAL_CLIMB) {
            float reach = SmoothStep01(agent->climb_progress / 0.18f);
            float release = SmoothStep01((agent->climb_progress - 0.76f) / 0.24f);
            Vector3 resting_left = hand_l;
            Vector3 resting_right = hand_r;
            Vector3 held_left = PhysicsLerp(resting_left,
                                            agent->climb_hand_left, reach);
            Vector3 held_right = PhysicsLerp(resting_right,
                                             agent->climb_hand_right, reach);
            hand_l = PhysicsLerp(held_left, resting_left, release);
            hand_r = PhysicsLerp(held_right, resting_right, release);
            Vector3 bend_left = FramePoint((Vector3){0}, -1.0f, 0.15f, -0.18f,
                                           upper_yaw);
            Vector3 bend_right = FramePoint((Vector3){0}, 1.0f, 0.15f, -0.18f,
                                            upper_yaw);
            elbow_l = SolveVisualTwoBone(shoulder_l, hand_l, bend_left,
                                         0.36f, 0.38f);
            elbow_r = SolveVisualTwoBone(shoulder_r, hand_r, bend_right,
                                         0.36f, 0.38f);
        }

        DrawOrientedBox(body, (Vector3){0.0f, 0.36f, -0.155f},
                        (Vector3){0.46f, 0.56f, 0.045f}, upper_yaw,
                        (Color){73, 55, 91, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.03f, 0.0f},
                        (Vector3){0.40f, 0.18f, 0.27f}, upper_yaw,
                        (Color){44, 61, 65, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.36f, hero_lean * 0.55f},
                        (Vector3){0.50f, 0.50f, 0.29f}, upper_yaw,
                        (Color){42, 128, 136, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.39f, 0.165f + hero_lean * 0.55f},
                        (Vector3){0.34f, 0.29f, 0.045f}, upper_yaw,
                        (Color){223, 173, 67, 255});
        DrawCylinderEx(shoulder_l, elbow_l, 0.065f, 0.055f, 8,
                       (Color){42, 128, 136, 255});
        DrawCylinderEx(elbow_l, hand_l, 0.055f, 0.045f, 8,
                       (Color){54, 66, 71, 255});
        DrawCylinderEx(shoulder_r, elbow_r, 0.065f, 0.055f, 8,
                       (Color){42, 128, 136, 255});
        DrawCylinderEx(elbow_r, hand_r, 0.055f, 0.045f, 8,
                       (Color){54, 66, 71, 255});
        DrawCharacterSphere(shoulder_l, 0.095f,
                            (Color){223, 173, 67, 255});
        DrawCharacterSphere(shoulder_r, 0.095f,
                            (Color){223, 173, 67, 255});
        DrawSmallSphere(hand_l, 0.055f, (Color){221, 174, 118, 255});
        DrawSmallSphere(hand_r, 0.055f, (Color){221, 174, 118, 255});

        Vector3 visual_spine_base = Add3(spine_base, rig_offset);
        Vector3 visual_chest = Add3(chest, rig_offset);
        Vector3 visual_neck = Add3(neck, rig_offset);
        Vector3 visual_shoulder_l = Add3(shoulder_l, rig_offset);
        Vector3 visual_shoulder_r = Add3(shoulder_r, rig_offset);
        Vector3 visual_elbow_l = Add3(elbow_l, rig_offset);
        Vector3 visual_elbow_r = Add3(elbow_r, rig_offset);
        Vector3 visual_hand_l = Add3(hand_l, rig_offset);
        Vector3 visual_hand_r = Add3(hand_r, rig_offset);
        DrawBoneSegment(visual_spine_base, visual_chest, WORLD_GOLD);
        DrawBoneSegment(visual_chest, visual_neck, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_l, visual_shoulder_r, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_l, visual_elbow_l, WORLD_GOLD);
        DrawBoneSegment(visual_elbow_l, visual_hand_l, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_r, visual_elbow_r, WORLD_GOLD);
        DrawBoneSegment(visual_elbow_r, visual_hand_r, WORLD_GOLD);
        Vector3 visual_joints[] = {
            visual_spine_base, visual_chest,      visual_neck,
            visual_shoulder_l, visual_shoulder_r, visual_elbow_l,
            visual_elbow_r,    visual_hand_l,     visual_hand_r,
        };
        for (size_t joint_index = 0;
             joint_index < sizeof(visual_joints) / sizeof(visual_joints[0]);
             joint_index++) {
            DrawBoneJoint(visual_joints[joint_index], WORLD_GOLD);
        }

        Vector3 head = LocalPoint(body, 0.0f, 0.81f, hero_lean * 1.25f,
                                  upper_yaw);
        DrawCharacterSphere(head, 0.18f, (Color){221, 174, 118, 255});
        DrawCharacterSphere(
            LocalPoint(body, 0.0f, 0.91f,
                       -0.025f + hero_lean * 1.25f, upper_yaw),
            0.145f, (Color){52, 46, 51, 255});
        DrawSmallSphere(
            LocalPoint(body, -0.058f, 0.82f,
                       0.165f + hero_lean * 1.25f, upper_yaw),
            0.022f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.058f, 0.82f,
                       0.165f + hero_lean * 1.25f, upper_yaw),
            0.022f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.0f, 1.11f, hero_lean * 1.20f, upper_yaw),
            0.060f, WORLD_GOLD);
    } else {
        DrawSmallSphere(
            LocalPoint(body, 0.0f, 0.52f, 0.0f, agent->facing_yaw),
            0.065f, WORLD_GOLD);
    }
}

static void PrepareHeroDisplayAt(const CcLocalAgent *source,
                                 Vector3 position,
                                 CcLocalSceneKind scene,
                                 float facing_yaw,
                                 CcLocalAgent *display)
{
    CcLocalAgentInit(display, (Vector2){position.x, position.z}, false);
    display->position = position;
    display->scene = scene;
    display->facing_yaw = facing_yaw;
    display->appearance = source->appearance;
    display->tunic_color = source->tunic_color;
    display->crowned = source->crowned;
    display->athletics = source->athletics;
    CcLocalAgentSetMorphology(display, source->morphology, false);
}

int32_t CcLocalCargoBoxCountInternal(const CcPlayerCompany *player)
{
    if (player == NULL) return 0;
    int32_t count = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (player->cargo[good] > 0) count += player->cargo[good];
        if (count >= 24) return 24;
    }
    return count;
}

static Color CargoBoxColor(CcGood good)
{
    switch (good) {
        case CC_GOOD_FOOD: return (Color){177, 116, 55, 255};
        case CC_GOOD_IRON: return WORLD_METAL_SHADOW;
        case CC_GOOD_TOOLS: return WORLD_TEAL;
        case CC_GOOD_WEAPONS: return WORLD_DANGER;
        case CC_GOOD_GOLD: return WORLD_GOLD;
        case CC_GOOD_GEMS: return WORLD_VIOLET;
        case CC_GOOD_COUNT: break;
    }
    return WORLD_WOOD;
}

static Color CargoBoxBandColor(CcGood good)
{
    if (good == CC_GOOD_FOOD) return WORLD_CROP_LIGHT;
    if (good == CC_GOOD_IRON) return WORLD_METAL_LIGHT;
    return WORLD_INK;
}

static void DrawCarriedCargoBoxes(const CcPlayerCompany *player,
                                  Vector3 base, float yaw)
{
    int32_t limit = CcLocalCargoBoxCountInternal(player);
    if (limit <= 0) return;
    const Vector3 rack_center = {-0.30f, 2.21f, 0.0f};
    DrawOrientedBox(base, rack_center,
                    (Vector3){2.92f, 0.10f, 1.42f}, yaw,
                    WORLD_WOOD_LIGHT);
    DrawOrientedBox(base, (Vector3){rack_center.x, 2.42f, -0.73f},
                    (Vector3){2.98f, 0.42f, 0.07f}, yaw,
                    WORLD_WOOD_SHADOW);
    DrawOrientedBox(base, (Vector3){rack_center.x, 2.42f, 0.73f},
                    (Vector3){2.98f, 0.42f, 0.07f}, yaw,
                    WORLD_WOOD_SHADOW);
    DrawOrientedBox(base, (Vector3){-1.78f, 2.42f, 0.0f},
                    (Vector3){0.07f, 0.42f, 1.52f}, yaw,
                    WORLD_WOOD_SHADOW);
    DrawOrientedBox(base, (Vector3){1.18f, 2.42f, 0.0f},
                    (Vector3){0.07f, 0.42f, 1.52f}, yaw,
                    WORLD_WOOD_SHADOW);
    int32_t drawn = 0;
    for (int32_t good = 0;
         good < CC_GOOD_COUNT && drawn < limit; ++good) {
        int32_t quantity = player != NULL ? player->cargo[good] : 0;
        for (int32_t unit = 0; unit < quantity && drawn < limit;
             ++unit, ++drawn) {
            int32_t layer = drawn / 8;
            int32_t spot = drawn % 8;
            int32_t column = spot % 4;
            int32_t row = spot / 4;
            Vector3 center = {
                -1.26f + (float)column * 0.64f,
                2.49f + (float)layer * 0.49f,
                -0.34f + (float)row * 0.68f
            };
            Color box = CargoBoxColor((CcGood)good);
            Color band = CargoBoxBandColor((CcGood)good);
            DrawOrientedBox(base, center,
                            (Vector3){0.58f, 0.44f, 0.59f}, yaw, box);
            DrawOrientedBox(
                base,
                (Vector3){center.x, center.y + 0.23f, center.z},
                (Vector3){0.60f, 0.055f, 0.61f}, yaw, band);
            DrawOrientedBox(
                base,
                (Vector3){center.x, center.y, center.z - 0.302f},
                (Vector3){0.15f, 0.32f, 0.022f}, yaw,
                Fade(band, 0.92f));
        }
    }
}

static void DrawCarriage3D(const CcPlayerCompany *player, bool targeted,
                           float clock)
{
    float x = CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width * 0.5f;
    float z = CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height * 0.5f;
    rlPushMatrix();
    rlTranslatef(0.0f, TerrainFootprintHeight(CARRIAGE_FOOTPRINT), 0.0f);
    RuntimeAsset *carriage = &runtime_assets[RUNTIME_ASSET_CARRIAGE];
    if (carriage->ready) {
        Vector3 origin = {x, 0.0f, z};
        float pulse = targeted ? 0.5f + 0.5f * sinf(clock * 5.0f) : 0.0f;
        Color tint = targeted ?
            (Color){255, (unsigned char)(235 + 12 * pulse),
                    (unsigned char)(174 + 34 * pulse), 255} : WHITE;
        DrawModelEx(carriage->model, origin, (Vector3){0.0f, 1.0f, 0.0f},
                    CARRIAGE_ASSET_STREET_YAW_DEGREES,
                    (Vector3){CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE}, tint);
        DrawCarriedCargoBoxes(
            player, origin, CARRIAGE_ASSET_STREET_YAW_DEGREES * DEG2RAD);
        rlPopMatrix();
        return;
    }
    DrawBox((Vector3){x, 1.06f, z},
            (Vector3){CARRIAGE_FOOTPRINT.width, 1.62f,
                      CARRIAGE_FOOTPRINT.height},
            (Color){111, 65, 54, 255});
    DrawBox((Vector3){x, 1.94f, z},
            (Vector3){CARRIAGE_FOOTPRINT.width + 0.22f, 0.18f,
                      CARRIAGE_FOOTPRINT.height + 0.18f},
            (Color){143, 124, 86, 255});
    DrawCubeWiresV((Vector3){x, 1.94f, z},
                   (Vector3){CARRIAGE_FOOTPRINT.width + 0.24f, 0.20f,
                             CARRIAGE_FOOTPRINT.height + 0.20f},
                   Fade(WORLD_GOLD, 0.58f));
    DrawBox((Vector3){CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width + 0.03f,
                       1.20f, z}, (Vector3){0.04f, 0.70f, 1.20f},
            (Color){35, 102, 108, 255});
    Vector3 wheels[] = {
        {CARRIAGE_FOOTPRINT.x, 0.58f, CARRIAGE_FOOTPRINT.y + 1.15f},
        {CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width, 0.58f,
         CARRIAGE_FOOTPRINT.y + 1.15f},
        {CARRIAGE_FOOTPRINT.x, 0.58f,
         CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height - 1.15f},
        {CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width, 0.58f,
         CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height - 1.15f}
    };
    for (int32_t i = 0; i < 4; ++i) {
        DrawScenerySphere(wheels[i], 0.54f, (Color){38, 31, 31, 255});
        DrawSphereWires(wheels[i], 0.55f, 7, 7,
                        Fade(WORLD_GOLD, 0.62f));
    }
    DrawCarriedCargoBoxes(
        player, (Vector3){x, 0.0f, z},
        CARRIAGE_ASSET_STREET_YAW_DEGREES * DEG2RAD);
    rlPopMatrix();
}

static void DrawCarriageTargetMarker3D(float clock)
{
    float ground = TerrainFootprintHeight(CARRIAGE_FOOTPRINT);
    float pulse = 0.72f + 0.18f * sinf(clock * 5.0f);
    Color accent = Fade(WORLD_GOLD, pulse);
    Vector3 center = {
        CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width * 0.5f,
        ground + 1.32f,
        CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height * 0.5f
    };
    DrawCubeWiresV(center,
                   (Vector3){CARRIAGE_FOOTPRINT.width + 0.42f, 2.72f,
                             CARRIAGE_FOOTPRINT.height + 0.42f},
                   accent);
    Vector3 approach = {
        CC_LOCAL_CARRIAGE_APPROACH_X,
        CcLocalTerrainHeightAt(CC_LOCAL_CARRIAGE_APPROACH_X,
                               CC_LOCAL_CARRIAGE_APPROACH_Z) + 0.025f,
        CC_LOCAL_CARRIAGE_APPROACH_Z
    };
    DrawCylinder(approach, 0.40f, 0.40f, 0.035f, 24,
                 Fade(WORLD_TEAL, pulse));
    DrawGroundBrushStroke(
        approach,
        (Vector3){CC_LOCAL_CARRIAGE_X - approach.x, 0.0f,
                  CC_LOCAL_CARRIAGE_Z - approach.z},
        0.78f, 0.11f, accent);
}

static void DrawNotice3D(const CcSim *sim)
{
    float x = CC_LOCAL_NOTICE_X;
    float z = CC_LOCAL_NOTICE_Z;
    rlPushMatrix();
    rlTranslatef(0.0f, CcLocalTerrainHeightAt(x, z), 0.0f);
    DrawBox((Vector3){x - 0.38f, 0.72f, z}, (Vector3){0.10f, 1.44f, 0.10f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){x + 0.38f, 0.72f, z}, (Vector3){0.10f, 1.44f, 0.10f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){x, 1.32f, z}, (Vector3){1.08f, 0.82f, 0.12f},
            (Color){148, 94, 52, 255});
    int32_t count = CcSimActiveSituationCount(sim);
    for (int32_t i = 0; i < count && i < 4; ++i) {
        DrawBox((Vector3){x - 0.34f + (float)i * 0.22f,
                          1.34f + (float)(i & 1) * 0.12f, z - 0.07f},
                (Vector3){0.16f, 0.30f, 0.025f},
                i == 3 ? WORLD_DANGER : WORLD_INK);
    }
    rlPopMatrix();
}

static void DrawSiteRoadGate(float x, float z, Color accent)
{
    rlPushMatrix();
    rlTranslatef(0.0f, CcLocalTerrainHeightAt(x, z), 0.0f);
    DrawBox((Vector3){x - 1.15f, 1.35f, z},
            (Vector3){0.22f, 2.70f, 0.22f}, WORLD_WOOD_SHADOW);
    DrawBox((Vector3){x + 1.15f, 1.35f, z},
            (Vector3){0.22f, 2.70f, 0.22f}, WORLD_WOOD_SHADOW);
    DrawBox((Vector3){x, 2.38f, z},
            (Vector3){2.55f, 0.55f, 0.18f}, WORLD_WOOD);
    DrawBox((Vector3){x, 2.39f, z - 0.10f},
            (Vector3){1.72f, 0.22f, 0.05f}, accent);
    for (int32_t mark = 0; mark < 5; ++mark) {
        float road_x = x + 2.4f + (float)mark * 1.2f;
        float road_y = CcLocalTerrainHeightAt(road_x, z);
        DrawBox((Vector3){road_x, road_y + 0.025f, z},
                (Vector3){0.72f, 0.05f, 1.48f},
                Fade(WORLD_STONE, 0.74f));
    }
    rlPopMatrix();
}

typedef enum TreeRegion {
    TREE_REGION_VERDANT = 0,
    TREE_REGION_EMBER,
    TREE_REGION_BLUE
} TreeRegion;

typedef struct TreeRegionalStyle {
    TreeRegion region;
    Vector3 proportions;
    Color foliage_bias;
    float foliage_mix;
    float cluster_pull;
    float turn_steps;
    float shape_shift;
} TreeRegionalStyle;

static TreeRegionalStyle TreeStyleForKingdom(Color kingdom)
{
    if (kingdom.r > kingdom.g && kingdom.r > kingdom.b) {
        return (TreeRegionalStyle){
            TREE_REGION_EMBER, {1.14f, 0.94f, 1.10f},
            {112, 99, 58, 255}, 0.24f, 0.34f, 5.0f, 0.08f
        };
    }
    if (kingdom.b > kingdom.r && kingdom.b > kingdom.g) {
        return (TreeRegionalStyle){
            TREE_REGION_BLUE, {0.98f, 1.06f, 0.88f},
            {65, 89, 109, 255}, 0.22f, 0.22f, 6.0f, -0.04f
        };
    }
    return (TreeRegionalStyle){
        TREE_REGION_VERDANT, {0.90f, 1.14f, 0.94f},
        {43, 107, 88, 255}, 0.20f, 0.28f, 4.0f, 0.02f
    };
}

static TreeFamily RegionalTreeFamily(TreeFamily family, int32_t tree_index,
                                     TreeRegionalStyle style)
{
    if (style.region == TREE_REGION_EMBER &&
        family == TREE_FAMILY_ALDER && tree_index % 4 == 0) {
        return TREE_FAMILY_OAK;
    }
    if (style.region == TREE_REGION_BLUE &&
        family == TREE_FAMILY_ALDER && tree_index % 5 == 1) {
        return TREE_FAMILY_POLLARD;
    }
    return family;
}

typedef enum TreeRenderPass {
    TREE_RENDER_FULL = 0,
    TREE_RENDER_BATCH_BUILD
} TreeRenderPass;

typedef struct TreeBatchMeshWriter {
    Mesh mesh;
    int32_t vertex_cursor;
    int32_t index_cursor;
    int32_t source_draw_count;
    bool indexed;
    bool count_only;
    bool failed;
} TreeBatchMeshWriter;

typedef struct TreeSceneryBatchCache {
    Model structure;
    Model foliage;
    uint32_t seed;
    uint32_t visibility_mask;
    CcSettlementFunction function;
    Color kingdom;
    int32_t source_draw_count;
    int32_t vertex_count;
    bool ready;
} TreeSceneryBatchCache;

#define CC_TREE_BATCH_SOURCE_VERTEX_LIMIT 512

typedef struct TreeBatchSourceIndexCache {
    const float *vertices;
    const float *normals;
    const unsigned char *colors;
    int32_t vertex_count;
    int32_t unique_count;
    int32_t remap[CC_TREE_BATCH_SOURCE_VERTEX_LIMIT];
    bool ready;
} TreeBatchSourceIndexCache;

static TreeRenderPass tree_render_pass = TREE_RENDER_FULL;
static TreeBatchMeshWriter *tree_structure_writer = NULL;
static TreeBatchMeshWriter *tree_foliage_writer = NULL;
static Vector3 tree_batch_parent_origin = {0};
static Vector3 tree_batch_parent_scale = {1.0f, 1.0f, 1.0f};
static TreeSceneryBatchCache tree_scenery_batch = {0};
static TreeBatchSourceIndexCache tree_batch_source_indices = {0};

static bool TreeBatchColorEqual(Color left, Color right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b &&
           left.a == right.a;
}

static void TreeSceneryBatchClear(void)
{
    if (IsModelValid(tree_scenery_batch.structure)) {
        UnloadModel(tree_scenery_batch.structure);
    }
    if (IsModelValid(tree_scenery_batch.foliage)) {
        UnloadModel(tree_scenery_batch.foliage);
    }
    tree_scenery_batch = (TreeSceneryBatchCache){0};
    tree_batch_source_indices = (TreeBatchSourceIndexCache){0};
}

static bool TreeBatchMeshWriterAllocate(TreeBatchMeshWriter *writer,
                                        int32_t vertex_count,
                                        int32_t index_count)
{
    bool indexed = index_count > 0;
    if (writer == NULL || vertex_count <= 0 ||
        (indexed && (index_count % 3 != 0 || vertex_count > UINT16_MAX)) ||
        (!indexed && vertex_count % 3 != 0)) {
        return false;
    }
    *writer = (TreeBatchMeshWriter){0};
    writer->indexed = indexed;
    writer->mesh.vertexCount = vertex_count;
    writer->mesh.triangleCount = indexed ? index_count / 3 :
                                           vertex_count / 3;
    writer->mesh.vertices = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    writer->mesh.normals = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    writer->mesh.colors = MemAlloc(
        (unsigned int)((size_t)vertex_count * 4U * sizeof(unsigned char)));
    if (indexed) {
        writer->mesh.indices = MemAlloc(
            (unsigned int)((size_t)index_count * sizeof(unsigned short)));
    }
    if (writer->mesh.vertices != NULL && writer->mesh.normals != NULL &&
        writer->mesh.colors != NULL &&
        (!indexed || writer->mesh.indices != NULL)) {
        return true;
    }
    if (writer->mesh.vertices != NULL) MemFree(writer->mesh.vertices);
    if (writer->mesh.normals != NULL) MemFree(writer->mesh.normals);
    if (writer->mesh.colors != NULL) MemFree(writer->mesh.colors);
    if (writer->mesh.indices != NULL) MemFree(writer->mesh.indices);
    *writer = (TreeBatchMeshWriter){0};
    return false;
}

static Vector3 TreeBatchTransformPoint(Vector3 point, Vector3 center,
                                       Vector3 scale, float turn)
{
    float cosine = cosf(turn);
    float sine = sinf(turn);
    float local_x = point.x * scale.x;
    float local_z = point.z * scale.z;
    Vector3 world = {
        center.x + local_x * cosine + local_z * sine,
        center.y + point.y * scale.y,
        center.z - local_x * sine + local_z * cosine,
    };
    return (Vector3){
        tree_batch_parent_origin.x +
            (world.x - tree_batch_parent_origin.x) *
                tree_batch_parent_scale.x,
        tree_batch_parent_origin.y +
            (world.y - tree_batch_parent_origin.y) *
                tree_batch_parent_scale.y,
        tree_batch_parent_origin.z +
            (world.z - tree_batch_parent_origin.z) *
                tree_batch_parent_scale.z,
    };
}

static Vector3 TreeBatchTransformParentPoint(Vector3 point)
{
    return (Vector3){
        tree_batch_parent_origin.x +
            (point.x - tree_batch_parent_origin.x) *
                tree_batch_parent_scale.x,
        tree_batch_parent_origin.y +
            (point.y - tree_batch_parent_origin.y) *
                tree_batch_parent_scale.y,
        tree_batch_parent_origin.z +
            (point.z - tree_batch_parent_origin.z) *
                tree_batch_parent_scale.z,
    };
}

static Vector3 TreeBatchTransformNormal(Vector3 normal, Vector3 scale,
                                        float turn)
{
    float cosine = cosf(turn);
    float sine = sinf(turn);
    float local_x = normal.x / scale.x;
    float local_y = normal.y / scale.y;
    float local_z = normal.z / scale.z;
    Vector3 world = {
        local_x * cosine + local_z * sine,
        local_y,
        -local_x * sine + local_z * cosine,
    };
    world.x /= tree_batch_parent_scale.x;
    world.y /= tree_batch_parent_scale.y;
    world.z /= tree_batch_parent_scale.z;
    return Vector3Normalize(world);
}

static Color TreeBatchTint(Color source, Color tint)
{
    return (Color){
        (unsigned char)(((int32_t)source.r * (int32_t)tint.r) / 255),
        (unsigned char)(((int32_t)source.g * (int32_t)tint.g) / 255),
        (unsigned char)(((int32_t)source.b * (int32_t)tint.b) / 255),
        (unsigned char)(((int32_t)source.a * (int32_t)tint.a) / 255),
    };
}

static int32_t TreeBatchMeshWriteVertex(TreeBatchMeshWriter *writer,
                                        Vector3 point, Vector3 normal,
                                        Color color)
{
    if (writer == NULL ||
        writer->vertex_cursor >= writer->mesh.vertexCount) {
        if (writer != NULL) writer->failed = true;
        return -1;
    }
    int32_t vertex = writer->vertex_cursor++;
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
    return vertex;
}

static void TreeBatchMeshWriteIndex(TreeBatchMeshWriter *writer,
                                    int32_t vertex)
{
    int32_t index_count = writer != NULL ?
        writer->mesh.triangleCount * 3 : 0;
    if (writer == NULL || !writer->indexed || vertex < 0 ||
        vertex > UINT16_MAX || writer->index_cursor >= index_count) {
        if (writer != NULL) writer->failed = true;
        return;
    }
    writer->mesh.indices[writer->index_cursor++] = (unsigned short)vertex;
}

static void TreeBatchMeshWriteIndexedTriangle(TreeBatchMeshWriter *writer,
                                              int32_t a, int32_t b,
                                              int32_t c)
{
    TreeBatchMeshWriteIndex(writer, a);
    TreeBatchMeshWriteIndex(writer, b);
    TreeBatchMeshWriteIndex(writer, c);
}

static void TreeBatchMeshAppendCylinder(TreeBatchMeshWriter *writer,
                                        Vector3 start, Vector3 end,
                                        float start_radius,
                                        float end_radius, int32_t sides,
                                        Color color)
{
    if (writer == NULL) return;
    if (sides < 3) sides = 3;
    Vector3 direction = Vector3Subtract(end, start);
    if (Vector3LengthSqr(direction) <= 0.0f) return;
    int32_t triangle_index_count = sides * 6;
    if (start_radius > 0.0f) triangle_index_count += sides * 3;
    if (end_radius > 0.0f) triangle_index_count += sides * 3;
    writer->source_draw_count += 1;
    if (!writer->indexed) {
        writer->failed = true;
        return;
    }
    if (writer->count_only) {
        writer->vertex_cursor += sides * 2;
        writer->vertex_cursor += start_radius > 0.0f ? 1 : 0;
        writer->vertex_cursor += end_radius > 0.0f ? 1 : 0;
        writer->index_cursor += triangle_index_count;
        return;
    }

    Vector3 basis_one = Vector3Normalize(Vector3Perpendicular(direction));
    Vector3 basis_two = Vector3Normalize(
        Vector3CrossProduct(basis_one, direction));
    float angle_step = 2.0f * PI / (float)sides;
    const Vector3 retained_normal = {-1.0f, 0.0f, 0.0f};
    int32_t start_center = -1;
    int32_t end_center = -1;
    if (start_radius > 0.0f) {
        start_center = TreeBatchMeshWriteVertex(
            writer, TreeBatchTransformParentPoint(start), retained_normal,
            color);
    }
    if (end_radius > 0.0f) {
        end_center = TreeBatchMeshWriteVertex(
            writer, TreeBatchTransformParentPoint(end), retained_normal,
            color);
    }
    int32_t start_ring = writer->vertex_cursor;
    for (int32_t side = 0; side < sides; ++side) {
        float sine = sinf(angle_step * (float)side);
        float cosine = cosf(angle_step * (float)side);
        Vector3 point = Vector3Add(
            start, Vector3Add(
                Vector3Scale(basis_one, sine * start_radius),
                Vector3Scale(basis_two, cosine * start_radius)));
        (void)TreeBatchMeshWriteVertex(
            writer, TreeBatchTransformParentPoint(point), retained_normal,
            color);
    }
    int32_t end_ring = writer->vertex_cursor;
    for (int32_t side = 0; side < sides; ++side) {
        float sine = sinf(angle_step * (float)side);
        float cosine = cosf(angle_step * (float)side);
        Vector3 point = Vector3Add(
            end, Vector3Add(
                Vector3Scale(basis_one, sine * end_radius),
                Vector3Scale(basis_two, cosine * end_radius)));
        (void)TreeBatchMeshWriteVertex(
            writer, TreeBatchTransformParentPoint(point), retained_normal,
            color);
    }
    for (int32_t side = 0; side < sides; ++side) {
        int32_t next = (side + 1) % sides;
        int32_t first_start = start_ring + side;
        int32_t next_start = start_ring + next;
        int32_t first_end = end_ring + side;
        int32_t next_end = end_ring + next;
        if (start_radius > 0.0f) {
            TreeBatchMeshWriteIndexedTriangle(
                writer, start_center, next_start, first_start);
        }
        TreeBatchMeshWriteIndexedTriangle(
            writer, first_start, next_start, first_end);
        TreeBatchMeshWriteIndexedTriangle(
            writer, next_start, next_end, first_end);
        if (end_radius > 0.0f) {
            TreeBatchMeshWriteIndexedTriangle(
                writer, end_center, first_end, next_end);
        }
    }
}

static void TreeBatchMeshAppendModelVertex(
    TreeBatchMeshWriter *writer, const Mesh *mesh, int32_t source,
    Vector3 center, Vector3 scale, float turn, Color tint)
{
    if (source < 0 || source >= mesh->vertexCount) {
        writer->failed = true;
        return;
    }
    Vector3 point = {
        mesh->vertices[source * 3 + 0],
        mesh->vertices[source * 3 + 1],
        mesh->vertices[source * 3 + 2],
    };
    Vector3 normal = {
        mesh->normals[source * 3 + 0],
        mesh->normals[source * 3 + 1],
        mesh->normals[source * 3 + 2],
    };
    Color source_color = WHITE;
    if (mesh->colors != NULL) {
        source_color = (Color){
            mesh->colors[source * 4 + 0],
            mesh->colors[source * 4 + 1],
            mesh->colors[source * 4 + 2],
            mesh->colors[source * 4 + 3],
        };
    }
    point = TreeBatchTransformPoint(point, center, scale, turn);
    normal = TreeBatchTransformNormal(normal, scale, turn);
    (void)TreeBatchMeshWriteVertex(
        writer, point, normal, TreeBatchTint(source_color, tint));
}

static bool TreeBatchMeshSourceVertexEqual(const Mesh *mesh,
                                           int32_t left, int32_t right)
{
    for (int32_t component = 0; component < 3; ++component) {
        if (mesh->vertices[left * 3 + component] !=
                mesh->vertices[right * 3 + component] ||
            mesh->normals[left * 3 + component] !=
                mesh->normals[right * 3 + component]) {
            return false;
        }
    }
    if (mesh->colors == NULL) return true;
    for (int32_t component = 0; component < 4; ++component) {
        if (mesh->colors[left * 4 + component] !=
            mesh->colors[right * 4 + component]) {
            return false;
        }
    }
    return true;
}

static const TreeBatchSourceIndexCache *TreeBatchMeshSourceIndexMap(
    const Mesh *mesh)
{
    if (mesh == NULL || mesh->vertices == NULL || mesh->normals == NULL ||
        mesh->indices != NULL || mesh->vertexCount <= 0 ||
        mesh->vertexCount > CC_TREE_BATCH_SOURCE_VERTEX_LIMIT) {
        return NULL;
    }
    if (tree_batch_source_indices.ready &&
        tree_batch_source_indices.vertices == mesh->vertices &&
        tree_batch_source_indices.normals == mesh->normals &&
        tree_batch_source_indices.colors == mesh->colors &&
        tree_batch_source_indices.vertex_count == mesh->vertexCount) {
        return &tree_batch_source_indices;
    }

    tree_batch_source_indices = (TreeBatchSourceIndexCache){
        .vertices = mesh->vertices,
        .normals = mesh->normals,
        .colors = mesh->colors,
        .vertex_count = mesh->vertexCount,
    };
    for (int32_t source = 0; source < mesh->vertexCount; ++source) {
        int32_t matching_source = -1;
        for (int32_t prior = 0; prior < source; ++prior) {
            if (TreeBatchMeshSourceVertexEqual(mesh, source, prior)) {
                matching_source = prior;
                break;
            }
        }
        if (matching_source >= 0) {
            tree_batch_source_indices.remap[source] =
                tree_batch_source_indices.remap[matching_source];
        } else {
            tree_batch_source_indices.remap[source] =
                tree_batch_source_indices.unique_count++;
        }
    }
    tree_batch_source_indices.ready = true;
    return &tree_batch_source_indices;
}

static void TreeBatchMeshAppendIndexedModel(
    TreeBatchMeshWriter *writer, const Mesh *mesh, Vector3 center,
    Vector3 scale, float turn, Color tint)
{
    int32_t base_vertex = writer->vertex_cursor;
    const TreeBatchSourceIndexCache *source_indices =
        TreeBatchMeshSourceIndexMap(mesh);
    if (source_indices == NULL) {
        for (int32_t source = 0; source < mesh->vertexCount; ++source) {
            TreeBatchMeshAppendModelVertex(
                writer, mesh, source, center, scale, turn, tint);
        }
        int32_t output_count = mesh->indices != NULL ?
            mesh->triangleCount * 3 : mesh->vertexCount;
        for (int32_t output = 0; output < output_count; ++output) {
            int32_t source = mesh->indices != NULL ?
                (int32_t)mesh->indices[output] : output;
            TreeBatchMeshWriteIndex(writer, base_vertex + source);
        }
        return;
    }

    int32_t next_unique = 0;
    for (int32_t source = 0; source < mesh->vertexCount; ++source) {
        if (source_indices->remap[source] != next_unique) continue;
        TreeBatchMeshAppendModelVertex(
            writer, mesh, source, center, scale, turn, tint);
        next_unique += 1;
    }
    for (int32_t output = 0; output < mesh->vertexCount; ++output) {
        TreeBatchMeshWriteIndex(
            writer, base_vertex + source_indices->remap[output]);
    }
}

static void TreeBatchMeshAppendModel(TreeBatchMeshWriter *writer, Model model,
                                     Vector3 center, Vector3 scale,
                                     float turn, Color tint)
{
    if (writer == NULL) return;
    writer->source_draw_count += 1;
    for (int32_t mesh_index = 0; mesh_index < model.meshCount; ++mesh_index) {
        const Mesh *mesh = &model.meshes[mesh_index];
        int32_t output_count = mesh->indices != NULL ?
            mesh->triangleCount * 3 : mesh->vertexCount;
        if (mesh->vertices == NULL || mesh->normals == NULL) {
            writer->failed = true;
            continue;
        }
        if (writer->count_only) {
            const TreeBatchSourceIndexCache *source_indices =
                writer->indexed ? TreeBatchMeshSourceIndexMap(mesh) : NULL;
            writer->vertex_cursor += source_indices != NULL ?
                source_indices->unique_count :
                writer->indexed ? mesh->vertexCount : output_count;
            if (writer->indexed) writer->index_cursor += output_count;
            continue;
        }
        if (writer->indexed) {
            TreeBatchMeshAppendIndexedModel(
                writer, mesh, center, scale, turn, tint);
            continue;
        }
        for (int32_t output = 0; output < output_count; ++output) {
            int32_t source = mesh->indices != NULL ?
                (int32_t)mesh->indices[output] : output;
            TreeBatchMeshAppendModelVertex(
                writer, mesh, source, center, scale, turn, tint);
        }
    }
}

static bool TreeBatchMeshUpload(TreeBatchMeshWriter *writer, bool foliage,
                                Model *model)
{
    if (writer == NULL || model == NULL || writer->failed ||
        writer->vertex_cursor != writer->mesh.vertexCount ||
        (writer->indexed &&
         writer->index_cursor != writer->mesh.triangleCount * 3)) {
        if (writer != NULL && writer->mesh.vertices != NULL) {
            UnloadMesh(writer->mesh);
            writer->mesh = (Mesh){0};
        }
        return false;
    }
    UploadMesh(&writer->mesh, false);
    *model = LoadModelFromMesh(writer->mesh);
    if (!IsModelValid(*model)) return false;
    if (foliage && tree_crown_models.ready) {
        ApplyTreeFoliageShader(model);
    } else {
        ApplyWorldShader(model);
    }
    ReleaseUploadedModelCpuData(model);
    return true;
}

static Model TreeCrownModel(TreeCrownShape shape)
{
    switch (shape) {
        case TREE_CROWN_OAK: return tree_crown_models.oak;
        case TREE_CROWN_POLLARD: return tree_crown_models.pollard;
        case TREE_CROWN_ALDER:
        default: return tree_crown_models.alder;
    }
}

static void DrawTreeFoliageMass(TreeCrownShape shape, Vector3 center,
                                Vector3 radius, float turn, Color color)
{
    Model model = tree_crown_models.ready ? TreeCrownModel(shape) :
                                           sphere_models.small;
    if (tree_render_pass == TREE_RENDER_BATCH_BUILD) {
        TreeBatchMeshAppendModel(tree_foliage_writer, model, center, radius,
                                 turn, color);
        return;
    }
    DrawModelEx(model, center, (Vector3){0.0f, 1.0f, 0.0f},
                turn * RAD2DEG, radius, color);
}

static void DrawTreeShadow(Vector3 root, float turn, float width, float depth)
{
    Vector3 broad = LocalPoint(root, width * 0.24f, 0.018f,
                               -depth * 0.20f, turn);
    Vector3 branch = LocalPoint(root, width * 0.54f, 0.020f,
                                -depth * 0.42f, turn);
    if (tree_render_pass == TREE_RENDER_BATCH_BUILD) {
        TreeBatchMeshAppendModel(
            tree_structure_writer, sphere_models.small, broad,
            (Vector3){width, 0.018f, depth}, turn,
            Fade(WORLD_VOID, 0.26f));
        TreeBatchMeshAppendModel(
            tree_structure_writer, sphere_models.small, branch,
            (Vector3){width * 0.62f, 0.020f, depth * 0.45f},
            turn - 0.38f, Fade(WORLD_VOID, 0.20f));
        return;
    }
    DrawModelEx(sphere_models.small, broad, (Vector3){0.0f, 1.0f, 0.0f},
                turn * RAD2DEG, (Vector3){width, 0.018f, depth},
                Fade(WORLD_VOID, 0.26f));
    DrawModelEx(sphere_models.small, branch, (Vector3){0.0f, 1.0f, 0.0f},
                (turn - 0.38f) * RAD2DEG,
                (Vector3){width * 0.62f, 0.020f, depth * 0.45f},
                Fade(WORLD_VOID, 0.20f));
}

static void DrawTreeCylinder(Vector3 start, Vector3 end,
                             float start_radius, float end_radius,
                             int32_t sides, Color color)
{
    if (tree_render_pass == TREE_RENDER_BATCH_BUILD) {
        TreeBatchMeshAppendCylinder(tree_structure_writer, start, end,
                                    start_radius, end_radius, sides, color);
        return;
    }
    DrawCylinderEx(start, end, start_radius, end_radius, sides, color);
}

static void DrawTreeRootFlare(Vector3 root, float turn, float radius,
                              Color bark)
{
    for (int32_t root_index = 0; root_index < 4; ++root_index) {
        float angle = turn + (float)root_index * PI * 0.5f +
                      (root_index & 1 ? 0.18f : -0.12f);
        Vector3 start = {root.x, root.y + radius * 0.72f, root.z};
        Vector3 end = {
            root.x + cosf(angle) * radius * 2.35f,
            root.y + 0.025f,
            root.z + sinf(angle) * radius * 2.35f
        };
        DrawTreeCylinder(start, end, radius * 0.56f, 0.025f, 6,
                         ShadeColor(bark, root_index & 1 ? 0.76f : 0.88f));
    }
}

static void DrawAlderTree(Vector3 root, float turn, float shape, Color leaves)
{
    float scale = 0.90f + shape * 0.18f;
    float height = (4.65f + shape * 0.55f) * scale;
    float lean = (shape - 0.5f) * 0.34f;
    Color bark = BlendColor((Color){57, 47, 41, 255},
                            (Color){94, 62, 43, 255}, shape * 0.44f);
    Color deep = ShadeColor(leaves, 0.78f);
    Color middle = ShadeColor(leaves, 0.94f + shape * 0.04f);
    Color light = ShadeColor(leaves, 1.08f);
    Vector3 fork = LocalPoint(root, lean, height * 0.48f, 0.0f, turn);
    Vector3 left = LocalPoint(root, -0.72f * scale, height * 0.68f,
                              0.08f * scale, turn);
    Vector3 right = LocalPoint(root, 0.68f * scale, height * 0.72f,
                               -0.10f * scale, turn);
    Vector3 crown = LocalPoint(root, lean * 1.35f, height * 0.80f,
                               0.04f * scale, turn);
    Vector3 top = LocalPoint(root, lean * 1.70f, height * 0.93f,
                             -0.02f * scale, turn);

    DrawTreeShadow(root, turn, 1.18f * scale, 0.90f * scale);
    DrawTreeRootFlare(root, turn, 0.25f * scale, bark);
    DrawTreeCylinder(root, fork, 0.27f * scale, 0.15f * scale, 7, bark);
    DrawTreeCylinder(fork, left, 0.14f * scale, 0.055f * scale, 6,
                     ShadeColor(bark, 0.88f));
    DrawTreeCylinder(fork, right, 0.13f * scale, 0.050f * scale, 6,
                     ShadeColor(bark, 0.94f));
    DrawTreeCylinder(fork, top, 0.15f * scale, 0.040f * scale, 6, bark);

    DrawTreeFoliageMass(TREE_CROWN_ALDER, left,
                        (Vector3){0.86f, 0.68f, 0.72f}, turn + 0.18f, deep);
    DrawTreeFoliageMass(TREE_CROWN_ALDER, right,
                        (Vector3){0.84f, 0.70f, 0.70f}, turn - 0.21f, middle);
    DrawTreeFoliageMass(TREE_CROWN_ALDER, crown,
                        (Vector3){0.90f, 0.76f, 0.76f}, turn + 0.08f, middle);
    DrawTreeFoliageMass(TREE_CROWN_ALDER,
                        LocalPoint(root, -0.38f * scale, height * 0.81f,
                                   -0.36f * scale, turn),
                        (Vector3){0.68f, 0.62f, 0.62f}, turn - 0.32f, deep);
    DrawTreeFoliageMass(TREE_CROWN_ALDER,
                        LocalPoint(root, 0.40f * scale, height * 0.85f,
                                   0.28f * scale, turn),
                        (Vector3){0.66f, 0.66f, 0.60f}, turn + 0.36f, middle);
    DrawTreeFoliageMass(TREE_CROWN_ALDER, top,
                        (Vector3){0.68f, 0.74f, 0.62f}, turn, light);
}

static void DrawOakTree(Vector3 root, float turn, float shape, Color leaves)
{
    float scale = 0.92f + shape * 0.16f;
    float height = (5.15f + shape * 0.55f) * scale;
    float lean = (shape - 0.38f) * 0.72f;
    Color bark = BlendColor((Color){60, 43, 37, 255},
                            (Color){105, 67, 43, 255}, shape * 0.34f);
    Color deep = ShadeColor(leaves, 0.72f);
    Color middle = ShadeColor(leaves, 0.88f);
    Color light = ShadeColor(leaves, 1.02f);
    Vector3 fork = LocalPoint(root, lean * 0.52f, height * 0.38f,
                              0.0f, turn);
    Vector3 left = LocalPoint(root, -1.58f * scale, height * 0.68f,
                              0.12f * scale, turn);
    Vector3 right = LocalPoint(root, 1.64f * scale, height * 0.70f,
                               -0.18f * scale, turn);
    Vector3 upper = LocalPoint(root, lean + 0.18f * scale,
                               height * 0.90f, 0.05f * scale, turn);
    Vector3 dead_tip = LocalPoint(root, 1.52f * scale, height * 0.91f,
                                  -0.12f * scale, turn);

    DrawTreeShadow(root, turn, 2.10f * scale, 1.42f * scale);
    DrawTreeRootFlare(root, turn, 0.38f * scale, bark);
    DrawTreeCylinder(root, fork, 0.43f * scale, 0.28f * scale, 7, bark);
    DrawTreeCylinder(fork, left, 0.27f * scale, 0.075f * scale, 7,
                     ShadeColor(bark, 0.86f));
    DrawTreeCylinder(fork, right, 0.25f * scale, 0.070f * scale, 7, bark);
    DrawTreeCylinder(fork, upper, 0.24f * scale, 0.060f * scale, 7,
                     ShadeColor(bark, 0.94f));
    DrawTreeCylinder(right, dead_tip, 0.065f * scale, 0.025f * scale, 5,
                     ShadeColor(bark, 1.08f));

    DrawTreeFoliageMass(TREE_CROWN_OAK, left,
                        (Vector3){1.20f, 0.68f, 0.94f}, turn + 0.18f, deep);
    DrawTreeFoliageMass(TREE_CROWN_OAK, right,
                        (Vector3){1.18f, 0.70f, 0.92f}, turn - 0.14f, middle);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, -0.72f * scale, height * 0.81f,
                                   -0.54f * scale, turn),
                        (Vector3){1.06f, 0.72f, 0.90f}, turn - 0.28f, middle);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, 0.54f * scale, height * 0.78f,
                                   0.58f * scale, turn),
                        (Vector3){1.10f, 0.70f, 0.86f}, turn + 0.30f, deep);
    DrawTreeFoliageMass(TREE_CROWN_OAK, upper,
                        (Vector3){1.15f, 0.76f, 0.94f}, turn + 0.04f, light);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, -1.78f * scale, height * 0.60f,
                                   -0.38f * scale, turn),
                        (Vector3){0.82f, 0.54f, 0.68f}, turn - 0.18f, deep);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, 1.82f * scale, height * 0.59f,
                                   0.22f * scale, turn),
                        (Vector3){0.84f, 0.56f, 0.70f}, turn + 0.23f, middle);
}

static void DrawPollardTree(Vector3 root, float turn, float shape,
                            Color leaves)
{
    float scale = 0.90f + shape * 0.16f;
    float height = (3.85f + shape * 0.45f) * scale;
    float head_height = height * 0.49f;
    Color bark = BlendColor((Color){62, 48, 40, 255},
                            (Color){111, 72, 45, 255}, shape * 0.30f);
    Color deep = ShadeColor(leaves, 0.78f);
    Color middle = ShadeColor(leaves, 0.94f);
    Color light = ShadeColor(leaves, 1.08f);
    Vector3 head = LocalPoint(root, (shape - 0.5f) * 0.18f,
                              head_height, 0.0f, turn);
    static const float stem_x[] = {-0.78f, -0.36f, 0.02f, 0.42f, 0.80f};
    static const float stem_z[] = {0.18f, -0.22f, 0.20f, -0.16f, 0.12f};
    static const float stem_height[] = {0.78f, 0.93f, 1.00f, 0.88f, 0.72f};

    DrawTreeShadow(root, turn, 1.34f * scale, 1.00f * scale);
    DrawTreeRootFlare(root, turn, 0.31f * scale, bark);
    DrawTreeCylinder(root, head, 0.34f * scale, 0.27f * scale, 7, bark);
    DrawTreeFoliageMass(TREE_CROWN_POLLARD, head,
                        (Vector3){0.40f, 0.32f, 0.38f}, turn,
                        ShadeColor(bark, 0.82f));

    for (int32_t stem = 0; stem < 5; ++stem) {
        Vector3 tip = LocalPoint(root, stem_x[stem] * scale,
                                 height * stem_height[stem],
                                 stem_z[stem] * scale, turn);
        DrawTreeCylinder(head, tip, 0.090f * scale, 0.030f * scale, 6,
                         ShadeColor(bark, 0.86f + (float)stem * 0.025f));
        Color stem_leaves = stem == 2 ? light :
                            (stem & 1) != 0 ? middle : deep;
        DrawTreeFoliageMass(TREE_CROWN_POLLARD, tip,
                            (Vector3){0.50f, 0.72f, 0.46f},
                            turn + stem_x[stem] * 0.32f, stem_leaves);
    }
}

static void DrawTree(float x, float z, TreeFamily family, Color leaves,
                     TreeRegionalStyle regional_style)
{
    float base = CcLocalTerrainHeightAt(x, z);
    int32_t column = (int32_t)lroundf(x * 10.0f);
    int32_t row = (int32_t)lroundf(z * 10.0f);
    float shape = CombatClamp(TerrainScatter01(column, row, 71U) +
                              regional_style.shape_shift, 0.0f, 1.0f);
    float turn = TerrainScatter01(column, row, 72U) * PI * 2.0f;
    float turn_step = PI * 2.0f / regional_style.turn_steps;
    turn = floorf(turn / turn_step + 0.5f) * turn_step +
           (shape - 0.5f) * 0.16f;
    Vector3 root = {x, base + 0.018f, z};
    leaves = BlendColor(leaves, regional_style.foliage_bias,
                        regional_style.foliage_mix);


    if (tree_render_pass == TREE_RENDER_BATCH_BUILD) {
        tree_batch_parent_origin = root;
        tree_batch_parent_scale = regional_style.proportions;
    } else {
        rlPushMatrix();
        rlTranslatef(root.x, root.y, root.z);
        rlScalef(regional_style.proportions.x,
                 regional_style.proportions.y,
                 regional_style.proportions.z);
        rlTranslatef(-root.x, -root.y, -root.z);
    }

    switch (family) {
        case TREE_FAMILY_OAK:
            DrawOakTree(root, turn, shape, leaves);
            break;
        case TREE_FAMILY_POLLARD:
            DrawPollardTree(root, turn, shape, leaves);
            break;
        case TREE_FAMILY_ALDER:
        default:
            DrawAlderTree(root, turn, shape, leaves);
            break;
    }
    if (tree_render_pass != TREE_RENDER_BATCH_BUILD) rlPopMatrix();
}

static void DrawWorldTreeAt(int32_t index, TreeRegionalStyle regional_style)
{
    Vector2 position = WORLD_TREES[index].position;
    position.x += (index & 1) != 0 ? -regional_style.cluster_pull :
                                     regional_style.cluster_pull;
    TreeFamily family = RegionalTreeFamily(WORLD_TREES[index].family, index,
                                           regional_style);
    Color leaves = (index & 1) != 0 ? WORLD_FOLIAGE : WORLD_FOLIAGE_LIGHT;
    if (family == TREE_FAMILY_OAK) {
        leaves = ShadeColor(leaves, 0.86f);
    } else if (family == TREE_FAMILY_POLLARD) {
        leaves = BlendColor(leaves, WORLD_GRASS_LIGHT, 0.18f);
    }
    DrawTree(position.x, position.y, family, leaves, regional_style);
}

static uint32_t WorldTreeVisibilityMask(Vector3 focus,
                                        TreeRegionalStyle regional_style)
{
    uint32_t mask = 0U;
    int32_t scene = street_camera_rig.shot;
    for (int32_t i = 0;
         i < (int32_t)(sizeof(WORLD_TREES) / sizeof(WORLD_TREES[0])); ++i) {
        if (active_place_function == CC_SETTLEMENT_MINING &&
            scene == CC_LOCAL_TOWN_SCENE_LANDMARK &&
            (i == 4 || i == 25)) continue;
        Vector2 position = WORLD_TREES[i].position;
        position.x += (i & 1) != 0 ? -regional_style.cluster_pull :
                                     regional_style.cluster_pull;
        if (!SceneryPointVisible(position.x, position.y, focus)) continue;
        mask |= UINT32_C(1) << (uint32_t)i;
    }
    return mask;
}

static void EmitWorldTreeMask(uint32_t visibility_mask,
                              TreeRegionalStyle regional_style)
{
    for (int32_t i = 0;
         i < (int32_t)(sizeof(WORLD_TREES) / sizeof(WORLD_TREES[0])); ++i) {
        if ((visibility_mask & (UINT32_C(1) << (uint32_t)i)) == 0U) continue;
        DrawWorldTreeAt(i, regional_style);
    }
}

static bool TreeSceneryBatchMatches(uint32_t visibility_mask, Color kingdom)
{
    return tree_scenery_batch.ready &&
           tree_scenery_batch.seed == street_terrain_seed &&
           tree_scenery_batch.visibility_mask == visibility_mask &&
           tree_scenery_batch.function == active_place_function &&
           TreeBatchColorEqual(tree_scenery_batch.kingdom, kingdom);
}

static void TreeBatchFinishBuildPass(void)
{
    tree_render_pass = TREE_RENDER_FULL;
    tree_structure_writer = NULL;
    tree_foliage_writer = NULL;
    tree_batch_parent_origin = (Vector3){0};
    tree_batch_parent_scale = (Vector3){1.0f, 1.0f, 1.0f};
}

static bool TreeSceneryBatchBuild(uint32_t visibility_mask, Color kingdom,
                                  TreeRegionalStyle regional_style)
{
    TreeSceneryBatchClear();
    rlDrawRenderBatchActive();

    TreeBatchMeshWriter structure_counter = {
        .indexed = true,
        .count_only = true,
    };
    TreeBatchMeshWriter foliage_counter = {.count_only = true};
    tree_render_pass = TREE_RENDER_BATCH_BUILD;
    tree_structure_writer = &structure_counter;
    tree_foliage_writer = &foliage_counter;
    EmitWorldTreeMask(visibility_mask, regional_style);
    TreeBatchFinishBuildPass();
    if (structure_counter.vertex_cursor <= 0 ||
        structure_counter.index_cursor <= 0 ||
        foliage_counter.vertex_cursor <= 0) {
        return false;
    }

    TreeBatchMeshWriter structure_writer;
    TreeBatchMeshWriter foliage_writer;
    if (!TreeBatchMeshWriterAllocate(&structure_writer,
                                     structure_counter.vertex_cursor,
                                     structure_counter.index_cursor)) {
        return false;
    }
    if (!TreeBatchMeshWriterAllocate(&foliage_writer,
                                     foliage_counter.vertex_cursor, 0)) {
        UnloadMesh(structure_writer.mesh);
        return false;
    }

    tree_render_pass = TREE_RENDER_BATCH_BUILD;
    tree_structure_writer = &structure_writer;
    tree_foliage_writer = &foliage_writer;
    EmitWorldTreeMask(visibility_mask, regional_style);
    TreeBatchFinishBuildPass();

    bool structure_ready = TreeBatchMeshUpload(
        &structure_writer, false, &tree_scenery_batch.structure);
    bool foliage_ready = TreeBatchMeshUpload(
        &foliage_writer, true, &tree_scenery_batch.foliage);
    if (!structure_ready || !foliage_ready) {
        TreeSceneryBatchClear();
        return false;
    }

    tree_scenery_batch.seed = street_terrain_seed;
    tree_scenery_batch.visibility_mask = visibility_mask;
    tree_scenery_batch.function = active_place_function;
    tree_scenery_batch.kingdom = kingdom;
    tree_scenery_batch.source_draw_count =
        structure_counter.source_draw_count +
        foliage_counter.source_draw_count;
    tree_scenery_batch.vertex_count = structure_counter.vertex_cursor +
                                      foliage_counter.vertex_cursor;
    tree_scenery_batch.ready = true;
    TraceLog(LOG_INFO,
             "TREE: batched %d model draws into 2 GPU meshes (%d vertices)",
             tree_scenery_batch.source_draw_count,
             tree_scenery_batch.vertex_count);
    return true;
}

static void DrawWorldTrees(Vector3 focus, Color kingdom)
{
    TreeRegionalStyle regional_style = TreeStyleForKingdom(kingdom);
    uint32_t visibility_mask = WorldTreeVisibilityMask(focus, regional_style);
    if (visibility_mask == 0U) return;
    if (!TreeSceneryBatchMatches(visibility_mask, kingdom) &&
        !TreeSceneryBatchBuild(visibility_mask, kingdom, regional_style)) {
        EmitWorldTreeMask(visibility_mask, regional_style);
        return;
    }

    int32_t draw_count = 0;
    DrawModel(tree_scenery_batch.structure, (Vector3){0}, 1.0f, WHITE);
    draw_count += 1;
    DrawModel(tree_scenery_batch.foliage, (Vector3){0}, 1.0f, WHITE);
    draw_count += 1;
    CcLocalRendererRecordStaticBatch(
        draw_count, tree_scenery_batch.source_draw_count,
        tree_scenery_batch.vertex_count);
}

static Rectangle ViewportRectangle(Rectangle viewport, float x, float y,
                                   float width, float height)
{
    return (Rectangle){viewport.x + x, viewport.y + y, width, height};
}

static void DrawViewportText(const char *text, Rectangle viewport,
                             int32_t x, int32_t y, int32_t font_size,
                             Color color)
{
    CcOverlayDrawText(text, (int32_t)lroundf(viewport.x) + x,
             (int32_t)lroundf(viewport.y) + y, font_size, color);
}

static bool AgentNearLabel(const CcLocalAgent *agent, float x, float z,
                           float radius)
{
    if (agent == NULL) return false;
    float dx = agent->position.x - x;
    float dz = agent->position.z - z;
    return dx * dx + dz * dz <= radius * radius;
}

static void DrawStreetTraversalPortals(const CcLocalAgent *agent,
                                       Camera3D camera, Rectangle viewport,
                                       int32_t art_width,
                                       int32_t art_height)
{
    int32_t count = CcLocalAgentStreetPortalCount(agent);
    for (int32_t portal_index = 0; portal_index < count; ++portal_index) {
        ResolvedStreetPortal portal = {0};
        if (!ResolveStreetPortal(agent, portal_index, &portal)) continue;
        Vector3 approach = StreetPortalApproachWorldPoint(&portal);
        if (!AgentNearLabel(agent, approach.x, approach.z, 7.0f)) continue;
        Vector2 art_point = StreetPortalEdgePoint(
            &portal, camera, art_width, art_height);
        Vector2 point = {
            viewport.x + art_point.x / (float)art_width * viewport.width,
            viewport.y + art_point.y / (float)art_height * viewport.height,
        };
        const char *name = CcLocalAgentStreetPortalName(agent, portal_index);
        if (name == NULL) continue;
        char label[96];
        (void)snprintf(label, sizeof(label), "%s  %s",
                       portal.exit != NULL ? "ROAD" : "TO", name);
    int32_t text_width = CcOverlayMeasureText(label, 9);
    float bubble_width = (float)text_width + 16.0f;
        float bubble_x = point.x < viewport.x + viewport.width * 0.5f ?
            point.x + 10.0f : point.x - bubble_width - 10.0f;
        float bubble_y = point.y - 10.0f;
        bubble_x = fmaxf(viewport.x + 5.0f,
                         fminf(bubble_x, viewport.x + viewport.width -
                                             bubble_width - 5.0f));
        bubble_y = fmaxf(viewport.y + 5.0f,
                         fminf(bubble_y, viewport.y + viewport.height -
                                             25.0f));
        Color accent = portal.exit != NULL ? WORLD_GOLD : WORLD_TEAL;
        DrawRectangleRounded(
            (Rectangle){bubble_x, bubble_y, bubble_width, 22.0f},
            0.04f, 3, Fade(CC_STYLE_PANEL_DEEP, 0.94f));
        DrawRectangleRoundedLinesEx(
            (Rectangle){bubble_x, bubble_y, bubble_width, 22.0f},
            0.04f, 3, 1.0f, Fade(WORLD_GOLD, 0.62f));
        DrawLine((int32_t)lroundf(bubble_x) + 7,
                 (int32_t)lroundf(bubble_y) + 5,
                 (int32_t)lroundf(bubble_x) + 30,
                 (int32_t)lroundf(bubble_y) + 5,
                 Fade(accent, 0.72f));
        DrawPoly(point, 4, 6.0f, 45.0f, accent);
        DrawPolyLinesEx(point, 4, 8.0f, 45.0f, 1.0f,
                        Fade(WORLD_INK, 0.82f));
        CcOverlayDrawText(label, (int32_t)lroundf(bubble_x) + 10,
                         (int32_t)lroundf(bubble_y) + 8, 10, accent);
    }
}

static void DrawLabels(const WorldLabel *labels, int32_t count, Camera3D camera,
                       Rectangle viewport)
{
    const float bubble_height = 18.0f;
    const float head_clearance = 8.0f;
    int32_t width = (int32_t)lroundf(viewport.width);
    int32_t height = (int32_t)lroundf(viewport.height);
    for (int32_t i = 0; i < count; ++i) {
        if (!CameraPointInFront(camera, labels[i].point)) continue;
        Vector2 screen = GetWorldToScreenEx(labels[i].point, camera, width, height);
        if (screen.x < -120.0f || screen.x > viewport.width + 120.0f ||
            screen.y < bubble_height + head_clearance + 4.0f ||
            screen.y > viewport.height + 40.0f) continue;
        int text_width = CcOverlayMeasureText(labels[i].text, 10);
        float bubble_width = (float)text_width + 16.0f;
        float bubble_x = viewport.x + screen.x - bubble_width * 0.5f;

        float bubble_y = viewport.y + screen.y - bubble_height -
                         head_clearance;
        bubble_x = fmaxf(viewport.x + 4.0f,
                         fminf(bubble_x,
                               viewport.x + viewport.width - bubble_width -
                                   4.0f));
        bubble_y = fmaxf(viewport.y + 4.0f,
                         fminf(bubble_y,
                               viewport.y + viewport.height - 20.0f));
        Rectangle bubble = {bubble_x, bubble_y, bubble_width, bubble_height};
        DrawRectangleRounded(bubble, 0.05f, 3,
                             Fade(CC_STYLE_PANEL_DEEP, 0.94f));
        DrawRectangleRoundedLinesEx(bubble, 0.05f, 3, 1.0f,
                                    Fade(WORLD_GOLD, 0.44f));
        DrawRectangle((int32_t)lroundf(bubble_x) + 4,
                      (int32_t)lroundf(bubble_y) + 4, 3, 10,
                      Fade(labels[i].color, 0.78f));
        DrawLine((int32_t)lroundf(bubble_x + bubble_width * 0.5f),
                 (int32_t)lroundf(bubble_y + bubble_height),
                 (int32_t)lroundf(bubble_x + bubble_width * 0.5f),
                 (int32_t)lroundf(bubble_y + bubble_height + 4.0f),
                 Fade(labels[i].color, 0.66f));
        CcOverlayDrawText(labels[i].text, (int)lroundf(bubble_x) + 10,
                 (int)lroundf(bubble_y) + 4, 10, labels[i].color);
    }
}

static void DrawCombatBar(const CcLocalAgent *agent, Camera3D camera,
                          Rectangle viewport, Color accent)
{
    if (agent == NULL) return;

    Vector3 anchor = {agent->position.x, agent->position.y + 2.82f,
                      agent->position.z};
    if (!CameraPointInFront(camera, anchor)) return;
    int32_t width = (int32_t)lroundf(viewport.width);
    int32_t height = (int32_t)lroundf(viewport.height);
    Vector2 screen = GetWorldToScreenEx(
        anchor, camera, width, height);
    const float bar_width = 44.0f;
    if (screen.x < bar_width * 0.5f + 3.0f ||
        screen.x > (float)width - bar_width * 0.5f - 3.0f ||
        screen.y < 10.0f || screen.y > (float)height - 8.0f) return;
    screen.x += viewport.x;
    screen.y += viewport.y;
    float health = CombatClamp(agent->combat.health /
                               CC_LOCAL_COMBAT_MAX_HEALTH, 0.0f, 1.0f);
    float posture = CombatClamp(agent->combat.posture /
                                CC_LOCAL_COMBAT_MAX_POSTURE, 0.0f, 1.0f);
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y,
                  (int)bar_width, 5, (Color){5, 11, 15, 220});
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y,
                  (int)(bar_width * health), 3,
                  CombatIsDefeated(&agent->combat) ? WORLD_MUTED : accent);
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y + 4,
                  (int)(bar_width * posture), 2, WORLD_GOLD);
    if (CombatIsDefeated(&agent->combat)) {
        int label_width = CcOverlayMeasureText("DEAD", 8);
        CcOverlayDrawText("DEAD", (int)screen.x - label_width / 2,
                 (int)screen.y - 9, 8, WORLD_DANGER);
    }
}

static void DrawPaintedTriangle(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(c, b, a, color);
}

static void DrawGroundBrushStroke(Vector3 center, Vector3 along,
                                  float length, float width, Color color)
{
    along.y = 0.0f;
    along = PhysicsNormalizeOr(along, (Vector3){0.0f, 0.0f, 1.0f});
    Vector3 side = PhysicsNormalizeOr(
        PhysicsCross((Vector3){0.0f, 1.0f, 0.0f}, along),
        (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 start = PhysicsAdd(center, PhysicsScale(along, -length * 0.52f));
    Vector3 end = PhysicsAdd(center, PhysicsScale(along, length * 0.48f));
    Vector3 a = PhysicsAdd(start, PhysicsScale(side, -width * 0.45f));
    Vector3 b = PhysicsAdd(start, PhysicsScale(side, width * 0.32f));
    Vector3 c = PhysicsAdd(end, PhysicsScale(side, width * 0.16f));
    Vector3 d = PhysicsAdd(end, PhysicsScale(side, -width * 0.23f));
    DrawPaintedTriangle(a, b, c, color);
    DrawPaintedTriangle(a, c, d, color);
}

static void DrawPaintedOverheadDiamond(Vector3 center, float radius,
                                       Color color)
{
    Vector3 top = {center.x, center.y + radius, center.z};
    Vector3 right = {center.x + radius * 0.72f, center.y, center.z};
    Vector3 bottom = {center.x, center.y - radius, center.z};
    Vector3 left = {center.x - radius * 0.72f, center.y, center.z};
    DrawPaintedTriangle(top, right, bottom, color);
    DrawPaintedTriangle(top, bottom, left, ShadeColor(color, 0.78f));
}

static void DrawCombatImpact(const CcLocalAgent *agent)
{
    if (agent->combat.hit_flash_seconds <= 0.0f) return;
    float pulse = 0.13f + agent->combat.hit_flash_seconds * 1.05f;
    Vector3 point = agent->combat.impact_valid ? agent->combat.impact_point :
        (Vector3){agent->position.x, agent->position.y + 1.02f,
                  agent->position.z};
    if (agent->combat.impact_valid) {
        Vector3 direction = PhysicsNormalizeOr(
            agent->combat.impact_direction,
            (Vector3){0.0f, 0.0f, 1.0f});
        Vector3 side = PhysicsNormalizeOr(
            PhysicsCross(direction, (Vector3){0.0f, 1.0f, 0.0f}),
            (Vector3){1.0f, 0.0f, 0.0f});
        float spark = pulse * 1.65f;
        Vector3 wedge_base = PhysicsAdd(
            point, PhysicsScale(direction, -pulse * 0.58f));
        Vector3 wedge_left = PhysicsAdd(
            PhysicsAdd(wedge_base, PhysicsScale(side, -pulse * 0.62f)),
            (Vector3){0.0f, -pulse * 0.26f, 0.0f});
        Vector3 wedge_right = PhysicsAdd(
            PhysicsAdd(wedge_base, PhysicsScale(side, pulse * 0.62f)),
            (Vector3){0.0f, pulse * 0.16f, 0.0f});
        Vector3 wedge_tip = PhysicsAdd(
            PhysicsAdd(point, PhysicsScale(direction, pulse * 1.52f)),
            (Vector3){0.0f, pulse * 0.28f, 0.0f});
        DrawPaintedTriangle(wedge_left, wedge_right, wedge_tip,
                            Fade(WORLD_GOLD, 0.84f));

        Vector3 slash_start = PhysicsAdd(
            PhysicsAdd(point, PhysicsScale(side, -spark)),
            (Vector3){0.0f, -pulse * 0.22f, 0.0f});
        Vector3 slash_end = PhysicsAdd(
            PhysicsAdd(point, PhysicsScale(side, spark * 0.86f)),
            (Vector3){0.0f, pulse * 0.48f, 0.0f});
        DrawCylinderEx(slash_start, slash_end, pulse * 0.12f,
                       pulse * 0.025f, 5, Fade(WORLD_INK, 0.78f));

        static const Vector3 spark_directions[] = {
            {-0.72f, 0.86f, 0.18f},
            {0.34f, 1.00f, -0.28f},
            {0.78f, 0.62f, 0.34f}
        };
        for (int32_t i = 0; i < 3; ++i) {
            Vector3 spark_end = PhysicsAdd(
                point, PhysicsScale(spark_directions[i], pulse * 1.22f));
            DrawCylinderEx(point, spark_end, pulse * 0.050f,
                           pulse * 0.012f, 5,
                           i == 1 ? WHITE : WORLD_GOLD);
        }

        Vector3 ground = {point.x, agent->position.y + 0.022f, point.z};
        DrawGroundBrushStroke(ground, direction, pulse * 2.8f,
                              pulse * 0.62f,
                              Fade((Color){69, 53, 43, 255}, 0.52f));
        DrawSmallSphere(point, pulse * 0.22f, WHITE);
    } else {
        DrawSmallSphere(point, pulse * 0.34f, WORLD_GOLD);
    }
}

static void DrawCombatSkillTell(const CcLocalAgent *agent)
{
    if (agent == NULL ||
        agent->humanoid.action != CC_HUMANOID_ACTION_STRIKE ||
        (agent->combat.active_skill != CC_COMBAT_SKILL_CRUSHING_BLOW &&
         agent->combat.active_skill != CC_COMBAT_SKILL_SUNDER)) {
        return;
    }
    float phase = CombatClamp(agent->humanoid.action_time / 1.20f,
                              0.0f, 1.0f);
    Color color = agent->combat.active_skill ==
                  CC_COMBAT_SKILL_CRUSHING_BLOW ? WORLD_GOLD : WORLD_VIOLET;
    float radius = 0.40f + sinf(phase * PI) * 0.22f;
    Vector3 ground = {agent->position.x, agent->position.y + 0.025f,
                      agent->position.z};
    Vector3 facing = {sinf(agent->facing_yaw), 0.0f,
                      cosf(agent->facing_yaw)};
    Vector3 side = {facing.z, 0.0f, -facing.x};
    Vector3 tip = PhysicsAdd(ground, PhysicsScale(facing, radius * 1.65f));
    Vector3 left = PhysicsAdd(
        PhysicsAdd(ground, PhysicsScale(facing, radius * 0.18f)),
        PhysicsScale(side, -radius * 0.54f));
    Vector3 right = PhysicsAdd(
        PhysicsAdd(ground, PhysicsScale(facing, radius * 0.18f)),
        PhysicsScale(side, radius * 0.54f));
    DrawPaintedTriangle(left, right, tip, Fade(color, 0.34f));
    DrawGroundBrushStroke(PhysicsAdd(ground, PhysicsScale(facing, radius)),
                          facing, radius * 1.28f, radius * 0.14f,
                          Fade(color, 0.74f));
}

static void DrawCombatFootprint(const CcLocalAgent *agent, Color color)
{
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        agent->humanoid.ragdoll.active) return;
    Vector3 ground = {agent->position.x, agent->position.y + 0.012f,
                      agent->position.z};
    DrawCylinder(ground, 0.31f, 0.31f, 0.012f, 20,
                 Fade(WORLD_VOID, 0.38f));
    Vector3 facing = {sinf(agent->facing_yaw), 0.0f,
                      cosf(agent->facing_yaw)};
    DrawGroundBrushStroke((Vector3){ground.x, ground.y + 0.006f, ground.z},
                          facing, 0.62f, 0.11f, Fade(color, 0.58f));
}

static void DrawCombatSword(const CcLocalAgent *agent)
{
    if (agent == NULL ||
        (agent->combat.team != CC_COMBAT_PLAYER &&
         agent->combat.team != CC_COMBAT_RAIDER) ||
        agent->combat.weapon_mode == CC_WEAPON_NONE ||
        agent->swimming || agent->climbing) {
        return;
    }
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    Vector3 hand = FromLimbVector(pose->hand[1]);
    Vector3 direction;
    Vector3 right;
    if (agent->combat.weapon_mode == CC_WEAPON_RAGDOLL_ATTACHED) {
        Vector3 elbow = FromLimbVector(pose->elbow[1]);
        direction = PhysicsNormalizeOr(PhysicsSubtract(hand, elbow),
                                       (Vector3){0.0f, 0.0f, 1.0f});
        right = PhysicsNormalizeOr(
            PhysicsCross((Vector3){0.0f, 1.0f, 0.0f}, direction),
            (Vector3){1.0f, 0.0f, 0.0f});
    } else {
        direction = CombatWeaponDirectionAt(agent,
                                             agent->humanoid.action_time);
        right = (Vector3){cosf(agent->facing_yaw), 0.0f,
                          -sinf(agent->facing_yaw)};
    }
    float blade_length = agent->combat.team == CC_COMBAT_PLAYER ? 0.86f :
                                                                    0.70f;
    bool combat_ready = agent->combat.focus_valid ||
        agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
        agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE;
    bool quiet_player_blade = agent->combat.team == CC_COMBAT_PLAYER &&
                              !combat_ready;
    Vector3 guard_center = PhysicsAdd(hand, PhysicsScale(direction, 0.13f));
    Vector3 blade_start = PhysicsAdd(guard_center,
                                     PhysicsScale(direction, 0.035f));
    Vector3 blade_tip = PhysicsAdd(blade_start,
                                   PhysicsScale(direction, blade_length));
    Vector3 pommel = PhysicsAdd(hand, PhysicsScale(direction, -0.17f));
    Color grip = agent->combat.team == CC_COMBAT_PLAYER ?
        (Color){66, 45, 43, 255} : (Color){47, 39, 42, 255};
    Color guard_color = agent->combat.team == CC_COMBAT_PLAYER ?
        WORLD_GOLD : (Color){164, 116, 65, 255};
    Color steel = CombatIsDefeated(&agent->combat) ?
                  (Color){95, 101, 101, 255} :
                  quiet_player_blade ? (Color){118, 139, 139, 255} :
                  (Color){196, 211, 212, 255};

    DrawCylinderEx(pommel, guard_center, 0.032f, 0.032f, 7, grip);
    DrawSmallSphere(pommel, 0.045f, guard_color);
    DrawCylinderEx(PhysicsAdd(guard_center, PhysicsScale(right, -0.14f)),
                   PhysicsAdd(guard_center, PhysicsScale(right, 0.14f)),
                   0.032f, 0.026f, 7, guard_color);
    DrawCylinderEx(blade_start, blade_tip,
                   quiet_player_blade ? 0.034f : 0.052f,
                   0.012f, 5, steel);
    DrawLine3D(blade_start, blade_tip,
               agent->combat.team == CC_COMBAT_PLAYER ? WORLD_GOLD :
                                                        WORLD_INK);

    if (agent->combat.weapon_mode == CC_WEAPON_HELD &&
        agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
        agent->humanoid.action_time > 0.18f &&
        agent->humanoid.action_time < 0.84f) {
        Vector3 previous_direction = CombatWeaponDirectionAt(
            agent, fmaxf(0.0f, agent->humanoid.action_time - 0.075f));
        Vector3 previous_tip = PhysicsAdd(
            hand, PhysicsScale(previous_direction, blade_length + 0.16f));
        Color trail = agent->combat.team == CC_COMBAT_RAIDER ?
            Fade(WORLD_DANGER, 0.54f) :
            agent->combat.active_skill == CC_COMBAT_SKILL_CRUSHING_BLOW ?
                Fade(WORLD_GOLD, 0.72f) :
            agent->combat.active_skill == CC_COMBAT_SKILL_SUNDER ?
                Fade(WORLD_VIOLET, 0.68f) : Fade(WORLD_TEAL, 0.58f);
        DrawCylinderEx(previous_tip, blade_tip, 0.018f, 0.006f, 5, trail);
        DrawLine3D(previous_tip, blade_tip, trail);
        DrawLine3D(PhysicsLerp(previous_tip, hand, 0.34f),
                   PhysicsLerp(blade_tip, hand, 0.34f), Fade(trail, 0.56f));
    }
}

static void DrawSelectedTarget(const CcLocalAgent *target)
{
    if (target == NULL || !CombatCanAct(&target->combat)) return;
    float surface = target->position.y + 0.035f;
    Vector3 center = {target->position.x, surface, target->position.z};
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){0.0f, 0.0f, -0.48f}),
        (Vector3){1.0f, 0.0f, 0.0f}, 0.34f, 0.085f, WORLD_GOLD);
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){0.0f, 0.0f, 0.48f}),
        (Vector3){-1.0f, 0.0f, 0.0f}, 0.34f, 0.085f, WORLD_DANGER);
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){-0.48f, 0.0f, 0.0f}),
        (Vector3){0.0f, 0.0f, 1.0f}, 0.34f, 0.085f, WORLD_GOLD);
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){0.48f, 0.0f, 0.0f}),
        (Vector3){0.0f, 0.0f, -1.0f}, 0.34f, 0.085f, WORLD_DANGER);
    DrawPaintedOverheadDiamond(
        (Vector3){target->position.x, surface + 2.22f,
                  target->position.z},
        0.13f, WORLD_GOLD);
}

static void PresentTarget(RenderTexture2D target, Rectangle destination)
{
    Rectangle source = {0.0f, 0.0f, (float)target.texture.width,
                        -(float)target.texture.height};
    if (visual_style.grade_ready) {
        BeginShaderMode(visual_style.grade);
        SetShaderValueTexture(visual_style.grade,
                              visual_style.palette_lut_location,
                              visual_style.palette_lut);
        const ArtAtmosphereDefinition *atmosphere =
            &visual_style.presentation_atmosphere;
        SetShaderValue(visual_style.grade,
                       visual_style.grade_exposure_location,
                       &atmosphere->grade_exposure, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.grade,
                       visual_style.grade_shadow_tone_location,
                       &atmosphere->grade_shadow_tone,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.grade,
                       visual_style.grade_highlight_tone_location,
                       &atmosphere->grade_highlight_tone,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.grade,
                       visual_style.grade_chroma_location,
                       &atmosphere->grade_chroma, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.grade,
                       visual_style.grade_practical_glow_location,
                       &visual_style.presentation_practical_glow,
                       SHADER_UNIFORM_FLOAT);
    }
    DrawTexturePro(target.texture, source, destination, (Vector2){0.0f, 0.0f},
                   0.0f, WHITE);
    if (visual_style.grade_ready) EndShaderMode();
}

static Color ArtLightBackground(ArtLightProfileId profile_id)
{
    if (profile_id < 0 || profile_id >= ART_LIGHT_PROFILE_COUNT) {
        profile_id = ART_LIGHT_CLEAR_MARKET;
    }
    ArtAtmosphereDefinition atmosphere = ArtAtmosphereForProfile(profile_id);
    return ArtAtmosphereMixColor(
        ART_LIGHT_PROFILES[profile_id].fog_color,
        atmosphere.fog_color, atmosphere.fog_influence);
}

static float ArtAtmosphereHash(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return (float)(value & UINT32_C(0xffff)) / 65535.0f;
}

static Vector2 ArtAtmospherePoint(float x, float y, float scale_x,
                                  float scale_y, float drift_y)
{
    const float dragon_scale = 0.78f;
    float placed_x = 38.0f + (x - 68.0f) * dragon_scale;
    float placed_y = 8.0f + (y - 40.0f) * dragon_scale + drift_y;
    return (Vector2){placed_x * scale_x, placed_y * scale_y};
}

static void DrawDragonOmenSilhouette(int32_t width, int32_t height,
                                     float clock, float omen)
{
    if (omen <= 0.01f) return;
    float scale_x = (float)width / 630.0f;
    float scale_y = (float)height / 320.0f;
    float drift_y = sinf(clock * 0.08f) * 1.8f;
    Color shadow = Fade(CC_VISUAL_PALETTE.cool_ink, omen * 0.60f);
    Color thin_shadow = Fade(CC_VISUAL_PALETTE.violet.shadow, omen * 0.46f);


    DrawTriangle(ArtAtmospherePoint(278, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(474, 54, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(330, 86, scale_x, scale_y, drift_y),
                 thin_shadow);
    DrawTriangle(ArtAtmospherePoint(246, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(150, 10, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(78, 18, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(244, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(78, 18, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(174, 70, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(272, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(365, 23, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(455, 38, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(272, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(455, 38, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(330, 81, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(168, 61, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(130, 84, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(215, 82, scale_x, scale_y, drift_y),
                 shadow);

    DrawEllipse((int32_t)((38.0f + (250.0f - 68.0f) * 0.78f) * scale_x),
                (int32_t)((8.0f + (73.0f - 40.0f) * 0.78f + drift_y) *
                          scale_y),
                67.0f * scale_x, 12.0f * scale_y, shadow);
    DrawTriangle(ArtAtmospherePoint(185, 66, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(112, 60, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(170, 84, scale_x, scale_y, drift_y),
                 shadow);
    DrawEllipse((int32_t)((38.0f + (108.0f - 68.0f) * 0.78f) * scale_x),
                (int32_t)((8.0f + (68.0f - 40.0f) * 0.78f + drift_y) *
                          scale_y),
                21.0f * scale_x, 11.0f * scale_y, shadow);
    DrawRectangle((int32_t)(38.0f * scale_x),
                  (int32_t)((8.0f + (66.0f - 40.0f) * 0.78f + drift_y) *
                            scale_y),
                  (int32_t)(30.0f * scale_x),
                  (int32_t)(9.0f * scale_y), shadow);

    DrawTriangle(ArtAtmospherePoint(95, 58, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(82, 42, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(108, 59, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(112, 58, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(110, 40, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(123, 61, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(83, 76, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(71, 86, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(104, 77, scale_x, scale_y, drift_y),
                 thin_shadow);

    DrawTriangle(ArtAtmospherePoint(222, 83, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(206, 112, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(240, 86, scale_x, scale_y, drift_y),
                 thin_shadow);
    DrawTriangle(ArtAtmospherePoint(282, 82, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(306, 108, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(298, 80, scale_x, scale_y, drift_y),
                 thin_shadow);
}

static void DrawTargetAtmosphere(RenderTexture2D target, float clock)
{
    ArtAtmosphereDefinition atmosphere =
        visual_style.presentation_atmosphere;
    int32_t width = target.texture.width;
    int32_t height = target.texture.height;

    /* These broad transparent washes are painted into the low-resolution
       world target before the palette pass. They behave like a traditional
       animation background: dusk warms the horizon while moonlight cools
       the upper planes, without adding a smooth full-resolution effect. */
    if (atmosphere.horizon_warmth > 0.01f) {
        float warmth = atmosphere.horizon_warmth;
        int32_t horizon_size = height * 2 / 5;
        Color clear = Fade(WORLD_GOLD, 0.0f);
        Color warm = Fade(WORLD_GOLD, warmth * 0.065f);
        DrawRectangleGradientV(0, horizon_size, width, horizon_size,
                               clear, warm);
        DrawRectangleGradientV(0, horizon_size * 2,
                               width, height - horizon_size * 2,
                               warm, clear);
        DrawTriangle((Vector2){0.0f, (float)height * 0.36f},
                     (Vector2){(float)width * 0.62f,
                               (float)height * 0.58f},
                     (Vector2){(float)width * 0.18f,
                               (float)height * 0.70f},
                     Fade(CC_STYLE_DANGER, warmth * 0.026f));
    }
    if (atmosphere.moonlight > 0.01f) {
        float moonlight = atmosphere.moonlight;
        Color clear = Fade(WORLD_TEAL, 0.0f);
        Color cool = Fade(WORLD_TEAL, moonlight * 0.050f);
        DrawRectangleGradientV(0, 0, width, height * 3 / 4,
                               cool, clear);
        DrawTriangle((Vector2){(float)width * 0.58f, 0.0f},
                     (Vector2){(float)width * 0.94f, 0.0f},
                     (Vector2){(float)width * 0.70f,
                               (float)height * 0.76f},
                     Fade(CC_STYLE_VIOLET, moonlight * 0.030f));
    }

    if (atmosphere.omen > 0.01f) {
        Color cloud = Fade(CC_VISUAL_PALETTE.stone.shadow,
                           atmosphere.omen * 0.20f);
        Color clear_cloud = Fade(cloud, 0.0f);
        Color horizon = Fade(CC_VISUAL_PALETTE.earth.light,
                             atmosphere.omen * 0.13f);
        Color cloud_break = Fade(CC_VISUAL_PALETTE.stone.light,
                                 atmosphere.omen * 0.16f);
        DrawRectangleGradientV(0, 0, width, height / 2,
                               cloud, clear_cloud);
        DrawRectangleGradientV(0, height / 3, width, height / 5,
                               Fade(horizon, 0.0f), horizon);
        DrawRectangleGradientV(0, height / 3 + height / 5,
                               width, height / 7,
                               horizon, Fade(horizon, 0.0f));
        DrawEllipse(width / 5, height / 8,
                    (float)width * 0.24f, (float)height * 0.10f, cloud);
        DrawEllipse(width / 2, height / 10,
                    (float)width * 0.31f, (float)height * 0.09f, cloud);
        DrawEllipse(width * 4 / 5, height / 7,
                    (float)width * 0.25f, (float)height * 0.11f, cloud);
        DrawEllipse((int32_t)((float)width * 0.30f),
                    (int32_t)((float)height * 0.12f),
                    (float)width * 0.29f, (float)height * 0.12f,
                    cloud_break);
        DrawDragonOmenSilhouette(width, height, clock, atmosphere.omen);
    }


    if (atmosphere.mist > 0.01f) {
        Color fog = atmosphere.fog_color;
        for (int32_t band = 0; band < 3; ++band) {
            float phase = clock * (0.035f + (float)band * 0.009f) +
                          (float)band * 1.83f;
            int32_t y = (int32_t)((0.30f + (float)band * 0.19f) *
                                  (float)height + sinf(phase) * 7.0f);
            int32_t band_height = 18 + band * 5;
            float opacity = atmosphere.mist *
                            (0.055f - (float)band * 0.009f);
            Color clear = Fade(fog, 0.0f);
            Color visible = Fade(fog, opacity);
            DrawRectangleGradientH(-12, y, width / 2 + 18, band_height,
                                   clear, visible);
            DrawRectangleGradientH(width / 2, y, width / 2 + 18,
                                   band_height, visible, clear);
        }
    }


    int32_t drop_count = (int32_t)roundf(atmosphere.rain * 88.0f);
    for (int32_t drop = 0; drop < drop_count; ++drop) {
        float seed_x = ArtAtmosphereHash(
            UINT32_C(0x91e10da5) + (uint32_t)drop * UINT32_C(0x9e3779b9));
        float seed_y = ArtAtmosphereHash(
            UINT32_C(0x68bc21eb) + (uint32_t)drop * UINT32_C(0x85ebca6b));
        float speed = 74.0f + ArtAtmosphereHash(
            UINT32_C(0x27d4eb2d) + (uint32_t)drop * UINT32_C(0xc2b2ae35)) *
            38.0f;
        float x = fmodf(seed_x * (float)(width + 36) + clock * 18.0f,
                        (float)(width + 36)) - 18.0f;
        float y = fmodf(seed_y * (float)(height + 24) + clock * speed,
                        (float)(height + 24)) - 12.0f;
        float opacity = atmosphere.rain * (0.20f + seed_y * 0.14f);
        Vector2 start = {x, y};
        Vector2 end = {x - 2.4f, y + 7.0f + seed_x * 3.0f};
        DrawLineEx(start, end, seed_x > 0.74f ? 1.4f : 1.0f,
                   Fade(WORLD_METAL_LIGHT, opacity));
    }
}

static void DrawMarketEmberFlames(float clock)
{
    const float lamp_x[] = {3.05f, 8.10f};
    for (int32_t lamp = 0; lamp < 2; ++lamp) {
        float flicker = sinf(clock * 8.3f + (float)lamp * 2.4f) * 0.018f +
                        sinf(clock * 13.7f + (float)lamp) * 0.010f;
        Vector3 flame = {lamp_x[lamp] + flicker * 0.45f,
                         1.84f + flicker, 0.735f};
        DrawCharacterEllipsoid(
            flame, (Vector3){0.095f, 0.19f + flicker, 0.070f},
            CC_STYLE_DANGER);
        DrawCharacterEllipsoid(
            (Vector3){flame.x - flicker * 0.30f, flame.y - 0.025f,
                      flame.z - 0.012f},
            (Vector3){0.054f, 0.125f, 0.044f}, WORLD_GOLD);
        DrawSmallSphere(
            (Vector3){flame.x, flame.y - 0.042f, flame.z - 0.024f},
            0.034f, CC_STYLE_ROAD_LIGHT);
    }
}

static void DrawMarketPracticalLightHalos(Camera3D camera,
                                          RenderTexture2D target,
                                          float clock)
{
    const Vector3 lamps[] = {
        {3.05f, 1.84f, 0.735f}, {8.10f, 1.84f, 0.735f},
    };
    BeginBlendMode(BLEND_ADDITIVE);
    for (int32_t lamp = 0; lamp < 2; ++lamp) {
        Vector2 screen = GetWorldToScreenEx(
            lamps[lamp], camera, target.texture.width, target.texture.height);
        float flicker = 0.92f +
                        sinf(clock * 8.3f + (float)lamp * 2.4f) * 0.08f;
        DrawCircleGradient(
            (Vector2){roundf(screen.x), roundf(screen.y)},
            25.0f * flicker, Fade(CC_STYLE_DANGER, 0.105f),
            Fade(CC_STYLE_DANGER_SHADOW, 0.0f));
        DrawCircleGradient(
            (Vector2){roundf(screen.x), roundf(screen.y)},
            12.0f * flicker, Fade(WORLD_GOLD, 0.135f),
            Fade(CC_STYLE_GOLD_SHADOW, 0.0f));
    }
    EndBlendMode();
}

static ArtLightProfileId StreetLightProfile(
    const CcSettlement *place, ArtLightProfileId authored)
{
    if (place != NULL && place->hunger >= 30) {
        return ART_LIGHT_SHORTAGE_OVERCAST;
    }
    if (place != NULL && place->prosperity >= 60) {
        return ART_LIGHT_RECOVERY_WARM;
    }
    return authored;
}

static void BeginWorldLighting(Camera3D camera,
                               const ArtComposition *composition)
{
    if (!visual_style.world_ready || composition == NULL) return;
    ArtLightProfileId profile_id = composition->light_profile;
    if (profile_id < 0 || profile_id >= ART_LIGHT_PROFILE_COUNT) {
        profile_id = ART_LIGHT_CLEAR_MARKET;
    }
    const ArtLightProfileDefinition *profile =
        &ART_LIGHT_PROFILES[profile_id];
    ArtAtmosphereDefinition atmosphere = ArtAtmosphereForProfile(profile_id);
    visual_style.presentation_atmosphere = atmosphere;
    visual_style.presentation_practical_glow = atmosphere.practical_glow;
    if (profile_id == ART_LIGHT_INTERIOR_EMBER) {
        visual_style.presentation_practical_glow = fmaxf(
            visual_style.presentation_practical_glow, 1.0f);
    } else if (profile_id == ART_LIGHT_ROAD_DUSK) {
        visual_style.presentation_practical_glow = fmaxf(
            visual_style.presentation_practical_glow, 0.52f);
    }
    Vector3 light_direction = Vector3Normalize(
        ArtAtmosphereMixVector(profile->light_direction,
                               atmosphere.light_direction,
                               atmosphere.direction_influence));
    Vector3 camera_forward_vector = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    float direction[3] = {light_direction.x, light_direction.y,
                          light_direction.z};
    float light_color[3] = {
        profile->light_color.x * atmosphere.light_tint.x,
        profile->light_color.y * atmosphere.light_tint.y,
        profile->light_color.z * atmosphere.light_tint.z,
    };
    float ambient_color[3] = {
        profile->ambient_color.x * atmosphere.ambient_tint.x,
        profile->ambient_color.y * atmosphere.ambient_tint.y,
        profile->ambient_color.z * atmosphere.ambient_tint.z,
    };
    float shadow_color[3] = {
        profile->shadow_color.x * atmosphere.shadow_tint.x,
        profile->shadow_color.y * atmosphere.shadow_tint.y,
        profile->shadow_color.z * atmosphere.shadow_tint.z,
    };
    float camera_position[3] = {camera.position.x, camera.position.y,
                                camera.position.z};
    float camera_forward[3] = {camera_forward_vector.x,
                               camera_forward_vector.y,
                               camera_forward_vector.z};
    Color atmosphere_fog = ArtAtmosphereMixColor(
        profile->fog_color, atmosphere.fog_color,
        atmosphere.fog_influence);
    float fog_color[3] = {(float)atmosphere_fog.r / 255.0f,
                          (float)atmosphere_fog.g / 255.0f,
                          (float)atmosphere_fog.b / 255.0f};
    float fog_near = profile->fog_near * atmosphere.fog_distance_scale;
    float fog_far = profile->fog_far * atmosphere.fog_distance_scale;
    float depth_strength = profile->depth_strength * atmosphere.depth_scale;
    float focal_contrast = profile->focal_contrast * atmosphere.focal_scale;
    float character_rim_tint[3] = {
        atmosphere.character_rim_tint.x,
        atmosphere.character_rim_tint.y,
        atmosphere.character_rim_tint.z,
    };
    float character_rim_strength = atmosphere.character_rim_strength;
    float focal_point[3] = {composition->focal_point.x,
                            composition->focal_point.y,
                            composition->focal_point.z};
    float story_axis[2] = {composition->story_axis.x,
                           composition->story_axis.y};
    float foreground_anchor[3] = {composition->foreground_anchor.x,
                                  composition->foreground_anchor.y,
                                  composition->foreground_anchor.z};
    float depth_splits[3] = {composition->depth_splits.x,
                             composition->depth_splits.y,
                             composition->depth_splits.z};
    SetShaderValue(visual_style.world,
                   visual_style.light_direction_location, direction,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.light_color_location,
                   light_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.ambient_color_location,
                   ambient_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world,
                   visual_style.camera_position_location, camera_position,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world,
                   visual_style.camera_forward_location, camera_forward,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.shadow_color_location,
                   shadow_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.fog_color_location,
                   fog_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.fog_near_location,
                   &fog_near, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.fog_far_location,
                   &fog_far, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.focal_point_location,
                   focal_point, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.story_axis_location,
                   story_axis, SHADER_UNIFORM_VEC2);
    SetShaderValue(visual_style.world,
                   visual_style.foreground_anchor_location,
                   foreground_anchor, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.depth_splits_location,
                   depth_splits, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.depth_strength_location,
                   &depth_strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.focal_contrast_location,
                   &focal_contrast, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world,
                   visual_style.weather_wetness_location,
                   &atmosphere.wetness, SHADER_UNIFORM_FLOAT);
    if (visual_style.painted_environment.ready) {
        PaintedEnvironmentStyle *painted =
            &visual_style.painted_environment;
        SetShaderValue(painted->shader, painted->light_direction_location,
                       direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->light_color_location,
                       light_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->ambient_color_location,
                       ambient_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->camera_forward_location,
                       camera_forward, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->fog_color_location,
                       fog_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->fog_near_location,
                       &fog_near, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader, painted->fog_far_location,
                       &fog_far, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader, painted->focal_point_location,
                       focal_point, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->story_axis_location,
                       story_axis, SHADER_UNIFORM_VEC2);
        SetShaderValue(painted->shader, painted->foreground_anchor_location,
                       foreground_anchor, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->depth_splits_location,
                       depth_splits, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->depth_strength_location,
                       &depth_strength, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader, painted->focal_contrast_location,
                       &focal_contrast, SHADER_UNIFORM_FLOAT);
    }
    const float foreground_reveal = 0.0f;
    SetShaderValue(visual_style.world,
                   visual_style.foreground_reveal_location,
                   &foreground_reveal, SHADER_UNIFORM_FLOAT);
    if (visual_style.painted_environment.ready) {
        SetShaderValue(
            visual_style.painted_environment.shader,
            visual_style.painted_environment.foreground_reveal_location,
            &foreground_reveal, SHADER_UNIFORM_FLOAT);
    }
    const float terrain_surface = 0.0f;
    SetShaderValue(visual_style.world,
                   visual_style.terrain_surface_location,
                   &terrain_surface, SHADER_UNIFORM_FLOAT);
    if (visual_style.foliage_ready) {
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_light_direction_location,
                       direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_light_color_location,
                       light_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_ambient_color_location,
                       ambient_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_camera_forward_location,
                       camera_forward, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_fog_color_location,
                       fog_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_fog_near_location,
                       &fog_near, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_fog_far_location,
                       &fog_far, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_depth_splits_location,
                       depth_splits, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_depth_strength_location,
                       &depth_strength, SHADER_UNIFORM_FLOAT);
    }
    if (visual_style.hero_ready) {
        SetShaderValue(visual_style.hero,
                       visual_style.hero_light_direction_location, direction,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_color_location, fog_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_near_location,
                       &fog_near,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_far_location,
                       &fog_far,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_rim_tint_location,
                       character_rim_tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_rim_strength_location,
                       &character_rim_strength, SHADER_UNIFORM_FLOAT);
    }
    if (visual_style.npc_ready) {
        SetShaderValue(visual_style.npc,
                       visual_style.npc_light_direction_location, direction,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_color_location, fog_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_near_location,
                       &fog_near,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_far_location,
                       &fog_far,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_rim_tint_location,
                       character_rim_tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_rim_strength_location,
                       &character_rim_strength, SHADER_UNIFORM_FLOAT);
    }
    if (visual_style.npc_skinned_ready) {
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_light_direction_location,
                       direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_fog_color_location,
                       fog_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_fog_near_location,
                       &fog_near, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_fog_far_location,
                       &fog_far, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_rim_tint_location,
                       character_rim_tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_rim_strength_location,
                       &character_rim_strength, SHADER_UNIFORM_FLOAT);
    }
    BeginShaderMode(visual_style.world);
}

static void EndWorldLighting(void)
{
    if (visual_style.world_ready) EndShaderMode();
}

void CcLocalDrawNpcReview3D(int32_t view, float clock,
                            RenderTexture2D target, Rectangle destination)
{
    Camera3D camera = {0};
    camera.target = (Vector3){0.0f, 1.05f, 0.0f};
    camera.position = (Vector3){0.0f, 2.75f, 7.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 3.05f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    FaceRenderContext previous_face_context = face_render_context;
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(CC_VISUAL_PALETTE.cool_ink);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    CcNpcRole roles[6] = {
        CC_NPC_ROLE_GUARD, CC_NPC_ROLE_RAIDER, CC_NPC_ROLE_LABORER,
        CC_NPC_ROLE_MERCHANT, CC_NPC_ROLE_TRAVELLER, CC_NPC_ROLE_REFUGEE,
    };
    Color accents[6] = {
        WORLD_TEAL, WORLD_DANGER, (Color){174, 94, 53, 255},
        (Color){223, 151, 68, 255}, (Color){117, 145, 116, 255},
        WORLD_GOLD,
    };
    int32_t poses[6] = {-1, -1, -1, -1, -1, -1};
    CcTraversalMode modes[6] = {
        CC_TRAVERSAL_IDLE, CC_TRAVERSAL_IDLE, CC_TRAVERSAL_IDLE,
        CC_TRAVERSAL_IDLE, CC_TRAVERSAL_IDLE, CC_TRAVERSAL_IDLE,
    };
    float yaw = view == 1 ? PI * 0.5f : 0.0f;
    if (view == 2) {
        const CcNpcRole motion_roles[6] = {
            CC_NPC_ROLE_TRAVELLER, CC_NPC_ROLE_SCOUT, CC_NPC_ROLE_GUARD,
            CC_NPC_ROLE_LABORER, CC_NPC_ROLE_MERCHANT, CC_NPC_ROLE_HEALER,
        };
        const int32_t motion_poses[6] = {
            1, 4, 7, CC_NPC_ARCHETYPE_WORK_POSE,
            CC_NPC_ARCHETYPE_WORK_POSE, CC_NPC_ARCHETYPE_WORK_POSE,
        };
        for (int32_t actor = 0; actor < 6; ++actor) {
            roles[actor] = motion_roles[actor];
            poses[actor] = motion_poses[actor];
            modes[actor] = actor < 3 ? CC_TRAVERSAL_WALK :
                                      CC_TRAVERSAL_IDLE;
        }
    } else if (view == 3) {
        const CcNpcRole state_roles[6] = {
            CC_NPC_ROLE_GUARD, CC_NPC_ROLE_GUARD,
            CC_NPC_ROLE_REFUGEE, CC_NPC_ROLE_REFUGEE,
            CC_NPC_ROLE_HEALER, CC_NPC_ROLE_HEALER,
        };
        for (int32_t actor = 0; actor < 6; ++actor) {
            roles[actor] = state_roles[actor];
            poses[actor] = (actor & 1) == 0 ?
                CC_NPC_ARCHETYPE_WORK_POSE :
                CC_NPC_ARCHETYPE_REACT_POSE;
            accents[actor] = (actor & 1) == 0 ? WORLD_TEAL : WORLD_DANGER;
        }
    }
    bool previous_exact_yaw = npc_review_exact_yaw;
    npc_review_exact_yaw = view == 1;
    DrawBox((Vector3){0.0f, -0.08f, 0.0f},
            (Vector3){6.45f, 0.16f, 2.25f}, WORLD_ROAD_SHADOW);
    DrawBox((Vector3){0.0f, 1.30f, -0.72f},
            (Vector3){6.45f, 2.76f, 0.10f}, WORLD_STONE_SHADOW);
    for (int32_t actor = 0; actor < 6; ++actor) {
        float x = -2.58f + (float)actor * 1.03f;
        DrawBox((Vector3){x, 0.015f, 0.0f},
                (Vector3){0.88f, 0.03f, 0.88f},
                view == 3 && (actor & 1) != 0 ?
                    ShadeColor(WORLD_DANGER, 0.58f) :
                    (actor & 1) == 0 ? WORLD_EARTH : WORLD_ROAD);
        DrawNpcFigurePose3D(
            (Vector3){x, 0.04f, 0.0f},
            actor == 0 ? 1.00f : 0.94f, yaw,
            UINT32_C(0x6e706301) + (uint32_t)actor, roles[actor],
            accents[actor], clock * 2.4f + (float)actor * 0.8f,
            modes[actor], poses[actor]);
    }
    npc_review_exact_yaw = previous_exact_yaw;
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    face_render_context = previous_face_context;
    PresentTarget(target, destination);
}

static void PresentCharacterPortrait(Rectangle bounds, Color accent)
{
    DrawRectangle((int32_t)bounds.x, (int32_t)bounds.y,
                  (int32_t)bounds.width, (int32_t)bounds.height,
                  (Color){8, 16, 21, 255});
    Rectangle source = {0.0f, 0.0f,
                        (float)npc_portrait_target.texture.width,
                        -(float)npc_portrait_target.texture.height};
    float available_width = fmaxf(1.0f, bounds.width - 4.0f);
    float available_height = fmaxf(1.0f, bounds.height - 4.0f);
    float source_aspect = (float)npc_portrait_target.texture.width /
                          (float)npc_portrait_target.texture.height;
    float draw_width = fminf(available_width,
                             available_height * source_aspect);
    float draw_height = fminf(available_height,
                              available_width / source_aspect);
    Rectangle inset = {
        bounds.x + (bounds.width - draw_width) * 0.5f,
        bounds.y + (bounds.height - draw_height) * 0.5f,
        draw_width, draw_height,
    };
    DrawTexturePro(npc_portrait_target.texture, source, inset,
                   (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    DrawRectangleLines((int32_t)bounds.x, (int32_t)bounds.y,
                       (int32_t)bounds.width, (int32_t)bounds.height,
                       ShadeColor(accent, 0.88f));
}

void CcLocalDrawNpcPortrait3D(const CcNpcAppearance *appearance,
                              Rectangle bounds,
                              CcNpcPortraitExpression expression)
{
    if (appearance == NULL || bounds.width < 4.0f || bounds.height < 4.0f ||
        !IsRenderTextureValid(npc_portrait_target)) {
        CcNpcDrawPixelPortrait(appearance, bounds, expression, false);
        return;
    }

    FaceRenderContext previous_face_context = face_render_context;
    Camera3D camera = {0};
    camera.target = (Vector3){0.0f, 1.66f, 0.0f};
    camera.position = (Vector3){0.0f, 1.77f, 4.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 0.98f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    rlDrawRenderBatchActive();
    BeginTextureMode(npc_portrait_target);
    ClearBackground((Color){17, 28, 32, 255});
    SetFaceRenderContext(camera, npc_portrait_target.texture.width,
                         npc_portrait_target.texture.height);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    (void)DrawNpcArchetype3D((Vector3){0.0f, 0.0f, 0.0f}, 1.0f,
                             0.0f, 0.0f, CC_TRAVERSAL_IDLE,
                             expression, appearance);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    face_render_context = previous_face_context;

    PresentCharacterPortrait(bounds, appearance->accent);
}

void CcLocalDrawAgentPortrait3D(const CcLocalAgent *agent,
                                Rectangle bounds)
{
    if (agent == NULL || bounds.width < 4.0f || bounds.height < 4.0f ||
        !IsRenderTextureValid(npc_portrait_target)) return;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(pose, &skin);
    if (!skin.valid) return;

    const CcHumanoidSkinBonePose *head_bone =
        &skin.bones[CC_HUMANOID_SKIN_HEAD];
    Vector3 head = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HEAD].position);
    Vector3 head_up = PhysicsNormalizeOr(FromLimbVector(head_bone->up),
                                         (Vector3){0.0f, 1.0f, 0.0f});
    Vector3 head_forward = PhysicsNormalizeOr(
        FromLimbVector(head_bone->forward),
        (Vector3){sinf(agent->facing_yaw), 0.0f,
                  cosf(agent->facing_yaw)});
    Camera3D camera = {0};
    float portrait_drop = agent->crowned ? -0.10f : -0.16f;
    camera.target = Vector3Add(head, Vector3Scale(head_up, portrait_drop));
    camera.position = Vector3Add(
        camera.target,
        Vector3Add(Vector3Scale(head_forward, 4.0f),
                   (Vector3){0.0f, 0.11f, 0.0f}));
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};

    camera.fovy = agent->crowned ? 0.84f : 0.98f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    FaceRenderContext previous_face_context = face_render_context;
    rlDrawRenderBatchActive();
    BeginTextureMode(npc_portrait_target);
    ClearBackground((Color){17, 28, 32, 255});
    SetFaceRenderContext(camera, npc_portrait_target.texture.width,
                         npc_portrait_target.texture.height);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);

    bool drew = false;
    CcNpcAppearance portrait_appearance = agent->appearance;
    if (agent->crowned && screen_first_hero_active) {
        UseCharacterLighting();
        drew = DrawDynamicNpcModules(agent, &skin, &portrait_appearance);
        if (drew) {
            DrawWayfarerHeroDetails(&skin);
        }
        RestoreWorldLighting();
    } else if (agent->crowned) {
        drew = DrawHeroSkin(&skin, &agent->render_cape, WHITE, true, false);
        if (drew) {
            UseCharacterLighting();
            DrawWayfarerHeroDetails(&skin);
            RestoreWorldLighting();
        }
    } else {
        UseCharacterLighting();
        drew = DrawDynamicNpcModules(agent, &skin, &portrait_appearance);
        RestoreWorldLighting();
    }
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    face_render_context = previous_face_context;

    if (drew) {
        PresentCharacterPortrait(bounds, portrait_appearance.accent);
    } else {
        CcNpcDrawPixelPortrait(&portrait_appearance, bounds,
                               CC_NPC_PORTRAIT_NEUTRAL, false);
    }
}

static void DrawMovementPathSegments(
    Vector3 origin, const Vector3 *points, int32_t point_count,
    CcLocalSceneKind scene, Color color, float width, bool dashed)
{
    Vector3 previous = origin;
    for (int32_t index = 0; index < point_count; ++index) {
        Vector3 waypoint = points[index];
        Vector3 along = PhysicsSubtract(waypoint, previous);
        float length = sqrtf(along.x * along.x + along.z * along.z);
        if (length <= 0.08f) {
            previous = waypoint;
            continue;
        }
        if (!dashed) {
            Vector3 center = PhysicsScale(
                PhysicsAdd(previous, waypoint), 0.5f);
            center.y = SurfaceHeightAt(scene, center.x, center.z) + 0.070f;
            DrawGroundBrushStroke(center, along, length, width, color);
        } else {
            const float dash_length = 0.26f;
            const float dash_stride = 0.43f;
            float inverse_length = 1.0f / length;
            for (float distance = 0.0f; distance < length;
                 distance += dash_stride) {
                float end_distance = fminf(length, distance + dash_length);
                float center_distance = (distance + end_distance) * 0.5f;
                float amount = center_distance * inverse_length;
                Vector3 center = {
                    previous.x + along.x * amount,
                    0.0f,
                    previous.z + along.z * amount,
                };
                center.y = SurfaceHeightAt(scene, center.x, center.z) + 0.120f;
                DrawGroundBrushStroke(
                    center, along, end_distance - distance, width, color);
            }
        }
        previous = waypoint;
    }
}

static void DrawMovementGroundReticle(Vector3 point, bool accepted,
                                       bool adjusted, bool active)
{
    float pulse = 1.0f + 0.08f * sinf((float)GetTime() * 8.0f);
    float radius = (active ? 0.36f : 0.32f) * pulse;
    Color color = !accepted ? WORLD_DANGER :
                  adjusted ? WORLD_VIOLET :
                  active ? WORLD_GOLD : WORLD_TEAL;
    point.y += active ? 0.090f : 0.150f;
    DrawCylinder(point, radius, radius, 0.025f, 24,
                 Fade(color, active ? 0.32f : 0.22f));
    rlSetLineWidth(active ? 3.0f : 2.0f);
    DrawCircle3D(point, radius, (Vector3){1.0f, 0.0f, 0.0f}, 90.0f,
                 Fade(color, active ? 1.0f : 0.92f));
    DrawCircle3D(point, radius * 0.70f,
                 (Vector3){1.0f, 0.0f, 0.0f}, 90.0f,
                 Fade(color, active ? 0.74f : 0.60f));
    rlSetLineWidth(1.0f);
    DrawSphere(point, active ? 0.105f : 0.085f, color);
    if (!accepted) {
        const Vector3 slash_a = {0.55f, 0.0f, 0.55f};
        const Vector3 slash_b = {0.55f, 0.0f, -0.55f};
        DrawGroundBrushStroke(point, slash_a, 0.55f, 0.065f, color);
        DrawGroundBrushStroke(point, slash_b, 0.55f, 0.065f, color);
    } else {
        const float tick_offset = radius + 0.09f;
        const Vector3 tick_x = {0.20f, 0.0f, 0.0f};
        const Vector3 tick_z = {0.0f, 0.0f, 0.20f};
        DrawGroundBrushStroke(
            (Vector3){point.x - tick_offset, point.y, point.z},
            tick_x, 0.20f, 0.050f, color);
        DrawGroundBrushStroke(
            (Vector3){point.x + tick_offset, point.y, point.z},
            tick_x, 0.20f, 0.050f, color);
        DrawGroundBrushStroke(
            (Vector3){point.x, point.y, point.z - tick_offset},
            tick_z, 0.20f, 0.050f, color);
        DrawGroundBrushStroke(
            (Vector3){point.x, point.y, point.z + tick_offset},
            tick_z, 0.20f, 0.050f, color);
    }
}

static void DrawAgentPath(const CcLocalAgent *agent, bool market_interior)
{
    if (agent == NULL) return;
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    const CcLocalMovementPreview *preview = &renderer_movement_preview;
    if (preview->valid && preview->scene == scene &&
        preview->world_target == CC_LOCAL_WORLD_TARGET_NONE) {
        Vector3 hover_target = preview->accepted ? preview->resolved_point :
                                                   preview->requested_point;
        if (preview->accepted && preview->path_count > 0) {
            DrawMovementPathSegments(
                agent->position, preview->path, preview->path_count, scene,
                Fade(preview->adjusted ? WORLD_VIOLET : WORLD_TEAL, 0.82f),
                0.085f, true);
        }
        DrawMovementGroundReticle(
            hover_target, preview->accepted, preview->adjusted, false);
    }

    if (!agent->command_point_valid ||
        (!agent->exact_target_valid && !agent->navigation_active)) return;
    if (agent->navigation_active) {
        DrawMovementPathSegments(
            agent->position,
            &agent->navigation_point[agent->navigation_point_index],
            agent->navigation_point_count - agent->navigation_point_index,
            scene, Fade(WORLD_GOLD, 0.72f), 0.082f, false);
    } else {
        DrawMovementPathSegments(
            agent->position, &agent->command_point, 1, scene,
            Fade(WORLD_GOLD, 0.64f), 0.075f, false);
    }
    DrawMovementGroundReticle(agent->command_point, true, false, true);
}

static Color CoursePlatformColor(int32_t style)
{
    switch (style) {
        case 1: return (Color){87, 113, 105, 255};
        case 2: return (Color){98, 79, 118, 255};
        case 3: return (Color){126, 82, 74, 255};
        default: return (Color){96, 121, 111, 255};
    }
}

static void DrawObstacleCourse(Vector3 focus)
{
    if (!SceneryFootprintVisible(COURSE_SCENERY_FOOTPRINT, focus)) return;
    float pool_center_x = COURSE_POOL.x + COURSE_POOL.width * 0.5f;
    float pool_center_z = COURSE_POOL.y + COURSE_POOL.height * 0.5f;
    DrawBox((Vector3){pool_center_x, 0.36f, pool_center_z},
            (Vector3){COURSE_POOL.width, 0.72f, COURSE_POOL.height},
            (Color){19, 57, 73, 255});
    DrawBox((Vector3){pool_center_x, COURSE_WATER_SURFACE, pool_center_z},
            (Vector3){COURSE_POOL.width - 0.10f, 0.035f,
                      COURSE_POOL.height - 0.10f},
            (Color){48, 151, 167, 205});
    DrawBox((Vector3){pool_center_x, 0.43f, COURSE_POOL.y - 0.06f},
            (Vector3){COURSE_POOL.width + 0.18f, 0.86f, 0.12f},
            (Color){86, 104, 102, 255});
    DrawBox((Vector3){pool_center_x, 0.43f,
                      COURSE_POOL.y + COURSE_POOL.height + 0.06f},
            (Vector3){COURSE_POOL.width + 0.18f, 0.86f, 0.12f},
            (Color){86, 104, 102, 255});
    for (int32_t i = 0; i < (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                      sizeof(COURSE_WAYPOINTS[0])); ++i) {
        Vector3 point = COURSE_WAYPOINTS[i];
        point.y = SurfaceHeightAt(CC_LOCAL_SCENE_STREET,
                                  point.x, point.z) + 0.035f;
        DrawCylinder(point, 0.075f, 0.075f, 0.025f, 10,
                     Fade(WORLD_GOLD, (i & 1) != 0 ? 0.52f : 0.30f));
    }
    DrawBox((Vector3){9.32f, 0.76f, 1.08f}, (Vector3){0.08f, 1.52f, 0.08f},
            (Color){72, 58, 55, 255});
    DrawBox((Vector3){9.32f, 0.76f, 1.82f}, (Vector3){0.08f, 1.52f, 0.08f},
            (Color){72, 58, 55, 255});
    DrawBox((Vector3){9.32f, 1.48f, 1.45f}, (Vector3){0.08f, 0.08f, 0.82f},
            WORLD_GOLD);
    DrawBox((Vector3){9.28f, 1.20f, 1.45f}, (Vector3){0.035f, 0.42f, 0.52f},
            (Color){73, 55, 91, 255});
}

static void DrawCourseRaider(const CcLocalAgent *raider)
{
    if (raider == NULL) return;
    DrawCombatFootprint(raider, WORLD_DANGER);
    DrawRobotShell(raider);
    DrawCombatSword(raider);
    DrawCombatSkillTell(raider);
    DrawCombatImpact(raider);
    DrawPaintedOverheadDiamond(
        (Vector3){raider->position.x,
                  raider->position.y + 2.05f,
                  raider->position.z},
        0.085f, WORLD_DANGER);
}

static void DrawCourseRunners(const CcLocalCourse *course, Vector3 focus)
{
    if (course == NULL) return;
    if (course->situation_witness_active &&
        SceneryPointVisible(course->situation_witness.position.x,
                            course->situation_witness.position.z, focus)) {
        DrawRobotShell(&course->situation_witness);
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        const CcLocalTraveller *traveller = &course->travellers[i];
        if (traveller->active &&
            SceneryPointVisible(traveller->agent.position.x,
                                traveller->agent.position.z, focus)) {
            DrawRobotShell(&traveller->agent);
        }
    }
    Vector3 threat = CourseThreatCenter(course);
    if (course->alarm_active) {
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            const CcLocalAgent *raider = &course->raiders[i];
            if (SceneryPointVisible(raider->position.x,
                                    raider->position.z, focus)) {
                DrawCourseRaider(raider);
            }
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const CcLocalCourseRunner *runner = &course->runners[i];
        bool runner_visible = SceneryPointVisible(runner->agent.position.x,
                                                   runner->agent.position.z,
                                                   focus);
        if (!runner_visible) {
            if (runner->agent.exact_target_valid) {
                Vector3 target = runner->agent.target_point;
                if (SceneryPointVisible(target.x, target.z, focus)) {
                    DrawGroundBrushStroke(
                        (Vector3){target.x, target.y + 0.025f, target.z},
                        (Vector3){1.0f, 0.0f, 0.45f}, 0.32f, 0.075f,
                        Fade(runner->marker_color, 0.58f));
                }
            }
            continue;
        }
        if (course->alarm_active) {
            DrawCombatFootprint(&runner->agent, runner->marker_color);
        }
        if (runner->agent.exact_target_valid) {
            Vector3 target = runner->agent.target_point;
            DrawGroundBrushStroke(
                (Vector3){target.x, target.y + 0.025f, target.z},
                (Vector3){1.0f, 0.0f, 0.45f}, 0.32f, 0.075f,
                Fade(runner->marker_color, 0.58f));
        }
        DrawRobotShell(&runner->agent);
        DrawCombatImpact(&runner->agent);
        if (!runner->agent.climbing &&
            !runner->agent.humanoid.ragdoll.active) {
            const CcHumanoidPose *pose = AgentRenderPose(&runner->agent);
            Vector3 hand = FromLimbVector(pose->hand[1]);
            Vector3 elbow = FromLimbVector(pose->elbow[1]);
            Vector3 aim = {sinf(runner->agent.facing_yaw), 0.0f,
                           cosf(runner->agent.facing_yaw)};
            if (course->alarm_active) {
                aim = PhysicsNormalizeOr(
                    PhysicsSubtract(threat, hand), aim);
                aim.y = runner->duty == CC_GUARD_ENGAGED ? 0.06f : 0.16f;
                aim = PhysicsNormalizeOr(aim,
                    (Vector3){0.0f, 0.0f, 1.0f});
            }
            Vector3 forearm = PhysicsNormalizeOr(
                PhysicsSubtract(hand, elbow), aim);
            float arm_weight = runner->duty == CC_GUARD_ENGAGED ? 0.78f : 0.42f;
            Vector3 spear_direction = PhysicsNormalizeOr(
                PhysicsLerp(aim, forearm, arm_weight), aim);
            float spear_length = runner->duty == CC_GUARD_ENGAGED ? 0.78f :
                                                                         0.58f;
            Vector3 spear_tip = PhysicsAdd(
                hand, PhysicsScale(spear_direction, spear_length));
            DrawCylinderEx(hand, spear_tip, 0.022f, 0.013f, 7,
                           (Color){128, 92, 55, 255});
            DrawSmallSphere(spear_tip, 0.038f, WORLD_GOLD);
            Vector3 shield_hand = FromLimbVector(pose->hand[0]);
            DrawCharacterSphere(shield_hand, 0.105f,
                                (Color){55, 74, 78, 255});
            DrawSmallSphere(shield_hand, 0.055f, runner->marker_color);
        }
        DrawPaintedOverheadDiamond(
            (Vector3){runner->agent.position.x,
                      runner->agent.position.y + 2.05f,
                      runner->agent.position.z},
            0.085f, runner->marker_color);
    }
}
