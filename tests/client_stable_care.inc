static int RunStableCareCardRegression(void)
{
    static CcSim sim;
    static CcSim restored;
    static LocalState local;
    const char *path = "stable-care-card.ccsave";
    char error[192] = "", message[256] = "", feedback[256] = "";
    float feedback_age = 0.0f;
    CcJournal *journal = NULL;
    ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
    int32_t selected = 0, selected_situation = 0;
    CcClientPreferences preferences;
    CcClientPreferencesDefault(&preferences);
    preferences.key_interact = KEY_J;
    adventure_preferences = &preferences;
#define CARE_CHECK(test, reason) do { if (!(test)) { \
    client_test_pointer_active = false; ClientInputClearPressed(); \
    if (journal != NULL) CcJournalAbandon(&journal); \
    adventure_preferences = NULL; (void)remove(path); \
    return ClientRegressionFailure(reason); } } while (0)
    (void)remove(path);
    CcSimInit(&sim, UINT32_C(0x57ab1e));
    CcSettlement *town = CcSimSettlementMutable(&sim, sim.player.location_id);
    CARE_CHECK(town != NULL, "Stable-care town is present.");
    sim.horse_team[0].health = 94;
    sim.horse_team[0].hunger = 1;
    sim.horse_team[0].fatigue = 1;
    sim.player.feed_tray_wheat = 0;
    sim.player.cargo[CC_GOOD_WHEAT] = 0;
    town->stock[CC_GOOD_WHEAT] = 100;
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    RepositionHero(&local, LOCAL_CARRIAGE_BAY, false);
    BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});

    ContextActionSet actions = BuildContextActions(&sim, &local, view, selected,
                                                    selected_situation);
    int32_t care_index = WorldCardIndex(&actions, CONTEXT_ACTION_CARE_HORSES, 0);
    CARE_CHECK(care_index >= 0 && actions.items[care_index].enabled,
               "Care card is visible at the parked stable.");
    CcHorseCarePreview offer;
    CARE_CHECK(CcSimHorseCarePreview(&sim, &offer) && offer.available,
               "The saved stable offers care.");
    CARE_CHECK(strstr(actions.items[care_index].label, "crowns") != NULL &&
               strstr(actions.items[care_index].detail, "1 Wheat from market") != NULL &&
               strstr(actions.items[care_index].detail, "1 day") != NULL &&
               strcmp(actions.items[care_index].key_hint, "Shift+J") == 0,
               "Care card shows the bound key, source, cost, and day.");
    char touch_label[128];
    (void)ContextTouchActionLabel(&local, view, care_index,
        &actions.items[care_index], touch_label, sizeof(touch_label));
    CARE_CHECK(strstr(touch_label, "crowns") != NULL &&
               strstr(touch_label, "1 Wheat from market") != NULL &&
               strstr(touch_label, "1 day") != NULL,
               "Touch button carries the full stable-care offer.");

    uint32_t services = town->service_mask;
    town->service_mask = 0U;
    actions = BuildContextActions(&sim, &local, view, selected,
                                  selected_situation);
    care_index = WorldCardIndex(&actions, CONTEXT_ACTION_CARE_HORSES, 0);
    CARE_CHECK(care_index >= 0 && !actions.items[care_index].enabled &&
               strstr(actions.items[care_index].detail, "no staffed stable") != NULL,
               "Closed stable explains why care is unavailable.");
    town->service_mask = services;
    town->stock[CC_GOOD_WHEAT] = 0;
    actions = BuildContextActions(&sim, &local, view, selected,
                                  selected_situation);
    care_index = WorldCardIndex(&actions, CONTEXT_ACTION_CARE_HORSES, 0);
    CARE_CHECK(care_index >= 0 && !actions.items[care_index].enabled &&
               strstr(actions.items[care_index].detail, "1 Wheat") != NULL,
               "Empty stable explains its feed need.");
    town->stock[CC_GOOD_WHEAT] = 100;

    journal = CcJournalStart(path, &sim, error, sizeof(error));
    CARE_CHECK(journal != NULL, error);
    CARE_CHECK(WorldCardPoint(&sim, &local, view, selected,
                             CONTEXT_ACTION_CARE_HORSES, 0, false),
               "Care card can be pressed.");
    CcMoney coins = sim.player.coins;
    int32_t day = sim.current_day;
    HandleInput(&journal, &sim, &selected, &selected_situation,
        &view, &return_view, &local, (RenderTexture2D){0}, (Rectangle){0},
        1.0f / 60.0f, path, "", message, sizeof(message), feedback,
        sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    client_test_pointer_active = false;
    CARE_CHECK(sim.current_day == day + 1 &&
               sim.player.coins == coins - offer.cost &&
               sim.horse_team[0].health == 100 &&
               strstr(message, "1 Wheat from stable market") != NULL,
               "Pressed care card spends the quote and improves the team.");
    actions = BuildContextActions(&sim, &local, view, selected,
                                  selected_situation);
    care_index = WorldCardIndex(&actions, CONTEXT_ACTION_CARE_HORSES, 0);
    CARE_CHECK(care_index >= 0 && !actions.items[care_index].enabled &&
               strstr(actions.items[care_index].detail, "healthy and rested") != NULL,
               "Healthy team shows a disabled care card with its reason.");
    uint64_t hash = CcSimHash(&sim);
    CARE_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)), error);
    CARE_CHECK(CcSaveRead(path, &restored, error, sizeof(error)) &&
               CcSimHash(&restored) == hash,
               "Care card result survives journal restart.");
    ResetLocalState(&local);
    local.adventure_ui = true;
    CARE_CHECK(InitializeOpenWorld(&sim, &local, false),
               "The parked town opens in the world scene.");
    local.open_world = true;
    PositionOpenWorldAtSettlement(&sim, &local);
    CARE_CHECK(local.world_carriage.visible,
               "The parked carriage has a world anchor.");
    RepositionHero(&local, (Vector2){local.world_carriage.position.x,
                                     local.world_carriage.position.z}, false);
    BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
    actions = BuildContextActions(&sim, &local, view, selected,
                                  selected_situation);
    CARE_CHECK(WorldCardIndex(&actions, CONTEXT_ACTION_CARE_HORSES, 0) >= 0,
               "Care is visible beside the parked world carriage.");
    const CcWorldSettlementPlacement *place =
        CcWorldSettlementPlacementForId(&local.world_stream.manifest,
                                        sim.player.location_id);
    CARE_CHECK(place != NULL, "The town has a world placement.");
    Vector2 far = {place->center.x + 8.0f, place->center.z};
    if (hypotf(far.x - local.world_carriage.position.x,
               far.y - local.world_carriage.position.z) < 6.0f)
        far.x = place->center.x - 8.0f;
    RepositionHero(&local, far, false);
    CARE_CHECK(OpenWorldSettlementDistance(&sim, &local) < 18.0f,
               "The far control stays inside the old town-center radius.");
    BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
    actions = BuildContextActions(&sim, &local, view, selected,
                                  selected_situation);
    CARE_CHECK(WorldCardIndex(&actions, CONTEXT_ACTION_CARE_HORSES, 0) < 0,
               "Care is offered beside the carriage, away from the town center.");
    LeaveOpenWorld(&local);
    (void)remove(path);
    adventure_preferences = NULL;
#undef CARE_CHECK
    (void)puts("Stable care card, touch label, and saved action passed");
    return 0;
}
