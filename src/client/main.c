#include "client/cc_local3d.h"
#include "client/cc_overlay.h"
#include "client/cc_visual_style.h"
#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "raylib.h"
#include "GLFW/glfw3.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKGROUND CC_STYLE_BACKGROUND
#define PANEL CC_STYLE_PANEL
#define PANEL_DEEP CC_STYLE_PANEL_DEEP
#define PANEL_HOVER CC_STYLE_PANEL_HOVER
#define BAR_TRACK CC_STYLE_BAR_TRACK
#define INK CC_STYLE_INK
#define MUTED CC_STYLE_MUTED
#define TEAL CC_STYLE_TEAL
#define CC_GOLD CC_STYLE_GOLD
#define DANGER CC_STYLE_DANGER
#define CC_VIOLET CC_STYLE_VIOLET
#define CC_GLOAMGATE_ALDERWATCH_MAP_ASSET \
    "assets/maps/gloamgate_to_alderwatch.png"
#define CC_COLLECTIBLE_MAP_ATLAS_ASSET \
    "assets/maps/collectible_map_atlas.png"
#define CC_MAP_LIST_ROWS 8

typedef enum ClientView {
    VIEW_LOCAL,
    VIEW_ROADS,
    VIEW_MAP,
    VIEW_LEDGER,
    VIEW_SITUATIONS,
    VIEW_ENCOUNTER
} ClientView;

typedef struct LocalState {
    CcLocalAgent agent;
    CcLocalCourse course;
    Vector2 movement_reticle;
    float movement_reticle_age;
    bool market_interior;
    bool journey_travel_active;
    bool journey_combat_active;
    bool journey_parley_active;
    bool movement_reticle_valid;
    bool movement_reticle_accepted;
} LocalState;

typedef struct ActionReelState {
    int32_t stage;
    int32_t stage_frame;
    int32_t captured_frames;
    bool jump_started;
    bool complete;
} ActionReelState;

typedef enum GameplayReelStage {
    GAMEPLAY_REEL_WALK_TO_NOTICE = 0,
    GAMEPLAY_REEL_CHOOSE_PROMISE,
    GAMEPLAY_REEL_WALK_TO_MARKET,
    GAMEPLAY_REEL_BUY_CARGO,
    GAMEPLAY_REEL_LEAVE_MARKET,
    GAMEPLAY_REEL_WALK_TO_CARRIAGE,
    GAMEPLAY_REEL_CHOOSE_ROUTE,
    GAMEPLAY_REEL_TRAVEL,
    GAMEPLAY_REEL_ROAD_CHOICE,
    GAMEPLAY_REEL_ROAD_COMBAT,
    GAMEPLAY_REEL_RESUME_TRAVEL,
    GAMEPLAY_REEL_ARRIVE_AT_MARKET,
    GAMEPLAY_REEL_QUEST_COMPLETE,
    GAMEPLAY_REEL_VILLAGE_ALARM,
    GAMEPLAY_REEL_VILLAGE_COMBAT
} GameplayReelStage;

typedef struct GameplayReelState {
    GameplayReelStage stage;
    int32_t stage_frame;
    int32_t captured_frames;
    int32_t journey_legs;
    CcId destination_id;
    bool stage_started;
    bool complete;
} GameplayReelState;

typedef enum ContextActionKind {
    CONTEXT_ACTION_NONE = 0,
    CONTEXT_ACTION_ENTER_MARKET,
    CONTEXT_ACTION_LEAVE_MARKET,
    CONTEXT_ACTION_CHOOSE_ROAD,
    CONTEXT_ACTION_OPEN_MAP,
    CONTEXT_ACTION_OPEN_PROMISES,
    CONTEXT_ACTION_EXPEDITION,
    CONTEXT_ACTION_BUY_CARGO,
    CONTEXT_ACTION_SELL_CARGO,
    CONTEXT_ACTION_DELIVER_CARGO,
    CONTEXT_ACTION_ACCEPT_PROMISE,
    CONTEXT_ACTION_ABANDON_PROMISE,
    CONTEXT_ACTION_NEXT_PROMISE,
    CONTEXT_ACTION_CLOSE_VIEW,
    CONTEXT_ACTION_FIGHT,
    CONTEXT_ACTION_PAY,
    CONTEXT_ACTION_TRAVEL,
    CONTEXT_ACTION_NEXT_BRANCH,
    CONTEXT_ACTION_BUY_MAP,
    CONTEXT_ACTION_REPAIR_ROUTE,
    CONTEXT_ACTION_PAY_COLLECTOR,
    CONTEXT_ACTION_OFFER_PROVISIONS,
    CONTEXT_ACTION_RETURN_TO_CHOICE,
    CONTEXT_ACTION_JUMP,
    CONTEXT_ACTION_RAISE_ALARM,
    CONTEXT_ACTION_BASIC_STRIKE,
    CONTEXT_ACTION_TOGGLE_GUARD,
    CONTEXT_ACTION_WITHDRAW,
    CONTEXT_ACTION_SKILL_CRUSHING,
    CONTEXT_ACTION_SKILL_SUNDER,
    CONTEXT_ACTION_SKILL_SECOND_WIND
} ContextActionKind;

typedef struct ContextAction {
    ContextActionKind kind;
    char label[64];
    char key_hint[16];
    char detail[48];
    bool enabled;
    bool active;
} ContextAction;

typedef struct ContextActionSet {
    ContextAction items[6];
    int32_t count;
} ContextActionSet;

typedef enum CommandActionKind {
    COMMAND_ACTION_NONE = 0,
    COMMAND_ACTION_QUESTS,
    COMMAND_ACTION_LEDGER,
    COMMAND_ACTION_MAP,
    COMMAND_ACTION_SAVE,
    COMMAND_ACTION_COUNT
} CommandActionKind;

/* Accessibility tools can post a complete press/release pair between two
   raylib frames. Keep the GLFW press edge so fast synthetic input is handled
   on the next gameplay update instead of disappearing between polls. */
static GLFWkeyfun previous_key_callback = NULL;
static GLFWmousebuttonfun previous_mouse_button_callback = NULL;
static bool queued_key_press[GLFW_KEY_LAST + 1] = {0};
static bool queued_mouse_button_press[GLFW_MOUSE_BUTTON_LAST + 1] = {0};
static bool queued_save_shortcut = false;

static void ClientKeyCallback(GLFWwindow *window, int key, int scancode,
                              int action, int mods)
{
    if (previous_key_callback != NULL) {
        previous_key_callback(window, key, scancode, action, mods);
    }
    if (action != GLFW_PRESS) return;
    if (key >= 0 && key <= GLFW_KEY_LAST) queued_key_press[key] = true;
    if (key == GLFW_KEY_S &&
        (mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER)) != 0) {
        queued_save_shortcut = true;
    }
}

static void ClientMouseButtonCallback(GLFWwindow *window, int button,
                                      int action, int mods)
{
    if (previous_mouse_button_callback != NULL) {
        previous_mouse_button_callback(window, button, action, mods);
    }
    if (action == GLFW_PRESS && button >= 0 &&
        button <= GLFW_MOUSE_BUTTON_LAST) {
        queued_mouse_button_press[button] = true;
    }
}

static void ClientInputInstall(void)
{
    GLFWwindow *window = glfwGetCurrentContext();
    if (window == NULL) return;
    previous_key_callback = glfwSetKeyCallback(window, ClientKeyCallback);
    previous_mouse_button_callback = glfwSetMouseButtonCallback(
        window, ClientMouseButtonCallback);
}

static bool ClientKeyPressed(int32_t key)
{
    bool queued = key >= 0 && key <= GLFW_KEY_LAST && queued_key_press[key];
    return IsKeyPressed(key) || queued;
}

static bool ClientMouseButtonPressed(int32_t button)
{
    bool queued = button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST &&
                  queued_mouse_button_press[button];
    return IsMouseButtonPressed(button) || queued;
}

static void ClientInputClearPressed(void)
{
    (void)memset(queued_key_press, 0, sizeof(queued_key_press));
    (void)memset(queued_mouse_button_press, 0,
                 sizeof(queued_mouse_button_press));
    queued_save_shortcut = false;
}

static const Vector2 LOCAL_MARKET = {CC_LOCAL_MARKET_X, CC_LOCAL_MARKET_Z};
static const Vector2 LOCAL_CARRIAGE = {CC_LOCAL_CARRIAGE_X,
                                      CC_LOCAL_CARRIAGE_Z};
static const Vector2 LOCAL_NOTICE = {CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z};
static const Vector2 LOCAL_DUNGEON = {CC_LOCAL_DUNGEON_X,
                                     CC_LOCAL_DUNGEON_Z};
static const Vector2 INTERIOR_COUNTER = {6.65f, 2.50f};
static const Vector2 INTERIOR_EXIT = {1.55f, 5.55f};

static void CampaignSavePath(char *path, size_t capacity)
{
#if defined(__APPLE__)
    const char *user_home = getenv("HOME");
    if (user_home != NULL && user_home[0] != '\0') {
        char directory[512];
        (void)snprintf(directory, sizeof(directory),
                       "%s/Library/Application Support/Crownless Carriage", user_home);
        if (!DirectoryExists(directory)) (void)MakeDirectory(directory);
        (void)snprintf(path, capacity, "%s/campaign.ccsave", directory);
        return;
    }
#endif
    (void)snprintf(path, capacity, "crownless_campaign.ccsave");
}

static bool ResolveClientAssetPath(const char *relative_path, char *resolved,
                                   size_t capacity)
{
    if (relative_path == NULL || resolved == NULL || capacity == 0U) {
        return false;
    }
    if (FileExists(relative_path)) {
        (void)snprintf(resolved, capacity, "%s", relative_path);
        return true;
    }
#if defined(CC_ASSET_SOURCE_ROOT)
    (void)snprintf(resolved, capacity, "%s/%s", CC_ASSET_SOURCE_ROOT,
                   relative_path);
    if (FileExists(resolved)) return true;
#endif
    (void)snprintf(resolved, capacity, "../%s", relative_path);
    if (FileExists(resolved)) return true;
    (void)snprintf(resolved, capacity, "%s/../Resources/%s",
                   GetApplicationDirectory(), relative_path);
    return FileExists(resolved);
}

static bool IsGloamgateAlderwatchMap(const CcMap *map)
{
    return map != NULL &&
           strcmp(map->name, CC_GLOAMGATE_ALDERWATCH_MAP_NAME) == 0;
}

static void SituationTargetLabel(const CcSim *sim, const CcSituation *situation,
                                 char *label, size_t capacity)
{
    if (situation == NULL || label == NULL || capacity == 0U) return;
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        const CcSettlement *place = CcSimSettlement(sim, situation->target_id);
        (void)snprintf(label, capacity, "%s", place != NULL ? place->name : "unknown town");
        return;
    }
    if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, situation->target_id);
        const CcSettlement *from = route != NULL ? CcSimSettlement(sim, route->from_id) : NULL;
        const CcSettlement *to = route != NULL ? CcSimSettlement(sim, route->to_id) : NULL;
        (void)snprintf(label, capacity, "%s - %s",
                       from != NULL ? from->name : "unknown",
                       to != NULL ? to->name : "unknown");
        return;
    }
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].id == situation->target_id) {
            (void)snprintf(label, capacity, "%s", sim->dungeons[i].name);
            return;
        }
    }
    (void)snprintf(label, capacity, "unknown target");
}

static int32_t FirstActiveSituationIndex(const CcSim *sim)
{
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) return i;
    }
    return -1;
}

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

static int32_t StepActiveSituationIndex(const CcSim *sim, int32_t selected,
                                        int32_t direction)
{
    if (sim == NULL || sim->situation_count <= 0) return -1;
    int32_t index = selected;
    for (int32_t step = 0; step < sim->situation_count; ++step) {
        index += direction < 0 ? -1 : 1;
        if (index < 0) index = sim->situation_count - 1;
        if (index >= sim->situation_count) index = 0;
        if (sim->situations[index].status == CC_SITUATION_ACTIVE) return index;
    }
    return -1;
}

static const CcSituation *SelectedActiveSituation(const CcSim *sim,
                                                  int32_t selected)
{
    if (sim == NULL || selected < 0 || selected >= sim->situation_count ||
        sim->situations[selected].status != CC_SITUATION_ACTIVE) {
        return NULL;
    }
    return &sim->situations[selected];
}

static CcId SituationSettlementId(const CcSim *sim,
                                  const CcSituation *situation)
{
    if (sim == NULL || situation == NULL) return 0U;
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        return situation->target_id;
    }
    if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, situation->target_id);
        if (route == NULL) return 0U;
        return sim->player.location_id == route->from_id ||
               sim->player.location_id == route->to_id ?
               sim->player.location_id : route->from_id;
    }
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].id == situation->target_id) {
            return sim->dungeons[i].settlement_id;
        }
    }
    return 0U;
}

static void SituationNextAction(const CcSim *sim,
                                const CcSituation *situation,
                                char *label, size_t capacity)
{
    if (sim == NULL || situation == NULL || label == NULL || capacity == 0U) {
        return;
    }
    CcId destination_id = SituationSettlementId(sim, situation);
    const CcSettlement *destination = CcSimSettlement(sim, destination_id);
    bool here = destination_id != 0U &&
                sim->player.location_id == destination_id;
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        int32_t remaining = situation->quantity - situation->progress;
        if (here && sim->player.cargo[situation->good] > 0) {
            (void)snprintf(label, capacity,
                           "Deliver %d %s at the market.",
                           remaining, CcGoodName(situation->good));
        } else if (here) {
            (void)snprintf(label, capacity,
                           "Bring %d %s here.",
                           remaining, CcGoodName(situation->good));
        } else if (sim->player.cargo[situation->good] >= remaining) {
            (void)snprintf(label, capacity,
                           "Travel to %s.",
                           destination != NULL ? destination->name : "the target");
        } else {
            (void)snprintf(label, capacity,
                           "Buy %d %s, then travel to %s.",
                           remaining, CcGoodName(situation->good),
                           destination != NULL ? destination->name : "the target");
        }
        return;
    }
    if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, situation->target_id);
        bool at_route = route != NULL &&
            (sim->player.location_id == route->from_id ||
             sim->player.location_id == route->to_id);
        (void)snprintf(label, capacity, at_route ?
                       "Repair this road: 3 Tools or 18 crowns." :
                       "Travel to either end of this road.");
        return;
    }
    if (here) {
        (void)snprintf(label, capacity, "Enter the mine.");
    } else {
        (void)snprintf(label, capacity,
                       "Travel to %s, then enter the mine.",
                       destination != NULL ? destination->name : "the town");
    }
}

static void DrawPanel(Rectangle bounds, Color color)
{
    DrawRectangleRounded(bounds, 0.025f, 4, color);
    DrawRectangleRoundedLinesEx(bounds, 0.025f, 4, 1.0f,
                                Fade(CC_GOLD, 0.62f));
    DrawLine((int)bounds.x + 11, (int)bounds.y + 8,
             (int)bounds.x + 45, (int)bounds.y + 8,
             Fade(CC_GOLD, 0.50f));
}

static void DrawPerformanceOverlay(void)
{
    CcLocalRendererStats stats = CcLocalRendererGetStats();
    float fps = stats.smoothed_frame_milliseconds > 0.001f ?
        1000.0f / stats.smoothed_frame_milliseconds : 0.0f;
    Rectangle bounds = {(float)GetScreenWidth() - 294.0f, 18.0f,
                        276.0f, 92.0f};
    DrawPanel(bounds, PANEL_DEEP);
    CcOverlayDrawText("LOCAL PERFORMANCE", (int)bounds.x + 14,
             (int)bounds.y + 11, 12, TEAL);
    CcOverlayDrawText(TextFormat("frame %5.2f ms  %5.1f fps",
                        stats.smoothed_frame_milliseconds, fps),
             (int)bounds.x + 14, (int)bounds.y + 32, 11, INK);
    CcOverlayDrawText(TextFormat("skin %d update  %d mesh uploads",
                        stats.skin_updates, stats.skinned_meshes),
             (int)bounds.x + 14, (int)bounds.y + 50, 11, INK);
    CcOverlayDrawText(TextFormat("bipeds %d  hero %d  lod %d",
                        stats.biomechanical_characters,
                        stats.high_detail_characters,
                        stats.low_detail_characters),
             (int)bounds.x + 14, (int)bounds.y + 68, 11, MUTED);
}

static void DrawTwoLineText(const char *text, int x, int y,
                            size_t line_capacity, int font_size, Color color)
{
    if (text == NULL || line_capacity < 2U) return;
    size_t length = strlen(text);
    size_t split = length > line_capacity ? line_capacity : length;
    while (split > 0U && split < length && text[split] != ' ') split -= 1U;
    if (split == 0U) split = length > line_capacity ? line_capacity : length;
    CcOverlayDrawText(TextFormat("%.*s", (int)split, text), x, y, font_size, color);
    if (split < length) {
        size_t next = text[split] == ' ' ? split + 1U : split;
        CcOverlayDrawText(text + next, x, y + font_size + 5, font_size, color);
    }
}

static void DrawBar(int x, int y, int width, const char *label,
                    int32_t value, Color color)
{
    CcOverlayDrawText(label, x, y, 11, MUTED);
    DrawRectangle(x + 76, y + 1, width, 10, BAR_TRACK);
    DrawRectangleLinesEx((Rectangle){(float)x + 76.0f, (float)y + 1.0f,
                                     (float)width, 10.0f},
                         1.0f, MUTED);
    int fill = (int)((float)width * (float)value / 100.0f);
    DrawRectangle(x + 76, y + 1, fill, 10, color);
    CcOverlayDrawText(TextFormat("%d", value), x + 81 + width, y - 2, 12, INK);
}

static Color SituationColor(CcSituationKind kind);
static int32_t SelectedCombatTargetIndex(const LocalState *local);
static const CcLocalAgent *SelectedCombatTarget(const LocalState *local);

static float GridDistance(Vector2 a, Vector2 b)
{
    float x = a.x - b.x;
    float y = a.y - b.y;
    return sqrtf(x * x + y * y);
}

static void ResetLocalState(LocalState *local)
{
    local->movement_reticle = (Vector2){0};
    local->movement_reticle_age = 0.0f;
    local->movement_reticle_valid = false;
    local->movement_reticle_accepted = false;
    local->market_interior = false;
    local->journey_travel_active = false;
    local->journey_combat_active = false;
    local->journey_parley_active = false;
    CcLocalAgentInit(&local->agent,
                     (Vector2){CC_LOCAL_START_X, CC_LOCAL_START_Z}, false);
    CcLocalCombatSetTeam(&local->agent, CC_COMBAT_PLAYER);
    CcLocalCourseInit(&local->course);
}

static void RepositionHero(LocalState *local, Vector2 position,
                           bool market_interior)
{
    CcAthleticProfile athletics = local->agent.athletics;
    CcLocalAgentInit(&local->agent, position, market_interior);
    local->agent.athletics = athletics;
    CcLocalCombatSetTeam(&local->agent, CC_COMBAT_PLAYER);
}

static void BeginRoadLocalState(const CcSim *sim, LocalState *local,
                                bool hostile)
{
    CcAthleticProfile athletics = local->agent.athletics;
    ResetLocalState(local);
    local->agent.athletics = athletics;
    RepositionHero(local,
                   (Vector2){CC_LOCAL_ROAD_START_X,
                             CC_LOCAL_ROAD_START_Z}, false);
    CcLocalCourseStageRoadEncounter(&local->course, &local->agent,
                                    hostile);
    CcLocalCourseBindRaiderCompany(&local->course, sim);
    local->journey_combat_active = hostile;
    local->journey_parley_active = !hostile;
}

static void BeginRoadTravelState(LocalState *local)
{
    CcAthleticProfile athletics = local->agent.athletics;
    ResetLocalState(local);
    local->agent.athletics = athletics;
    RepositionHero(local,
                   (Vector2){CC_LOCAL_ROAD_START_X,
                             CC_LOCAL_ROAD_START_Z}, false);
    CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_ROAD);
    local->course.scene = CC_LOCAL_SCENE_ROAD;
    local->course.alarm_countdown = 1000.0f;
    local->journey_travel_active = true;
}

static void ActionReelSetStage(ActionReelState *reel, int32_t stage)
{
    reel->stage = stage;
    reel->stage_frame = 0;
}

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
            /* The reel is scripted, but it should exercise the same explicit
               target contract as live play. A focus point alone must never
               be enough to move the camera. */
            hero->combat.target_index =
                opponent->combat.life_state == CC_LIFE_ALIVE ? 0 : -1;
            if (reel->stage_frame > 390) reel->complete = true;
            break;
        default:
            reel->complete = true;
            break;
    }
}

static Vector2 LocalPosition(const LocalState *local)
{
    return CcLocalAgentPosition(&local->agent);
}

static const CcDungeon *DungeonAtSettlement(const CcSim *sim, CcId settlement_id)
{
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].settlement_id == settlement_id) return &sim->dungeons[i];
    }
    return NULL;
}


static void DrawLocalHeader(const CcSim *sim, const LocalState *local)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    bool road = local->journey_travel_active ||
                local->journey_combat_active ||
                local->journey_parley_active;
    const CcSettlement *origin = road ?
        CcSimSettlement(sim, sim->journey.origin_id) : NULL;
    const CcSettlement *destination = road ?
        CcSimSettlement(sim, sim->journey.destination_id) : NULL;
    CcOverlayDrawText(road ?
             TextFormat("%s  ->  %s",
                        origin != NULL ? origin->name : "Road",
                        destination != NULL ? destination->name : "Gate") :
             local->market_interior && place != NULL ?
                 TextFormat("%s  /  Market", place->name) :
             place != NULL ? place->name : "Crownless",
             22, 18, 18, INK);
    const char *summary = TextFormat(
        "DAY %d     %" PRId64 " cr     CARGO %d/%d",
        sim->current_day, sim->player.coins,
        CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity);
    int summary_width = CcOverlayMeasureText(summary, 10);
    CcOverlayDrawText(summary, GetScreenWidth() - summary_width - 22,
                      22, 10, road ? TEAL : CC_GOLD);
}

static bool LocalCombatActive(const LocalState *local)
{
    return local != NULL &&
           (local->journey_combat_active ||
            (!local->market_interior && local->course.alarm_active));
}

static int32_t StandingRaiders(const LocalState *local)
{
    int32_t standing = 0;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (local->course.raiders[i].combat.life_state == CC_LIFE_ALIVE) {
            standing += 1;
        }
    }
    return standing;
}

static void DrawCombatMeter(int x, int y, int width, const char *label,
                            float value, Color color, bool enabled)
{
    float clamped = fmaxf(0.0f, fminf(100.0f, value));
    Color ink = enabled ? INK : Fade(MUTED, 0.52f);
    Color meter = enabled ? color : Fade(MUTED, 0.34f);
    CcOverlayDrawText(label, x, y, 7, Fade(ink, 0.88f));
    const char *number = TextFormat("%d", (int32_t)lroundf(clamped));
    int number_width = CcOverlayMeasureText(number, 8);
    CcOverlayDrawText(number, x + width - number_width, y - 1, 8, ink);
    DrawRectangle(x, y + 12, width, 7, Fade(BAR_TRACK, 0.94f));
    DrawRectangle(x, y + 12,
                  (int)lroundf((float)width * clamped / 100.0f), 7, meter);
}

static const char *CombatResolveLabel(const LocalState *local)
{
    if (local->course.raiders_retreating) return "BREAKING";
    if (local->course.raider_resolve <= 25) return "WAVERING";
    if (local->course.raider_resolve <= 55) return "PRESSURED";
    return "HOLDING";
}

static Color CombatTeamColor(CcCombatTeam team)
{
    switch (team) {
        case CC_COMBAT_PLAYER: return TEAL;
        case CC_COMBAT_GUARD: return CC_GOLD;
        case CC_COMBAT_RAIDER: return DANGER;
        case CC_COMBAT_NEUTRAL:
        default: return MUTED;
    }
}

static void DrawCombatImpactBanner(const LocalState *local)
{
    const CcLocalCourse *course = &local->course;
    if (course->combat_event_seconds <= 0.0f ||
        course->last_outcome == CC_COMBAT_OUTCOME_NONE) {
        return;
    }
    char headline[64];
    char detail[64];
    (void)snprintf(headline, sizeof(headline), "%s  /  %s",
                   CcLocalCombatTeamName(course->last_attacker_team),
                   CcLocalCombatOutcomeName(course->last_outcome));
    if (course->last_health_damage >= 0.5f) {
        (void)snprintf(detail, sizeof(detail), "-%d HEALTH",
                       (int32_t)lroundf(course->last_health_damage));
    } else if (course->last_posture_damage >= 0.5f) {
        (void)snprintf(detail, sizeof(detail), "-%d POSTURE",
                       (int32_t)lroundf(course->last_posture_damage));
    } else {
        (void)snprintf(detail, sizeof(detail), "%s",
                       course->last_outcome == CC_COMBAT_OUTCOME_MISS ?
                           "NO CONTACT" : "NO DAMAGE");
    }
    float opacity = fminf(1.0f, course->combat_event_seconds / 0.24f);
    int headline_width = CcOverlayMeasureText(headline, 12);
    int detail_width = CcOverlayMeasureText(detail, 8);
    int width = (headline_width > detail_width ? headline_width :
                 detail_width) + 32;
    float x = ((float)GetScreenWidth() - (float)width) * 0.5f;
    Rectangle bounds = {x, 87.0f, (float)width, 52.0f};
    Color team_color = CombatTeamColor(course->last_attacker_team);
    DrawRectangleRounded(bounds, 0.20f, 5, Fade(PANEL_DEEP, 0.94f * opacity));
    DrawRectangleRoundedLinesEx(bounds, 0.20f, 5, 2.0f,
                                Fade(team_color, opacity));
    CcOverlayDrawText(headline,
                      (int)(x + ((float)width - (float)headline_width) * 0.5f),
                      98, 12, Fade(team_color, opacity));
    CcOverlayDrawText(detail,
                      (int)(x + ((float)width - (float)detail_width) * 0.5f),
                      120, 8,
                      Fade(course->last_defender_team == CC_COMBAT_PLAYER ?
                               DANGER : INK,
                           opacity));
}

static void DrawCombatPanel(const LocalState *local)
{
    int32_t guards_standing = 0;
    int32_t guards_engaged = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const CcLocalCourseRunner *guard = &local->course.runners[i];
        if (guard->agent.combat.life_state == CC_LIFE_ALIVE) {
            guards_standing += 1;
            if (guard->duty == CC_GUARD_ENGAGED) guards_engaged += 1;
        }
    }

    Rectangle panel = {(float)GetScreenWidth() - 326.0f, 78.0f,
                       304.0f, 240.0f};
    DrawPanel(panel, Fade(PANEL_DEEP, 0.95f));
    int x = (int)panel.x + 16;
    int y = (int)panel.y;
    const char *title = local->journey_combat_active ?
        "ROAD AMBUSH" : "STREET DEFENSE";
    CcOverlayDrawText(title, x, y + 13, 11, DANGER);
    CcOverlayDrawText(TextFormat("%.28s", local->course.raider_company_name),
                      x, y + 32, 9, INK);
    const char *resolve_label = CombatResolveLabel(local);
    int resolve_width = CcOverlayMeasureText(resolve_label, 8);
    CcOverlayDrawText(resolve_label,
                      (int)(panel.x + panel.width) - resolve_width - 16,
                      y + 14, 8,
                      local->course.raiders_retreating ? TEAL : CC_GOLD);
    DrawRectangle(x, y + 53, (int)panel.width - 32, 1, Fade(MUTED, 0.30f));

    const int column_width = 126;
    const int target_x = x + 146;
    const CcLocalAgent *target = SelectedCombatTarget(local);
    int32_t target_index = SelectedCombatTargetIndex(local);
    const char *player_state =
        local->agent.combat.life_state != CC_LIFE_ALIVE ? "DOWN" :
        local->agent.combat.stagger_seconds > 0.0f ? "STAGGERED" :
        local->agent.humanoid.guard_requested ? "GUARD UP" : "READY";
    CcOverlayDrawText("YOU", x, y + 65, 9, TEAL);
    CcOverlayDrawText(player_state, x, y + 81, 7,
                      local->agent.humanoid.guard_requested ? CC_GOLD : MUTED);
    DrawCombatMeter(x, y + 99, column_width, "HEALTH",
                    local->agent.combat.health, TEAL, true);
    DrawCombatMeter(x, y + 127, column_width, "POSTURE",
                    local->agent.combat.posture, CC_GOLD, true);

    CcOverlayDrawText(target != NULL ?
                          TextFormat("%.16s",
                                     local->course.raider_names[target_index]) :
                          "NO TARGET",
                      target_x, y + 65, 9,
                      target != NULL ? DANGER : Fade(MUTED, 0.68f));
    CcOverlayDrawText(target != NULL ?
                          CcLocalRaiderRoleName(
                              local->course.raider_roles[target_index]) :
                          "CLICK AN OUTLAW",
                      target_x, y + 81, 7, target != NULL ? MUTED : CC_GOLD);
    DrawCombatMeter(target_x, y + 99, column_width, "HEALTH",
                    target != NULL ? target->combat.health : 0.0f,
                    DANGER, target != NULL);
    DrawCombatMeter(target_x, y + 127, column_width, "POSTURE",
                    target != NULL ? target->combat.posture : 0.0f,
                    CC_VIOLET, target != NULL);

    CcOverlayDrawText("COMPANY NERVE", x, y + 164, 7, MUTED);
    CcOverlayDrawText(TextFormat("%d", local->course.raider_resolve),
                      x + 256, y + 163, 8, INK);
    DrawRectangle(x, y + 177, 268, 8, BAR_TRACK);
    DrawRectangle(x, y + 177,
                  (int)lroundf(268.0f *
                               (float)local->course.raider_resolve / 100.0f),
                  8, DANGER);
    CcOverlayDrawText(
        TextFormat("GUARDS %d  ENGAGED %d  OUTLAWS %d",
                   guards_standing, guards_engaged, StandingRaiders(local)),
        x, y + 204, 7, MUTED);
    CcOverlayDrawText(local->journey_combat_active ?
                          "BREAK THEM OR WITHDRAW" : "BREAK THEIR NERVE",
                      x, y + 220, 7, CC_GOLD);
    DrawCombatImpactBanner(local);
}

static void DrawLocalPanel(const CcSim *sim, const LocalState *local)
{
    bool road = local->journey_travel_active ||
                local->journey_combat_active ||
                local->journey_parley_active;
    if (road) {
        const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
        const CcSettlement *destination = CcSimSettlement(
            sim, sim->journey.destination_id);
        if (local->journey_combat_active) {
            DrawCombatPanel(local);
            return;
        }
        float panel_x = (float)GetScreenWidth() - 286.0f;
        int content_x = (int)panel_x + 18;
        DrawPanel((Rectangle){panel_x, 78.0f, 264.0f, 150.0f},
                  Fade(PANEL_DEEP, 0.93f));
        if (local->journey_travel_active) {
            int32_t progress = sim->carriage.progress_milli / 10;
            CcOverlayDrawText("ON THE ROAD", content_x, 91, 9, TEAL);
            CcOverlayDrawText(destination != NULL ? destination->name : "THE FAR GATE",
                     content_x, 112, 15, INK);
            DrawBar(content_x, 145, 74, "PROGRESS", progress, TEAL);
            DrawBar(content_x, 166, 74, "DANGER", sim->journey.danger, DANGER);
            DrawBar(content_x, 187, 74, "ROAD",
                    route != NULL ? route->condition : 0, CC_GOLD);
            return;
        }
        CcOverlayDrawText(local->journey_parley_active ? "PARLEY" :
                                               "CORDON",
                 content_x, 91, 9,
                 local->journey_parley_active ? TEAL : DANGER);
        CcOverlayDrawText(TextFormat("%.20s",
                                    local->course.raider_company_name),
                          content_x, 112, 12, INK);
        CcLocalDrawAgentPortrait3D(
            &local->course.raiders[0],
            (Rectangle){(float)content_x, 143.0f, 44.0f, 54.0f});
        CcOverlayDrawText(local->course.raider_names[0],
                 content_x + 54, 148, 9,
                 local->journey_parley_active ? CC_GOLD : DANGER);
        CcOverlayDrawText(CcLocalRaiderRoleName(local->course.raider_roles[0]),
                          content_x + 54, 160, 7, MUTED);
        DrawBar(content_x + 54, 175, 44, "HP",
                (int32_t)lroundf(local->course.raiders[0].combat.health),
                DANGER);
        return;
    }
    if (!local->market_interior && local->course.alarm_active) {
        DrawCombatPanel(local);
        return;
    }
}

static void DrawLocalMovementReticle(const LocalState *local,
                                     Rectangle local_bounds)
{
    if (local == NULL || !local->movement_reticle_valid ||
        !CheckCollisionPointRec(local->movement_reticle, local_bounds)) {
        return;
    }
    Vector2 point = local->movement_reticle;
    float pulse = 1.0f + 0.12f * sinf(local->movement_reticle_age * 12.0f);
    float arm = 7.0f * pulse;
    if (local->movement_reticle_accepted) {
        Color accepted = CC_GOLD;
        DrawCircleLines((int)lroundf(point.x), (int)lroundf(point.y),
                        arm, accepted);
        DrawCircleV(point, 1.8f, TEAL);
        DrawLineEx((Vector2){point.x - arm - 3.0f, point.y},
                   (Vector2){point.x - arm + 1.0f, point.y},
                   2.0f, accepted);
        DrawLineEx((Vector2){point.x + arm - 1.0f, point.y},
                   (Vector2){point.x + arm + 3.0f, point.y},
                   2.0f, accepted);
        return;
    }
    DrawLineEx((Vector2){point.x - arm, point.y - arm},
               (Vector2){point.x + arm, point.y + arm}, 2.0f, DANGER);
    DrawLineEx((Vector2){point.x + arm, point.y - arm},
               (Vector2){point.x - arm, point.y + arm}, 2.0f, DANGER);
}


static bool MapVisibleAtCarriage(const CcSim *sim, const CcMap *map)
{
    if (sim == NULL || map == NULL) return false;
    if (map->owner_id == sim->player.location_id) return true;
    if (map->owner_id != sim->player.id ||
        !CcSimMapIsCatalogued(sim, map)) return false;
    return !CcSimMapIsArchived(sim, map) ||
           sim->player.location_id == sim->settlements[1].id;
}

static int32_t VisibleMapCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        if (MapVisibleAtCarriage(sim, &sim->maps[i])) count += 1;
    }
    return count;
}

static int32_t VisibleMapListStart(const CcSim *sim, int32_t selected)
{
    int32_t rank = 0;
    int32_t selected_rank = 0;
    for (int32_t i = 0; sim != NULL && i < sim->map_count; ++i) {
        if (!MapVisibleAtCarriage(sim, &sim->maps[i])) continue;
        if (i == selected) selected_rank = rank;
        rank += 1;
    }
    int32_t start = selected_rank - CC_MAP_LIST_ROWS / 2;
    int32_t maximum = rank - CC_MAP_LIST_ROWS;
    if (start < 0) start = 0;
    if (maximum < 0) maximum = 0;
    if (start > maximum) start = maximum;
    return start;
}

static int32_t FirstVisibleMapIndex(const CcSim *sim)
{
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        const CcRoute *route = CcSimRoute(sim, map->route_id);
        if (map->owner_id == sim->player.id && route != NULL &&
            (route->from_id == sim->player.location_id ||
             route->to_id == sim->player.location_id)) return i;
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        if (sim->maps[i].owner_id == sim->player.location_id) return i;
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        if (sim->maps[i].owner_id == sim->player.id) return i;
    }
    return -1;
}

static int32_t StepVisibleMapIndex(const CcSim *sim, int32_t selected,
                                   int32_t direction)
{
    if (sim == NULL || sim->map_count <= 0) return -1;
    int32_t index = selected;
    for (int32_t step = 0; step < sim->map_count; ++step) {
        index = (index + direction + sim->map_count) % sim->map_count;
        if (MapVisibleAtCarriage(sim, &sim->maps[index])) return index;
    }
    return selected;
}

static const CcMap *SelectedVisibleMap(const CcSim *sim, int32_t selected)
{
    if (sim == NULL || selected < 0 || selected >= sim->map_count ||
        !MapVisibleAtCarriage(sim, &sim->maps[selected])) return NULL;
    return &sim->maps[selected];
}

static bool RouteLeavesCurrentPlace(const CcSim *sim, const CcRoute *route)
{
    return sim != NULL && route != NULL &&
           (route->from_id == sim->player.location_id ||
            route->to_id == sim->player.location_id);
}

static int32_t FirstOutgoingRouteIndex(const CcSim *sim)
{
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (RouteLeavesCurrentPlace(sim, &sim->routes[i])) return i;
    }
    return -1;
}

static int32_t OutgoingRouteCount(const CcSim *sim)
{
    int32_t count = 0;
    if (sim == NULL) return count;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (RouteLeavesCurrentPlace(sim, &sim->routes[i])) count += 1;
    }
    return count;
}

static int32_t StepOutgoingRouteIndex(const CcSim *sim, int32_t selected,
                                      int32_t direction)
{
    if (sim == NULL || sim->route_count <= 0) return -1;
    int32_t index = selected;
    for (int32_t step = 0; step < sim->route_count; ++step) {
        index = (index + direction + sim->route_count) % sim->route_count;
        if (RouteLeavesCurrentPlace(sim, &sim->routes[index])) return index;
    }
    return selected;
}

static const CcRoute *SelectedOutgoingRoute(const CcSim *sim,
                                            int32_t selected)
{
    if (sim == NULL || selected < 0 || selected >= sim->route_count ||
        !RouteLeavesCurrentPlace(sim, &sim->routes[selected])) return NULL;
    return &sim->routes[selected];
}

static const CcMap *VisibleMapForRoute(const CcSim *sim, CcId route_id)
{
    if (sim == NULL) return NULL;
    const CcMap *local_offer = NULL;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (map->route_id != route_id || !MapVisibleAtCarriage(sim, map)) {
            continue;
        }
        if (map->owner_id == sim->player.id) return map;
        local_offer = map;
    }
    return local_offer;
}

static CcId RouteOtherEnd(const CcRoute *route, CcId here)
{
    if (route == NULL) return 0U;
    if (route->from_id == here) return route->to_id;
    if (route->to_id == here) return route->from_id;
    return 0U;
}

static void AddContextAction(ContextActionSet *set, ContextActionKind kind,
                             const char *label)
{
    if (set == NULL || label == NULL ||
        set->count >= (int32_t)(sizeof(set->items) / sizeof(set->items[0]))) {
        return;
    }
    ContextAction *action = &set->items[set->count++];
    action->kind = kind;
    action->enabled = true;
    (void)snprintf(action->label, sizeof(action->label), "%s", label);
}

static void AddDetailedContextAction(ContextActionSet *set,
                                     ContextActionKind kind,
                                     const char *label,
                                     const char *key_hint,
                                     const char *detail,
                                     bool enabled, bool active)
{
    int32_t previous_count = set != NULL ? set->count : 0;
    AddContextAction(set, kind, label);
    if (set == NULL || set->count == previous_count) return;
    ContextAction *action = &set->items[set->count - 1];
    action->enabled = enabled;
    action->active = active;
    (void)snprintf(action->key_hint, sizeof(action->key_hint), "%s",
                   key_hint != NULL ? key_hint : "");
    (void)snprintf(action->detail, sizeof(action->detail), "%s",
                   detail != NULL ? detail : "");
}

static int32_t ActiveSituationCount(const CcSim *sim)
{
    int32_t count = 0;
    if (sim == NULL) return count;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) count += 1;
    }
    return count;
}

static CcGood ContextCargoGood(const CcSim *sim)
{
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    if (accepted != NULL &&
        (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
         accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY)) {
        return accepted->good;
    }
    return CC_GOOD_FOOD;
}

static int32_t SelectedCombatTargetIndex(const LocalState *local)
{
    if (local == NULL) return -1;
    int32_t target = local->agent.combat.target_index;
    if (target < 0 || target >= CC_LOCAL_RAIDER_COUNT ||
        local->course.raiders[target].combat.life_state != CC_LIFE_ALIVE) {
        return -1;
    }
    return target;
}

static const CcLocalAgent *SelectedCombatTarget(const LocalState *local)
{
    int32_t target = SelectedCombatTargetIndex(local);
    return target >= 0 ? &local->course.raiders[target] : NULL;
}

static const char *CombatSkillDetail(const CcLocalAgent *player,
                                     CcCombatSkill skill,
                                     bool needs_target,
                                     bool has_target)
{
    float cooldown = CcLocalCombatSkillCooldown(player, skill);
    if (cooldown > 0.0f) return TextFormat("READY IN %.1fs", cooldown);
    if (needs_target && !has_target) return "CHOOSE TARGET";
    if (skill == CC_COMBAT_SKILL_SECOND_WIND) {
        return TextFormat("POSTURE %d",
                          (int32_t)lroundf(player->combat.posture));
    }
    return "READY";
}

static bool CombatSkillEnabled(const CcLocalAgent *player,
                               CcCombatSkill skill,
                               bool needs_target,
                               bool has_target)
{
    if (player == NULL || player->combat.life_state != CC_LIFE_ALIVE ||
        CcLocalCombatSkillCooldown(player, skill) > 0.0f ||
        (needs_target && !has_target)) {
        return false;
    }
    return skill != CC_COMBAT_SKILL_SECOND_WIND ||
           player->combat.posture < CC_LOCAL_COMBAT_MAX_POSTURE;
}

static void AddCombatActions(ContextActionSet *set,
                             const LocalState *local,
                             bool allow_withdraw)
{
    int32_t target = SelectedCombatTargetIndex(local);
    bool has_target = target >= 0;
    const CcCombatState *combat = &local->agent.combat;
    AddDetailedContextAction(
        set, CONTEXT_ACTION_BASIC_STRIKE, "Attack", "SPACE",
        has_target ? local->course.raider_names[target] : "CHOOSE TARGET",
        has_target, false);
    AddDetailedContextAction(
        set, CONTEXT_ACTION_TOGGLE_GUARD, "Guard", "X",
        local->agent.humanoid.guard_requested ? "GUARD UP" : "GUARD DOWN",
        true, local->agent.humanoid.guard_requested);
    AddDetailedContextAction(
        set, CONTEXT_ACTION_SKILL_CRUSHING, "Crushing blow", "1",
        CombatSkillDetail(&local->agent, CC_COMBAT_SKILL_CRUSHING_BLOW,
                          true, has_target),
        CombatSkillEnabled(&local->agent, CC_COMBAT_SKILL_CRUSHING_BLOW,
                           true, has_target),
        combat->queued_skill == CC_COMBAT_SKILL_CRUSHING_BLOW ||
            combat->active_skill == CC_COMBAT_SKILL_CRUSHING_BLOW);
    AddDetailedContextAction(
        set, CONTEXT_ACTION_SKILL_SUNDER, "Sunder", "2",
        CombatSkillDetail(&local->agent, CC_COMBAT_SKILL_SUNDER,
                          true, has_target),
        CombatSkillEnabled(&local->agent, CC_COMBAT_SKILL_SUNDER,
                           true, has_target),
        combat->queued_skill == CC_COMBAT_SKILL_SUNDER ||
            combat->active_skill == CC_COMBAT_SKILL_SUNDER);
    AddDetailedContextAction(
        set, CONTEXT_ACTION_SKILL_SECOND_WIND, "Second wind", "3",
        CombatSkillDetail(&local->agent, CC_COMBAT_SKILL_SECOND_WIND,
                          false, has_target),
        CombatSkillEnabled(&local->agent, CC_COMBAT_SKILL_SECOND_WIND,
                           false, has_target), false);
    if (allow_withdraw) {
        AddDetailedContextAction(
            set, CONTEXT_ACTION_WITHDRAW, "Withdraw", "BACKSPACE",
            "LEAVE THE FIGHT", true, false);
    }
}

static ContextActionSet BuildContextActions(
    const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation)
{
    ContextActionSet set = {0};
    if (sim == NULL || local == NULL) return set;
    if (view == VIEW_ENCOUNTER) {
        AddDetailedContextAction(&set, CONTEXT_ACTION_FIGHT,
                                 "Draw steel", "1",
                                 "ENTER COMBAT", true, false);
        AddDetailedContextAction(&set, CONTEXT_ACTION_PAY,
                                 "Hear them out", "2",
                                 "ENTER PARLEY", true, false);
        return set;
    }
    if (view == VIEW_LEDGER) {
        AddContextAction(&set, CONTEXT_ACTION_CLOSE_VIEW, "Close");
        return set;
    }
    if (view == VIEW_SITUATIONS) {
        const CcSituation *detail = SelectedActiveSituation(
            sim, selected_situation);
        if (detail != NULL &&
            detail->id == sim->player.accepted_situation_id) {
            AddContextAction(&set, CONTEXT_ACTION_ABANDON_PROMISE,
                             "Abandon quest");
        } else if (detail != NULL && CcSimAcceptedSituation(sim) == NULL) {
            AddContextAction(&set, CONTEXT_ACTION_ACCEPT_PROMISE,
                             "Accept quest");
        }
        if (ActiveSituationCount(sim) > 1) {
            AddContextAction(&set, CONTEXT_ACTION_NEXT_PROMISE,
                             "Next quest");
        }
        AddContextAction(&set, CONTEXT_ACTION_CLOSE_VIEW, "Close");
        return set;
    }
    if (view == VIEW_MAP) {
        const CcMap *map = SelectedVisibleMap(sim, selected);
        if (map != NULL && map->owner_id == sim->player.location_id) {
            AddContextAction(&set, CONTEXT_ACTION_BUY_MAP,
                             TextFormat("Buy map — %d crowns",
                                        map->ask_price));
        }
        AddContextAction(&set, CONTEXT_ACTION_CLOSE_VIEW, "Close map case");
        return set;
    }
    if (view == VIEW_ROADS) {
        const CcRoute *route = SelectedOutgoingRoute(sim, selected);
        const CcMap *map = route != NULL ?
            VisibleMapForRoute(sim, route->id) : NULL;
        if (route != NULL) {
            AddContextAction(&set, CONTEXT_ACTION_TRAVEL,
                             route->smuggler_route ?
                                 "Take hidden branch" : "Take branch");
        }
        if (OutgoingRouteCount(sim) > 1) {
            AddContextAction(&set, CONTEXT_ACTION_NEXT_BRANCH,
                             "Keep on track");
        }
        if (map != NULL && map->owner_id == sim->player.location_id) {
            AddContextAction(&set, CONTEXT_ACTION_BUY_MAP,
                             TextFormat("Buy notes — %d crowns", map->ask_price));
        } else if (route != NULL &&
                   (route->closed || route->condition < 75)) {
            AddContextAction(&set, CONTEXT_ACTION_REPAIR_ROUTE,
                             "Repair road");
        }
        AddContextAction(&set, CONTEXT_ACTION_CLOSE_VIEW, "Return to town");
        return set;
    }

    if (local->journey_travel_active) return set;
    if (local->journey_parley_active) {
        Vector2 collector = {CC_LOCAL_ROAD_PARLEY_X,
                             CC_LOCAL_ROAD_PARLEY_Z};
        if (GridDistance(LocalPosition(local), collector) < 1.55f) {
            AddContextAction(&set, CONTEXT_ACTION_PAY_COLLECTOR,
                             TextFormat("Pay %d crowns",
                                        sim->journey.bargain_cost));
            CcGood good = CC_GOOD_FOOD;
            int32_t quantity = 0;
            if (CcSimBanditProvisionDemand(sim, sim->journey.route_id,
                                           &good, &quantity)) {
                AddContextAction(&set, CONTEXT_ACTION_OFFER_PROVISIONS,
                                 TextFormat("Offer %d %s", quantity,
                                            CcGoodName(good)));
            }
        }
        AddContextAction(&set, CONTEXT_ACTION_RETURN_TO_CHOICE,
                         "Return to carriage");
        return set;
    }
    if (local->journey_combat_active) {
        AddCombatActions(&set, local, true);
        return set;
    }
    if (!local->market_interior && local->course.alarm_active) {
        AddCombatActions(&set, local, false);
        return set;
    }

    Vector2 position = LocalPosition(local);
    if (local->market_interior) {
        if (GridDistance(position, INTERIOR_COUNTER) < 2.25f) {
            CcGood good = ContextCargoGood(sim);
            const CcSituation *accepted = CcSimAcceptedSituation(sim);
            bool delivery = accepted != NULL &&
                (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
                 accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) &&
                accepted->target_id == sim->player.location_id &&
                sim->player.cargo[good] > 0;
            if (delivery) {
                int32_t remaining = accepted->quantity - accepted->progress;
                if (remaining < 1) remaining = 1;
                AddContextAction(
                    &set, CONTEXT_ACTION_DELIVER_CARGO,
                    TextFormat("Deliver %d %s", remaining,
                               CcGoodName(good)));
            } else {
                const CcSettlement *place = CcSimSettlement(
                    sim, sim->player.location_id);
                if (place != NULL && place->stock[good] > 0) {
                    AddContextAction(
                        &set, CONTEXT_ACTION_BUY_CARGO,
                        TextFormat("Buy %s — %d crowns", CcGoodName(good),
                                   place->price[good]));
                }
                if (sim->player.cargo[good] > 0) {
                    AddContextAction(
                        &set, CONTEXT_ACTION_SELL_CARGO,
                        TextFormat("Sell %s", CcGoodName(good)));
                }
            }
        } else if (GridDistance(position, INTERIOR_EXIT) < 1.25f) {
            AddContextAction(&set, CONTEXT_ACTION_LEAVE_MARKET,
                             "Leave market");
        }
        return set;
    }

    if (GridDistance(position, LOCAL_MARKET) < 1.30f) {
        AddContextAction(&set, CONTEXT_ACTION_ENTER_MARKET,
                         "Enter market");
    }
    if (GridDistance(position, LOCAL_CARRIAGE) < 1.35f) {
        AddContextAction(&set, CONTEXT_ACTION_CHOOSE_ROAD, "Drive out");
        AddContextAction(&set, CONTEXT_ACTION_OPEN_MAP, "Open map case");
    }
    if (GridDistance(position, LOCAL_NOTICE) < 1.15f) {
        AddContextAction(&set, CONTEXT_ACTION_OPEN_PROMISES,
                         "View quests");
    }
    const CcDungeon *dungeon = DungeonAtSettlement(
        sim, sim->player.location_id);
    if (dungeon != NULL &&
        GridDistance(position, LOCAL_DUNGEON) < 1.35f) {
        AddContextAction(&set, CONTEXT_ACTION_EXPEDITION,
                         "Enter the mine");
    }
    if (local->agent.morphology == CC_MORPHOLOGY_BIPED) {
        AddContextAction(&set, CONTEXT_ACTION_JUMP, "Jump");
    }
    AddContextAction(&set, CONTEXT_ACTION_RAISE_ALARM, "Raise alarm");
    return set;
}

static Rectangle ContextActionBounds(int32_t index, int32_t count)
{
    const float gap = 8.0f;
    const float margin = 22.0f;
    float width = ((float)GetScreenWidth() - margin * 2.0f -
                   (float)(count - 1) * gap) / (float)count;
    if (width > 220.0f) width = 220.0f;
    float total = (float)count * width + (float)(count - 1) * gap;
    return (Rectangle){((float)GetScreenWidth() - total) * 0.5f +
                           (float)index * (width + gap),
                       (float)GetScreenHeight() - 66.0f, width, 58.0f};
}

static Color ContextActionColor(ContextActionKind kind)
{
    if (kind == CONTEXT_ACTION_FIGHT ||
        kind == CONTEXT_ACTION_ABANDON_PROMISE ||
        kind == CONTEXT_ACTION_RAISE_ALARM ||
        kind == CONTEXT_ACTION_WITHDRAW) return DANGER;
    if (kind == CONTEXT_ACTION_ACCEPT_PROMISE ||
        kind == CONTEXT_ACTION_TRAVEL ||
        kind == CONTEXT_ACTION_DELIVER_CARGO ||
        kind == CONTEXT_ACTION_ENTER_MARKET ||
        kind == CONTEXT_ACTION_JUMP ||
        kind == CONTEXT_ACTION_OFFER_PROVISIONS) return TEAL;
    if (kind == CONTEXT_ACTION_BASIC_STRIKE ||
        kind == CONTEXT_ACTION_TOGGLE_GUARD) return CC_GOLD;
    if (kind == CONTEXT_ACTION_SKILL_CRUSHING ||
        kind == CONTEXT_ACTION_SKILL_SUNDER ||
        kind == CONTEXT_ACTION_SKILL_SECOND_WIND) return CC_VIOLET;
    return CC_GOLD;
}

static void DrawContextActionTray(const CcSim *sim, const LocalState *local,
                                  ClientView view, int32_t selected,
                                  int32_t selected_situation)
{
    ContextActionSet actions = BuildContextActions(
        sim, local, view, selected, selected_situation);
    Vector2 mouse = GetMousePosition();
    for (int32_t i = 0; i < actions.count; ++i) {
        Rectangle bounds = ContextActionBounds(i, actions.count);
        const ContextAction *action = &actions.items[i];
        bool hover = action->enabled && CheckCollisionPointRec(mouse, bounds);
        Color accent = ContextActionColor(action->kind);
        Color fill = action->active ? Fade(accent, 0.20f) :
                     hover ? PANEL_HOVER : Fade(PANEL_DEEP, 0.96f);
        DrawRectangleRounded(bounds, 0.16f, 5, fill);
        DrawRectangleRoundedLinesEx(bounds, 0.18f, 5,
                                    hover || action->active ? 2.0f : 1.0f,
                                    action->enabled ?
                                        Fade(accent, hover || action->active ?
                                                     0.96f : 0.62f) :
                                        Fade(MUTED, 0.34f));
        if (hover || action->active) {
            DrawRectangleRounded(
                (Rectangle){bounds.x + 7.0f, bounds.y + 10.0f,
                            3.0f, bounds.height - 20.0f},
                0.8f, 3, accent);
        }
        bool detailed = action->detail[0] != '\0' ||
                        action->key_hint[0] != '\0';
        Color label_color = !action->enabled ? Fade(MUTED, 0.62f) :
                            hover || action->active ? accent : INK;
        int width = CcOverlayMeasureText(action->label, detailed ? 10 : 11);
        CcOverlayDrawText(action->label,
                          (int)(bounds.x +
                                (bounds.width - (float)width) * 0.5f),
                          (int)bounds.y + (detailed ? 10 : 22),
                          detailed ? 10 : 11, label_color);
        if (action->key_hint[0] != '\0') {
            const char *hint = TextFormat("[%s]", action->key_hint);
            int hint_width = CcOverlayMeasureText(hint, 7);
            CcOverlayDrawText(hint,
                              (int)(bounds.x + bounds.width -
                                    (float)hint_width - 9.0f),
                              (int)bounds.y + 9, 7,
                              action->enabled ? Fade(accent, 0.86f) :
                                                Fade(MUTED, 0.48f));
        }
        if (action->detail[0] != '\0') {
            int detail_width = CcOverlayMeasureText(action->detail, 7);
            CcOverlayDrawText(action->detail,
                              (int)(bounds.x +
                                    (bounds.width - (float)detail_width) *
                                        0.5f),
                              (int)bounds.y + 38, 7,
                              action->enabled ? Fade(MUTED, 0.92f) :
                                                Fade(MUTED, 0.46f));
        }
    }
}

static ContextActionKind PressedContextAction(
    const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation)
{
    if (!ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return CONTEXT_ACTION_NONE;
    }
    ContextActionSet actions = BuildContextActions(
        sim, local, view, selected, selected_situation);
    Vector2 mouse = GetMousePosition();
    for (int32_t i = 0; i < actions.count; ++i) {
        if (actions.items[i].enabled && CheckCollisionPointRec(
                mouse, ContextActionBounds(i, actions.count))) {
            return actions.items[i].kind;
        }
    }
    return CONTEXT_ACTION_NONE;
}

static void DrawCombatStatusLine(const LocalState *local,
                                 const char *message, float message_age)
{
    if (!LocalCombatActive(local)) return;
    const CcLocalAgent *target = SelectedCombatTarget(local);
    const char *status = NULL;
    if (message != NULL && message[0] != '\0' && message_age < 3.2f) {
        status = message;
    } else if (target == NULL) {
        status = "Click an outlaw to focus your attacks.";
    } else if (local->agent.humanoid.guard_requested) {
        status = "Guard is up. Watch posture and strike after the block.";
    } else {
        status = "Attack with Space, guard with X, and use skills with 1–3.";
    }
    char shown[96];
    (void)snprintf(shown, sizeof(shown), "COMBAT  /  %.72s", status);
    int width = CcOverlayMeasureText(shown, 9) + 30;
    if (width > 760) width = 760;
    float x = ((float)GetScreenWidth() - (float)width) * 0.5f;
    float y = (float)GetScreenHeight() - 99.0f;
    Color accent = target != NULL ? TEAL : CC_GOLD;
    DrawRectangleRounded((Rectangle){x, y, (float)width, 27.0f},
                         0.20f, 5, Fade(PANEL_DEEP, 0.96f));
    DrawRectangleRoundedLinesEx((Rectangle){x, y, (float)width, 27.0f},
                                0.20f, 5, 1.0f, Fade(accent, 0.62f));
    CcOverlayDrawText(shown, (int)x + 15, (int)y + 8, 9, INK);
}

static Rectangle CommandActionBounds(CommandActionKind action)
{
    const float width = 112.0f;
    const float gap = 8.0f;
    int32_t index = (int32_t)action - 1;
    return (Rectangle){22.0f + (float)index * (width + gap),
                       47.0f, width, 28.0f};
}

static const char *CommandActionLabel(CommandActionKind action)
{
    switch (action) {
        case COMMAND_ACTION_QUESTS: return "Quests";
        case COMMAND_ACTION_LEDGER: return "Ledger";
        case COMMAND_ACTION_MAP: return "Map";
        case COMMAND_ACTION_SAVE: return "Save";
        default: return "";
    }
}

static bool CommandActionActive(CommandActionKind action, ClientView view)
{
    return (action == COMMAND_ACTION_QUESTS && view == VIEW_SITUATIONS) ||
           (action == COMMAND_ACTION_LEDGER && view == VIEW_LEDGER) ||
           (action == COMMAND_ACTION_MAP && view == VIEW_MAP);
}

static bool CommandActionEnabled(CommandActionKind action,
                                 const LocalState *local)
{
    bool road_local = local != NULL &&
        (local->journey_travel_active || local->journey_combat_active ||
         local->journey_parley_active);
    return !road_local ||
           (action != COMMAND_ACTION_QUESTS && action != COMMAND_ACTION_MAP);
}

static void DrawCommandBar(ClientView view, const LocalState *local)
{
    Vector2 mouse = GetMousePosition();
    for (int32_t value = COMMAND_ACTION_QUESTS;
         value < COMMAND_ACTION_COUNT; ++value) {
        CommandActionKind action = (CommandActionKind)value;
        Rectangle bounds = CommandActionBounds(action);
        bool enabled = CommandActionEnabled(action, local);
        bool hover = enabled && CheckCollisionPointRec(mouse, bounds);
        bool active = CommandActionActive(action, view);
        Color accent = action == COMMAND_ACTION_SAVE ? TEAL : CC_GOLD;
        DrawRectangleRounded(
            bounds, 0.18f, 5,
            hover || active ? PANEL_HOVER :
            enabled ? Fade(PANEL_DEEP, 0.96f) : Fade(PANEL_DEEP, 0.72f));
        DrawRectangleRoundedLinesEx(
            bounds, 0.18f, 5, hover || active ? 2.0f : 1.0f,
            enabled ? Fade(accent, hover || active ? 0.96f : 0.62f) :
                      Fade(MUTED, 0.28f));
        const char *label = CommandActionLabel(action);
        int width = CcOverlayMeasureText(label, 10);
        CcOverlayDrawText(label,
                          (int)(bounds.x +
                                (bounds.width - (float)width) * 0.5f),
                          (int)bounds.y + 9, 10,
                          !enabled ? Fade(MUTED, 0.48f) :
                          hover || active ? accent : INK);
    }
}

static CommandActionKind PressedCommandAction(const LocalState *local)
{
    if (!ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return COMMAND_ACTION_NONE;
    }
    Vector2 mouse = GetMousePosition();
    for (int32_t value = COMMAND_ACTION_QUESTS;
         value < COMMAND_ACTION_COUNT; ++value) {
        CommandActionKind action = (CommandActionKind)value;
        if (CommandActionEnabled(action, local) &&
            CheckCollisionPointRec(mouse, CommandActionBounds(action))) {
            return action;
        }
    }
    return COMMAND_ACTION_NONE;
}

static void RoadChoiceLabel(const CcSim *sim, const CcRoute *route,
                            char *label, size_t capacity)
{
    CcId destination_id = RouteOtherEnd(route, sim->player.location_id);
    const CcSettlement *destination = CcSimSettlement(sim, destination_id);
    CcTravelPreview preview = {0};
    bool available = CcSimTravelPreview(sim, destination_id, &preview,
                                        NULL, 0U);
    if (route != NULL && route->smuggler_route &&
        (!available || !preview.destination_known)) {
        (void)snprintf(label, capacity, "Unmarked");
        return;
    }
    (void)snprintf(label, capacity, "%s",
                   destination != NULL ? destination->name : "Unknown road");
}

static void DrawRoadPanel(const CcSim *sim, int32_t selected)
{
    Rectangle panel = {978.0f, 82.0f, 282.0f, 322.0f};
    DrawPanel(panel, (Color){8, 16, 20, 226});
    const CcRoute *route = SelectedOutgoingRoute(sim, selected);
    CcOverlayDrawText("BRANCH", 998, 102, 9, TEAL);
    if (route == NULL) {
        CcOverlayDrawText("NO ROAD", 998, 128, 15, MUTED);
        return;
    }
    CcId destination_id = RouteOtherEnd(route, sim->player.location_id);
    CcTravelPreview preview = {0};
    (void)CcSimTravelPreview(sim, destination_id, &preview, NULL, 0U);
    char road_label[64];
    RoadChoiceLabel(sim, route, road_label, sizeof(road_label));
    CcOverlayDrawText(road_label, 998, 126, 18,
                      preview.destination_known ? INK : CC_VIOLET);
    CcOverlayDrawText(route->smuggler_route ? "FAINT WHEEL RUTS" :
                      route->closed ? "GUARDED CROSSING" : "SIGNED ROAD",
                      998, 151, 8, MUTED);

    CcOverlayDrawText(TextFormat("%d DAYS", preview.travel_days),
                      998, 185, 14, CC_GOLD);
    CcOverlayDrawText(TextFormat("%" PRId64 " CROWNS",
                                 preview.provision_cost),
                      1105, 185, 14, CC_GOLD);

    const CcMap *map = VisibleMapForRoute(sim, route->id);
    CcOverlayDrawText("NOTES", 998, 225, 9, TEAL);
    if (map != NULL && map->owner_id == sim->player.id) {
        CcOverlayDrawText(TextFormat("SURVEY %d DAYS OLD",
                                     sim->current_day - map->surveyed_day),
                          998, 246, 9, INK);
        DrawBar(998, 273, 92, "ROAD", map->recorded_condition, CC_GOLD);
        DrawBar(998, 300, 92, "DANGER", map->recorded_danger, DANGER);
        DrawBar(998, 327, 92, "TRUST", map->accuracy, TEAL);
    } else if (preview.sponsored_guide) {
        CcOverlayDrawText("LOCAL GUIDE", 998, 250, 11, INK);
    } else if (map != NULL) {
        CcOverlayDrawText(TextFormat("FOR SALE  %d CROWNS",
                                     map->ask_price),
                          998, 250, 10, CC_GOLD);
    } else {
        CcOverlayDrawText("NONE", 998, 250, 11, MUTED);
        CcOverlayDrawText("+2 DAYS   +20 RISK", 998, 274, 9, DANGER);
    }
}

static Vector2 ChartEndpoint(const CcMap *map, bool far_end)
{
    uint32_t mark = (uint32_t)(map->id & UINT64_C(0xffffffff));
    float angle = ((float)(mark % 101U) / 100.0f - 0.5f) * 1.05f;
    float half = 188.0f + (float)((mark >> 8U) % 29U);
    float sign = far_end ? 1.0f : -1.0f;
    return (Vector2){605.0f + cosf(angle) * half * sign,
                     363.0f + sinf(angle) * half * sign};
}

static void DrawChartRoute(const CcMap *map, Color ink)
{
    Vector2 a = ChartEndpoint(map, false);
    Vector2 b = ChartEndpoint(map, true);
    Vector2 delta = {b.x - a.x, b.y - a.y};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
    Vector2 normal = length > 0.01f ?
        (Vector2){-delta.y / length, delta.x / length} : (Vector2){0.0f, 1.0f};
    float error = (float)(100 - map->accuracy) / 100.0f;
    float bend = ((map->id >> 20U) & 1U ? 1.0f : -1.0f) *
                 (24.0f + error * 74.0f);
    Vector2 previous = a;
    for (int32_t step = 1; step <= 28; ++step) {
        float t = (float)step / 28.0f;
        float arch = 4.0f * t * (1.0f - t);
        Vector2 point = {a.x + delta.x * t + normal.x * bend * arch,
                         a.y + delta.y * t + normal.y * bend * arch};
        if (!map->contraband || (step % 3) != 0) {
            DrawLineEx(previous, point, 3.0f, ink);
        }
        previous = point;
    }
    for (int32_t mark = 1; mark <= 3; ++mark) {
        float t = (float)mark / 4.0f;
        Vector2 point = {a.x + delta.x * t, a.y + delta.y * t};
        DrawCircleV(point, 4.0f, Fade(ink, 0.72f));
        DrawCircleLinesV(point, 7.0f, Fade(ink, 0.32f));
    }
}

static void DrawChartTown(Vector2 point, const CcSettlement *place, bool current,
                          Color ink)
{
    DrawCircleV(point, current ? 19.0f : 15.0f, Fade(ink, 0.16f));
    DrawPoly(point, place != NULL && place->function == CC_SETTLEMENT_FORTRESS ? 4 : 6,
             current ? 12.0f : 9.0f, 0.0f, ink);
    const char *name = place != NULL ? place->name : "Unknown terminus";
    int width = CcOverlayMeasureText(name, 15);
    CcOverlayDrawText(name, (int)point.x - width / 2, (int)point.y + 24, 15, ink);
    if (current) CcOverlayDrawText("CARRIAGE", (int)point.x - 34, (int)point.y - 38, 10, DANGER);
}

static void MapRouteTitle(const CcSim *sim, const CcMap *map,
                          char *label, size_t capacity)
{
    const CcRoute *route = map != NULL ? CcSimRoute(sim, map->route_id) : NULL;
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    (void)snprintf(label, capacity, "%s to %s",
                   from != NULL ? from->name : "Unknown",
                   to != NULL ? to->name : "Unknown");
}

static bool DrawCollectibleMapArt(const CcSim *sim, const CcMap *map,
                                  Texture2D illustrated_map,
                                  Texture2D collectible_atlas)
{
    if (IsGloamgateAlderwatchMap(map) && illustrated_map.id != 0U) {
        Rectangle source = {0.0f, 0.0f, (float)illustrated_map.width,
                            (float)illustrated_map.height};
        Rectangle destination = {294.0f, 163.0f, 596.0f, 397.33f};
        DrawTexturePro(illustrated_map, source, destination,
                       (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
        return true;
    }
    if (sim == NULL || map == NULL || collectible_atlas.id == 0U) {
        return false;
    }
    static const int32_t art_cell[CC_MAP_COLLECTION_COUNT] = {
        2, 0, 4, 5, 8, 9, 1, 3, 6, 7, 10, 11
    };
    int32_t slot = (int32_t)(map - sim->maps);
    if (slot < 0 || slot >= CC_MAP_COLLECTION_COUNT) return false;
    int32_t cell = art_cell[slot];
    float cell_width = (float)collectible_atlas.width / 4.0f;
    float cell_height = (float)collectible_atlas.height / 3.0f;
    Rectangle source = {(float)(cell % 4) * cell_width,
                        (float)(cell / 4) * cell_height,
                        cell_width, cell_height};
    Rectangle destination = {393.5f, 164.0f, 397.0f, 397.0f};
    DrawTexturePro(collectible_atlas, source, destination,
                   (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    Color title_ink = (Color){66, 57, 43, 255};
    int32_t title_width = CcOverlayMeasureText(map->name, 20);
    CcOverlayDrawText(map->name, 592 - title_width / 2, 125, 20,
                      title_ink);
    return true;
}

static void DrawMap(const CcSim *sim, int32_t selected, float clock,
                    Texture2D illustrated_map,
                    Texture2D collectible_atlas)
{
    (void)clock;
    DrawPanel((Rectangle){20.0f, 82.0f, 900.0f, 568.0f},
              PANEL);
    CcOverlayDrawText("CARRIAGE MAP CASE", 38, 101, 18, CC_GOLD);
    CcOverlayDrawText(TextFormat("CASE %d/%d   CATALOGUE %d/%d",
                        CcPlayerMapCount(sim), sim->player.map_capacity,
                        CcPlayerMapCollectionCount(sim),
                        CC_MAP_COLLECTION_COUNT), 38, 126, 10, MUTED);

    int32_t visible_start = VisibleMapListStart(sim, selected);
    int32_t visible_rank = 0;
    int32_t row = 0;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (!MapVisibleAtCarriage(sim, map)) continue;
        if (visible_rank++ < visible_start) continue;
        if (row >= CC_MAP_LIST_ROWS) break;
        bool owned = map->owner_id == sim->player.id;
        bool archived = CcSimMapIsArchived(sim, map);
        int y = 154 + row * 51;
        Color paper_color = map->contraband ? CC_STYLE_CONTRABAND_SHADOW :
                                              CC_STYLE_PARCHMENT_SHADOW;
        if (i == selected) {
            paper_color = map->contraband ? CC_STYLE_CONTRABAND :
                                            CC_STYLE_PARCHMENT;
        }
        DrawRectangleRounded((Rectangle){37.0f, (float)y, 226.0f, 43.0f},
                             0.16f, 5, paper_color);
        CcOverlayDrawText(map->name, 48, y + 7, 11, INK);
        CcOverlayDrawText(archived ? "IN GLOAMGATE ARCHIVE" :
                 owned ? "IN THE CASE" :
                 TextFormat("FOR SALE  %d C", map->ask_price),
                 48, y + 25, 9, owned ? TEAL : CC_GOLD);
        if (map->contraband) CcOverlayDrawText("UNLICENSED", 181, y + 25, 8, CC_VIOLET);
        row += 1;
    }
    if (VisibleMapCount(sim) > CC_MAP_LIST_ROWS) {
        CcOverlayDrawText(TextFormat("%d-%d OF %d", visible_start + 1,
                            visible_start + row, VisibleMapCount(sim)),
                 48, 570, 9, MUTED);
    }
    if (row == 0) CcOverlayDrawText("No charts are present at this stop.", 42, 172, 11, MUTED);

    const CcMap *map = SelectedVisibleMap(sim, selected);
    Rectangle paper = {282.0f, 105.0f, 620.0f, 525.0f};
    DrawRectangleRounded(paper, 0.025f, 4,
                         map != NULL && map->contraband ?
                         CC_STYLE_CONTRABAND_LIGHT :
                         CC_STYLE_PARCHMENT_LIGHT);
    DrawRectangleRoundedLinesEx(paper, 0.025f, 4, 2.0f,
                                Fade(CC_STYLE_PARCHMENT, 0.86f));
    if (map == NULL) {
        CcOverlayDrawText("NO MAP SELECTED", 452, 335, 22,
                          CC_STYLE_PARCHMENT);
        return;
    }
    if (DrawCollectibleMapArt(sim, map, illustrated_map,
                              collectible_atlas)) return;
    const CcRoute *route = CcSimRoute(sim, map->route_id);
    const CcSettlement *from = route != NULL ? CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ? CcSimSettlement(sim, route->to_id) : NULL;
    Color chart_ink = map->contraband ? CC_STYLE_CONTRABAND_SHADOW :
                                       CC_STYLE_PARCHMENT_SHADOW;
    char route_title[96];
    MapRouteTitle(sim, map, route_title, sizeof(route_title));
    CcOverlayDrawText(route_title, 307, 125, 20, chart_ink);
    CcOverlayDrawText(TextFormat("Updated day %d  /  Accuracy %d%%",
                        map->surveyed_day, map->accuracy), 309, 151, 10,
                        Fade(chart_ink, 0.75f));
    DrawChartRoute(map, chart_ink);
    DrawChartTown(ChartEndpoint(map, false), from,
                  from != NULL && from->id == sim->player.location_id, chart_ink);
    DrawChartTown(ChartEndpoint(map, true), to,
                  to != NULL && to->id == sim->player.location_id, chart_ink);

    Vector2 compass = {847.0f, 173.0f};
    DrawCircleLinesV(compass, 24.0f, Fade(chart_ink, 0.45f));
    float tilt = ((float)((map->id >> 12U) % 31U) - 15.0f) * 0.012f;
    DrawLineEx(compass, (Vector2){compass.x + sinf(tilt) * 21.0f,
                                  compass.y - cosf(tilt) * 21.0f}, 2.0f, chart_ink);
    CcOverlayDrawText("N", 841, 202, 10, chart_ink);
    if (map->contraband) {
        CcOverlayDrawText("Unlicensed copy", 309, 596, 10,
                          Fade(chart_ink, 0.74f));
    }
}

static void DrawSettlementPanel(const CcSim *sim, int32_t selected)
{
    Rectangle panel = {938.0f, 82.0f, 322.0f, 310.0f};
    DrawPanel(panel, PANEL);
    const CcMap *map = SelectedVisibleMap(sim, selected);
    const CcSettlement *here = CcSimSettlement(sim, sim->player.location_id);
    CcOverlayDrawText("CARTOGRAPHER'S CASE", 958, 102, 12, TEAL);
    CcOverlayDrawText(here != NULL ? here->name : "Unknown stop", 958, 125, 22, INK);
    CcOverlayDrawText(TextFormat("CROWNS %" PRId64 "   CASE %d/%d", sim->player.coins,
                        CcPlayerMapCount(sim), sim->player.map_capacity),
             958, 154, 10, MUTED);
    CcOverlayDrawText(TextFormat("CATALOGUE %d/%d",
                        CcPlayerMapCollectionCount(sim),
                        CC_MAP_COLLECTION_COUNT),
             958, 171, 10, MUTED);
    if (map == NULL) {
        CcOverlayDrawText("NO MAP", 958, 143, 15, MUTED);
        return;
    }
    const CcRoute *route = CcSimRoute(sim, map->route_id);
    const CcSettlement *from = route != NULL ? CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ? CcSimSettlement(sim, route->to_id) : NULL;
    const CcSettlement *maker = CcSimSettlement(sim, map->maker_settlement_id);
    CcOverlayDrawText("THIS OBJECT DEPICTS", 958, 193, 10, MUTED);
    CcOverlayDrawText(from != NULL ? from->name : "Unknown", 958, 214, 16, INK);
    CcOverlayDrawText("TO", 958, 235, 9, MUTED);
    CcOverlayDrawText(to != NULL ? to->name : "Unknown", 958, 252, 16, INK);
    CcOverlayDrawText(TextFormat("MAKER  %s", maker != NULL ? maker->name : "unknown"),
             958, 289, 10, MUTED);
    CcOverlayDrawText(TextFormat("AGE    %d days", sim->current_day - map->surveyed_day),
             958, 308, 10, MUTED);
    DrawBar(958, 337, 124, "ACCURACY", map->accuracy, TEAL);
    DrawBar(958, 361, 124, "ROAD INK", map->recorded_condition, CC_GOLD);
    DrawBar(958, 385, 124, "DANGER", map->recorded_danger, DANGER);
    bool owned = map->owner_id == sim->player.id;
    bool archived = CcSimMapIsArchived(sim, map);
    if (!owned) {
        CcOverlayDrawText(TextFormat("B  BUY FOR %d CROWNS", map->ask_price),
                 958, 486, 13, CC_GOLD);
    } else if (archived) {
        CcOverlayDrawText("A  RETRIEVE FROM ARCHIVE",
                 958, 486, 12, TEAL);
        CcOverlayDrawText(TextFormat("S  SELL FOR %d CROWNS",
                            map->ask_price * 2 / 3),
                 958, 511, 11, MUTED);
    } else {
        CcOverlayDrawText(TextFormat("S  SELL FOR %d CROWNS", map->ask_price * 2 / 3),
                 958, 486, 11, MUTED);
        if (sim->player.location_id == sim->settlements[1].id) {
            CcOverlayDrawText("A  STORE IN GLOAMGATE ARCHIVE",
                     958, 511, 10, TEAL);
        }
    }
    CcOverlayDrawText("LEFT/RIGHT  leaf through objects", 958, 584, 9, MUTED);
    CcOverlayDrawText("M  close case   Q situations", 958, 604, 9, MUTED);
}

static void DrawRoadHeader(const CcSim *sim)
{
    const CcSettlement *here = CcSimSettlement(sim, sim->player.location_id);
    CcOverlayDrawText(TextFormat("ROAD FROM %s",
                                 here != NULL ? here->name : "HERE"),
                      26, 22, 22, INK);
    CcOverlayDrawText(TextFormat("DAY %d     %" PRId64 " cr     NOTES %d/%d",
                        sim->current_day, sim->player.coins,
                        CcPlayerMapCount(sim), sim->player.map_capacity),
                      995, 27, 9, CC_GOLD);
}

static void DrawMapHeader(const CcSim *sim)
{
    CcOverlayDrawText("MAP CASE", 26, 22, 22, INK);
    CcOverlayDrawText(TextFormat("DAY %d     %" PRId64 " cr     MAPS %d/%d",
                        sim->current_day, sim->player.coins,
                        CcPlayerMapCount(sim), sim->player.map_capacity),
                      1000, 27, 9, CC_GOLD);
}

static void DrawLedger(const CcSim *sim)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.67f));
    Rectangle bounds = {350.0f, 205.0f, 580.0f, 300.0f};
    DrawPanel(bounds, PANEL_DEEP);
    CcOverlayDrawText("NEWS", 382, 237, 22, INK);
    int32_t shown = sim->event_count < 3 ? sim->event_count : 3;
    for (int32_t i = 0; i < shown; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) continue;
        int y = 292 + i * 54;
        CcOverlayDrawText(TextFormat("DAY %d", event->day), 382, y, 9,
                 CC_GOLD);
        CcOverlayDrawText(CcEventKindName(event->kind), 458, y, 11,
                 event->kind == CC_EVENT_MONSTER_PRESSURE ? CC_VIOLET : TEAL);
    }
    if (shown == 0) CcOverlayDrawText("No news.", 382, 292, 12, MUTED);
}

static Color SituationColor(CcSituationKind kind)
{
    if (kind == CC_SITUATION_RELIEF_DELIVERY) return TEAL;
    if (kind == CC_SITUATION_ROUTE_REPAIR) return CC_GOLD;
    if (kind == CC_SITUATION_MONSTER_EXPEDITION) return CC_VIOLET;
    return DANGER;
}

static const char *SituationTitle(CcSituationKind kind)
{
    switch (kind) {
        case CC_SITUATION_RELIEF_DELIVERY: return "Deliver supplies";
        case CC_SITUATION_ROUTE_REPAIR: return "Repair a road";
        case CC_SITUATION_MONSTER_EXPEDITION: return "Clear a threat";
        case CC_SITUATION_BLACK_MARKET_DELIVERY: return "Secret delivery";
        default: return "Quest";
    }
}

static void DrawSituationBoard(const CcSim *sim, int32_t selected)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.67f));
    Rectangle bounds = {330.0f, 190.0f, 620.0f, 340.0f};
    DrawPanel(bounds, PANEL_DEEP);
    const CcSituation *detail = SelectedActiveSituation(sim, selected);
    int32_t active_count = 0;
    int32_t active_ordinal = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status != CC_SITUATION_ACTIVE) continue;
        if (i == selected) active_ordinal = active_count;
        active_count += 1;
    }
    CcOverlayDrawText("QUEST", 360, 216, 10, TEAL);
    CcOverlayDrawText(active_count > 0 ?
             TextFormat("%d / %d", active_ordinal + 1, active_count) : "0 / 0",
             874, 216, 10, CC_GOLD);
    if (detail != NULL) {
        char target[96];
        SituationTargetLabel(sim, detail, target, sizeof(target));
        CcOverlayDrawText(SituationTitle(detail->kind), 360, 250, 21,
                 SituationColor(detail->kind));
        CcOverlayDrawText(TextFormat("%s  /  %s", target,
                            detail->affected_name[0] != '\0' ?
                                detail->affected_name : "Someone waiting"),
                 360, 284, 11, INK);
        char next[192];
        SituationNextAction(sim, detail, next, sizeof(next));
        CcOverlayDrawText("NEXT STEP", 360, 323, 9, MUTED);
        DrawTwoLineText(next, 360, 346, 58U, 11, CC_GOLD);
        CcOverlayDrawText(TextFormat("DUE DAY %d", detail->deadline_day),
                 360, 408, 10, MUTED);
        CcOverlayDrawText(TextFormat("REWARD  +%" PRId64 " CROWNS", detail->reward),
                 714, 408, 10, TEAL);
    } else {
        CcOverlayDrawText("NO QUESTS", 360, 268, 17, MUTED);
    }
}

static void DrawJourneyEncounter(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED) return;
    const CcSettlement *from = CcSimSettlement(sim, sim->journey.origin_id);
    const CcSettlement *to = CcSimSettlement(sim, sim->journey.destination_id);
    const CcBanditGroup *bandits = CcSimBanditGroupOnRoute(
        sim, sim->journey.route_id);
    CcGood demanded_good = CC_GOOD_FOOD;
    int32_t demanded_quantity = 0;
    bool has_demand = CcSimBanditProvisionDemand(
        sim, sim->journey.route_id, &demanded_good, &demanded_quantity);
    int32_t reaction = CcSimBanditReactionRoll(
        sim, sim->journey.route_id);
    int32_t combat_damage = 7 + sim->journey.danger / 8;
    if (combat_damage < 8) combat_damage = 8;
    if (combat_damage > 20) combat_damage = 20;
    int32_t wound_cost = 3 + sim->journey.danger / 12;
    if ((CcMoney)wound_cost > sim->player.coins) {
        wound_cost = (int32_t)sim->player.coins;
    }
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.56f));
    DrawPanel((Rectangle){350.0f, 170.0f, 580.0f, 320.0f},
              PANEL_DEEP);
    CcOverlayDrawText("A COMPANY HOLDS THE ROAD", 386, 199, 20, INK);
    CcOverlayDrawText(TextFormat("%s  ->  %s", from != NULL ? from->name : "Origin",
                        to != NULL ? to->name : "Destination"),
             386, 236, 11, CC_GOLD);
    CcOverlayDrawText(bandits != NULL ? bandits->name : "Road company",
                      386, 273, 17, DANGER);
    if (bandits != NULL) {
        CcOverlayDrawText(
            TextFormat("%s  /  %d PEOPLE  /  SUPPLIES %d",
                       CcBanditCampSizeName(bandits->camp_size),
                       bandits->members, bandits->supplies),
            386, 304, 10, MUTED);
        CcOverlayDrawText(
            TextFormat("REACTION %d  /  %s", reaction,
                       CcBanditReactionName(reaction)),
            386, 329, 10, reaction <= 5 ? DANGER : TEAL);
        const char *pressure = bandits->raid_phase != CC_BANDIT_RAID_IDLE ?
            TextFormat("%s FOR %s",
                       CcBanditRaidPhaseName(bandits->raid_phase),
                       CcGoodName(bandits->raid_good)) :
            bandits->supplies < 30 ? "SHORT STORES. THE CAMP IS HUNGRY." :
            bandits->influence >= 70 ?
                "THEY CLAIM THIS BORDER AS THEIR OWN." :
                "THEIR NO-MAN'S-LAND CAMP NEEDS SUPPLY.";
        CcOverlayDrawText(pressure, 386, 354, 10, INK);
    }
    if (has_demand) {
        CcOverlayDrawText(
            TextFormat("DEMAND  %d %s  OR  %d CROWNS",
                       demanded_quantity, CcGoodName(demanded_good),
                       sim->journey.bargain_cost),
            386, 393, 12, TEAL);
    }
    CcOverlayDrawText(
        TextFormat("IF YOU WIN  /  CARRIAGE -%d  /  WOUNDS %d CROWNS",
                   combat_damage, wound_cost),
        386, 425, 10, DANGER);
    CcOverlayDrawText(
        "You may withdraw. Under fire, retreat also costs condition and care.",
        386, 455, 9, MUTED);
}

static bool ApplyCommand(CcJournal *journal, CcSim *sim, CcCommand command,
                         char *message, size_t message_capacity)
{
    char error[192];
    bool applied = journal != NULL ?
        CcJournalApply(journal, sim, &command, error, sizeof(error)) :
        CcSimApply(sim, &command, error, sizeof(error));
    if (!applied) {
        (void)snprintf(message, message_capacity, "%s", error);
        return false;
    }
    const char *confirmation = "Done.";
    switch (command.kind) {
        case CC_COMMAND_TRADE:
            confirmation = command.amount > 0 ? "Cargo bought." :
                                                "Cargo delivered.";
            break;
        case CC_COMMAND_TRAVEL: confirmation = "Journey started."; break;
        case CC_COMMAND_REPAIR_ROUTE: confirmation = "Road repaired."; break;
        case CC_COMMAND_CHANGE_DUNGEON: confirmation = "Mine updated."; break;
        case CC_COMMAND_BUY_MAP: confirmation = "Traveller's notes bought."; break;
        case CC_COMMAND_SELL_MAP: confirmation = "Traveller's notes sold."; break;
        case CC_COMMAND_ACCEPT_SITUATION: confirmation = "Quest accepted."; break;
        case CC_COMMAND_ABANDON_SITUATION: confirmation = "Quest abandoned."; break;
        case CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT:
            confirmation = "Road clear.";
            break;
        case CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE:
            confirmation = "Toll paid.";
            break;
        case CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS:
            confirmation = "Provisions accepted.";
            break;
        case CC_COMMAND_WITHDRAW_ENCOUNTER:
            confirmation = "The carriage withdrew.";
            break;
        default: break;
    }
    (void)snprintf(message, message_capacity, "%s", confirmation);
    return true;
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

static void PrepareTownRaidReel(CcSim *sim, LocalState *local)
{
    if (!local->course.alarm_active) {
        CcLocalCourseBindRaiderCompany(&local->course, sim);
        CcLocalCourseRaiseAlarmNear(&local->course, &local->agent);
    }
    (void)CcLocalCourseSelectPlayerTarget(
        &local->course, &local->agent, 0);
    /* This is still the live raid simulation. Fast-forward only the long
       ingress so the reel opens when the selected raider reaches the same
       local combat radius used by normal play. */
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

static void PrepareRoadCombatReel(CcSim *sim, LocalState *local)
{
    BeginRoadLocalState(sim, local, true);
    (void)CcLocalCourseSelectPlayerTarget(
        &local->course, &local->agent, 0);
    /* Keep the real road fight, but skip most of the uneventful run-in. The
       shown shot now starts just outside weapon range, before the first hit. */
    for (int32_t frame = 0; frame < 900; ++frame) {
        float x = local->course.raiders[0].position.x -
                  local->agent.position.x;
        float z = local->course.raiders[0].position.z -
                  local->agent.position.z;
        if (x * x + z * z <= 2.40f * 2.40f) break;
        (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                 1.0f / 60.0f, false, true);
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
                    &local->agent, (Vector3){49.20f, 0.0f, 27.15f}, false);
                GameplayReelSetStage(reel, GAMEPLAY_REEL_WALK_TO_MARKET);
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
                           "Bring the cargo to the carriage.");
            (void)CcLocalWorldUpdate(&local->course, &local->agent, sim,
                                     delta_time, false, true);
            if ((reel->stage_frame >= 60 &&
                 GridDistance(LocalPosition(local), LOCAL_CARRIAGE) < 1.35f) ||
                reel->stage_frame >= 1200) {
                *selected = FirstOutgoingRouteIndex(sim);
                reel->destination_id = GameplayReelDestination(sim, selected);
                *return_view = VIEW_LOCAL;
                *view = VIEW_ROADS;
                GameplayReelSetStage(reel, GAMEPLAY_REEL_CHOOSE_ROUTE);
            }
            break;
        case GAMEPLAY_REEL_CHOOSE_ROUTE:
            (void)snprintf(message, message_capacity,
                           "A branch lies ahead.");
            if (reel->stage_frame >= 150 && !reel->stage_started &&
                reel->destination_id != 0U) {
                CcCommand travel = {
                    .kind = CC_COMMAND_TRAVEL,
                    .target_id = reel->destination_id
                };
                reel->stage_started = true;
                if (ApplyCommand(NULL, sim, travel, message,
                                 message_capacity)) {
                    reel->journey_legs += 1;
                    BeginRoadTravelState(local);
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
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                local->journey_travel_active = false;
                *view = VIEW_ENCOUNTER;
                GameplayReelSetStage(reel, GAMEPLAY_REEL_ROAD_CHOICE);
            } else if (!sim->journey.active) {
                const CcSituation *accepted = CcSimAcceptedSituation(sim);
                CcId target = SituationSettlementId(sim, accepted);
                ResetLocalState(local);
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
                    BeginRoadTravelState(local);
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
            if (!sim->journey.active) {
                const CcSituation *accepted = CcSimAcceptedSituation(sim);
                CcId target = SituationSettlementId(sim, accepted);
                ResetLocalState(local);
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

static void HandleExpedition(CcJournal *journal, CcSim *sim,
                             const CcDungeon *dungeon,
                             char *message, size_t message_capacity)
{
    if (dungeon == NULL) return;
    CcDungeonState next = dungeon->state == CC_DUNGEON_DISTURBED ?
                          CC_DUNGEON_EXPLORED :
                          dungeon->state == CC_DUNGEON_EXPLORED ?
                          CC_DUNGEON_PUBLIC_ROUTE : CC_DUNGEON_RESEALED;
    CcCommand expedition = {
        .kind = CC_COMMAND_CHANGE_DUNGEON,
        .target_id = dungeon->id,
        .dungeon_state = next
    };
    (void)ApplyCommand(journal, sim, expedition, message, message_capacity);
}

static void HandleInput(CcJournal **journal, CcSim *sim, int32_t *selected,
                        int32_t *selected_situation, ClientView *view,
                        ClientView *return_view, LocalState *local,
                        RenderTexture2D local_target, Rectangle local_bounds,
                        float delta_time,
                        const char *save_path, char *message, size_t message_capacity)
{
    CommandActionKind command_action = PressedCommandAction(local);
    ContextActionKind context_action = PressedContextAction(
        sim, local, *view, *selected, *selected_situation);
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if (command_action == COMMAND_ACTION_SAVE || ClientKeyPressed(KEY_F5) ||
        queued_save_shortcut || (control && ClientKeyPressed(KEY_S))) {
        char error[256];
        bool saved = false;
        if (*journal == NULL) {
            *journal = CcJournalStart(save_path, sim, error, sizeof(error));
            saved = *journal != NULL;
        } else {
            saved = CcJournalCheckpoint(*journal, sim, error, sizeof(error));
        }
        (void)snprintf(message, message_capacity, "%s",
                       saved ? "Game saved." : error);
        return;
    }
    if (*view == VIEW_ENCOUNTER) {
        if (!sim->journey.active ||
            sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED) {
            *view = VIEW_LOCAL;
            return;
        }
        if (ClientKeyPressed(KEY_ONE) ||
            context_action == CONTEXT_ACTION_FIGHT) {
            BeginRoadLocalState(sim, local, true);
            *view = VIEW_LOCAL;
            (void)snprintf(message, message_capacity,
                           "Break the company line.");
        } else if (ClientKeyPressed(KEY_TWO) ||
                   context_action == CONTEXT_ACTION_PAY) {
            BeginRoadLocalState(sim, local, false);
            *view = VIEW_LOCAL;
            (void)snprintf(message, message_capacity,
                           "Walk to the captain and hear the demand.");
        }
        return;
    }
    if (context_action == CONTEXT_ACTION_CLOSE_VIEW &&
        (*view == VIEW_LEDGER || *view == VIEW_SITUATIONS)) {
        *view = *return_view;
        return;
    }
    if (ClientKeyPressed(KEY_TAB) ||
        command_action == COMMAND_ACTION_LEDGER) {
        if (*view == VIEW_LEDGER) {
            *view = *return_view;
        } else {
            *return_view = *view;
            *view = VIEW_LEDGER;
        }
        return;
    }
    bool road_local = local->journey_travel_active ||
                      local->journey_combat_active ||
                      local->journey_parley_active;
    bool quests_requested = ClientKeyPressed(KEY_Q) ||
                            command_action == COMMAND_ACTION_QUESTS;
    if (quests_requested && road_local) {
        (void)snprintf(message, message_capacity,
                       local->journey_travel_active ?
                           "Quests are unavailable while travelling." :
                           "Finish the fight first.");
        return;
    }
    if (quests_requested) {
        if (*view == VIEW_SITUATIONS) {
            *view = *return_view;
        } else {
            *return_view = *view;
            if (SelectedActiveSituation(sim, *selected_situation) == NULL) {
                *selected_situation = FirstActiveSituationIndex(sim);
            }
            *view = VIEW_SITUATIONS;
        }
        return;
    }
    bool map_requested = ClientKeyPressed(KEY_M) ||
                         command_action == COMMAND_ACTION_MAP;
    if (map_requested && road_local) {
        (void)snprintf(message, message_capacity,
                       local->journey_travel_active ?
                           "Map case unavailable while travelling." :
                           "Finish the fight first.");
        return;
    }
    if (map_requested) {
        if (*view == VIEW_MAP) {
            *view = VIEW_LOCAL;
            *selected = FirstOutgoingRouteIndex(sim);
        } else if (*view == VIEW_LOCAL && !local->market_interior &&
                   GridDistance(LocalPosition(local), LOCAL_CARRIAGE) < 1.35f) {
            *selected = FirstVisibleMapIndex(sim);
            *view = VIEW_MAP;
        } else {
            (void)snprintf(message, message_capacity,
                           "Walk closer to the carriage.");
        }
        return;
    }
    if (*view == VIEW_SITUATIONS) {
        if (SelectedActiveSituation(sim, *selected_situation) == NULL) {
            *selected_situation = FirstActiveSituationIndex(sim);
        }
        if (ClientKeyPressed(KEY_DOWN) || ClientKeyPressed(KEY_RIGHT) ||
            context_action == CONTEXT_ACTION_NEXT_PROMISE) {
            *selected_situation = StepActiveSituationIndex(
                sim, *selected_situation, 1);
        }
        if (ClientKeyPressed(KEY_UP) || ClientKeyPressed(KEY_LEFT)) {
            *selected_situation = StepActiveSituationIndex(
                sim, *selected_situation, -1);
        }
        const CcSituation *situation = SelectedActiveSituation(
            sim, *selected_situation);
        if ((ClientKeyPressed(KEY_ENTER) ||
             context_action == CONTEXT_ACTION_ACCEPT_PROMISE) &&
            situation != NULL) {
            CcCommand accept = {
                .kind = CC_COMMAND_ACCEPT_SITUATION,
                .target_id = situation->id
            };
            (void)ApplyCommand(*journal, sim, accept, message,
                               message_capacity);
        }
        if ((ClientKeyPressed(KEY_BACKSPACE) ||
             context_action == CONTEXT_ACTION_ABANDON_PROMISE) &&
            CcSimAcceptedSituation(sim) != NULL) {
            CcCommand abandon = {
                .kind = CC_COMMAND_ABANDON_SITUATION,
                .target_id = sim->player.accepted_situation_id
            };
            (void)ApplyCommand(*journal, sim, abandon, message,
                               message_capacity);
        }
        return;
    }
    if (*view == VIEW_LEDGER) return;

    if (ClientKeyPressed(KEY_F9)) {
        char error[256];
        if (!CcJournalClose(journal, sim, error, sizeof(error))) {
            (void)snprintf(message, message_capacity, "%s", error);
            return;
        }
        *journal = CcJournalResume(save_path, sim, error, sizeof(error));
        bool loaded = *journal != NULL;
        (void)snprintf(message, message_capacity, "%s",
                       loaded ?
                           "Game loaded." :
                           error);
        if (loaded) {
            *selected = FirstOutgoingRouteIndex(sim);
            *selected_situation = FirstActiveSituationIndex(sim);
            CcLocalTerrainSetSeed(sim->world_seed);
            ResetLocalState(local);
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                *view = VIEW_ENCOUNTER;
            } else {
                if (sim->journey.active) BeginRoadTravelState(local);
                *view = VIEW_LOCAL;
            }
        }
    }
    if (ClientKeyPressed(KEY_N)) {
        char error[256];
        if (!CcJournalClose(journal, sim, error, sizeof(error))) {
            (void)snprintf(message, message_capacity, "%s", error);
            return;
        }
        CcSimInit(sim, sim->world_seed + UINT32_C(0x9e3779b9));
        *selected = FirstOutgoingRouteIndex(sim);
        *selected_situation = FirstActiveSituationIndex(sim);
        CcLocalTerrainSetSeed(sim->world_seed);
        ResetLocalState(local);
        *view = VIEW_LOCAL;
        (void)snprintf(message, message_capacity, "New game started.");
    }
    if (ClientKeyPressed(KEY_PERIOD) && !sim->journey.active) {
        char error[256];
        bool advanced = *journal != NULL ?
            CcJournalAdvanceDays(*journal, sim, 1, error, sizeof(error)) :
            (CcSimAdvanceDays(sim, 1), true);
        (void)snprintf(message, message_capacity, "%s",
                       advanced ?
                           "One day passed." :
                           error);
    }
    if (ClientKeyPressed(KEY_K) && !sim->journey.active) {
        char error[256];
        bool advanced = *journal != NULL ?
            CcJournalAdvanceDays(*journal, sim, 7, error, sizeof(error)) :
            (CcSimAdvanceDays(sim, 7), true);
        (void)snprintf(message, message_capacity, "%s",
                       advanced ?
                           "One week passed." :
                           error);
    }

    if (*view == VIEW_LOCAL) {
        if (local->journey_travel_active) {
            int32_t ticks = CcLocalWorldUpdate(
                &local->course, &local->agent, sim, delta_time,
                false, false);
            char error[256];
            bool advanced = *journal != NULL ?
                CcJournalAdvanceRuntimeTicks(*journal, sim, ticks,
                                             error, sizeof(error)) :
                (CcSimAdvanceRuntimeTicks(sim, ticks), true);
            if (!advanced) {
                (void)snprintf(message, message_capacity, "%s", error);
                return;
            }
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                local->journey_travel_active = false;
                *view = VIEW_ENCOUNTER;
                const CcEvent *event = CcSimRecentEvent(sim, 0);
                (void)snprintf(message, message_capacity, "%s",
                               event != NULL ? event->text :
                               "An outlaw company blocked the road.");
            } else if (!sim->journey.active) {
                *selected = FirstOutgoingRouteIndex(sim);
                ResetLocalState(local);
                const CcEvent *event = CcSimRecentEvent(sim, 0);
                (void)snprintf(message, message_capacity, "%s",
                               event != NULL ? event->text :
                               "Arrived.");
            }
            return;
        }
        if (local->journey_combat_active &&
            (context_action == CONTEXT_ACTION_WITHDRAW ||
             ClientKeyPressed(KEY_BACKSPACE))) {
            bool under_fire = local->agent.combat.health <
                                  CC_LOCAL_COMBAT_MAX_HEALTH ||
                              local->course.raider_resolve <
                                  local->course.raider_initial_resolve;
            CcCommand withdraw = {
                .kind = CC_COMMAND_WITHDRAW_ENCOUNTER,
                .amount = under_fire ? 1 : 0
            };
            if (ApplyCommand(*journal, sim, withdraw, message,
                             message_capacity)) {
                CcAthleticProfile athletics = local->agent.athletics;
                ResetLocalState(local);
                local->agent.athletics = athletics;
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_LOCAL;
            }
            return;
        }
        if ((ClientKeyPressed(KEY_J) ||
             context_action == CONTEXT_ACTION_JUMP) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED) {
            bool jumped = CcLocalAgentJump(&local->agent);
            (void)snprintf(
                message, message_capacity, "%s",
                jumped ? "Jump." : "Can't jump now.");
        }
        if ((ClientKeyPressed(KEY_X) ||
             context_action == CONTEXT_ACTION_TOGGLE_GUARD) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED &&
            !local->journey_parley_active) {
            bool guarded = !local->agent.humanoid.guard_requested;
            CcLocalCourseSetPlayerGuarded(&local->course, &local->agent,
                                          guarded);
            (void)snprintf(
                message, message_capacity, "%s",
                guarded ? "Guard raised — posture absorbs the next blow." :
                          "Guard lowered — movement and attacks are open.");
        }
        if ((ClientKeyPressed(KEY_SPACE) ||
             context_action == CONTEXT_ACTION_BASIC_STRIKE) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED &&
            !local->journey_parley_active) {
            int32_t target = SelectedCombatTargetIndex(local);
            bool struck = CcLocalCourseBeginPlayerStrike(
                &local->course, &local->agent);
            (void)snprintf(
                message, message_capacity, "%s",
                struck && target >= 0 ?
                    TextFormat("Striking %s.",
                               local->course.raider_names[target]) :
                target < 0 ? "Choose an outlaw before attacking." :
                             "Recovering — wait for an opening.");
        }
        if (local->journey_combat_active ||
            (!local->market_interior && local->course.alarm_active)) {
            for (int32_t skill = 0; skill < CC_COMBAT_SKILL_COUNT; ++skill) {
                ContextActionKind skill_action =
                    skill == CC_COMBAT_SKILL_CRUSHING_BLOW ?
                        CONTEXT_ACTION_SKILL_CRUSHING :
                    skill == CC_COMBAT_SKILL_SUNDER ?
                        CONTEXT_ACTION_SKILL_SUNDER :
                        CONTEXT_ACTION_SKILL_SECOND_WIND;
                if (!ClientKeyPressed(KEY_ONE + skill) &&
                    context_action != skill_action) continue;
                CcCombatSkill combat_skill = (CcCombatSkill)skill;
                bool used = CcLocalCourseUsePlayerSkill(
                    &local->course, &local->agent, combat_skill);
                float cooldown = CcLocalCombatSkillCooldown(
                    &local->agent, combat_skill);
                (void)snprintf(
                    message, message_capacity, "%s",
                    used && combat_skill == CC_COMBAT_SKILL_SECOND_WIND ?
                        "Second wind restored your posture." :
                    used ? TextFormat("%s committed.",
                                      CcLocalCombatSkillName(combat_skill)) :
                    cooldown > 0.0f ?
                        TextFormat("%s ready in %.1fs.",
                                   CcLocalCombatSkillName(combat_skill),
                                   cooldown) :
                    combat_skill == CC_COMBAT_SKILL_SECOND_WIND ?
                        "Posture is already full." :
                        "Choose an outlaw before using that skill.");
            }
        }
        if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            int32_t combat_target = local->market_interior ? -1 :
                CcLocalCoursePickPlayerTarget(
                    &local->course, &local->agent, mouse, local_target,
                    local_bounds);
            if (combat_target >= 0) {
                local->movement_reticle = mouse;
                local->movement_reticle_age = 0.0f;
                local->movement_reticle_valid = true;
                local->movement_reticle_accepted = true;
                (void)snprintf(
                    message, message_capacity,
                    "Focused: %s / %s.",
                    local->course.raider_names[combat_target],
                    CcLocalRaiderRoleName(
                        local->course.raider_roles[combat_target]));
            } else {
                bool movement_accepted = CcLocalAgentPickTarget(
                    &local->agent, mouse, local_target, local_bounds,
                    local->market_interior);
                bool in_local_view = CheckCollisionPointRec(
                    mouse, local_bounds);
                if (in_local_view) {
                    local->movement_reticle = mouse;
                    local->movement_reticle_age = 0.0f;
                    local->movement_reticle_valid = true;
                    local->movement_reticle_accepted = movement_accepted;
                }
                if (movement_accepted) {
                    if (!local->market_interior &&
                        local->course.alarm_active) {
                        CcLocalCourseClearPlayerTarget(&local->agent);
                    }
                    const char *navigation =
                        CcLocalAgentNavigationName(&local->agent);
                    if (navigation != NULL) {
                        (void)snprintf(
                            message, message_capacity,
                            "Going to %s.",
                            navigation);
                    } else {
                        message[0] = '\0';
                    }
                } else if (in_local_view) {
                    (void)snprintf(
                        message, message_capacity,
                        "Can't walk there.");
                }
            }
        }
        bool advance_course = !local->market_interior &&
                              !local->journey_parley_active;
        (void)CcLocalWorldUpdate(
            &local->course, &local->agent, sim, delta_time,
            local->market_interior, advance_course);
        if (local->movement_reticle_valid) {
            local->movement_reticle_age += delta_time;
            float reticle_lifetime = local->movement_reticle_accepted ?
                                     0.48f : 0.75f;
            if (local->movement_reticle_age > reticle_lifetime) {
                local->movement_reticle_valid = false;
            }
        }
        if (!local->market_interior &&
            CcLocalAgentConsumeWorldExit(&local->agent)) {
            *selected = FirstOutgoingRouteIndex(sim);
            *view = VIEW_ROADS;
            (void)snprintf(
                message, message_capacity,
                "The first branch is ahead.");
            return;
        }
        if (local->journey_combat_active &&
            local->agent.combat.life_state == CC_LIFE_DEAD &&
            sim->journey.active) {
            CcCommand defeated = {
                .kind = CC_COMMAND_WITHDRAW_ENCOUNTER,
                .amount = 1
            };
            if (ApplyCommand(*journal, sim, defeated, message,
                             message_capacity)) {
                CcAthleticProfile athletics = local->agent.athletics;
                ResetLocalState(local);
                local->agent.athletics = athletics;
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_LOCAL;
                (void)snprintf(message, message_capacity,
                               "Defeated. The carriage withdrew under fire.");
            }
            return;
        }
        if (local->journey_parley_active) {
            Vector2 collector = {CC_LOCAL_ROAD_PARLEY_X,
                                 CC_LOCAL_ROAD_PARLEY_Z};
            if (ClientKeyPressed(KEY_BACKSPACE) ||
                context_action == CONTEXT_ACTION_RETURN_TO_CHOICE) {
                ResetLocalState(local);
                *view = VIEW_ENCOUNTER;
                (void)snprintf(message, message_capacity,
                               "Back at the carriage.");
                return;
            }
            if (context_action == CONTEXT_ACTION_OFFER_PROVISIONS &&
                GridDistance(LocalPosition(local), collector) < 1.55f) {
                CcCommand offer = {
                    .kind = CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS
                };
                if (ApplyCommand(*journal, sim, offer, message,
                                 message_capacity)) {
                    *selected = FirstVisibleMapIndex(sim);
                    BeginRoadTravelState(local);
                    *view = VIEW_LOCAL;
                }
                return;
            }
            if ((ClientKeyPressed(KEY_F) ||
                 context_action == CONTEXT_ACTION_PAY_COLLECTOR) &&
                GridDistance(LocalPosition(local), collector) < 1.55f) {
                CcCommand negotiate = {
                    .kind = CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE
                };
                if (ApplyCommand(*journal, sim, negotiate, message,
                                 message_capacity)) {
                    *selected = FirstOutgoingRouteIndex(sim);
                    BeginRoadTravelState(local);
                    *view = VIEW_LOCAL;
                }
            }
            return;
        }
        if (local->journey_combat_active) {
            if (local->course.defenses_completed > 0 &&
                sim->journey.active) {
                CcCommand victory = {
                    .kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT
                };
                if (ApplyCommand(*journal, sim, victory, message,
                                 message_capacity)) {
                    *selected = FirstOutgoingRouteIndex(sim);
                    BeginRoadTravelState(local);
                    *view = VIEW_LOCAL;
                }
            }
            return;
        }
        if (!local->market_interior) {
            if (ClientKeyPressed(KEY_G) ||
                context_action == CONTEXT_ACTION_RAISE_ALARM) {
                if (!local->course.alarm_active) {
                    CcLocalCourseBindRaiderCompany(&local->course, sim);
                    CcLocalCourseRaiseAlarmNear(&local->course,
                                                &local->agent);
                    (void)snprintf(message, message_capacity,
                                   "%s is approaching.",
                                   local->course.raider_company_name);
                } else {
                    (void)snprintf(message, message_capacity,
                                   "The alarm is already active.");
                }
            }
        }
        Vector2 position = LocalPosition(local);

        bool interact = ClientKeyPressed(KEY_F);
        if (interact || context_action != CONTEXT_ACTION_NONE) {
            if ((interact ||
                 context_action == CONTEXT_ACTION_LEAVE_MARKET) &&
                local->market_interior &&
                GridDistance(position, INTERIOR_EXIT) < 1.25f) {
                local->market_interior = false;
                RepositionHero(local,
                               (Vector2){CC_LOCAL_MARKET_X,
                                         CC_LOCAL_MARKET_Z + 1.10f}, false);
                message[0] = '\0';
            } else if ((interact ||
                        context_action == CONTEXT_ACTION_ENTER_MARKET) &&
                       !local->market_interior &&
                       GridDistance(position, LOCAL_MARKET) < 1.30f) {
                local->market_interior = true;
                RepositionHero(local, (Vector2){2.05f, 5.35f}, true);
                message[0] = '\0';
            } else if ((interact ||
                        context_action == CONTEXT_ACTION_CHOOSE_ROAD) &&
                       !local->market_interior &&
                       GridDistance(position, LOCAL_CARRIAGE) < 1.35f) {
                *selected = FirstOutgoingRouteIndex(sim);
                *view = VIEW_ROADS;
                message[0] = '\0';
            } else if (context_action == CONTEXT_ACTION_OPEN_MAP &&
                       !local->market_interior &&
                       GridDistance(position, LOCAL_CARRIAGE) < 1.35f) {
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_MAP;
                message[0] = '\0';
            } else if ((interact ||
                        context_action == CONTEXT_ACTION_OPEN_PROMISES) &&
                       !local->market_interior &&
                       GridDistance(position, LOCAL_NOTICE) < 1.15f) {
                *return_view = VIEW_LOCAL;
                if (SelectedActiveSituation(sim, *selected_situation) == NULL) {
                    *selected_situation = FirstActiveSituationIndex(sim);
                }
                *view = VIEW_SITUATIONS;
            }
        }

        bool can_trade = local->market_interior &&
                         GridDistance(position, INTERIOR_COUNTER) < 2.25f;
        if (can_trade) {
            CcGood context_good = ContextCargoGood(sim);
            if (context_action == CONTEXT_ACTION_DELIVER_CARGO) {
                const CcSituation *accepted = CcSimAcceptedSituation(sim);
                if (accepted != NULL) {
                    int32_t amount = accepted->quantity - accepted->progress;
                    if (amount < 1) amount = 1;
                    if (amount > sim->player.cargo[context_good]) {
                        amount = sim->player.cargo[context_good];
                    }
                    CcCommand trade = {
                        .kind = CC_COMMAND_TRADE,
                        .good = context_good,
                        .amount = -amount
                    };
                    (void)ApplyCommand(*journal, sim, trade, message,
                                       message_capacity);
                }
            } else if (context_action == CONTEXT_ACTION_BUY_CARGO ||
                       context_action == CONTEXT_ACTION_SELL_CARGO) {
                CcCommand trade = {
                    .kind = CC_COMMAND_TRADE,
                    .good = context_good,
                    .amount = context_action == CONTEXT_ACTION_BUY_CARGO ?
                                  1 : -1
                };
                (void)ApplyCommand(*journal, sim, trade, message,
                                   message_capacity);
            }
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                if (!ClientKeyPressed(KEY_ONE + good)) continue;
                CcCommand trade = {
                    .kind = CC_COMMAND_TRADE,
                    .good = (CcGood)good,
                    .amount = shift ? -1 : 1
                };
                (void)ApplyCommand(*journal, sim, trade, message,
                                   message_capacity);
            }
        }
        const CcDungeon *dungeon = DungeonAtSettlement(sim, sim->player.location_id);
        if (!local->market_interior && dungeon != NULL &&
            (ClientKeyPressed(KEY_E) ||
             context_action == CONTEXT_ACTION_EXPEDITION) &&
            GridDistance(position, LOCAL_DUNGEON) < 1.35f) {
            HandleExpedition(*journal, sim, dungeon, message,
                             message_capacity);
        }
        return;
    }

    if (*view == VIEW_MAP) {
        if (context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            *view = VIEW_LOCAL;
            *selected = FirstOutgoingRouteIndex(sim);
            return;
        }
        if (ClientKeyPressed(KEY_RIGHT) || ClientKeyPressed(KEY_DOWN)) {
            *selected = StepVisibleMapIndex(sim, *selected, 1);
        }
        if (ClientKeyPressed(KEY_LEFT) || ClientKeyPressed(KEY_UP)) {
            *selected = StepVisibleMapIndex(sim, *selected, -1);
        }
        if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            int32_t visible_start = VisibleMapListStart(sim, *selected);
            int32_t visible_rank = 0;
            int32_t row = 0;
            for (int32_t i = 0; i < sim->map_count; ++i) {
                if (!MapVisibleAtCarriage(sim, &sim->maps[i])) continue;
                if (visible_rank++ < visible_start) continue;
                if (row >= CC_MAP_LIST_ROWS) break;
                Rectangle item = {
                    37.0f, (float)(154 + row * 51), 226.0f, 43.0f
                };
                if (CheckCollisionPointRec(mouse, item)) *selected = i;
                row += 1;
            }
        }

        const CcMap *selected_map = SelectedVisibleMap(sim, *selected);
        if (selected_map == NULL) return;
        CcId map_id = selected_map->id;
        if ((ClientKeyPressed(KEY_B) ||
             context_action == CONTEXT_ACTION_BUY_MAP) &&
            selected_map->owner_id == sim->player.location_id) {
            CcCommand buy = {
                .kind = CC_COMMAND_BUY_MAP,
                .target_id = map_id
            };
            (void)ApplyCommand(*journal, sim, buy, message,
                               message_capacity);
            return;
        }
        if (ClientKeyPressed(KEY_S) &&
            selected_map->owner_id == sim->player.id) {
            CcCommand sell = {
                .kind = CC_COMMAND_SELL_MAP,
                .target_id = map_id
            };
            (void)ApplyCommand(*journal, sim, sell, message,
                               message_capacity);
            return;
        }
        if (ClientKeyPressed(KEY_A) &&
            selected_map->owner_id == sim->player.id) {
            CcCommand archive = {
                .kind = CcSimMapIsArchived(sim, selected_map) ?
                    CC_COMMAND_RETRIEVE_MAP : CC_COMMAND_ARCHIVE_MAP,
                .target_id = map_id
            };
            (void)ApplyCommand(*journal, sim, archive, message,
                               message_capacity);
        }
        return;
    }

    if (context_action == CONTEXT_ACTION_CLOSE_VIEW) {
        *view = VIEW_LOCAL;
        return;
    }
    bool moved_to_next_branch = false;
    if (ClientKeyPressed(KEY_RIGHT) || ClientKeyPressed(KEY_DOWN) ||
        context_action == CONTEXT_ACTION_NEXT_BRANCH) {
        *selected = StepOutgoingRouteIndex(sim, *selected, 1);
        moved_to_next_branch = true;
    }
    if (ClientKeyPressed(KEY_LEFT) || ClientKeyPressed(KEY_UP)) {
        *selected = StepOutgoingRouteIndex(sim, *selected, -1);
        moved_to_next_branch = true;
    }
    if (moved_to_next_branch) {
        (void)snprintf(message, message_capacity,
                       "The carriage reaches the next branch.");
        return;
    }

    const CcRoute *route = SelectedOutgoingRoute(sim, *selected);
    if (route == NULL) return;
    const CcMap *map = VisibleMapForRoute(sim, route->id);
    if (map != NULL &&
        (ClientKeyPressed(KEY_B) ||
         context_action == CONTEXT_ACTION_BUY_MAP) &&
        map->owner_id == sim->player.location_id) {
        CcCommand buy = {.kind = CC_COMMAND_BUY_MAP, .target_id = map->id};
        (void)ApplyCommand(*journal, sim, buy, message, message_capacity);
        return;
    }
    if (map != NULL && ClientKeyPressed(KEY_S) &&
        map->owner_id == sim->player.id) {
        CcCommand sell = {.kind = CC_COMMAND_SELL_MAP, .target_id = map->id};
        (void)ApplyCommand(*journal, sim, sell, message, message_capacity);
        return;
    }
    CcId destination_id = RouteOtherEnd(route, sim->player.location_id);
    if ((ClientKeyPressed(KEY_ENTER) ||
         context_action == CONTEXT_ACTION_TRAVEL) &&
        destination_id != 0U) {
        CcCommand travel = {
            .kind = CC_COMMAND_TRAVEL,
            .target_id = destination_id
        };
        if (ApplyCommand(*journal, sim, travel, message, message_capacity)) {
            *selected = FirstOutgoingRouteIndex(sim);
            BeginRoadTravelState(local);
            *view = VIEW_LOCAL;
        }
    }
    if ((ClientKeyPressed(KEY_R) ||
         context_action == CONTEXT_ACTION_REPAIR_ROUTE) &&
        destination_id != 0U) {
        CcCommand repair = {
            .kind = CC_COMMAND_REPAIR_ROUTE,
            .target_id = route->id
        };
        (void)ApplyCommand(*journal, sim, repair, message, message_capacity);
    }
}

static CcLocalAtmospherePreset LocalAtmosphereForSimulation(
    const CcSim *sim)
{
    if (sim != NULL && !sim->dragon.slain &&
        sim->dragon.omen_days_remaining > 0 &&
        sim->dragon.retaliation_target_id == sim->player.location_id) {
        return CC_LOCAL_ATMOSPHERE_DRAGON_OMEN;
    }
    return CcLocalAtmosphereForDay(sim != NULL ? sim->current_day : 0);
}

int main(int argc, char **argv)
{
    bool screen_first_hero = true;
    for (int32_t argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--screen-first-hero") == 0) {
            screen_first_hero = true;
        } else if (strcmp(argv[argument], "--old-hero") == 0) {
            screen_first_hero = false;
        }
    }
    bool render_benchmark = argc >= 2 &&
                            strcmp(argv[1], "--benchmark-render") == 0;
    int32_t render_benchmark_frames = 600;
    double render_benchmark_minimum_fps = 0.0;
    if (render_benchmark && argc >= 3) {
        char *end = NULL;
        long parsed = strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0' || parsed <= 0 || parsed > INT32_MAX) {
            (void)fprintf(stderr, "Render benchmark frame count is invalid.\n");
            return 1;
        }
        render_benchmark_frames = (int32_t)parsed;
    }
    if (render_benchmark && argc >= 4) {
        char *end = NULL;
        double parsed = strtod(argv[3], &end);
        if (end == argv[3] || *end != '\0' || parsed <= 0.0) {
            (void)fprintf(stderr,
                          "Render benchmark minimum FPS is invalid.\n");
            return 1;
        }
        render_benchmark_minimum_fps = parsed;
    }
    bool capture_board = argc >= 2 && strcmp(argv[1], "--capture-board") == 0;
    bool capture_interior = argc >= 2 && strcmp(argv[1], "--capture-interior") == 0;
    bool capture_navigation = argc >= 2 && strcmp(argv[1], "--capture-nav") == 0;
    bool capture_defense = argc >= 2 &&
                           strcmp(argv[1], "--capture-defense") == 0;
    bool capture_downclimb = argc >= 2 &&
                             strcmp(argv[1], "--capture-downclimb") == 0;
    bool capture_limbs = argc >= 2 && strcmp(argv[1], "--capture-limbs") == 0;
    bool capture_walk_cycle = argc >= 2 &&
                              strcmp(argv[1], "--capture-walk-cycle") == 0;
    bool capture_road_fork = argc >= 2 &&
        strcmp(argv[1], "--capture-road-fork") == 0;
    bool capture_map_case = argc >= 2 &&
        strcmp(argv[1], "--capture-map-case") == 0;
    bool capture_dojo = argc >= 2 && strcmp(argv[1], "--capture-dojo") == 0;
    bool capture_jump = argc >= 2 && strcmp(argv[1], "--capture-jump") == 0;
    bool capture_action_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-action-reel") == 0;
    bool capture_gameplay_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-gameplay-reel") == 0;
    bool capture_encounter = argc >= 2 &&
        strcmp(argv[1], "--capture-encounter") == 0;
    bool capture_witness = argc >= 2 &&
        strcmp(argv[1], "--capture-witness") == 0;
    bool capture_travel = argc >= 2 &&
        strcmp(argv[1], "--capture-travel") == 0;
    bool capture_road = argc >= 2 &&
        strcmp(argv[1], "--capture-road") == 0;
    bool capture_parley = argc >= 2 &&
        strcmp(argv[1], "--capture-parley") == 0;
    bool capture_aftermath = argc >= 2 &&
        strcmp(argv[1], "--capture-aftermath") == 0;
    bool capture_golden = argc >= 2 &&
        strcmp(argv[1], "--capture-golden") == 0;
    bool capture_atmosphere = argc >= 2 &&
        strcmp(argv[1], "--capture-atmosphere") == 0;
    CcLocalAtmospherePreset capture_atmosphere_preset =
        CC_LOCAL_ATMOSPHERE_CLEAR_DAY;
    if (capture_atmosphere) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "capture atmosphere requires a mood and a frame path.\n");
            return 1;
        }
        if (strcmp(argv[2], "clear") == 0) {
            capture_atmosphere_preset = CC_LOCAL_ATMOSPHERE_CLEAR_DAY;
        } else if (strcmp(argv[2], "rain") == 0) {
            capture_atmosphere_preset =
                CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST;
        } else if (strcmp(argv[2], "dusk") == 0) {
            capture_atmosphere_preset = CC_LOCAL_ATMOSPHERE_AMBER_DUSK;
        } else if (strcmp(argv[2], "night") == 0) {
            capture_atmosphere_preset =
                CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT;
        } else if (strcmp(argv[2], "omen") == 0) {
            capture_atmosphere_preset = CC_LOCAL_ATMOSPHERE_DRAGON_OMEN;
        } else {
            (void)fprintf(stderr,
                          "atmosphere must be clear, rain, dusk, night, or omen.\n");
            return 1;
        }
    }
    bool capture_face = argc >= 2 &&
        strcmp(argv[1], "--capture-face") == 0;
    bool capture_creatures = argc >= 2 &&
        strcmp(argv[1], "--capture-creatures") == 0;
    bool capture_creature_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-creature-reel") == 0;
    bool capture_creature_media = capture_creatures || capture_creature_reel;
    const char *capture_creature_family = NULL;
    if (capture_creature_media) {
        if (argc < 4 ||
            (strcmp(argv[2], "goblins") != 0 &&
             strcmp(argv[2], "dragon") != 0 &&
             strcmp(argv[2], "animals") != 0 &&
             strcmp(argv[2], "horse") != 0 &&
             strcmp(argv[2], "cow") != 0)) {
            (void)fprintf(stderr,
                          "creature capture requires goblins, dragon, horse, "
                          "cow, or animals and a frame path.\n");
            return 1;
        }
        capture_creature_family = argv[2];
    }
    bool capture_creature_horse = capture_creature_media &&
        strcmp(capture_creature_family, "horse") == 0;
    int32_t capture_face_view = -1;
    if (capture_face) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "capture face requires an angle and a frame path.\n");
            return 1;
        }
        if (strcmp(argv[2], "front") == 0) capture_face_view = 0;
        else if (strcmp(argv[2], "three-quarter") == 0) {
            capture_face_view = 1;
        } else if (strcmp(argv[2], "profile") == 0) {
            capture_face_view = 2;
        } else if (strcmp(argv[2], "distant") == 0) {
            capture_face_view = 3;
        } else if (strcmp(argv[2], "back") == 0) {
            capture_face_view = 4;
        } else {
            (void)fprintf(stderr,
                          "capture face angle must be front, three-quarter, profile, back, or distant.\n");
            return 1;
        }
    }
    bool capture_room = argc >= 2 &&
        strcmp(argv[1], "--capture-room") == 0;
    float capture_room_x = 0.0f;
    float capture_room_z = 0.0f;
    if (capture_room) {
        if (argc < 5) {
            (void)fprintf(stderr,
                          "capture room requires X, Z, and a frame path.\n");
            return 1;
        }
        char *x_end = NULL;
        char *z_end = NULL;
        capture_room_x = strtof(argv[2], &x_end);
        capture_room_z = strtof(argv[3], &z_end);
        if (x_end == argv[2] || *x_end != '\0' ||
            z_end == argv[3] || *z_end != '\0' ||
            capture_room_x < 0.5f ||
            capture_room_x > CC_LOCAL_WORLD_WIDTH - 0.5f ||
            capture_room_z < 0.5f ||
            capture_room_z > CC_LOCAL_WORLD_DEPTH - 0.5f) {
            (void)fprintf(stderr, "capture room coordinates are invalid.\n");
            return 1;
        }
    }
    bool capture = argc >= 2 &&
                   (strcmp(argv[1], "--capture") == 0 || capture_board ||
                    capture_interior || capture_navigation || capture_limbs ||
                    capture_walk_cycle || capture_defense ||
                    capture_downclimb || capture_road_fork ||
                    capture_map_case || capture_dojo ||
                    capture_jump || capture_action_reel ||
                    capture_gameplay_reel || capture_encounter ||
                    capture_witness || capture_travel || capture_road ||
                    capture_parley ||
                    capture_aftermath || capture_golden ||
                    capture_atmosphere || capture_face ||
                    capture_room || capture_creature_media);
    const char *capture_path = capture_creature_media ? argv[3] :
                               capture_room ? argv[4] :
                               capture_face ? argv[3] :
                               capture_atmosphere ? argv[3] :
                               argc >= 3 ? argv[2] :
                               "architecture-proof.png";
    char save_path[640];
    CampaignSavePath(save_path, sizeof(save_path));

    if (capture || render_benchmark) SetTraceLogLevel(LOG_WARNING);
    /* The world is deliberately rasterized at its final art-pixel
       resolution. Multisample coverage before that raster step makes thin
       edges change blend weights as actors move and reads as temporal
       flicker after nearest-neighbor enlargement. */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE |
                   (capture ? FLAG_WINDOW_HIDDEN : 0U));
    InitWindow(1280, 760, "Crownless Carriage — living world spine");
    if (!IsWindowReady()) {
        (void)fprintf(stderr,
                      "Crownless Carriage could not connect to the desktop window server.\n");
        return 1;
    }
    ClientInputInstall();
    SetWindowMinSize(1080, 680);
    SetTargetFPS(render_benchmark || capture_action_reel ||
                 capture_gameplay_reel || capture_creature_reel ? 0 : 60);
    Texture2D illustrated_map = {0};
    Texture2D collectible_atlas = {0};
    char illustrated_map_path[1024];
    if (ResolveClientAssetPath(CC_GLOAMGATE_ALDERWATCH_MAP_ASSET,
                               illustrated_map_path,
                               sizeof(illustrated_map_path))) {
        illustrated_map = LoadTexture(illustrated_map_path);
        SetTextureFilter(illustrated_map, TEXTURE_FILTER_BILINEAR);
    }
    char collectible_atlas_path[1024];
    if (ResolveClientAssetPath(CC_COLLECTIBLE_MAP_ATLAS_ASSET,
                               collectible_atlas_path,
                               sizeof(collectible_atlas_path))) {
        collectible_atlas = LoadTexture(collectible_atlas_path);
        SetTextureFilter(collectible_atlas, TEXTURE_FILTER_BILINEAR);
    }
    /* The playable world is authored against a fixed 2x art-pixel grid.
       Render it at half the presentation size, then enlarge with point
       sampling. Screen-space labels and HUD are drawn after presentation. */
    RenderTexture2D local_target = LoadRenderTexture(630, 320);
    SetTextureFilter(local_target.texture, TEXTURE_FILTER_POINT);
    CcLocalRendererSetScreenFirstHero(screen_first_hero);
    CcLocalRendererInit();
    CcLocalRendererSetDiagnosticOverlay(capture_limbs);

    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcLocalTerrainSetSeed(sim.world_seed);
    CcJournal *journal = NULL;
    if (capture || render_benchmark) CcSimAdvanceDays(&sim, 28);
    if (capture_creature_media &&
        strcmp(capture_creature_family, "goblins") == 0) {
        sim.player.location_id = sim.goblins.lair_settlement_id;
    } else if (capture_creature_media &&
               strcmp(capture_creature_family, "dragon") == 0) {
        sim.player.location_id = sim.dragon.lair_settlement_id;
        sim.dragon.omen_days_remaining = 2;
        sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
    } else if (capture_creature_media) {
        for (int32_t settlement = 0; settlement < sim.settlement_count;
             ++settlement) {
            if (sim.settlements[settlement].id != sim.player.location_id) {
                continue;
            }
            sim.settlements[settlement].stock[CC_GOOD_FOOD] = 32;
            break;
        }
    }
    if (capture_road_fork || capture_map_case) {
        sim.player.location_id = sim.settlements[1].id;
    }
    if (capture_witness) {
        for (int32_t situation = 0; situation < sim.situation_count;
             ++situation) {
            if (sim.situations[situation].status != CC_SITUATION_ACTIVE) {
                continue;
            }
            bool found = false;
            for (int32_t settlement = 0;
                 settlement < sim.settlement_count; ++settlement) {
                if (!CcSimSituationTouchesSettlement(
                        &sim, &sim.situations[situation],
                        sim.settlements[settlement].id)) continue;
                sim.player.location_id = sim.settlements[settlement].id;
                found = true;
                break;
            }
            if (found) break;
        }
    }
    if (capture_encounter || capture_travel || capture_road || capture_parley ||
        capture_aftermath || capture_creature_horse) {
        int32_t charter_index = FirstActiveSituationIndex(&sim);
        if (charter_index >= 0) {
            char setup_error[192];
            CcSituation *capture_charter = &sim.situations[charter_index];
            capture_charter->kind = CC_SITUATION_RELIEF_DELIVERY;
            capture_charter->target_id = sim.settlements[1].id;
            capture_charter->good = CC_GOOD_FOOD;
            capture_charter->quantity = 1;
            capture_charter->progress = 0;
            sim.player.cargo[CC_GOOD_FOOD] = 1;
            sim.routes[0].closed = true;
            if (sim.bandit_count > 0) {
                sim.bandits[0].route_id = sim.routes[0].id;
            }
            CcCommand accept = {
                .kind = CC_COMMAND_ACCEPT_SITUATION,
                .target_id = capture_charter->id
            };
            (void)CcSimApply(&sim, &accept, setup_error,
                             sizeof(setup_error));
            CcCommand travel = {
                .kind = CC_COMMAND_TRAVEL,
                .target_id = sim.settlements[1].id
            };
            (void)CcSimApply(&sim, &travel, setup_error,
                             sizeof(setup_error));
            if (capture_travel || capture_creature_horse) {
                while (sim.journey.active &&
                       sim.carriage.progress_milli < 200) {
                    CcSimAdvanceRuntimeTicks(
                        &sim, CC_WORLD_TICKS_PER_SECOND);
                }
            } else {
                while (sim.journey.active &&
                       sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
                    CcSimAdvanceRuntimeTicks(
                        &sim, CC_WORLD_TICKS_PER_SECOND);
                }
            }
            if (capture_aftermath && sim.journey.active) {
                CcCommand resolve = {
                    .kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT
                };
                (void)CcSimApply(&sim, &resolve, setup_error,
                                 sizeof(setup_error));
                while (sim.journey.active) {
                    CcSimAdvanceRuntimeTicks(
                        &sim, CC_WORLD_TICKS_PER_SECOND);
                }
            }
        }
    }
    int32_t selected = capture_map_case ?
        CC_MAP_GLOAMGATE_NIGHT_ROAD : FirstOutgoingRouteIndex(&sim);
    int32_t selected_situation = FirstActiveSituationIndex(&sim);
    ClientView view = capture_board ? VIEW_SITUATIONS :
                      capture_encounter ? VIEW_ENCOUNTER :
                      capture_road_fork ? VIEW_ROADS :
                      capture_map_case ? VIEW_MAP : VIEW_LOCAL;
    ClientView return_view = VIEW_LOCAL;
    LocalState local;
    CcLocalAgent walk_cycle_frames[8] = {0};
    uint32_t walk_cycle_mask = 0;
    ActionReelState action_reel = {0};
    GameplayReelState gameplay_reel = {0};
    ResetLocalState(&local);
    if (capture_golden || capture_atmosphere) {
        RepositionHero(&local, (Vector2){44.25f, 28.85f}, false);
        local.agent.facing_yaw = -0.35f;
        local.course.alarm_countdown = 1000.0f;
    }
    if (capture_face) {
        if (capture_face_view == 3) {
            RepositionHero(&local, (Vector2){64.0f, 31.0f}, false);
            local.agent.facing_yaw = -0.32f;
        } else {
            local.market_interior = true;
            CcLocalAgentInit(&local.agent, (Vector2){4.60f, 5.10f}, true);
            CcLocalCombatSetTeam(&local.agent, CC_COMBAT_PLAYER);
            local.agent.facing_yaw = 0.14f +
                (capture_face_view == 1 ? -0.38f :
                 capture_face_view == 2 ? 1.42f :
                 capture_face_view == 4 ? PI : 0.0f);
        }
        CcLocalAgentSetMorphology(&local.agent, CC_MORPHOLOGY_BIPED,
                                  local.market_interior);
        local.course.alarm_countdown = 1000.0f;
    }
    if (capture_room) {
        RepositionHero(&local,
                       (Vector2){capture_room_x, capture_room_z}, false);
        local.agent.facing_yaw = -0.18f;
        local.course.alarm_countdown = 1000.0f;
    }
    if (capture_creature_media) {
        if (capture_creature_horse) {
            BeginRoadTravelState(&local);
        } else {
            bool animal_view =
                strcmp(capture_creature_family, "animals") == 0 ||
                strcmp(capture_creature_family, "cow") == 0;
            Vector2 creature_view = animal_view ?
                (Vector2){59.5f, 40.0f} : (Vector2){24.5f, 49.5f};
            RepositionHero(&local, creature_view, false);
            local.agent.facing_yaw = -0.18f;
            local.course.alarm_countdown = 1000.0f;
        }
    }
    if (capture_road || capture_parley) {
        BeginRoadLocalState(&sim, &local, capture_road);
    }
    if (capture_travel) BeginRoadTravelState(&local);
    if (capture && !capture_interior && !capture_walk_cycle &&
        !capture_jump && !capture_defense && !capture_downclimb &&
        !capture_navigation && !capture_limbs && !capture_dojo &&
        !capture_action_reel && !capture_gameplay_reel &&
        !capture_encounter && !capture_travel &&
        !capture_road &&
        !capture_parley && !capture_golden && !capture_atmosphere &&
        !capture_face && !capture_room && !capture_creature_media) {
        local.course.alarm_countdown = 1000.0f;
        for (int32_t frame = 0; frame < 1500; ++frame) {
            CcLocalCourseUpdate(&local.course, &local.agent, &sim,
                                1.0f / 60.0f);
        }
    }
    if (capture_jump) {
        local.course.alarm_countdown = 1000.0f;
        local.agent.facing_yaw = 0.25f * PI;
        (void)CcLocalAgentJump(&local.agent);
        for (int32_t frame = 0; frame < 27; ++frame) {
            CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, false);
        }
    }
    if (capture_action_reel) PrepareActionReel(&local, &action_reel);
    if (capture_gameplay_reel) {
        PrepareGameplayReel(&sim, &local, &gameplay_reel, &selected,
                            &selected_situation, &view, &return_view);
    }
    if (capture_downclimb) {
        CcLocalAgentInit(&local.agent, (Vector2){3.50f, 6.20f}, false);
        (void)CcLocalAgentSetExactTarget(
            &local.agent, (Vector3){3.50f, 0.0f, 7.50f}, false);
        for (int32_t frame = 0; frame < 1200; ++frame) {
            CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, false);
            if (!local.agent.exact_target_valid && local.agent.grounded &&
                fabsf(local.agent.position.y - 1.65f) < 0.01f) break;
        }
        (void)CcLocalAgentSetExactTarget(
            &local.agent, (Vector3){3.50f, 0.0f, 6.20f}, false);
        for (int32_t frame = 0; frame < 600; ++frame) {
            CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, false);
            if (local.agent.climbing_down &&
                local.agent.climb_progress >= 0.55f) break;
        }
    }
    if (capture_defense) {
        CcLocalCourseBindRaiderCompany(&local.course, &sim);
        CcLocalCourseRaiseAlarmNear(&local.course, &local.agent);
        (void)CcLocalCourseSelectPlayerTarget(&local.course,
                                               &local.agent, 0);
        (void)CcLocalCourseUsePlayerSkill(
            &local.course, &local.agent, CC_COMBAT_SKILL_CRUSHING_BLOW);
        int32_t fighting_frames = 0;
        bool captured_skill_impact = false;
        for (int32_t frame = 0; frame < 9000 && !captured_skill_impact;
             ++frame) {
            CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, false);
            CcLocalCourseUpdate(&local.course, &local.agent, &sim,
                                1.0f / 60.0f);
            fighting_frames = local.course.alarm_active &&
                              !local.course.raiders_retreating &&
                              local.course.raider_resolve <
                                  local.course.raider_initial_resolve ?
                              fighting_frames + 1 : 0;
            captured_skill_impact =
                local.agent.humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
                local.agent.combat.active_skill ==
                    CC_COMBAT_SKILL_CRUSHING_BLOW &&
                local.agent.humanoid.action_time >= 0.54f;
        }
        (void)printf("defense capture: alarm %d retreat %d resolve %d wins %d fight frames %d skill impact %d origin %.2f,%.2f guards %.2f,%.2f raiders %.2f,%.2f\n",
                     local.course.alarm_active,
                     local.course.raiders_retreating,
                     local.course.raider_resolve,
                     local.course.defenses_completed, fighting_frames,
                     captured_skill_impact,
                     local.course.combat_origin.x,
                     local.course.combat_origin.z,
                     local.course.runners[0].agent.position.x,
                     local.course.runners[0].agent.position.z,
                     local.course.raiders[0].position.x,
                     local.course.raiders[0].position.z);
    }
    if (capture_dojo) {
        local.course.alarm_countdown = 1000.0f;
        CcLocalCourseRunner *swimmer = &local.course.runners[0];
        CcLocalAgentInit(&swimmer->agent, (Vector2){13.90f, 9.78f}, false);
        swimmer->agent.crowned = false;
        swimmer->agent.tunic_color = swimmer->marker_color;
        (void)CcLocalAgentSetExactTarget(
            &swimmer->agent, (Vector3){9.40f, 0.0f, 9.72f}, false);
        for (int32_t frame = 0; frame < 420; ++frame) {
            CcLocalCourseUpdate(&local.course, &local.agent, &sim,
                                1.0f / 60.0f);
            if (swimmer->agent.swimming && frame > 90) break;
        }
    }
    if (capture_interior) {
        local.market_interior = true;
        CcLocalAgentInit(&local.agent, (Vector2){4.60f, 5.10f}, true);
    }
    if (capture_limbs) {
        CcLocalAgentSetMorphology(&local.agent, CC_MORPHOLOGY_BIPED, false);
        if (CcLocalAgentSetExactTarget(&local.agent,
                                       (Vector3){5.10f, 0.0f, 4.80f}, false)) {
            for (int32_t frame = 0; frame < 120; ++frame) {
                CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, false);
                if (frame > 30 &&
                    (local.agent.humanoid.feet[0].contact ==
                         CC_HUMANOID_CONTACT_SWING ||
                     local.agent.humanoid.feet[1].contact ==
                         CC_HUMANOID_CONTACT_SWING)) break;
            }
        }
    }
    if (capture_walk_cycle) {
        local.market_interior = true;
        CcLocalAgentInit(&local.agent, (Vector2){4.00f, 5.50f}, true);
        (void)CcLocalAgentSetExactTarget(&local.agent,
                                         (Vector3){8.00f, 0.0f, 5.50f}, true);
        for (int32_t frame = 0; frame < 1200 && walk_cycle_mask != UINT32_C(0xff);
             ++frame) {
            CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, true);
            float speed = sqrtf(local.agent.velocity.x * local.agent.velocity.x +
                                local.agent.velocity.z * local.agent.velocity.z);
            int32_t bin = (int32_t)floorf(local.agent.humanoid.phase * 8.0f) & 7;
            uint32_t bit = UINT32_C(1) << bin;
            if (speed > 0.25f && (walk_cycle_mask & bit) == 0) {
                walk_cycle_frames[bin] = local.agent;
                walk_cycle_mask |= bit;
            }
        }
        if (walk_cycle_mask != UINT32_C(0xff)) {
            (void)fprintf(stderr, "could not sample every walk pose (mask 0x%02x)\n",
                          walk_cycle_mask);
            CcLocalRendererShutdown();
            UnloadRenderTexture(local_target);
            if (illustrated_map.id != 0U) UnloadTexture(illustrated_map);
            if (collectible_atlas.id != 0U) UnloadTexture(collectible_atlas);
            CloseWindow();
            return 1;
        }
        local.agent = walk_cycle_frames[0];
    }
    if (capture_navigation &&
        CcLocalAgentSetExactTarget(&local.agent, (Vector3){3.50f, 0.0f, 7.50f},
                                   false)) {
        for (int32_t frame = 0; frame < 600; ++frame) {
            CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, false);
            if (!local.agent.grounded && local.agent.position.y > 0.65f) break;
        }
    }
    char message[256] = "";
    if (capture_golden) {
        (void)snprintf(message, sizeof(message), "Town square.");
    } else if (capture_atmosphere) {
        (void)snprintf(message, sizeof(message), "%s.",
                       CcLocalAtmosphereName(capture_atmosphere_preset));
    } else if (capture_face) {
        (void)snprintf(message, sizeof(message),
                       "Same character model.");
    } else if (capture_room) {
        (void)snprintf(message, sizeof(message), "Town view.");
    } else if (capture_creature_media) {
        (void)snprintf(message, sizeof(message), "Creature settlement.");
    }
    if (capture_road) {
        (void)snprintf(message, sizeof(message), "Break the company line.");
    } else if (capture_parley) {
        (void)snprintf(message, sizeof(message), "Speak with the captain.");
    } else if (capture_travel) {
        (void)snprintf(message, sizeof(message), "Travelling.");
    }
    int capture_frames = 0;
    int walk_frame_count = 0;
    int32_t render_benchmark_count = 0;
    double render_benchmark_started = GetTime();
    bool performance_overlay = false;
    float message_age = 0.0f;

    CcLocalRendererSetAtmosphere(
        capture_atmosphere ? capture_atmosphere_preset :
            LocalAtmosphereForSimulation(&sim),
        0.0f);

    Rectangle local_bounds = {10.0f, 54.0f, 1260.0f, 640.0f};
    while (!WindowShouldClose()) {
        float frame_delta_time = GetFrameTime();
        char previous_message[sizeof(message)];
        (void)snprintf(previous_message, sizeof(previous_message), "%s",
                       message);
        CcLocalRendererBeginFrame(frame_delta_time);
        CcLocalRendererSetAtmosphere(
            capture_atmosphere ? capture_atmosphere_preset :
                LocalAtmosphereForSimulation(&sim),
            2.4f);
        CcLocalRendererUpdateAtmosphere(frame_delta_time);
        if (!capture && ClientKeyPressed(KEY_F3)) {
            performance_overlay = !performance_overlay;
            (void)snprintf(message, sizeof(message), "%s",
                           performance_overlay ?
                           "Performance overlay enabled." :
                           "Performance overlay hidden.");
        }
        if (capture_walk_cycle) {
            local.agent = walk_cycle_frames[walk_frame_count];
        } else if (capture_gameplay_reel) {
            for (int32_t step = 0;
                 step < 4 && !gameplay_reel.complete; ++step) {
                UpdateGameplayReel(&sim, &local, &gameplay_reel,
                                   &selected, &selected_situation,
                                   &view, &return_view, message,
                                   sizeof(message));
            }
        } else if (capture_action_reel) {
            for (int32_t step = 0; step < 4 && !action_reel.complete; ++step) {
                UpdateActionReel(&local, &action_reel, message,
                                 sizeof(message));
            }
        } else {
            HandleInput(&journal, &sim, &selected, &selected_situation,
                        &view, &return_view, &local,
                        local_target, local_bounds,
                        frame_delta_time,
                        save_path, message, sizeof(message));
            ClientInputClearPressed();
        }
        if (strcmp(previous_message, message) != 0) {
            message_age = 0.0f;
        } else {
            message_age += capture_gameplay_reel ? 1.0f / 15.0f :
                           frame_delta_time;
        }
        float clock = capture_gameplay_reel ?
            (float)gameplay_reel.captured_frames / 15.0f :
            capture_creature_reel ? (float)capture_frames / 15.0f :
            (float)GetTime();

        BeginDrawing();
        ClearBackground(BACKGROUND);
        CcOverlayBegin(1.0f);
        bool road_choice_underlay = view == VIEW_ROADS ||
                            ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
                             return_view == VIEW_ROADS);
        if (road_choice_underlay) {
            CcLocalDrawFork3D(&sim, selected, clock,
                              local_target, local_bounds);
            DrawRoadHeader(&sim);
            DrawRoadPanel(&sim, selected);
        } else if (view == VIEW_MAP ||
                   ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
                    return_view == VIEW_MAP)) {
            DrawMapHeader(&sim);
            DrawMap(&sim, selected, clock, illustrated_map,
                    collectible_atlas);
            DrawSettlementPanel(&sim, selected);
        } else {
            if (view == VIEW_ENCOUNTER) {
                CcLocalDrawRoad3D(&sim, &local.agent, &local.course,
                                  false, false, clock,
                                  local_target, local_bounds);
            } else if (local.journey_travel_active ||
                local.journey_combat_active ||
                local.journey_parley_active) {
                CcLocalDrawRoad3D(&sim, &local.agent, &local.course,
                                  local.journey_travel_active,
                                  local.journey_parley_active, clock,
                                  local_target, local_bounds);
            } else if (local.market_interior) {
                CcLocalDrawMarket3D(&sim, &local.agent, clock,
                                    local_target, local_bounds);
            } else {
                CcLocalDrawStreet3D(&sim, &local.agent, &local.course, clock,
                                    local_target, local_bounds);
            }
            if (view != VIEW_ENCOUNTER) {
                DrawLocalMovementReticle(&local, local_bounds);
                DrawLocalHeader(&sim, &local);
                DrawLocalPanel(&sim, &local);
            }
        }
        if (!capture_gameplay_reel && view == VIEW_LOCAL &&
            LocalCombatActive(&local)) {
            DrawCombatStatusLine(&local, message, message_age);
        }
        if (!capture_gameplay_reel && view == VIEW_LOCAL &&
            !LocalCombatActive(&local) &&
            message_age < 2.2f &&
            message[0] != '\0' &&
            !local.journey_travel_active) {
            const char *toast = TextFormat("%.48s", message);
            int width = CcOverlayMeasureText(toast, 10) + 26;
            if (width > 660) width = 660;
            float opacity = message_age > 1.6f ?
                1.0f - (message_age - 1.6f) / 0.6f : 1.0f;
            float x = ((float)GetScreenWidth() - (float)width) * 0.5f;
            float toast_y = (float)GetScreenHeight() - 107.0f;
            DrawRectangleRounded((Rectangle){x, toast_y,
                                              (float)width, 28.0f},
                                 0.22f, 5,
                                 Fade(BACKGROUND, opacity));
            CcOverlayDrawText(toast, (int)x + 13,
                              (int)toast_y + 8, 10,
                              Fade(INK, opacity));
        }
        if (view == VIEW_LEDGER) {
            CcOverlayFlush();
            DrawLedger(&sim);
        }
        if (view == VIEW_SITUATIONS) {
            CcOverlayFlush();
            DrawSituationBoard(&sim, selected_situation);
        }
        if (view == VIEW_ENCOUNTER) {
            CcOverlayFlush();
            DrawJourneyEncounter(&sim);
        }
        CcOverlayFlush();
        if (!capture_gameplay_reel ||
            gameplay_reel.stage != GAMEPLAY_REEL_QUEST_COMPLETE) {
            DrawContextActionTray(&sim, &local, view, selected,
                                  selected_situation);
        }
        DrawCommandBar(view, &local);
        if (performance_overlay) {
            CcOverlayFlush();
            DrawPerformanceOverlay();
        }
        if (capture_gameplay_reel) {
            CcOverlayFlush();
            DrawGameplayReelQuestComplete(&gameplay_reel);
            DrawGameplayReelTransition(&gameplay_reel);
        }
        CcOverlayEnd();
        EndDrawing();

        if (render_benchmark) {
            render_benchmark_count += 1;
            if (render_benchmark_count >= render_benchmark_frames) break;
        } else if (capture_gameplay_reel) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                           capture_path, gameplay_reel.captured_frames);
            TakeScreenshot(frame_path);
            gameplay_reel.captured_frames += 1;
            if (gameplay_reel.complete) break;
        } else if (capture_action_reel) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                           capture_path, action_reel.captured_frames);
            TakeScreenshot(frame_path);
            action_reel.captured_frames += 1;
            if (action_reel.complete) break;
        } else if (capture_walk_cycle) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%02d.png",
                           capture_path, walk_frame_count);
            TakeScreenshot(frame_path);
            walk_frame_count += 1;
            if (walk_frame_count >= 8) break;
        } else if (capture_creature_reel) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            int32_t creature_frame = capture_frames - 3;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                           capture_path, creature_frame);
            TakeScreenshot(frame_path);
            if (creature_frame >= 44) break;
        } else if (capture) {
            capture_frames += 1;
            int32_t settled_frames = capture_road ? 45 : 3;
            if (capture_frames >= settled_frames) {
                TakeScreenshot(capture_path);
                break;
            }
        }
    }

    char journal_error[256];
    bool journal_close_failed = !CcJournalClose(
        &journal, &sim, journal_error, sizeof(journal_error));
    if (journal_close_failed) {
        (void)fprintf(stderr, "Could not commit the action journal: %s\n",
                      journal_error);
    }
    double render_benchmark_elapsed = render_benchmark ?
        GetTime() - render_benchmark_started : 0.0;
    CcLocalRendererStats final_renderer_stats =
        CcLocalRendererGetStats();
    CcLocalRendererShutdown();
    UnloadRenderTexture(local_target);
    if (illustrated_map.id != 0U) UnloadTexture(illustrated_map);
    if (collectible_atlas.id != 0U) UnloadTexture(collectible_atlas);
    CloseWindow();
    if (render_benchmark) {
        double frames_per_second =
            (double)render_benchmark_count / render_benchmark_elapsed;
        (void)printf("render: frames=%d seconds=%.6f ms/frame=%.3f fps=%.1f skin_updates=%d skinned_meshes=%d high_detail=%d lod=%d\n",
                     render_benchmark_count, render_benchmark_elapsed,
                     render_benchmark_elapsed * 1000.0 /
                         (double)render_benchmark_count,
                     frames_per_second, final_renderer_stats.skin_updates,
                     final_renderer_stats.skinned_meshes,
                     final_renderer_stats.high_detail_characters,
                     final_renderer_stats.low_detail_characters);
        bool performance_failed = render_benchmark_minimum_fps > 0.0 &&
                                  frames_per_second <
                                      render_benchmark_minimum_fps;
        bool hero_layout_failed =
            final_renderer_stats.high_detail_characters != 1 ||
            final_renderer_stats.low_detail_characters <= 0;
        bool skin_layout_failed = screen_first_hero ?
            final_renderer_stats.skin_updates != 0 ||
                final_renderer_stats.skinned_meshes != 0 ||
                hero_layout_failed :
            final_renderer_stats.skin_updates != 1 ||
                final_renderer_stats.skinned_meshes >
                    CC_LOCAL_HERO_RUNTIME_MESH_BUDGET ||
                hero_layout_failed;
        if (performance_failed) {
            (void)fprintf(stderr,
                          "render performance budget failed: %.1f FPS < %.1f FPS\n",
                          frames_per_second, render_benchmark_minimum_fps);
        }
        if (skin_layout_failed) {
            (void)fprintf(stderr,
                          "render skin budget failed: updates=%d meshes=%d hero=%d lod=%d (mesh budget %d)\n",
                          final_renderer_stats.skin_updates,
                          final_renderer_stats.skinned_meshes,
                          final_renderer_stats.high_detail_characters,
                          final_renderer_stats.low_detail_characters,
                          CC_LOCAL_HERO_RUNTIME_MESH_BUDGET);
        }
        if (performance_failed || skin_layout_failed) return 2;
    } else if (capture_walk_cycle) {
        (void)printf("captured %d walk-cycle frames with prefix %s\n",
                     walk_frame_count, capture_path);
    } else if (capture_gameplay_reel) {
        (void)printf("captured %d real-gameplay reel frames with prefix %s\n",
                     gameplay_reel.captured_frames, capture_path);
    } else if (capture_action_reel) {
        (void)printf("captured %d heroic action-reel frames with prefix %s\n",
                     action_reel.captured_frames, capture_path);
    } else if (capture) {
        (void)printf("captured %s\n", capture_path);
    }
    return journal_close_failed ? 1 : 0;
}
