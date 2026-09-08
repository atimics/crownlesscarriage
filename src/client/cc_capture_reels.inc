static int32_t FirstDeliverySituationIndex(const CcSim *sim)
{
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status == CC_SITUATION_ACTIVE &&
            (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
             situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY)) {
            return i;
        }
    }
    return FirstActiveSituationIndex(sim);
}

/* Reel simulation and overlays used by the capture frame hooks. */

static void ActionReelSetStage(ActionReelState *reel, int32_t stage)
{
    reel->stage = stage;
    reel->stage_frame = 0;
}

static void RecordActionReelImpact(CcLocalCourse *course,
                                   CcLocalAgent *attacker,
                                   CcLocalAgent *defender)
{
    if (!CcHumanoidGaitConsumeStrikeImpact(&attacker->humanoid)) return;
    float previous_health = defender != NULL ? defender->combat.health : 0.0f;
    float previous_posture = defender != NULL ?
        defender->combat.posture : 0.0f;
    course->last_outcome = CcLocalCombatResolveStrike(attacker, defender);
    course->last_attacker_team = attacker->combat.team;
    course->last_defender_team = defender != NULL ?
        defender->combat.team : CC_COMBAT_NEUTRAL;
    course->last_health_damage = defender != NULL ?
        fmaxf(0.0f, previous_health - defender->combat.health) : 0.0f;
    course->last_posture_damage = defender != NULL ?
        fmaxf(0.0f, previous_posture - defender->combat.posture) : 0.0f;
    course->combat_event_seconds = 0.72f;
}

static void UpdateActionReel(LocalState *local, ActionReelState *reel,
                             char *message, size_t message_capacity)
{
    const float delta_time = 1.0f / 60.0f;
    CcLocalAgent *hero = &local->agent;
    CcLocalAgent *opponent = &local->course.raiders[0];
    reel->stage_frame += 1;
    if (reel->stage < 4) CcLocalAgentUpdate(opponent, delta_time, false);

    switch (reel->stage) {
        case 0:
            (void)snprintf(message, message_capacity,
                           "Climb.");
            CcLocalAgentUpdate(hero, delta_time, false);
            if ((!hero->climbing && !hero->exact_target_valid &&
                 hero->position.y > 1.50f) || reel->stage_frame > 300) {
                (void)CcLocalAgentSetExactTarget(
                    hero, (Vector3){1.82f, 0.0f, 7.50f}, false);
                ActionReelSetStage(reel, 1);
            }
            break;
        case 1:
            (void)snprintf(message, message_capacity,
                           "Climb down.");
            CcLocalAgentUpdate(hero, delta_time, false);
            if ((!hero->climbing && !hero->exact_target_valid &&
                 hero->position.y < 0.08f) || reel->stage_frame > 330) {
                (void)CcLocalAgentSetExactTarget(
                    hero, (Vector3){9.40f, 0.0f, 9.65f}, false);
                ActionReelSetStage(reel, 2);
            }
            break;
        case 2:
            (void)snprintf(message, message_capacity,
                           "Jump.");
            if (!reel->jump_started && reel->stage_frame >= 62) {
                reel->jump_started = CcLocalAgentJump(hero);
            }
            CcLocalAgentUpdate(hero, delta_time, false);
            if ((!hero->exact_target_valid && hero->grounded &&
                 reel->stage_frame > 90) || reel->stage_frame > 330) {
                (void)CcLocalAgentSetExactTarget(
                    hero, (Vector3){14.45f, 0.0f, 9.65f}, false);
                ActionReelSetStage(reel, 3);
            }
            break;
        case 3:
            (void)snprintf(message, message_capacity,
                           "Swim.");
            CcLocalAgentUpdate(hero, delta_time, false);
            if ((!hero->exact_target_valid && !hero->swimming &&
                 hero->grounded && reel->stage_frame > 120) ||
                reel->stage_frame > 420) {
                hero->exact_target_valid = false;
                CcLocalCombatSetFocus(hero, opponent);
                CcLocalCombatSetFocus(opponent, hero);
                CcLocalCombatSetGuarded(hero, opponent, true);
                CcLocalCombatSetGuarded(opponent, hero, true);
                ActionReelSetStage(reel, 4);
            }
            break;
        case 4:
            (void)snprintf(message, message_capacity,
                           "Fight.");
            CcLocalCombatSetFocus(hero, opponent);
            CcLocalCombatSetFocus(opponent, hero);
            if (reel->stage_frame == 34) {
                CcLocalCombatSetGuarded(hero, opponent, false);
                (void)CcLocalCombatBeginStrike(hero, opponent);
            }
            if (reel->stage_frame == 112) {
                CcLocalCombatSetGuarded(opponent, hero, false);
                (void)CcLocalCombatBeginStrike(hero, opponent);
            }
            if (reel->stage_frame == 190) {
                CcLocalCombatSetGuarded(hero, opponent, true);
                (void)CcLocalCombatBeginStrike(opponent, hero);
            }
            if (reel->stage_frame == 274) {
                CcLocalCombatSetGuarded(hero, opponent, false);
                CcLocalCombatSetGuarded(opponent, hero, false);
                opponent->combat.health = fminf(opponent->combat.health, 24.0f);
                (void)CcLocalCombatBeginStrike(hero, opponent);
            }
            CcLocalAgentUpdate(hero, delta_time, false);
            CcLocalAgentUpdate(opponent, delta_time, false);
            RecordActionReelImpact(&local->course, hero, opponent);
            RecordActionReelImpact(&local->course, opponent, hero);

            hero->combat.target_index =
                opponent->combat.life_state == CC_LIFE_ALIVE ? 0 : -1;
            if (reel->stage_frame > 390) reel->complete = true;
            break;
        default:
            reel->complete = true;
            break;
    }
}

static void GameplayReelSetStage(GameplayReelState *reel,
                                 GameplayReelStage stage)
{
    reel->stage = stage;
    reel->stage_frame = 0;
    reel->stage_started = false;
}

static CcId GameplayReelDestination(const CcSim *sim, int32_t *selected)
{
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    CcId preferred = SituationSettlementId(sim, accepted);
    CcId next_hop = 0U;
    if (preferred != 0U && preferred != sim->player.location_id) {
        int32_t start = -1;
        int32_t goal = -1;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            if (sim->settlements[i].id == sim->player.location_id) start = i;
            if (sim->settlements[i].id == preferred) goal = i;
        }
        if (start >= 0 && goal >= 0) {
            int32_t queue[CC_MAX_SETTLEMENTS] = {0};
            int32_t parent[CC_MAX_SETTLEMENTS];
            for (int32_t i = 0; i < CC_MAX_SETTLEMENTS; ++i) parent[i] = -1;
            int32_t head = 0;
            int32_t tail = 0;
            queue[tail++] = start;
            parent[start] = start;
            while (head < tail && parent[goal] < 0) {
                int32_t current = queue[head++];
                CcId current_id = sim->settlements[current].id;
                for (int32_t route_index = 0;
                     route_index < sim->route_count; ++route_index) {
                    if (sim->routes[route_index].smuggler_route &&
                        (accepted == NULL || accepted->kind !=
                            CC_SITUATION_BLACK_MARKET_DELIVERY)) {
                        continue;
                    }
                    CcId neighbor_id = RouteOtherEnd(
                        &sim->routes[route_index], current_id);
                    if (neighbor_id == 0U) continue;
                    int32_t neighbor = -1;
                    for (int32_t i = 0; i < sim->settlement_count; ++i) {
                        if (sim->settlements[i].id == neighbor_id) {
                            neighbor = i;
                            break;
                        }
                    }
                    if (neighbor < 0 || parent[neighbor] >= 0) continue;
                    parent[neighbor] = current;
                    queue[tail++] = neighbor;
                }
            }
            if (parent[goal] >= 0) {
                int32_t step = goal;
                while (parent[step] != start && parent[step] != step) {
                    step = parent[step];
                }
                if (step != start) next_hop = sim->settlements[step].id;
            }
        }
    }
    if (next_hop != 0U) {
        for (int32_t i = 0; i < sim->route_count; ++i) {
            if (RouteOtherEnd(&sim->routes[i], sim->player.location_id) ==
                next_hop) {
                *selected = i;
                break;
            }
        }
        return next_hop;
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcId destination = RouteOtherEnd(&sim->routes[i],
                                         sim->player.location_id);
        if (destination != 0U) {
            *selected = i;
            return destination;
        }
    }
    return 0U;
}

static void PrepareTownRaidReel(CcSim *sim, LocalState *local)
{
    if (!local->course.alarm_active) {
        CcLocalCourseBindRaiderCompany(&local->course, sim);
        CcLocalCourseRaiseAlarmNear(&local->course, &local->agent);
    }
    (void)CcLocalCourseSelectPlayerTarget(
        &local->course, &local->agent, 0);

    for (int32_t frame = 0; frame < 6000; ++frame) {
        (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                 1.0f / 60.0f, false, true);
        float x = local->course.raiders[0].position.x -
                  local->agent.position.x;
        float z = local->course.raiders[0].position.z -
                  local->agent.position.z;
        if (local->agent.combat.target_index == 0 &&
            x * x + z * z <= 7.5f * 7.5f) break;
        if (local->agent.combat.target_index < 0) {
            (void)CcLocalCourseSelectPlayerTarget(
                &local->course, &local->agent, 0);
        }
    }
}

static void GameplayReelEnterMarket(LocalState *local)
{
    local->market_interior = true;
    RepositionHero(local, (Vector2){2.05f, 5.35f}, true);
    (void)CcLocalAgentSetExactTarget(
        &local->agent, (Vector3){5.70f, 0.0f, 3.15f}, true);
}

static void GameplayReelLeaveMarket(LocalState *local, Vector3 next_target)
{
    local->market_interior = false;
    RepositionHero(local,
                   (Vector2){CC_LOCAL_MARKET_X,
                             CC_LOCAL_MARKET_Z + 1.10f}, false);
    (void)CcLocalAgentSetExactTarget(&local->agent, next_target, false);
}

static void GameplayReelStartTownArrival(LocalState *local,
                                         int32_t journey_leg,
                                         bool final_town)
{
    Vector2 entry = final_town ? (Vector2){78.50f, 32.80f} :
                    journey_leg == 1 ? (Vector2){15.20f, 29.00f} :
                                       (Vector2){22.00f, 55.40f};
    Vector3 destination = final_town ?
        (Vector3){49.20f, 0.0f, 27.15f} :
        (Vector3){LOCAL_CARRIAGE.x, 0.0f, LOCAL_CARRIAGE.y};
    RepositionHero(local, entry, false);
    if (!CcLocalAgentSetStreetTarget(&local->agent, destination)) {
        (void)CcLocalAgentSetExactTarget(&local->agent, destination, false);
    }
}

static void GameplayReelTrade(CcSim *sim, bool delivery,
                              char *message, size_t message_capacity)
{
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    CcGood good = CC_GOOD_FOOD;
    int32_t amount = 1;
    if (accepted != NULL &&
        (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
         accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY)) {
        good = accepted->good;
        amount = accepted->quantity - accepted->progress;
        if (amount < 1) amount = 1;
    }
    if (delivery) {
        if (accepted == NULL || accepted->target_id != sim->player.location_id) {
            return;
        }
        amount = -amount;
    }
    CcCommand trade = {
        .kind = CC_COMMAND_TRADE,
        .good = good,
        .amount = amount
    };
    (void)ApplyCommand(NULL, sim, trade, message, message_capacity);
}

static void GameplayReelResolveJourneyStop(
    CcSim *sim, char *message, size_t message_capacity)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_RESTING) return;
    CcCommand rest = {
        .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
            CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
    };
    (void)ApplyCommand(NULL, sim, rest, message, message_capacity);
}

static void UpdateGameplayReel(CcSim *sim, LocalState *local,
                               GameplayReelState *reel,
                               int32_t *selected,
                               int32_t *selected_situation,
                               ClientView *view, ClientView *return_view,
                               char *message, size_t message_capacity)
{
    const float delta_time = 1.0f / 60.0f;
    reel->stage_frame += 1;

    switch (reel->stage) {
        case GAMEPLAY_REEL_WALK_TO_NOTICE:
            (void)snprintf(message, message_capacity,
                           "Walk to the promise board.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if ((reel->stage_frame >= 90 &&
                 GridDistance(LocalPosition(local), LOCAL_NOTICE) < 1.15f) ||
                reel->stage_frame >= 360) {
                *selected_situation = FirstDeliverySituationIndex(sim);
                *return_view = VIEW_LOCAL;
                *view = VIEW_SITUATIONS;
                GameplayReelSetStage(reel, GAMEPLAY_REEL_CHOOSE_PROMISE);
            }
            break;
        case GAMEPLAY_REEL_CHOOSE_PROMISE:
            (void)snprintf(message, message_capacity,
                           "Choose one promise.");
            if (reel->stage_frame == 88 && *selected_situation >= 0) {
                CcCommand accept = {
                    .kind = CC_COMMAND_ACCEPT_SITUATION,
                    .target_id = sim->situations[*selected_situation].id
                };
                (void)ApplyCommand(NULL, sim, accept, message,
                                   message_capacity);
            }
            if (reel->stage_frame >= 180) {
                *view = VIEW_LOCAL;
                (void)CcLocalAgentSetExactTarget(
                    &local->agent,
                    (Vector3){LOCAL_CARRIAGE.x, 0.0f, LOCAL_CARRIAGE.y},
                    false);
                GameplayReelSetStage(reel,
                                     GAMEPLAY_REEL_WALK_TO_CARRIAGE);
            }
            break;
        case GAMEPLAY_REEL_WALK_TO_MARKET:
            (void)snprintf(message, message_capacity,
                           "Walk to Mara's market.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if ((reel->stage_frame >= 60 &&
                 GridDistance(LocalPosition(local), LOCAL_MARKET) < 1.30f) ||
                reel->stage_frame >= 600) {
                GameplayReelEnterMarket(local);
                GameplayReelSetStage(reel, GAMEPLAY_REEL_BUY_CARGO);
            }
            break;
        case GAMEPLAY_REEL_BUY_CARGO:
            (void)snprintf(message, message_capacity,
                           reel->stage_started ?
                               "Cargo loaded. Leave through the door." :
                               "Walk to the counter and load the cargo.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, true, false);
            if (!reel->stage_started && reel->stage_frame >= 60 &&
                (GridDistance(LocalPosition(local), INTERIOR_COUNTER) < 2.25f ||
                 reel->stage_frame >= 150)) {
                GameplayReelTrade(sim, false, message, message_capacity);
                reel->stage_started = true;
            }
            if (reel->stage_started && reel->stage_frame >= 210) {
                (void)CcLocalAgentSetExactTarget(
                    &local->agent,
                    (Vector3){INTERIOR_EXIT.x, 0.0f, INTERIOR_EXIT.y}, true);
                GameplayReelSetStage(reel, GAMEPLAY_REEL_LEAVE_MARKET);
            }
            break;
        case GAMEPLAY_REEL_LEAVE_MARKET:
            (void)snprintf(message, message_capacity,
                           "Return to the street.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, true, false);
            if ((reel->stage_frame >= 45 &&
                 GridDistance(LocalPosition(local), INTERIOR_EXIT) < 1.25f) ||
                reel->stage_frame >= 360) {
                GameplayReelLeaveMarket(
                    local, (Vector3){LOCAL_CARRIAGE.x, 0.0f,
                                     LOCAL_CARRIAGE.y});
                GameplayReelSetStage(reel,
                                     GAMEPLAY_REEL_WALK_TO_CARRIAGE);
            }
            break;
        case GAMEPLAY_REEL_WALK_TO_CARRIAGE:
            (void)snprintf(message, message_capacity,
                           "The food boxes are aboard. Go to the carriage.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if ((reel->stage_frame >= 60 &&
                 GridDistance(LocalPosition(local), LOCAL_CARRIAGE_BAY) <
                     1.85f) ||
                reel->stage_frame >= 1200) {
                *selected = FirstOutgoingRouteIndex(sim);
                reel->destination_id = GameplayReelDestination(sim, selected);
                *return_view = VIEW_LOCAL;
                BeginRoadChoiceApproachState(local, true);
                *view = VIEW_LOCAL;
                GameplayReelSetStage(reel, GAMEPLAY_REEL_CHOOSE_ROUTE);
            }
            break;
        case GAMEPLAY_REEL_CHOOSE_ROUTE:
            if (!reel->stage_started) {
                (void)snprintf(message, message_capacity,
                               "The carriage leaves before a road is chosen.");
                if (UpdateRoadChoiceApproach(local, delta_time)) {
                    const CcRoute *route = SelectedOutgoingRoute(
                        sim, *selected);
                    if (route != NULL && EnterRoadBookFromTownGate(
                            sim, local, route->id)) {
                        *view = VIEW_ROADS;
                        reel->stage_started = true;
                        reel->stage_frame = 0;
                        (void)snprintf(message, message_capacity,
                                       "The road book rises above the gate.");
                    }
                }
                break;
            }
            (void)snprintf(message, message_capacity,
                           "Choose the branch when the carriage reaches it.");
            if (reel->stage_frame >= 150 && reel->destination_id != 0U) {
                CcCommand travel = {
                    .kind = CC_COMMAND_TRAVEL,
                    .target_id = reel->destination_id
                };
                if (ApplyCommand(NULL, sim, travel, message,
                                 message_capacity)) {
                    reel->journey_legs += 1;
                    BeginRoadTravelState(sim, local);
                    *view = VIEW_LOCAL;
                    GameplayReelSetStage(reel, GAMEPLAY_REEL_TRAVEL);
                }
            }
            if (reel->stage_frame >= 240) reel->complete = true;
            break;
        case GAMEPLAY_REEL_TRAVEL:
            (void)snprintf(message, message_capacity,
                           "The carriage follows the chosen road.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, false);
            CcSimAdvanceRuntimeTicks(sim, 16);
            GameplayReelResolveJourneyStop(
                sim, message, message_capacity);
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                local->journey_travel_active = false;
                *view = VIEW_ENCOUNTER;
                GameplayReelSetStage(reel, GAMEPLAY_REEL_ROAD_CHOICE);
            } else if (!sim->journey.active) {
                const CcSituation *accepted = CcSimAcceptedSituation(sim);
                CcId target = SituationSettlementId(sim, accepted);
                CcLocalBindPlace(sim);
                ResetLocalStatePreservingAthletics(local);
                local->course.alarm_countdown = 1000.0f;
                if (target != 0U && target != sim->player.location_id &&
                    reel->journey_legs < sim->settlement_count) {
                    GameplayReelStartTownArrival(
                        local, reel->journey_legs, false);
                    GameplayReelSetStage(
                        reel, GAMEPLAY_REEL_WALK_TO_CARRIAGE);
                } else {
                    GameplayReelStartTownArrival(
                        local, reel->journey_legs, true);
                    GameplayReelSetStage(
                        reel, GAMEPLAY_REEL_ARRIVE_AT_MARKET);
                }
            }
            break;
        case GAMEPLAY_REEL_ROAD_CHOICE:
            (void)snprintf(message, message_capacity,
                           "The road is blocked. Choose fight or pay.");
            if (reel->stage_frame >= 180) {
                PrepareRoadCombatReel(sim, local);
                *view = VIEW_LOCAL;
                GameplayReelSetStage(reel, GAMEPLAY_REEL_ROAD_COMBAT);
            }
            break;
        case GAMEPLAY_REEL_ROAD_COMBAT:
            (void)snprintf(message, message_capacity,
                           "Break the cordon beside the carriage.");
            if (reel->stage_frame == 12) {
                (void)CcLocalCourseSelectPlayerTarget(
                    &local->course, &local->agent, 0);
                (void)CcLocalCourseUsePlayerSkill(
                    &local->course, &local->agent,
                    CC_COMBAT_SKILL_CRUSHING_BLOW);
            } else if (reel->stage_frame == 82 ||
                       reel->stage_frame == 154) {
                (void)CcLocalCourseBeginPlayerStrike(
                    &local->course, &local->agent);
            }
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if (reel->stage_frame >= 228) {
                CcCommand victory = {
                    .kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT
                };
                if (ApplyCommand(NULL, sim, victory, message,
                                 message_capacity)) {
                    BeginRoadTravelState(sim, local);
                    GameplayReelSetStage(reel,
                                         GAMEPLAY_REEL_RESUME_TRAVEL);
                } else {
                    reel->complete = true;
                }
            }
            break;
        case GAMEPLAY_REEL_RESUME_TRAVEL:
            (void)snprintf(message, message_capacity,
                           "The journey resumes after the fight.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, false);
            CcSimAdvanceRuntimeTicks(sim, 32);
            GameplayReelResolveJourneyStop(
                sim, message, message_capacity);
            if (!sim->journey.active) {
                const CcSituation *accepted = CcSimAcceptedSituation(sim);
                CcId target = SituationSettlementId(sim, accepted);
                CcLocalBindPlace(sim);
                ResetLocalStatePreservingAthletics(local);
                local->course.alarm_countdown = 1000.0f;
                if (target != 0U && target != sim->player.location_id &&
                    reel->journey_legs < sim->settlement_count) {
                    GameplayReelStartTownArrival(
                        local, reel->journey_legs, false);
                    GameplayReelSetStage(
                        reel, GAMEPLAY_REEL_WALK_TO_CARRIAGE);
                } else {
                    GameplayReelStartTownArrival(
                        local, reel->journey_legs, true);
                    GameplayReelSetStage(
                        reel, GAMEPLAY_REEL_ARRIVE_AT_MARKET);
                }
            }
            break;
        case GAMEPLAY_REEL_ARRIVE_AT_MARKET:
            (void)snprintf(message, message_capacity,
                           "The cargo has reached its town.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if ((reel->stage_frame >= 60 &&
                 GridDistance(LocalPosition(local), LOCAL_MARKET) < 1.30f) ||
                reel->stage_frame >= 1200) {
                GameplayReelTrade(sim, true, message, message_capacity);
                (void)CcLocalAgentSetExactTarget(
                    &local->agent, (Vector3){48.10f, 0.0f, 29.00f}, false);
                GameplayReelSetStage(reel, GAMEPLAY_REEL_QUEST_COMPLETE);
            }
            break;
        case GAMEPLAY_REEL_QUEST_COMPLETE:
            (void)snprintf(message, message_capacity,
                           "Quest complete. The promised cargo arrived.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if (reel->stage_frame >= 180) {
                GameplayReelSetStage(reel, GAMEPLAY_REEL_VILLAGE_ALARM);
            }
            break;
        case GAMEPLAY_REEL_VILLAGE_ALARM:
            if (!reel->stage_started) {
                reel->stage_started = true;
                *view = VIEW_LOCAL;
                CcLocalCourseRaiseAlarmNear(&local->course, &local->agent);
            }
            (void)snprintf(message, message_capacity,
                           "The village bell names the company on the road.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if (reel->stage_frame >= 180) {
                PrepareTownRaidReel(sim, local);
                GameplayReelSetStage(reel,
                                     GAMEPLAY_REEL_VILLAGE_COMBAT);
            }
            break;
        case GAMEPLAY_REEL_VILLAGE_COMBAT:
            (void)snprintf(message, message_capacity,
                           "Defend the same street with the town guard.");
            if (local->agent.combat.target_index < 0) {
                (void)CcLocalCourseSelectPlayerTarget(
                    &local->course, &local->agent, 0);
            }
            if (reel->stage_frame == 12) {
                (void)CcLocalCourseUsePlayerSkill(
                    &local->course, &local->agent,
                    CC_COMBAT_SKILL_CRUSHING_BLOW);
            } else if (reel->stage_frame == 96 ||
                       reel->stage_frame == 168) {
                (void)CcLocalCourseBeginPlayerStrike(
                    &local->course, &local->agent);
            }
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if (reel->stage_frame >= 240) reel->complete = true;
            break;
        default:
            reel->complete = true;
            break;
    }
}

static void DrawGameplayReelTransition(const GameplayReelState *reel)
{
    if (reel == NULL || reel->stage_frame > 40) return;
    const char *title = NULL;
    switch (reel->stage) {
        case GAMEPLAY_REEL_BUY_CARGO:
            title = "MARA'S MARKET";
            break;
        case GAMEPLAY_REEL_TRAVEL:
        case GAMEPLAY_REEL_RESUME_TRAVEL:
            title = "ON THE ROAD";
            break;
        case GAMEPLAY_REEL_ROAD_COMBAT:
            title = "BANDIT FIGHT";
            break;
        case GAMEPLAY_REEL_ARRIVE_AT_MARKET:
            title = "ARRIVAL";
            break;
        case GAMEPLAY_REEL_VILLAGE_COMBAT:
            title = "THE VILLAGE BELL";
            break;
        default:
            return;
    }
    float opacity = 1.0f - (float)reel->stage_frame / 40.0f;
    if (opacity < 0.0f) opacity = 0.0f;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, opacity));
    int width = CcOverlayMeasureText(title, 25);
    CcOverlayDrawText(title, (GetScreenWidth() - width) / 2,
                      GetScreenHeight() / 2 - 16, 25,
                      Fade(INK, fminf(1.0f, opacity * 1.6f)));
}

static void DrawGameplayReelQuestComplete(const GameplayReelState *reel)
{
    if (reel == NULL || reel->stage != GAMEPLAY_REEL_QUEST_COMPLETE) return;
    float fade_in = fminf(1.0f, (float)reel->stage_frame / 20.0f);
    float fade_out = fminf(1.0f,
                           (180.0f - (float)reel->stage_frame) / 20.0f);
    float opacity = fmaxf(0.0f, fminf(fade_in, fade_out));
    Rectangle bounds = {438.0f, 92.0f, 404.0f, 82.0f};
    DrawRectangleRounded(bounds, 0.16f, 5,
                         Fade(PANEL_DEEP, opacity));
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 5, 1.5f,
                                Fade(TEAL, opacity));
    const char *title = "QUEST COMPLETE";
    int width = CcOverlayMeasureText(title, 19);
    CcOverlayDrawText(title, (GetScreenWidth() - width) / 2, 112, 19,
                      Fade(TEAL, opacity));
    const char *detail = "The promised cargo arrived.";
    width = CcOverlayMeasureText(detail, 10);
    CcOverlayDrawText(detail, (GetScreenWidth() - width) / 2, 143, 10,
                      Fade(INK, opacity));
}
