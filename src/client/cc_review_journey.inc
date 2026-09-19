/* Journey preparation shared by render benchmarks and capture fixtures. */
typedef struct CcReviewJourneyOptions {
    bool clear_route_knowledge;
    bool suppress_encounter;
    bool advance_trip;
    bool finish_arrival;
    bool resolve_combat;
    int32_t target_progress;
    int32_t tick_step;
} CcReviewJourneyOptions;

static bool ContinueReviewRoadChoice(CcSim *sim, char *error,
                                     size_t error_capacity)
{
    if (!sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_ROAD_CHOICE) return false;
    CcRoadLegPreview previews[3];
    int32_t preview_count = CcRoadNextLegPreviews(sim, previews, 3);
    for (int32_t i = 0; i < preview_count; ++i) {
        if (previews[i].direction != sim->journey.road_direction ||
            previews[i].segment_id == CC_PILOT_ROAD_MILL_SEGMENT_ID) continue;
        CcCommand choose = {
            .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
            .target_id = previews[i].decision_token
        };
        return CcSimApply(sim, &choose, error, error_capacity);
    }
    return false;
}

static void PrepareReviewJourney(CcSim *sim, CcReviewJourneyOptions options)
{
    int32_t charter_index = FirstActiveSituationIndex(sim);
    if (charter_index >= 0) {
        char setup_error[192];
        CcSituation *charter = &sim->situations[charter_index];
        charter->kind = CC_SITUATION_RELIEF_DELIVERY;
        charter->target_id = sim->settlements[1].id;
        charter->good = CC_GOOD_FOOD;
        charter->quantity = 1;
        charter->progress = 0;
        sim->player.cargo[CC_GOOD_FOOD] = 0;
        sim->routes[0].closed = true;
        if (sim->bandit_count > 0) {
            sim->bandits[0].route_id = sim->routes[0].id;
        }
        CcCommand accept = {
            .kind = CC_COMMAND_ACCEPT_SITUATION,
            .target_id = charter->id
        };
        (void)CcSimApply(sim, &accept, setup_error,
                         sizeof(setup_error));
        if (options.clear_route_knowledge) {
            const CcRoute *route = CcSimRouteBetween(
                sim, sim->player.location_id, sim->settlements[1].id);
            if (route != NULL) {
                for (int32_t map_index = 0;
                     map_index < sim->map_count &&
                     map_index < CC_MAX_MAPS;
                     ++map_index) {
                    CcMap *map = &sim->maps[map_index];
                    if (map->route_id != route->id) continue;
                    if (map->owner_id == sim->player.id) {
                        map->owner_id = sim->player.location_id;
                    }
                    uint32_t bit = UINT32_C(1) << (uint32_t)map_index;
                    sim->player.map_catalogue_mask &= ~bit;
                    sim->player.map_archive_mask &= ~bit;
                }
                CcSimInitializePlayerRouteKnowledge(sim);
            }
        }
        CcCommand travel = {
            .kind = CC_COMMAND_TRAVEL,
            .target_id = sim->settlements[1].id
        };
        (void)CcSimApply(sim, &travel, setup_error,
                         sizeof(setup_error));
        if (options.suppress_encounter &&
            sim->journey.active) {
            sim->journey.situation_id = 0U;
            sim->journey.encounter_subticks = 0;
            sim->journey.ambush_pending = false;
            sim->journey.encounter_triggered = true;
        }
        if (options.advance_trip) {
            int32_t target_progress = options.target_progress;
            int32_t setup_ticks = 0;
            while (sim->journey.active &&
                   sim->carriage.progress_milli < target_progress &&
                   setup_ticks < 10000) {
                if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
                    if (!ContinueReviewRoadChoice(
                            sim, setup_error, sizeof(setup_error))) break;
                    continue;
                }
                if (sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) break;
                CcSimAdvanceRuntimeTicks(
                    sim, options.tick_step);
                setup_ticks += 1;
            }
            if (!options.finish_arrival && sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
                if (!ContinueReviewRoadChoice(
                        sim, setup_error, sizeof(setup_error))) return;
            }
            if (options.finish_arrival) {
                while (sim->journey.active &&
                       setup_ticks < 20000) {
                    if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
                        if (!ContinueReviewRoadChoice(
                                sim, setup_error, sizeof(setup_error))) break;
                        continue;
                    }
                    if (sim->journey.phase !=
                            CC_JOURNEY_PHASE_TRAVELLING) break;
                    CcSimAdvanceRuntimeTicks(
                        sim, CC_WORLD_TICKS_PER_SECOND);
                    setup_ticks += 1;
                }
            }
        } else {
            while (sim->journey.active) {
                if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
                    if (!ContinueReviewRoadChoice(
                            sim, setup_error, sizeof(setup_error))) break;
                    continue;
                }
                if (sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) break;
                CcSimAdvanceRuntimeTicks(
                    sim, CC_WORLD_TICKS_PER_SECOND);
            }
        }
        if (options.resolve_combat && sim->journey.active) {
            CcCommand resolve = {
                .kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT
            };
            (void)CcSimApply(sim, &resolve, setup_error,
                             sizeof(setup_error));
            while (sim->journey.active) {
                if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
                    if (!ContinueReviewRoadChoice(
                            sim, setup_error, sizeof(setup_error))) break;
                    continue;
                }
                CcSimAdvanceRuntimeTicks(
                    sim, CC_WORLD_TICKS_PER_SECOND);
            }
        }
    }
}
