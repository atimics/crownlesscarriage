static void TestAbandonedTownGraphics(const char *prefix)
{
    static CcSim sim;
    static CcLocalCourse course;
    CcLocalAgent hero;
    CcSimInit(&sim, 42U);
    CcSettlement *town = &sim.settlements[5];
    sim.player.location_id = town->id;
    sim.carriage.location_id = town->id;
    town->population = 0;
    town->prosperity = 0;
    town->service_mask = 0;
    town->hunger = 100;
    sim.character_count = 0;
    sim.bandit_count = 1;
    sim.bandits[0].camp_settlement_id = town->id;
    sim.bandits[0].members = 4;
    (void)snprintf(sim.bandits[0].name, sizeof(sim.bandits[0].name), "The Tallow Knives");
    CcLocalCourseInit(&course);
    CcLocalAgentInit(&hero, (Vector2){42.0f, 50.0f}, false);
    CcLocalRendererSetOpeningStep(CC_LOCAL_OPENING_COMPLETE);
    RenderTexture2D target = LoadRenderTexture(960, 540);
    const char *names[] = {"occupied", "abandoned", "resettled"};
    for (int32_t state = 0; state < 3; ++state) {
        if (state == 1) sim.bandits[0].camp_settlement_id = 0;
        if (state == 2) { town->population = 180; town->hunger = 25; }
        uint64_t hash = CcSimHash(&sim);
        course.travellers[0].active = true;
        course.situation_witness_active = true;
        CcLocalCourseFixedStepInternal(&course, &hero, &sim, 1.0f / 60.0f);
        if (state < 2) {
            RequireRenderer(!course.travellers[0].active && !course.situation_witness_active,
                "Ruin updates retire synthetic travellers and witnesses.");
        }
        BeginDrawing();
        ClearBackground(BLACK);
        CcLocalDrawStreet3D(&sim, &hero, &course, false, NULL, 2.0f,
                           target, (Rectangle){0, 0, 1280, 720});
        char status[128];
        CcLocalTownStatus(&sim, town->id, status, sizeof(status));
        DrawRectangle(0, 0, 1280, 60, (Color){20, 18, 25, 255});
        DrawText(town->name, 20, 8, 20, WHITE);
        DrawText(status, 20, 34, 18, LIGHTGRAY);
        CcLocalPresentedPerson people[64];
        int32_t count = CcLocalPresentedPeople(people, 64);
        if (state == 0) {
            RequireRenderer(count == 4, "Four occupying bandits appear in the ruin.");
            for (int32_t i = 0; i < count; ++i)
                RequireRenderer(people[i].role == CC_NPC_ROLE_RAIDER,
                    "The occupied ruin renders its band rather than civilians.");
        } else if (state == 1) RequireRenderer(count == 0, "The empty ruin has an empty civilian scene.");
        else RequireRenderer(count > 0, "Residents return after resettlement.");
        RequireRenderer(CcSimHash(&sim) == hash, "Rendering preserves simulation state.");
        rlDrawRenderBatchActive();
        Image shot = LoadImageFromScreen();
        char path[768];
        (void)snprintf(path, sizeof(path), "%s-%s.png", prefix, names[state]);
        RequireRenderer(ExportImage(shot, path), "Write town state capture.");
        UnloadImage(shot);
        EndDrawing();
    }
    UnloadRenderTexture(target);
}
