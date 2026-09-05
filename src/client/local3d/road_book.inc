static const char *RoadArchetypeName(const CcRoute *route)
{
    if (route == NULL) return "UNKNOWN ROAD";
    if (route->smuggler_route) return "HIDDEN WOODLAND TRACK";
    if (route->closed) return "BROKEN CAUSEWAY";
    if (route->condition < 48) return "RUTTED FRONTIER ROAD";
    if (route->security >= 70) return "PATROLLED KING'S ROAD";
    return "HEDGEROW TRADE ROAD";
}

float CcLocalRoadCheckpointSurfaceYInternal(float x, float z)
{

    Vector2 point = {x, z};
    if (CheckCollisionPointRec(point, ROAD_BRIDGE_DECK_SUPPORT)) {
        return 0.56f;
    }
    if (CheckCollisionPointRec(point,
                               ROAD_BRIDGE_WEST_CAUSEWAY_SUPPORT) ||
        CheckCollisionPointRec(point,
                               ROAD_BRIDGE_EAST_CAUSEWAY_SUPPORT)) {
        return 0.10f;
    }
    return 0.0f;
}

float CcLocalRoadHorseLateralSpacingInternal(bool bridge_checkpoint)
{

    return bridge_checkpoint ? 0.78f : 1.05f;
}

float CcLocalRoadHorseLongitudinalOffsetInternal(void)
{

    return 5.55f;
}

static Vector3 RoadPonySkinPoint(Vector3 base, float yaw, Vector3 point,
                                 CcQuadrupedBone part)
{
    const CreatureModelCache *pony =
        &creature_models[CC_CREATURE_HORSE][CC_CREATURE_POSE_IDLE];
    if (pony->ready) {
        for (int32_t bone = 0; bone < pony->model.skeleton.boneCount; ++bone) {
            if (pony->quadruped_bone[bone] != (int32_t)part) continue;
            Transform bind = pony->model.skeleton.bindPose[bone];
            Transform pose = pony->pose[bone];
            Quaternion rotation = QuaternionMultiply(
                pose.rotation, QuaternionInvert(bind.rotation));
            point = Vector3Add(pose.translation, Vector3RotateByQuaternion(
                Vector3Subtract(point, bind.translation), rotation));
            break;
        }
    }
    return LocalPoint(base, point.x * 0.96f, point.y * 0.96f,
                       point.z * 0.96f, yaw);
}

static void DrawRoadHorseHarness(Vector3 base, float yaw)
{
    Color leather = (Color){55, 35, 28, 255};
    Color leather_edge = (Color){91, 57, 38, 255};
    Color buckle = (Color){172, 137, 78, 255};
    Vector3 collar_top = RoadPonySkinPoint(
        base, yaw, (Vector3){0.0f, 1.57f, 0.65f}, CC_QUADRUPED_NECK);
    Vector3 nose_ends[2];
    Vector3 brow_ends[2];
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 collar_side = RoadPonySkinPoint(
            base, yaw, (Vector3){(float)side * 0.44f, 1.19f, 0.64f},
            CC_QUADRUPED_CHEST);
        Vector3 shoulder = RoadPonySkinPoint(
            base, yaw, (Vector3){(float)side * 0.47f, 1.15f, 0.08f},
            CC_QUADRUPED_CHEST);
        DrawCylinderEx(collar_top, collar_side, 0.034f, 0.030f, 6,
                       leather_edge);
        DrawCylinderEx(collar_side, shoulder, 0.026f, 0.022f, 6, leather);
        Vector3 cheek = RoadPonySkinPoint(
            base, yaw, (Vector3){(float)side * 0.30f, 1.65f, 1.07f},
            CC_QUADRUPED_HEAD);
        Vector3 bit_end = RoadPonySkinPoint(
            base, yaw, (Vector3){(float)side * 0.26f, 1.27f, 1.39f},
            CC_QUADRUPED_HEAD);
        DrawCylinderEx(cheek, bit_end, 0.020f, 0.017f, 6, leather);
        DrawCharacterSphere(bit_end, 0.027f, buckle);
        brow_ends[side < 0 ? 0 : 1] = cheek;
        nose_ends[side < 0 ? 0 : 1] = bit_end;
    }
    Vector3 brow_front = RoadPonySkinPoint(
        base, yaw, (Vector3){0.0f, 1.66f, 1.20f}, CC_QUADRUPED_HEAD);
    Vector3 nose_front = RoadPonySkinPoint(
        base, yaw, (Vector3){0.0f, 1.29f, 1.56f}, CC_QUADRUPED_HEAD);
    for (int32_t side = 0; side < 2; ++side) {
        DrawCylinderEx(brow_ends[side], brow_front, 0.018f, 0.018f, 6, leather);
        DrawCylinderEx(nose_ends[side], nose_front, 0.016f, 0.016f, 6, leather);
    }
}

static void DrawRoadHorseTeam(const CcSim *sim, Vector3 base, float clock, float pace,
                              float yaw, bool bridge_checkpoint)
{
    CcClientConvoyGait convoy_gait = CcClientConvoyGaitForPace(pace);
    CcCreatureRigGait rig_gait =
        convoy_gait == CC_CLIENT_CONVOY_GAIT_CANTER ?
            CC_CREATURE_RIG_GAIT_CANTER :
        convoy_gait == CC_CLIENT_CONVOY_GAIT_TROT ?
            CC_CREATURE_RIG_GAIT_TROT : CC_CREATURE_RIG_GAIT_WALK;
    bool moving = convoy_gait != CC_CLIENT_CONVOY_GAIT_HALT;
    float cadence = CcClientConvoyGaitCadence(convoy_gait);
    float forward_speed = moving ? 0.50f + pace * 1.45f : 0.0f;
    CcCreaturePose left_pose = CcCreatureSteppedPose(
        CC_CREATURE_HORSE, clock * cadence, moving);
    CcCreaturePose right_pose = CcCreatureSteppedPose(
        CC_CREATURE_HORSE, clock * cadence + PI, moving);
    float lateral_spacing =
        CcLocalRoadHorseLateralSpacingInternal(bridge_checkpoint);
    for (int32_t horse = -1; horse <= 1; horse += 2) {
        Color coat = PONY_COLORS[sim->pony_company.team[horse < 0 ? 0 : 1]][0];
        Vector3 horse_base = LocalPoint(
            base, (float)horse * lateral_spacing, 0.0f,
            CcLocalRoadHorseLongitudinalOffsetInternal(), yaw);
        if (bridge_checkpoint) {
            float carriage_support =
                CcLocalRoadCheckpointSurfaceYInternal(base.x, base.z);
            float horse_support =
                CcLocalRoadCheckpointSurfaceYInternal(
                    horse_base.x, horse_base.z);
            horse_base.y += horse_support - carriage_support;
        } else if (active_world_stream != NULL) {
            horse_base.y += CcLocalTerrainHeightAt(horse_base.x, horse_base.z) -
                             CcLocalTerrainHeightAt(base.x, base.z);
        }
        CcCreaturePose pose = horse < 0 ? left_pose : right_pose;
        float gait_phase = clock * cadence + (horse < 0 ? 0.0f : PI);
        CcCreatureRigPose controlled_pose;
        CreatureGaitSlot slot = horse < 0 ? CREATURE_GAIT_ROAD_HORSE_LEFT :
                                            CREATURE_GAIT_ROAD_HORSE_RIGHT;
        bool controlled = ResolveControlledCreatureGait(
            slot, CC_CREATURE_RIG_HORSE, rig_gait, clock,
            horse < 0 ? 0.0f : 0.5f, forward_speed, moving,
            horse_base, yaw, CC_LOCAL_SCENE_ROAD,
            &controlled_pose);
        (void)DrawCreatureGait3D(
            CC_CREATURE_HORSE, pose, horse_base,
            yaw, 0.96f, coat, gait_phase, moving,
            controlled ? &controlled_pose : NULL);
        DrawRoadHorseHarness(horse_base, yaw);
        Vector3 trace_start = LocalPoint(
            base, (float)horse * 0.42f, 0.77f, 3.05f, yaw);
        Vector3 trace_end = LocalPoint(horse_base, 0.0f, 0.91f, -0.52f, yaw);
        DrawCylinderEx(trace_start, trace_end, 0.020f, 0.016f, 6,
                       WORLD_WOOD_SHADOW);
        DrawOrientedBox(horse_base, (Vector3){0.0f, 1.02f, 0.18f},
                        (Vector3){0.66f, 0.055f, 0.055f}, yaw,
                        WORLD_WOOD_SHADOW);
    }
}

static Camera3D PonyConversationFraming(Camera3D road, Vector3 carriage,
                                        float yaw, float weight)
{
    if (weight <= 0.0f) return road;
    Vector3 pony = LocalPoint(carriage, 2.7f, 0.0f, 3.3f, yaw);
    pony.y = CcLocalTerrainHeightAt(pony.x, pony.z);
    Vector3 focus = Vector3Add(pony, (Vector3){0, 0.78f, 0});
    Vector3 position = LocalPoint(pony, 2.8f, 2.2f, 5.0f, yaw - 0.6f);
    float span = road.projection == CAMERA_PERSPECTIVE ?
        2.0f * tanf(road.fovy * DEG2RAD * 0.5f) * Vector3Distance(road.position, road.target) : road.fovy;
    road.position = Vector3Lerp(road.position, position, weight);
    road.target = Vector3Lerp(road.target, focus, weight);
    span += (3.4f - span) * weight;
    road.fovy = road.projection == CAMERA_PERSPECTIVE ?
        2.0f * atanf(span * 0.5f / fmaxf(0.25f,
            Vector3Distance(road.position, road.target))) * RAD2DEG : span;
    return road;
}

static Camera3D PonyConversationCamera(Camera3D road, Vector3 carriage, float yaw, const CcSim *sim)
{
    static float nearby_weight;
    float nearby = CcSimJourneyRoadSiteStop(sim) != NULL ? 1.0f : 0.0f;
    float easing = fminf(1.0f, GetFrameTime() * 2.0f);
    nearby_weight = reduced_motion_enabled ? nearby : nearby_weight + (nearby - nearby_weight) * easing;
    road.fovy *= 1.0f - 0.12f * nearby_weight;
    float target = pony_conversation_active ? 1.0f : 0.0f;
    float step = fminf(GetFrameTime(), 0.1f) * 4.0f;
    if (reduced_motion_enabled) pony_conversation_weight = target;
    else if (pony_conversation_weight < target) pony_conversation_weight = fminf(target, pony_conversation_weight + step);
    else pony_conversation_weight = fmaxf(target, pony_conversation_weight - step);
    return PonyConversationFraming(road, carriage, yaw,
                                   SmoothStep01(pony_conversation_weight));
}

static void DrawRoadCarriage(Vector3 base, const CcSim *sim,
                             float clock, float pace, float yaw,
                             bool horses_hitched, bool bridge_checkpoint)
{
    const CcPlayerCompany *player = &sim->player;
    float asset_yaw_degrees = yaw * RAD2DEG - 90.0f;
    bool follow_ground = active_world_stream != NULL && !bridge_checkpoint;
    if (follow_ground) {
        Vector3 front = LocalPoint(base, 0.0f, 0.0f, 1.38f, yaw);
        Vector3 rear = LocalPoint(base, 0.0f, 0.0f, -1.38f, yaw);
        float pitch = atan2f(
            CcLocalTerrainHeightAt(rear.x, rear.z) -
            CcLocalTerrainHeightAt(front.x, front.z), 2.76f);
        float sway = pace > 0.02f ? sinf(clock * 2.4f) * 0.7f : 0.0f;
        rlPushMatrix();
        rlTranslatef(base.x, base.y, base.z);
        rlRotatef(pitch * RAD2DEG, cosf(yaw), 0.0f, -sinf(yaw));
        rlRotatef(sway, sinf(yaw), 0.0f, cosf(yaw));
        rlTranslatef(-base.x, -base.y, -base.z);
    }
    RuntimeAsset *carriage = &runtime_assets[RUNTIME_ASSET_CARRIAGE];
    if (carriage->ready) {
        static float wheel_phase[2] = {0};
        static float last_wheel_clock = 0.0f;
        float step = fmaxf(0.0f, fminf(0.1f, clock - last_wheel_clock));
        wheel_phase[0] = fmodf(wheel_phase[0] + step * pace * 2.2f / 0.80f, 2.0f * PI);
        wheel_phase[1] = fmodf(wheel_phase[1] + step * pace * 2.2f / 0.62f, 2.0f * PI);
        last_wheel_clock = clock;
        for (int mesh = 0; mesh < carriage->model.meshCount; ++mesh) {
            Model part = carriage->model;
            part.meshCount = 1;
            part.meshes = &carriage->model.meshes[mesh];
            part.meshMaterial = &carriage->model.meshMaterial[mesh];
            int wheel = mesh < 144 ? (int)carriage_wheel_mesh[mesh] - 1 : -1;
            if (wheel >= 0) {
                Vector3 center = carriage_wheel_centers[wheel];
                Matrix spin = MatrixMultiply(
                    MatrixTranslate(-center.x, -center.y, -center.z),
                    MatrixRotateZ(-wheel_phase[wheel < 2 ? 0 : 1]));
                spin = MatrixMultiply(spin, MatrixTranslate(center.x, center.y, center.z));
                part.transform = MatrixMultiply(spin, part.transform);
            }
            DrawModelEx(part, base, (Vector3){0.0f, 1.0f, 0.0f},
                        asset_yaw_degrees,
                        (Vector3){CARRIAGE_ASSET_SCALE, CARRIAGE_ASSET_SCALE,
                                  CARRIAGE_ASSET_SCALE}, WHITE);
        }
    } else {
        DrawOrientedBox(base, (Vector3){0.0f, 1.10f, 0.0f},
                        (Vector3){2.55f, 1.58f, 4.15f}, yaw,
                        BlendColor(WORLD_EARTH, WORLD_DANGER, 0.28f));
        DrawOrientedBox(base, (Vector3){0.0f, 1.98f, 0.0f},
                        (Vector3){2.78f, 0.18f, 4.38f}, yaw,
                        WORLD_GOLD);
        DrawOrientedBox(base, (Vector3){0.0f, 1.27f, 2.10f},
                        (Vector3){1.30f, 0.72f, 0.06f}, yaw, WORLD_TEAL);
        static const Vector2 wheel_offsets[4] = {
            {-1.36f, -1.38f}, {1.36f, -1.38f},
            {-1.36f, 1.38f}, {1.36f, 1.38f}
        };
        for (int32_t i = 0; i < 4; ++i) {
            Vector3 wheel = LocalPoint(base, wheel_offsets[i].x, 0.58f,
                                       wheel_offsets[i].y, yaw);
            DrawScenerySphere(wheel, 0.53f, WORLD_WOOD_SHADOW);
            DrawSphereWires(wheel, 0.55f, 7, 7, WORLD_GOLD);
        }
        Vector3 pole_left = LocalPoint(base, -0.48f, 0.82f, 2.10f, yaw);
        Vector3 pole_right = LocalPoint(base, 0.48f, 0.82f, 2.10f, yaw);
        Vector3 pole_left_end = LocalPoint(base, -0.48f, 0.72f, 4.10f, yaw);
        Vector3 pole_right_end = LocalPoint(base, 0.48f, 0.72f, 4.10f, yaw);
        DrawCylinderEx(pole_left, pole_left_end, 0.045f, 0.035f, 7,
                       WORLD_WOOD);
        DrawCylinderEx(pole_right, pole_right_end, 0.045f, 0.035f, 7,
                       WORLD_WOOD);
    }
    DrawCarriedCargoBoxes(player, base, yaw);
    if (follow_ground) rlPopMatrix();
    if (horses_hitched) {
        DrawRoadHorseTeam(sim, base, clock, pace, yaw, bridge_checkpoint);
        int32_t pony = CcPonyOnRoad(sim);
        if (pony >= 0) {
            Vector3 position = LocalPoint(base, 2.7f, 0.0f, 3.3f, yaw);
            if (follow_ground) position.y = CcLocalTerrainHeightAt(position.x, position.z);
            (void)DrawCreatureGait3D(CC_CREATURE_HORSE, CC_CREATURE_POSE_IDLE,
                position, yaw - 0.6f, 0.96f, PONY_COLORS[pony][0], clock * 0.35f, false, NULL);
        }
    }
}

static void DrawStableHorseTeam(const CcSim *sim, float clock)
{
    static const Vector2 stalls[] = {{40.55f, 30.25f}, {40.55f, 33.05f}};
    Color rail = WORLD_WOOD_SHADOW;
    Color rail_cap = WORLD_WOOD_LIGHT;
    switch (ActiveTownRoadSurface()) {
        case CC_LOCAL_ROAD_FARM_TRACK:
            rail_cap = WORLD_CROP_LIGHT;
            break;
        case CC_LOCAL_ROAD_INDUSTRIAL:
            rail = WORLD_METAL_SHADOW;
            rail_cap = WORLD_METAL_LIGHT;
            break;
        case CC_LOCAL_ROAD_TRADE:
            rail_cap = WORLD_GOLD;
            break;
        case CC_LOCAL_ROAD_MILITARY:
            rail = WORLD_STONE_SHADOW;
            rail_cap = WORLD_STONE_LIGHT;
            break;
        case CC_LOCAL_ROAD_PROCESSIONAL:
            rail = WORLD_STONE;
            rail_cap = WORLD_GOLD;
            break;
        case CC_LOCAL_ROAD_EXPEDITION:
            rail_cap = WORLD_VIOLET;
            break;
    }
    for (int32_t horse = 0; horse < 2; ++horse) {
        Vector3 base = TerrainWorldPoint(stalls[horse].x, stalls[horse].y);
        Color coat = PONY_COLORS[sim->pony_company.team[horse]][0];
        CcCreaturePose pose = CcCreatureSteppedPose(
            CC_CREATURE_HORSE, clock * 0.35f + (float)horse * PI, false);
        (void)DrawCreatureGait3D(
            CC_CREATURE_HORSE, pose, base, 0.5f * PI, 0.96f, coat,
            clock * 0.35f + (float)horse * PI, false, NULL);
    }
    float rail_y = CcLocalTerrainHeightAt(39.45f, 31.65f);
    DrawBox((Vector3){39.45f, rail_y + 0.72f, 31.65f},
            (Vector3){0.12f, 1.44f, 4.15f}, rail);
    DrawBox((Vector3){39.45f, rail_y + 1.12f, 31.65f},
            (Vector3){0.18f, 0.12f, 4.25f}, rail_cap);
}

static bool ForkRouteLeaves(const CcSim *sim, const CcRoute *route)
{
    return sim != NULL && route != NULL &&
           (route->from_id == sim->player.location_id ||
            route->to_id == sim->player.location_id);
}

Vector2 CcLocalForkBranchEndInternal(int32_t branch_ordinal,
                                     int32_t branch_count)
{
    if (branch_count < 1) branch_count = 1;
    if (branch_ordinal < 0) branch_ordinal = 0;
    if (branch_ordinal >= branch_count) branch_ordinal = branch_count - 1;
    float amount = branch_count == 1 ? 0.5f :
        (float)branch_ordinal / (float)(branch_count - 1);
    float spread = fminf(1.44f, 0.48f * (float)(branch_count - 1));
    float angle = -0.5f * spread + amount * spread;
    return (Vector2){50.5f + cosf(angle) * 42.0f,
                     40.0f + sinf(angle) * 40.0f};
}

static void DrawForkRoadSegment(Vector3 from, Vector3 to, float width,
                                Color color, bool rutted)
{
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float length = sqrtf(dx * dx + dz * dz);
    if (length <= 0.01f) return;
    float yaw = atan2f(dx, dz);
    DrawOrientedBox(from, (Vector3){0.0f, 0.015f, length * 0.5f},
                    (Vector3){width + 0.55f, 0.055f, length}, yaw,
                    ShadeColor(color, 0.58f));
    DrawOrientedBox(from, (Vector3){0.0f, 0.050f, length * 0.5f},
                    (Vector3){width, 0.045f, length}, yaw, color);
    if (!rutted) return;
    for (int32_t side = -1; side <= 1; side += 2) {
        DrawOrientedBox(
            from,
            (Vector3){(float)side * width * 0.22f, 0.078f,
                      length * 0.5f},
            (Vector3){0.13f, 0.012f, length}, yaw,
            Fade((Color){45, 35, 29, 255}, 0.72f));
    }
}

static void DrawForkRouteState(const CcRoute *route, Vector3 from,
                               Vector3 to)
{
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float length = sqrtf(dx * dx + dz * dz);
    if (route == NULL || length <= 0.01f) return;
    Vector3 forward = {dx / length, 0.0f, dz / length};
    Vector3 side = {-forward.z, 0.0f, forward.x};
    if (route->closed) {
        Vector3 center = Vector3Add(from, Vector3Scale(forward, 20.0f));
        Vector3 left = Vector3Add(center, Vector3Scale(side, -2.8f));
        Vector3 right = Vector3Add(center, Vector3Scale(side, 2.8f));
        left.y = 0.72f;
        right.y = 0.72f;
        DrawCylinderEx(left, right, 0.13f, 0.13f, 7,
                       (Color){108, 69, 43, 255});
        DrawCylinder((Vector3){left.x, 0.0f, left.z},
                     0.12f, 0.09f, 1.25f, 7,
                     (Color){86, 59, 42, 255});
        DrawCylinder((Vector3){right.x, 0.0f, right.z},
                     0.12f, 0.09f, 1.25f, 7,
                     (Color){86, 59, 42, 255});
    }
    int32_t scars = (100 - route->condition) / 18;
    for (int32_t scar = 0; scar < scars && scar < 4; ++scar) {
        Vector3 center = Vector3Add(
            from, Vector3Scale(forward, 7.0f + (float)scar * 5.0f));
        center = Vector3Add(
            center, Vector3Scale(side, (scar & 1) != 0 ? 0.9f : -0.7f));
        DrawCylinder((Vector3){center.x, 0.08f, center.z},
                     0.34f, 0.48f, 0.025f, 12,
                     (Color){49, 39, 32, 255});
    }
}

static void DrawForkSignpost(Vector3 junction, Vector3 branch_end,
                             bool hidden)
{
    Vector3 base = Vector3Lerp(junction, branch_end, 0.32f);
    DrawCylinder(base, 0.17f, 0.13f, 3.25f, 8,
                 (Color){91, 61, 42, 255});
    float yaw = atan2f(branch_end.x - base.x, branch_end.z - base.z);
    DrawOrientedBox(base, (Vector3){0.0f, 1.58f, 0.72f},
                    (Vector3){1.65f, 0.30f, 0.12f}, yaw,
                    hidden ? (Color){77, 61, 48, 255} :
                             (Color){132, 91, 54, 255});
}

void CcLocalDrawFork3D(const CcSim *sim, const CcLocalAgent *agent, int32_t selected_route,
                       float turn_progress, float clock,
                       RenderTexture2D target,
                       Rectangle destination)
{
    if (sim == NULL) return;
    PrepareRuntimeAssets(0U);
    const CcSettlement *here = CcSimSettlement(
        sim, sim->player.location_id);
    CcId kingdom_id = here != NULL ? here->kingdom_id : 0U;
    Color kingdom = KingdomColor3D(sim, kingdom_id);
    Color ground = BlendColor((Color){41, 67, 48, 255}, kingdom, 0.16f);
    Camera3D camera = PerspectiveCameraComposed(
        (Vector3){54.0f, 1.15f, 40.0f},
        (Vector3){-24.0f, 7.2f, 8.5f}, 44.0f);
    camera = SnapCameraToArtPixels(camera, target.texture.height);
    RememberPresentedCamera(CC_LOCAL_SCENE_ROAD, camera, agent,
        target.texture.width, target.texture.height);
    ArtComposition fork_art = ROAD_ART_COMPOSITION;
    fork_art.focal_point = camera.target;
    fork_art.foreground_anchor = (Vector3){29.0f, 0.0f, 41.0f};
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(ArtLightBackground(fork_art.light_profile));
    rlSetClipPlanes(0.10, 512.0);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &fork_art);
    DrawPlane((Vector3){48.0f, -0.08f, 40.0f},
              (Vector2){104.0f, 78.0f}, ground);
    Vector3 approach = {5.0f, 0.0f, 42.0f};
    Vector3 junction = {50.5f, 0.0f, 40.0f};
    DrawForkRoadSegment(approach, junction, 5.1f,
                        (Color){101, 91, 72, 255}, true);

    WorldLabel labels[CC_MAX_ROUTES] = {0};
    char label_text[CC_MAX_ROUTES][CC_NAME_CAPACITY] = {{0}};
    int32_t label_count = 0;
    int32_t branch_count = 0;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (ForkRouteLeaves(sim, &sim->routes[i])) branch_count += 1;
    }
    Vector3 selected_branch_end = junction;
    bool selected_branch_found = false;
    int32_t ordinal = 0;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (!ForkRouteLeaves(sim, route)) continue;
        Vector2 endpoint = CcLocalForkBranchEndInternal(ordinal,
                                                        branch_count);
        Vector3 branch_end = {endpoint.x, 0.0f, endpoint.y};
        float decay = 1.0f - (float)route->condition / 100.0f;
        Color road = route->smuggler_route ?
            (Color){69, 56, 49, 255} :
            BlendColor((Color){113, 102, 80, 255},
                       (Color){77, 61, 47, 255}, decay);
        bool selected = i == selected_route;
        if (selected) {
            selected_branch_end = branch_end;
            selected_branch_found = true;
            road = BlendColor(road, WORLD_GOLD, 0.30f);
        }
        float width = (route->smuggler_route ? 2.8f : 4.2f) +
                      (selected ? 0.65f : 0.0f);
        DrawForkRoadSegment(junction, branch_end, width, road, true);
        DrawForkRouteState(route, junction, branch_end);

        CcId destination_id = route->from_id == sim->player.location_id ?
            route->to_id : route->from_id;
        const CcSettlement *place = CcSimSettlement(sim, destination_id);
        CcTravelPreview preview = {0};
        (void)CcSimTravelPreview(sim, destination_id, &preview, NULL, 0U);
        (void)snprintf(
            label_text[label_count], sizeof(label_text[label_count]), "%s",
            preview.destination_known && place != NULL ?
                place->name : "UNMARKED TRACK");
        Vector3 label_position = Vector3Lerp(junction, branch_end, 0.72f);
        labels[label_count] = (WorldLabel){
            {label_position.x, 1.30f, label_position.z},
            label_text[label_count],
            selected ? WORLD_GOLD :
            route->smuggler_route ? WORLD_VIOLET : WORLD_MUTED
        };
        label_count += 1;
        DrawForkSignpost(junction, branch_end, route->smuggler_route);
        ordinal += 1;
    }

    TreeRegionalStyle tree_style = TreeStyleForKingdom(kingdom);
    static const Vector2 tree_positions[] = {
        {17.0f, 27.0f}, {23.0f, 55.0f}, {35.0f, 24.0f},
        {39.0f, 58.0f}, {54.0f, 9.0f}, {56.0f, 72.0f},
        {72.0f, 7.0f}, {74.0f, 75.0f}, {88.0f, 15.0f},
        {91.0f, 55.0f}
    };
    for (int32_t tree = 0;
         tree < (int32_t)(sizeof(tree_positions) /
                          sizeof(tree_positions[0])); ++tree) {
        if (!SceneryPointVisible(tree_positions[tree].x,
                                 tree_positions[tree].y,
                                 camera.target)) {
            continue;
        }
        TreeFamily family = tree % 4 == 0 ? TREE_FAMILY_OAK :
                            tree % 3 == 0 ? TREE_FAMILY_POLLARD :
                                            TREE_FAMILY_ALDER;
        Color leaves = (tree & 1) != 0 ? (Color){46, 96, 69, 255} :
                                        (Color){63, 112, 77, 255};
        DrawTree(tree_positions[tree].x, tree_positions[tree].y,
                 family, leaves, tree_style);
    }

    Vector3 carriage_start = {29.0f, 0.0f, 41.0f};
    float turn = SmoothStep01(turn_progress);
    float branch_heading = 0.5f * PI;
    if (selected_branch_found) {
        branch_heading = atan2f(selected_branch_end.x - junction.x,
                                selected_branch_end.z - junction.z);
    }
    Vector3 carriage;
    float carriage_heading;
    if (turn < 0.68f || !selected_branch_found) {
        float amount = fminf(1.0f, turn / 0.68f);
        carriage = Vector3Lerp(carriage_start, junction, amount);
        carriage_heading = 0.5f * PI;
    } else {
        float amount = (turn - 0.68f) / 0.32f;
        carriage = Vector3Lerp(junction, selected_branch_end,
                               amount * 0.24f);
        carriage_heading = 0.5f * PI +
            WrapAngle(branch_heading - 0.5f * PI) * SmoothStep01(amount);
    }
    DrawRoadCarriage(carriage, sim, clock,
                     turn < 1.0f ? 0.72f : 0.0f,
                     carriage_heading, true, false);
    DrawVisibleNpcFigure3D(
        (Vector3){33.0f, 0.0f, 35.6f}, 0.90f, 1.42f,
        UINT32_C(0x666f726b), CC_NPC_ROLE_SCOUT, kingdom,
        clock * 0.35f, CC_TRAVERSAL_IDLE, camera.target);
    DrawVisibleNpcFigure3D(
        (Vector3){26.0f, 0.0f, 45.2f}, 0.86f, 0.74f,
        UINT32_C(0x666f726d), CC_NPC_ROLE_TRAVELLER, WORLD_TEAL,
        clock * 0.28f + 1.0f, CC_TRAVERSAL_IDLE, camera.target);
    if (crew_exchange != NULL) {
        CcLocalAgent passenger;
        PrepareCrewPassenger(agent, carriage, carriage_heading, &passenger);
        DrawRobotShell(&passenger);
        DrawCrew(&passenger, 7);
    }
    EndWorldLighting();
    EndMode3D();
    rlSetClipPlanes(0.05, 4000.0);
    EndTextureMode();
    PresentTarget(target, destination);
    DrawCrewLabels(camera, destination);
    if (!interaction_ui_enabled) DrawLabels(labels, label_count, camera, destination);
}

static void DrawRoadBarricade(const CcRoute *route)
{
    const float x = ROAD_BARRICADE_X;
    Color timber = route != NULL && route->smuggler_route ?
                   WORLD_WOOD_SHADOW : WORLD_WOOD;
    for (int32_t rail = -1; rail <= 1; rail += 2) {
        Vector3 from = {x - 0.18f, 0.48f + (float)(rail + 1) * 0.18f,
                        36.95f};
        Vector3 to = {x + 0.18f, 0.66f + (float)(rail + 1) * 0.18f,
                      43.05f};
        DrawCylinderEx(from, to, 0.10f, 0.10f, 8, timber);
    }
    for (int32_t post = 0; post < 3; ++post) {
        float z = 37.25f + (float)post * 2.75f;
        DrawCylinder((Vector3){x, 0.0f, z}, 0.12f, 0.09f, 1.35f, 7,
                     timber);
        DrawCylinderEx((Vector3){x, 0.74f, z},
                       (Vector3){x - 0.72f, 0.10f, z - 0.35f},
                       0.065f, 0.025f, 6, (Color){75, 61, 51, 255});
    }
    DrawBox((Vector3){x - 0.90f, 0.42f, 37.55f},
            (Vector3){0.72f, 0.84f, 0.72f},
            (Color){121, 82, 52, 255});
    DrawBox((Vector3){x + 0.83f, 0.30f, 42.30f},
            (Vector3){0.92f, 0.60f, 0.72f},
            (Color){105, 72, 49, 255});
    DrawBox((Vector3){x, 1.82f, 40.00f},
            (Vector3){0.08f, 1.12f, 1.25f}, WORLD_DANGER);
}

static void DrawRoadSurface(float x, float width, Color road)
{
    if (width <= 0.0f) return;
    DrawTerrainPatchAtTop(x, 36.30f, width, 7.40f, -0.024f,
                          ShadeColor(road, 0.68f));
    DrawTerrainPatchAtTop(x, 36.55f, width, 6.90f, -0.014f, road);
    DrawTerrainPatchAtTop(x, 36.40f, width, 0.16f, -0.005f,
                          ShadeColor(road, 0.56f));
    DrawTerrainPatchAtTop(x, 43.44f, width, 0.16f, -0.005f,
                          ShadeColor(road, 0.56f));
    for (int32_t rut = 0; rut < 2; ++rut) {
        DrawTerrainPatchAtTop(x, 38.45f + (float)rut * 3.05f,
                              width, 0.16f, -0.003f,
                              Fade(WORLD_WOOD_SHADOW, 0.72f));
    }
}

static const char *RoadJourneyBeatName(const CcSim *sim)
{
    CcJourneyStopKind stop = CcSimJourneyStop(sim);
    if (stop == CC_JOURNEY_STOP_MIDDAY) return "MIDDAY BREAK";
    if (stop == CC_JOURNEY_STOP_OVERNIGHT) {
        return CcSimJourneyRoadHouseAvailable(sim) ?
            "ROAD HOUSE STOP" : "CAMP STOP";
    }
    return CcSimJourneyWatchNumber(sim) % 2 == 1 ?
        "MORNING ROAD" : "AFTERNOON ROAD";
}

uint32_t CcLocalRoadWildernessSeedInternal(uint32_t world_seed,
                                           CcId route_id,
                                           int32_t segment_index)
{
    uint32_t seed = world_seed ^ (uint32_t)route_id ^
                    (uint32_t)(route_id >> 32U);
    seed ^= (uint32_t)segment_index * UINT32_C(0x9e3779b9);
    seed ^= seed >> 16U;
    seed *= UINT32_C(0x7feb352d);
    seed ^= seed >> 15U;
    seed *= UINT32_C(0x846ca68b);
    seed ^= seed >> 16U;
    return seed == 0U ? UINT32_C(0xa341316c) : seed;
}

static void DrawRoadTerrain(uint32_t world_seed, const CcRoute *route,
                            int32_t danger, bool bridge_checkpoint,
                            Color kingdom, int32_t progress_milli,
                            Vector3 focus, bool road_house_nearby)
{
    int32_t road_beat = progress_milli < 250 ? 0 :
                        progress_milli < 500 ? 1 :
                        progress_milli < 750 ? 2 : 3;
    float decay = route != NULL ?
        1.0f - (float)route->condition / 100.0f : 0.5f;
    Color ground = route != NULL && route->smuggler_route ?
        WORLD_GRASS_SHADOW :
        BlendColor(WORLD_GRASS, WORLD_EARTH, decay * 0.75f);
    ground = road_beat == 1 ? ShadeColor(ground, 0.82f) :
             road_beat == 2 ? BlendColor(ground, WORLD_EARTH, 0.22f) :
             road_beat == 3 ? BlendColor(ground, kingdom, 0.16f) : ground;
    Color road = BlendColor(WORLD_ROAD_LIGHT,
                            WORLD_EARTH_SHADOW, decay);
    DrawPlane((Vector3){CC_LOCAL_WORLD_WIDTH * 0.5f, -0.09f,
                        CC_LOCAL_WORLD_DEPTH * 0.5f},
              (Vector2){CC_LOCAL_WORLD_WIDTH + 8.0f,
                        CC_LOCAL_WORLD_DEPTH + 8.0f}, ground);
    if (bridge_checkpoint) {

        DrawRoadSurface(15.0f, ROAD_BARRICADE_X - 4.40f - 15.0f, road);
        DrawRoadSurface(ROAD_BARRICADE_X + 4.40f,
                        81.0f - (ROAD_BARRICADE_X + 4.40f), road);
    } else {
        DrawRoadSurface(15.0f, 66.0f, road);
    }
    int32_t scars = route != NULL ? (100 - route->condition) / 12 : 4;
    for (int32_t scar = 0; scar < scars && scar < 7; ++scar) {
        float x = 28.0f + (float)scar * 6.70f;
        if (bridge_checkpoint &&
            x > ROAD_BARRICADE_X - 4.40f &&
            x < ROAD_BARRICADE_X + 4.40f) continue;
        float z = 39.10f + (float)(scar & 1) * 1.65f;
        if (!SceneryPointVisible(x, z, focus)) continue;
        DrawCylinder((Vector3){x, -0.025f, z}, 0.28f, 0.42f,
                     0.035f, 12, WORLD_EARTH_SHADOW);
    }
    Color leaves = route != NULL && route->smuggler_route ?
        WORLD_FOLIAGE_SHADOW : WORLD_FOLIAGE;
    TreeRegionalStyle regional_style = TreeStyleForKingdom(kingdom);
    CcId route_id = route != NULL ? route->id : 0U;
    int32_t tree_count = road_beat == 1 ? 17 : road_beat == 3 ? 10 : 13;
    for (int32_t tree = 0; tree < tree_count; ++tree) {
        uint32_t seed = CcLocalRoadWildernessSeedInternal(
            world_seed, route_id, tree + road_beat * 31);
        float x = 15.5f + (float)tree * 5.35f +
                  ((float)((seed >> 8U) % 101U) / 100.0f - 0.5f) * 2.2f;
        bool far_side = (seed & 1U) != 0U;
        float verge = 1.0f + (float)((seed >> 16U) % 31U) / 10.0f;
        float z = far_side ? 45.4f + verge : 34.6f - verge;
        x += far_side ? -regional_style.cluster_pull :
                        regional_style.cluster_pull;
        if (!SceneryPointVisible(x, z, focus)) continue;
        TreeFamily family = (seed % 7U) == 0U ? TREE_FAMILY_OAK :
                            (seed % 5U) == 0U ? TREE_FAMILY_POLLARD :
                                               TREE_FAMILY_ALDER;
        family = RegionalTreeFamily(family, tree, regional_style);
        Color tree_leaves = (seed % 3U) == 0U ?
                            ShadeColor(leaves, 0.82f) : leaves;
        if (family == TREE_FAMILY_OAK) {
            tree_leaves = ShadeColor(tree_leaves, 0.86f);
        } else if (family == TREE_FAMILY_POLLARD) {
            tree_leaves = BlendColor(tree_leaves,
                                     WORLD_GRASS_LIGHT, 0.16f);
        }
        DrawTree(x, z, family, tree_leaves, regional_style);
        if ((seed & 12U) == 12U) {
            DrawScenerySphere(
                (Vector3){x + (far_side ? -1.1f : 1.1f), 0.22f, z},
                0.38f + (float)((seed >> 20U) % 4U) * 0.08f,
                ShadeColor(WORLD_STONE, 0.78f));
        }
    }
    if (road_house_nearby &&
        SceneryFootprintVisible((Rectangle){48.0f, 27.0f, 12.0f, 8.0f},
                                focus)) {
        DrawBox((Vector3){54.0f, 1.25f, 31.0f},
                (Vector3){6.8f, 2.5f, 4.2f}, WORLD_WOOD_LIGHT);
        DrawBox((Vector3){54.0f, 2.75f, 31.0f},
                (Vector3){7.6f, 0.55f, 4.8f}, WORLD_EARTH_SHADOW);
        DrawBox((Vector3){54.0f, 0.82f, 33.12f},
                (Vector3){1.05f, 1.64f, 0.12f}, WORLD_WOOD_SHADOW);
        DrawSmallSphere((Vector3){51.2f, 1.72f, 33.25f}, 0.18f,
                        WORLD_GOLD);
        DrawBox((Vector3){50.0f, 1.18f, 34.2f},
                (Vector3){0.12f, 2.36f, 0.12f}, WORLD_WOOD_SHADOW);
        DrawBox((Vector3){51.0f, 2.10f, 34.2f},
                (Vector3){2.0f, 0.72f, 0.12f}, WORLD_GOLD);
    }
    if (road_beat == 0) {
        for (int32_t post = 0; post < 5; ++post) {
            DrawBox((Vector3){39.0f + (float)post * 1.45f, 0.36f, 33.15f},
                    (Vector3){0.12f, 0.72f, 0.12f}, WORLD_WOOD_SHADOW);
        }
        DrawBox((Vector3){41.9f, 0.62f, 33.15f},
                (Vector3){5.9f, 0.10f, 0.10f}, WORLD_WOOD);
    } else if (road_beat == 1) {
        DrawBox((Vector3){56.0f, 0.82f, 33.10f},
                (Vector3){0.72f, 1.64f, 0.72f}, WORLD_STONE);
        DrawBox((Vector3){56.0f, 1.52f, 33.10f},
                (Vector3){1.05f, 0.26f, 0.22f}, WORLD_STONE_LIGHT);
    } else if (road_beat == 2) {
        DrawBox((Vector3){52.0f, 0.48f, 33.25f},
                (Vector3){3.4f, 0.12f, 2.0f}, WORLD_VIOLET);
        for (int32_t post = -1; post <= 1; post += 2) {
            DrawBox((Vector3){52.0f + (float)post * 1.45f, 0.65f, 33.25f},
                    (Vector3){0.10f, 1.30f, 0.10f}, WORLD_WOOD_SHADOW);
        }
        DrawSmallSphere((Vector3){48.8f, 0.14f, 33.30f}, 0.22f,
                        WORLD_GOLD);
    } else {
        for (int32_t side = -1; side <= 1; side += 2) {
            float z = 40.0f + (float)side * 5.0f;
            DrawBox((Vector3){61.0f, 0.90f, z},
                    (Vector3){0.16f, 1.80f, 0.16f}, WORLD_STONE_LIGHT);
            DrawSmallSphere((Vector3){61.0f, 1.92f, z}, 0.22f,
                            WORLD_GOLD);
        }
    }
    if (route != NULL && (route->closed || route->condition < 42) &&
        SceneryFootprintVisible((Rectangle){68.0f, 32.0f, 4.0f, 16.0f},
                                focus)) {
        DrawTerrainPatchAtTop(68.0f, 32.0f, 4.0f, 16.0f, -0.002f,
                              CC_STYLE_PANEL_DEEP);
        for (int32_t plank = 0; plank < 5; ++plank) {
            DrawBox((Vector3){70.0f, 0.12f, 37.35f + (float)plank * 1.32f},
                    (Vector3){4.45f, 0.18f, 0.92f},
                    plank == 2 ? WORLD_WOOD_SHADOW : WORLD_WOOD_LIGHT);
        }
    }
    if (route != NULL && route->security >= 65 &&
        SceneryPointVisible(30.20f, 35.70f, focus)) {
        DrawBox((Vector3){30.20f, 1.05f, 35.70f},
                (Vector3){0.58f, 2.10f, 0.58f},
                WORLD_STONE_LIGHT);
        DrawBox((Vector3){30.20f, 1.68f, 35.38f},
                (Vector3){0.42f, 0.46f, 0.05f}, WORLD_GOLD);
    }
    if (danger >= 45 && SceneryFootprintVisible(
            (Rectangle){58.5f, 43.79f, 2.72f, 0.72f}, focus)) {
        for (int32_t marker = 0; marker < 3; ++marker) {
            DrawBox((Vector3){58.5f + (float)marker * 1.1f, 0.18f,
                              44.15f},
                    (Vector3){0.52f, 0.36f, 0.72f},
                    BlendColor(WORLD_EARTH_SHADOW, WORLD_DANGER, 0.24f));
        }
    }
}

static bool DrawBridgeCheckpoint(void)
{
    RuntimeAsset *asset = &runtime_assets[RUNTIME_ASSET_BRIDGE];
    if (!asset->ready) return false;
    DrawModel(asset->model,
              (Vector3){ROAD_BARRICADE_X, 0.0f, 40.0f}, 1.0f, WHITE);
    return true;
}

float CcLocalRoadCarriageX(int32_t progress_milli)
{
    if (progress_milli < 0) progress_milli = 0;
    if (progress_milli > 1000) progress_milli = 1000;
    return 20.15f + (float)progress_milli * 0.052f;
}

void CcLocalDrawRoad3D(const CcSim *sim, const CcLocalAgent *agent,
                       const CcLocalCourse *course, bool travelling,
                       bool parley, const CcLocalConvoyState *convoy,
                       float clock, RenderTexture2D target,
    Rectangle destination)
{
    if (sim == NULL || agent == NULL || course == NULL) return;
    PrepareRuntimeAssets(
        travelling ? 0U : UINT32_C(1) << RUNTIME_ASSET_BRIDGE);
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    const CcSettlement *origin = CcSimSettlement(
        sim, sim->journey.origin_id);
    const CcSettlement *destination_place = CcSimSettlement(
        sim, sim->journey.destination_id);
    const CcBanditGroup *bandits = NULL;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == sim->journey.route_id) {
            bandits = &sim->bandits[i];
            break;
        }
    }
    bool authored_checkpoint = !travelling &&
        runtime_assets[RUNTIME_ASSET_BRIDGE].ready;
    float carriage_x = CcLocalRoadCarriageX(sim->carriage.progress_milli);
    float lateral_offset = convoy != NULL ? convoy->lateral_offset : 0.0f;
    float carriage_pace = travelling && convoy != NULL ?
        convoy->pace : travelling ? 0.72f : 0.0f;
    bool carriage_moving = carriage_pace > 0.02f;
    Vector3 carriage_base = {
        carriage_x,
        carriage_moving ? 0.025f + sinf(clock * 5.0f) * 0.018f : 0.0f,
        40.0f + lateral_offset
    };
    if (authored_checkpoint) {
        carriage_base.y += CcLocalRoadCheckpointSurfaceYInternal(
            carriage_base.x, carriage_base.z);
    }
    Vector3 travelling_hero_position = {
        carriage_x - 3.25f + (float)(crew_seat % 4) * 0.85f, 0.0f,
        37.95f + lateral_offset + (float)(crew_seat / 4) * 1.25f
    };
    CcLocalAgent travelling_hero = {0};
    if (travelling) {
        PrepareHeroDisplayAt(agent, travelling_hero_position,
                             CC_LOCAL_SCENE_ROAD, 0.5f * PI,
                             &travelling_hero);
    }
    Vector3 camera_focus = travelling ? carriage_base : agent->position;
    Camera3D base_camera = RoadCamera(camera_focus, travelling, clock, true,
                                      target.texture.height);
    Camera3D camera = CcLocalCombatCameraInternal(
        base_camera, agent, course, clock, true, target.texture.height);
    if (travelling) camera = PonyConversationCamera(camera, carriage_base, 0.5f * PI, sim);
    if (!travelling) {
        camera = KeepHeroInsideStreetFrame(
            camera,
            Vector3Add(agent->position, (Vector3){0.0f, 1.05f, 0.0f}),
            target.texture.height,
            (Rectangle){0.10f, 0.12f, 0.80f, 0.76f});
        camera = SnapCameraToArtPixels(camera, target.texture.height);
    }
    bool combat_presentation = !travelling && course->alarm_active &&
                               camera.projection == CAMERA_PERSPECTIVE;
    const CcLocalAgent *duel_opponent = combat_presentation ?
        CombatCameraOpponent(course, agent) : NULL;
    RememberPresentedCamera(CC_LOCAL_SCENE_ROAD, camera, agent,
                            target.texture.width, target.texture.height);
    ArtComposition road_art = ROAD_ART_COMPOSITION;
    road_art.focal_point = camera.target;
    road_art.foreground_anchor = carriage_base;
    Color background = ArtLightBackground(road_art.light_profile);
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(background);
    rlSetClipPlanes(0.10, 512.0);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &road_art);
    CcId road_kingdom_id = origin != NULL ? origin->kingdom_id :
        destination_place != NULL ? destination_place->kingdom_id : 0;
    Color road_kingdom = KingdomColor3D(sim, road_kingdom_id);
    int32_t house_progress = route != NULL ?
        CcSimRoadHouseProgressMilli(
            sim, route->id, CcSimJourneyWatchCount(sim)) : 0;
    int32_t house_distance = sim->carriage.progress_milli > house_progress ?
        sim->carriage.progress_milli - house_progress :
        house_progress - sim->carriage.progress_milli;
    bool road_house_nearby = house_progress > 0 &&
        (house_distance <= 80 || CcSimJourneyRoadHouseAvailable(sim));
    DrawRoadTerrain(sim->world_seed, route, sim->journey.danger,
                    authored_checkpoint, road_kingdom,
                    sim->carriage.progress_milli, camera.target,
                    road_house_nearby);
    if (!travelling &&
        SceneryPointVisible(ROAD_BARRICADE_X, 40.0f, camera.target)) {
        bool checkpoint = DrawBridgeCheckpoint();
        if (!checkpoint) {
            DrawRoadBarricade(route);
        }
        float banner_z = checkpoint ? 39.10f : 37.15f;
        DrawHeraldryBanner3D(
            CcHeraldryForSpecial(CC_SPECIAL_BANNER_BANDITS),
            (Vector3){ROAD_BARRICADE_X - 2.55f,
                      checkpoint ?
                          CcLocalRoadCheckpointSurfaceYInternal(
                              ROAD_BARRICADE_X - 2.55f, banner_z) : 0.0f,
                      banner_z},
            0.0f, 0.78f, clock);
    }
    if (!travelling) DrawAgentPath(agent, false);
    DrawRoadCarriage(carriage_base, sim, clock, carriage_pace,
                     0.5f * PI, true, authored_checkpoint);
    int32_t roadside_cattle = 0;
    if (origin != NULL) {
        roadside_cattle += origin->cow_adults + origin->cow_calves;
    }
    if (destination_place != NULL) {
        roadside_cattle += destination_place->cow_adults +
                           destination_place->cow_calves;
    }
    if (roadside_cattle > 0) {
        Vector3 cow_position = {carriage_x + 3.8f, 0.0f, 33.40f};
        CcCreaturePose cow_pose = CcCreatureSteppedPose(
            CC_CREATURE_COW, clock * 1.55f, travelling);
        CcCreatureRigPose controlled_cow;
        bool controlled = ResolveControlledCreatureGait(
            CREATURE_GAIT_ROAD_COW, CC_CREATURE_RIG_COW,
            CC_CREATURE_RIG_GAIT_WALK, clock,
            0.18f, 0.62f, travelling, cow_position,
            -0.72f * PI, CC_LOCAL_SCENE_ROAD, &controlled_cow);
        (void)DrawCreatureGait3D(
            CC_CREATURE_COW, cow_pose, cow_position,
            -0.72f * PI, 0.88f, (Color){184, 169, 139, 255},
            clock * 1.55f, travelling,
            controlled ? &controlled_cow : NULL);
        if (roadside_cattle >= 24) {
            (void)DrawCreature3D(
                CC_CREATURE_COW, CC_CREATURE_POSE_IDLE,
                (Vector3){carriage_x + 5.60f, 0.0f, 32.80f},
                -0.56f * PI, 0.78f, (Color){118, 86, 66, 255});
        }
    }
    int32_t roadside_sheep = 0;
    if (origin != NULL) {
        roadside_sheep += origin->sheep_adults + origin->sheep_lambs;
    }
    if (destination_place != NULL) {
        roadside_sheep += destination_place->sheep_adults +
                          destination_place->sheep_lambs;
    }
    if (roadside_sheep > 0) {
        (void)DrawCreatureGait3D(
            CC_CREATURE_SHEEP,
            CcCreatureSteppedPose(
                CC_CREATURE_SHEEP, clock * 1.42f, travelling),
            (Vector3){carriage_x + 5.10f, 0.0f, 35.10f},
            -0.60f * PI, 0.44f, (Color){228, 220, 185, 255},
            clock * 1.42f, travelling, NULL);
        if (roadside_sheep >= 32) {
            (void)DrawCreature3D(
                CC_CREATURE_SHEEP, CC_CREATURE_POSE_IDLE,
                (Vector3){carriage_x + 6.10f, 0.0f, 34.35f},
                -0.48f * PI, 0.39f, (Color){206, 198, 169, 255});
        }
    }

    if (!combat_presentation) {
        if (travelling) {
            DrawRobotShell(&travelling_hero);
        } else {
            DrawNpcFigure3D(
                travelling_hero_position, 0.90f, 1.35f,
                UINT32_C(0x726f6101), CC_NPC_ROLE_TRAVELLER,
                (Color){151, 103, 78, 255}, clock * 0.42f,
                CC_TRAVERSAL_IDLE);
        }
        DrawNpcFigure3D(
            (Vector3){carriage_x - 4.00f, 0.0f, 39.40f}, 0.84f, 1.10f,
            UINT32_C(0x726f6102), CC_NPC_ROLE_HEALER,
            WORLD_TEAL, clock * 0.36f + 1.4f,
            CC_TRAVERSAL_IDLE);
        DrawNpcFigure3D(
            (Vector3){carriage_x - 3.35f, 0.0f, 41.55f}, 0.80f, 1.55f,
            UINT32_C(0x726f6103), CC_NPC_ROLE_REFUGEE,
            WORLD_VIOLET, clock * 0.31f + 2.1f,
            CC_TRAVERSAL_IDLE);
        DrawBox((Vector3){carriage_x - 4.25f, 0.34f, 42.45f},
                (Vector3){0.72f, 0.68f, 0.72f},
                (Color){137, 91, 55, 255});
        DrawBox((Vector3){carriage_x - 3.33f, 0.25f, 42.42f},
                (Vector3){0.82f, 0.50f, 0.64f},
                (Color){112, 76, 53, 255});
    }

    if (!travelling) {
        if (combat_presentation) DrawCourseRaider(duel_opponent);
        else DrawCourseRunners(course, camera.target);
    }
    if (parley && !course->alarm_active) {
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            DrawRobotShell(&course->raiders[i]);
        }
    }
    if (agent->combat.target_index >= 0 &&
        agent->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
        DrawSelectedTarget(&course->raiders[agent->combat.target_index]);
    }
    if (!travelling) {
        DrawRobotShell(agent);
        DrawCombatSword(agent);
        DrawCombatSkillTell(agent);
        DrawCombatImpact(agent);
    }
    DrawCrew(travelling ? &travelling_hero : agent, 2);
    EndWorldLighting();
    EndMode3D();
    rlSetClipPlanes(0.05, 4000.0);
    DrawTargetAtmosphere(target, clock);
    EndTextureMode();
    PresentTarget(target, destination);
    DrawCrewLabels(camera, destination);
    char route_label[96];
    char blockade_label[96];
    (void)snprintf(route_label, sizeof(route_label), "%s -> %s",
                   origin != NULL ? origin->name : "ORIGIN",
                   destination_place != NULL ? destination_place->name :
                                               "DESTINATION");
    (void)snprintf(blockade_label, sizeof(blockade_label), "%s",
                   bandits != NULL ? bandits->name : "ROAD COLLECTORS");
    WorldLabel labels[6] = {0};
    int32_t count = 0;
    if (!travelling && !combat_presentation) {
        labels[count++] = (WorldLabel){{agent->position.x,
                                        agent->position.y + 2.50f,
                                        agent->position.z}, "YOU", WORLD_TEAL};
    } else if (travelling) {
        labels[count++] = (WorldLabel){
            {travelling_hero_position.x,
             travelling_hero_position.y + 2.50f,
             travelling_hero_position.z}, "YOU", WORLD_TEAL};
    }
    if (!travelling && !parley && !combat_presentation) {
        labels[count++] = (WorldLabel){{ROAD_BARRICADE_X, 2.58f, 40.00f},
                                       blockade_label, WORLD_DANGER};
    }
    if (!combat_presentation) {
        labels[count++] = (WorldLabel){
            {travelling ? carriage_x + 8.0f : 57.0f, 1.18f, 40.0f},
            route_label, WORLD_GOLD};
    }
    if (travelling && road_house_nearby) {
        labels[count++] = (WorldLabel){
            {54.0f, 3.35f, 31.0f},
            CcSimRoadHouseName(sim, sim->journey.route_id), WORLD_TEAL};
    }
    if (parley && !combat_presentation) {
        labels[count++] = (WorldLabel){
            {course->raiders[0].position.x,
             course->raiders[0].position.y + 2.18f,
             course->raiders[0].position.z},
            course->raider_names[0], WORLD_DANGER};
        labels[count++] = (WorldLabel){{CC_LOCAL_ROAD_PARLEY_X, 0.42f,
                                        CC_LOCAL_ROAD_PARLEY_Z},
                                       "F  SPEAK WITH CAPTAIN", WORLD_TEAL};
    }
    DrawLabels(labels, count, camera, destination);
    if (!travelling &&
        CcLocalCourseHasNearbyHostile(course, agent)) {
        DrawCombatBar(agent, camera, destination, WORLD_TEAL);
        if (combat_presentation) {
            DrawCombatBar(CombatCameraOpponent(course, agent), camera,
                          destination, WORLD_GOLD);
        } else {
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                DrawCombatBar(&course->runners[i].agent, camera,
                              destination, course->runners[i].marker_color);
            }
            for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
                DrawCombatBar(&course->raiders[i], camera,
                              destination,
                              i == agent->combat.target_index ? WORLD_GOLD :
                                                                WORLD_DANGER);
            }
        }
    }
    Rectangle road_status = ViewportRectangle(
        destination, 10.0f, 9.0f, 610.0f, 46.0f);
    DrawRectangleRounded(road_status,
                         0.02f, 3, Fade(CC_STYLE_PANEL_DEEP, 0.94f));
    DrawRectangleRoundedLinesEx(road_status, 0.02f, 3, 1.0f,
                                Fade(WORLD_GOLD, 0.52f));
    DrawLine((int32_t)road_status.x + 8, (int32_t)road_status.y + 7,
             (int32_t)road_status.x + 46, (int32_t)road_status.y + 7,
             Fade(WORLD_GOLD, 0.56f));
    DrawViewportText(
        TextFormat("%s  /  DANGER %d%%  /  ROAD %d%%  /  SECURITY %d%%",
                   RoadArchetypeName(route), sim->journey.danger,
                   route != NULL ? route->condition : 0,
                   route != NULL ? route->security : 0),
        destination, 18, 18, 10, parley ? WORLD_TEAL : WORLD_DANGER);
    DrawViewportText(
        travelling ?
            TextFormat("%s / %d%% COMPLETE / %s PACE / ETA %dH",
                       RoadJourneyBeatName(sim),
                       sim->carriage.progress_milli / 10,
                       CcJourneyPaceName(sim->journey.pace),
                       (CcSimJourneyEtaMinutes(sim) + 59) / 60) : parley ?
        "PARLEY / approach the captain, then choose coin or needed supplies" :
        TextFormat("BREAK THE CORDON / YOU %d HP / COMPANY %d%% NERVE",
                   (int32_t)lroundf(agent->combat.health),
                   course->raider_resolve > 0 ? course->raider_resolve : 0),
        destination, 18, 35, 10, WORLD_INK);
}

const char *CcLocalSiteName(const CcSim *sim, CcLocalSiteKind site)
{
    if (sim == NULL) return "Remote site";
    if (site == CC_LOCAL_SITE_DUNGEON) {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            if (sim->dungeons[i].settlement_id ==
                sim->player.location_id) {
                return sim->dungeons[i].name;
            }
        }
        return "Dungeon road";
    }
    if (site == CC_LOCAL_SITE_GOBLIN_CAVE) return sim->goblins.name;
    if (site == CC_LOCAL_SITE_DRAGON_CAVE) return sim->dragon.name;
    return "Remote site";
}

static const CcRoute *SiteAnchorRoad(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id == sim->player.location_id ||
            route->to_id == sim->player.location_id) return route;
    }
    return NULL;
}

static void DrawRemoteFootTrailSegment(Vector2 start, Vector2 end,
                                       float width, Color color)
{
    float dx = end.x - start.x;
    float dz = end.y - start.y;
    float length = sqrtf(dx * dx + dz * dz);
    if (length < 0.01f) return;
    DrawTiltedBox(
        (Vector3){(start.x + end.x) * 0.5f, 0.015f,
                  (start.y + end.y) * 0.5f},
        (Vector3){length + 0.25f, 0.07f, width},
        (Vector3){0.0f, 1.0f, 0.0f},
        -atan2f(dz, dx) * RAD2DEG, color);
}

static void DrawGoblinCaveStream(void)
{
    static const Vector2 STREAM[] = {
        {69.0f, 40.2f}, {63.0f, 42.4f}, {57.0f, 40.8f},
        {51.0f, 44.7f}, {45.0f, 42.6f}, {39.2f, 45.8f},
        {33.8f, 44.2f}
    };
    Color channel = BlendColor(WORLD_EARTH_SHADOW, WORLD_INK, 0.42f);
    Color water = BlendColor(WORLD_TEAL, (Color){38, 74, 88, 255}, 0.46f);
    Color glint = BlendColor(WORLD_TEAL, WORLD_STONE, 0.20f);
    DrawCylinder((Vector3){68.2f, 0.045f, 40.45f},
                 2.05f, 1.55f, 0.07f, 12, water);
    int32_t count = (int32_t)(sizeof(STREAM) / sizeof(STREAM[0]));
    for (int32_t segment = 0; segment < count - 1; ++segment) {
        DrawRemoteFootTrailSegment(STREAM[segment], STREAM[segment + 1],
                                   2.05f, channel);
        DrawRemoteFootTrailSegment(STREAM[segment], STREAM[segment + 1],
                                   1.18f, ShadeColor(
                                       water, segment % 2 == 0 ? 0.90f : 1.08f));
        DrawRemoteFootTrailSegment(STREAM[segment], STREAM[segment + 1],
                                   0.16f, Fade(glint, 0.78f));
    }
    for (int32_t stone = 0; stone < 6; ++stone) {
        float x = 37.0f + (float)stone * 5.35f;
        float z = stone % 2 == 0 ? 43.0f : 45.1f;
        DrawTiltedBox((Vector3){x, 0.22f, z},
                      (Vector3){0.48f, 0.44f, 0.62f},
                      (Vector3){0.0f, 0.0f, 1.0f},
                      stone % 2 == 0 ? 11.0f : -8.0f,
                      WORLD_STONE_SHADOW);
    }
}

static void DrawRemoteSiteTerrain(uint32_t world_seed,
                                  const CcRoute *road,
                                  CcLocalSiteKind site,
                                  bool travelling,
                                  Color kingdom,
                                  int32_t progress_milli,
                                  Vector3 focus)
{
    int32_t danger = site == CC_LOCAL_SITE_DRAGON_CAVE ? 72 :
                     site == CC_LOCAL_SITE_GOBLIN_CAVE ? 48 : 38;
    if (!travelling && site == CC_LOCAL_SITE_DRAGON_CAVE) {
        DrawPlane((Vector3){CC_LOCAL_WORLD_WIDTH * 0.5f, -0.09f,
                            CC_LOCAL_WORLD_DEPTH * 0.5f},
                  (Vector2){CC_LOCAL_WORLD_WIDTH + 8.0f,
                            CC_LOCAL_WORLD_DEPTH + 8.0f},
                  BlendColor(WORLD_STONE_SHADOW, WORLD_INK, 0.46f));
    } else {
        DrawRoadTerrain(world_seed, road, danger, false, kingdom,
                        progress_milli, focus, false);
    }
    if (travelling || site == CC_LOCAL_SITE_DUNGEON) return;

    bool mountain = site == CC_LOCAL_SITE_DRAGON_CAVE;
    float cover_start = mountain ? 14.0f : 34.0f;
    float cover_width = 82.0f - cover_start;
    Color wild_ground = mountain ?
        BlendColor(WORLD_STONE_SHADOW, WORLD_INK, 0.38f) :
        BlendColor(WORLD_GRASS_SHADOW, WORLD_INK, 0.28f);
    DrawBox((Vector3){cover_start + cover_width * 0.5f, -0.035f, 40.0f},
            (Vector3){cover_width, 0.12f, 11.6f}, wild_ground);

    static const Vector2 GOBLIN_TRAIL[] = {
        {33.0f, 40.0f}, {39.0f, 43.6f}, {46.5f, 37.0f},
        {54.5f, 43.8f}, {62.0f, 36.6f}, {70.0f, 40.0f}
    };
    if (!mountain) {
        int32_t trail_count =
            (int32_t)(sizeof(GOBLIN_TRAIL) / sizeof(GOBLIN_TRAIL[0]));
        Color trail_color = BlendColor(
            WORLD_EARTH_SHADOW, WORLD_VIOLET, 0.12f);
        for (int32_t segment = 0; segment < trail_count - 1; ++segment) {
            DrawRemoteFootTrailSegment(
                GOBLIN_TRAIL[segment], GOBLIN_TRAIL[segment + 1], 0.92f,
                ShadeColor(trail_color,
                           segment % 2 == 0 ? 0.88f : 1.08f));
        }
        DrawGoblinCaveStream();
    } else {
        for (int32_t shelf = 0; shelf < 5; ++shelf) {
            float x = 31.0f + (float)shelf * 7.2f;
            float z = shelf % 2 == 0 ? 35.0f : 45.2f;
            DrawTiltedBox(
                (Vector3){x, 0.12f + (float)(shelf % 2) * 0.22f, z},
                (Vector3){4.6f, 0.34f, 2.8f},
                (Vector3){0.0f, 1.0f, 0.0f},
                shelf % 2 == 0 ? 17.0f : -21.0f,
                ShadeColor(WORLD_STONE_SHADOW,
                           shelf % 3 == 0 ? 0.72f : 0.92f));
        }
    }

    Color cliff = mountain ?
        BlendColor(WORLD_STONE_SHADOW, WORLD_DANGER, 0.10f) :
        BlendColor(WORLD_STONE_SHADOW, WORLD_VIOLET, 0.14f);
    for (int32_t ridge = 0; ridge < 5; ++ridge) {
        float x = 37.0f + (float)ridge * 10.0f;
        float z = mountain ? 23.0f + (float)(ridge % 2) * 3.2f :
                             ridge % 2 == 0 ? 28.0f : 53.0f;
        float lower_radius = mountain ? 9.5f : 7.0f;
        float upper_radius = mountain ? 3.2f : 2.6f;
        DrawBackdropMass(
            x, -3.4f, z, lower_radius, lower_radius,
            upper_radius / lower_radius,
            mountain ? 14.0f + (float)(ridge % 3) * 2.4f :
                       10.0f + (float)(ridge % 2) * 1.8f,
            8, ShadeColor(cliff, ridge % 2 == 0 ? 0.78f : 1.0f));
    }
    for (int32_t marker = 0; marker < 6; ++marker) {
        float x = 37.0f + (float)marker * 6.1f;
        float z = marker % 2 == 0 ? 34.1f : 46.0f;
        DrawTiltedBox((Vector3){x, 0.55f, z},
                      (Vector3){0.20f, 1.10f + (float)(marker % 3) * 0.32f,
                                0.24f},
                      (Vector3){0.0f, 0.0f, 1.0f},
                      marker % 2 == 0 ? 11.0f : -13.0f,
                      marker % 3 == 0 ? WORLD_DANGER : cliff);
    }

    if (mountain) {
        DrawTiltedBox((Vector3){22.8f, 1.35f, 40.0f},
                      (Vector3){1.25f, 2.75f, 2.30f},
                      (Vector3){0.0f, 0.0f, 1.0f}, 9.0f, cliff);
        DrawTiltedBox((Vector3){25.2f, 1.30f, 40.0f},
                      (Vector3){1.20f, 2.65f, 2.20f},
                      (Vector3){0.0f, 0.0f, 1.0f}, -8.0f,
                      ShadeColor(cliff, 0.82f));
        DrawBox((Vector3){24.0f, 1.05f, 39.72f},
                (Vector3){1.65f, 2.10f, 0.28f}, WORLD_VOID);
    }
}

static void DrawDragonCliffRoost(const CcSim *sim)
{
    const float x = CC_LOCAL_SITE_ENTRANCE_X;
    const float z = CC_LOCAL_SITE_ENTRANCE_Z;
    Color rock = BlendColor(WORLD_STONE_SHADOW, WORLD_DANGER, 0.18f);
    Color deep_rock = BlendColor(rock, WORLD_INK, 0.34f);
    Color ledge = ShadeColor(rock, 0.82f);
    CcCreatureVariant dragon_variant = DragonCreatureForLifeStage(
        sim->dragon.life_stage);
    float perch_width =
        dragon_variant == CC_CREATURE_DRAGON_DEEP_WYRM ? 31.0f :
        dragon_variant == CC_CREATURE_DRAGON ? 21.0f :
        dragon_variant == CC_CREATURE_DRAGON_WANDERER ? 15.0f : 9.0f;

    DrawBackdropMass(x + 19.0f, -3.4f, z, 18.5f, 18.5f,
                     4.8f / 18.5f, 31.0f, 9, deep_rock);
    for (int32_t slab = -2; slab <= 2; ++slab) {
        DrawTiltedBox(
            (Vector3){x - 0.15f, 6.1f + (float)(abs(slab) % 2) * 0.7f,
                      z + (float)slab * 3.0f},
            (Vector3){2.6f,
                      12.8f + (float)(abs(slab) % 2) * 1.2f, 3.25f},
            (Vector3){0.0f, 0.0f, 1.0f},
            slab % 2 == 0 ? 3.5f : -4.5f,
            ShadeColor(rock, 0.76f + (float)(slab + 2) * 0.045f));
    }

    DrawTiltedBox((Vector3){x - 4.4f, 5.92f, z - 0.2f},
                  (Vector3){7.6f, 1.45f, 7.9f},
                  (Vector3){0.0f, 0.0f, 1.0f}, -2.6f, ledge);
    DrawTiltedBox((Vector3){x - 10.8f, 5.58f, z + 0.35f},
                  (Vector3){6.7f, 1.68f, 8.5f},
                  (Vector3){0.0f, 0.0f, 1.0f}, 2.2f,
                  ShadeColor(ledge, 0.86f));
    DrawTiltedBox((Vector3){x - 17.1f, 5.18f, z - 0.55f},
                  (Vector3){7.2f, 1.86f, 6.8f},
                  (Vector3){0.0f, 0.0f, 1.0f}, -3.4f, deep_rock);
    DrawTiltedBox((Vector3){x - 1.5f - perch_width * 0.5f,
                            5.88f, z + 4.35f},
                  (Vector3){perch_width, 1.30f, 4.1f},
                  (Vector3){0.0f, 0.0f, 1.0f}, 1.8f,
                  ShadeColor(rock, 1.05f));
    for (int32_t tooth = 0; tooth < 6; ++tooth) {
        float tooth_x = x - 20.0f + (float)tooth * 3.7f;
        DrawTiltedBox((Vector3){tooth_x, 4.65f, z + 4.3f},
                      (Vector3){1.4f, 3.4f + (float)(tooth % 2), 1.2f},
                      (Vector3){0.0f, 0.0f, 1.0f},
                      tooth % 2 == 0 ? 12.0f : -9.0f,
                      ShadeColor(deep_rock, tooth % 3 == 0 ? 0.72f : 0.92f));
    }

    DrawBox((Vector3){x - 1.48f, 10.35f, z - 0.28f},
            (Vector3){0.38f, 5.10f, 5.90f}, WORLD_VOID);
    DrawTiltedBox((Vector3){x - 1.10f, 13.12f, z},
                  (Vector3){1.0f, 1.55f, 7.35f},
                  (Vector3){0.0f, 0.0f, 1.0f}, -5.0f, rock);
    DrawTiltedBox((Vector3){x - 1.05f, 9.9f, z - 3.25f},
                  (Vector3){0.95f, 6.7f, 1.3f},
                  (Vector3){0.0f, 0.0f, 1.0f}, 5.0f, rock);
    DrawTiltedBox((Vector3){x - 1.05f, 9.7f, z + 3.20f},
                  (Vector3){0.95f, 6.4f, 1.25f},
                  (Vector3){0.0f, 0.0f, 1.0f}, -4.0f,
                  ShadeColor(rock, 0.86f));

    CcCreaturePose pose = sim->dragon.slain ? CC_CREATURE_POSE_DOWN_A :
        sim->dragon.stolen_outstanding > 0 ? CC_CREATURE_POSE_THREAT :
                                            CC_CREATURE_POSE_REST;
    float dragon_back_offset =
        dragon_variant == CC_CREATURE_DRAGON_DEEP_WYRM ? 19.2f :
        dragon_variant == CC_CREATURE_DRAGON ? 10.4f : 3.9f;
    if (dragon_variant == CC_CREATURE_DRAGON_WANDERER) {
        dragon_back_offset = 7.8f;
    }
    Vector3 dragon_position = {
        x - dragon_back_offset,
        7.58f,
        z + 4.85f
    };
    float dragon_yaw = dragon_variant == CC_CREATURE_DRAGON_WHELP ?
        -0.48f * PI : 0.48f * PI;
    float dragon_scale = dragon_variant == CC_CREATURE_DRAGON_WHELP ?
        1.24f : 0.90f;
    (void)DrawCreature3D(dragon_variant, pose, dragon_position,
                         dragon_yaw, dragon_scale, DragonCourtHairColor(&sim->dragon));

    int32_t marks = 2 + sim->dragon.crown_strength / 20;
    for (int32_t i = 0; i < marks && i < 6; ++i) {
        DrawSmallSphere(
            (Vector3){x - 4.1f + (float)(i % 3) * 0.40f,
                      6.78f, z - 1.2f + (float)(i / 3) * 0.36f},
            0.14f, WORLD_GOLD);
    }
    CcHeraldryStyle varkesh = CcHeraldryForSpecial(
        CC_SPECIAL_BANNER_VARKESH);
    DrawHeraldryBanner3D(
        varkesh, (Vector3){x - 7.2f, 5.95f, z - 2.65f},
        0.0f, 0.78f, 0.0f);
    DrawHeraldryBanner3D(
        varkesh, (Vector3){x - 13.4f, 5.62f, z + 2.45f},
        180.0f, 0.64f, 0.6f);
}

static void DrawRemoteSiteEntrance(const CcSim *sim, CcLocalSiteKind site,
                                   float clock)
{
    const float x = CC_LOCAL_SITE_ENTRANCE_X;
    const float z = CC_LOCAL_SITE_ENTRANCE_Z;
    if (site == CC_LOCAL_SITE_DRAGON_CAVE) {
        DrawDragonCliffRoost(sim);
        return;
    }
    Color rock = site == CC_LOCAL_SITE_GOBLIN_CAVE ?
        BlendColor(WORLD_STONE_SHADOW, WORLD_VIOLET, 0.18f) :
        WORLD_STONE_SHADOW;
    if (site == CC_LOCAL_SITE_DUNGEON) {
        DrawScenerySphere((Vector3){x - 1.55f, 1.35f, z}, 1.75f, rock);
        DrawScenerySphere((Vector3){x + 1.45f, 1.25f, z}, 1.65f, rock);
    } else {
        DrawTiltedBox((Vector3){x - 1.58f, 1.45f, z},
                      (Vector3){1.55f, 2.90f, 1.48f},
                      (Vector3){0.0f, 0.0f, 1.0f}, 10.0f, rock);
        DrawTiltedBox((Vector3){x + 1.52f, 1.38f, z},
                      (Vector3){1.45f, 2.76f, 1.42f},
                      (Vector3){0.0f, 0.0f, 1.0f}, -9.0f,
                      ShadeColor(rock, 0.82f));
    }
    DrawBox((Vector3){x, 2.35f, z}, (Vector3){4.8f, 1.65f, 1.15f}, rock);
    DrawBox((Vector3){x, 1.20f, z - 0.58f},
            (Vector3){2.05f, 2.40f, 0.32f}, WORLD_VOID);

    if (site == CC_LOCAL_SITE_DUNGEON) {
        for (int32_t rail = -1; rail <= 1; rail += 2) {
            DrawBox((Vector3){x - 5.0f, 0.10f, z + (float)rail * 0.72f},
                    (Vector3){9.0f, 0.12f, 0.13f}, WORLD_WOOD_SHADOW);
        }
        DrawBox((Vector3){x - 2.9f, 0.55f, z + 2.2f},
                (Vector3){1.9f, 1.1f, 1.25f}, WORLD_WOOD);
        DrawHeraldryBanner3D(
            CcHeraldryForSpecial(CC_SPECIAL_BANNER_IRON_LEDGER),
            (Vector3){x - 5.1f, 0.0f, z - 1.75f},
            0.0f, 0.68f, clock);
    } else if (site == CC_LOCAL_SITE_GOBLIN_CAVE) {
        CcCreaturePose scavenger = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_SCAVENGER, clock * 2.1f, true);
        CcCreaturePose raider = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_RAIDER, clock * 2.5f + PI, true);
        (void)DrawCreature3D(
            CC_CREATURE_GOBLIN_SCAVENGER, scavenger,
            (Vector3){x - 5.2f, 0.0f, z - 2.2f}, -0.5f * PI, 1.18f,
            DragonCourtHairColor(&sim->dragon));
        (void)DrawCreature3D(
            CC_CREATURE_GOBLIN_RAIDER, raider,
            (Vector3){x - 3.7f, 0.0f, z + 2.4f}, -0.5f * PI, 1.24f,
            DragonCourtHairColor(&sim->dragon));
        DrawBox((Vector3){x - 5.0f, 0.42f, z + 0.6f},
                (Vector3){1.0f, 0.84f, 1.0f}, WORLD_WOOD);
        DrawHeraldryBanner3D(
            CcHeraldryForSpecial(CC_SPECIAL_BANNER_CINDER_TITHE),
            (Vector3){x - 6.7f, 0.0f, z - 0.4f},
            0.0f, 0.74f, clock);
    }
}

void CcLocalDrawSite3D(const CcSim *sim, const CcLocalAgent *agent,
                       CcLocalSiteKind site, bool travelling,
                       bool returning, float progress, float clock,
                       RenderTexture2D target, Rectangle destination)
{
    if (sim == NULL || agent == NULL || site == CC_LOCAL_SITE_NONE) return;
    PrepareRuntimeAssets(0U);
    float amount = fmaxf(0.0f, fminf(1.0f, progress));
    float carriage_x = travelling ?
        returning ? 62.0f - amount * 40.0f : 22.0f + amount * 40.0f :
        CC_LOCAL_SITE_CARRIAGE_X;
    Vector3 carriage = {carriage_x, 0.0f, CC_LOCAL_SITE_CARRIAGE_Z};
    Camera3D camera;
    if (travelling && returning) {
        float sway = sinf(clock * 0.85f) * 0.24f;
        camera = SnapCameraToArtPixels(
            PerspectiveCameraComposed(
                (Vector3){carriage.x - 12.0f, carriage.y + 1.20f,
                          carriage.z + sway * 0.35f},
                (Vector3){11.5f, 4.8f, 3.8f + sway}, 43.0f),
            target.texture.height);
    } else if (travelling) {
        camera = RoadCamera(carriage, true, clock, true,
                            target.texture.height);
    } else if (site == CC_LOCAL_SITE_DRAGON_CAVE &&
               agent->position.x > 45.0f) {
        camera = SnapCameraToArtPixels(
            PerspectiveCameraComposed(
                Vector3Add(agent->position, (Vector3){7.5f, 5.2f, 0.0f}),
                (Vector3){-24.0f, 9.5f, 19.0f}, 50.0f),
            target.texture.height);
    } else {
        camera = SnapCameraToArtPixels(
            PerspectiveCameraComposed(
                Vector3Add(agent->position, (Vector3){5.0f, 1.0f, 0.0f}),
                (Vector3){-11.0f, 7.5f, 12.5f}, 43.0f),
            target.texture.height);
    }
    RememberPresentedCamera(CC_LOCAL_SCENE_ROAD, camera, agent,
                            target.texture.width, target.texture.height);
    const CcSettlement *anchor = CcSimSettlement(
        sim, sim->player.location_id);
    CcId kingdom_id = anchor != NULL ? anchor->kingdom_id : 0U;
    Color kingdom = KingdomColor3D(sim, kingdom_id);
    const CcRoute *road = SiteAnchorRoad(sim);
    ArtComposition site_art = ROAD_ART_COMPOSITION;
    if (!travelling && (site == CC_LOCAL_SITE_GOBLIN_CAVE ||
                        site == CC_LOCAL_SITE_DRAGON_CAVE)) {
        site_art.light_profile = ART_LIGHT_INTERIOR_EMBER;
    }
    site_art.focal_point = camera.target;
    site_art.foreground_anchor = carriage;
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(ArtLightBackground(site_art.light_profile));
    rlSetClipPlanes(0.10, 512.0);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &site_art);
    DrawRemoteSiteTerrain(
        sim->world_seed ^ ((uint32_t)site << 24U), road, site, travelling,
        kingdom, (int32_t)lroundf(amount * 1000.0f), camera.target);
    DrawRemoteSiteEntrance(sim, site, clock);
    if (site != CC_LOCAL_SITE_DRAGON_CAVE) {
        DrawRoadCarriage(carriage, sim, clock,
                         travelling ? 0.72f : 0.0f,
                         returning ? -0.5f * PI : 0.5f * PI,
                         true, false);
    }
    if (!travelling) {
        DrawAgentPath(agent, false);
        DrawRobotShell(agent);
        DrawCombatSword(agent);
    }
    CcLocalAgent passenger;
    if (travelling && crew_exchange != NULL) {
        PrepareCrewPassenger(agent, carriage, 0.5f * PI, &passenger);
        DrawRobotShell(&passenger);
    }
    DrawCrew(travelling && crew_exchange != NULL ? &passenger : agent, 2 + (int32_t)site);
    EndWorldLighting();
    EndMode3D();
    rlSetClipPlanes(0.05, 4000.0);
    DrawTargetAtmosphere(target, clock);
    EndTextureMode();
    PresentTarget(target, destination);
    DrawCrewLabels(camera, destination);

    WorldLabel labels[3] = {
        {{CC_LOCAL_SITE_ENTRANCE_X -
              (site == CC_LOCAL_SITE_DRAGON_CAVE ? 4.5f : 0.0f),
          site == CC_LOCAL_SITE_DRAGON_CAVE ? 10.2f : 3.55f,
          CC_LOCAL_SITE_ENTRANCE_Z},
         CcLocalSiteName(sim, site),
         site == CC_LOCAL_SITE_DRAGON_CAVE ? WORLD_DANGER :
         site == CC_LOCAL_SITE_GOBLIN_CAVE ? WORLD_VIOLET : WORLD_GOLD},
        {{CC_LOCAL_SITE_CARRIAGE_X, 2.65f, CC_LOCAL_SITE_CARRIAGE_Z},
         site == CC_LOCAL_SITE_DRAGON_CAVE ? "GOBLIN NETWORK / F" :
                                             "CARRIAGE / F",
         site == CC_LOCAL_SITE_DRAGON_CAVE ? WORLD_VIOLET : WORLD_TEAL},
        {{agent->position.x, agent->position.y + 2.45f, agent->position.z},
         "YOU", WORLD_TEAL}
    };
    DrawLabels(labels, travelling ? 1 : 3, camera, destination);
    const char *site_caption =
        site == CC_LOCAL_SITE_DRAGON_CAVE ?
            "SHEER ROOST CLIFF  /  NO EXTERNAL APPROACH" :
        site == CC_LOCAL_SITE_GOBLIN_CAVE ?
            "OFF-ROAD FOOT TRAIL  /  CARRIAGE LEFT AT ROAD EDGE" :
            "SEPARATE LOCAL MAP  /  THE CARRIAGE REMAINS PARKED";
    DrawViewportText(
        travelling ?
            returning ? "CARRIAGE VIEW  /  RETURNING TO TOWN" :
                        site == CC_LOCAL_SITE_GOBLIN_CAVE ?
                            "CARRIAGE VIEW  /  HIDDEN TRAILHEAD AHEAD" :
                            "CARRIAGE VIEW  /  THE SITE ROAD AHEAD" :
            site_caption,
        destination, 18, 18, 10, WORLD_INK);
}

static void DrawJourneyAftermath3D(const CcSim *sim,
                                   const CcSettlement *place)
{
    if (sim == NULL || place == NULL ||
        sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_NONE ||
        sim->journey.destination_id != place->id) return;
    const float x = 47.35f;
    const float z = 31.05f;
    rlPushMatrix();
    rlTranslatef(0.0f, CcLocalTerrainHeightAt(x, z), 0.0f);
    if (sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT) {
        DrawBox((Vector3){x, 0.88f, z}, (Vector3){0.14f, 1.76f, 0.14f},
                (Color){82, 61, 46, 255});
        DrawBox((Vector3){x, 1.46f, z - 0.03f},
                (Vector3){0.08f, 0.72f, 0.92f}, WORLD_TEAL);
        DrawBox((Vector3){x + 0.62f, 0.20f, z + 0.48f},
                (Vector3){0.92f, 0.40f, 0.42f},
                (Color){108, 73, 48, 255});
        DrawCylinderEx((Vector3){x + 0.12f, 0.16f, z + 0.78f},
                       (Vector3){x + 1.28f, 0.22f, z + 0.92f},
                       0.075f, 0.065f, 7, (Color){91, 61, 43, 255});
    } else {
        DrawBox((Vector3){x, 0.92f, z}, (Vector3){0.13f, 1.84f, 0.13f},
                (Color){73, 53, 47, 255});
        DrawBox((Vector3){x, 1.50f, z - 0.03f},
                (Vector3){0.08f, 0.78f, 0.94f}, WORLD_VIOLET);
        DrawBox((Vector3){x + 0.58f, 0.30f, z + 0.44f},
                (Vector3){0.74f, 0.60f, 0.74f},
                (Color){94, 64, 50, 255});
        DrawSmallSphere((Vector3){x + 0.58f, 0.68f, z + 0.44f},
                        0.12f, WORLD_GOLD);
    }
    rlPopMatrix();
}

static void DrawTownRaidStaging(const CcLocalCourse *course, Vector3 focus)
{
    if (course == NULL || course->scene != CC_LOCAL_SCENE_STREET ||
        !course->alarm_active || !course->combat_origin_valid) return;
    Vector3 origin = course->combat_origin;
    if (!TerrainPointInPlayableWorld(origin.x, origin.z) ||
        !SceneryPointVisible(origin.x, origin.z, focus)) return;

    for (int32_t mark = -2; mark <= 2; ++mark) {
        float z = origin.z + (float)mark * 0.82f;
        float x = origin.x + ((mark & 1) != 0 ? 0.12f : -0.05f);
        float height = CcLocalTerrainHeightAt(x, z);
        DrawBox((Vector3){x, height + 0.025f, z},
                (Vector3){0.16f, 0.05f, 0.58f},
                mark == 0 ? WORLD_GOLD : WORLD_TEAL);
    }
    for (int32_t side = -1; side <= 1; side += 2) {
        float z = origin.z + (float)side * 2.35f;
        float height = CcLocalTerrainHeightAt(origin.x - 0.20f, z);
        DrawBox((Vector3){origin.x - 0.20f, height + 0.055f, z},
                (Vector3){1.05f, 0.11f, 0.42f},
                ShadeColor((Color){87, 76, 63, 255}, 0.84f));
    }
    DrawHeraldryBanner3D(
        CcHeraldryForSpecial(CC_SPECIAL_BANNER_BANDITS),
        (Vector3){origin.x - 1.15f,
                  CcLocalTerrainHeightAt(origin.x - 1.15f,
                                         origin.z - 1.55f),
                  origin.z - 1.55f},
        0.0f, 0.66f, 0.0f);
}

static void DrawSettlementCreatures(const CcSim *sim,
                                    const CcSettlement *place,
                                    float clock, Vector3 scenery_focus)
{
    if (sim == NULL || place == NULL) return;
    const CcGoblinCult *goblins = &sim->goblins;

    if (place->cow_adults + place->cow_calves > 0 &&
        SceneryPointVisible(63.0f, 38.3f, scenery_focus)) {
        CcCreaturePose cow_pose = CcCreatureSteppedPose(
            CC_CREATURE_COW, clock * 1.25f, true);
        CcCreatureRigPose controlled_cow;
        bool controlled = ResolveControlledCreatureGait(
            CREATURE_GAIT_STREET_COW, CC_CREATURE_RIG_COW,
            CC_CREATURE_RIG_GAIT_WALK, clock,
            0.34f, 0.48f, true, TerrainWorldPoint(63.0f, 38.3f),
            0.72f * PI, CC_LOCAL_SCENE_STREET, &controlled_cow);
        (void)DrawCreatureGait3D(
            CC_CREATURE_COW, cow_pose,
            TerrainWorldPoint(63.0f, 38.3f), 0.72f * PI, 0.84f,
            (Color){177, 162, 132, 255}, clock * 1.25f, true,
            controlled ? &controlled_cow : NULL);
    }

    if (place->sheep_adults + place->sheep_lambs > 0 &&
        SceneryPointVisible(61.65f, 39.15f, scenery_focus)) {
        (void)DrawCreatureGait3D(
            CC_CREATURE_SHEEP,
            CcCreatureSteppedPose(CC_CREATURE_SHEEP,
                                   clock * 1.42f, true),
            TerrainWorldPoint(61.65f, 39.15f), 0.58f * PI, 0.42f,
            (Color){228, 220, 185, 255}, clock * 1.42f, true, NULL);
        if (place->sheep_adults + place->sheep_lambs >= 32) {
            (void)DrawCreature3D(
                CC_CREATURE_SHEEP, CC_CREATURE_POSE_IDLE,
                TerrainWorldPoint(60.55f, 38.75f), 0.45f * PI, 0.37f,
                (Color){207, 200, 174, 255});
        }
    }

    bool raid_at_place = goblins->tribute_target_id == place->id &&
        (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND ||
         goblins->tribute_phase == CC_GOBLIN_TRIBUTE_RETURNING);
    if (raid_at_place && SceneryPointVisible(47.2f, 34.2f,
                                             scenery_focus)) {
        CcCreaturePose raid_pose = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_RAIDER, clock * 5.0f, true);
        CcCreaturePose scout_pose = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_SCAVENGER, clock * 5.0f + PI, true);
        (void)DrawCreature3D(
            CC_CREATURE_GOBLIN_RAIDER, raid_pose,
            TerrainWorldPoint(46.7f, 34.0f), 0.62f * PI, 1.02f,
            DragonCourtHairColor(&sim->dragon));
        (void)DrawCreature3D(
            CC_CREATURE_GOBLIN_SCAVENGER, scout_pose,
            TerrainWorldPoint(48.0f, 34.7f), 0.62f * PI, 0.94f,
            DragonCourtHairColor(&sim->dragon));
    }

}

void CcLocalDrawStreet3D(const CcSim *sim, const CcLocalAgent *agent,
                         const CcLocalCourse *course,
                         bool conversation,
                         const CcLocalConvoyState *convoy, float clock,
                         RenderTexture2D target, Rectangle destination)
{
    CcLocalBindPlace(sim);
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    PrepareRuntimeAssets(StreetRuntimeAssetMask(place));
    bool convoy_visible = convoy != NULL &&
        (convoy->phase == CC_LOCAL_CONVOY_DEPARTING ||
         convoy->phase == CC_LOCAL_CONVOY_ARRIVING);
    bool arrival_pan = convoy_visible &&
        convoy->phase == CC_LOCAL_CONVOY_ARRIVING;
    CcLocalAgent convoy_subject = agent != NULL ? *agent : (CcLocalAgent){0};
    CcLocalAgent convoy_hero = {0};
    Vector3 convoy_hero_position = {0};
    if (convoy_visible) {
        convoy_subject.position = convoy->town_position;
        convoy_subject.facing_yaw = convoy->town_heading_yaw;
        convoy_hero_position = LocalPoint(
            convoy->town_position, -1.65f + (float)(crew_seat % 4) * 0.85f,
            0.0f, -2.10f + (float)(crew_seat / 4) * 1.25f,
            convoy->town_heading_yaw);
        convoy_hero_position.y = CcLocalTerrainHeightAt(
            convoy_hero_position.x, convoy_hero_position.z);
        if (agent != NULL) {
            PrepareHeroDisplayAt(agent, convoy_hero_position,
                                 CC_LOCAL_SCENE_STREET,
                                 convoy->town_heading_yaw, &convoy_hero);
        }
    }
    const CcLocalAgent *camera_subject = convoy_visible ?
        &convoy_subject : agent;
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(place);
    Camera3D base_camera = arrival_pan ?
        TownArrivalCamera(convoy, clock, target.texture.height) :
        CcLocalStreetCameraInternal(
            camera_subject, clock, true, target.texture.height);
    Camera3D camera = CcLocalCombatCameraInternal(
        base_camera, camera_subject, course, clock, true,
        target.texture.height);
    const CcLocalAgent *conversation_partner =
        conversation && conversation_focus_active ? &conversation_focus_agent :
        conversation && course != NULL && course->situation_witness_active ?
            &course->situation_witness : NULL;
    camera = CcLocalConversationCameraInternal(
        camera, camera_subject, conversation_partner,
        conversation_partner != NULL, clock, true, target.texture.height);
    camera = StreetCameraClearVolume(camera);
    if (camera_subject != NULL && !arrival_pan &&
        conversation_partner == NULL) {
        camera = KeepHeroInsideStreetFrame(
            camera,
            Vector3Add(camera_subject->position,
                       (Vector3){0.0f, 1.05f, 0.0f}),
            target.texture.height,
            (Rectangle){0.10f, 0.12f, 0.80f, 0.76f});
        camera = SnapCameraToArtPixels(camera, target.texture.height);
    }
    RememberPresentedCamera(CC_LOCAL_SCENE_STREET, camera, camera_subject,
                            target.texture.width, target.texture.height);
    int32_t presented_scene = arrival_pan ?
        CC_LOCAL_TOWN_SCENE_ARRIVAL : street_camera_rig.shot;
    ArtComposition street_art = StreetCameraShotAt(
        CC_LOCAL_TOWN_SCENE_ARRIVAL)->art;
    street_art.light_profile = StreetLightProfile(
        place, StreetCameraShotAt(CC_LOCAL_TOWN_SCENE_ARRIVAL)->art.light_profile);
    street_art.focal_point = camera.target;
    Color background = ArtLightBackground(street_art.light_profile);
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    Vector3 scenery_focus = camera.target;
    Color kingdom = KingdomColor3D(sim, place->kingdom_id);
    BeginTextureMode(target);
    ClearBackground(background);
    rlSetClipPlanes(0.10, 512.0);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &street_art);

    uint32_t town_conditions = CcSimTownConditions(sim, place->id);
    DrawExteriorTerrain(place, town_conditions, scenery_focus, clock);
    DrawTownRaidStaging(course, scenery_focus);
    const CcLocalAgent *sightline_opponent = conversation_partner != NULL ?
        conversation_partner : course != NULL && course->alarm_active ?
        CombatCameraOpponent(course, agent) : NULL;
    Vector3 player_sightline = agent != NULL ?
        Vector3Add(agent->position, (Vector3){0.0f, 1.02f, 0.0f}) :
        (Vector3){0};
    Vector3 opponent_sightline = sightline_opponent != NULL ?
        Vector3Add(sightline_opponent->position,
                   (Vector3){0.0f, 1.02f, 0.0f}) :
        player_sightline;
    enum { PLATFORM_REVEAL_COUNT = sizeof(STREET_PLATFORMS) /
                                    sizeof(STREET_PLATFORMS[0]) };
    static WorldBuildingRevealState platform_reveals[PLATFORM_REVEAL_COUNT];
    static WorldBuildingRevealState gate_reveal;
    static CcId reveal_place = 0;
    static uint32_t reveal_seed = 0;
    static float reveal_clock = 0.0f;
    bool reset_reveals = reveal_place != place->id ||
        reveal_seed != street_terrain_seed || clock < reveal_clock;
    float reveal_dt = reset_reveals ? 0.0f :
        CombatClamp(clock - reveal_clock, 0.0f, 0.05f);
    reveal_place = place->id;
    reveal_seed = street_terrain_seed;
    reveal_clock = clock;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        Rectangle footprint = {
            platform->x, platform->z, platform->width, platform->depth
        };
        float overlap = agent != NULL ?
            CameraStreetPlatformSubjectOverlap(
                camera, platform, player_sightline, opponent_sightline) :
            0.0f;

        bool sightline_cut = overlap >
            (platform_reveals[i].revealed ? 0.0005f : 0.0015f);
        UpdateBuildingReveal(&platform_reveals[i], sightline_cut,
                             reset_reveals, reveal_dt, 0.18f, 5.0f);
        if (!SceneryFootprintVisible(footprint, scenery_focus)) continue;
        Color color = CoursePlatformColor(platform->style);
        float base = PlatformBaseHeight(platform);
        float render_height = platform->height +
            (fminf(platform->height, 0.12f) - platform->height) *
            platform_reveals[i].amount;
        float top = base + render_height;
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          base + render_height * 0.5f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.10f),
                          render_height,
                          fmaxf(0.10f, platform->depth - 0.10f)}, color);
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          top + 0.025f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.04f), 0.05f,
                          fmaxf(0.10f, platform->depth - 0.04f)},
                i == 0 ? (Color){124, 145, 131, 255} :
                         Fade(WORLD_GOLD, 0.70f));
    }
    DrawObstacleCourse(scenery_focus);
    Vector3 foreground_reveal_world = {
        camera_subject->position.x, camera_subject->position.y + 1.05f,
        camera_subject->position.z,
    };
    secondary_reveal_valid = sightline_opponent != NULL;
    secondary_reveal_subject = opponent_sightline;
    DrawWorldBuildings(place, town_conditions, kingdom, scenery_focus, profile, camera,
                       foreground_reveal_world,
                       target.texture.width, target.texture.height, clock);
    WorldBuilding primary_building = ActivePrimaryBuilding();
    if (profile->function == CC_SETTLEMENT_MARKET &&
        SceneryFootprintVisible(primary_building.footprint, scenery_focus)) {
        float reveal_cut_height = foreground_reveal_world.y - 0.30f;
        SetWorldForegroundReveal(
            world_building_reveals[profile->primary_building].amount,
            reveal_cut_height);
        (void)DrawAuthoredMarket(place);
        SetWorldForegroundReveal(0.0f, reveal_cut_height);
    }
    DrawCastle(sim, place, kingdom, profile, scenery_focus, camera,
               foreground_reveal_world,
               target.texture.width, target.texture.height, clock);
    if (active_opening_step == CC_LOCAL_OPENING_COMPLETE &&
        SceneryFootprintVisible(CARRIAGE_FOOTPRINT, scenery_focus)) {
        if (!convoy_visible) {
            bool carriage_targeted = agent != NULL &&
                agent->world_target == CC_LOCAL_WORLD_TARGET_CARRIAGE;
            carriage_targeted = carriage_targeted ||
                (renderer_movement_preview.valid &&
                 renderer_movement_preview.world_target ==
                     CC_LOCAL_WORLD_TARGET_CARRIAGE);
            DrawCarriage3D(&sim->player, carriage_targeted, clock);
            DrawStableHorseTeam(sim, clock);
            if (carriage_targeted) DrawCarriageTargetMarker3D(clock);
        }
    }
    if (convoy_visible) {
        DrawRoadCarriage(convoy->town_position, sim, clock,
                         convoy->pace,
                         convoy->town_heading_yaw, true, false);
        if (agent != NULL) DrawRobotShell(&convoy_hero);
    }
    if (SceneryPointVisible(CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z,
                            scenery_focus)) {
        DrawNotice3D(sim);
        if (sim->iron_ledger_reserve > 0) {
            DrawHeraldryBanner3D(
                CcHeraldryForSpecial(CC_SPECIAL_BANNER_IRON_LEDGER),
                (Vector3){CC_LOCAL_NOTICE_X - 1.35f,
                          CcLocalTerrainHeightAt(CC_LOCAL_NOTICE_X - 1.35f,
                                                 CC_LOCAL_NOTICE_Z + 0.35f),
                          CC_LOCAL_NOTICE_Z + 0.35f},
                0.0f, 0.45f, clock);
        }
    }
    DrawWorldTrees(scenery_focus, kingdom);
    float gate_overlap = agent != NULL ?
        CameraWayfarerGateSubjectOverlap(
            camera, player_sightline, opponent_sightline) : 0.0f;
    UpdateBuildingReveal(&gate_reveal,
        gate_overlap > (gate_reveal.revealed ? 0.0005f : 0.0015f),
        reset_reveals, reveal_dt, 0.18f, 5.0f);
    DrawRoomLandmarks(place, kingdom, profile, scenery_focus,
                      gate_reveal.amount);
    DrawPlaceLandmarks(kingdom, scenery_focus);
    if (SceneryPointVisible(47.35f, 31.05f, scenery_focus)) {
        DrawJourneyAftermath3D(sim, place);
    }

    int32_t crates = place->stock[CC_GOOD_FOOD] / 12;
    if (crates > 4) crates = 4;
    for (int32_t i = 0; i < crates; ++i) {
        const NavPlatform *crate = &STREET_CRATE_PLATFORMS[i];
        Rectangle footprint = {
            crate->x, crate->z, crate->width, crate->depth
        };
        if (!SceneryFootprintVisible(footprint, scenery_focus)) continue;
        DrawBox((Vector3){crate->x + crate->width * 0.5f,
                          PlatformBaseHeight(crate) + crate->height * 0.5f,
                          crate->z + crate->depth * 0.5f},
                (Vector3){0.62f, 0.60f, 0.62f},
                (Color){177, 116, 55, 255});
    }
    const CcDungeon *dungeon = DungeonAt(sim, place->id);
    if (dungeon != NULL &&
        SceneryFootprintVisible(DUNGEON_FOOTPRINT, scenery_focus)) {
        DrawSiteRoadGate(CC_LOCAL_DUNGEON_X, CC_LOCAL_DUNGEON_Z,
                         WORLD_VIOLET);
    }
    if (place->id == sim->goblins.lair_settlement_id &&
        SceneryPointVisible(CC_LOCAL_DRAGON_CAVE_X,
                            CC_LOCAL_DRAGON_CAVE_Z, scenery_focus)) {
        DrawSiteRoadGate(
            CC_LOCAL_DRAGON_CAVE_X, CC_LOCAL_DRAGON_CAVE_Z,
            WORLD_VIOLET);
    }
    DrawSettlementCreatures(sim, place, clock, scenery_focus);

    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[0].x, STREET_PEOPLE[0].y),
        0.96f, -0.55f, UINT32_C(0x73747201), CC_NPC_ROLE_MERCHANT,
        (Color){223, 151, 68, 255}, clock * 1.2f, CC_TRAVERSAL_IDLE,
        scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[1].x, STREET_PEOPLE[1].y),
        1.02f, 1.70f, UINT32_C(0x73747202), CC_NPC_ROLE_GUARD,
        kingdom, clock + 1.0f, CC_TRAVERSAL_IDLE, scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[2].x, STREET_PEOPLE[2].y),
        0.92f, 0.35f, UINT32_C(0x73747203), CC_NPC_ROLE_LABORER,
        (Color){97, 154, 137, 255}, clock + 2.0f, CC_TRAVERSAL_IDLE,
        scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[3].x, STREET_PEOPLE[3].y),
        0.88f, 2.40f, UINT32_C(0x73747204), CC_NPC_ROLE_HEALER,
        (Color){168, 112, 128, 255}, clock * 0.8f + 3.0f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    bool hungry_crowd = place->hunger >= 30;
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[4].x, STREET_PEOPLE[4].y),
        0.82f, -0.40f, UINT32_C(0x73747205),
        hungry_crowd ? CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_TRAVELLER,
        hungry_crowd ? WORLD_DANGER : kingdom, clock * 0.6f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    bool underworld_present = HasSmugglerRoad(sim, place->id) ||
                              place->security < 50;
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[5].x, STREET_PEOPLE[5].y),
        0.88f, 2.75f, UINT32_C(0x73747206),
        underworld_present ? CC_NPC_ROLE_SCOUT : CC_NPC_ROLE_TRAVELLER,
        underworld_present ? WORLD_VIOLET : kingdom, clock * 0.7f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    float settlement_pressure = fmaxf(
        Clamp((float)place->hunger / 100.0f, 0.0f, 1.0f),
        Clamp((50.0f - (float)place->security) / 50.0f, 0.0f, 1.0f));
    if (course != NULL && course->alarm_active) settlement_pressure = 1.0f;

    DrawAmbientNpcRoute3D(
        (Vector2){13.60f, 25.80f}, (Vector2){14.80f, 26.60f}, 0.88f,
        UINT32_C(0x64697301), CC_NPC_ROLE_LABORER,
        (Color){143, 118, 65, 255}, clock, 0.08f, settlement_pressure,
        scenery_focus);
    DrawAmbientNpcRoute3D(
        (Vector2){24.10f, 53.20f}, (Vector2){25.70f, 53.20f}, 0.90f,
        UINT32_C(0x64697302), CC_NPC_ROLE_SCOUT,
        (Color){103, 103, 112, 255}, clock, 0.29f, settlement_pressure,
        scenery_focus);
    DrawAmbientNpcRoute3D(
        (Vector2){35.20f, 26.65f}, (Vector2){36.75f, 27.25f}, 0.94f,
        UINT32_C(0x64697303), CC_NPC_ROLE_LABORER,
        (Color){174, 94, 53, 255}, clock, 0.47f, settlement_pressure,
        scenery_focus);
    DrawAmbientNpcRoute3D(
        (Vector2){49.25f, 51.05f}, (Vector2){50.15f, 52.35f}, 0.92f,
        UINT32_C(0x64697304), CC_NPC_ROLE_TRAVELLER,
        (Color){117, 145, 116, 255}, clock, 0.72f, settlement_pressure,
        scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(74.65f, 31.95f), 1.02f, 1.50f,
        UINT32_C(0x64697305), CC_NPC_ROLE_GUARD, kingdom,
        clock * 0.40f + 2.9f, CC_TRAVERSAL_IDLE, scenery_focus);
    DrawAmbientNpcRoute3D(
        (Vector2){84.80f, 51.80f}, (Vector2){86.30f, 51.00f}, 0.94f,
        UINT32_C(0x64697306), CC_NPC_ROLE_LABORER,
        (Color){161, 128, 68, 255}, clock, 0.88f, settlement_pressure,
        scenery_focus);
    if (sim->resolved_journey_outcome != CC_JOURNEY_OUTCOME_NONE &&
        sim->journey.destination_id == place->id) {
        DrawVisibleNpcFigure3D(
            TerrainWorldPoint(46.80f, 31.15f), 0.86f, -1.10f,
            UINT32_C(0x61667401), CC_NPC_ROLE_GUARD, WORLD_TEAL,
            clock * 0.64f + 1.4f, CC_TRAVERSAL_IDLE, scenery_focus);
        if (sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT) {
            DrawVisibleNpcFigure3D(
                TerrainWorldPoint(47.65f, 30.55f), 0.82f, -0.85f,
                UINT32_C(0x61667402), CC_NPC_ROLE_HEALER, WORLD_GOLD,
                clock * 0.58f + 2.1f, CC_TRAVERSAL_IDLE, scenery_focus);
        }
    }
    DrawCourseRunners(course, scenery_focus);
    if (course != NULL && agent->combat.target_index >= 0 &&
        agent->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
        DrawSelectedTarget(
            &course->raiders[agent->combat.target_index]);
    }
    if (!convoy_visible) {
        DrawAgentPath(agent, false);
        DrawRobotShell(agent);
        DrawCombatSword(agent);
        DrawCombatSkillTell(agent);
        DrawCombatImpact(agent);
    }
    DrawCrew(convoy_visible ? &convoy_hero : agent, 0);
    EndWorldLighting();
    EndMode3D();
    rlSetClipPlanes(0.05, 4000.0);
    DrawTargetAtmosphere(target, clock);
    EndTextureMode();
    PresentTarget(target, destination);
    DrawCrewLabels(camera, destination);
    bool combat_nearby = CcLocalCourseHasNearbyHostile(course, agent);
    bool combat_presentation = course != NULL && course->alarm_active &&
                               camera.projection == CAMERA_PERSPECTIVE;

    int32_t scene_index = presented_scene;
    if (!combat_presentation && conversation_partner == NULL &&
        scene_index >= 0 &&
        scene_index < CC_LOCAL_PLACE_SCENE_COUNT) {
        const char *scene_name = profile->scene[scene_index].name;
        int32_t title_width = CcOverlayMeasureText(scene_name, 10);
        DrawRectangleRounded(
            ViewportRectangle(destination, 11.0f, 10.0f,
                              (float)title_width + 16.0f, 18.0f),
            0.24f, 4, (Color){4, 10, 14, 202});
        DrawViewportText(scene_name, destination, 19, 14, 10, WORLD_GOLD);
    }
    if (!combat_nearby && !convoy_visible &&
        conversation_partner == NULL) {
        DrawStreetTraversalPortals(agent, camera, destination,
                                   target.texture.width,
                                   target.texture.height);
    }

    char witness_label[96];
    WorldLabel labels[20];
    int32_t count = 0;
    if (convoy_visible && agent != NULL) {
        labels[count++] = (WorldLabel){
            {convoy_hero_position.x, convoy_hero_position.y + 2.50f,
             convoy_hero_position.z}, "YOU", WORLD_TEAL};
    }
    char primary_hall_label[96];
    (void)snprintf(primary_hall_label, sizeof(primary_hall_label),
                   "%s  /  approach, then press F", profile->primary_hall);
    if (AgentNearLabel(agent, 50.0f, 21.0f, 8.0f)) {
        labels[count++] = (WorldLabel){{50.0f,
                                        TerrainFootprintHeight(
                                            primary_building.footprint) +
                                            primary_building.height + 0.42f,
                                        21.0f},
                                       primary_hall_label, WORLD_GOLD};
    }
    bool carriage_targeted = agent != NULL &&
        agent->world_target == CC_LOCAL_WORLD_TARGET_CARRIAGE;
    if (active_opening_step == CC_LOCAL_OPENING_COMPLETE &&
        (carriage_targeted ||
        AgentNearLabel(agent, CC_LOCAL_CARRIAGE_X,
                       CC_LOCAL_CARRIAGE_Z, 7.0f))) {
        labels[count++] = (WorldLabel){{CC_LOCAL_CARRIAGE_X,
                                        TerrainFootprintHeight(
                                            CARRIAGE_FOOTPRINT) + 2.28f,
                                        CC_LOCAL_CARRIAGE_Z},
                                       carriage_targeted ?
                                           "Carriage targeted / approaching" :
                                           "Carriage / click or F",
                                       carriage_targeted ? WORLD_TEAL :
                                                           WORLD_GOLD};
    }
    if (AgentNearLabel(agent, CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z, 6.0f)) {
        labels[count++] = (WorldLabel){{CC_LOCAL_NOTICE_X,
                                        CcLocalTerrainHeightAt(
                                            CC_LOCAL_NOTICE_X,
                                            CC_LOCAL_NOTICE_Z) + 1.82f,
                                        CC_LOCAL_NOTICE_Z},
                                       active_opening_step ==
                                               CC_LOCAL_OPENING_MEET_MARA ?
                                           "Harvest board" :
                                           profile->notice_board,
                                       WORLD_INK};
    }
    if (AgentNearLabel(agent, 11.80f, 0.82f, 7.0f)) {
        labels[count++] = (WorldLabel){{11.80f,
                                        CcLocalTerrainHeightAt(11.80f, 0.82f) +
                                            2.05f,
                                        0.82f},
                                       profile->training_yard, WORLD_GOLD};
    }
    if (AgentNearLabel(agent, 11.28f, 9.72f, 7.0f)) {
        labels[count++] = (WorldLabel){{11.28f,
                                        CcLocalTerrainHeightAt(11.28f, 9.72f) +
                                            1.12f,
                                        9.72f},
                                       "Swimming trench", WORLD_TEAL};
    }
    Rectangle compound_bounds = ActiveCompoundBounds();
    float compound_x = compound_bounds.x + compound_bounds.width * 0.5f;
    float compound_z = compound_bounds.y + compound_bounds.height * 0.5f;
    if (AgentNearLabel(agent, compound_x, compound_z, 10.0f)) {
        labels[count++] = (WorldLabel){{compound_x,
                                        CcLocalTerrainHeightAt(
                                            compound_x, compound_z) +
                                            ActiveCompoundMaximumHeight() +
                                            0.55f,
                                        compound_z},
                                       profile->compound, kingdom};
    }
    for (int32_t i = 0;
         i < CC_LOCAL_PLACE_LANDMARK_COUNT && count < 20; ++i) {
        const CcLocalPlaceLandmark *landmark =
            ActivePlaceLandmarkAt(i);
        if (landmark == NULL) continue;
        float center_x = landmark->x + landmark->width * 0.5f;
        float center_z = landmark->z + landmark->depth * 0.5f;
        if (!AgentNearLabel(agent, center_x, center_z, 7.5f)) continue;
        Rectangle footprint = PlaceLandmarkFootprint(landmark);
        labels[count++] = (WorldLabel){
            {center_x, TerrainFootprintHeight(footprint) +
                       landmark->height + 0.42f, center_z},
            landmark->name,
            landmark->family == CC_LOCAL_LANDMARK_EXPEDITION ?
                WORLD_VIOLET : kingdom};
    }
    for (int32_t i = 0;
         i < CC_LOCAL_PLACE_ROAD_COUNT && count < 20; ++i) {
        const CcLocalPlaceRoad *road = ActivePlaceRoadAt(i);
        if (road == NULL) continue;
        float center_x = road->x + road->width * 0.5f;
        float center_z = road->z + road->depth * 0.5f;
        if (!AgentNearLabel(agent, center_x, center_z, 5.5f)) continue;
        labels[count++] = (WorldLabel){
            {center_x, CcLocalTerrainHeightAt(center_x, center_z) + 0.42f,
             center_z},
            road->name, WORLD_MUTED};
    }
    if (sim->resolved_journey_outcome != CC_JOURNEY_OUTCOME_NONE &&
        sim->journey.destination_id == place->id &&
        AgentNearLabel(agent, 47.35f, 31.05f, 7.0f)) {
        labels[count++] = (WorldLabel){
            {47.35f, CcLocalTerrainHeightAt(47.35f, 31.05f) + 2.05f,
             31.05f},
            sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT ?
                "Road guards" : "Company token",
            sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT ?
                WORLD_TEAL : WORLD_VIOLET};
    }
    if (course != NULL && course->situation_witness_active) {
        const CcSituation *situation = CcSimSituation(
            sim, course->situation_witness_id);
        const CcCharacter *character = CcSimCharacter(
            sim, course->situation_witness_character_id);
        if (situation != NULL && character != NULL &&
            AgentNearLabel(agent, course->situation_witness.position.x,
                           course->situation_witness.position.z, 6.0f)) {
            (void)snprintf(witness_label, sizeof(witness_label),
                           "%.31s / %s / F", character->name,
                           CcCharacterActivityName(character->activity));
            labels[count++] = (WorldLabel){
                {course->situation_witness.position.x,
                 course->situation_witness.position.y + 2.18f,
                 course->situation_witness.position.z},
                witness_label,
                situation->status == CC_SITUATION_RESOLVED ? WORLD_TEAL :
                sim->player.accepted_situation_id == situation->id ?
                    WORLD_GOLD : WORLD_DANGER};
        }
    }
    if (dungeon != NULL &&
        AgentNearLabel(agent, CC_LOCAL_DUNGEON_X,
                       CC_LOCAL_DUNGEON_Z, 8.0f)) {
        labels[count++] = (WorldLabel){{CC_LOCAL_DUNGEON_X,
                                        TerrainFootprintHeight(
                                            DUNGEON_FOOTPRINT) + 3.38f,
                                        CC_LOCAL_DUNGEON_Z - 0.70f},
                                       TextFormat("Road to %s",
                                                  dungeon->name),
                                       WORLD_VIOLET};
    }
    if (place->id == sim->dragon.lair_settlement_id &&
        AgentNearLabel(agent, CC_LOCAL_DRAGON_CAVE_X,
                       CC_LOCAL_DRAGON_CAVE_Z, 9.0f)) {
        labels[count++] = (WorldLabel){
            {CC_LOCAL_DRAGON_CAVE_X,
             CcLocalTerrainHeightAt(CC_LOCAL_DRAGON_CAVE_X,
                                    CC_LOCAL_DRAGON_CAVE_Z) + 2.75f,
             CC_LOCAL_DRAGON_CAVE_Z},
            sim->dragon.slain ? "Ashen roost / sheer cliff" :
                                "Dragon roost / sheer cliff / no road",
            sim->dragon.slain ? WORLD_VIOLET : WORLD_DANGER};
    }
    if (place->id == sim->goblins.lair_settlement_id &&
        AgentNearLabel(agent, CC_LOCAL_DRAGON_CAVE_X,
                       CC_LOCAL_DRAGON_CAVE_Z, 9.0f)) {
        labels[count++] = (WorldLabel){
            {CC_LOCAL_DRAGON_CAVE_X,
             CcLocalTerrainHeightAt(CC_LOCAL_DRAGON_CAVE_X,
                                    CC_LOCAL_DRAGON_CAVE_Z) + 2.75f,
             CC_LOCAL_DRAGON_CAVE_Z},
            "Hidden trail to goblin dungeon", WORLD_VIOLET};
    }
    if (!interaction_ui_enabled && !combat_nearby && conversation_partner == NULL) {
        DrawLabels(labels, count, camera, destination);
    }
    if (combat_nearby) {
        DrawCombatBar(agent, camera, destination, WORLD_TEAL);
        if (combat_presentation) {
            DrawCombatBar(CombatCameraOpponent(course, agent), camera,
                          destination, WORLD_GOLD);
        } else {
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                DrawCombatBar(&course->runners[i].agent, camera,
                              destination, course->runners[i].marker_color);
            }
            for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
                DrawCombatBar(&course->raiders[i], camera, destination,
                              i == agent->combat.target_index ? WORLD_GOLD :
                                                               WORLD_DANGER);
            }
        }
    }
    if (draw_hero_rig_debug &&
        agent->morphology == CC_MORPHOLOGY_BIPED) {
        DrawViewportText(
            TextFormat("BIOMECHANICAL BIPED / %d JOINTS / %s / %s / MUSCLES + LIGAMENTS",
                       agent->humanoid.body.morphology.joint_count,
                       CcLocalTraversalName(agent->traversal),
                       CcHumanoidActionName(agent->humanoid.action)),
            destination, 18, 18, 10, WORLD_TEAL);
    } else if (draw_hero_rig_debug) {
        DrawViewportText(
            TextFormat("ROBOTIC %s / %d LEGS / %s / %s / %s / %.0f%% CONTROL",
                       CcLocalAgentMorphologyName(agent),
                       agent->limb_rig.morphology.limb_count,
                       CcLocalTraversalName(agent->traversal),
                       CcLimbPaceName(agent->limb_rig.pace),
                       CcLimbSupportStateName(
                           agent->limb_rig.support_state),
                       agent->limb_rig.control_authority * 100.0f),
            destination, 18, 18, 10, WORLD_TEAL);
    }
}

static CcNpcRole InteriorKeeperRole(CcSettlementFunction function)
{
    switch (function) {
        case CC_SETTLEMENT_FARMING: return CC_NPC_ROLE_LABORER;
        case CC_SETTLEMENT_MINING: return CC_NPC_ROLE_LABORER;
        case CC_SETTLEMENT_MARKET: return CC_NPC_ROLE_MERCHANT;
        case CC_SETTLEMENT_FORTRESS: return CC_NPC_ROLE_GUARD;
        case CC_SETTLEMENT_CAPITAL: return CC_NPC_ROLE_MERCHANT;
        case CC_SETTLEMENT_DUNGEON_TOWN: return CC_NPC_ROLE_SCOUT;
    }
    return CC_NPC_ROLE_MERCHANT;
}

static Color InteriorWallColor(const CcLocalPlaceProfile *profile,
                               Color kingdom)
{
    if (profile == NULL) return WORLD_EARTH_SHADOW;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(WORLD_EARTH_SHADOW, WORLD_CROP, 0.18f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(WORLD_STONE_SHADOW,
                              WORLD_METAL_SHADOW, 0.28f);
        case CC_SETTLEMENT_MARKET:
            return BlendColor(WORLD_EARTH_SHADOW, WORLD_TEAL, 0.10f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(WORLD_STONE_SHADOW, kingdom, 0.18f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(WORLD_STONE, WORLD_GOLD, 0.12f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(WORLD_STONE_SHADOW, WORLD_VIOLET, 0.20f);
    }
    return WORLD_EARTH_SHADOW;
}

static void DrawInteriorServiceMark(const CcLocalPlaceProfile *profile,
                                    Color accent)
{
    if (profile == NULL) return;
    const float z = 0.586f;
    const float x = 6.55f;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            for (int32_t stalk = -1; stalk <= 1; ++stalk) {
                DrawBox((Vector3){x + (float)stalk * 0.30f, 1.48f, z},
                        (Vector3){0.09f, 0.88f, 0.035f}, accent);
                DrawBox((Vector3){x + (float)stalk * 0.30f + 0.10f,
                                  1.54f, z + 0.006f},
                        (Vector3){0.24f, 0.08f, 0.040f}, accent);
            }
            break;
        case CC_SETTLEMENT_MINING:
            DrawBox((Vector3){x, 1.48f, z},
                    (Vector3){1.10f, 0.22f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.48f, z},
                    (Vector3){0.22f, 1.10f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.48f, z + 0.004f},
                    (Vector3){0.62f, 0.62f, 0.045f},
                    ShadeColor(accent, 0.72f));
            break;
        case CC_SETTLEMENT_MARKET:
            DrawBox((Vector3){x, 1.62f, z},
                    (Vector3){1.16f, 0.10f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.37f, z},
                    (Vector3){0.10f, 0.60f, 0.040f}, accent);
            DrawBox((Vector3){x - 0.42f, 1.30f, z},
                    (Vector3){0.48f, 0.10f, 0.040f}, accent);
            DrawBox((Vector3){x + 0.42f, 1.30f, z},
                    (Vector3){0.48f, 0.10f, 0.040f}, accent);
            break;
        case CC_SETTLEMENT_FORTRESS:
            DrawBox((Vector3){x, 1.48f, z},
                    (Vector3){0.72f, 1.08f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.48f, z + 0.004f},
                    (Vector3){1.16f, 0.24f, 0.045f},
                    ShadeColor(accent, 0.74f));
            break;
        case CC_SETTLEMENT_CAPITAL:
            DrawBox((Vector3){x, 1.28f, z},
                    (Vector3){1.22f, 0.22f, 0.040f}, accent);
            for (int32_t point = -1; point <= 1; ++point) {
                DrawBox((Vector3){x + (float)point * 0.42f,
                                  1.58f + (point == 0 ? 0.12f : 0.0f), z},
                        (Vector3){0.24f,
                                  point == 0 ? 0.78f : 0.56f, 0.040f},
                        accent);
            }
            break;
        case CC_SETTLEMENT_DUNGEON_TOWN:
            DrawBox((Vector3){x, 1.46f, z},
                    (Vector3){0.72f, 0.86f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.90f, z},
                    (Vector3){1.02f, 0.12f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.03f, z},
                    (Vector3){0.24f, 0.18f, 0.040f}, accent);
            break;
    }
}

Vector3 CcLocalWarehouseGoodPosition(CcGood good)
{
    if (good < 0 || good >= CC_GOOD_COUNT) return (Vector3){0};
    return (Vector3){MARKET_SHELF_FOOTPRINT.x + 0.40f + (float)((int32_t)good % 7) * 0.57f,
        0.23f + (float)((int32_t)good / 7) * 0.82f,
        (int32_t)good < 7 ? 1.65f : 1.02f};
}

int32_t CcLocalWarehousePileCountInternal(const CcSettlement *place, CcGood good)
{
    if (place == NULL || good < 0 || good >= CC_GOOD_COUNT || place->stock[good] <= 0) return 0;
    return place->stock[good] < 3 ? place->stock[good] : 3;
}

static void DrawWarehouseStock(const CcSettlement *place)
{
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        Vector3 bay = CcLocalWarehouseGoodPosition((CcGood)good);
        int32_t piles = CcLocalWarehousePileCountInternal(place, (CcGood)good);
        for (int32_t pile = 0; pile < piles; ++pile) {
            Vector3 floor = {bay.x, bay.y + (float)(pile / 2) * 0.34f,
                bay.z - 0.17f + (float)(pile % 2) * 0.34f};
            DrawEconomicCargoUnit((CcGood)good, (Vector3){0}, floor,
                (Vector3){0.49f, 0.32f, 0.30f}, 0.0f);
        }
        /* A fixed tag keeps a sold-out bay available for restocking. */
        DrawBox((Vector3){bay.x, bay.y - 0.03f, bay.z + 0.34f},
            (Vector3){0.26f, 0.09f, 0.025f}, CargoBoxColor((CcGood)good));
    }
}

void CcLocalDrawInterior3D(const CcSim *sim, const CcLocalAgent *agent,
                           float clock, RenderTexture2D target,
                           Rectangle destination)
{
    CcLocalBindPlace(sim);
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    PrepareRuntimeAssets(0U);
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(place);
    Color kingdom = KingdomColor3D(sim, place->kingdom_id);
    Color identity = PlaceIdentityAccent(profile, kingdom);
    Color wall = InteriorWallColor(profile, kingdom);
    Camera3D camera = LocalCamera(true, agent->position);
    RememberPresentedCamera(CC_LOCAL_SCENE_MARKET, camera, agent,
                            target.texture.width, target.texture.height);
    Color background = ArtLightBackground(
        INTERIOR_ART_COMPOSITION.light_profile);
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(background);
    rlSetClipPlanes(0.10, 512.0);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);

    Color floor_dark = BlendColor(WORLD_EARTH_SHADOW, identity, 0.12f);
    Color floor_light = BlendColor(WORLD_EARTH, identity, 0.14f);
    DrawBox((Vector3){4.50f, -0.055f, 3.50f}, (Vector3){9.0f, 0.11f, 7.0f},
            floor_dark);
    for (int32_t row = 0; row < 2; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            DrawBox((Vector3){1.50f + (float)column * 3.0f, 0.006f,
                              1.75f + (float)row * 3.5f},
                    (Vector3){2.86f, 0.022f, 3.36f},
                    ((row + column) & 1) != 0 ? floor_light :
                                                        ShadeColor(floor_light,
                                                                   0.91f));
        }
    }
    DrawAgentPath(agent, true);
    DrawBox((Vector3){4.50f, 1.30f, 0.25f}, (Vector3){9.0f, 2.60f, 0.50f},
            wall);
    DrawBox((Vector3){0.25f, 1.30f, 3.50f}, (Vector3){0.50f, 2.60f, 7.0f},
            ShadeColor(wall, 0.86f));
    Color interior_timber = BlendColor(WORLD_WOOD_SHADOW, identity, 0.08f);
    Color interior_trim = BlendColor(WORLD_WOOD, identity, 0.16f);
    DrawBox((Vector3){4.50f, 0.54f, 0.515f},
            (Vector3){8.86f, 0.10f, 0.045f},
            interior_timber);
    DrawBox((Vector3){4.50f, 2.39f, 0.525f},
            (Vector3){8.86f, 0.13f, 0.065f}, interior_timber);
    for (int32_t bay = 0; bay < 5; ++bay) {
        float post_x = 0.82f + (float)bay * 1.92f;
        DrawBox((Vector3){post_x, 1.44f, 0.525f},
                (Vector3){0.14f, 1.82f, 0.07f}, interior_timber);
    }
    DrawBox((Vector3){0.515f, 2.39f, 3.50f},
            (Vector3){0.065f, 0.13f, 6.86f}, interior_timber);
    for (int32_t bay = 0; bay < 3; ++bay) {
        float post_z = 1.02f + (float)bay * 2.30f;
        DrawBox((Vector3){0.525f, 1.44f, post_z},
                (Vector3){0.07f, 1.82f, 0.14f}, interior_timber);
    }
    DrawBox((Vector3){6.55f, 1.42f, 0.525f},
            (Vector3){2.05f, 1.56f, 0.055f},
            ShadeColor(wall, 0.66f));
    DrawBox((Vector3){6.55f, 2.21f, 0.535f},
            (Vector3){2.24f, 0.10f, 0.065f}, identity);
    DrawInteriorServiceMark(profile, identity);
    DrawBox((Vector3){MARKET_COUNTER_FOOTPRINT.x +
                      MARKET_COUNTER_FOOTPRINT.width * 0.5f,
                      0.46f,
                      MARKET_COUNTER_FOOTPRINT.y +
                      MARKET_COUNTER_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_COUNTER_FOOTPRINT.width, 0.92f,
                      MARKET_COUNTER_FOOTPRINT.height},
            BlendColor(WORLD_WOOD, identity, 0.14f));
    for (int32_t panel = 0; panel < 3; ++panel) {
        float panel_x = MARKET_COUNTER_FOOTPRINT.x + 0.38f +
                        (float)panel * 0.66f;
        DrawBox((Vector3){panel_x, 0.47f,
                          MARKET_COUNTER_FOOTPRINT.y +
                              MARKET_COUNTER_FOOTPRINT.height + 0.035f},
                (Vector3){0.50f, 0.55f, 0.055f},
                panel == 1 ? (Color){85, 53, 43, 255} :
                             (Color){100, 62, 44, 255});
    }
    for (int32_t end = 0; end < 2; ++end) {
        float post_x = MARKET_COUNTER_FOOTPRINT.x +
                       (end == 0 ? 0.10f :
                                   MARKET_COUNTER_FOOTPRINT.width - 0.10f);
        DrawBox((Vector3){post_x, 0.48f,
                          MARKET_COUNTER_FOOTPRINT.y +
                              MARKET_COUNTER_FOOTPRINT.height + 0.065f},
                (Vector3){0.16f, 0.78f, 0.12f}, interior_timber);
    }
    DrawBox((Vector3){MARKET_COUNTER_FOOTPRINT.x +
                      MARKET_COUNTER_FOOTPRINT.width * 0.5f,
                      0.94f,
                      MARKET_COUNTER_FOOTPRINT.y +
                      MARKET_COUNTER_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_COUNTER_FOOTPRINT.width + 0.12f, 0.10f,
                      MARKET_COUNTER_FOOTPRINT.height + 0.12f},
            BlendColor(WORLD_WOOD_LIGHT, identity, 0.18f));

    float shelf_x = MARKET_SHELF_FOOTPRINT.x + MARKET_SHELF_FOOTPRINT.width * 0.5f;
    for (int32_t side = 0; side < 2; ++side) {
        float post_x = MARKET_SHELF_FOOTPRINT.x + 0.08f +
            (float)side * (MARKET_SHELF_FOOTPRINT.width - 0.16f);
        DrawBox((Vector3){post_x, 0.92f, 0.73f},
            (Vector3){0.12f, 1.84f, 0.12f}, interior_timber);
    }
    for (int32_t shelf = 0; shelf < 2; ++shelf) {
        DrawBox((Vector3){shelf_x, 0.18f + (float)shelf * 0.82f,
            shelf == 0 ? 1.65f : 1.02f},
            (Vector3){MARKET_SHELF_FOOTPRINT.width, 0.10f, 0.70f}, interior_trim);
    }
    DrawBox((Vector3){1.55f, 1.05f, 6.54f}, (Vector3){0.82f, 2.10f, 0.08f},
            (Color){37, 28, 30, 255});


    const float lamp_x[] = {3.05f, 8.10f};
    for (int32_t lamp = 0; lamp < 2; ++lamp) {
        DrawBox((Vector3){lamp_x[lamp], 1.76f, 0.62f},
                (Vector3){0.08f, 0.58f, 0.08f}, interior_timber);
        DrawBox((Vector3){lamp_x[lamp], 2.04f, 0.73f},
                (Vector3){0.34f, 0.08f, 0.28f}, interior_trim);
        DrawSmallSphere((Vector3){lamp_x[lamp], 1.78f, 0.76f},
                        0.14f, (Color){224, 160, 67, 255});
    }
    DrawBox((Vector3){4.48f, 0.032f, 3.88f},
            (Vector3){1.64f, 0.035f, 3.20f},
            ShadeColor(identity, 0.62f));
    DrawBox((Vector3){4.48f, 0.050f, 2.36f},
            (Vector3){1.84f, 0.045f, 0.11f}, identity);

    DrawWarehouseStock(place);
    CcNpcAppearance keeper = profile->function == CC_SETTLEMENT_MARKET ?
        CcNpcMaraAppearance() :
        CcNpcAppearanceGenerate(profile->keeper_seed,
                                InteriorKeeperRole(profile->function),
                                identity);
    DrawNpcAppearanceFigure3D(
        (Vector3){MARKET_PEOPLE[0].x, 0.0f, MARKET_PEOPLE[0].y},
        1.02f, 2.75f, &keeper, clock, CC_TRAVERSAL_IDLE);
    DrawRobotShell(agent);
    DrawCombatSword(agent);
    DrawCombatSkillTell(agent);
    DrawCrew(agent, 1);
    EndWorldLighting();
    DrawMarketEmberFlames(clock);
    EndMode3D();
    rlSetClipPlanes(0.05, 4000.0);
    DrawMarketPracticalLightHalos(camera, target, clock);
    EndTextureMode();
    PresentTarget(target, destination);
    DrawCrewLabels(camera, destination);
    WorldLabel warehouse_label = {{3.22f, 2.07f, 0.73f}, "Warehouse", WORLD_GOLD};
    DrawLabels(&warehouse_label, 1, camera, destination);
    WorldLabel labels[3];
    int32_t label_count = 0;
    if (AgentNearLabel(agent, MARKET_PEOPLE[0].x,
                       MARKET_PEOPLE[0].y, 5.0f)) {
        labels[label_count++] = (WorldLabel){
            {6.55f, 2.05f, 1.60f}, profile->keeper_name, identity};
        labels[label_count++] = (WorldLabel){
            {6.55f, 2.36f, 0.62f}, profile->interior_service, WORLD_MUTED};
    }
    if (AgentNearLabel(agent, 1.55f, 6.54f, 3.5f)) {
        labels[label_count++] = (WorldLabel){
            {1.55f, 2.25f, 6.54f}, "Exit  /  approach, then press F",
            WORLD_MUTED};
    }
    if (!interaction_ui_enabled) DrawLabels(labels, label_count, camera, destination);
    if (draw_hero_rig_debug &&
        agent->morphology == CC_MORPHOLOGY_BIPED) {
        DrawViewportText(
            TextFormat("BIO %s / %s / %s + %s / %.0f%% MUSCLE",
                       CcHumanoidPoseOwnerName(agent->humanoid.pose_owner),
                       CcMotionClipName(
                           agent->humanoid.motion.clip != NULL ?
                           agent->humanoid.motion.clip->id :
                           CC_MOTION_CLIP_NONE),
                       CcHumanoidContactName(agent->humanoid.feet[0].contact),
                       CcHumanoidContactName(agent->humanoid.feet[1].contact),
                       CcBiomechRigMeanActivation(&agent->humanoid.body) * 100.0f),
            destination, 18, 18, 10, WORLD_TEAL);
    } else if (draw_hero_rig_debug) {
        DrawViewportText(
            TextFormat("ROBOTIC %s / %d LEGS / %s / %s / %.0f%% CONTROL",
                       CcLocalAgentMorphologyName(agent),
                       agent->limb_rig.morphology.limb_count,
                       CcLimbPaceName(agent->limb_rig.pace),
                       CcLimbSupportStateName(
                           agent->limb_rig.support_state),
                       agent->limb_rig.control_authority * 100.0f),
            destination, 18, 18, 10, WORLD_TEAL);
    }
}

void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent,
                         float clock, RenderTexture2D target,
                         Rectangle destination)
{
    CcLocalDrawInterior3D(sim, agent, clock, target, destination);
}

static bool InsideExpanded(Vector2 point, Rectangle footprint, float radius)
{
    return point.x > footprint.x - radius &&
           point.x < footprint.x + footprint.width + radius &&
           point.y > footprint.y - radius &&
           point.y < footprint.y + footprint.height + radius;
}

static bool StreetCanOccupy(Vector2 point)
{
    const float radius = PLAYER_COLLISION_RADIUS;
    if (point.x < 0.28f + radius ||
        point.x > CC_LOCAL_WORLD_WIDTH - 0.28f - radius ||
        point.y < 0.28f + radius ||
        point.y > CC_LOCAL_WORLD_DEPTH - 0.28f - radius) return false;
    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        WorldBuilding building = ActiveWorldBuildingAt(i);
        if (InsideExpanded(point, building.footprint, radius)) {
            return false;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        if (InsideExpanded(
                point, PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i)),
                radius)) return false;
    }
    for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
        WorldStructure structure = ActiveCompoundStructureAt(i);
        if (InsideExpanded(point, structure.footprint, radius)) {
            return false;
        }
    }
    if ((active_opening_step == CC_LOCAL_OPENING_COMPLETE &&
         InsideExpanded(point, CARRIAGE_FOOTPRINT, radius)) ||
        InsideExpanded(point, DUNGEON_FOOTPRINT, radius)) return false;
    for (int32_t i = 0; i < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                                      sizeof(ROOM_ART_OBSTACLES[0])); ++i) {
        if (InsideExpanded(point, ROOM_ART_OBSTACLES[i], radius)) return false;
    }
    return true;
}

static bool MarketCanOccupy(Vector2 point)
{
    const float radius = PLAYER_COLLISION_RADIUS;
    if (point.x < 0.52f + radius || point.x > 8.72f - radius ||
        point.y < 0.52f + radius || point.y > 6.72f - radius) return false;
    if (InsideExpanded(point, MARKET_COUNTER_FOOTPRINT, radius)) return false;
    if (InsideExpanded(point, MARKET_SHELF_FOOTPRINT, radius)) return false;
    return true;
}

Vector2 CcLocalMove(Vector2 current, Vector2 delta, bool market_interior)
{
    bool (*can_occupy)(Vector2) = market_interior ? MarketCanOccupy : StreetCanOccupy;
    Vector2 candidate = {current.x + delta.x, current.y};
    if (can_occupy(candidate)) current.x = candidate.x;
    candidate = (Vector2){current.x, current.y + delta.y};
    if (can_occupy(candidate)) current.y = candidate.y;
    return current;
}
