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
        if (action->enabled && (int32_t)action->kind == target->action &&
            action->amount == target->amount &&
            (action->target.kind == CC_INTERACTION_NONE ||
             CcInteractionKeyEqual(action->target, target->key))) return *action;
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

static void BuildWorldActionTargets(const CcSim *sim, LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation, RenderTexture2D texture, Rectangle destination)
{
    local->interactions.count = 0;
    ContextActionSet actions = BuildContextActions(sim, local, view, selected, selected_situation);
    Vector3 carriage = WorldActionCarriagePosition(local);
    int32_t branch = 0;
    for (int32_t i = 0; i < actions.count; ++i) {
        const ContextAction *action = &actions.items[i];
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
            case CONTEXT_ACTION_TOGGLE_DRIVE: break;
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
        if (action->target.kind != CC_INTERACTION_NONE) target->key = action->target;
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
