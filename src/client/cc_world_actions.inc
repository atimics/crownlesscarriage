/* World hit areas point back to the current card actions. Commands remain in
   HandleInput, so a card and its world object share checks and journal writes. */
static ContextAction WorldContextActionAt(const CcSim *sim, const LocalState *local,
    ClientView view, const ContextActionSet *actions, Vector2 mouse)
{
    ContextAction none = {.kind = CONTEXT_ACTION_NONE};
    if (mouse.y < 88 || mouse.y >= (float)ContextViewportHeight() - 116 ||
        (view != VIEW_LOCAL && view != VIEW_ROADS) ||
        view != local->interaction_view || AdventureScene(local)) return none;
    const CcInteractionTarget *target = WorldActionPointerTarget(local, mouse);
    if (target == NULL || target->key.kind != CC_INTERACTION_ACTION ||
        target->key.place != sim->player.location_id) return none;
    for (int32_t i = 0; i < actions->count; ++i) {
        const ContextAction *action = &actions->items[i];
        if ((int32_t)action->kind == target->action &&
            action->amount == target->amount &&
            (action->target.kind == CC_INTERACTION_NONE ||
             action->target.object == target->subject_id)) return *action;
    }
    return none;
}

static Vector3 WorldActionCarriagePosition(const LocalState *local)
{
    if (local->open_world) return local->world_carriage.position;
    if (local->site_travel_active) {
        float x = local->site_returning ? 62.0f - local->site_travel_progress * 40.0f :
            22.0f + local->site_travel_progress * 40.0f;
        return (Vector3){x, 0, CC_LOCAL_SITE_CARRIAGE_Z};
    }
    if (local->site_kind != CC_LOCAL_SITE_NONE)
        return (Vector3){CC_LOCAL_SITE_CARRIAGE_X, 0, CC_LOCAL_SITE_CARRIAGE_Z};
    if (local->convoy.phase == CC_LOCAL_CONVOY_DEPARTING ||
        local->convoy.phase == CC_LOCAL_CONVOY_ARRIVING) return local->convoy.town_position;
    return (Vector3){CC_LOCAL_ROAD_START_X, 0, CC_LOCAL_ROAD_START_Z};
}

static Vector3 WorldActionAlongRoad(const LocalState *local, Vector3 carriage, float distance, float side)
{
    float heading = local->open_world ? local->world_carriage.heading_yaw : PI * 0.5f;
    Vector3 point = {carriage.x + sinf(heading) * distance + cosf(heading) * side, 0,
        carriage.z + cosf(heading) * distance - sinf(heading) * side};
    point.y = local->open_world ? CcWorldTerrainHeight(&local->world_stream.manifest, point.x, point.z) : 0;
    return point;
}

/* The road carriage keeps one identity while the renderer updates its pose.
 * The route id and current simulation progress form the activation revision. */
static bool RoadCarriageTarget(const CcSim *sim, const LocalState *local,
                               CcInteractionTarget *target)
{
    if (sim == NULL || local == NULL || target == NULL ||
        !local->journey_travel_active || !local->open_world ||
        !local->world_carriage.visible || !sim->journey.active ||
        sim->journey.route_id == 0U) return false;
    Vector3 carriage = WorldActionCarriagePosition(local);
    Vector3 approach = WorldActionAlongRoad(local, carriage, 0.0f, 3.4f);
    *target = (CcInteractionTarget){
        .key = {sim->player.location_id, sim->journey.route_id,
                CC_INTERACTION_CARRIAGE},
        .subject_id = sim->journey.route_id,
        .amount = sim->carriage.progress_milli,
        .x = carriage.x, .y = carriage.y, .z = carriage.z,
        .width = 3.4f, .height = 3.2f, .depth = 5.2f,
        .approach_x = approach.x, .approach_z = approach.z,
        .radius = 1.45f, .available = true};
    (void)snprintf(target->name, sizeof(target->name), "Crownless carriage");
    (void)snprintf(target->verb, sizeof(target->verb),
                   local->world_carriage.hero_embarked ?
                       "Inspect carriage" : "Board carriage");
    return true;
}

static bool RoadCarriageRevisionMatches(const CcSim *sim,
                                        const LocalState *local)
{
    return sim != NULL && local != NULL && sim->journey.active &&
        sim->journey.route_id == local->road_carriage_route_id &&
        sim->carriage.progress_milli == local->road_carriage_progress_milli;
}

static bool BeginRoadCarriageApproach(const CcSim *sim, LocalState *local,
                                      char *message, size_t capacity)
{
    CcInteractionTarget target = {0};
    if (!RoadCarriageTarget(sim, local, &target) ||
        local->world_carriage.hero_embarked) return false;
    local->road_carriage_route_id = sim->journey.route_id;
    local->road_carriage_progress_milli = sim->carriage.progress_milli;
    if (!CcInteractionStart(&local->interaction, &target,
                            local->agent.position.x,
                            local->agent.position.z)) return false;
    if (!CcLocalAgentApproachInteraction(&local->agent,
            (Vector2){target.approach_x, target.approach_z},
            target.radius * 0.9f, false)) {
        CcInteractionCancel(&local->interaction,
                            "Choose a clear path to the carriage.");
        (void)snprintf(message, capacity, "%s", local->interaction.feedback);
        return true;
    }
    (void)snprintf(message, capacity, "Approaching the carriage.");
    return true;
}

static bool HandleRoadCarriageInteraction(
    const CcSim *sim, LocalState *local, ClientView *view,
    ContextActionKind action, CcInteractionKey key, float delta_time,
    char *message, size_t capacity)
{
    if (sim == NULL || local == NULL || view == NULL ||
        !local->journey_travel_active || !local->open_world) return false;
    if (*view == VIEW_LOCAL && ClientKeyPressed(KEY_F) &&
        action == CONTEXT_ACTION_NONE) {
        action = local->world_carriage.hero_embarked ?
            CONTEXT_ACTION_INSPECT_CARRIAGE : CONTEXT_ACTION_BOARD_CARRIAGE;
    }
    bool carriage_target = action == CONTEXT_ACTION_WORLD_TARGET &&
        key.kind == CC_INTERACTION_CARRIAGE && key.object == sim->journey.route_id;
    if ((action == CONTEXT_ACTION_INSPECT_CARRIAGE || carriage_target) &&
        local->world_carriage.hero_embarked) {
        local->carriage_inspection_road = true;
        CcInteractionCancel(&local->interaction, "");
        *view = VIEW_CARRIAGE;
        message[0] = '\0';
        return true;
    }
    if ((action == CONTEXT_ACTION_BOARD_CARRIAGE || carriage_target) &&
        !local->interaction.approaching) {
        return BeginRoadCarriageApproach(sim, local, message, capacity);
    }
    if (!local->interaction.approaching ||
        local->interaction.pending.kind != CC_INTERACTION_CARRIAGE) return false;
    bool manual_movement = ClientKeyPressed(KEY_ESCAPE) ||
        ClientKeyPressed(KEY_W) || ClientKeyPressed(KEY_A) ||
        ClientKeyPressed(KEY_S) || ClientKeyPressed(KEY_D) ||
        ClientKeyPressed(KEY_UP) || ClientKeyPressed(KEY_DOWN) ||
        ClientKeyPressed(KEY_LEFT) || ClientKeyPressed(KEY_RIGHT) ||
        IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) ||
        IsKeyDown(KEY_D) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) ||
        IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) ||
        (action == CONTEXT_ACTION_NONE &&
         ClientMouseButtonPressed(MOUSE_BUTTON_LEFT));
    if (manual_movement ||
        (action != CONTEXT_ACTION_NONE &&
         action != CONTEXT_ACTION_BOARD_CARRIAGE)) {
        CcInteractionCancel(&local->interaction, "Approach cancelled.");
        CcLocalAgentStop(&local->agent);
        (void)snprintf(message, capacity, "%s", local->interaction.feedback);
        return false;
    }
    CcInteractionTarget target = {0};
    if (!RoadCarriageTarget(sim, local, &target) ||
        !RoadCarriageRevisionMatches(sim, local)) {
        CcInteractionCancel(&local->interaction,
                            "The carriage moved. Choose it again.");
        CcLocalAgentStop(&local->agent);
        (void)snprintf(message, capacity, "%s", local->interaction.feedback);
        return true;
    }
    CcInteractionPlan plan = {.targets = {target}, .count = 1};
    if (CcInteractionAdvance(&local->interaction, &plan,
                             local->agent.position.x, local->agent.position.z,
                             delta_time)) {
        float dx = local->agent.position.x - local->world_carriage.position.x;
        float dz = local->agent.position.z - local->world_carriage.position.z;
        if (dx * dx + dz * dz > 4.8f * 4.8f ||
            !RoadCarriageRevisionMatches(sim, local)) {
            (void)snprintf(message, capacity,
                           "The carriage is out of reach. Choose it again.");
            return true;
        }
        local->world_carriage.hero_embarked = true;
        local->world_carriage.camera_target = 0.0f;
        local->carriage_stopped = false;
        CcLocalAgentStop(&local->agent);
        (void)snprintf(message, capacity,
                       "The company boards and the road goes on.");
        return true;
    }
    if (local->interaction.repath_seconds <= 0.0f &&
        !local->agent.target_valid) {
        (void)CcLocalAgentApproachInteraction(&local->agent,
            (Vector2){target.approach_x, target.approach_z},
            target.radius * 0.9f, false);
        local->interaction.repath_seconds = 0.75f;
    }
    (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                             delta_time, false, true);
    return true;
}

static void BuildWorldActionTargets(const CcSim *sim, LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation, RenderTexture2D texture, Rectangle destination)
{
    local->interactions.count = 0;
    ContextActionSet actions = BuildContextActions(sim, local, view, selected, selected_situation);
    Vector3 carriage = WorldActionCarriagePosition(local);
    int32_t branch = 0;
    for (int32_t i = 0; i < actions.count; ++i) {
        const ContextAction *action = &actions.items[i];
        if (action->kind == CONTEXT_ACTION_BOARD_CARRIAGE ||
            action->kind == CONTEXT_ACTION_INSPECT_CARRIAGE) {
            CcInteractionTarget road_target = {0};
            if (!RoadCarriageTarget(sim, local, &road_target)) continue;
            road_target.key = (CcInteractionKey){sim->player.location_id,
                ((uint64_t)action->kind << 32) | (uint32_t)action->amount,
                CC_INTERACTION_ACTION};
            road_target.action = (int32_t)action->kind;
            road_target.amount = action->amount;
            (void)snprintf(road_target.name, sizeof(road_target.name), "%s",
                           action->label);
            (void)snprintf(road_target.verb, sizeof(road_target.verb), "%s",
                           action->label);
            if (local->interactions.count >= CC_INTERACTION_CAPACITY) break;
            CcInteractionTarget *target =
                &local->interactions.targets[local->interactions.count++];
            *target = road_target;
            CcLocalAgent camera_subject = local->agent;
            camera_subject.scene = CC_LOCAL_SCENE_STREET;
            CcLocalProjectInteraction(&camera_subject, texture, destination,
                                      target);
            continue;
        }
        Vector3 position = carriage;
        float width = 3.4f, height = 3.4f, depth = 4.8f;
        bool project = true;
        switch (action->kind) {
            case CONTEXT_ACTION_TRAVEL: {
                const CcRoute *route = SelectedOutgoingRoute(sim, action->amount);
                if (route == NULL) continue;
                if (local->open_world) {
                    const CcWorldRoutePlacement *placement = CcWorldRoutePlacementForId(
                        &local->world_stream.manifest, route->id);
                    CcWorldPoint point;
                    float heading;
                    if (placement == NULL || !CcWorldRoutePose(placement, sim->player.location_id,
                        CcWorldRouteJourneyAmount(placement, sim->player.location_id, 0.08f), &point, &heading)) continue;
                    position = (Vector3){point.x,
                        CcWorldTerrainHeight(&local->world_stream.manifest, point.x, point.z), point.z};
                } else {
                    Vector2 end = CcLocalForkBranchEndInternal(branch, OutgoingRouteCount(sim));
                    position = (Vector3){50.5f + (end.x - 50.5f) * 0.32f, 0,
                        40.0f + (end.y - 40.0f) * 0.32f};
                }
                ++branch;
                width = 4.5f; depth = 4.5f;
                break;
            }
            case CONTEXT_ACTION_HOLD_TRAVEL: break;
            case CONTEXT_ACTION_INSPECT_CARRIAGE: break;
            case CONTEXT_ACTION_BOARD_CARRIAGE: break;
            case CONTEXT_ACTION_CLOSE_VIEW:
                if (view != VIEW_ROADS) project = false;
                if (!local->open_world) position = (Vector3){29, 0, 41};
                break;
            case CONTEXT_ACTION_SKIP_TRAVEL:
            case CONTEXT_ACTION_PASS_ROAD_SITE:
            case CONTEXT_ACTION_PRESS_ON:
                position = WorldActionAlongRoad(local, carriage, 10, 0);
                width = 5; height = 1; depth = 5;
                break;
            case CONTEXT_ACTION_TRANSFER_ROAD_SITE:
            case CONTEXT_ACTION_REPAIR_ROAD_SITE:
            case CONTEXT_ACTION_CLEAR_ROAD_SITE:
            case CONTEXT_ACTION_CAMP_ROAD_SITE: {
                position = WorldActionAlongRoad(local, carriage, 2, 7);
                const CcRoadSite *stop = CcSimJourneyRoadSiteStop(sim);
                if (local->open_world && stop != NULL) {
                    for (int32_t site = 0; site < local->world_stream.manifest.road_site_count; ++site) {
                        const CcWorldRoadSitePlacement *placement = &local->world_stream.manifest.road_sites[site];
                        if (placement->road_site_id != stop->id) continue;
                        position = (Vector3){placement->blocker_position.x,
                            CcWorldTerrainHeight(&local->world_stream.manifest, placement->blocker_position.x,
                                placement->blocker_position.z), placement->blocker_position.z};
                        break;
                    }
                }
                width = 5; depth = 5;
                break;
            }
            case CONTEXT_ACTION_MAKE_CAMP:
            case CONTEXT_ACTION_TAKE_BREAK:
                position = WorldActionAlongRoad(local, carriage, 0, 7);
                break;
            case CONTEXT_ACTION_LODGE_ROAD_HOUSE:
                position = local->open_world ? WorldActionAlongRoad(local, carriage, 8, -7) :
                    (Vector3){54, 0, 31};
                break;
            case CONTEXT_ACTION_APPROACH_ENTRANCE:
            case CONTEXT_ACTION_EXPEDITION:
            case CONTEXT_ACTION_TRAVEL_DRAGON_SITE:
                position = (Vector3){CC_LOCAL_SITE_ENTRANCE_X, 0, CC_LOCAL_SITE_ENTRANCE_Z};
                width = 5; height = 4;
                break;
            case CONTEXT_ACTION_RETURN_FROM_SITE:
            case CONTEXT_ACTION_TRAVEL_GOBLIN_SITE: break;
            default: project = false; break;
        }
        if (!project) continue;
        CcInteractionTarget *target = AdventureTarget(local, sim, CC_INTERACTION_ACTION,
            ((uint64_t)action->kind << 32) | (uint32_t)action->amount,
            action->label, action->label, position, width, height, depth,
            (Vector2){position.x, position.z}, 1.0f);
        if (target == NULL) break;
        target->subject_id = action->target.object;
        target->action = (int32_t)action->kind;
        target->amount = action->amount;
        target->available = action->enabled;
        CcLocalAgent camera_subject = local->agent;
        camera_subject.scene = local->open_world ? CC_LOCAL_SCENE_STREET :
            view == VIEW_ROADS || local->site_kind != CC_LOCAL_SITE_NONE ||
            local->convoy.phase == CC_LOCAL_CONVOY_ROAD ? CC_LOCAL_SCENE_ROAD : CC_LOCAL_SCENE_STREET;
        CcLocalProjectInteraction(&camera_subject, texture, destination, target);
    }
}
