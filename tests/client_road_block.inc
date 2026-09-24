static int RunRoadBlockChoiceRegression(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x5ca17));
    CcRoute *route = &sim.routes[0];
    route->closed = false;
    sim.player.location_id = route->from_id;
    sim.carriage.location_id = route->from_id;
    sim.bandits[0].route_id = route->id;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = route->to_id
    };
    char error[192] = "", message[256] = "", feedback[256] = "";
    if (!CcSimApply(&sim, &travel, error, sizeof(error)))
        return ClientRegressionFailure(error);
    sim.journey.ambush_pending = true;
    sim.journey.ambush_warned = false;
    sim.journey.ambush_resolved = false;
    sim.journey.encounter_triggered = false;
    for (int32_t step = 0; step < 6000 &&
         !sim.journey.ambush_warned; ++step) {
        if (sim.journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
            CcRoadLegPreview previews[3];
            int32_t count = CcRoadNextLegPreviews(&sim, previews, 3);
            bool selected = false;
            for (int32_t i = 0; i < count; ++i) {
                if (previews[i].direction != sim.journey.road_direction ||
                    previews[i].segment_id == CC_PILOT_ROAD_MILL_SEGMENT_ID)
                    continue;
                CcCommand leg = {
                    .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
                    .target_id = previews[i].decision_token
                };
                if (!CcSimApply(&sim, &leg, error, sizeof(error)))
                    return ClientRegressionFailure(error);
                selected = true;
                break;
            }
            if (!selected) return ClientRegressionFailure("A road leg is missing.");
        } else CcSimAdvanceRuntimeTicks(&sim, CC_WORLD_TICKS_PER_SECOND);
    }
    if (!sim.journey.ambush_warned)
        return ClientRegressionFailure("The scout warning is missing.");
    const char *warning = TravelForecastLine(&sim);
    if (strstr(warning, sim.bandits[0].name) == NULL ||
        strstr(warning, "CAREFUL PACE") == NULL)
        return ClientRegressionFailure("The road warning needs its saved claimant and bypass.");
    for (int32_t step = 0; step < 6000 &&
         sim.journey.phase != CC_JOURNEY_PHASE_BLOCKED; ++step) {
        if (sim.journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
            CcRoadLegPreview previews[3];
            int32_t count = CcRoadNextLegPreviews(&sim, previews, 3);
            bool selected = false;
            for (int32_t i = 0; i < count; ++i) {
                if (previews[i].direction != sim.journey.road_direction ||
                    previews[i].segment_id == CC_PILOT_ROAD_MILL_SEGMENT_ID)
                    continue;
                CcCommand leg = {
                    .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
                    .target_id = previews[i].decision_token
                };
                if (!CcSimApply(&sim, &leg, error, sizeof(error)))
                    return ClientRegressionFailure(error);
                selected = true;
                break;
            }
            if (!selected) return ClientRegressionFailure("A road leg is missing.");
        } else if (sim.journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {
                .kind = CcSimJourneyStop(&sim) == CC_JOURNEY_STOP_MIDDAY ?
                    CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
            };
            if (!CcSimApply(&sim, &rest, error, sizeof(error)))
                return ClientRegressionFailure(error);
        } else CcSimAdvanceRuntimeTicks(&sim, CC_WORLD_TICKS_PER_SECOND);
    }
    if (sim.journey.phase != CC_JOURNEY_PHASE_BLOCKED)
        return ClientRegressionFailure("The warned riders never reached the road block.");
    LocalState local = {0};
    ResetLocalState(&local);
    ContextActionSet cards = BuildContextActions(
        &sim, &local, VIEW_ENCOUNTER, 0, 0);
    int32_t withdraw_index = WorldCardIndex(
        &cards, CONTEXT_ACTION_WITHDRAW, 0);
    const CcSettlement *origin = CcSimSettlement(
        &sim, sim.journey.origin_id);
    if (withdraw_index < 0 || origin == NULL ||
        strstr(cards.items[withdraw_index].detail, origin->name) == NULL ||
        strstr(cards.items[withdraw_index].detail, "0 CROWNS") == NULL ||
        strstr(cards.items[withdraw_index].detail, "CURRENT TIME") == NULL)
        return ClientRegressionFailure("The road choice needs exact retreat terms.");
    CcMoney coins = sim.player.coins;
    int32_t cargo = CcPlayerCargoUsed(&sim.player);
    int32_t day = sim.current_day;
    int32_t minute = sim.clock.minute_subticks;
    const char *path = "road-block-choice.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(
        path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
    ClientView view = VIEW_ENCOUNTER, return_view = VIEW_LOCAL;
    int32_t selected = 0, selected_situation = 0;
    float feedback_age = 0;
    queued_key_press[KEY_THREE] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation,
        &view, &return_view, &local, (RenderTexture2D){0},
        (Rectangle){0}, 1.0f / 60.0f, path, "", message,
        sizeof(message), feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    if (view != VIEW_LOCAL || sim.journey.active ||
        sim.carriage.location_id != origin->id ||
        sim.player.coins != coins || CcPlayerCargoUsed(&sim.player) != cargo ||
        sim.current_day != day || sim.clock.minute_subticks != minute ||
        CcSimRecentEvent(&sim, 0)->kind != CC_EVENT_ENCOUNTER_WITHDRAWN) {
        (void)fprintf(stderr, "Road choice: view=%d active=%d carriage=%llu origin=%llu coins=%lld cargo=%d day=%d minute=%d event=%d message=%s\n",
            (int)view, (int)sim.journey.active,
            (unsigned long long)sim.carriage.location_id,
            (unsigned long long)origin->id, (long long)sim.player.coins,
            CcPlayerCargoUsed(&sim.player), sim.current_day,
            sim.clock.minute_subticks, (int)CcSimRecentEvent(&sim, 0)->kind,
            message);
        (void)CcJournalClose(&journal, &sim, error, sizeof(error));
        (void)remove(path);
        return ClientRegressionFailure("The third encounter choice must save the quoted withdrawal.");
    }
    if (!CcJournalClose(&journal, &sim, error, sizeof(error)))
        return ClientRegressionFailure(error);
    (void)remove(path);
    (void)puts("Road block choice: named warning, exact retreat card and native input passed");
    return 0;
}
