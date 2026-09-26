/* Uses the production world placement, cache and draw code in this TU. */
static CcLocalWorldCarriageState CarriageWorldFixture(CcSim *sim, CcWorldStream *stream)
{
    CcSimInit(sim, UINT32_C(0xc0a71a9e));
    RequireRenderer(CcWorldStreamInit(stream, sim), "world fixture initialization");
    CcLocalBindOpenWorld(stream);
    const CcWorldRoutePlacement *route = &stream->manifest.routes[0];
    CcWorldPoint point;
    float yaw;
    RequireRenderer(CcWorldRoutePose(route, route->from_id, 0.5f, &point, &yaw), "world route pose");
    CcLocalWorldCarriageState c = {.storybook_travel = true, .visible = true,
        .hero_embarked = true, .route_id = route->route_id, .camera_heading_yaw = yaw};
    CcLocalCarriagePublishPose(&c, (Vector3){point.x,
        CcWorldTerrainHeight(&stream->manifest, point.x, point.z), point.z}, yaw,
        CcWorldRouteLength(route) * 0.5f, false, 1);
    return c;
}

static void TestCarriageWorldTargets(void)
{
    static CcSim sim;
    static CcWorldStream stream;
    memset(creature_gaits, 0, sizeof(creature_gaits));
    CcLocalWorldCarriageState c = CarriageWorldFixture(&sim, &stream);
    CcLocalOpenWorldCarriageTargetsInternal(&sim, &c, 100, 1.0f/60);
    RoadHorsePlacement places[2];
    int count = RoadHorsePlacements(&sim, OpenWorldCarriageBase(&c, 100, false),
        c.heading_yaw, false, CC_LOCAL_SCENE_ROAD, 1, places);
    RequireRenderer(count > 0, "fixture must have a team");
    for (int i = 0; i < count; ++i) {
        CreatureGaitCache *cache = &creature_gaits[places[i].slot];
        RequireRenderer(Vector3Distance(places[i].base,
            FromLimbVector(cache->command.ground_position)) < 0.0001f,
            "world publisher substituted local-road coordinates");
        cache->controller.skeleton.gait_phase = 0.37f;
    }
    CcLocalOpenWorldCarriageTargetsInternal(&sim, &c, 3000, 1.0f/60);
    for (int i = 0; i < count; ++i) {
        RequireRenderer(creature_gaits[places[i].slot].controller.skeleton.gait_phase == 0.37f,
            "campaign fast-forward reset a world pony controller");
    }
    CcLocalCreatureGaitsAdvanceInternal(1);
    CcLocalCarriageGaitInterpolateInternal(0.5f);
    for (int i = 0; i < count; ++i) {
        CreatureGaitCache before = creature_gaits[places[i].slot];
        CcCreatureRigPose first, second;
        RequireRenderer(CcLocalCreatureGaitPoseInternal(places[i].slot,
            places[i].base, c.heading_yaw, &first), "world pony pose");
        RequireRenderer(CcLocalCreatureGaitPoseInternal(places[i].slot,
            places[i].base, c.heading_yaw, &second), "repeated world pony pose");
        RequireRenderer(memcmp(&first, &second, sizeof(first)) == 0 &&
            memcmp(&before, &creature_gaits[places[i].slot], sizeof(before)) == 0,
            "reading a pony pose mutated its controller or commands");
    }
    CcLocalBindOpenWorld(NULL);
    memset(creature_gaits, 0, sizeof(creature_gaits));
    puts("PASS world pony targets and read-only pose retrieval");
}

static void TestCarriagePlantedInterpolation(void)
{
    CreatureGaitCache *c = &creature_gaits[CREATURE_GAIT_ROAD_HORSE_LEFT];
    *c = (CreatureGaitCache){0};
    c->ready = c->previous_valid = true;
    c->scale = 1;
    c->render_alpha = 0.5f;
    c->previous_ground = (Vector3){100, 0, 200};
    c->controller.ground_position = (CcLimbVec3){101, 0, 200};
    c->pose.valid = c->previous_pose.valid = true;
    c->pose.limb_count = c->previous_pose.limb_count = 1;
    c->pose.limbs[0].segment_count = c->previous_pose.limbs[0].segment_count = 2;
    c->pose.limbs[0].state = c->previous_pose.limbs[0].state = CC_LIMB_STANCE;
    c->previous_pose.limbs[0].joints[2] = (CcLimbVec3){0, 0, 0};
    c->pose.limbs[0].joints[2] = (CcLimbVec3){-1, 0, 0};
    CcCreatureRigPose pose;
    Vector3 rendered = {100.5f, 0, 200};
    RequireRenderer(CcLocalCreatureGaitPoseInternal(0, rendered, 0, &pose), "interpolated fixture pose");
    RequireRenderer(fabsf(rendered.x + pose.limbs[0].joints[2].x - 100) < 0.0001f,
                    "moving render origin dragged a planted world hoof");
    memset(creature_gaits, 0, sizeof(creature_gaits));
}

static void TestCarriageDrawReadOnly(void)
{
    static CcSim sim;
    static CcWorldStream stream;
    CcLocalWorldCarriageState c = CarriageWorldFixture(&sim, &stream);
    CcLocalOpenWorldCarriageTargetsInternal(&sim, &c, 1, 1.0f/60);
    CcLocalCreatureGaitsAdvanceInternal(1);
    CreatureGaitCache before[CREATURE_GAIT_SLOT_COUNT];
    memcpy(before, creature_gaits, sizeof(before));
    CcLocalAgent agent = {.position = c.position};
    RenderTexture2D target = LoadRenderTexture(320, 180);
    for (int draw = 0; draw < 2; ++draw) {
        BeginDrawing();
        CcLocalDrawOpenWorld3D(&sim, &stream, &agent, NULL, &c, 1,
                              target, (Rectangle){0, 0, 320, 180});
        EndDrawing();
    }
    const int slots[] = {CREATURE_GAIT_ROAD_HORSE_LEFT,
        CREATURE_GAIT_ROAD_HORSE_RIGHT, CREATURE_GAIT_ROAD_PONY};
    for (int i = 0; i < 3; ++i) {
        RequireRenderer(memcmp(&before[slots[i]], &creature_gaits[slots[i]],
                               sizeof(CreatureGaitCache)) == 0,
                        "drawing the world carriage changed pony gait state");
    }
    UnloadRenderTexture(target);
    CcLocalBindOpenWorld(NULL);
    memset(creature_gaits, 0, sizeof(creature_gaits));
    puts("PASS repeated world carriage DRAW: no gait/controller mutation");
}

/* GFX-03: the remaining local-only road scenes now publish their team's
   gait targets from the update path (CcLocalRoad*HorseTargetsInternal), so
   drawing each one twice in a row must not touch the gait caches at all. */
static void CheckRoadSceneDrawReadOnly(const CreatureGaitCache *before,
                                       const char *scene)
{
    const int slots[] = {CREATURE_GAIT_ROAD_HORSE_LEFT,
        CREATURE_GAIT_ROAD_HORSE_RIGHT, CREATURE_GAIT_ROAD_PONY};
    for (int i = 0; i < 3; ++i) {
        RequireRenderer(memcmp(&before[slots[i]], &creature_gaits[slots[i]],
                               sizeof(CreatureGaitCache)) == 0, scene);
    }
}

static void TestForkDrawReadOnly(void)
{
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcLocalBindPlace(&sim);
    memset(creature_gaits, 0, sizeof(creature_gaits));
    int32_t selected_route = -1;
    for (int32_t i = 0; i < sim.route_count; ++i) {
        if (ForkRouteLeaves(&sim, &sim.routes[i])) { selected_route = i; break; }
    }
    RequireRenderer(selected_route >= 0, "fork fixture needs an outgoing route");
    CcLocalAgent agent = {.position = {40.0f, 0.0f, 40.0f}};
    float turn_progress = 0.35f;
    CcLocalRoadForkHorseTargetsInternal(&sim, selected_route, turn_progress, 1.0f);
    CcLocalCreatureGaitsAdvanceInternal(1);
    CreatureGaitCache before[CREATURE_GAIT_SLOT_COUNT];
    memcpy(before, creature_gaits, sizeof(before));
    RenderTexture2D target = LoadRenderTexture(320, 180);
    for (int draw = 0; draw < 2; ++draw) {
        BeginDrawing();
        CcLocalDrawFork3D(&sim, &agent, selected_route, turn_progress, 1.0f,
                          target, (Rectangle){0, 0, 320, 180});
        EndDrawing();
    }
    CheckRoadSceneDrawReadOnly(before,
        "drawing the fork carriage changed pony gait state");
    UnloadRenderTexture(target);
    CcLocalBindPlace(NULL);
    memset(creature_gaits, 0, sizeof(creature_gaits));
    puts("PASS repeated fork carriage DRAW: no gait/controller mutation");
}

static void TestEncounterDrawReadOnly(void)
{
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcLocalBindPlace(&sim);
    static CcLocalCourse course;
    CcLocalCourseInit(&course);
    CcLocalAgent agent = {.position = {40.0f, 0.0f, 38.0f}};
    memset(creature_gaits, 0, sizeof(creature_gaits));
    CcLocalRoadEncounterHorseTargetsInternal(&sim, 1.0f);
    CcLocalCreatureGaitsAdvanceInternal(1);
    CreatureGaitCache before[CREATURE_GAIT_SLOT_COUNT];
    memcpy(before, creature_gaits, sizeof(before));
    RenderTexture2D target = LoadRenderTexture(320, 180);
    for (int draw = 0; draw < 2; ++draw) {
        BeginDrawing();
        CcLocalDrawRoad3D(&sim, &agent, &course, false, false, NULL, 1.0f,
                          target, (Rectangle){0, 0, 320, 180});
        EndDrawing();
    }
    CheckRoadSceneDrawReadOnly(before,
        "drawing the encounter/combat/parley carriage changed pony gait state");
    UnloadRenderTexture(target);
    CcLocalBindPlace(NULL);
    memset(creature_gaits, 0, sizeof(creature_gaits));
    puts("PASS repeated encounter carriage DRAW: no gait/controller mutation");
}

static void TestSiteDrawReadOnly(void)
{
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcLocalBindPlace(&sim);
    CcLocalAgent agent = {.position =
        {CC_LOCAL_SITE_ENTRANCE_X - 3.0f, 0.0f, CC_LOCAL_SITE_ENTRANCE_Z}};
    memset(creature_gaits, 0, sizeof(creature_gaits));
    CcLocalSiteKind site = CC_LOCAL_SITE_GOBLIN_CAVE;
    CcLocalRoadSiteHorseTargetsInternal(&sim, site, false, false, 0.0f, 1.0f);
    CcLocalCreatureGaitsAdvanceInternal(1);
    CreatureGaitCache before[CREATURE_GAIT_SLOT_COUNT];
    memcpy(before, creature_gaits, sizeof(before));
    RenderTexture2D target = LoadRenderTexture(320, 180);
    for (int draw = 0; draw < 2; ++draw) {
        BeginDrawing();
        CcLocalDrawSite3D(&sim, &agent, site, false, false, 0.0f, 1.0f,
                         target, (Rectangle){0, 0, 320, 180});
        EndDrawing();
    }
    CheckRoadSceneDrawReadOnly(before,
        "drawing the remote-site carriage changed pony gait state");
    UnloadRenderTexture(target);
    CcLocalBindPlace(NULL);
    memset(creature_gaits, 0, sizeof(creature_gaits));
    puts("PASS repeated remote-site carriage DRAW: no gait/controller mutation");
}

static void TestConvoyDrawReadOnly(void)
{
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcLocalBindPlace(&sim);
    static CcLocalCourse course;
    CcLocalCourseInit(&course);
    CcLocalAgent agent = {.position = {40.0f, 0.0f, 40.0f}};
    CcLocalConvoyState convoy = {0};
    convoy.phase = CC_LOCAL_CONVOY_ARRIVING;
    convoy.pace = 0.4f;
    convoy.town_position = (Vector3){50.0f, 0.0f, 35.0f};
    convoy.town_heading_yaw = 0.2f;
    convoy.travelled = 12.0f;
    memset(creature_gaits, 0, sizeof(creature_gaits));
    CcLocalRoadConvoyHorseTargetsInternal(&sim, &convoy, 1.0f);
    CcLocalCreatureGaitsAdvanceInternal(1);
    CreatureGaitCache before[CREATURE_GAIT_SLOT_COUNT];
    memcpy(before, creature_gaits, sizeof(before));
    /* The Return, milestone 2: an active arrival-shot cue must be pure to
       draw. Stage every cue at once so each of DrawReturnArrivalStaging's
       branches runs, and check that neither the sim nor the cues moved. */
    CcReturnSceneCues cues = {
        .active = true, .fire = true, .new_ruler = true,
        .empty_market = true, .empty_market_goods_mask = 3U,
        .hunger = true, .bandit_camp = true, .service_lost = true,
        .service_lost_kind = CC_SERVICE_SMITHY,
    };
    /* Placement is the update path's job; do it once, then draw. The
       town must be burned for the smoke to have somewhere to rise from. */
    CcSettlement *town = &sim.settlements[0];
    sim.player.location_id = town->id;
    town->fire_damage = 60;
    CcLocalReturnStagingPlace(&sim, &cues);
    if (!cues.placed || cues.crowd_count <= 0 || cues.stall_count <= 0 ||
        cues.smoke_count <= 0)
        RequireRenderer(false, "return-scene staging placed no props");
    CcReturnSceneCues cues_before = cues;
    uint64_t sim_hash_before = CcSimHash(&sim);
    RenderTexture2D target = LoadRenderTexture(320, 180);
    for (int draw = 0; draw < 2; ++draw) {
        BeginDrawing();
        CcLocalDrawStreet3D(&sim, &agent, &course, false, &convoy, 1.0f,
                           target, (Rectangle){0, 0, 320, 180}, &cues);
        EndDrawing();
    }
    CheckRoadSceneDrawReadOnly(before,
        "drawing the town-street convoy changed pony gait state");
    if (CcSimHash(&sim) != sim_hash_before)
        RequireRenderer(false, "drawing return-scene staging changed the sim");
    if (memcmp(&cues, &cues_before, sizeof(cues)) != 0)
        RequireRenderer(false, "drawing return-scene staging changed its own cues");
    UnloadRenderTexture(target);
    CcLocalBindPlace(NULL);
    memset(creature_gaits, 0, sizeof(creature_gaits));
    puts("PASS repeated town-street convoy DRAW: no gait/controller/sim mutation, "
         "return-scene cues staged read-only");
}
