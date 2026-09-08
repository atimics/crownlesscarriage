/* Scene fixtures run at the same startup boundaries as normal play. */
typedef struct CcCaptureScene {
    CcSim *sim;
    LocalState *local;
    CcCaptureState *state;
    int32_t *selected;
    int32_t *selected_situation;
    ClientView *view;
    ClientView *return_view;
    CcClientPreferences *preferences;
    int argc;
    char **argv;
} CcCaptureScene;

#if defined(CC_CLIENT_SELF_TESTS)
static void PrepareActionReel(LocalState *local, ActionReelState *reel)
{
    *reel = (ActionReelState){0};
    local->market_interior = false;
    CcLocalAgentInit(&local->agent, (Vector2){1.82f, 7.50f}, false);
    CcLocalCombatSetTeam(&local->agent, CC_COMBAT_PLAYER);
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        CcLocalAgentSetAthleticLevel(
            &local->agent, (CcAthleticDiscipline)discipline,
            CC_ATHLETIC_MAX_LEVEL);
    }
    local->agent.facing_yaw = 0.5f * PI;
    (void)CcLocalAgentSetExactTarget(
        &local->agent, (Vector3){3.50f, 0.0f, 7.50f}, false);

    local->course.alarm_active = true;
    local->course.alarm_countdown = 1000.0f;
    local->course.combat_origin = (Vector3){15.40f, 0.0f, 9.65f};
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgentInit(&local->course.runners[i].agent,
                         (Vector2){88.0f + (float)i, 66.0f}, false);
        local->course.runners[i].agent.crowned = false;
        CcLocalCombatSetTeam(&local->course.runners[i].agent,
                             CC_COMBAT_GUARD);
    }
    CcLocalAgent *opponent = &local->course.raiders[0];
    CcLocalAgentInit(opponent, (Vector2){18.20f, 9.65f}, false);
    opponent->crowned = false;
    opponent->tunic_color = DANGER;
    opponent->facing_yaw = -0.5f * PI;
    CcLocalCombatSetTeam(opponent, CC_COMBAT_RAIDER);
    CcLocalAgentSetAthleticLevel(opponent, CC_ATHLETIC_MOBILITY, 3);
    CcLocalAgentSetAthleticLevel(opponent, CC_ATHLETIC_POWER, 3);
    (void)CcLocalAgentSetExactTarget(
        opponent, (Vector3){15.40f, 0.0f, 9.65f}, false);
    CcLocalAgentInit(&local->course.raiders[1],
                     (Vector2){91.0f, 65.0f}, false);
    local->course.raiders[1].crowned = false;
    CcLocalCombatSetTeam(&local->course.raiders[1], CC_COMBAT_RAIDER);
}

static void PrepareGameplayReel(CcSim *sim, LocalState *local,
                                GameplayReelState *reel,
                                int32_t *selected,
                                int32_t *selected_situation,
                                ClientView *view, ClientView *return_view)
{
    *reel = (GameplayReelState){0};
    if (sim->player.coins < 120) sim->player.coins = 120;
    *selected = FirstOutgoingRouteIndex(sim);
    *selected_situation = FirstDeliverySituationIndex(sim);
    *view = VIEW_LOCAL;
    *return_view = VIEW_LOCAL;
    ResetLocalState(local);
    RepositionHero(local, (Vector2){44.25f, 28.85f}, false);
    local->course.alarm_countdown = 1000.0f;
    (void)CcLocalAgentSetExactTarget(
        &local->agent, (Vector3){41.90f, 0.0f, 28.45f}, false);
}

static bool CheckPonyInterface(CcSim *sim, LocalState *local, int32_t pony)
{
    char message[256] = {0};
    ClientInputClearPressed();
    queued_key_press[KEY_ENTER] = true;
    if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, (ContextAction){0}, message, sizeof(message)) ||
        !sim->pony_company.ponies[pony].ready) return false;
    CcPonyCompany ready = sim->pony_company;
    CcHorse horses[2] = {sim->horse_team[0], sim->horse_team[1]};
    for (int32_t slot = 0; slot < CcSimHorseTeamCount(sim); ++slot) {
        ClientInputClearPressed();
        queued_key_press[KEY_ONE + slot] = true;
        if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, (ContextAction){0}, message, sizeof(message)) ||
            sim->pony_company.team[slot] != pony ||
            sim->pony_company.team[1 - slot] != ready.team[1 - slot] ||
            sim->pony_company.ponies[ready.team[slot]].route_id == 0U ||
            sim->pony_company.ponies[ready.team[slot]].releases !=
                ready.ponies[ready.team[slot]].releases + 1) return false;
        sim->pony_company = ready;
        sim->horse_team[0] = horses[0];
        sim->horse_team[1] = horses[1];
    }
    ClientInputClearPressed();
    queued_key_press[KEY_F7] = true;
    if (!HandleCarriageTabs(local, VIEW_CARRIAGE) ||
        local->carriage_tab != CARRIAGE_PONIES) return false;
    ClientInputClearPressed();
    queued_key_press[KEY_ONE] = true;
    if (!HandleCarriageTabs(local, VIEW_CARRIAGE) ||
        local->carriage_tab != CARRIAGE_OVERVIEW) return false;
    ClientInputClearPressed();
    (void)puts("PASS pony interface: quest, harnessed swap slots, carriage tabs");
    return true;
}

static void CcCaptureSceneAbort(RenderTexture2D target, ClientMapTextures *maps,
                                 CcClientInstanceLock *lock)
{
    CcLocalRendererShutdown();
    UnloadRenderTexture(target);
    ReleaseMapTextures(maps);
    CcCoopClientShutdown();
    CloseWindow();
    CcClientInstanceLockRelease(lock);
}

static void CcCapturePrepareJourney(const CcCaptureRequest *request, CcSim *sim)
{
    if (!(request->capture_encounter || request->capture_travel ||
          request->capture_route_sight || request->capture_road ||
          request->capture_parley || request->capture_aftermath ||
          request->capture_creature_horse || request->capture_road_arrival)) return;
    PrepareReviewJourney(sim, (CcReviewJourneyOptions){
        .clear_route_knowledge = request->capture_route_sight,
        .suppress_encounter = request->capture_road_arrival || request->capture_route_sight,
        .advance_trip = request->capture_travel || request->capture_route_sight ||
            request->capture_creature_horse || request->capture_road_arrival,
        .finish_arrival = request->capture_road_arrival,
        .resolve_combat = request->capture_aftermath,
        .target_progress = request->capture_road_arrival ? 850 :
            request->capture_storybook ? (int32_t)(request->storybook_progress * 1000.0f + 0.5f) :
            request->capture_route_sight ? (int32_t)(request->capture_route_sight_progress * 1000.0f + 0.5f) : 200,
        .tick_step = request->capture_storybook ? 1 : CC_WORLD_TICKS_PER_SECOND,
    });
}

static bool CcCapturePrepareWorld(const CcCaptureRequest *request, CcSim *sim, char **argv)
{
    if (request->capture_creature_media &&
        strcmp(request->capture_creature_family, "goblins") == 0) {
        sim->player.location_id = sim->goblins.lair_settlement_id;
    } else if (request->capture_creature_dragon) {
        sim->player.location_id = sim->dragon.lair_settlement_id;
        sim->dragon.omen_days_remaining = 2;
        sim->goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
        if (strcmp(request->capture_creature_family, "dragon-whelp") == 0) {
            sim->dragon.life_stage = CC_DRAGON_STAGE_WHELP;
        } else if (strcmp(request->capture_creature_family,
                          "dragon-wanderer") == 0) {
            sim->dragon.life_stage = CC_DRAGON_STAGE_WANDERER;
        } else if (strcmp(request->capture_creature_family,
                          "dragon-deep-wyrm") == 0) {
            sim->dragon.life_stage = CC_DRAGON_STAGE_DEEP_WYRM;
        }
    } else if (request->capture_creature_media) {
        for (int32_t settlement = 0; settlement < sim->settlement_count;
             ++settlement) {
            if (sim->settlements[settlement].id != sim->player.location_id) {
                continue;
            }
            sim->settlements[settlement].stock[CC_GOOD_FOOD] = 32;
            break;
        }
    }
    if (request->capture_dragon_cave) {
        sim->player.location_id = sim->dragon.lair_settlement_id;
        sim->goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
        sim->goblins.tribute_target_id = sim->dragon.lair_settlement_id;
        sim->goblins.tribute_days_remaining = 2;
    }
    if (request->capture_road_fork || request->capture_map_case || request->capture_dragon_hoard_map) {
        sim->player.location_id = sim->settlements[1].id;
    }
    if (request->capture_town || request->capture_town_arrival) {
        sim->player.location_id = sim->settlements[request->capture_town_index].id;
    }
    if (request->capture_town_state) {
        CcSettlement *town = &sim->settlements[request->capture_town_index];
        town->security = 75;
        town->prosperity = strcmp(argv[6], "thriving") == 0 ? 85 : 45;
        town->hunger = 10;
        town->fire_damage = 0;
        town->last_fire_day = 0;
        town->service_project = CC_SERVICE_NONE;
        town->service_project_days = 0;
        if (strcmp(argv[6], "burnt") == 0 || strcmp(argv[6], "rebuilding") == 0) {
            town->fire_damage = 60;
            town->last_fire_day = sim->current_day;
            town->stock[CC_GOOD_WOOD] = strcmp(argv[6], "rebuilding") == 0 ? 12 : 0;
            town->stock[CC_GOOD_STONE] = 12;
            town->stock[CC_GOOD_TOOLS] = 12;
            town->prosperity = 25;
        }
        if (strcmp(argv[6], "lawless") == 0) town->security = 15;
        for (int32_t a = 0; a < sim->kingdom_count; ++a) {
            for (int32_t b = 0; b < sim->kingdom_count; ++b) {
                sim->diplomacy[a][b] = CC_DIPLOMACY_PEACE;
            }
        }
    }
    if (request->capture_witness || request->capture_character) {
        for (int32_t situation = 0; situation < sim->situation_count;
             ++situation) {
            if (sim->situations[situation].status != CC_SITUATION_ACTIVE) {
                continue;
            }
            const CcCharacter *character = CcSimSituationAffectedCharacter(
                sim, &sim->situations[situation]);
            if (character == NULL) continue;
            sim->player.location_id = character->current_settlement_id;
            break;
        }
    }
    return true;
}

static bool CcCapturePrepareDungeon(const CcCaptureRequest *request, CcSim *sim)
{
    if (request->capture_underroad && sim->dungeon_count > 0) {
        sim->player.location_id = sim->dungeons[0].settlement_id;
        sim->carriage.location_id = sim->player.location_id;
        sim->player.cargo[CC_GOOD_FOOD] = 6;
        sim->player.cargo[CC_GOOD_TOOLS] = 2;
        sim->player.cargo[CC_GOOD_WEAPONS] = 1;
        CcCommand enter = {
            .kind = CC_COMMAND_BEGIN_DUNGEON_EXPEDITION,
            .target_id = sim->dungeons[0].id
        };
        char setup_error[192];
        (void)CcSimApply(sim, &enter, setup_error, sizeof(setup_error));
        for (int32_t room = 0; room <= 11; ++room) {
            sim->dungeons[0].rooms[room].state_flags |=
                CC_DUNGEON_ROOM_DISCOVERED;
        }
        sim->dungeons[0].links[29].flags |= CC_DUNGEON_LINK_DISCOVERED;
        sim->dungeon_expedition.current_room = 9;
        sim->dungeon_expedition.turns_elapsed = 8;
        sim->dungeon_expedition.days_elapsed = 1;
        sim->dungeon_expedition.light_remaining = 10;
        sim->dungeon_expedition.noise = 3;
        sim->dungeon_expedition.strain = 28;
        sim->dungeon_expedition.maximum_depth = 2;
        sim->dungeon_expedition.encounter_kind =
            CC_DUNGEON_ENCOUNTER_TITHE_KEEPERS;
        sim->dungeon_expedition.encounter_reaction = 6;
        sim->dungeon_expedition.encounter_room = 9;
    }
    return true;
}

static bool CcCaptureEnterWorld(const CcCaptureRequest *request, CcCaptureScene *scene)
{
    if (request->capture_world) {
        const CcRoute *route = OpenWorldRouteFromSettlement(
            scene->sim, scene->sim->player.location_id);
        if (route != NULL) {
            (void)EnterOpenWorldAtRoadGate(scene->sim, scene->local, route->id);
            scene->local->world_carriage.camera_weight = 1.0f;
        }
    }
    return true;
}

static bool CcCapturePrepareDeparture(const CcCaptureRequest *request, CcCaptureScene *scene)
{
    if (request->capture_opening) {
        BeginOpening(scene->local);
        (*scene->view) = VIEW_LOCAL;
    }
    if (request->capture_carriage || request->capture_carriage_target) {
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            scene->sim->player.cargo[good] = 1;
        }
    }
    if (request->capture_carriage) {
        RepositionHero(scene->local, LOCAL_CARRIAGE_BAY, false);
        scene->local->agent.world_target = CC_LOCAL_WORLD_TARGET_CARRIAGE;
        scene->local->course.alarm_countdown = 1000.0f;
    }
    if (request->capture_underroad) {
        scene->local->site_kind = CC_LOCAL_SITE_DUNGEON;
        RepositionHero(
            scene->local,
            (Vector2){CC_LOCAL_SITE_ENTRANCE_X - 3.0f,
                      CC_LOCAL_SITE_ENTRANCE_Z}, false);
    }
    if (request->capture_carriage_target) {
        (void)CcLocalAgentApproachWorldTarget(
            &scene->local->agent, CC_LOCAL_WORLD_TARGET_CARRIAGE);
        scene->local->course.alarm_countdown = 1000.0f;
    }
    if (request->capture_road_fork || request->capture_road_departure) {
        const CcRoute *route = SelectedOutgoingRoute(scene->sim, (*scene->selected));
        if (route != NULL) {
            (void)EnterOpenWorldAtRoadGate(scene->sim, scene->local, route->id);
            scene->local->world_carriage.camera_weight = request->capture_road_departure ?
                request->capture_road_zoom_weight : 1.0f;
            scene->local->world_carriage.camera_target =
                scene->local->world_carriage.camera_weight;
            scene->local->road_choice_active = true;
            scene->local->departure = (CcClientDepartureTransition){
                .phase = CC_CLIENT_DEPARTURE_READY,
                .town_progress = 1.0f,
                .road_book_progress = request->capture_road_departure ?
                    request->capture_road_zoom_weight : 1.0f,
            };
            scene->local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
            scene->local->convoy.phase_progress = scene->local->departure.road_book_progress;
            scene->local->convoy.pace = 0.0f;
            if (request->capture_road_fork) PositionOpenWorldDeparture(scene->sim, scene->local);
            if (request->capture_road_departure) {
                PositionOpenWorldDeparture(scene->sim, scene->local);
                if (request->capture_road_zoom_weight < 1.0f) {
                    scene->local->convoy.pace = 0.72f;
                    scene->local->world_carriage.pace = scene->local->convoy.pace;
                }
            }
        }
        scene->local->fork_turn_progress = 1.0f;
    }
    return true;
}

static bool CcCapturePrepareLocal(const CcCaptureRequest *request, CcCaptureScene *scene)
{
    uint32_t walk_cycle_mask = 0;
    if (request->capture_dragon_cave) {
        scene->local->site_kind = CC_LOCAL_SITE_DRAGON_CAVE;
        RepositionHero(
            scene->local,
            (Vector2){CC_LOCAL_SITE_ENTRANCE_X - 3.0f,
                      CC_LOCAL_SITE_ENTRANCE_Z}, false);
        CcLocalAgentSetScene(&scene->local->agent, CC_LOCAL_SCENE_ROAD);
        scene->local->course.scene = CC_LOCAL_SCENE_ROAD;
        scene->local->course.alarm_countdown = 1000.0f;
    }
    if (request->capture_golden || request->capture_town || request->capture_atmosphere) {
        RepositionHero(
            scene->local,
            request->capture_town ? (Vector2){request->capture_town_x, request->capture_town_z} :
                           (Vector2){44.25f, 28.85f},
            false);
        scene->local->agent.facing_yaw = -0.35f;
        scene->local->course.alarm_countdown = 1000.0f;
    }
    if (request->capture_town_arrival) {
        BeginTownArrivalState(scene->local);
        scene->local->convoy.phase_progress = request->capture_town_arrival_progress;
        SetConvoyTownPose(&scene->local->convoy, 0.0f);
    }
    if (request->capture_face) {
        if (request->capture_face_view == 3) {
            RepositionHero(scene->local, (Vector2){64.0f, 31.0f}, false);
            scene->local->agent.facing_yaw = -0.32f;
        } else {
            scene->local->market_interior = true;
            CcLocalAgentInit(&scene->local->agent, (Vector2){4.60f, 5.10f}, true);
            CcLocalCombatSetTeam(&scene->local->agent, CC_COMBAT_PLAYER);
            scene->local->agent.facing_yaw = 0.14f +
                (request->capture_face_view == 1 ? -0.38f :
                 request->capture_face_view == 2 ? 1.42f :
                 request->capture_face_view == 4 ? PI : 0.0f);
        }
        CcLocalAgentSetMorphology(&scene->local->agent, CC_MORPHOLOGY_BIPED,
                                  scene->local->market_interior);
        scene->local->course.alarm_countdown = 1000.0f;
    }
    if (request->capture_room) {
        RepositionHero(scene->local,
                       (Vector2){request->capture_room_x, request->capture_room_z}, false);
        scene->local->agent.facing_yaw = -0.18f;
        scene->local->course.alarm_countdown = 1000.0f;
    }
    if (request->capture_creature_media) {
        if (request->capture_creature_horse) {
            BeginRoadTravelState(scene->sim, scene->local);
        } else if (request->capture_creature_dragon ||
                   strcmp(request->capture_creature_family, "goblins") == 0) {
            scene->local->site_kind = request->capture_creature_dragon ?
                CC_LOCAL_SITE_DRAGON_CAVE : CC_LOCAL_SITE_GOBLIN_CAVE;
            RepositionHero(
                scene->local,
                (Vector2){CC_LOCAL_SITE_ENTRANCE_X -
                              (request->capture_creature_dragon ? 12.0f : 7.0f),
                          CC_LOCAL_SITE_ENTRANCE_Z}, false);
            CcLocalAgentSetScene(&scene->local->agent, CC_LOCAL_SCENE_ROAD);
            scene->local->course.scene = CC_LOCAL_SCENE_ROAD;
            scene->local->course.alarm_countdown = 1000.0f;
        } else {
            bool animal_view =
                strcmp(request->capture_creature_family, "animals") == 0 ||
                strcmp(request->capture_creature_family, "cow") == 0 ||
                strcmp(request->capture_creature_family, "sheep") == 0;
            Vector2 creature_view = animal_view ?
                (Vector2){59.5f, 40.0f} : (Vector2){24.5f, 49.5f};
            RepositionHero(scene->local, creature_view, false);
            scene->local->agent.facing_yaw = -0.18f;
            scene->local->course.alarm_countdown = 1000.0f;
        }
    }
    if (request->capture_road || request->capture_parley) {
        BeginRoadLocalState(scene->sim, scene->local, request->capture_road);
        if (request->capture_road && scene->argc >= 4 && strcmp(scene->argv[3], "focused") == 0) {
            (void)CcLocalCourseSelectPlayerTarget(&scene->local->course, &scene->local->agent, 0);
        }
    }
    if (request->capture_travel || request->capture_route_sight) {
        BeginRoadTravelState(scene->sim, scene->local);
        scene->local->travel_time_blend = request->capture_storybook ? request->storybook_blend :
                                   request->capture_route_sight ? 1.0f : 0.0f;
        scene->local->travel_fast_forward = scene->local->travel_time_blend > 0.5f;
        scene->local->world_carriage.storybook_travel = !request->capture_route_sight;
        scene->local->world_carriage.camera_weight = scene->local->travel_time_blend;
        scene->local->world_carriage.camera_target = scene->local->travel_time_blend;
    }
    if (request->capture_pony_book || request->capture_pony_encounter || request->capture_pony_swap) {
        int32_t pony = 0;
        while (pony == scene->sim->pony_company.team[0] || pony == scene->sim->pony_company.team[1]) pony++;
        scene->sim->pony_company.ponies[pony].route_id = scene->sim->journey.route_id;
        char pony_error[160];
        CcCommand meet = {.kind = CC_COMMAND_MEET_PONY, .target_id = (CcId)pony + 1U};
        if (!CcSimApply(scene->sim, &meet, pony_error, sizeof(pony_error))) {
            (void)fprintf(stderr, "Pony capture: %s\n", pony_error);
            return false;
        }
        if (request->capture_pony_swap || request->capture_pony_book) {
            CcPony *p = &scene->sim->pony_company.ponies[pony];
            scene->sim->player.cargo[CcPonyQuestGood(p)] = p->quest_amount;
            if (!CheckPonyInterface(scene->sim, scene->local, pony)) return false;
            if (request->capture_pony_book) {
                queued_key_press[KEY_ONE] = true;
                if (!HandlePonyInput(NULL, scene->sim, scene->local, VIEW_LOCAL, (ContextAction){0},
                                     pony_error, sizeof(pony_error))) return false;
            }
        }
        if (request->capture_pony_book) {
            (*scene->view) = VIEW_CARRIAGE;
            scene->local->carriage_tab = CARRIAGE_PONIES;
        }
        scene->local->convoy.pace = 0.0f;
        scene->local->world_carriage.pace = 0.0f;
    }
    if (request->capture_road_arrival) {
        if (EnterOpenWorldAtRoadGate(
                scene->sim, scene->local, scene->sim->journey.route_id)) {
            scene->local->convoy.pace = 0.48f;
            BeginRoadBookArrivalState(scene->sim, scene->local);
            scene->local->arrival.road_book_progress =
                1.0f - request->capture_road_zoom_weight;
            scene->local->arrival.phase = CC_CLIENT_ARRIVAL_ROAD_BOOK;
            scene->local->world_carriage.camera_weight = request->capture_road_zoom_weight;
            scene->local->world_carriage.camera_target = request->capture_road_zoom_weight;
            (void)PositionOpenWorldArrival(scene->sim, scene->local);
            scene->local->world_carriage.storybook_travel = request->capture_storybook_arrival;
            scene->local->world_carriage.arrival_travel_weight = 1.0f;
            scene->local->world_carriage.camera_heading_yaw = scene->local->world_carriage.heading_yaw;
        }
    }
    if (request->capture && !request->capture_world && !request->capture_interior &&
        !request->capture_walk_cycle &&
        !request->capture_jump && !request->capture_defense && !request->capture_downclimb &&
        !request->capture_navigation && !request->capture_limbs && !request->capture_dojo &&
        !request->capture_action_reel && !request->capture_gameplay_reel &&
        !request->capture_encounter && !request->capture_travel && !request->capture_route_sight &&
        !request->capture_road_fork &&
        !request->capture_road_zoom &&
        !request->capture_road &&
        !request->capture_parley && !request->capture_carriage &&
        !request->capture_carriage_target &&
        !request->capture_golden && !request->capture_town &&
        !request->capture_town_arrival &&
        !request->capture_atmosphere &&
        !request->capture_face && !request->capture_room && !request->capture_creature_media) {
        scene->local->course.alarm_countdown = 1000.0f;
        for (int32_t frame = 0; frame < 1500; ++frame) {
            CcLocalCourseUpdate(&scene->local->course, &scene->local->agent, scene->sim,
                                1.0f / 60.0f);
        }
    }
    if (request->capture_character && scene->local->course.situation_witness_active) {
        RepositionHero(
            scene->local,
            (Vector2){scene->local->course.situation_witness.position.x + 1.15f,
                      scene->local->course.situation_witness.position.z + 0.88f},
            false);
        scene->local->conversation_character_id =
            scene->local->course.situation_witness_character_id;
        scene->local->conversation_situation_id =
            scene->local->course.situation_witness_id;
        Vector3 toward_witness = {
            scene->local->course.situation_witness.position.x -
                scene->local->agent.position.x,
            0.0f,
            scene->local->course.situation_witness.position.z -
                scene->local->agent.position.z,
        };
        scene->local->agent.facing_yaw = atan2f(
            toward_witness.x, toward_witness.z);
        scene->local->course.situation_witness.facing_yaw = atan2f(
            -toward_witness.x, -toward_witness.z);
    }
    if (request->capture_jump) {
        scene->local->course.alarm_countdown = 1000.0f;
        scene->local->agent.facing_yaw = 0.25f * PI;
        (void)CcLocalAgentJump(&scene->local->agent);
        for (int32_t frame = 0; frame < 27; ++frame) {
            CcLocalAgentUpdate(&scene->local->agent, 1.0f / 60.0f, false);
        }
    }
    if (request->capture_action_reel) PrepareActionReel(scene->local, &scene->state->action_reel);
    if (request->capture_gameplay_reel) {
        PrepareGameplayReel(scene->sim, scene->local, &scene->state->gameplay_reel, scene->selected,
                            scene->selected_situation, scene->view, scene->return_view);
    }
    if (request->capture_downclimb) {
        CcLocalAgentInit(&scene->local->agent, (Vector2){3.50f, 6.20f}, false);
        (void)CcLocalAgentSetExactTarget(
            &scene->local->agent, (Vector3){3.50f, 0.0f, 7.50f}, false);
        for (int32_t frame = 0; frame < 1200; ++frame) {
            CcLocalAgentUpdate(&scene->local->agent, 1.0f / 60.0f, false);
            if (!scene->local->agent.exact_target_valid && scene->local->agent.grounded &&
                fabsf(scene->local->agent.position.y - 1.65f) < 0.01f) break;
        }
        (void)CcLocalAgentSetExactTarget(
            &scene->local->agent, (Vector3){3.50f, 0.0f, 6.20f}, false);
        for (int32_t frame = 0; frame < 600; ++frame) {
            CcLocalAgentUpdate(&scene->local->agent, 1.0f / 60.0f, false);
            if (scene->local->agent.climbing_down &&
                scene->local->agent.climb_progress >= 0.55f) break;
        }
    }
    if (request->capture_defense) {
        CcLocalCourseBindRaiderCompany(&scene->local->course, scene->sim);
        CcLocalCourseRaiseAlarmNear(&scene->local->course, &scene->local->agent);
        (void)CcLocalCourseSelectPlayerTarget(&scene->local->course,
                                               &scene->local->agent, 0);
        (void)CcLocalCourseUsePlayerSkill(
            &scene->local->course, &scene->local->agent, CC_COMBAT_SKILL_CRUSHING_BLOW);
        int32_t fighting_frames = 0;
        bool captured_skill_impact = false;
        for (int32_t frame = 0; frame < 9000 && !captured_skill_impact;
             ++frame) {
            CcLocalAgentUpdate(&scene->local->agent, 1.0f / 60.0f, false);
            CcLocalCourseUpdate(&scene->local->course, &scene->local->agent, scene->sim,
                                1.0f / 60.0f);
            fighting_frames = scene->local->course.alarm_active &&
                              !scene->local->course.raiders_retreating &&
                              scene->local->course.raider_resolve <
                                  scene->local->course.raider_initial_resolve ?
                              fighting_frames + 1 : 0;
            captured_skill_impact =
                scene->local->agent.humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
                scene->local->agent.combat.active_skill ==
                    CC_COMBAT_SKILL_CRUSHING_BLOW &&
                scene->local->agent.humanoid.action_time >= 0.54f;
        }
        (void)printf("defense capture: alarm %d retreat %d resolve %d wins %d fight frames %d skill impact %d origin %.2f,%.2f guards %.2f,%.2f raiders %.2f,%.2f\n",
                     scene->local->course.alarm_active,
                     scene->local->course.raiders_retreating,
                     scene->local->course.raider_resolve,
                     scene->local->course.defenses_completed, fighting_frames,
                     captured_skill_impact,
                     scene->local->course.combat_origin.x,
                     scene->local->course.combat_origin.z,
                     scene->local->course.runners[0].agent.position.x,
                     scene->local->course.runners[0].agent.position.z,
                     scene->local->course.raiders[0].position.x,
                     scene->local->course.raiders[0].position.z);
    }
    if (request->capture_dojo) {
        scene->local->course.alarm_countdown = 1000.0f;
        CcLocalCourseRunner *swimmer = &scene->local->course.runners[0];
        CcLocalAgentInit(&swimmer->agent, (Vector2){13.90f, 9.78f}, false);
        swimmer->agent.crowned = false;
        swimmer->agent.tunic_color = swimmer->marker_color;
        (void)CcLocalAgentSetExactTarget(
            &swimmer->agent, (Vector3){9.40f, 0.0f, 9.72f}, false);
        for (int32_t frame = 0; frame < 420; ++frame) {
            CcLocalCourseUpdate(&scene->local->course, &scene->local->agent, scene->sim,
                                1.0f / 60.0f);
            if (swimmer->agent.swimming && frame > 90) break;
        }
    }
    if (request->capture_interior) {
        scene->local->market_interior = true;
        CcLocalAgentInit(&scene->local->agent, (Vector2){4.60f, 5.10f}, true);
    }
    if (request->capture_limbs) {
        CcLocalAgentSetMorphology(&scene->local->agent, CC_MORPHOLOGY_BIPED, false);
        if (CcLocalAgentSetExactTarget(&scene->local->agent,
                                       (Vector3){5.10f, 0.0f, 4.80f}, false)) {
            for (int32_t frame = 0; frame < 120; ++frame) {
                CcLocalAgentUpdate(&scene->local->agent, 1.0f / 60.0f, false);
                if (frame > 30 &&
                    (scene->local->agent.humanoid.feet[0].contact ==
                         CC_HUMANOID_CONTACT_SWING ||
                     scene->local->agent.humanoid.feet[1].contact ==
                         CC_HUMANOID_CONTACT_SWING)) break;
            }
        }
    }
    if (request->capture_walk_cycle) {
        scene->local->market_interior = true;
        CcLocalAgentInit(&scene->local->agent, (Vector2){4.00f, 5.50f}, true);
        (void)CcLocalAgentSetExactTarget(&scene->local->agent,
                                         (Vector3){8.00f, 0.0f, 5.50f}, true);
        for (int32_t frame = 0; frame < 1200 && walk_cycle_mask != UINT32_C(0xff);
             ++frame) {
            CcLocalAgentUpdate(&scene->local->agent, 1.0f / 60.0f, true);
            float speed = sqrtf(scene->local->agent.velocity.x * scene->local->agent.velocity.x +
                                scene->local->agent.velocity.z * scene->local->agent.velocity.z);
            int32_t bin = (int32_t)floorf(scene->local->agent.humanoid.phase * 8.0f) & 7;
            uint32_t bit = UINT32_C(1) << bin;
            if (speed > 0.25f && (walk_cycle_mask & bit) == 0) {
                scene->state->walk_cycle_frames[bin] = scene->local->agent;
                walk_cycle_mask |= bit;
            }
        }
        if (walk_cycle_mask != UINT32_C(0xff)) {
            (void)fprintf(stderr, "could not sample every walk pose (mask 0x%02x)\n",
                          walk_cycle_mask);
            return false;
        }
        scene->local->agent = scene->state->walk_cycle_frames[0];
    }
    if (request->capture_navigation &&
        CcLocalAgentSetExactTarget(&scene->local->agent, (Vector3){3.50f, 0.0f, 7.50f},
                                   false)) {
        for (int32_t frame = 0; frame < 600; ++frame) {
            CcLocalAgentUpdate(&scene->local->agent, 1.0f / 60.0f, false);
            if (!scene->local->agent.grounded && scene->local->agent.position.y > 0.65f) break;
        }
    }
    return true;
}

static bool CcCapturePrepareUX(const CcCaptureRequest *request, CcCaptureScene *scene)
{
    if (request->capture_ux) {
        scene->local->adventure_ui = true;
        scene->local->course.automatic_alarm = false;
        RepositionHero(scene->local, (Vector2){44.25f, 28.85f}, false);
        (void)CcLocalWorldUpdate(&scene->local->course, &scene->local->agent, scene->sim, 0.2f, false, true);
        if (request->capture_ux_view == 1 || request->capture_ux_view == 3) {
            scene->local->market_interior = true;
            RepositionHero(scene->local, (Vector2){4.5f, 4.2f}, true);
        }
        if (request->capture_ux_view == 2) {
            scene->local->conversation_character_id = scene->local->course.situation_witness_character_id;
            scene->local->conversation_situation_id = scene->local->course.situation_witness_id;
            RepositionHero(scene->local, (Vector2){scene->local->course.situation_witness.position.x + 1.4f,
                scene->local->course.situation_witness.position.z + 0.4f}, false);
            (*scene->view) = VIEW_CHARACTER;
        }
        if (request->capture_ux_view == 13 || request->capture_ux_view == 14) {
            for (int32_t i = 0; i < scene->sim->character_count; ++i) {
                const CcCharacter *person = &scene->sim->characters[i];
                if (person->current_settlement_id != scene->sim->player.location_id ||
                    person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING || CcCharacterAgeYears(scene->sim, person) < 16) continue;
                scene->local->conversation_character_id = person->id;
                scene->local->conversation_situation_id = 0U;
                scene->local->conversation_gossip_source = request->capture_ux_view == 14;
                char capture_error[192];
                if (!ApplyCommand(NULL, scene->sim, (CcCommand){.kind = CC_COMMAND_EXCHANGE_GOSSIP,
                    .target_id = person->id}, capture_error, sizeof(capture_error))) return false;
                (*scene->view) = VIEW_CHARACTER;
                break;
            }
        }
        if (request->capture_ux_view == 3) { (*scene->view) = VIEW_TRADE; scene->local->trade_good = CC_GOOD_FOOD; scene->local->trade_quantity = 2; }
        if (request->capture_ux_view == 4) { (*scene->view) = VIEW_LEDGER; scene->local->book_page = 3; }
        if (request->capture_ux_view == 11 || request->capture_ux_view == 12) {
            (*scene->view) = VIEW_CARRIAGE;
            scene->local->carriage_tab = CARRIAGE_PONIES;
            int32_t pony = scene->sim->pony_company.team[0];
            if (request->capture_ux_view == 12) {
                for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
                    if (i != scene->sim->pony_company.team[0] && i != scene->sim->pony_company.team[1]) {
                        pony = i;
                        break;
                    }
                }
            }
            scene->sim->pony_company.ponies[pony].seen = true;
            scene->sim->pony_company.ponies[pony].bond = 3;
            scene->sim->pony_company.ponies[pony].quests_completed = 2;
            scene->sim->pony_company.ponies[pony].releases = request->capture_ux_view == 12 ? 1 : 0;
            scene->sim->pony_company.ponies[pony].last_seen_route = scene->sim->routes[0].id;
        }
        if (request->capture_ux_view == 5) (*scene->view) = VIEW_LOCAL;
        if (request->capture_ux_view == 6) (*scene->view) = VIEW_SITUATIONS;
        if (request->capture_ux_view >= 7 && request->capture_ux_view <= 9) {
            (void)InitializeOpenWorld(scene->sim, scene->local, false);
            (*scene->selected) = FirstOutgoingRouteIndex(scene->sim);
            const CcRoute *route = SelectedOutgoingRoute(scene->sim, (*scene->selected));
            if (route == NULL) return false;
            if (request->capture_ux_view == 7) {
                (void)EnterOpenWorldAtRoadGate(scene->sim, scene->local, route->id);
                scene->local->road_choice_active = true;
                scene->local->departure = (CcClientDepartureTransition){.phase = CC_CLIENT_DEPARTURE_READY,
                    .town_progress = 1, .road_book_progress = 1};
                scene->local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
                PositionOpenWorldDeparture(scene->sim, scene->local);
                (*scene->view) = VIEW_ROADS;
            } else {
                CcCommand travel = {.kind = CC_COMMAND_TRAVEL,
                    .target_id = RouteOtherEnd(route, scene->sim->player.location_id)};
                char capture_error[192];
                if (!ApplyCommand(NULL, scene->sim, travel, capture_error, sizeof(capture_error))) return false;
                if (request->capture_ux_view == 9) {
                    for (int32_t site = 0; site < scene->sim->road_site_count; ++site) {
                        if (scene->sim->road_sites[site].route_id != route->id) continue;
                        scene->sim->carriage.progress_milli = route->from_id == scene->sim->journey.origin_id ?
                            scene->sim->road_sites[site].progress_milli : 1000 - scene->sim->road_sites[site].progress_milli;
                        scene->sim->journey.elapsed_subticks = (int32_t)((int64_t)scene->sim->journey.total_subticks *
                            scene->sim->carriage.progress_milli / 1000);
                        break;
                    }
                }
                BeginRoadTravelState(scene->sim, scene->local);
                (*scene->view) = VIEW_LOCAL;
            }
        }
        if (scene->argc >= 6) scene->preferences->text_size = atoi(scene->argv[5]) == 2 ? 2 : 0;
    }
    return true;
}

static bool CcCapturePrepareMessage(const CcCaptureRequest *request, CcCaptureScene *scene, char *message, size_t message_size)
{
    if (request->capture_golden) {
        (void)snprintf(message, message_size, "Town square.");
    } else if (request->capture_town || request->capture_town_arrival) {
        (void)snprintf(message, message_size, "%s town plan.",
                       CcSettlementFunctionName(
                           scene->sim->settlements[request->capture_town_index].function));
    } else if (request->capture_atmosphere) {
        (void)snprintf(message, message_size, "%s.",
                       CcLocalAtmosphereName(request->capture_atmosphere_preset));
    } else if (request->capture_face) {
        (void)snprintf(message, message_size,
                       "Same character model.");
    } else if (request->capture_room) {
        (void)snprintf(message, message_size, "Town view.");
    } else if (request->capture_creature_media) {
        (void)snprintf(message, message_size, "Creature settlement.");
    }
    if (request->capture_road) {
        (void)snprintf(message, message_size, "Break the company line.");
    } else if (request->capture_parley) {
        (void)snprintf(message, message_size, "Speak with the captain.");
    } else if (request->capture_travel || request->capture_route_sight) {
        (void)snprintf(message, message_size, "Travelling.");
    }
    if(request->capture_mine_yard || request->capture_mine_level) {
        const CcRoadSite *site=CcMineSite(scene->sim);
        const CcRoute *road=CcSimRoute(scene->sim,site->route_id);
        scene->sim->player.location_id=road->from_id; scene->sim->carriage.location_id=road->from_id;
        char mine_error[192];
        CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=road->to_id};
        if(!CcSimApply(scene->sim,&travel,mine_error,sizeof(mine_error))) return false;
        scene->sim->journey.elapsed_subticks=(int32_t)(((int64_t)scene->sim->journey.total_subticks*site->progress_milli+999)/1000);
        scene->sim->carriage.progress_milli=(int32_t)((int64_t)scene->sim->journey.elapsed_subticks*1000/scene->sim->journey.total_subticks);
        scene->sim->pony_company.encounter=-1;
        CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=site->id};
        if(!CcSimApply(scene->sim,&visit,mine_error,sizeof(mine_error))) return false;
        if(request->capture_mine_level) {
            scene->sim->mine.phase=CC_MINE_LEVEL; scene->sim->mine.x=15; scene->sim->mine.y=5;scene->sim->mine.light=18;scene->sim->mine.seen=3;
        }
        (*scene->view)=VIEW_LOCAL;
        (void)snprintf(message,message_size,"Follow the mine road. Pack food before entering.");
    }
    return true;
}

#else
#define CcCaptureSceneAbort(...) ((void)0)
#define CcCapturePrepareJourney(...) ((void)0)
#define CcCapturePrepareWorld(...) true
#define CcCapturePrepareDungeon(...) true
#define CcCaptureEnterWorld(...) true
#define CcCapturePrepareDeparture(...) true
#define CcCapturePrepareLocal(...) true
#define CcCapturePrepareUX(...) true
#define CcCapturePrepareMessage(...) true
#endif
