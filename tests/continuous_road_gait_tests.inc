/* Production road publisher and physical feet, not a phase-only assertion. */
static void TestContinuousRoadGait(void)
{
    static CcSim sim;
    static CcWorldStream stream;
    for (int speed = 1; speed <= 8; speed *= 8) {
        memset(creature_gaits, 0, sizeof(creature_gaits));
        int swing_frames = 0, frozen = 0, max_frozen = 0;
        CcLocalWorldCarriageState carriage = CarriageWorldFixture(&sim, &stream);
        const CcWorldRoutePlacement *route = &stream.manifest.routes[0];
        Vector3 ground = {0};
        int previous_leg = -1;
        for (int f = 0; f < 360; ++f) {
            Vector3 previous = ground;
            /* Repeatedly traverse actual generated road geometry. Reset at
               explicit reversals; do not run off the road to invent terrain
               cliffs. High compression must exceed the replant threshold. */
            float distance = (float)(f * speed) * .0012f;
            int leg = (int)floorf(distance / .90f);
            float along = fmodf(distance, .90f);
            float amount = (leg % 2 == 0) ? .05f + along : .95f - along;
            bool rebase = leg != previous_leg;
            if (rebase) CcLocalCarriageResetGaitsInternal();
            previous_leg = leg;
            CcWorldPoint point; float yaw;
            RequireRenderer(CcWorldRoutePose(route, route->from_id, amount,
                &point, &yaw), "generated road sample");
            carriage.position = (Vector3){point.x,
                CcWorldTerrainHeight(&stream.manifest,point.x,point.z),point.z};
            carriage.heading_yaw = yaw + (leg % 2 != 0 ? PI : 0.0f);
            carriage.pace = .72f;
            CcLocalOpenWorldCarriageTargetsInternal(&sim, &carriage, (float)f/60, 1.0f/60);
            ground = FromLimbVector(creature_gaits[CREATURE_GAIT_ROAD_HORSE_RIGHT].command.ground_position);
            CcLocalCreatureGaitsAdvanceInternal(1);
            CreatureGaitCache *cache = &creature_gaits[CREATURE_GAIT_ROAD_HORSE_RIGHT];
            if (f > 0 && !rebase && cache->previous_valid) RequireRenderer(fabsf(cache->previous_ground.x - previous.x) < .01f,
                "compressed translation discarded the preceding visual sample");
            RequireRenderer(Vector3Distance(FromLimbVector(cache->controller.ground_position), ground) < .1f,
                "pony controller detached from carriage");
            if (f >= 60) {
                if (cache->pose.swinging_count > 0) { swing_frames++; frozen = 0; }
                else if (++frozen > max_frozen) max_frozen = frozen;
            }
        }
        printf("road gait %dx: %d/300 swinging frames; longest planted-only run %d frames\n",
               speed, swing_frames, max_frozen);
        RequireRenderer(swing_frames > 180 && max_frozen < 30,
            "accelerated road replants hooves instead of completing strides");
        carriage.pace = 0;
        for (int f = 0; f < 180; ++f) {
            CcLocalOpenWorldCarriageTargetsInternal(&sim, &carriage, 6, 1.0f/60);
            CcLocalCreatureGaitsAdvanceInternal(1);
        }
        const CreatureGaitCache *stopped = &creature_gaits[CREATURE_GAIT_ROAD_HORSE_RIGHT];
        float phase = stopped->pose.phase;
        RequireRenderer(stopped->pose.swinging_count == 0, "stopped pony keeps walking");
        for (int f = 0; f < 60; ++f) {
            CcLocalOpenWorldCarriageTargetsInternal(&sim, &carriage, 6, 1.0f/60);
            CcLocalCreatureGaitsAdvanceInternal(1);
        }
        RequireRenderer(fabsf(stopped->pose.phase-phase)<.0001f, "stopped stride phase advanced");
        /* A new route is an explicit discontinuity, not swept animation. */
        ground.x += 200;
        (void)CreatureTravelTarget(CREATURE_GAIT_ROAD_HORSE_RIGHT,
            CC_CREATURE_RIG_GAIT_WALK, 0, ground, 0, .96f, 88, 0, 1.0f/60);
        RequireRenderer(!stopped->previous_valid, "route change retained stale pose interval");
    }
    memset(creature_gaits, 0, sizeof(creature_gaits));
    CcLocalBindOpenWorld(NULL);
}
