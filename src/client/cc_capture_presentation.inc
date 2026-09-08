/* Capture-only choices at the window and rendering boundaries. */
typedef struct CcCaptureWindow {
    int32_t width, height;
    int32_t minimum_width, minimum_height;
    int32_t fps;
} CcCaptureWindow;

typedef struct CcCapturePresentation {
    bool adventure_ui, accept_input, update_convoy, local_panels;
    bool combat_status, feedback, save_feedback, context_actions, commands, navigation;
    float message_step;
} CcCapturePresentation;

static CcCapturePresentation CcCapturePresentationFor(
    const CcCaptureRequest *request, const CcCaptureState *state, bool normal_play)
{
    CcCapturePresentation result = {
        .adventure_ui = normal_play, .navigation = normal_play,
        .accept_input = true, .update_convoy = true, .local_panels = true,
        .combat_status = true, .feedback = true, .save_feedback = true,
        .context_actions = true, .commands = true,
    };
#if defined(CC_CLIENT_SELF_TESTS)
    result.adventure_ui |= request->capture_ux || request->capture_road_fork ||
        request->capture_road || request->capture_parley;
    result.navigation |= request->capture_ux;
    result.accept_input = !(request->capture_ux || request->capture_road_fork);
    result.update_convoy = !(request->capture_road_arrival || request->capture_storybook);
    result.local_panels = !(request->capture_npc_review || request->capture_heraldry);
    result.combat_status = !request->capture_gameplay_reel;
    result.feedback = !(request->capture_gameplay_reel || request->capture_road_departure);
    result.save_feedback = !(request->capture_gameplay_reel || request->capture_npc_review);
    result.context_actions = !request->capture_npc_review &&
        (!request->capture_gameplay_reel || state->gameplay_reel.stage != GAMEPLAY_REEL_QUEST_COMPLETE);
    result.commands = !request->capture_npc_review;
    result.message_step = request->capture_gameplay_reel ? 1.0f / 15.0f : 0.0f;
#else
    (void)request;
    (void)state;
#endif
    return result;
}

#if defined(CC_CLIENT_SELF_TESTS)
static bool CcCaptureActive(const CcCaptureRequest *request) { return request->capture; }

static bool CcCaptureConfigureWindow(const CcCaptureRequest *request,
    int argc, char **argv, CcCaptureWindow *window)
{
    if ((request->capture_road_fork || request->capture_pony_book) && argc >= 4) {
        char *end = NULL;
        long width = strtol(argv[3], &end, 10);
        if (*end != '\0' || (width != 1040 && width != 1200 && width != 1280)) {
            (void)fprintf(stderr, "Choose capture width 1040, 1200, or 1280.\n");
            return false;
        }
        window->width = (int32_t)width;
        window->height = request->capture_pony_book && window->width == 1040 ? 620 : 700;
    }
    if (request->capture_ux && argc >= 5) {
        char *end = NULL;
        long width = strtol(argv[4], &end, 10);
        int32_t height;
        if (*end != '\0') return false;
        switch (width) {
            case 1040: height = 620; break;
            case 1200: height = 700; break;
            case 1280: height = 720; break;
            case 1600: height = 900; break;
            case 1920: height = 1080; break;
            default: return false;
        }
        window->width = (int32_t)width;
        window->height = height;
    }
    if (request->capture_road_fork || request->capture_pony_book || request->capture_ux) {
        window->minimum_width = 1040;
        window->minimum_height = 620;
    }
    if (request->capture_action_reel || request->capture_gameplay_reel || request->capture_creature_reel)
        window->fps = 0;
    return true;
}

static void CcCaptureConfigureRenderer(const CcCaptureRequest *request)
{
    CcLocalRendererSetDiagnosticOverlay(request->capture_limbs);
}

static void CcCaptureSelectStart(const CcCaptureRequest *request, int32_t *selected, ClientView *view)
{
    if (request->capture_dragon_hoard_map) *selected = CC_MAP_DRAGON_HOARD;
    else if (request->capture_map_case) *selected = CC_MAP_GLOAMGATE_NIGHT_ROAD;
    *view = request->capture_board ? VIEW_SITUATIONS : request->capture_character ? VIEW_CHARACTER :
        request->capture_encounter ? VIEW_ENCOUNTER : request->capture_underroad ? VIEW_DUNGEON :
        request->capture_dragon_cave ? VIEW_DRAGON_CAVE : request->capture_road_fork ? VIEW_ROADS :
        (request->capture_map_case || request->capture_dragon_hoard_map) ? VIEW_MAP :
        request->capture_carriage ? VIEW_CARRIAGE : VIEW_LOCAL;
}

static bool CcCaptureNeedsRoadbook(const CcCaptureRequest *request)
{
    return request->capture_world || request->capture_travel || request->capture_route_sight ||
        request->capture_road_fork || request->capture_road_zoom;
}

static bool CcCaptureJourneyReady(const CcCaptureRequest *request, const CcSim *sim,
                                  const LocalState *local, bool fallback)
{
    if (request->capture_road_arrival) return !sim->journey.active && local->journey_travel_active &&
        local->arrival.phase == CC_CLIENT_ARRIVAL_ROAD_BOOK;
    if (request->capture_travel || request->capture_route_sight)
        return sim->journey.active && sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING &&
            local->journey_travel_active;
    return fallback;
}

static CcLocalAtmospherePreset CcCaptureAtmosphere(const CcCaptureRequest *request, const CcSim *sim)
{
    return request->capture_atmosphere ? request->capture_atmosphere_preset : LocalAtmosphereForSimulation(sim);
}

static void CcCaptureConfigureFrontend(const CcCaptureRequest *request, FrontendState *frontend)
{
    if (request->capture_title) frontend->focus = 2;
    if (!frontend->online) {
        if (request->capture_title) frontend->screen = FRONTEND_TITLE;
        else if (request->capture_delete) frontend->screen = FRONTEND_DELETE;
        else if (request->capture_ux && request->capture_ux_view == 10) frontend->screen = FRONTEND_SOUND;
        else if (request->capture_menu || request->capture_mine_menu ||
                 (request->capture_ux && request->capture_ux_view == 5)) frontend->screen = FRONTEND_PAUSED;
    }
    frontend->has_world |= request->capture_menu || request->capture_delete || request->capture_mine_menu;
}

static bool CcCaptureDrawScene(const CcCaptureRequest *request, const CcSim *sim, float clock,
                              RenderTexture2D local_target, Rectangle local_bounds)
{
    if (request->capture_heraldry) {
        CcLocalDrawHeraldryReview3D(
            sim, clock, local_target, local_bounds);
    } else if (request->capture_npc_review) {
        CcLocalDrawNpcReview3D(request->capture_npc_review_view, clock,
                               local_target, local_bounds);
    }
    return request->capture_heraldry || request->capture_npc_review;
}
#else
#define CcCaptureActive(...) false
#define CcCaptureConfigureWindow(...) true
#define CcCaptureConfigureRenderer(...) ((void)0)
#define CcCaptureSelectStart(...) ((void)0)
#define CcCaptureNeedsRoadbook(...) false
#define CcCaptureJourneyReady(request, sim, local, fallback) (fallback)
#define CcCaptureAtmosphere(request, sim) LocalAtmosphereForSimulation(sim)
#define CcCaptureConfigureFrontend(...) ((void)0)
#define CcCaptureDrawScene(...) false
#endif
