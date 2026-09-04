#include "client/cc_client_policy.h"
#include "client/cc_client_session.h"
#include "client/cc_local3d.h"
#include "client/cc_local_place.h"
#include "client/cc_overlay.h"
#include "client/cc_road_book.h"
#include "client/cc_visual_style.h"
#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "story/cc_story.h"
#include "world/cc_world.h"

#include "raylib.h"
#include "GLFW/glfw3.h"
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#include <float.h>
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
    VIEW_CARRIAGE,
    VIEW_ROADS,
    VIEW_MAP,
    VIEW_LEDGER,
    VIEW_SITUATIONS,
    VIEW_CHARACTER,
    VIEW_ENCOUNTER,
    VIEW_DUNGEON,
    VIEW_DRAGON_CAVE
} ClientView;

typedef struct ClientMapTextures {
    Texture2D illustrated;
    Texture2D collectible_atlas;
    bool illustrated_attempted;
    bool collectible_atlas_attempted;
} ClientMapTextures;

static bool IsCommandOverlay(ClientView view)
{
    return view == VIEW_LEDGER || view == VIEW_SITUATIONS;
}

static ClientView SafeOverlayReturnView(ClientView return_view)
{
    return IsCommandOverlay(return_view) ? VIEW_LOCAL : return_view;
}

static void ToggleCommandOverlay(ClientView requested,
                                 ClientView *view,
                                 ClientView *return_view)
{
    if (view == NULL || return_view == NULL) return;
    if (*view == requested) {
        *view = SafeOverlayReturnView(*return_view);
        return;
    }
    if (!IsCommandOverlay(*view)) *return_view = *view;
    *view = requested;
}

typedef struct LocalState {
    CcWorldStream world_stream;
    CcLocalAgent agent;
    CcLocalCourse course;
    CcLocalConvoyState convoy;
    CcLocalWorldCarriageState world_carriage;
    CcClientDepartureTransition departure;
    CcClientArrivalTransition arrival;
    CcLocalMovementPreview movement_preview;
    CcLocalSiteKind site_kind;
    Vector2 movement_reticle;
    float movement_reticle_age;
    float movement_preview_cooldown;
    float site_travel_progress;
    float fork_turn_progress;
    bool market_interior;
    bool site_travel_active;
    bool site_returning;
    bool road_choice_active;
    bool journey_travel_active;
    bool journey_combat_active;
    bool journey_parley_active;
    bool movement_reticle_valid;
    bool movement_reticle_accepted;
    bool open_world;
    bool open_world_market;
    CcLocalOpeningStep opening_step;
    CcId conversation_character_id;
    CcId conversation_situation_id;
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
    CONTEXT_ACTION_SKIP_TRAVEL,
    CONTEXT_ACTION_JUMP,
    CONTEXT_ACTION_RAISE_ALARM,
    CONTEXT_ACTION_SELECT_TARGET,
    CONTEXT_ACTION_BASIC_STRIKE,
    CONTEXT_ACTION_TOGGLE_GUARD,
    CONTEXT_ACTION_WITHDRAW,
    CONTEXT_ACTION_SKILL_CRUSHING,
    CONTEXT_ACTION_SKILL_SUNDER,
    CONTEXT_ACTION_SKILL_SECOND_WIND,
    CONTEXT_ACTION_STEAL_DRAGON_CROWNS,
    CONTEXT_ACTION_RETURN_DRAGON_CROWNS,
    CONTEXT_ACTION_STEAL_DRAGON_RELIC,
    CONTEXT_ACTION_RETURN_DRAGON_RELIC,
    CONTEXT_ACTION_INTERCEPT_DRAGON_TRIBUTE,
    CONTEXT_ACTION_TRAVEL_DUNGEON_SITE,
    CONTEXT_ACTION_TRAVEL_GOBLIN_SITE,
    CONTEXT_ACTION_TRAVEL_DRAGON_SITE,
    CONTEXT_ACTION_RETURN_FROM_SITE,
    CONTEXT_ACTION_GOBLIN_TRADE,
    CONTEXT_ACTION_TALK_CHARACTER,
    CONTEXT_ACTION_LISTEN_CHARACTER,
    CONTEXT_ACTION_PLEDGE_CHARACTER,
    CONTEXT_ACTION_REPORT_EVIDENCE,
    CONTEXT_ACTION_KEEP_CONFIDENCE,
    CONTEXT_ACTION_DUNGEON_MOVE,
    CONTEXT_ACTION_DUNGEON_SEARCH,
    CONTEXT_ACTION_DUNGEON_OPEN_SHORTCUT,
    CONTEXT_ACTION_DUNGEON_PARLEY,
    CONTEXT_ACTION_DUNGEON_EVADE,
    CONTEXT_ACTION_DUNGEON_FORCE,
    CONTEXT_ACTION_DUNGEON_RETREAT,
    CONTEXT_ACTION_DUNGEON_PUBLIC_ROUTE,
    CONTEXT_ACTION_DUNGEON_SMUGGLER_ROUTE,
    CONTEXT_ACTION_DUNGEON_RESEAL
} ContextActionKind;

typedef struct ContextAction {
    ContextActionKind kind;
    CcGood good;
    int32_t amount;
    char label[64];
    char key_hint[16];
    char detail[48];
    bool enabled;
    bool active;
} ContextAction;

typedef struct ContextActionSet {
    ContextAction items[8];
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

static bool queued_save_shortcut = false;

static GLFWkeyfun previous_key_callback = NULL;
static GLFWmousebuttonfun previous_mouse_button_callback = NULL;
static bool queued_key_press[GLFW_KEY_LAST + 1] = {0};
static bool queued_mouse_button_press[GLFW_MOUSE_BUTTON_LAST + 1] = {0};

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

#if defined(PLATFORM_WEB)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
EM_ASYNC_JS(void, ClientWaitForAnimationFrame, (), {
    await new Promise((resolve) => requestAnimationFrame(resolve));
});
EM_ASYNC_JS(int, ClientFlushBrowserSaves,
            (const char *campaign_path, const char *session_path), {
    try {
        await Module.persistCrownlessSave(
            UTF8ToString(campaign_path), UTF8ToString(session_path));
        console.info("Crownless Carriage campaign stored.");
        return 1;
    } catch (error) {
        console.error("Could not store Crownless Carriage saves", error);
        return 0;
    }
});
EM_JS(int, ClientReleaseBrowserAssets, (), {
    let releasedBytes = 0;
    const removeTree = (path) => {
        for (const name of FS.readdir(path)) {
            if (name === "." || name === "..") continue;
            const child = path + "/" + name;
            const stat = FS.stat(child);
            if (FS.isDir(stat.mode)) {
                removeTree(child);
                FS.rmdir(child);
            } else {
                releasedBytes += stat.size;
                FS.unlink(child);
            }
        }
    };
    try {
        removeTree("/assets");
        FS.rmdir("/assets");
        return releasedBytes;
    } catch (error) {
        console.warn("Could not release Crownless Carriage startup files", error);
        return -1;
    }
});
EM_JS(int, ClientBrowserHeapBytes, (), {
    document.documentElement.dataset.crownlessWasmMemory = HEAP8.length;
    return HEAP8.length;
});
#pragma clang diagnostic pop
#endif

static void ClientTakeScreenshot(const char *path)
{
    TakeScreenshot(path);
#if defined(PLATFORM_WEB)
    /* Browser capture files live in MEMFS and cannot be collected by the
       desktop art pipeline. Discard each one after raylib has encoded it so
       long diagnostic reels do not grow the page until it is killed. */
    (void)remove(path);
#endif
}

static const Vector2 LOCAL_MARKET = {CC_LOCAL_MARKET_X, CC_LOCAL_MARKET_Z};
static const Vector2 LOCAL_CARRIAGE = {CC_LOCAL_CARRIAGE_X,
                                      CC_LOCAL_CARRIAGE_Z};
static const Vector2 LOCAL_CARRIAGE_BAY = {
    CC_LOCAL_CARRIAGE_APPROACH_X, CC_LOCAL_CARRIAGE_APPROACH_Z
};
static const Vector2 LOCAL_NOTICE = {CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z};
static const Vector2 LOCAL_DUNGEON = {CC_LOCAL_DUNGEON_X,
                                     CC_LOCAL_DUNGEON_Z};
static const Vector2 LOCAL_DRAGON_CAVE = {CC_LOCAL_DRAGON_CAVE_X,
                                         CC_LOCAL_DRAGON_CAVE_Z};
static const Vector2 INTERIOR_COUNTER = {6.65f, 2.50f};
static const Vector2 INTERIOR_EXIT = {1.55f, 5.55f};

static void CampaignSavePath(char *path, size_t capacity)
{
#if defined(PLATFORM_WEB)
    (void)snprintf(path, capacity,
                   "/crownless-save/crownless_campaign.ccsave");
#elif defined(__APPLE__)
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

static bool CampaignSaveExists(const char *path)
{
#if defined(PLATFORM_WEB)
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    (void)fclose(file);
    return true;
#else
    return FileExists(path);
#endif
}

static bool CampaignCompanionPath(const char *save_path, const char *suffix,
                                  char *path, size_t capacity)
{
    if (save_path == NULL || suffix == NULL || path == NULL || capacity == 0U) {
        return false;
    }
    int length = snprintf(path, capacity, "%s%s", save_path, suffix);
    return length >= 0 && (size_t)length < capacity;
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

static int32_t OpeningSituationIndex(const CcSim *sim)
{
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE &&
            sim->situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            return i;
        }
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) return i;
    }
    return -1;
}

static bool OpeningRequired(const CcSim *sim)
{
    int32_t index = OpeningSituationIndex(sim);
    return sim != NULL && index >= 0 && sim->player.reputation == 0 &&
        sim->player.accepted_situation_id == 0U &&
        CcSimSituationOfferSettlementId(
            sim, &sim->situations[index]) == sim->player.location_id;
}

static bool SituationVisibleToPlayer(const CcSim *sim, int32_t index)
{
    if (sim == NULL || index < 0 || index >= sim->situation_count ||
        sim->situations[index].status != CC_SITUATION_ACTIVE) return false;

    if (sim->player.reputation > 0) return true;
    if (sim->player.accepted_situation_id != 0U) {
        return sim->situations[index].id ==
               sim->player.accepted_situation_id;
    }
    return index == OpeningSituationIndex(sim);
}

static int32_t FirstActiveSituationIndex(const CcSim *sim)
{
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (SituationVisibleToPlayer(sim, i)) return i;
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
        if (SituationVisibleToPlayer(sim, index)) return index;
    }
    return -1;
}

static const CcSituation *SelectedActiveSituation(const CcSim *sim,
                                                  int32_t selected)
{
    if (!SituationVisibleToPlayer(sim, selected)) {
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
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION &&
        situation->discovery_stage != CC_DISCOVERY_OFFER) {
        const CcCharacter *contact = NULL;
        if (situation->discovery_stage == CC_DISCOVERY_WITNESS) {
            contact = CcSimSituationWitnessCharacter(sim, situation);
        } else if (situation->discovery_stage == CC_DISCOVERY_AUTHORITY) {
            contact = CcSimSituationSponsorCharacter(sim, situation);
        } else {
            contact = CcSimSituationAffectedCharacter(sim, situation);
        }
        const CcSettlement *contact_place = contact != NULL ?
            CcSimSettlement(sim, contact->current_settlement_id) : NULL;
        const char *name = contact != NULL ? contact->name : "the miner";
        const char *place = contact_place != NULL ? contact_place->name :
            "the mining town";
        const char *travel = contact != NULL &&
            contact->current_settlement_id == sim->player.location_id ?
            "" : TextFormat("Go to %s. ", place);
        (void)snprintf(label, capacity, "%sTalk to %s.", travel, name);
        return;
    }
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        int32_t remaining = situation->quantity - situation->progress;
        if (here && sim->player.cargo[situation->good] > 0) {
            if (situation->good == CC_GOOD_FOOD) {
                (void)snprintf(label, capacity,
                               "Deliver %d food boxes at the market.",
                               remaining);
            } else {
                (void)snprintf(label, capacity,
                               "Deliver %d %s at the market.",
                               remaining, CcGoodName(situation->good));
            }
        } else if (here) {
            (void)snprintf(label, capacity,
                           "Bring %d %s here.",
                           remaining, CcGoodName(situation->good));
        } else if (sim->player.cargo[situation->good] >= remaining) {
            (void)snprintf(label, capacity,
                           situation->good == CC_GOOD_FOOD ?
                           "The food boxes are aboard. Travel to %s." :
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
                       "Repair this road: 2 Tools or 18 crowns." :
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
    DrawRectangleRounded(bounds, 0.015f, 3, color);
    DrawRectangleRoundedLinesEx(bounds, 0.015f, 3, 1.0f,
                                Fade(CC_GOLD, 0.68f));
    DrawLine((int)bounds.x + 10, (int)bounds.y + 8,
             (int)bounds.x + 48, (int)bounds.y + 8,
             Fade(CC_GOLD, 0.64f));
    DrawLine((int)(bounds.x + bounds.width) - 34,
             (int)(bounds.y + bounds.height) - 8,
             (int)(bounds.x + bounds.width) - 10,
             (int)(bounds.y + bounds.height) - 8,
             Fade(CC_STYLE_GOLD_SHADOW, 0.76f));
    DrawRectangle((int)bounds.x + 5, (int)bounds.y + 5, 3, 3,
                  Fade(CC_GOLD, 0.72f));
}

static void DrawCampaignUnavailable(const char *error)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.88f));
    float width = fminf(720.0f, (float)GetScreenWidth() - 48.0f);
    Rectangle bounds = {
        ((float)GetScreenWidth() - width) * 0.5f,
        ((float)GetScreenHeight() - 190.0f) * 0.5f,
        width,
        190.0f
    };
    DrawPanel(bounds, PANEL_DEEP);
    const char *title = "CAMPAIGN COULD NOT BE OPENED";
    int title_width = CcOverlayMeasureText(title, 20);
    CcOverlayDrawText(title,
                      (GetScreenWidth() - title_width) / 2,
                      (int)bounds.y + 34, 20, DANGER);
    const char *detail = TextFormat(
        "%.72s",
        error != NULL && error[0] != '\0' ? error :
            "The campaign journal is unavailable.");
    int detail_width = CcOverlayMeasureText(detail, 10);
    CcOverlayDrawText(detail,
                      (GetScreenWidth() - detail_width) / 2,
                      (int)bounds.y + 82, 10, INK);
    const char *safety = "Your campaign was not changed.";
    int safety_width = CcOverlayMeasureText(safety, 11);
    CcOverlayDrawText(safety,
                      (GetScreenWidth() - safety_width) / 2,
                      (int)bounds.y + 116, 11, TEAL);
    const char *instruction =
        "Close the game, fix the save problem, then reopen it.";
    int instruction_width = CcOverlayMeasureText(instruction, 10);
    CcOverlayDrawText(instruction,
                      (GetScreenWidth() - instruction_width) / 2,
                      (int)bounds.y + 145, 10, MUTED);
}

static void DrawPerformanceOverlay(void)
{
    CcLocalRendererStats stats = CcLocalRendererGetStats();
    float fps = stats.smoothed_frame_milliseconds > 0.001f ?
        1000.0f / stats.smoothed_frame_milliseconds : 0.0f;
    Rectangle bounds = {(float)GetScreenWidth() - 294.0f, 18.0f,
                        276.0f, 128.0f};
    DrawPanel(bounds, PANEL_DEEP);
    CcOverlayDrawText("LOCAL PERFORMANCE", (int)bounds.x + 14,
             (int)bounds.y + 11, 12, TEAL);
    CcOverlayDrawText(TextFormat("frame %5.2f ms  %5.1f fps",
                        stats.smoothed_frame_milliseconds, fps),
             (int)bounds.x + 14, (int)bounds.y + 32, 11, INK);
    CcOverlayDrawText(TextFormat("skin %d update  %d mesh uploads",
                        stats.skin_updates, stats.skinned_meshes),
             (int)bounds.x + 14, (int)bounds.y + 50, 11, INK);
    CcOverlayDrawText(TextFormat("npc %d/%d  creature %d/%d",
                        stats.npc_skin_updates, stats.npc_skinned_meshes,
                        stats.creature_skin_updates,
                        stats.creature_skinned_meshes),
             (int)bounds.x + 14, (int)bounds.y + 68, 10, MUTED);
    CcOverlayDrawText(TextFormat("p95 %4.1f  p99 %4.1f  hitches %d",
                        stats.p95_frame_milliseconds,
                        stats.p99_frame_milliseconds,
                        stats.hitch_count),
             (int)bounds.x + 14, (int)bounds.y + 86, 10,
             stats.hitch_count > 0 ? DANGER : MUTED);
    CcOverlayDrawText(TextFormat("bipeds %d  hero %d  lod %d",
                        stats.biomechanical_characters,
                        stats.high_detail_characters,
                        stats.low_detail_characters),
             (int)bounds.x + 14, (int)bounds.y + 104, 11, MUTED);
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

static float ClampUnit(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float WrapLocalAngle(float angle)
{
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

static void SampleConvoyPath(const Vector2 *points, int32_t count,
                             float progress, Vector2 *position,
                             float *heading)
{
    if (points == NULL || count < 2 || position == NULL || heading == NULL) {
        return;
    }
    float length = 0.0f;
    for (int32_t i = 0; i + 1 < count; ++i) {
        float x = points[i + 1].x - points[i].x;
        float y = points[i + 1].y - points[i].y;
        length += sqrtf(x * x + y * y);
    }
    float remaining = ClampUnit(progress) * length;
    for (int32_t i = 0; i + 1 < count; ++i) {
        Vector2 delta = {points[i + 1].x - points[i].x,
                         points[i + 1].y - points[i].y};
        float segment = sqrtf(delta.x * delta.x + delta.y * delta.y);
        if (remaining <= segment || i + 2 == count) {
            float amount = segment > 0.0001f ? remaining / segment : 0.0f;
            amount = ClampUnit(amount);
            *position = (Vector2){
                points[i].x + delta.x * amount,
                points[i].y + delta.y * amount
            };
            *heading = atan2f(delta.x, delta.y);
            return;
        }
        remaining -= segment;
    }
}

static void SetConvoyTownPose(CcLocalConvoyState *convoy, float delta_time)
{
    static const Vector2 departure_path[] = {
        {CC_LOCAL_CARRIAGE_X, CC_LOCAL_CARRIAGE_Z},
        {38.2f, 35.8f}, {48.0f, 36.0f}, {63.8f, 34.0f},
        {78.5f, 35.8f}, {93.0f, CC_LOCAL_TOWN_GATE_Z},
        {CC_LOCAL_TOWN_GATE_X, CC_LOCAL_TOWN_GATE_Z}
    };
    static const Vector2 arrival_path[] = {
        {CC_LOCAL_TOWN_GATE_X, CC_LOCAL_TOWN_GATE_Z},
        {93.0f, CC_LOCAL_TOWN_GATE_Z}, {78.5f, 35.8f},
        {63.8f, 34.0f}, {48.0f, 36.0f}, {38.2f, 35.8f},
        {CC_LOCAL_CARRIAGE_X, CC_LOCAL_CARRIAGE_Z}
    };
    _Static_assert(sizeof(departure_path) == sizeof(arrival_path),
                   "Convoy town paths must have the same point count.");
    const Vector2 *path = convoy->phase == CC_LOCAL_CONVOY_ARRIVING ?
        arrival_path : departure_path;
    int32_t count = (int32_t)(sizeof(departure_path) /
                              sizeof(departure_path[0]));
    Vector2 position = {0};
    float heading = convoy->town_heading_yaw;
    SampleConvoyPath(path, count, convoy->phase_progress,
                     &position, &heading);
    float turn = WrapLocalAngle(heading - convoy->town_heading_yaw);
    float turn_weight = delta_time > 0.0f ?
        ClampUnit(delta_time * 4.5f) : 1.0f;
    convoy->town_heading_yaw = WrapLocalAngle(
        convoy->town_heading_yaw + turn * turn_weight);
    position.x += cosf(convoy->town_heading_yaw) * convoy->lateral_offset;
    position.y -= sinf(convoy->town_heading_yaw) * convoy->lateral_offset;
    convoy->town_position = (Vector3){
        position.x, CcLocalTerrainHeightAt(position.x, position.y), position.y
    };
}

typedef enum ConvoyUpdateResult {
    CONVOY_UPDATE_NONE = 0,
    CONVOY_UPDATE_OPEN_ROAD_BOOK,
    CONVOY_UPDATE_PARKED
} ConvoyUpdateResult;

static ConvoyUpdateResult UpdateDrivenConvoy(LocalState *local,
                                              const CcSim *sim,
                                              float delta_time)
{
    CcLocalConvoyState *convoy = &local->convoy;
    bool stopped = IsKeyDown(KEY_SPACE);
    bool captain_pace = local->journey_travel_active && sim != NULL &&
        sim->journey.active && convoy->phase == CC_LOCAL_CONVOY_ROAD;
    if (captain_pace) {
        float target = stopped ? 0.0f : CcClientConvoyPosturePace(
            (int32_t)sim->journey.pace);
        float change = target - convoy->pace;
        float maximum_change = delta_time * 1.20f;
        change = fmaxf(-maximum_change, fminf(maximum_change, change));
        convoy->pace = fmaxf(0.0f, fminf(1.0f, convoy->pace + change));
    } else {
        bool urge = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
        bool rein_in = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
        convoy->pace = CcClientConvoyPaceStep(
            convoy->pace, convoy->phase == CC_LOCAL_CONVOY_ROAD,
            urge, rein_in, stopped, delta_time);
    }

    float centering_step = delta_time * 1.30f;
    if (fabsf(convoy->lateral_offset) <= centering_step) {
        convoy->lateral_offset = 0.0f;
    } else {
        convoy->lateral_offset -= copysignf(
            centering_step, convoy->lateral_offset);
    }

    if (convoy->phase == CC_LOCAL_CONVOY_DEPARTING) {
        CcClientDepartureAdvance(
            &local->departure, convoy->pace, delta_time);
        convoy->phase_progress = local->departure.town_progress;
        SetConvoyTownPose(convoy, delta_time);
        if (local->departure.phase == CC_CLIENT_DEPARTURE_ROAD_BOOK) {
            convoy->lateral_offset = 0.0f;
            return CONVOY_UPDATE_OPEN_ROAD_BOOK;
        }
        return CONVOY_UPDATE_NONE;
    }
    if (convoy->phase == CC_LOCAL_CONVOY_ARRIVING) {
        CcClientArrivalAdvance(
            &local->arrival, convoy->pace, delta_time);
        convoy->phase_progress = local->arrival.town_progress;
        SetConvoyTownPose(convoy, delta_time);
        return local->arrival.phase == CC_CLIENT_ARRIVAL_PARKED ?
            CONVOY_UPDATE_PARKED : CONVOY_UPDATE_NONE;
    }
    return CONVOY_UPDATE_NONE;
}

static void RepositionHero(LocalState *local, Vector2 position,
                           bool market_interior);

static void ResetLocalState(LocalState *local)
{
    local->movement_preview = (CcLocalMovementPreview){0};
    local->movement_preview_cooldown = 0.0f;
    local->movement_reticle = (Vector2){0};
    local->movement_reticle_age = 0.0f;
    local->movement_reticle_valid = false;
    local->movement_reticle_accepted = false;
    local->conversation_character_id = 0U;
    local->conversation_situation_id = 0U;
    local->opening_step = CC_LOCAL_OPENING_COMPLETE;
    local->market_interior = false;
    local->open_world_market = false;
    local->site_kind = CC_LOCAL_SITE_NONE;
    local->site_travel_progress = 0.0f;
    local->fork_turn_progress = 0.0f;
    local->site_travel_active = false;
    local->site_returning = false;
    local->road_choice_active = false;
    local->journey_travel_active = false;
    local->journey_combat_active = false;
    local->journey_parley_active = false;
    local->departure = (CcClientDepartureTransition){
        .phase = CC_CLIENT_DEPARTURE_READY,
    };
    local->arrival = (CcClientArrivalTransition){
        .phase = CC_CLIENT_ARRIVAL_PARKED,
        .road_book_progress = 1.0f,
        .town_progress = 1.0f,
    };
    local->convoy = (CcLocalConvoyState){
        .phase = CC_LOCAL_CONVOY_PARKED,
        .town_position = {CC_LOCAL_CARRIAGE_X, 0.0f,
                          CC_LOCAL_CARRIAGE_Z}
    };

    CcLocalAgentInit(
        &local->agent,
        (Vector2){CC_LOCAL_CARRIAGE_APPROACH_X,
                  CC_LOCAL_CARRIAGE_APPROACH_Z}, false);
    local->agent.facing_yaw = atan2f(
        CC_LOCAL_CARRIAGE_X - CC_LOCAL_CARRIAGE_APPROACH_X,
        CC_LOCAL_CARRIAGE_Z - CC_LOCAL_CARRIAGE_APPROACH_Z);
    CcLocalCombatSetTeam(&local->agent, CC_COMBAT_PLAYER);
    CcLocalCourseInit(&local->course);
}

static void BeginOpening(LocalState *local)
{
    if (local == NULL) return;
    RepositionHero(local,
                   (Vector2){CC_LOCAL_INTRO_START_X,
                             CC_LOCAL_INTRO_START_Z}, false);
    local->opening_step = CC_LOCAL_OPENING_MEET_MARA;
    local->agent.facing_yaw = atan2f(
        CC_LOCAL_NOTICE_X - CC_LOCAL_INTRO_START_X,
        CC_LOCAL_NOTICE_Z - CC_LOCAL_INTRO_START_Z);
    local->course.alarm_countdown = 1000.0f;
}

static void RepositionHero(LocalState *local, Vector2 position,
                           bool market_interior)
{
    CcAthleticProfile athletics = local->agent.athletics;
    CcCombatState combat = local->agent.combat;
    CcNpcAppearance appearance = local->agent.appearance;
    CcMorphologyPreset morphology = local->agent.morphology;
    Color tunic_color = local->agent.tunic_color;
    bool crowned = local->agent.crowned;
    CcLocalAgentInit(&local->agent, position, market_interior);
    local->agent.athletics = athletics;
    local->agent.combat = combat;
    local->agent.appearance = appearance;
    local->agent.tunic_color = tunic_color;
    local->agent.crowned = crowned;
    CcLocalAgentSetMorphology(&local->agent, morphology, market_interior);
    CcLocalCombatSetTeam(&local->agent, CC_COMBAT_PLAYER);
}

static const CcRoute *OpenWorldRouteFromSettlement(const CcSim *sim,
                                                    CcId settlement_id)
{
    if (sim == NULL || settlement_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].from_id == settlement_id ||
            sim->routes[i].to_id == settlement_id) {
            return &sim->routes[i];
        }
    }
    return NULL;
}

static int32_t OpenWorldRouteIndex(const CcSim *sim, CcId route_id)
{
    if (sim == NULL || route_id == 0U) return -1;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].id == route_id) return i;
    }
    return -1;
}

static float SessionAngleDistance(float first, float second)
{
    float difference = first - second;
    while (difference > PI) difference -= 2.0f * PI;
    while (difference < -PI) difference += 2.0f * PI;
    return fabsf(difference);
}

static float SessionPointSegmentDistanceSquared(
    CcWorldPoint point, CcWorldPoint first, CcWorldPoint second,
    float *segment_amount)
{
    float dx = second.x - first.x;
    float dz = second.z - first.z;
    float length_squared = dx * dx + dz * dz;
    float amount = length_squared > 0.0001f ?
        ((point.x - first.x) * dx + (point.z - first.z) * dz) /
            length_squared : 0.0f;
    amount = ClampUnit(amount);
    if (segment_amount != NULL) *segment_amount = amount;
    float nearest_x = first.x + dx * amount;
    float nearest_z = first.z + dz * amount;
    float offset_x = point.x - nearest_x;
    float offset_z = point.z - nearest_z;
    return offset_x * offset_x + offset_z * offset_z;
}

static bool WorldSessionRouteScore(
    const CcWorldRoutePlacement *route, CcId settlement_id,
    CcWorldPoint position, float facing_yaw,
    float *distance_squared, float *heading_distance)
{
    if (route == NULL || distance_squared == NULL ||
        heading_distance == NULL ||
        (route->from_id != settlement_id &&
         route->to_id != settlement_id)) {
        return false;
    }
    float total_length = CcWorldRouteLength(route);
    if (total_length <= 0.0001f) return false;
    float best_distance = FLT_MAX;
    float best_heading = FLT_MAX;
    float travelled = 0.0f;
    for (int32_t sample = 0;
         sample < CC_WORLD_ROUTE_SAMPLE_COUNT - 1; ++sample) {
        CcWorldPoint first = route->samples[sample];
        CcWorldPoint second = route->samples[sample + 1];
        float dx = second.x - first.x;
        float dz = second.z - first.z;
        float segment_length = sqrtf(dx * dx + dz * dz);
        float segment_amount = 0.0f;
        float candidate_distance = SessionPointSegmentDistanceSquared(
            position, first, second, &segment_amount);
        float route_amount = (travelled + segment_length * segment_amount) /
            total_length;
        float journey_amount = route->from_id == settlement_id ?
            route_amount : 1.0f - route_amount;
        CcWorldPoint ignored_position;
        float heading = 0.0f;
        bool has_pose = CcWorldRoutePose(
            route, settlement_id, journey_amount,
            &ignored_position, &heading);
        float candidate_heading = has_pose ?
            SessionAngleDistance(heading, facing_yaw) : FLT_MAX;
        if (candidate_distance < best_distance - 0.0001f ||
            (fabsf(candidate_distance - best_distance) <= 0.0001f &&
             candidate_heading < best_heading)) {
            best_distance = candidate_distance;
            best_heading = candidate_heading;
        }
        travelled += segment_length;
    }
    int32_t gate_sample = route->from_id == settlement_id ?
        0 : CC_WORLD_ROUTE_SAMPLE_COUNT - 1;
    int32_t junction_sample = route->from_id == settlement_id ?
        CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE :
        CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE;
    float connector_distance = SessionPointSegmentDistanceSquared(
        position, route->samples[gate_sample],
        route->samples[junction_sample], NULL);
    if (connector_distance <= best_distance + 0.0001f) {
        float route_amount = CcWorldRouteSampleAmount(
            route, junction_sample);
        float journey_amount = route->from_id == settlement_id ?
            route_amount : 1.0f - route_amount;
        CcWorldPoint ignored_position;
        float departure_heading = 0.0f;
        if (CcWorldRoutePose(
                route, settlement_id, journey_amount,
                &ignored_position, &departure_heading)) {
            best_heading = SessionAngleDistance(
                departure_heading, facing_yaw);
        }
    }
    *distance_squared = best_distance;
    *heading_distance = best_heading;
    return best_distance < FLT_MAX;
}

static const CcRoute *InferWorldSessionRoute(
    const CcSim *sim, const CcWorldManifest *manifest,
    const CcClientSession *session)
{
    if (sim == NULL || manifest == NULL || session == NULL) return NULL;
    const CcRoute *best_route = NULL;
    float best_distance = FLT_MAX;
    float best_heading = FLT_MAX;
    CcWorldPoint position = {session->position_x, session->position_z};
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id != session->location_id &&
            route->to_id != session->location_id) {
            continue;
        }
        const CcWorldRoutePlacement *placement =
            CcWorldRoutePlacementForId(manifest, route->id);
        float distance = FLT_MAX;
        float heading = FLT_MAX;
        if (!WorldSessionRouteScore(
                placement, session->location_id, position,
                session->facing_yaw, &distance, &heading)) {
            continue;
        }
        if (distance < best_distance - 0.0001f ||
            (fabsf(distance - best_distance) <= 0.0001f &&
             heading < best_heading)) {
            best_route = route;
            best_distance = distance;
            best_heading = heading;
        }
    }
    return best_distance <= 49.0f ? best_route : NULL;
}

static bool SetOpenWorldCarriageOnRoute(
    const CcSim *sim, LocalState *local, CcId route_id, CcId origin_id,
    float distance_from_origin, float pace, bool hero_embarked)
{
    if (sim == NULL || local == NULL || !local->open_world) return false;
    const CcWorldRoutePlacement *route = CcWorldRoutePlacementForId(
        &local->world_stream.manifest, route_id);
    if (route == NULL) return false;
    float length = CcWorldRouteLength(route);
    float amount = length > 0.001f ?
        fminf(0.22f, ClampUnit(distance_from_origin / length)) : 0.0f;
    CcWorldPoint point;
    float heading = 0.0f;
    if (!CcWorldRoutePose(route, origin_id, amount, &point, &heading)) {
        return false;
    }
    local->world_carriage.position = (Vector3){
        point.x,
        CcWorldStreamHeightAt(&local->world_stream, point.x, point.z),
        point.z
    };
    local->world_carriage.heading_yaw = heading;
    local->world_carriage.route_amount =
        route->from_id == origin_id ? amount : 1.0f - amount;
    local->world_carriage.pace = pace;
    local->world_carriage.route_id = route_id;
    local->world_carriage.visible = true;
    local->world_carriage.hero_embarked = hero_embarked;
    local->world_carriage.town_arrival = false;
    return true;
}

static void SetOpenWorldCarriageAtSettlement(const CcSim *sim,
                                             LocalState *local)
{
    if (sim == NULL || local == NULL || !local->open_world) return;
    const CcWorldSettlementPlacement *place =
        CcWorldSettlementPlacementForId(
            &local->world_stream.manifest, sim->player.location_id);
    const CcRoute *route = OpenWorldRouteFromSettlement(
        sim, sim->player.location_id);
    local->world_carriage.camera_target = 0.0f;
    local->world_carriage.pace = 0.0f;
    local->world_carriage.hero_embarked = false;
    if (place == NULL) {
        local->world_carriage.visible = false;
        return;
    }
    if (route != NULL && SetOpenWorldCarriageOnRoute(
            sim, local, route->id, sim->player.location_id,
            0.0f, 0.0f, false)) {
        return;
    }
    local->world_carriage.position = (Vector3){
        place->center.x,
        CcWorldStreamHeightAt(&local->world_stream,
                              place->center.x, place->center.z),
        place->center.z
    };
    local->world_carriage.heading_yaw = 0.0f;
    local->world_carriage.route_amount = 0.0f;
    local->world_carriage.route_id = 0U;
    local->world_carriage.visible = true;
}

static void SetOpenWorldCarriageAtRoadGate(const CcSim *sim,
                                           LocalState *local,
                                           CcId route_id)
{
    if (sim == NULL || local == NULL || !local->open_world) return;
    if (SetOpenWorldCarriageOnRoute(
            sim, local, route_id, sim->player.location_id,
            0.0f, 0.0f, true)) {
        local->world_carriage.camera_target = 1.0f;
    }
}

static bool RoadBookDepartureInProgress(const LocalState *local)
{
    return local != NULL && local->road_choice_active && local->open_world &&
           local->departure.phase == CC_CLIENT_DEPARTURE_ROAD_BOOK;
}

static bool RoadBookArrivalInProgress(const LocalState *local)
{
    return local != NULL && local->open_world &&
           local->journey_travel_active &&
           local->arrival.phase == CC_CLIENT_ARRIVAL_ROAD_BOOK;
}

static void PositionOpenWorldDeparture(const CcSim *sim, LocalState *local)
{
    if (sim == NULL || local == NULL || !local->open_world) return;
    const CcWorldRoutePlacement *route = CcWorldRoutePlacementForId(
        &local->world_stream.manifest, local->world_carriage.route_id);
    if (route == NULL) return;
    bool forward = route->from_id == sim->player.location_id;
    int32_t junction_sample = forward ?
        CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE :
        CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE;
    float route_amount = CcWorldRouteSampleAmount(route, junction_sample);
    float junction_amount = forward ? route_amount : 1.0f - route_amount;
    float progress = ClampUnit(local->departure.road_book_progress);
    float movement = progress * progress * (3.0f - 2.0f * progress);
    float journey_amount = junction_amount * movement;
    CcWorldPoint point;
    float heading = 0.0f;
    if (!CcWorldRoutePose(
            route, sim->player.location_id, journey_amount,
            &point, &heading)) {
        return;
    }
    local->world_carriage.position = (Vector3){
        point.x,
        CcWorldStreamHeightAt(&local->world_stream, point.x, point.z),
        point.z
    };
    local->world_carriage.heading_yaw = heading;
    local->world_carriage.route_amount = forward ? journey_amount :
                                                  1.0f - journey_amount;
    local->world_carriage.pace = local->departure.phase ==
            CC_CLIENT_DEPARTURE_READY ? 0.0f : local->convoy.pace;
    local->agent.position = local->world_carriage.position;
    local->agent.facing_yaw = heading;
    local->convoy.phase_progress = progress;
    if (local->departure.phase == CC_CLIENT_DEPARTURE_READY) {
        local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
    }
}

static bool PositionOpenWorldArrival(const CcSim *sim, LocalState *local)
{
    if (sim == NULL || local == NULL || !local->open_world) return false;
    const CcWorldRoutePlacement *route = CcWorldRoutePlacementForId(
        &local->world_stream.manifest, sim->journey.route_id);
    CcId origin_id = sim->journey.origin_id;
    CcId destination_id = sim->journey.destination_id;
    if (route == NULL ||
        (origin_id != route->from_id && origin_id != route->to_id) ||
        (destination_id != route->from_id &&
         destination_id != route->to_id) ||
        origin_id == destination_id) {
        return false;
    }
    bool destination_is_from = destination_id == route->from_id;
    int32_t junction_sample = destination_is_from ?
        CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE :
        CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE;
    float route_amount = CcWorldRouteSampleAmount(route, junction_sample);
    float junction_amount = origin_id == route->from_id ?
        route_amount : 1.0f - route_amount;
    float transition = ClampUnit(local->arrival.road_book_progress);
    float movement = transition * transition * (3.0f - 2.0f * transition);
    float journey_amount = junction_amount +
        (1.0f - junction_amount) * movement;
    CcWorldPoint point;
    float heading = 0.0f;
    if (!CcWorldRoutePose(route, origin_id, journey_amount,
                          &point, &heading)) {
        return false;
    }
    local->world_carriage.position = (Vector3){
        point.x,
        CcWorldStreamHeightAt(&local->world_stream, point.x, point.z),
        point.z
    };
    local->world_carriage.heading_yaw = heading;
    local->world_carriage.route_amount = origin_id == route->from_id ?
        journey_amount : 1.0f - journey_amount;
    local->world_carriage.pace = local->convoy.pace;
    local->world_carriage.route_id = route->route_id;
    local->world_carriage.visible = true;
    local->world_carriage.hero_embarked = true;
    local->agent.position = local->world_carriage.position;
    local->agent.facing_yaw = heading;
    return true;
}

static void UpdateOpenWorldCamera(const CcSim *sim, LocalState *local,
                                  float delta_time)
{
    if (local == NULL || !local->open_world) return;
    if (RoadBookDepartureInProgress(local)) {
        CcClientDepartureAdvance(
            &local->departure, local->convoy.pace, delta_time);
        local->world_carriage.camera_weight =
            local->departure.road_book_progress;
        local->world_carriage.camera_target = 1.0f;
        PositionOpenWorldDeparture(sim, local);
        return;
    }
    if (RoadBookArrivalInProgress(local)) {
        CcClientArrivalAdvance(&local->arrival, local->convoy.pace,
                               delta_time);
        local->world_carriage.camera_weight =
            CcClientArrivalCameraWeight(&local->arrival);
        local->world_carriage.camera_target = 0.0f;
        (void)PositionOpenWorldArrival(sim, local);
        return;
    }
    float change = local->world_carriage.camera_target -
                   local->world_carriage.camera_weight;
    float maximum_change = fmaxf(0.0f, delta_time) * 1.35f;
    change = fmaxf(-maximum_change, fminf(maximum_change, change));
    local->world_carriage.camera_weight = ClampUnit(
        local->world_carriage.camera_weight + change);
}

static void PositionOpenWorldJourney(const CcSim *sim, LocalState *local);

static bool InitializeOpenWorld(const CcSim *sim, LocalState *local,
                                bool preserve_position)
{
    if (sim == NULL || local == NULL) return false;
    (void)preserve_position;
    CcLocalBindOpenWorld(NULL);
    local->open_world = false;
    if (!CcWorldStreamInit(&local->world_stream, sim)) return false;
    local->world_carriage = (CcLocalWorldCarriageState){0};
    return true;
}

static void LeaveOpenWorld(LocalState *local)
{
    if (local == NULL) return;
    local->open_world = false;
    local->open_world_market = false;
    local->world_carriage.visible = false;
    local->world_carriage.hero_embarked = false;
    local->world_carriage.town_arrival = false;
    local->world_carriage.camera_weight = 0.0f;
    local->world_carriage.camera_target = 0.0f;
    CcLocalBindOpenWorld(NULL);
}

static bool EnterOpenWorldAtRoadGate(const CcSim *sim, LocalState *local,
                                     CcId route_id)
{
    if (sim == NULL || local == NULL ||
        local->world_stream.manifest.route_count <= 0) return false;
    local->open_world = true;
    CcLocalBindOpenWorld(&local->world_stream);
    local->world_carriage.camera_weight = 0.0f;
    SetOpenWorldCarriageAtRoadGate(sim, local, route_id);
    local->agent.position = local->world_carriage.position;
    local->agent.facing_yaw = local->world_carriage.heading_yaw;
    return local->world_carriage.visible;
}

static bool EnterRoadBookFromTownGate(const CcSim *sim, LocalState *local,
                                      CcId route_id)
{
    if (local == NULL ||
        local->departure.phase != CC_CLIENT_DEPARTURE_ROAD_BOOK) {
        return false;
    }
    float pace = local->convoy.pace;
    if (!EnterOpenWorldAtRoadGate(sim, local, route_id)) return false;
    local->world_carriage.pace = pace;
    local->convoy.phase_progress = 0.0f;
    PositionOpenWorldDeparture(sim, local);
    return true;
}

static void FaceOpenWorldRoadChoice(const CcSim *sim, LocalState *local,
                                    CcId route_id)
{
    SetOpenWorldCarriageAtRoadGate(sim, local, route_id);
    if (local != NULL &&
        local->departure.phase == CC_CLIENT_DEPARTURE_READY) {
        local->departure.road_book_progress = 1.0f;
        PositionOpenWorldDeparture(sim, local);
    }
}

static bool EnterOpenWorldJourney(const CcSim *sim, LocalState *local)
{
    if (sim == NULL || local == NULL || !sim->journey.active ||
        local->world_stream.manifest.route_count <= 0) return false;
    local->open_world = true;
    CcLocalBindOpenWorld(&local->world_stream);
    PositionOpenWorldJourney(sim, local);
    local->world_carriage.camera_target = 1.0f;
    return local->world_carriage.visible;
}

static void BindOpenWorldForLocalState(const LocalState *local)
{
    CcLocalBindOpenWorld(local != NULL && local->open_world &&
                         !local->market_interior ?
                             &local->world_stream : NULL);
}

static void PositionOpenWorldAtSettlement(const CcSim *sim,
                                          LocalState *local)
{
    if (sim == NULL || local == NULL || !local->open_world) return;
    const CcWorldSettlementPlacement *place =
        CcWorldSettlementPlacementForId(
            &local->world_stream.manifest, sim->player.location_id);
    if (place == NULL) return;
    CcWorldPoint point = CcWorldSettlementFeaturePoint(
        &local->world_stream.manifest, place->settlement_id, 0.0f, 10.0f);
    BindOpenWorldForLocalState(local);
    RepositionHero(local, (Vector2){point.x, point.z}, false);
    SetOpenWorldCarriageAtSettlement(sim, local);
}

static void PositionOpenWorldJourney(const CcSim *sim, LocalState *local)
{
    if (sim == NULL || local == NULL || !local->open_world ||
        !sim->journey.active) return;
    const CcWorldRoutePlacement *route = CcWorldRoutePlacementForId(
        &local->world_stream.manifest, sim->journey.route_id);
    if (route == NULL) return;
    float progress = ClampUnit(
        (float)sim->carriage.progress_milli / 1000.0f);
    float amount = CcWorldRouteJourneyAmount(
        route, sim->journey.origin_id, progress);
    CcWorldPoint point;
    float heading = 0.0f;
    if (!CcWorldRoutePose(route, sim->journey.origin_id, amount,
                          &point, &heading)) return;
    local->agent.position.x = point.x;
    local->agent.position.z = point.z;
    local->agent.position.y = CcWorldStreamHeightAt(
        &local->world_stream, point.x, point.z);
    local->agent.target_valid = false;
    local->agent.exact_target_valid = false;
    local->agent.facing_yaw = heading;
    local->world_carriage.position = local->agent.position;
    local->world_carriage.heading_yaw = heading;
    local->world_carriage.route_amount =
        route->from_id == sim->journey.origin_id ? amount : 1.0f - amount;
    local->world_carriage.pace = local->convoy.pace;
    local->world_carriage.camera_target = 1.0f;
    local->world_carriage.route_id = sim->journey.route_id;
    local->world_carriage.visible = true;
    local->world_carriage.hero_embarked = true;
}

static float OpenWorldSettlementDistance(const CcSim *sim,
                                         const LocalState *local)
{
    if (sim == NULL || local == NULL || !local->open_world) return 1000000.0f;
    const CcWorldSettlementPlacement *place =
        CcWorldSettlementPlacementForId(
            &local->world_stream.manifest, sim->player.location_id);
    if (place == NULL) return 1000000.0f;
    float dx = local->agent.position.x - place->center.x;
    float dz = local->agent.position.z - place->center.z;
    return sqrtf(dx * dx + dz * dz);
}

static bool LocalSessionEligible(const LocalState *local)
{
    return local != NULL && !local->road_choice_active &&
           !local->journey_travel_active &&
           !local->site_travel_active &&
           !local->journey_combat_active && !local->journey_parley_active &&
           !CcLocalCourseHasNearbyHostile(&local->course, &local->agent) &&
           (!local->open_world || !local->market_interior) &&
           local->agent.combat.life_state == CC_LIFE_ALIVE;
}

static CcClientSessionScene ClientSceneForLocalState(const LocalState *local)
{
    if (local->market_interior) return CC_CLIENT_SESSION_MARKET;
    switch (local->site_kind) {
        case CC_LOCAL_SITE_DUNGEON: return CC_CLIENT_SESSION_DUNGEON_SITE;
        case CC_LOCAL_SITE_GOBLIN_CAVE: return CC_CLIENT_SESSION_GOBLIN_SITE;
        case CC_LOCAL_SITE_DRAGON_CAVE: return CC_CLIENT_SESSION_DRAGON_SITE;
        case CC_LOCAL_SITE_NONE: return CC_CLIENT_SESSION_STREET;
    }
    return CC_CLIENT_SESSION_STREET;
}

static CcLocalSiteKind LocalSiteForClientScene(CcClientSessionScene scene)
{
    switch (scene) {
        case CC_CLIENT_SESSION_DUNGEON_SITE:
            return CC_LOCAL_SITE_DUNGEON;
        case CC_CLIENT_SESSION_GOBLIN_SITE:
            return CC_LOCAL_SITE_GOBLIN_CAVE;
        case CC_CLIENT_SESSION_DRAGON_SITE:
            return CC_LOCAL_SITE_DRAGON_CAVE;
        case CC_CLIENT_SESSION_STREET:
        case CC_CLIENT_SESSION_MARKET:
            return CC_LOCAL_SITE_NONE;
    }
    return CC_LOCAL_SITE_NONE;
}

static bool SaveLocalSession(const char *path, const CcSim *sim,
                             const LocalState *local,
                             char *error, size_t error_capacity)
{
    if (!LocalSessionEligible(local)) return true;
    CcClientSession session = {
        .version = CC_CLIENT_SESSION_VERSION,
        .world_seed = sim->world_seed,
        .location_id = sim->player.location_id,
        .scene = ClientSceneForLocalState(local),
        .coordinate_space = local->open_world ?
            CC_CLIENT_SESSION_WORLD : CC_CLIENT_SESSION_LEGACY_LOCAL,
        .route_id = local->open_world ?
            local->world_carriage.route_id : 0U,
        .position_x = local->agent.position.x,
        .position_z = local->agent.position.z,
        .facing_yaw = local->agent.facing_yaw,
        .opening_step = (uint32_t)local->opening_step
    };
    return CcClientSessionWrite(path, &session, error, error_capacity);
}

static bool WorldStreamCanRestoreSession(
    const CcSim *sim, const LocalState *local,
    const CcClientSession *session, const CcRoute **route)
{
    if (sim == NULL || local == NULL || session == NULL ||
        local->world_stream.manifest.world_seed != sim->world_seed ||
        local->world_stream.manifest.generator_version !=
            sim->generator_version ||
        CcWorldSettlementPlacementForId(
            &local->world_stream.manifest, session->location_id) == NULL ||
        !CcWorldManifestContains(&local->world_stream.manifest,
                                 session->position_x,
                                 session->position_z)) {
        return false;
    }
    const CcRoute *restored_route = session->route_id != 0U ?
        CcSimRoute(sim, session->route_id) :
        InferWorldSessionRoute(
            sim, &local->world_stream.manifest, session);
    if (restored_route == NULL ||
        (restored_route->from_id != session->location_id &&
         restored_route->to_id != session->location_id) ||
        CcWorldRoutePlacementForId(
            &local->world_stream.manifest, restored_route->id) == NULL) {
        return false;
    }
    const CcWorldRoutePlacement *placement = CcWorldRoutePlacementForId(
        &local->world_stream.manifest, restored_route->id);
    float road_distance = FLT_MAX;
    float heading_distance = FLT_MAX;
    if (!WorldSessionRouteScore(
            placement, session->location_id,
            (CcWorldPoint){session->position_x, session->position_z},
            session->facing_yaw, &road_distance, &heading_distance) ||
        road_distance > 49.0f) {
        return false;
    }
    if (route != NULL) *route = restored_route;
    return true;
}

static bool RestoreWorldSession(const CcSim *sim, LocalState *local,
                                const CcClientSession *session)
{
    const CcRoute *route = NULL;
    if (!WorldStreamCanRestoreSession(sim, local, session, &route)) {
        return false;
    }

    LeaveOpenWorld(local);
    ResetLocalState(local);
    local->world_carriage = (CcLocalWorldCarriageState){0};
    local->open_world = true;
    BindOpenWorldForLocalState(local);
    CcWorldStreamUpdate(&local->world_stream,
                        session->position_x, session->position_z,
                        CC_WORLD_STREAM_CAPACITY);
    RepositionHero(local,
                   (Vector2){session->position_x, session->position_z}, false);
    CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_STREET);
    local->course.scene = CC_LOCAL_SCENE_STREET;
    local->course.alarm_countdown = 1000.0f;
    local->agent.facing_yaw = session->facing_yaw;
    local->opening_step = CC_LOCAL_OPENING_COMPLETE;
    local->road_choice_active = true;
    local->fork_turn_progress = 1.0f;
    local->departure = (CcClientDepartureTransition){
        .phase = CC_CLIENT_DEPARTURE_READY,
        .town_progress = 1.0f,
        .road_book_progress = 1.0f,
    };
    local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
    local->convoy.phase_progress = 1.0f;
    local->convoy.pace = 0.0f;
    local->world_carriage = (CcLocalWorldCarriageState){
        .position = local->agent.position,
        .heading_yaw = session->facing_yaw,
        .pace = 0.0f,
        .camera_weight = 1.0f,
        .camera_target = 1.0f,
        .route_id = route->id,
        .visible = true,
        .hero_embarked = true,
    };
    return true;
}

static bool RestoreLocalSession(const char *path, const CcSim *sim,
                                LocalState *local)
{
    CcClientSession session = {0};
    char error[192];
    if (!CcClientSessionRead(path, &session, error, sizeof(error)) ||
        session.world_seed != sim->world_seed ||
        session.location_id != sim->player.location_id) {
        return false;
    }
    if (session.coordinate_space == CC_CLIENT_SESSION_WORLD) {
        return RestoreWorldSession(sim, local, &session);
    }

    LeaveOpenWorld(local);
    ResetLocalState(local);
    bool market = session.scene == CC_CLIENT_SESSION_MARKET;
    CcLocalSiteKind site = LocalSiteForClientScene(session.scene);
    bool in_bounds = market ?
        session.position_x >= 0.5f && session.position_x <= 12.0f &&
        session.position_z >= 0.5f && session.position_z <= 8.0f :
        session.position_x >= 0.5f &&
        session.position_x <= CC_LOCAL_WORLD_WIDTH - 0.5f &&
        session.position_z >= 0.5f &&
        session.position_z <= CC_LOCAL_WORLD_DEPTH - 0.5f;
    if (!in_bounds) return false;
    RepositionHero(local,
                   (Vector2){session.position_x, session.position_z}, market);
    local->market_interior = market;
    local->site_kind = site;
    if (site != CC_LOCAL_SITE_NONE) {
        CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_ROAD);
        local->course.scene = CC_LOCAL_SCENE_ROAD;
        local->course.alarm_countdown = 1000.0f;
    }
    local->agent.facing_yaw = session.facing_yaw;
    local->opening_step = (CcLocalOpeningStep)session.opening_step;
    if (sim->player.accepted_situation_id != 0U ||
        sim->player.reputation != 0) {
        local->opening_step = CC_LOCAL_OPENING_COMPLETE;
    } else if (local->opening_step != CC_LOCAL_OPENING_COMPLETE) {
        /* Old saves may still contain the removed Jory tutorial step. */
        local->opening_step = CC_LOCAL_OPENING_MEET_MARA;
    }
    return true;
}

static bool RestoreClientStartupSession(const char *path, const CcSim *sim,
                                        LocalState *local, ClientView *view,
                                        int32_t *selected)
{
    if (view == NULL || !RestoreLocalSession(path, sim, local)) return false;
    *view = local->open_world ? VIEW_ROADS : VIEW_LOCAL;
    if (local->open_world && selected != NULL) {
        *selected = OpenWorldRouteIndex(
            sim, local->world_carriage.route_id);
    }
    return true;
}

static void BeginRoadLocalState(const CcSim *sim, LocalState *local,
                                bool hostile)
{
    CcAthleticProfile athletics = local->agent.athletics;
    float lateral_offset = local->convoy.lateral_offset;
    float pace = local->convoy.pace;
    LeaveOpenWorld(local);
    ResetLocalState(local);
    local->agent.athletics = athletics;
    local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
    local->convoy.lateral_offset = lateral_offset;
    local->convoy.pace = pace;
    if (!local->open_world) {
        RepositionHero(local,
                       (Vector2){CC_LOCAL_ROAD_START_X,
                                 CC_LOCAL_ROAD_START_Z}, false);
    } else {
        PositionOpenWorldJourney(sim, local);
    }
    CcLocalCourseStageRoadEncounter(&local->course, &local->agent,
                                    hostile);
    if (local->open_world) {
        float translate_x = local->agent.position.x - 47.20f;
        float translate_z = local->agent.position.z - 40.00f;
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            local->course.runners[i].agent.position.x += translate_x;
            local->course.runners[i].agent.position.z += translate_z;
            local->course.runners[i].agent.position.y =
                CcWorldStreamHeightAt(
                    &local->world_stream,
                    local->course.runners[i].agent.position.x,
                    local->course.runners[i].agent.position.z);
            local->course.runners[i].agent.humanoid_needs_reset = true;
            local->course.guard_entry[i].x += translate_x;
            local->course.guard_entry[i].z += translate_z;
        }
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            local->course.raiders[i].position.x += translate_x;
            local->course.raiders[i].position.z += translate_z;
            local->course.raiders[i].position.y = CcWorldStreamHeightAt(
                &local->world_stream,
                local->course.raiders[i].position.x,
                local->course.raiders[i].position.z);
            local->course.raiders[i].humanoid_needs_reset = true;
            local->course.raider_entry[i].x += translate_x;
            local->course.raider_entry[i].z += translate_z;
        }
        local->course.combat_origin.x += translate_x;
        local->course.combat_origin.z += translate_z;
        CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_STREET);
        local->course.scene = CC_LOCAL_SCENE_STREET;
    }
    CcLocalCourseBindRaiderCompany(&local->course, sim);
    if (local->open_world) {
        local->world_carriage.hero_embarked = false;
        local->world_carriage.pace = 0.0f;
        local->world_carriage.camera_target = 0.38f;
    }
    local->journey_combat_active = hostile;
    local->journey_parley_active = !hostile;
}

static void BeginRoadTravelState(const CcSim *sim, LocalState *local)
{
    CcAthleticProfile athletics = local->agent.athletics;
    float lateral_offset = local->convoy.lateral_offset;
    float pace = local->convoy.pace;
    ResetLocalState(local);
    local->agent.athletics = athletics;
    local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
    local->convoy.lateral_offset = lateral_offset;
    local->convoy.pace = sim != NULL && sim->journey.active ?
        CcClientConvoyPosturePace((int32_t)sim->journey.pace) :
        (pace > 0.05f ? pace : 0.72f);
    if (EnterOpenWorldJourney(sim, local)) {
        CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_STREET);
        local->course.scene = CC_LOCAL_SCENE_STREET;
    } else {
        RepositionHero(local,
                       (Vector2){CC_LOCAL_ROAD_START_X,
                                 CC_LOCAL_ROAD_START_Z}, false);
        CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_ROAD);
        local->course.scene = CC_LOCAL_SCENE_ROAD;
    }
    local->course.alarm_countdown = 1000.0f;
    if (local->open_world) {
        local->world_carriage.hero_embarked = true;
        local->world_carriage.camera_target = 1.0f;
    }
    local->journey_travel_active = true;
}

static void BeginRoadChoiceApproachState(LocalState *local, bool from_town)
{
    CcAthleticProfile athletics = local->agent.athletics;
    float pace = local->convoy.pace;
    ResetLocalState(local);
    local->agent.athletics = athletics;
    local->road_choice_active = true;
    local->convoy.phase = from_town ? CC_LOCAL_CONVOY_DEPARTING :
                                      CC_LOCAL_CONVOY_ROAD;
    local->convoy.pace = from_town ? 0.0f :
        (pace > 0.05f ? pace : 0.72f);
    local->convoy.phase_progress = 0.0f;
    local->course.alarm_countdown = 1000.0f;
    if (from_town) {
        CcClientDepartureBegin(&local->departure);
        SetConvoyTownPose(&local->convoy, 0.0f);
    } else {
        local->departure = (CcClientDepartureTransition){
            .phase = CC_CLIENT_DEPARTURE_READY,
            .town_progress = 1.0f,
            .road_book_progress = 1.0f,
        };
        RepositionHero(local,
                       (Vector2){CC_LOCAL_ROAD_START_X,
                                 CC_LOCAL_ROAD_START_Z}, false);
        CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_ROAD);
        local->course.scene = CC_LOCAL_SCENE_ROAD;
    }
}

static bool UpdateRoadChoiceApproach(LocalState *local, float delta_time)
{
    ConvoyUpdateResult update = UpdateDrivenConvoy(local, NULL, delta_time);
    return update == CONVOY_UPDATE_OPEN_ROAD_BOOK;
}

static void BeginTownArrivalState(LocalState *local)
{
    CcAthleticProfile athletics = local->agent.athletics;
    float pace = local->convoy.pace;
    CcClientArrivalTransition arrival = local->arrival;
    if (arrival.phase != CC_CLIENT_ARRIVAL_TOWN) {
        arrival = (CcClientArrivalTransition){
            .phase = CC_CLIENT_ARRIVAL_TOWN,
            .road_book_progress = 1.0f,
        };
    }
    LeaveOpenWorld(local);
    ResetLocalState(local);
    local->agent.athletics = athletics;
    local->arrival = arrival;
    local->convoy.phase = CC_LOCAL_CONVOY_ARRIVING;
    local->convoy.pace = pace > 0.05f ? pace : 0.48f;
    local->convoy.phase_progress = arrival.town_progress;
    SetConvoyTownPose(&local->convoy, 0.0f);
    local->course.alarm_countdown = 1000.0f;
    local->journey_travel_active = true;
}

static void BeginRoadBookArrivalState(const CcSim *sim, LocalState *local)
{
    if (local == NULL) return;
    if (sim == NULL || !local->open_world || sim->journey.route_id == 0U) {
        BeginTownArrivalState(local);
        return;
    }
    CcClientArrivalBegin(&local->arrival);
    local->journey_travel_active = true;
    local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
    local->convoy.pace = local->convoy.pace > 0.05f ?
        local->convoy.pace : 0.48f;
    local->world_carriage.camera_weight =
        CcClientArrivalCameraWeight(&local->arrival);
    local->world_carriage.camera_target = 0.0f;
    local->world_carriage.town_arrival = true;
    if (!PositionOpenWorldArrival(sim, local)) {
        BeginTownArrivalState(local);
    }
}

typedef enum SiteTravelResult {
    SITE_TRAVEL_NONE = 0,
    SITE_TRAVEL_ARRIVED,
    SITE_TRAVEL_RETURNED
} SiteTravelResult;

static void BeginSiteTravelState(LocalState *local, CcLocalSiteKind site,
                                 bool returning)
{
    CcAthleticProfile athletics = local->agent.athletics;
    CcNpcAppearance appearance = local->agent.appearance;
    CcMorphologyPreset morphology = local->agent.morphology;
    Color tunic_color = local->agent.tunic_color;
    bool crowned = local->agent.crowned;
    ResetLocalState(local);
    local->agent.athletics = athletics;
    local->agent.appearance = appearance;
    local->agent.morphology = morphology;
    local->agent.tunic_color = tunic_color;
    local->agent.crowned = crowned;
    local->site_kind = site;
    local->site_travel_active = true;
    local->site_returning = returning;
    local->site_travel_progress = 0.0f;
    local->convoy.phase = CC_LOCAL_CONVOY_ROAD;
    local->convoy.pace = 0.72f;
    RepositionHero(local,
                   (Vector2){CC_LOCAL_SITE_CARRIAGE_X + 3.0f,
                             CC_LOCAL_SITE_CARRIAGE_Z}, false);
    CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_ROAD);
    local->course.scene = CC_LOCAL_SCENE_ROAD;
    local->course.alarm_countdown = 1000.0f;
}

static SiteTravelResult UpdateSiteTravelState(LocalState *local,
                                              float delta_time)
{
    if (local == NULL || !local->site_travel_active) {
        return SITE_TRAVEL_NONE;
    }
    (void)UpdateDrivenConvoy(local, NULL, delta_time);
    local->site_travel_progress = ClampUnit(
        local->site_travel_progress +
        delta_time * 0.18f * local->convoy.pace);
    if (local->site_travel_progress < 1.0f) return SITE_TRAVEL_NONE;
    if (local->site_returning) {
        CcAthleticProfile athletics = local->agent.athletics;
        ResetLocalState(local);
        local->agent.athletics = athletics;
        return SITE_TRAVEL_RETURNED;
    }
    local->site_travel_active = false;
    local->site_returning = false;
    local->convoy.phase = CC_LOCAL_CONVOY_PARKED;
    local->convoy.pace = 0.0f;
    RepositionHero(local,
                   (Vector2){CC_LOCAL_SITE_CARRIAGE_X + 3.0f,
                             CC_LOCAL_SITE_CARRIAGE_Z}, false);
    CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_ROAD);
    local->course.scene = CC_LOCAL_SCENE_ROAD;
    local->course.alarm_countdown = 1000.0f;
    return SITE_TRAVEL_ARRIVED;
}

static void EnterSiteFromGoblinTunnel(LocalState *local,
                                      CcLocalSiteKind site,
                                      Vector2 position)
{
    CcAthleticProfile athletics = local->agent.athletics;
    CcNpcAppearance appearance = local->agent.appearance;
    CcMorphologyPreset morphology = local->agent.morphology;
    Color tunic_color = local->agent.tunic_color;
    bool crowned = local->agent.crowned;
    ResetLocalState(local);
    local->agent.athletics = athletics;
    local->agent.appearance = appearance;
    local->agent.morphology = morphology;
    local->agent.tunic_color = tunic_color;
    local->agent.crowned = crowned;
    local->site_kind = site;
    local->convoy.phase = CC_LOCAL_CONVOY_PARKED;
    RepositionHero(local, position, false);
    CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_ROAD);
    local->course.scene = CC_LOCAL_SCENE_ROAD;
    local->course.alarm_countdown = 1000.0f;
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


static void DrawLocalHeader(const CcSim *sim, const LocalState *local,
                            bool conversation)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(place);
    bool finding_road = local->road_choice_active;
    bool site = local->site_kind != CC_LOCAL_SITE_NONE;
    bool road = finding_road || local->journey_travel_active ||
                local->site_travel_active ||
                local->journey_combat_active ||
                local->journey_parley_active;
    const CcSettlement *origin = finding_road || site ? place : road ?
        CcSimSettlement(sim, sim->journey.origin_id) : NULL;
    const CcSettlement *destination = road && !finding_road && !site ?
        CcSimSettlement(sim, sim->journey.destination_id) : NULL;
    if (local->opening_step != CC_LOCAL_OPENING_COMPLETE) {
        CcOverlayDrawText("THORNFORD  /  THE FIRST BELL",
                          22, 18, 15, INK);
        const char *beat = "OBJECTIVE  /  Talk to Mara";
        int beat_width = CcOverlayMeasureText(beat, 8) + 14;
        DrawRectangleRounded((Rectangle){18.0f, 39.0f,
                                          (float)beat_width, 17.0f},
                             0.24f, 4, Fade(PANEL_DEEP, 0.84f));
        CcOverlayDrawText(beat, 25, 44, 8, TEAL);
        const char *hint = conversation ? "CHOOSE A REPLY BELOW" :
                                          "CLICK THE ROAD TO WALK";
        int hint_width = CcOverlayMeasureText(hint, 8);
        CcOverlayDrawText(hint, GetScreenWidth() - hint_width - 22,
                          23, 8, MUTED);
        return;
    }
    CcOverlayDrawText(site ?
             local->site_travel_active ?
                 TextFormat("%s  ->  %s",
                            origin != NULL ? origin->name : "Town",
                            CcLocalSiteName(sim, local->site_kind)) :
                 TextFormat("%s  /  LOCAL SITE",
                            CcLocalSiteName(sim, local->site_kind)) :
             finding_road ?
             TextFormat("%s  ->  ROAD AHEAD",
                        origin != NULL ? origin->name : "Town") :
             road ?
             TextFormat("%s  ->  %s",
                        origin != NULL ? origin->name : "Road",
                        destination != NULL ? destination->name : "Gate") :
             local->market_interior && place != NULL ?
                 TextFormat("%s  /  %s", place->name,
                            profile->primary_hall) :
             place != NULL ?
                 TextFormat("%s  /  %s", place->name, profile->identity) :
                 "Crownless",
             22, 18, 18, INK);
    const char *summary = TextFormat(
        "DAY %d     %" PRId64 " cr     CARGO %d/%d",
        sim->current_day, sim->player.coins,
        CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity);
    int summary_width = CcOverlayMeasureText(summary, 10);
    CcOverlayDrawText(summary, GetScreenWidth() - summary_width - 22,
                      22, 10, road ? TEAL : CC_GOLD);
    if (!road && !site && place != NULL) {
        const CcSituation *accepted = CcSimAcceptedSituation(sim);
        if (accepted != NULL) {
            char next_action[160] = "";
            SituationNextAction(sim, accepted, next_action,
                                sizeof(next_action));
            CcOverlayDrawText(TextFormat("NEXT  /  %.96s", next_action),
                              22, 40, 8, TEAL);
        } else if (!local->market_interior) {
            CcOverlayDrawText(
                TextFormat("NEXT  /  Visit the %s.    MAP  /  %s",
                           profile->notice_board, profile->map_form),
                22, 40, 8, MUTED);
        } else {
            CcOverlayDrawText(
                TextFormat("%s  /  %s", profile->identity,
                           profile->purpose),
                22, 40, 8, MUTED);
        }
    }
}

static bool LocalCombatActive(const LocalState *local)
{
    return local != NULL && !local->market_interior &&
           CcLocalCourseHasNearbyHostile(&local->course, &local->agent);
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
                          "CHOOSE BELOW / [T] NEXT",
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

static const char *JourneyRoadBeatName(int32_t progress_milli)
{
    if (progress_milli < 250) return "MORNING WATCH · OUTER FARMS";
    if (progress_milli < 500) return "AFTERNOON WATCH · ROAD SIGNS";
    if (progress_milli < 750) return "NIGHT WATCH · CAMP OR PRESS ON";
    return "LAST MILES · THE FAR GATE";
}

static void DrawLocalPanel(const CcSim *sim, const LocalState *local)
{
    if (local->site_kind != CC_LOCAL_SITE_NONE) {
        float panel_x = (float)GetScreenWidth() - 286.0f;
        int content_x = (int)panel_x + 18;
        DrawPanel((Rectangle){panel_x, 78.0f, 264.0f, 150.0f},
                  Fade(PANEL_DEEP, 0.93f));
        const char *site_panel_title = local->site_travel_active ?
            local->site_kind == CC_LOCAL_SITE_GOBLIN_CAVE ?
                "HIDDEN TRAILHEAD" : "SITE ROAD" :
            local->site_kind == CC_LOCAL_SITE_DRAGON_CAVE ?
                "DRAGON MOUNTAIN" : "LOCAL MAP";
        CcOverlayDrawText(site_panel_title,
                          content_x, 91, 9, TEAL);
        CcOverlayDrawText(TextFormat("%.24s",
                                    CcLocalSiteName(sim, local->site_kind)),
                          content_x, 112, 15, INK);
        if (local->site_travel_active) {
            DrawBar(content_x, 145, 74, "PROGRESS",
                    (int32_t)lroundf(local->site_travel_progress * 100.0f),
                    TEAL);
            DrawBar(content_x, 166, 74, "PACE",
                    (int32_t)lroundf(local->convoy.pace * 100.0f), CC_GOLD);
            CcOverlayDrawText("W FASTER  /  S SLOWER  /  SPACE PAUSE",
                              content_x, 205, 7, MUTED);
        } else if (local->site_kind == CC_LOCAL_SITE_DRAGON_CAVE) {
            CcOverlayDrawText("NO ROAD / NO CARRIAGE",
                              content_x, 151, 8, CC_GOLD);
            CcOverlayDrawText("ROOST REACHED FROM INSIDE ONLY",
                              content_x, 178, 7, MUTED);
        } else {
            CcOverlayDrawText("CARRIAGE PARKED AT THE ROAD EDGE",
                              content_x, 151, 8, CC_GOLD);
            CcOverlayDrawText("WALK TO THE ENTRANCE OR DRIVE BACK",
                              content_x, 178, 7, MUTED);
        }
        return;
    }
    bool road = local->road_choice_active ||
                local->journey_travel_active ||
                local->journey_combat_active ||
                local->journey_parley_active;
    if (road) {
        const CcSettlement *destination = CcSimSettlement(
            sim, sim->journey.destination_id);
        if (local->journey_combat_active &&
            SelectedCombatTargetIndex(local) >= 0) {
            DrawCombatPanel(local);
            return;
        }
        float panel_x = (float)GetScreenWidth() - 286.0f;
        int content_x = (int)panel_x + 18;
        DrawPanel((Rectangle){panel_x, 78.0f, 264.0f, 184.0f},
                  Fade(PANEL_DEEP, 0.93f));
        if (local->road_choice_active) {
            bool town_departure =
                local->convoy.phase == CC_LOCAL_CONVOY_DEPARTING;
            bool raising_road_book = RoadBookDepartureInProgress(local) ||
                (local->open_world &&
                 local->world_carriage.camera_weight < 1.0f);
            int32_t progress = (int32_t)lroundf(
                (town_departure ? local->convoy.phase_progress :
                 local->world_carriage.camera_weight) * 100.0f);
            CcOverlayDrawText(raising_road_book ? "RAISING THE ROAD BOOK" :
                              town_departure ? "LEAVING THE STABLE" :
                                               "JUNCTION REACHED",
                              content_x, 91, 9, TEAL);
            CcOverlayDrawText("NEXT JUNCTION", content_x, 112, 15, INK);
            DrawBar(content_x, 145, 74, "APPROACH", progress, TEAL);
            DrawBar(content_x, 166, 74, "PACE",
                    (int32_t)lroundf(local->convoy.pace * 100.0f), CC_GOLD);
            CcOverlayDrawText(raising_road_book ?
                                  "THE CAMERA FOLLOWS THE TEAM" :
                              town_departure ?
                                  "W FASTER  /  S SLOWER  /  SPACE PAUSE" :
                                  "CHOOSE A BRANCH OR TURN BACK",
                              content_x, 205, 7, MUTED);
            return;
        }
        if (local->journey_travel_active) {
            bool town_departure =
                local->convoy.phase == CC_LOCAL_CONVOY_DEPARTING;
            bool arrival = local->convoy.phase == CC_LOCAL_CONVOY_ARRIVING;
            bool closing_road_book = RoadBookArrivalInProgress(local);
            int32_t progress = closing_road_book ?
                (int32_t)lroundf(
                    local->arrival.road_book_progress * 100.0f) :
                arrival || town_departure ?
                (int32_t)lroundf(local->convoy.phase_progress * 100.0f) :
                sim->carriage.progress_milli / 10;
            CcOverlayDrawText(closing_road_book ? "CLOSING THE ROAD BOOK" :
                              arrival ? "ENTERING TOWN" :
                              town_departure ? "LEAVING THE STABLE" :
                              "ON THE ROAD",
                              content_x, 91, 9, TEAL);
            CcOverlayDrawText(destination != NULL ? destination->name : "THE FAR GATE",
                     content_x, 112, 15, INK);
            DrawBar(content_x, 145, 74, "PROGRESS", progress, TEAL);
            if (sim->journey.active) {
                int32_t eta_hours =
                    (CcSimJourneyEtaMinutes(sim) + 59) / 60;
                CcOverlayDrawText(
                    TextFormat("%s / %s / ETA %dD %dH",
                               CcJourneyPaceName(sim->journey.pace),
                               CcClientConvoyGaitName(
                                   CcClientConvoyGaitForPace(
                                       local->convoy.pace)),
                               eta_hours / 24, eta_hours % 24),
                    content_x, 170, 8, CC_GOLD);
                CcOverlayDrawText(
                    TextFormat("TEAM %d%%  /  WAGON %d%%",
                               CcSimHorseTeamReadiness(sim),
                               sim->carriage.condition),
                    content_x, 189, 8, INK);
                CcOverlayDrawText(
                    sim->journey.ambush_warned &&
                    sim->journey.ambush_pending ?
                        "SCOUTS: RIDERS SHADOWING THE ROAD" :
                        JourneyRoadBeatName(sim->carriage.progress_milli),
                    content_x, 208, 7,
                    sim->journey.ambush_warned &&
                    sim->journey.ambush_pending ? DANGER : TEAL);
                CcOverlayDrawText(
                    "W PUSH  /  S CAREFUL  /  SPACE HOLD",
                    content_x, 238, 7, MUTED);
            } else {
                CcOverlayDrawText(closing_road_book ?
                                      "THE ROAD CLOSES ON THE TOWN GATE" :
                                      "GUIDE THE TEAM INTO ITS BAY",
                                  content_x, 188, 8, MUTED);
            }
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
    if (!local->market_interior && local->course.alarm_active &&
        SelectedCombatTargetIndex(local) >= 0) {
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

static void ReleaseMapTexture(Texture2D *texture, bool *attempted)
{
    if (texture == NULL || attempted == NULL) return;
    if (texture->id != 0U) UnloadTexture(*texture);
    *texture = (Texture2D){0};
    *attempted = false;
}

static void ReleaseMapTextures(ClientMapTextures *textures)
{
    if (textures == NULL) return;
    ReleaseMapTexture(&textures->illustrated,
                      &textures->illustrated_attempted);
    ReleaseMapTexture(&textures->collectible_atlas,
                      &textures->collectible_atlas_attempted);
}

static bool LoadMapTexture(Texture2D *texture, bool *attempted,
                           const char *relative_path, const char *label)
{
    if (texture == NULL || attempted == NULL || relative_path == NULL) {
        return false;
    }
    if (texture->id != 0U) return true;
    if (*attempted) return false;
    *attempted = true;
#if defined(PLATFORM_WEB)
    if (!FileExists(relative_path) &&
        emscripten_wget(relative_path, relative_path) != 0) {
        TraceLog(LOG_WARNING, "WEB: could not fetch %s", label);
        return false;
    }
#endif
    char resolved[1024];
    bool found = ResolveClientAssetPath(relative_path, resolved,
                                        sizeof(resolved));
    if (found) *texture = LoadTexture(resolved);
#if defined(PLATFORM_WEB)
    if (found) (void)remove(resolved);
#endif
    if (texture->id == 0U) {
        TraceLog(LOG_WARNING, "MAP: could not load %s", label);
        return false;
    }
    SetTextureFilter(*texture, TEXTURE_FILTER_BILINEAR);
    return true;
}

static void PrepareMapTextures(const CcSim *sim, int32_t selected,
                               ClientMapTextures *textures)
{
    if (textures == NULL) return;
    const CcMap *map = SelectedVisibleMap(sim, selected);
    bool needs_illustrated = IsGloamgateAlderwatchMap(map);
    int32_t slot = map != NULL ? (int32_t)(map - sim->maps) : -1;
    bool needs_atlas = slot >= 0 && slot < CC_MAP_COLLECTION_COUNT &&
                       slot != CC_MAP_DRAGON_HOARD && !needs_illustrated;
#if defined(PLATFORM_WEB)
    if (!needs_illustrated) {
        ReleaseMapTexture(&textures->illustrated,
                          &textures->illustrated_attempted);
    }
    if (!needs_atlas) {
        ReleaseMapTexture(&textures->collectible_atlas,
                          &textures->collectible_atlas_attempted);
    }
#endif
    if (needs_illustrated) {
        (void)LoadMapTexture(&textures->illustrated,
                             &textures->illustrated_attempted,
                             CC_GLOAMGATE_ALDERWATCH_MAP_ASSET,
                             "Gloamgate road map");
    } else if (needs_atlas) {
        (void)LoadMapTexture(&textures->collectible_atlas,
                             &textures->collectible_atlas_attempted,
                             CC_COLLECTIBLE_MAP_ATLAS_ASSET,
                             "collectible map atlas");
    }
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

static int32_t OutgoingRouteOrdinal(const CcSim *sim, int32_t selected)
{
    int32_t ordinal = 0;
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (!RouteLeavesCurrentPlace(sim, &sim->routes[i])) continue;
        if (i == selected) return ordinal;
        ordinal += 1;
    }
    return -1;
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

static const CcTreasure *FirstDragonTreasure(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        if (!treasure->destroyed && treasure->owner_id == sim->dragon.id &&
            treasure->location_id == sim->dragon.lair_settlement_id) {
            return treasure;
        }
    }
    return NULL;
}

static void DrawDragonCavePanel(const CcSim *sim)
{
    if (sim == NULL) return;
    const CcDragon *dragon = &sim->dragon;
    float width = 790.0f;
    float height = 490.0f;
    Rectangle bounds = {
        ((float)GetScreenWidth() - width) * 0.5f,
        ((float)GetScreenHeight() - height) * 0.5f - 8.0f,
        width, height
    };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.76f));
    DrawPanel(bounds, Fade(PANEL_DEEP, 0.98f));
    int x = (int)bounds.x + 28;
    int y = (int)bounds.y + 23;
    Color state_color = dragon->slain ? CC_VIOLET :
        dragon->activity == CC_DRAGON_ACTIVITY_RETALIATING ? DANGER : CC_GOLD;
    CcOverlayDrawText("DRAGON CAVE", x, y, 11, state_color);
    CcOverlayDrawText(dragon->name, x, y + 25, 25, INK);
    CcOverlayDrawText(
        TextFormat("%s  /  %s  /  age %d years",
                   CcDragonLifeStageName(dragon->life_stage),
                   CcDragonActivityName(dragon->activity),
                   dragon->age_days / 365),
        x, y + 59, 11, MUTED);

    int named_count = CcSimTreasureCountForOwner(sim, dragon->id);
    CcOverlayDrawText(
        TextFormat("HOARD  %" PRId64 " crowns   GOLD %d   GEMS %d   RELICS %d",
                   dragon->hoard, dragon->hoard_goods[CC_GOOD_GOLD],
                   dragon->hoard_goods[CC_GOOD_GEMS], named_count),
        x, y + 92, 12, CC_GOLD);
    CcOverlayDrawText(
        TextFormat("BATTLE STRENGTH  %d   GOBLIN COURT  %d members / %d devotion",
                   CcSimDragonBattleStrength(sim), sim->goblins.members,
                   sim->goblins.devotion),
        x, y + 116, 10, INK);

    int left = x;
    int right = x + 370;
    DrawBar(left, y + 158, 205, "BODY", dragon->body_condition, TEAL);
    DrawBar(left, y + 188, 205, "CROWN", dragon->crown_strength, CC_GOLD);
    DrawBar(left, y + 218, 205, "MEMORY", dragon->memory_integrity,
            CC_VIOLET);
    DrawBar(right, y + 158, 205, "TERRITORY",
            dragon->territory_stability, TEAL);
    DrawBar(right, y + 188, 205, "SHADOW",
            dragon->regional_influence, CC_VIOLET);

    int detail_y = y + 268;
    if (dragon->slain) {
        CcOverlayDrawText(
            TextFormat("AFTERDRAGON  day %d  /  the shadow now changes nearby roads",
                       dragon->afterdeath_days),
            x, detail_y, 12, CC_VIOLET);
        CcOverlayDrawText(
            dragon->egg_count > 0 ?
                TextFormat("VISIBLE CLUTCH  %d egg%s  /  successor in %d days",
                           dragon->egg_count,
                           dragon->egg_count == 1 ? "" : "s",
                           dragon->brood_days_remaining) :
                "NO VISIBLE CLUTCH  /  this dragon line ends here",
            x, detail_y + 27, 11, INK);
        DrawTwoLineText(
            "Ash fields, heat vents, goblin shrines, and unsafe roads remain after the body is gone.",
            x, detail_y + 57, 78, 10, MUTED);
    } else if (dragon->stolen_outstanding > 0) {
        const CcSettlement *target = CcSimSettlement(
            sim, dragon->retaliation_target_id);
        CcOverlayDrawText(
            TextFormat("OPEN WOUND  %" PRId64 " crowns  /  %d nights remain",
                       dragon->stolen_outstanding,
                       dragon->omen_days_remaining),
            x, detail_y, 12, DANGER);
        CcOverlayDrawText(
            TextFormat("RETALIATION TARGET  %s",
                       target != NULL ? target->name : "the richest realm"),
            x, detail_y + 27, 11, INK);
        DrawTwoLineText(
            dragon->stolen_treasure_id != 0U ?
                "Only the exact named relic can repair this memory wound." :
                "Return the missing crowns before the omen reaches zero.",
            x, detail_y + 57, 78, 10, MUTED);
    } else {
        CcOverlayDrawText(
            dragon->egg_count > 0 ?
                TextFormat("BROOD  %d egg%s  /  hatching in %d days",
                           dragon->egg_count,
                           dragon->egg_count == 1 ? "" : "s",
                           dragon->brood_days_remaining) :
                "THE HOARD IS WHOLE  /  the dragon is calm",
            x, detail_y, 12, state_color);
        DrawTwoLineText(
            "Taking treasure weakens crown and memory. The dragon gives fourteen nights to return it before burning a realm.",
            x, detail_y + 36, 82, 10, MUTED);
    }
    CcOverlayDrawText(
        "Click an action below. Number keys choose the same actions.",
        x, (int)(bounds.y + bounds.height) - 31, 9, MUTED);
}

static Vector2 DungeonMapPoint(const CcDungeonRoom *room)
{
    return room == NULL ? (Vector2){0.0f, 0.0f} :
        (Vector2){105.0f + (float)room->map_x * 68.0f,
                  305.0f + (float)room->map_y * 58.0f};
}

static void DrawDungeonPanel(const CcSim *sim)
{
    if (sim == NULL || !sim->dungeon_expedition.active) return;
    const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    const CcDungeon *dungeon = CcSimDungeon(sim, expedition->dungeon_id);
    const CcDungeonRoom *current = CcSimDungeonCurrentRoom(sim);
    if (dungeon == NULL || current == NULL) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.92f));
    DrawPanel((Rectangle){42.0f, 64.0f, 870.0f, 535.0f},
              Fade(PANEL_DEEP, 0.98f));
    DrawPanel((Rectangle){934.0f, 64.0f, 304.0f, 535.0f},
              Fade(PANEL_DEEP, 0.98f));
    CcOverlayDrawText("THE UNDERROAD", 66, 82, 10, TEAL);
    CcOverlayDrawText(dungeon->name, 66, 104, 23, INK);
    CcOverlayDrawText(
        "The map records only passages the company has walked or found.",
        66, 136, 9, MUTED);

    for (int32_t i = 0; i < dungeon->link_count; ++i) {
        const CcDungeonLink *link = &dungeon->links[i];
        const CcDungeonRoom *from = &dungeon->rooms[link->from_room];
        const CcDungeonRoom *to = &dungeon->rooms[link->to_room];
        bool from_known = (from->state_flags &
                           CC_DUNGEON_ROOM_DISCOVERED) != 0U;
        bool to_known = (to->state_flags &
                         CC_DUNGEON_ROOM_DISCOVERED) != 0U;
        bool hidden = (link->kind == CC_DUNGEON_LINK_SECRET ||
                       link->kind == CC_DUNGEON_LINK_SHORTCUT) &&
                      (link->flags & CC_DUNGEON_LINK_DISCOVERED) == 0U;
        if (!from_known || !to_known || hidden) continue;
        Color color = link->kind == CC_DUNGEON_LINK_SHORTCUT ? CC_GOLD :
                      link->kind == CC_DUNGEON_LINK_SECRET ? CC_VIOLET :
                      Fade(TEAL, 0.66f);
        if ((link->flags & CC_DUNGEON_LINK_OPEN) == 0U) color = DANGER;
        DrawLineEx(DungeonMapPoint(from), DungeonMapPoint(to),
                   link->kind == CC_DUNGEON_LINK_SHORTCUT ? 4.0f : 2.0f,
                   color);
    }
    for (int32_t i = 0; i < dungeon->room_count; ++i) {
        const CcDungeonRoom *room = &dungeon->rooms[i];
        if ((room->state_flags & CC_DUNGEON_ROOM_DISCOVERED) == 0U) continue;
        Vector2 point = DungeonMapPoint(room);
        bool here = i == expedition->current_room;
        Color accent = here ? CC_GOLD :
            (room->flags & CC_DUNGEON_ROOM_GOBLIN) != 0U ? CC_VIOLET :
            (room->flags & CC_DUNGEON_ROOM_STONEBACK) != 0U ? TEAL : INK;
        float radius = here ? 12.0f : 7.0f;
        DrawCircleV(point, radius + 4.0f, Fade(accent, 0.18f));
        DrawCircleV(point, radius, accent);
        if ((room->flags & CC_DUNGEON_ROOM_HAZARD) != 0U) {
            DrawCircleLinesV(point, radius + 7.0f, Fade(DANGER, 0.75f));
        }
        CcOverlayDrawText(TextFormat("%.18s", room->name),
                          (int)point.x - 22, (int)point.y + 16, 7,
                          here ? INK : MUTED);
    }

    int x = 958;
    CcOverlayDrawText("CURRENT CHAMBER", x, 86, 9, TEAL);
    CcOverlayDrawText(current->name, x, 110, 18, INK);
    CcOverlayDrawText(
        TextFormat("DEPTH %d  /  %s", current->depth,
                   CcDungeonRoomKindName(current->kind)),
        x, 139, 9, MUTED);
    DrawBar(x, 177, 146, "LIGHT", expedition->light_remaining * 100 / 18,
            expedition->light_remaining <= 4 ? DANGER : CC_GOLD);
    DrawBar(x, 209, 146, "STRAIN", expedition->strain,
            expedition->strain >= 70 ? DANGER : TEAL);
    DrawBar(x, 241, 146, "NOISE", expedition->noise * 10,
            expedition->noise >= 6 ? DANGER : CC_VIOLET);
    CcOverlayDrawText(
        TextFormat("TURN %d  /  %d DAY%s BELOW",
                   expedition->turns_elapsed, expedition->days_elapsed,
                   expedition->days_elapsed == 1 ? "" : "S"),
        x, 278, 9, MUTED);
    CcOverlayDrawText(
        TextFormat("CARGO %d/%d  FOOD %d  TOOLS %d",
                   CcPlayerCargoUsed(&sim->player),
                   sim->player.cargo_capacity,
                   sim->player.cargo[CC_GOOD_FOOD],
                   sim->player.cargo[CC_GOOD_TOOLS]),
        x, 302, 9, INK);

    int detail_y = 341;
    if (expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE) {
        CcOverlayDrawText("ENCOUNTER", x, detail_y, 10, DANGER);
        CcOverlayDrawText(
            CcDungeonEncounterName(expedition->encounter_kind),
            x, detail_y + 25, 13, INK);
        CcOverlayDrawText(
            TextFormat("REACTION %d  /  %s",
                       expedition->encounter_reaction,
                       CcDungeonReactionName(
                           expedition->encounter_reaction)),
            x, detail_y + 51, 8,
            expedition->encounter_reaction <= 5 ? DANGER : TEAL);
        DrawTwoLineText(
            "Parley tests the reaction. Evading favors light loads. Force is loud and dangerous.",
            x, detail_y + 80, 42, 9, MUTED);
    } else {
        const char *sign = (current->flags &
                            CC_DUNGEON_ROOM_STONEBACK) != 0U ?
            "Stone shapes hold tools and lost objects in patterns." :
            (current->flags & CC_DUNGEON_ROOM_GOBLIN) != 0U ?
            "Ash marks name debts, tolls, and tribute roads." :
            (current->flags & CC_DUNGEON_ROOM_DRAGON_SIGN) != 0U ?
            "Warm air and worn tribute tracks lead deeper." :
            (current->flags & CC_DUNGEON_ROOM_HAZARD) != 0U ?
            "The chamber itself is dangerous. Slow work may draw company." :
            "Old freight marks survive beneath soot and mineral bloom.";
        CcOverlayDrawText("SIGNS", x, detail_y, 10, CC_GOLD);
        DrawTwoLineText(sign, x, detail_y + 28, 42, 9, MUTED);
    }
    CcOverlayDrawText(
        "Each move or search spends a turn. Six turns consume Food and a day.",
        66, 569, 9, MUTED);
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
    action->good = CC_GOOD_FOOD;
    action->amount = 0;
    action->enabled = true;
    (void)snprintf(action->label, sizeof(action->label), "%s", label);
}

static void AddCargoContextAction(ContextActionSet *set, CcGood good,
                                  const char *label)
{
    AddContextAction(set, CONTEXT_ACTION_BUY_CARGO, label);
    if (set == NULL || set->count <= 0) return;
    ContextAction *action = &set->items[set->count - 1];
    action->good = good;
    action->amount = 1;
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
        if (SituationVisibleToPlayer(sim, i)) count += 1;
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
        local->course.raiders[target].combat.life_state != CC_LIFE_ALIVE ||
        !CcLocalCourseCanPlayerEngage(
            &local->course, &local->agent, target)) {
        return -1;
    }
    return target;
}

static const CcLocalAgent *SelectedCombatTarget(const LocalState *local)
{
    int32_t target = SelectedCombatTargetIndex(local);
    return target >= 0 ? &local->course.raiders[target] : NULL;
}

static int32_t NextCombatTargetIndex(const LocalState *local)
{
    if (local == NULL) return -1;
    int32_t current = SelectedCombatTargetIndex(local);
    for (int32_t step = 1; step <= CC_LOCAL_RAIDER_COUNT; ++step) {
        int32_t candidate = (current + step) % CC_LOCAL_RAIDER_COUNT;
        if (CcLocalCourseCanPlayerEngage(
                &local->course, &local->agent, candidate)) {
            return candidate;
        }
    }
    return -1;
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
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (local->course.raiders[i].combat.life_state != CC_LIFE_ALIVE) {
            continue;
        }
        int32_t previous_count = set != NULL ? set->count : 0;
        AddDetailedContextAction(
            set, CONTEXT_ACTION_SELECT_TARGET,
            local->course.raider_names[i], i == target ? "TARGET" : "",
            CcLocalRaiderRoleName(local->course.raider_roles[i]),
            true, i == target);
        if (set != NULL && set->count > previous_count) {
            set->items[set->count - 1].amount = i;
        }
    }

    if (has_target) {
        AddDetailedContextAction(
            set, CONTEXT_ACTION_BASIC_STRIKE, "Attack", "SPACE",
            local->course.raider_names[target], true, false);
        AddDetailedContextAction(
            set, CONTEXT_ACTION_TOGGLE_GUARD, "Guard", "X",
            local->agent.humanoid.guard_requested ?
                "GUARD UP" : "GUARD DOWN",
            true, local->agent.humanoid.guard_requested);
        AddDetailedContextAction(
            set, CONTEXT_ACTION_SKILL_CRUSHING, "Crushing blow", "1",
            CombatSkillDetail(&local->agent,
                              CC_COMBAT_SKILL_CRUSHING_BLOW, true,
                              has_target),
            CombatSkillEnabled(&local->agent,
                               CC_COMBAT_SKILL_CRUSHING_BLOW, true,
                               has_target),
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
                              true, has_target),
            CombatSkillEnabled(&local->agent, CC_COMBAT_SKILL_SECOND_WIND,
                               true, has_target), false);
    }
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
    if (view == VIEW_DUNGEON) {
        const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
        const CcDungeon *dungeon = CcSimDungeon(sim, expedition->dungeon_id);
        const CcDungeonRoom *room = CcSimDungeonCurrentRoom(sim);
        if (!expedition->active || dungeon == NULL || room == NULL) return set;
        if (expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE) {
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_DUNGEON_PARLEY, "Parley",
                TextFormat("%d", set.count + 1), "OFFER FOOD / TEST REACTION",
                true, false);
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_DUNGEON_EVADE, "Evade",
                TextFormat("%d", set.count + 1), "LIGHT + LOW LOAD HELP",
                true, false);
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_DUNGEON_FORCE, "Force passage",
                TextFormat("%d", set.count + 1), "WEAPONS HELP / LOUD",
                true, false);
        } else {
            int32_t exits = CcSimDungeonVisibleExitCount(sim);
            for (int32_t i = 0; i < exits; ++i) {
                int32_t target = CcSimDungeonVisibleExitAt(sim, i);
                if (target < 0 || target >= dungeon->room_count) continue;
                const CcDungeonRoom *next = &dungeon->rooms[target];
                bool known = (next->state_flags &
                              CC_DUNGEON_ROOM_DISCOVERED) != 0U;
                int32_t action_index = set.count;
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_DUNGEON_MOVE,
                    known ? next->name : "Unmapped passage",
                    TextFormat("%d", set.count + 1),
                    known ? TextFormat("DEPTH %d / %s", next->depth,
                                       CcDungeonRoomKindName(next->kind)) :
                            "ONE DUNGEON TURN",
                    expedition->strain < 100, false);
                if (set.count > action_index) {
                    set.items[action_index].amount = target;
                }
            }
            if ((room->state_flags & CC_DUNGEON_ROOM_SEARCHED) == 0U) {
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_DUNGEON_SEARCH, "Search chamber",
                    TextFormat("%d", set.count + 1),
                    "NOISY / MAY REVEAL PASSAGES", true, false);
            }
            int32_t shortcut = CcSimDungeonOpenableShortcut(sim);
            if (shortcut >= 0) {
                int32_t action_index = set.count;
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_DUNGEON_OPEN_SHORTCUT,
                    "Open freight shortcut",
                    TextFormat("%d", set.count + 1),
                    sim->player.cargo[CC_GOOD_TOOLS] > 0 ?
                        "1 TOOLS / TWO TURNS / LOUD" : "NEEDS 1 TOOLS",
                    sim->player.cargo[CC_GOOD_TOOLS] > 0, false);
                if (set.count > action_index) {
                    set.items[action_index].amount = shortcut;
                }
            }
        }
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_DUNGEON_RETREAT,
            room->depth == 0 ? "Return to carriage" : "Flee to carriage",
            "ESC", room->depth == 0 ? "SAFE WITHDRAWAL" :
                                       "RISK CARGO + STRAIN",
            true, false);
        return set;
    }
    if (view == VIEW_DRAGON_CAVE) {
        const CcTreasure *treasure = FirstDragonTreasure(sim);
        if (sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_TO_DRAGON &&
            !sim->dragon.slain) {
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_INTERCEPT_DRAGON_TRIBUTE,
                "Intercept tribute", TextFormat("%d", set.count + 1),
                "TAKE IT BEFORE DELIVERY", true, false);
        }
        if (!sim->dragon.slain && sim->dragon.stolen_outstanding == 0) {
            if (sim->dragon.hoard > 0) {
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_STEAL_DRAGON_CROWNS,
                    TextFormat("Take %d crowns",
                               sim->dragon.hoard < 10 ?
                                   (int32_t)sim->dragon.hoard : 10),
                    TextFormat("%d", set.count + 1),
                    "14 NIGHTS TO RETURN", true, false);
            }
            if (treasure != NULL) {
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_STEAL_DRAGON_RELIC,
                    TextFormat("Take %.28s", treasure->name),
                    TextFormat("%d", set.count + 1),
                    "THE DRAGON REMEMBERS", true, false);
            }
        } else if (!sim->dragon.slain &&
                   sim->dragon.stolen_treasure_id != 0U) {
            const CcTreasure *stolen = CcSimTreasure(
                sim, sim->dragon.stolen_treasure_id);
            bool carries_relic = stolen != NULL &&
                stolen->owner_id == sim->player.id;
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_RETURN_DRAGON_RELIC,
                stolen != NULL ? TextFormat("Return %.28s", stolen->name) :
                                  "Return remembered relic",
                TextFormat("%d", set.count + 1),
                carries_relic ? "SETTLE THE WOUND" : "NOT IN CARRIAGE",
                carries_relic, false);
        } else if (!sim->dragon.slain &&
                   sim->dragon.stolen_outstanding > 0) {
            CcMoney available =
                sim->player.coins < sim->dragon.stolen_outstanding ?
                    sim->player.coins : sim->dragon.stolen_outstanding;
            int32_t payment = available > INT32_MAX ?
                INT32_MAX : (int32_t)available;
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_RETURN_DRAGON_CROWNS,
                TextFormat("Return %d crowns", payment),
                TextFormat("%d", set.count + 1),
                payment > 0 ? "PAY DOWN THE DEBT" : "NO CROWNS CARRIED",
                payment > 0, false);
        }
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Return to goblin chimney", "ESC",
            sim->dragon.slain ? "LEAVE THE ASHEN ROOST" :
                                "THE ONLY EXIT", true, false);
        return set;
    }
    if (view == VIEW_LEDGER) {
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Close ledger", "ESC",
            "RETURN TO PREVIOUS VIEW", true, false);
        return set;
    }
    if (view == VIEW_SITUATIONS) {
        const CcSituation *detail = SelectedActiveSituation(
            sim, selected_situation);
        if (detail != NULL &&
            detail->id == sim->player.accepted_situation_id) {
            AddContextAction(&set, CONTEXT_ACTION_ABANDON_PROMISE,
                             "Abandon quest");
        } else if (detail != NULL && CcSimAcceptedSituation(sim) == NULL &&
                   CcSimSituationCanAccept(sim, detail)) {
            bool at_notice = CcClientPromiseCanBeAccepted(
                local->market_interior,
                GridDistance(LocalPosition(local), LOCAL_NOTICE));
            if (local->open_world) {
                at_notice = !local->market_interior &&
                            OpenWorldSettlementDistance(sim, local) < 18.0f;
            }
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_ACCEPT_PROMISE, "Accept quest", "ENTER",
                at_notice ? "MAKE THE PROMISE" : "VISIT THE LOCAL BOARD",
                at_notice, false);
        }
        if (ActiveSituationCount(sim) > 1) {
            AddContextAction(&set, CONTEXT_ACTION_NEXT_PROMISE,
                             "Next quest");
        }
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Close quests", "ESC",
            "RETURN TO PREVIOUS VIEW", true, false);
        return set;
    }
    if (view == VIEW_CARRIAGE) {
        bool away_from_town = local->site_kind != CC_LOCAL_SITE_NONE;
        AddDetailedContextAction(
            &set,
            away_from_town ? CONTEXT_ACTION_RETURN_FROM_SITE :
                             CONTEXT_ACTION_CHOOSE_ROAD,
            away_from_town ? "Drive back to town" : "Take the reins",
            "ENTER",
            away_from_town ? "RETURN ALONG THE SITE ROAD" :
                             "OPEN THE DEPARTURE ROAD",
            true, false);
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_OPEN_MAP, "Open map case", "M",
            "CHARTS AND ROAD NOTES", true, false);
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_OPEN_PROMISES, "Review quest papers", "Q",
            "ACTIVE LOAD AND COMMITMENTS", true, false);
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Step away", "ESC",
            "RETURN TO THE STREET", true, false);
        return set;
    }
    if (view == VIEW_CHARACTER) {
        const CcSituation *situation = CcSimSituation(
            sim, local->conversation_situation_id);
        const CcCharacter *character = CcSimCharacter(
            sim, local->conversation_character_id);
        if (situation != NULL && character != NULL) {
            bool listened = CcCharacterRemembers(
                character, CC_CHARACTER_MEMORY_MET_PLAYER, situation->id);
            bool promised = CcCharacterRemembers(
                character, CC_CHARACTER_MEMORY_PLAYER_PROMISED,
                situation->id);
            bool decision = situation->kind ==
                    CC_SITUATION_MONSTER_EXPEDITION &&
                situation->discovery_stage == CC_DISCOVERY_DECISION;
            if (decision) {
                AddContextAction(
                    &set, CONTEXT_ACTION_REPORT_EVIDENCE,
                    CcStoryPlayerChoiceText(
                        situation->kind, CC_STORY_PLAYER_REPORT));
                AddContextAction(
                    &set, CONTEXT_ACTION_KEEP_CONFIDENCE,
                    CcStoryPlayerChoiceText(
                        situation->kind,
                        CC_STORY_PLAYER_KEEP_CONFIDENCE));
            } else if (!listened && !promised) {
                AddContextAction(&set, CONTEXT_ACTION_LISTEN_CHARACTER,
                                 CcStoryPlayerChoiceText(
                                     situation->kind,
                                     CC_STORY_PLAYER_ASK));
            }
            const CcSituation *accepted = CcSimAcceptedSituation(sim);
            bool can_promise = situation->status == CC_SITUATION_ACTIVE &&
                (accepted == NULL || accepted->id == situation->id) &&
                CcSimSituationCanAccept(sim, situation) && listened &&
                !promised;
            if (can_promise) {
                AddContextAction(&set, CONTEXT_ACTION_PLEDGE_CHARACTER,
                                 CcStoryPlayerChoiceText(
                                     situation->kind,
                                     CC_STORY_PLAYER_PROMISE));
            }
        }
        AddContextAction(&set, CONTEXT_ACTION_CLOSE_VIEW,
                         CcStoryPlayerChoiceText(
                             situation != NULL ? situation->kind :
                                 CC_SITUATION_RELIEF_DELIVERY,
                             CC_STORY_PLAYER_LEAVE));
        return set;
    }
    if (view == VIEW_MAP) {
        const CcMap *map = SelectedVisibleMap(sim, selected);
        if (map != NULL && map->owner_id == sim->player.location_id) {
            AddContextAction(&set, CONTEXT_ACTION_BUY_MAP,
                             TextFormat("Buy map — %d crowns",
                                        map->ask_price));
        }
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Close map case", "ESC",
            "RETURN TO THE CARRIAGE", true, false);
        return set;
    }
    if (view == VIEW_ROADS) {
        if (RoadBookDepartureInProgress(local)) return set;
        const CcRoute *route = SelectedOutgoingRoute(sim, selected);
        const CcMap *map = route != NULL ?
            VisibleMapForRoute(sim, route->id) : NULL;
        if (route != NULL) {
            AddContextAction(&set, CONTEXT_ACTION_TRAVEL,
                             route->smuggler_route ?
                                 "Turn onto hidden track" :
                                 "Turn onto branch");
        }
        if (OutgoingRouteCount(sim) > 1) {
            AddContextAction(&set, CONTEXT_ACTION_NEXT_BRANCH,
                             "Face next branch");
        }
        if (map != NULL && map->owner_id == sim->player.location_id) {
            AddContextAction(&set, CONTEXT_ACTION_BUY_MAP,
                             TextFormat("Buy notes — %d crowns", map->ask_price));
        } else if (route != NULL &&
                   (route->closed || route->condition < 75)) {
            AddContextAction(&set, CONTEXT_ACTION_REPAIR_ROUTE,
                             "Repair road");
        }
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Turn back to town", "ESC",
            "LEAVE THE JUNCTION", true, false);
        return set;
    }

    if (local->road_choice_active) return set;

    if (local->site_travel_active) return set;

    if (local->journey_travel_active) {
        bool safe_journey = sim->journey.active && sim->journey.danger <= 30;
        bool parking = !sim->journey.active &&
            (RoadBookArrivalInProgress(local) ||
             local->convoy.phase == CC_LOCAL_CONVOY_ARRIVING);
        if (sim->journey.active || parking) {
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_SKIP_TRAVEL,
                parking ? "Park carriage" :
                safe_journey ? "Finish safe trip" : "Advance to next event",
                "ENTER",
                parking ? "END ARRIVAL" :
                safe_journey ? "COMPLETE ROUTINE ROAD" :
                               "STOP AT THE NEXT DECISION",
                true, false);
        }
        return set;
    }
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
    if (LocalCombatActive(local)) {
        AddCombatActions(&set, local, false);
        return set;
    }

    Vector2 position = LocalPosition(local);
    if (local->site_kind != CC_LOCAL_SITE_NONE) {
        Vector2 entrance = {CC_LOCAL_SITE_ENTRANCE_X,
                            CC_LOCAL_SITE_ENTRANCE_Z};
        Vector2 tunnel = {CC_LOCAL_SITE_CARRIAGE_X,
                          CC_LOCAL_SITE_CARRIAGE_Z};
        if (local->site_kind == CC_LOCAL_SITE_DRAGON_CAVE &&
            GridDistance(position, tunnel) < 2.25f) {
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_TRAVEL_GOBLIN_SITE,
                "Descend through goblin dungeon", "F",
                "RETURN THROUGH THE CAVE NETWORK", true, false);
        }
        if (GridDistance(position, entrance) < 2.25f) {
            if (local->site_kind == CC_LOCAL_SITE_DUNGEON) {
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_EXPEDITION,
                    "Enter the Underroad", "E", "BRING FOOD / LIGHT 18",
                    sim->player.cargo[CC_GOOD_FOOD] > 0, false);
                const CcDungeon *dungeon = DungeonAtSettlement(
                    sim, sim->player.location_id);
                if (dungeon != NULL &&
                    CcSimDungeonOutcomeAvailable(
                        dungeon, CC_DUNGEON_PUBLIC_ROUTE) &&
                    dungeon->state != CC_DUNGEON_PUBLIC_ROUTE) {
                    AddDetailedContextAction(
                        &set, CONTEXT_ACTION_DUNGEON_PUBLIC_ROUTE,
                        "Publish freight road", "",
                        "2 TOOLS / 12 CROWNS",
                        sim->player.cargo[CC_GOOD_TOOLS] >= 2 &&
                            sim->player.coins >= 12,
                        false);
                }
                if (dungeon != NULL &&
                    CcSimDungeonOutcomeAvailable(
                        dungeon, CC_DUNGEON_SMUGGLER_ROUTE) &&
                    dungeon->state != CC_DUNGEON_SMUGGLER_ROUTE) {
                    AddDetailedContextAction(
                        &set, CONTEXT_ACTION_DUNGEON_SMUGGLER_ROUTE,
                        "Sell the Night Road", "",
                        "1 TOOLS / 6 CROWNS",
                        sim->player.cargo[CC_GOOD_TOOLS] >= 1 &&
                            sim->player.coins >= 6,
                        false);
                }
                if (dungeon != NULL &&
                    CcSimDungeonOutcomeAvailable(
                        dungeon, CC_DUNGEON_RESEALED) &&
                    dungeon->state != CC_DUNGEON_RESEALED) {
                    AddDetailedContextAction(
                        &set, CONTEXT_ACTION_DUNGEON_RESEAL,
                        "Reseal the Underroad", "", "3 TOOLS",
                        sim->player.cargo[CC_GOOD_TOOLS] >= 3, false);
                }
            } else if (local->site_kind == CC_LOCAL_SITE_GOBLIN_CAVE) {
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_TRAVEL_DRAGON_SITE,
                    "Climb through goblin dungeon", "F",
                    "ONE DAY / DARK / DANGEROUS", true, false);
                static const CcGood trade_goods[] = {
                    CC_GOOD_FOOD, CC_GOOD_TOOLS, CC_GOOD_WEAPONS
                };
                for (int32_t i = 0;
                     i < (int32_t)(sizeof(trade_goods) /
                                   sizeof(trade_goods[0])); ++i) {
                    CcGood good = trade_goods[i];
                    if (sim->player.cargo[good] <= 0) continue;
                    int32_t action_index = set.count;
                    AddContextAction(
                        &set, CONTEXT_ACTION_GOBLIN_TRADE,
                        TextFormat("Trade 1 %s", CcGoodName(good)));
                    if (set.count > action_index) {
                        set.items[action_index].good = good;
                        set.items[action_index].amount = 1;
                    }
                }
            }
        }
        return set;
    }
    if (local->market_interior || local->open_world_market) {
        if (local->open_world_market ||
            GridDistance(position, INTERIOR_COUNTER) < 2.25f) {
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
                    good == CC_GOOD_FOOD ?
                        TextFormat("Deliver %d food boxes", remaining) :
                        TextFormat("Deliver %d %s", remaining,
                                   CcGoodName(good)));
            }
            const CcSettlement *place = CcSimSettlement(
                sim, sim->player.location_id);
            if (place != NULL) {
                for (int32_t cargo_good = 0;
                     cargo_good < CC_GOOD_COUNT; ++cargo_good) {
                    if (place->stock[cargo_good] <= 0 &&
                        sim->player.cargo[cargo_good] <= 0) continue;
                    AddCargoContextAction(
                        &set, (CcGood)cargo_good,
                        (CcGood)cargo_good == CC_GOOD_FOOD ?
                            TextFormat("Food boxes %dc · %d local · %d aboard",
                                       place->price[cargo_good],
                                       place->stock[cargo_good],
                                       sim->player.cargo[cargo_good]) :
                            TextFormat("%s %dc · %d held",
                                       CcGoodName((CcGood)cargo_good),
                                       place->price[cargo_good],
                                       sim->player.cargo[cargo_good]));
                }
            }
        }
        if (local->open_world_market) {
            AddContextAction(&set, CONTEXT_ACTION_LEAVE_MARKET,
                             "Step back into town");
        }
        return set;
    }

    if (local->open_world) {
        float town_distance = OpenWorldSettlementDistance(sim, local);
        if (town_distance < 18.0f) {
            AddContextAction(&set, CONTEXT_ACTION_OPEN_PROMISES,
                             "Read town board");
            AddContextAction(&set, CONTEXT_ACTION_ENTER_MARKET,
                             "Enter market hall");
            if (OutgoingRouteCount(sim) > 0) {
                AddContextAction(&set, CONTEXT_ACTION_CHOOSE_ROAD,
                                 "Choose a road");
            }
        }
        return set;
    }

    if (local->course.situation_witness_active &&
        GridDistance(position,
                     (Vector2){local->course.situation_witness.position.x,
                               local->course.situation_witness.position.z}) <
            1.85f) {
        const CcCharacter *character = CcSimCharacter(
            sim, local->course.situation_witness_character_id);
        AddContextAction(
            &set, CONTEXT_ACTION_TALK_CHARACTER,
            character != NULL ? TextFormat("Talk to %.28s",
                                            character->name) :
                                "Talk to witness");
    }
    if (GridDistance(position, LOCAL_NOTICE) < 1.15f) {
        AddContextAction(&set, CONTEXT_ACTION_OPEN_PROMISES,
                         "View quests");
    }
    const CcDungeon *dungeon = DungeonAtSettlement(
        sim, sim->player.location_id);
    if (GridDistance(position, LOCAL_DRAGON_CAVE) < 1.35f) {
        if (sim->player.location_id ==
                   sim->goblins.lair_settlement_id) {
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_TRAVEL_GOBLIN_SITE,
                "Travel to hidden goblin trailhead", "F",
                "CARRIAGE STOPS AT ROAD EDGE", true, false);
        }
    }
    if (dungeon != NULL &&
        GridDistance(position, LOCAL_DUNGEON) < 1.35f) {
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_TRAVEL_DUNGEON_SITE,
            "Drive to the mine", "F", "SEPARATE LOCAL MAP", true, false);
    }
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
                       (float)GetScreenHeight() - 78.0f, width, 58.0f};
}

static Color ContextActionColor(ContextActionKind kind)
{
    if (kind == CONTEXT_ACTION_FIGHT ||
        kind == CONTEXT_ACTION_ABANDON_PROMISE ||
        kind == CONTEXT_ACTION_RAISE_ALARM ||
        kind == CONTEXT_ACTION_WITHDRAW ||
        kind == CONTEXT_ACTION_STEAL_DRAGON_CROWNS ||
        kind == CONTEXT_ACTION_STEAL_DRAGON_RELIC ||
        kind == CONTEXT_ACTION_DUNGEON_FORCE ||
        kind == CONTEXT_ACTION_DUNGEON_RETREAT ||
        kind == CONTEXT_ACTION_DUNGEON_RESEAL) return DANGER;
    if (kind == CONTEXT_ACTION_ACCEPT_PROMISE ||
        kind == CONTEXT_ACTION_TRAVEL ||
        kind == CONTEXT_ACTION_SKIP_TRAVEL ||
        kind == CONTEXT_ACTION_DELIVER_CARGO ||
        kind == CONTEXT_ACTION_PLEDGE_CHARACTER ||
        kind == CONTEXT_ACTION_TALK_CHARACTER ||
        kind == CONTEXT_ACTION_ENTER_MARKET ||
        kind == CONTEXT_ACTION_JUMP ||
        kind == CONTEXT_ACTION_OFFER_PROVISIONS ||
        kind == CONTEXT_ACTION_RETURN_DRAGON_CROWNS ||
        kind == CONTEXT_ACTION_RETURN_DRAGON_RELIC ||
        kind == CONTEXT_ACTION_INTERCEPT_DRAGON_TRIBUTE ||
        kind == CONTEXT_ACTION_OPEN_MAP ||
        kind == CONTEXT_ACTION_TRAVEL_DRAGON_SITE ||
        kind == CONTEXT_ACTION_DUNGEON_MOVE ||
        kind == CONTEXT_ACTION_DUNGEON_PARLEY ||
        kind == CONTEXT_ACTION_DUNGEON_OPEN_SHORTCUT ||
        kind == CONTEXT_ACTION_DUNGEON_PUBLIC_ROUTE) return TEAL;
    if (kind == CONTEXT_ACTION_SELECT_TARGET) return DANGER;
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
    bool cargo_controls = false;
    for (int32_t i = 0; i < actions.count; ++i) {
        if (actions.items[i].kind == CONTEXT_ACTION_BUY_CARGO) {
            cargo_controls = true;
        }
    }
    if (cargo_controls) {
        int width = CcOverlayMeasureText("LEFT BUY · RIGHT SELL", 9);
        CcOverlayDrawText("LEFT BUY · RIGHT SELL",
                          (GetScreenWidth() - width) / 2,
                          GetScreenHeight() - 94, 9, MUTED);
    }
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
        Color label_color = !action->enabled ? Fade(MUTED, 0.62f) :
                            hover || action->active ? accent : INK;
        if (action->kind == CONTEXT_ACTION_SELECT_TARGET &&
            action->amount >= 0 && action->amount < CC_LOCAL_RAIDER_COUNT) {
            const CcLocalAgent *outlaw =
                &local->course.raiders[action->amount];
            Rectangle portrait = {bounds.x + 7.0f, bounds.y + 5.0f,
                                  40.0f, bounds.height - 10.0f};
            CcLocalDrawAgentPortrait3D(outlaw, portrait);
            float text_x = portrait.x + portrait.width + 8.0f;
            int text_width = (int)(bounds.x + bounds.width - text_x - 7.0f);
            const char *name = TextFormat("%.14s", action->label);
            CcOverlayDrawText(name, (int)text_x, (int)bounds.y + 8, 8,
                              label_color);
            CcOverlayDrawText(action->detail, (int)text_x,
                              (int)bounds.y + 22, 6, MUTED);
            int32_t health = (int32_t)lroundf(outlaw->combat.health);
            CcOverlayDrawText(TextFormat("HP %d", health), (int)text_x,
                              (int)bounds.y + 36, 6,
                              health < 35 ? DANGER : INK);
            int bar_width = text_width > 5 ? text_width : 5;
            DrawRectangle((int)text_x, (int)bounds.y + 47,
                          bar_width, 3, BAR_TRACK);
            DrawRectangle((int)text_x, (int)bounds.y + 47,
                          (int)lroundf((float)bar_width *
                                      ClampUnit(outlaw->combat.health /
                                                CC_LOCAL_COMBAT_MAX_HEALTH)),
                          3, DANGER);
            continue;
        }
        bool detailed = action->detail[0] != '\0' ||
                        action->key_hint[0] != '\0';
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

static ContextAction PressedContextAction(
    const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation)
{
    ContextAction none = {.kind = CONTEXT_ACTION_NONE};
    bool left = ClientMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool right = ClientMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (!left && !right) {
        return none;
    }
    ContextActionSet actions = BuildContextActions(
        sim, local, view, selected, selected_situation);
    Vector2 mouse = GetMousePosition();
    for (int32_t i = 0; i < actions.count; ++i) {
        if (actions.items[i].enabled && CheckCollisionPointRec(
                mouse, ContextActionBounds(i, actions.count))) {
            ContextAction pressed = actions.items[i];
            if (right && pressed.kind != CONTEXT_ACTION_BUY_CARGO) {
                return none;
            }
            if (right) pressed.amount = -1;
            return pressed;
        }
    }
    return none;
}

static bool PointerOverContextAction(
    const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation, Vector2 mouse)
{
    ContextActionSet actions = BuildContextActions(
        sim, local, view, selected, selected_situation);
    for (int32_t index = 0; index < actions.count; ++index) {
        if (CheckCollisionPointRec(
                mouse, ContextActionBounds(index, actions.count))) {
            return true;
        }
    }
    return false;
}

static void UpdateMovementPreview(
    const CcSim *sim, LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation,
    RenderTexture2D local_target, Rectangle local_bounds, float delta_time)
{
    if (local == NULL) return;
    Vector2 mouse = GetMousePosition();
    bool unavailable = view != VIEW_LOCAL || local->site_travel_active ||
        local->road_choice_active || local->journey_travel_active ||
        local->journey_parley_active ||
        !CheckCollisionPointRec(mouse, local_bounds) ||
        PointerOverContextAction(
            sim, local, view, selected, selected_situation, mouse);
    if (unavailable) {
        local->movement_preview = (CcLocalMovementPreview){0};
        local->movement_preview_cooldown = 0.0f;
        return;
    }

    local->movement_preview_cooldown = fmaxf(
        0.0f, local->movement_preview_cooldown - delta_time);
    if (local->movement_preview_cooldown > 0.0f) return;
    local->movement_preview_cooldown = 0.08f;

    if (!LocalCombatActive(local)) {
        CcLocalWorldTargetKind world_target = CcLocalAgentPickWorldTarget(
            &local->agent, mouse, local_target, local_bounds,
            local->market_interior);
        if (world_target != CC_LOCAL_WORLD_TARGET_NONE) {
            local->movement_preview = (CcLocalMovementPreview){
                .screen_point = mouse,
                .origin = local->agent.position,
                .scene = local->agent.scene,
                .world_target = world_target,
                .valid = true,
                .accepted = true,
            };
            return;
        }
    }
    (void)CcLocalAgentProbeTarget(
        &local->agent, mouse, local_target, local_bounds,
        local->market_interior, &local->movement_preview);
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
        status = "Choose an outlaw below or press T to cycle targets.";
    } else if (local->agent.humanoid.guard_requested) {
        status = "Guard is up. Watch posture and strike after the block.";
    } else {
        status = "Attack with Space, guard with X, and use skills with 1–3.";
    }
    char shown[96];
    (void)snprintf(shown, sizeof(shown), "%s  /  %.72s",
                   target != NULL ? "COMBAT" : "THREAT", status);
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
    const float width = 104.0f;
    const float gap = 8.0f;
    int32_t index = (int32_t)action - 1;
    float total = (float)(COMMAND_ACTION_COUNT - 1) * width +
                  (float)(COMMAND_ACTION_COUNT - 2) * gap;
    return (Rectangle){((float)GetScreenWidth() - total) * 0.5f +
                           (float)index * (width + gap),
                       11.0f, width, 30.0f};
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
                                 const LocalState *local,
                                 ClientView view)
{
    if (local != NULL &&
        local->opening_step != CC_LOCAL_OPENING_COMPLETE) {
        return action == COMMAND_ACTION_SAVE;
    }
    bool road_local = local != NULL &&
        (local->road_choice_active || local->journey_travel_active ||
         local->site_travel_active ||
         local->journey_combat_active || local->journey_parley_active);
    bool choosing_road = local != NULL && local->road_choice_active;
    if (action == COMMAND_ACTION_QUESTS) {
        return !road_local && !choosing_road;
    }
    if (action == COMMAND_ACTION_SAVE) return !choosing_road;
    if (action == COMMAND_ACTION_MAP) {
        float carriage_distance = 1000.0f;
        if (local != NULL) {
            Vector2 carriage = local->site_kind == CC_LOCAL_SITE_NONE ?
                LOCAL_CARRIAGE_BAY :
                (Vector2){CC_LOCAL_SITE_CARRIAGE_X,
                          CC_LOCAL_SITE_CARRIAGE_Z};
            carriage_distance = GridDistance(LocalPosition(local), carriage);
        }
        return local != NULL && CcClientMapCommandEnabled(
            view == VIEW_MAP, road_local, local->market_interior,
            carriage_distance);
    }
    return true;
}

static void DrawCommandBar(ClientView view, const LocalState *local)
{
    if (local != NULL &&
        local->opening_step != CC_LOCAL_OPENING_COMPLETE) return;
    Vector2 mouse = GetMousePosition();
    for (int32_t value = COMMAND_ACTION_QUESTS;
         value < COMMAND_ACTION_COUNT; ++value) {
        CommandActionKind action = (CommandActionKind)value;
        Rectangle bounds = CommandActionBounds(action);
        bool enabled = CommandActionEnabled(action, local, view);
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

static CommandActionKind PressedCommandAction(const LocalState *local,
                                              ClientView view)
{
    if (local != NULL &&
        local->opening_step != CC_LOCAL_OPENING_COMPLETE) {
        return COMMAND_ACTION_NONE;
    }
    if (!ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return COMMAND_ACTION_NONE;
    }
    Vector2 mouse = GetMousePosition();
    for (int32_t value = COMMAND_ACTION_QUESTS;
         value < COMMAND_ACTION_COUNT; ++value) {
        CommandActionKind action = (CommandActionKind)value;
        if (CommandActionEnabled(action, local, view) &&
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
    int32_t ordinal = OutgoingRouteOrdinal(sim, selected);
    CcOverlayDrawText(
        ordinal >= 0 ?
            TextFormat("VISIBLE BRANCH %d OF %d", ordinal + 1,
                       OutgoingRouteCount(sim)) :
            "VISIBLE BRANCH",
        998, 102, 9, TEAL);
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
    CcOverlayDrawText("STEADY PLAN  /  W-S CHANGE PACE ON ROAD",
                      998, 208, 8, TEAL);
    DrawBar(998, 358, 92, "TEAM", preview.horse_readiness, TEAL);
    CcOverlayDrawText(TextFormat("%d FODDER",
                                 preview.horse_feed_required),
                      1105, 362, 9, INK);
    CcOverlayDrawText(TextFormat("%.10s + %.10s",
                                 sim->horse_team[0].name,
                                 sim->horse_team[1].name),
                      998, 383, 8, MUTED);

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

static void DrawDragonHoardMapArt(const CcSim *sim, const CcMap *map)
{
    Color ink = (Color){67, 45, 39, 255};
    Color faint = Fade(ink, 0.34f);
    Color gold = (Color){155, 105, 38, 255};
    Rectangle chart = {306.0f, 163.0f, 572.0f, 398.0f};
    DrawRectangleRounded(chart, 0.035f, 5,
                         (Color){207, 174, 139, 255});
    DrawRectangleRoundedLinesEx(chart, 0.035f, 5, 2.0f, faint);

    int32_t title_width = CcOverlayMeasureText(map->name, 20);
    CcOverlayDrawText(map->name, 592 - title_width / 2, 124, 20, ink);
    CcOverlayDrawText(
        TextFormat("Smuggled copy  /  %d%% trusted  /  surveyed day %d",
                   map->accuracy, map->surveyed_day),
        330, 178, 10, Fade(ink, 0.76f));

    const Vector2 tunnel[] = {
        {340.0f, 264.0f}, {408.0f, 250.0f}, {457.0f, 292.0f},
        {512.0f, 276.0f}, {555.0f, 336.0f}, {622.0f, 318.0f},
        {663.0f, 382.0f}, {731.0f, 370.0f}
    };
    for (int32_t i = 0; i + 1 < (int32_t)(sizeof(tunnel) /
                                           sizeof(tunnel[0])); ++i) {
        DrawLineEx(tunnel[i], tunnel[i + 1], 13.0f, Fade(ink, 0.17f));
        DrawLineEx(tunnel[i], tunnel[i + 1], 2.4f, ink);
    }
    DrawCircleLines(340, 264, 27.0f, ink);
    DrawCircleLines(555, 336, 48.0f, ink);
    DrawCircleLines(751, 383, 76.0f, ink);
    DrawCircleLines(751, 383, 61.0f, faint);
    CcOverlayDrawText("LOW GATE", 314, 301, 10, ink);
    CcOverlayDrawText("FALSE VAULT", 514, 398, 10, ink);
    CcOverlayDrawText("CROWN CHAMBER", 699, 476, 10, ink);

    for (int32_t coin = 0; coin < 14; ++coin) {
        int32_t column = coin % 5;
        int32_t row = coin / 5;
        DrawCircle(715 + column * 15 + row * 3,
                   416 + row * 11, 6.0f, gold);
    }
    DrawPoly((Vector2){785.0f, 431.0f}, 4, 10.0f, 45.0f,
             (Color){116, 54, 59, 255});
    DrawPoly((Vector2){810.0f, 421.0f}, 4, 8.0f, 45.0f,
             (Color){76, 91, 102, 255});
    CcOverlayDrawText("NAMED RELICS", 730, 452, 9, ink);

    Vector2 dragon_body = {738.0f, 350.0f};
    DrawEllipse((int)dragon_body.x, (int)dragon_body.y, 30.0f, 15.0f,
                Fade(ink, 0.82f));
    DrawTriangle((Vector2){720.0f, 347.0f},
                 (Vector2){681.0f, 319.0f},
                 (Vector2){710.0f, 361.0f}, Fade(ink, 0.72f));
    DrawTriangle((Vector2){748.0f, 344.0f},
                 (Vector2){780.0f, 311.0f},
                 (Vector2){756.0f, 359.0f}, Fade(ink, 0.72f));
    DrawLineEx((Vector2){762.0f, 352.0f}, (Vector2){799.0f, 369.0f},
               7.0f, Fade(ink, 0.82f));
    DrawCircle(708, 348, 9.0f, ink);

    CcOverlayDrawText("DO NOT WAKE IT", 622, 211, 13,
                      (Color){126, 47, 43, 255});
    CcOverlayDrawText(
        TextFormat("LATEST REPORT: %s  /  HOARD %" PRId64 " CROWNS",
                   CcDragonLifeStageName(sim->dragon.life_stage),
                   sim->dragon.hoard),
        330, 520, 10, ink);
    CcOverlayDrawText("Three turns after the false vault. Keep left at ash.",
                      330, 539, 9, Fade(ink, 0.76f));

    Vector2 compass = {841.0f, 213.0f};
    DrawCircleLinesV(compass, 19.0f, faint);
    DrawLineEx(compass, (Vector2){841.0f, 191.0f}, 2.0f, ink);
    CcOverlayDrawText("N", 836, 235, 9, ink);
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
    if (sim == NULL || map == NULL) {
        return false;
    }
    static const int32_t art_cell[CC_MAP_COLLECTION_COUNT] = {
        2, 0, 4, 5, 8, 9, 1, 3, 6, 7, 10, 11, 0
    };
    int32_t slot = (int32_t)(map - sim->maps);
    if (slot < 0 || slot >= CC_MAP_COLLECTION_COUNT) return false;
    if (slot == CC_MAP_DRAGON_HOARD) {
        DrawDragonHoardMapArt(sim, map);
        return true;
    }
    if (collectible_atlas.id == 0U) return false;
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
    float panel_width = fminf(820.0f, (float)GetScreenWidth() - 80.0f);
    float panel_height = fminf(500.0f, (float)GetScreenHeight() - 150.0f);
    if (panel_width < 520.0f) panel_width = 520.0f;
    if (panel_height < 360.0f) panel_height = 360.0f;
    Rectangle bounds = {
        ((float)GetScreenWidth() - panel_width) * 0.5f,
        ((float)GetScreenHeight() - panel_height) * 0.5f - 8.0f,
        panel_width, panel_height
    };
    DrawPanel(bounds, PANEL_DEEP);
    int x = (int)bounds.x + 24;
    int title_y = (int)bounds.y + 25;
    CcOverlayDrawText("LEDGER", x, title_y, 22, INK);
    CcOverlayDrawText("MOST RECENT FIRST", x + 126, title_y + 7,
                      9, MUTED);
    int32_t shown = sim->event_count < 4 ? sim->event_count : 4;
    float row_height = (bounds.height - 82.0f) / 4.0f;
    size_t line_capacity = (size_t)fmaxf(
        42.0f, fminf(88.0f, (bounds.width - 190.0f) / 6.2f));
    for (int32_t i = 0; i < shown; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) continue;
        int y = (int)(bounds.y + 66.0f + (float)i * row_height);
        DrawRectangle((int)bounds.x + 14, y - 4,
                      (int)bounds.width - 28, (int)row_height - 5,
                      i == 0 ? Fade(PANEL_HOVER, 0.52f) :
                               Fade(PANEL, 0.30f));
        CcOverlayDrawText(TextFormat("DAY %d", event->day), x, y + 7, 9,
                          CC_GOLD);
        Color kind_color = event->kind == CC_EVENT_MONSTER_PRESSURE ||
                           event->kind == CC_EVENT_DRAGON_OMEN ||
                           event->kind == CC_EVENT_DRAGON_RETALIATION ?
                           CC_VIOLET :
                           event->kind == CC_EVENT_SITUATION_FAILED ||
                           event->kind == CC_EVENT_SETTLEMENT_RAIDED ||
                           event->kind == CC_EVENT_SHIPMENT_LOST ||
                           event->kind == CC_EVENT_JOURNEY_WARNING ?
                           DANGER : TEAL;
        CcOverlayDrawText(CcEventKindName(event->kind), x, y + 27, 10,
                          kind_color);
        DrawTwoLineText(event->text, x + 132, y + 7,
                        line_capacity, 10, INK);
    }
    if (shown == 0) {
        CcOverlayDrawText("No events have reached the ledger yet.",
                          x, (int)bounds.y + 86, 12, MUTED);
    }
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
        if (!SituationVisibleToPlayer(sim, i)) continue;
        if (i == selected) active_ordinal = active_count;
        active_count += 1;
    }
    bool selected_is_lead = detail != NULL &&
        !CcSimSituationCanAccept(sim, detail);
    CcOverlayDrawText(selected_is_lead ? "OBJECTIVE" : "QUEST",
                      360, 216, 10, TEAL);
    CcOverlayDrawText(active_count > 0 ?
             TextFormat("%d / %d", active_ordinal + 1, active_count) : "0 / 0",
             874, 216, 10, CC_GOLD);
    if (detail != NULL) {
        char target[96];
        SituationTargetLabel(sim, detail, target, sizeof(target));
        const char *title = detail->kind == CC_SITUATION_MONSTER_EXPEDITION &&
                !CcSimSituationCanAccept(sim, detail) ?
            "Strange noises in the mine" : SituationTitle(detail->kind);
        CcOverlayDrawText(title, 360, 250, 21,
                 SituationColor(detail->kind));
        CcOverlayDrawText(TextFormat("%s  /  %s", target,
                            detail->affected_name[0] != '\0' ?
                                detail->affected_name : "Someone waiting"),
                 360, 284, 11, INK);
        char next[192];
        SituationNextAction(sim, detail, next, sizeof(next));
        CcOverlayDrawText("NEXT STEP", 360, 323, 9, MUTED);
        DrawTwoLineText(next, 360, 346, 58U, 11, CC_GOLD);
        if (CcSimSituationCanAccept(sim, detail)) {
            CcOverlayDrawText(TextFormat("DUE DAY %d", detail->deadline_day),
                     360, 408, 10, MUTED);
            CcOverlayDrawText(
                TextFormat("REWARD  +%" PRId64 " CROWNS", detail->reward),
                714, 408, 10, TEAL);
        } else {
            CcOverlayDrawText("NO JOB HAS BEEN OFFERED YET",
                              360, 408, 10, MUTED);
        }
    } else {
        CcOverlayDrawText("NO QUESTS", 360, 268, 17, MUTED);
    }
}

static int32_t CarriagePassengerCount(const CcSim *sim)
{
    int32_t passengers = 0;
    if (sim == NULL) return passengers;
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        if (sim->couriers[i].status == CC_COURIER_WITH_PLAYER) {
            passengers += 1;
        }
    }
    return passengers;
}

static void DrawCarriageScreen(const CcSim *sim, const LocalState *local)
{
    if (sim == NULL || local == NULL) return;
    const CcSettlement *place = CcSimSettlement(
        sim, sim->player.location_id);
    const CcSituation *quest = CcSimAcceptedSituation(sim);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.78f));
    DrawPanel((Rectangle){24.0f, 78.0f, 1232.0f, 666.0f}, PANEL_DEEP);
    CcOverlayDrawText("THE CROWNLESS CARRIAGE", 52, 102, 23, INK);
    CcOverlayDrawText(
        TextFormat("%s / %s",
                   local->site_kind == CC_LOCAL_SITE_NONE ?
                       "LOADING BAY" : "ROADSIDE BAY",
                   local->site_kind == CC_LOCAL_SITE_NONE ?
                       (place != NULL ? place->name : "TOWN") :
                       CcLocalSiteName(sim, local->site_kind)),
        52, 136, 10, TEAL);
    CcOverlayDrawText(
        TextFormat("DAY %d / %" PRId64 " CROWNS",
                   sim->current_day, sim->player.coins),
        1026, 112, 9, CC_GOLD);
    DrawRectangle(52, 156, 1176, 1, Fade(CC_GOLD, 0.42f));

    DrawPanel((Rectangle){52.0f, 174.0f, 350.0f, 454.0f},
              Fade(BACKGROUND, 0.76f));
    CcOverlayDrawText("CARGO MANIFEST", 72, 194, 15, CC_GOLD);
    int32_t cargo_used = CcPlayerCargoUsed(&sim->player);
    DrawBar(72, 224, 110, "LOAD", cargo_used * 100 /
            (sim->player.cargo_capacity > 0 ?
                 sim->player.cargo_capacity : 1), TEAL);
    CcOverlayDrawText(
        TextFormat("%d / %d SLOTS", cargo_used,
                   sim->player.cargo_capacity),
        276, 226, 9, INK);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t y = 260 + good * 54;
        Rectangle slot = {72.0f, (float)y, 310.0f, 43.0f};
        DrawRectangleRounded(slot, 0.14f, 4, Fade(PANEL_HOVER, 0.54f));
        DrawRectangleRoundedLinesEx(
            slot, 0.14f, 4, 1.0f,
            sim->player.cargo[good] > 0 ? Fade(TEAL, 0.72f) :
                                          Fade(MUTED, 0.24f));
        CcOverlayDrawText(TextFormat("SLOT %d", good + 1),
                          84, y + 8, 7, MUTED);
        CcOverlayDrawText((CcGood)good == CC_GOOD_FOOD ?
                              "FOOD BOXES" : CcGoodName((CcGood)good),
                          144, y + 12, 10, INK);
        const char *quantity = TextFormat("x%d", sim->player.cargo[good]);
        int32_t quantity_width = CcOverlayMeasureText(quantity, 11);
        CcOverlayDrawText(quantity, 368 - quantity_width,
                          y + 11, 11,
                          sim->player.cargo[good] > 0 ? CC_GOLD : MUTED);
    }

    DrawPanel((Rectangle){422.0f, 174.0f, 424.0f, 454.0f},
              Fade(BACKGROUND, 0.76f));
    CcOverlayDrawText("QUEST PAPERS", 444, 194, 15, TEAL);
    if (quest != NULL) {
        char target[96];
        char next[192];
        SituationTargetLabel(sim, quest, target, sizeof(target));
        SituationNextAction(sim, quest, next, sizeof(next));
        CcOverlayDrawText("ACTIVE PROMISE", 444, 230, 8, MUTED);
        CcOverlayDrawText(SituationTitle(quest->kind),
                          444, 254, 18, SituationColor(quest->kind));
        CcOverlayDrawText(TextFormat("%s / %.28s", target,
                                     quest->affected_name),
                          444, 285, 10, INK);
        CcOverlayDrawText("NEXT STEP", 444, 326, 8, MUTED);
        DrawTwoLineText(next, 444, 349, 54U, 11, CC_GOLD);
        CcOverlayDrawText(
            TextFormat("DUE DAY %d", quest->deadline_day),
            444, 414, 9, MUTED);
        CcOverlayDrawText(
            TextFormat("REWARD +%" PRId64 " CROWNS", quest->reward),
            630, 414, 9, TEAL);
        if (quest->kind == CC_SITUATION_RELIEF_DELIVERY ||
            quest->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            int32_t remaining = quest->quantity - quest->progress;
            if (remaining < 0) remaining = 0;
            int32_t aboard = sim->player.cargo[quest->good] < remaining ?
                sim->player.cargo[quest->good] : remaining;
            CcOverlayDrawText("COMMITTED LOAD", 444, 462, 8, MUTED);
            CcOverlayDrawText(
                quest->good == CC_GOOD_FOOD ?
                    TextFormat("%d / %d food boxes aboard",
                               aboard, remaining) :
                    TextFormat("%d / %d %s aboard", aboard, remaining,
                               CcGoodName(quest->good)),
                444, 485, 11,
                sim->player.cargo[quest->good] >= remaining ? TEAL : DANGER);
        }
    } else {
        CcOverlayDrawText("NO ACTIVE PROMISE", 444, 244, 17, MUTED);
        DrawTwoLineText(
            "The quest papers are empty. Review the town notices before committing the carriage.",
            444, 282, 55U, 11, INK);
    }
    CcFoodEconomy food = {0};
    if (place != NULL && CcSimFoodEconomyAtSettlement(
            sim, place->id, &food)) {
        const char *food_state = food.stock < food.weekly_consumption * 2 ?
            "SHORTAGE" : food.stock < food.reserve_target ? "TIGHT" :
            "SURPLUS";
        CcOverlayDrawText("LOCAL FOOD ECONOMY", 444, 526, 8, MUTED);
        CcOverlayDrawText(
            TextFormat("%s  %d boxes  +%d/-%d weekly  %dc each",
                       food_state, food.stock, food.weekly_production,
                       food.weekly_consumption, food.unit_price),
            444, 549, 9,
            strcmp(food_state, "SHORTAGE") == 0 ? DANGER : TEAL);
    }
    CcOverlayDrawText("Q  REVIEW ALL AVAILABLE QUESTS",
                      444, 584, 8, MUTED);

    DrawPanel((Rectangle){866.0f, 174.0f, 362.0f, 454.0f},
              Fade(BACKGROUND, 0.76f));
    CcOverlayDrawText("TEAM & DEPARTURE", 888, 194, 15, CC_VIOLET);
    for (int32_t horse = 0; horse < CC_CARRIAGE_HORSE_COUNT; ++horse) {
        int32_t y = 232 + horse * 70;
        CcOverlayDrawText(sim->horse_team[horse].name, 888, y, 13, INK);
        CcOverlayDrawText(
            TextFormat("HEALTH %d / FATIGUE %d",
                       sim->horse_team[horse].health,
                       sim->horse_team[horse].fatigue),
            888, y + 25, 8, MUTED);
    }
    DrawBar(888, 382, 104, "TEAM",
            CcSimHorseTeamReadiness(sim), TEAL);
    DrawBar(888, 424, 104, "WAGON",
            sim->carriage.condition, CC_GOLD);
    CcOverlayDrawText(
        TextFormat("PASSENGER BERTHS  %d / %d",
                   CarriagePassengerCount(sim),
                   sim->player.passenger_capacity),
        888, 478, 9, INK);
    CcOverlayDrawText(
        TextFormat("MAP CASE  %d / %d",
                   CcPlayerMapCount(sim), sim->player.map_capacity),
        888, 506, 9, INK);
    CcOverlayDrawText(
        local->site_kind == CC_LOCAL_SITE_NONE ?
            TextFormat("%d ROADS LEAVE THIS TOWN",
                       OutgoingRouteCount(sim)) :
            "RETURN ROAD IS READY",
        888, 552, 10, CC_GOLD);
    CcOverlayDrawText(
        CcSimHorseTeamReadiness(sim) < 30 ?
            "TEAM MUST REST BEFORE DEPARTURE" :
            "DEPARTURE CHECKS READY",
        888, 584, 8,
        CcSimHorseTeamReadiness(sim) < 30 ? DANGER : TEAL);
}

static void DrawCharacterConversation(const CcSim *sim,
                                      const LocalState *local)
{
    if (sim == NULL || local == NULL) return;
    const CcSituation *situation = CcSimSituation(
        sim, local->conversation_situation_id);
    const CcCharacter *character = CcSimCharacter(
        sim, local->conversation_character_id);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.14f));
    float panel_y = (float)GetScreenHeight() - 268.0f;
    Rectangle bounds = {18.0f, panel_y,
                        (float)GetScreenWidth() - 36.0f, 174.0f};
    DrawPanel(bounds, PANEL_DEEP);
    if (situation == NULL || character == NULL) {
        CcOverlayDrawText("No one is here.", 44, (int)panel_y + 42,
                          18, MUTED);
        return;
    }
    float speaker_width = fminf(260.0f, fmaxf(210.0f, bounds.width * 0.23f));
    int32_t speaker_x = (int)bounds.x + 24;
    int32_t speech_x = (int)(bounds.x + speaker_width + 34.0f);
    int32_t text_right = (int)(bounds.x + bounds.width - 26.0f);
    DrawRectangleRounded(
        (Rectangle){bounds.x + 10.0f, bounds.y + 18.0f, 4.0f,
                    bounds.height - 36.0f},
        0.9f, 3, CC_GOLD);
    DrawLine((int)(bounds.x + speaker_width + 17.0f),
             (int)bounds.y + 20,
             (int)(bounds.x + speaker_width + 17.0f),
             (int)(bounds.y + bounds.height) - 20, Fade(MUTED, 0.42f));
    CcOverlayDrawText("CONVERSATION", speaker_x, (int)panel_y + 20,
                      8, TEAL);
    CcOverlayDrawText(character->name, speaker_x, (int)panel_y + 43,
                      19, CC_GOLD);
    CcOverlayDrawText(SituationTitle(situation->kind), speaker_x,
                      (int)panel_y + 76, 10, MUTED);
    char spoken[192];
    bool has_spoken = CcStoryCharacterText(
        sim, situation, character, spoken, sizeof(spoken));
    CcOverlayDrawText("SPEAKS", speech_x, (int)panel_y + 20, 8, TEAL);
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        const CcRelationship *relationship = CcSimRelationship(
            sim, situation->affected_character_id,
            situation->sponsor_character_id);
        if (relationship != NULL &&
            (character->id == situation->affected_character_id ||
             character->id == situation->sponsor_character_id)) {
            CcOverlayDrawText(
                TextFormat("Jory and Mara: %s",
                           CcRelationshipHistoryName(
                               relationship->history)),
                speaker_x, (int)panel_y + 105, 8, MUTED);
        }
    }
    CcOverlayDrawText("\"", speech_x, (int)panel_y + 42, 24, MUTED);
    int32_t available_width = text_right - speech_x - 30;
    size_t line_capacity = (size_t)(available_width / 9);
    if (line_capacity < 48U) line_capacity = 48U;
    if (line_capacity > 104U) line_capacity = 104U;
    DrawTwoLineText(has_spoken ? spoken :
                        "They have nothing more to ask.",
                    speech_x + 26, (int)panel_y + 48,
                    line_capacity, 17, INK);
    CcOverlayDrawText("CHOOSE A REPLY BELOW", speech_x,
                      (int)(panel_y + bounds.height) - 24, 8, MUTED);
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
        DrawTwoLineText(CcStoryRoadCompanyLine(bandits),
                        386, 352, 56U, 10, INK);
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
        case CC_COMMAND_STEAL_DRAGON_HOARD:
        case CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE:
            confirmation = "The hoard remembers what you took.";
            break;
        case CC_COMMAND_RETURN_DRAGON_TREASURE:
        case CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE:
            confirmation = "The dragon's memory settles.";
            break;
        case CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE:
            confirmation = "The tribute is yours. The hoard never received it.";
            break;
        case CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL:
            confirmation = "The goblin tunnel is behind you.";
            break;
        case CC_COMMAND_BEGIN_DUNGEON_EXPEDITION:
            confirmation = "The company enters the Underroad.";
            break;
        case CC_COMMAND_MOVE_DUNGEON:
            confirmation = "One dungeon turn passes.";
            break;
        case CC_COMMAND_SEARCH_DUNGEON:
            confirmation = "The chamber is searched.";
            break;
        case CC_COMMAND_OPEN_DUNGEON_SHORTCUT:
            confirmation = "The shortcut will remain open.";
            break;
        case CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER:
            confirmation = sim->dungeon_expedition.encounter_kind ==
                    CC_DUNGEON_ENCOUNTER_NONE ?
                "The way is clear." : "The encounter is not settled.";
            break;
        case CC_COMMAND_RETREAT_DUNGEON:
            confirmation = "The company reaches the carriage.";
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
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                local->journey_travel_active = false;
                *view = VIEW_ENCOUNTER;
                GameplayReelSetStage(reel, GAMEPLAY_REEL_ROAD_CHOICE);
            } else if (!sim->journey.active) {
                const CcSituation *accepted = CcSimAcceptedSituation(sim);
                CcId target = SituationSettlementId(sim, accepted);
                CcLocalBindPlace(sim);
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
            if (!sim->journey.active) {
                const CcSituation *accepted = CcSimAcceptedSituation(sim);
                CcId target = SituationSettlementId(sim, accepted);
                CcLocalBindPlace(sim);
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

static bool HandleExpedition(CcJournal *journal, CcSim *sim,
                             const CcDungeon *dungeon,
                             char *message, size_t message_capacity)
{
    if (dungeon == NULL) return false;
    CcCommand expedition = {
        .kind = CC_COMMAND_BEGIN_DUNGEON_EXPEDITION,
        .target_id = dungeon->id
    };
    if (!ApplyCommand(journal, sim, expedition, message, message_capacity)) {
        return false;
    }
    (void)snprintf(message, message_capacity,
                   "The company enters the Underroad. Light is burning.");
    return true;
}

static void FinishTownArrivalState(const CcSim *sim, LocalState *local,
                                   int32_t *selected, char *message,
                                   size_t message_capacity)
{
    *selected = FirstOutgoingRouteIndex(sim);
    LeaveOpenWorld(local);
    ResetLocalState(local);
    (void)snprintf(message, message_capacity,
                   "The team is watered and stabled.");
}

static bool HandleTownArrivalAction(
    const CcSim *sim, LocalState *local, int32_t *selected,
    ContextActionKind context_action, bool enter_pressed,
    char *message, size_t message_capacity)
{
    bool parking_requested =
        context_action == CONTEXT_ACTION_SKIP_TRAVEL || enter_pressed;
    if (!parking_requested || sim == NULL || sim->journey.active ||
        local == NULL || !local->journey_travel_active ||
        (!RoadBookArrivalInProgress(local) &&
         local->convoy.phase != CC_LOCAL_CONVOY_ARRIVING)) {
        return false;
    }
    CcClientArrivalComplete(&local->arrival);
    FinishTownArrivalState(
        sim, local, selected, message, message_capacity);
    return true;
}

#if defined(CC_CLIENT_SELF_TESTS)
static int RunTownArrivalParkingRegression(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    LocalState skipped = {0};
    ResetLocalState(&skipped);
    skipped.journey_travel_active = true;
    skipped.convoy.phase = CC_LOCAL_CONVOY_ARRIVING;
    skipped.convoy.phase_progress = 0.47f;
    skipped.convoy.pace = 0.48f;

    ContextActionSet actions = BuildContextActions(
        &sim, &skipped, VIEW_LOCAL, -1, -1);
    ContextAction park = {.kind = CONTEXT_ACTION_NONE};
    for (int32_t i = 0; i < actions.count; ++i) {
        if (actions.items[i].kind == CONTEXT_ACTION_SKIP_TRAVEL) {
            park = actions.items[i];
            break;
        }
    }
    if (park.kind != CONTEXT_ACTION_SKIP_TRAVEL ||
        strcmp(park.label, "Park carriage") != 0) {
        (void)fprintf(stderr,
                      "Partial arrival did not offer Park carriage.\n");
        return 1;
    }

    int32_t selected = -1;
    char message[96] = "";
    if (!HandleTownArrivalAction(
            &sim, &skipped, &selected, park.kind, false,
            message, sizeof(message))) {
        (void)fprintf(stderr, "Park carriage was not handled.\n");
        return 1;
    }
    if (skipped.journey_travel_active ||
        skipped.convoy.phase != CC_LOCAL_CONVOY_PARKED ||
        skipped.convoy.phase_progress != 0.0f ||
        selected != FirstOutgoingRouteIndex(&sim) ||
        strcmp(message, "The team is watered and stabled.") != 0) {
        (void)fprintf(stderr,
                      "Park carriage did not reach the parked town state.\n");
        return 1;
    }

    int32_t parked_selection = selected;
    if (HandleTownArrivalAction(
            &sim, &skipped, &selected, park.kind, true,
            message, sizeof(message)) ||
        skipped.journey_travel_active ||
        skipped.convoy.phase != CC_LOCAL_CONVOY_PARKED ||
        skipped.convoy.phase_progress != 0.0f ||
        selected != parked_selection) {
        (void)fprintf(stderr, "Repeated Enter changed the parked state.\n");
        return 1;
    }

    LocalState natural = {0};
    ResetLocalState(&natural);
    natural.journey_travel_active = true;
    natural.convoy.phase = CC_LOCAL_CONVOY_ARRIVING;
    natural.convoy.phase_progress = 1.0f;
    int32_t natural_selection = -1;
    char natural_message[96] = "";
    FinishTownArrivalState(
        &sim, &natural, &natural_selection,
        natural_message, sizeof(natural_message));
    if (natural.journey_travel_active != skipped.journey_travel_active ||
        natural.convoy.phase != skipped.convoy.phase ||
        natural.convoy.phase_progress != skipped.convoy.phase_progress ||
        natural_selection != selected ||
        strcmp(natural_message, message) != 0) {
        (void)fprintf(stderr,
                      "Natural and skipped arrival finished differently.\n");
        return 1;
    }

    (void)puts("Town arrival parking regression passed");
    return 0;
}

static float TownDepartureDistance(Vector3 first, Vector3 second)
{
    float x = first.x - second.x;
    float z = first.z - second.z;
    return sqrtf(x * x + z * z);
}

static bool FirstJourneyPoseContinuesFromJunction(bool reverse)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a7137e));
    CcRoute *route = &sim.routes[0];
    CcId origin_id = reverse ? route->to_id : route->from_id;
    CcId destination_id = reverse ? route->from_id : route->to_id;
    sim.player.location_id = origin_id;
    sim.carriage.location_id = origin_id;
    for (int32_t map_index = 0;
         map_index < sim.map_count && map_index < CC_MAX_MAPS; ++map_index) {
        CcMap *map = &sim.maps[map_index];
        if (map->route_id != route->id) continue;
        if (map->owner_id == sim.player.id) {
            map->owner_id = origin_id;
        }
        uint32_t bit = UINT32_C(1) << (uint32_t)map_index;
        sim.player.map_catalogue_mask &= ~bit;
        sim.player.map_archive_mask &= ~bit;
    }
    CcSimInitializePlayerRouteKnowledge(&sim);
    route->closed = false;
    route->security = 100;
    route->condition = 100;

    LocalState local = {0};
    ResetLocalState(&local);
    if (!InitializeOpenWorld(&sim, &local, false) ||
        !EnterOpenWorldAtRoadGate(&sim, &local, route->id)) {
        return false;
    }
    local.road_choice_active = true;
    local.departure = (CcClientDepartureTransition){
        .phase = CC_CLIENT_DEPARTURE_ROAD_BOOK,
        .town_progress = 1.0f,
        .road_book_progress = 0.0f,
    };
    local.convoy.phase = CC_LOCAL_CONVOY_DEPARTING;
    local.convoy.pace = 1.0f;
    local.world_carriage.camera_weight = 0.0f;
    local.world_carriage.camera_target = 1.0f;
    for (int32_t step = 0;
         step < 40 && RoadBookDepartureInProgress(&local); ++step) {
        UpdateOpenWorldCamera(&sim, &local, 0.10f);
    }
    if (local.departure.phase != CC_CLIENT_DEPARTURE_READY ||
        local.world_carriage.camera_weight < 0.9999f) {
        return false;
    }
    Vector3 junction_position = local.world_carriage.position;
    float junction_route_amount = local.world_carriage.route_amount;
    float junction_heading = local.world_carriage.heading_yaw;
    CcRoadBookRouteView before = {0};
    if (!CcRoadBookReadRoute(&sim, route->id, &before) || before.charted) {
        return false;
    }

    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = destination_id
    };
    char error[160];
    if (!CcSimApply(&sim, &travel, error, sizeof(error))) return false;
    BeginRoadTravelState(&sim, &local);
    const CcWorldRoutePlacement *placement = CcWorldRoutePlacementForId(
        &local.world_stream.manifest, route->id);
    CcRoadBookRouteView after = {0};
    if (placement == NULL || !CcRoadBookReadRouteAtCarriage(
            &sim, route->id, local.world_carriage.route_amount,
            CcWorldRouteLength(placement), &after) || after.charted) {
        return false;
    }
    float dx = local.world_carriage.position.x - junction_position.x;
    float dy = local.world_carriage.position.y - junction_position.y;
    float dz = local.world_carriage.position.z - junction_position.z;
    float pose_distance = sqrtf(dx * dx + dy * dy + dz * dz);
    float heading_difference =
        local.world_carriage.heading_yaw - junction_heading;
    return local.open_world && local.world_carriage.visible &&
           pose_distance <= 0.001f &&
           fabsf(local.world_carriage.route_amount - junction_route_amount) <=
               0.0001f &&
           cosf(heading_difference) >= 0.9999f &&
           after.from_reveal + 0.0001f >= before.from_reveal &&
           after.to_reveal + 0.0001f >= before.to_reveal &&
           CcRoadBookShowsRouteAmount(
               &after, local.world_carriage.route_amount);
}

static int RunTownDepartureRegression(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a7137e));
    LocalState local = {0};
    ResetLocalState(&local);
    if (!InitializeOpenWorld(&sim, &local, false)) {
        (void)fprintf(stderr, "Departure world setup failed.\n");
        return 1;
    }
    int32_t selected = FirstOutgoingRouteIndex(&sim);
    const CcRoute *route = SelectedOutgoingRoute(&sim, selected);
    if (route == NULL) {
        (void)fprintf(stderr, "Departure route setup failed.\n");
        return 1;
    }
    const CcWorldSettlementPlacement *place =
        CcWorldSettlementPlacementForId(
            &local.world_stream.manifest, sim.player.location_id);
    if (place == NULL) {
        (void)fprintf(stderr, "Departure town setup failed.\n");
        return 1;
    }

    BeginRoadChoiceApproachState(&local, true);
    local.convoy.pace = 1.0f;
    local.departure.town_progress = 0.99f;
    local.convoy.phase_progress = 0.99f;
    if (!UpdateRoadChoiceApproach(&local, 1.0f) ||
        local.departure.phase != CC_CLIENT_DEPARTURE_ROAD_BOOK ||
        local.convoy.phase != CC_LOCAL_CONVOY_DEPARTING ||
        fabsf(local.convoy.town_position.x - CC_LOCAL_TOWN_GATE_X) > 0.001f ||
        fabsf(local.convoy.town_position.z - CC_LOCAL_TOWN_GATE_Z) > 0.001f ||
        fabsf(local.convoy.town_heading_yaw - 0.5f * PI) > 0.001f) {
        (void)fprintf(stderr,
                      "Town departure did not finish at the authored gate.\n");
        return 1;
    }

    if (!EnterRoadBookFromTownGate(&sim, &local, route->id)) {
        (void)fprintf(stderr, "Road book did not open at the town gate.\n");
        return 1;
    }
    Vector3 gate = {place->gate.x, local.world_carriage.position.y,
                    place->gate.z};
    if (!local.open_world ||
        TownDepartureDistance(local.world_carriage.position, gate) > 0.001f ||
        fabsf(sinf(local.world_carriage.heading_yaw) -
              sinf(place->entrance_heading_yaw)) > 0.001f ||
        fabsf(cosf(local.world_carriage.heading_yaw) -
              cosf(place->entrance_heading_yaw)) > 0.001f ||
        fabsf(local.world_carriage.pace - local.convoy.pace) > 0.001f ||
        local.world_carriage.camera_weight != 0.0f) {
        (void)fprintf(stderr,
                      "Road-book gate pose did not match the town exit.\n");
        return 1;
    }
    ContextActionSet rising_actions = BuildContextActions(
        &sim, &local, VIEW_ROADS, selected, -1);
    if (rising_actions.count != 0) {
        (void)fprintf(stderr,
                      "Road controls opened before the camera reached the junction.\n");
        return 1;
    }

    UpdateOpenWorldCamera(&sim, &local, 0.25f);
    if (local.world_carriage.camera_weight <= 0.0f ||
        local.world_carriage.camera_weight >= 1.0f ||
        TownDepartureDistance(local.world_carriage.position, gate) <= 0.01f) {
        (void)fprintf(stderr,
                      "Carriage did not follow the rising road book.\n");
        return 1;
    }
    for (int32_t step = 0;
         step < 20 && RoadBookDepartureInProgress(&local); ++step) {
        UpdateOpenWorldCamera(&sim, &local, 0.10f);
    }
    Vector3 junction = {place->junction.x, local.world_carriage.position.y,
                        place->junction.z};
    ContextActionSet ready_actions = BuildContextActions(
        &sim, &local, VIEW_ROADS, selected, -1);
    if (local.departure.phase != CC_CLIENT_DEPARTURE_READY ||
        local.convoy.phase != CC_LOCAL_CONVOY_ROAD ||
        local.world_carriage.camera_weight != 1.0f ||
        local.world_carriage.pace != 0.0f ||
        TownDepartureDistance(local.world_carriage.position, junction) >
            0.001f ||
        ready_actions.count <= 0) {
        (void)fprintf(stderr,
                      "Road-book departure did not finish at the junction.\n");
        return 1;
    }
    Vector3 settled_position = local.world_carriage.position;
    UpdateOpenWorldCamera(&sim, &local, 1.0f);
    if (TownDepartureDistance(
            local.world_carriage.position, settled_position) > 0.001f) {
        (void)fprintf(stderr,
                      "Completed road-book departure moved twice.\n");
        return 1;
    }
    if (!FirstJourneyPoseContinuesFromJunction(false) ||
        !FirstJourneyPoseContinuesFromJunction(true)) {
        (void)fprintf(stderr,
                      "Active travel did not continue from its departure junction.\n");
        return 1;
    }
    (void)puts("Town departure regression passed");
    return 0;
}

static int RunRoadBookArrivalRegression(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcId origin_id = sim.player.location_id;
    const CcRoute *route = OpenWorldRouteFromSettlement(&sim, origin_id);
    CcId destination_id = route != NULL ?
        RouteOtherEnd(route, origin_id) : 0U;
    if (route == NULL || destination_id == 0U) {
        (void)fprintf(stderr, "Arrival route setup failed.\n");
        return 1;
    }
    sim.journey.origin_id = origin_id;
    sim.journey.destination_id = destination_id;
    sim.journey.route_id = route->id;
    sim.player.location_id = destination_id;

    LocalState natural = {0};
    ResetLocalState(&natural);
    if (!InitializeOpenWorld(&sim, &natural, false) ||
        !EnterOpenWorldAtRoadGate(&sim, &natural, route->id)) {
        (void)fprintf(stderr, "Arrival world setup failed.\n");
        return 1;
    }
    natural.convoy.pace = 0.72f;
    BeginRoadBookArrivalState(&sim, &natural);
    const CcWorldSettlementPlacement *destination =
        CcWorldSettlementPlacementForId(
            &natural.world_stream.manifest, destination_id);
    if (!RoadBookArrivalInProgress(&natural) || destination == NULL ||
        !natural.world_carriage.town_arrival ||
        natural.world_carriage.camera_weight != 1.0f ||
        fabsf(natural.world_carriage.position.x - destination->junction.x) >
            0.01f ||
        fabsf(natural.world_carriage.position.z - destination->junction.z) >
            0.01f) {
        (void)fprintf(stderr,
                      "Arrival did not start at the destination junction.\n");
        return 1;
    }

    UpdateOpenWorldCamera(&sim, &natural, 1.0f);
    if (natural.arrival.phase != CC_CLIENT_ARRIVAL_TOWN ||
        natural.world_carriage.camera_weight != 0.0f ||
        fabsf(natural.world_carriage.position.x - destination->gate.x) >
            0.01f ||
        fabsf(natural.world_carriage.position.z - destination->gate.z) >
            0.01f) {
        (void)fprintf(stderr,
                      "Arrival did not close on the destination gate.\n");
        return 1;
    }
    Vector3 world_gate = natural.world_carriage.position;
    float world_heading = natural.world_carriage.heading_yaw;
    BeginTownArrivalState(&natural);
    CcWorldPoint local_gate = CcWorldSettlementLocalPoint(
        &natural.world_stream.manifest, destination_id,
        natural.convoy.town_position.x, natural.convoy.town_position.z);
    float transformed_heading = natural.convoy.town_heading_yaw +
        destination->entrance_heading_yaw - 0.5f * PI;
    if (natural.open_world ||
        natural.world_carriage.town_arrival ||
        natural.convoy.phase != CC_LOCAL_CONVOY_ARRIVING ||
        natural.arrival.phase != CC_CLIENT_ARRIVAL_TOWN ||
        fabsf(local_gate.x - world_gate.x) > 0.01f ||
        fabsf(local_gate.z - world_gate.z) > 0.01f ||
        fabsf(WrapLocalAngle(transformed_heading - world_heading)) > 0.01f) {
        (void)fprintf(stderr,
                      "Arrival changed carriage pose at the town handoff.\n");
        return 1;
    }

    ConvoyUpdateResult result = CONVOY_UPDATE_NONE;
    int32_t arrival_updates = 0;
    while (result != CONVOY_UPDATE_PARKED && arrival_updates < 1200) {
        result = UpdateDrivenConvoy(&natural, &sim, 1.0f / 60.0f);
        arrival_updates += 1;
    }
    int32_t natural_selection = -1;
    char natural_message[96] = "";
    if (result != CONVOY_UPDATE_PARKED) {
        (void)fprintf(stderr, "Arrival did not reach the town yard.\n");
        return 1;
    }
    FinishTownArrivalState(
        &sim, &natural, &natural_selection,
        natural_message, sizeof(natural_message));

    LocalState reduced_motion = {0};
    ResetLocalState(&reduced_motion);
    if (!InitializeOpenWorld(&sim, &reduced_motion, false) ||
        !EnterOpenWorldAtRoadGate(&sim, &reduced_motion, route->id)) {
        (void)fprintf(stderr, "Reduced-motion arrival setup failed.\n");
        return 1;
    }
    reduced_motion.convoy.pace = 0.72f;
    BeginRoadBookArrivalState(&sim, &reduced_motion);
    int32_t reduced_selection = -1;
    char reduced_message[96] = "";
    if (!HandleTownArrivalAction(
            &sim, &reduced_motion, &reduced_selection,
            CONTEXT_ACTION_SKIP_TRAVEL, false,
            reduced_message, sizeof(reduced_message)) ||
        reduced_motion.journey_travel_active !=
            natural.journey_travel_active ||
        reduced_motion.open_world != natural.open_world ||
        reduced_motion.convoy.phase != natural.convoy.phase ||
        reduced_motion.arrival.phase != natural.arrival.phase ||
        reduced_motion.world_carriage.visible !=
            natural.world_carriage.visible ||
        reduced_motion.world_carriage.town_arrival !=
            natural.world_carriage.town_arrival ||
        reduced_selection != natural_selection ||
        strcmp(reduced_message, natural_message) != 0) {
        (void)fprintf(stderr,
                      "Reduced-motion arrival reached a different state.\n");
        return 1;
    }

    (void)puts("Road-book arrival regression passed");
    return 0;
}

static bool SessionTestFloatMatches(float first, float second)
{
    return fabsf(first - second) < 0.0001f;
}

static int SessionStartupTestFailed(const char *path, const char *message)
{
    CcLocalBindOpenWorld(NULL);
    (void)remove(path);
    (void)fprintf(stderr, "%s\n", message);
    return 1;
}

static bool WriteVersionThreeWorldSession(
    const char *path, const CcSim *sim,
    CcWorldPoint position, float facing_yaw)
{
    FILE *file = fopen(path, "wb");
    bool written = file != NULL &&
        fprintf(file,
                "CROWNLESS_SESSION 3\n%u %llu 0 1 %.9g %.9g %.9g 2\n",
                sim->world_seed,
                (unsigned long long)sim->player.location_id,
                (double)position.x, (double)position.z,
                (double)facing_yaw) > 0;
    if (file != NULL && fclose(file) != 0) written = false;
    return written;
}

static int RunWorldSessionStartupRegression(void)
{
    const char *session_path = "world-session-startup-test.state";
    (void)remove(session_path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a7118e));
    CcId branch_location_id = 0U;
    for (int32_t settlement = 0;
         settlement < sim.settlement_count; ++settlement) {
        int32_t route_count = 0;
        for (int32_t route_slot = 0;
             route_slot < sim.route_count; ++route_slot) {
            if (sim.routes[route_slot].from_id ==
                    sim.settlements[settlement].id ||
                sim.routes[route_slot].to_id ==
                    sim.settlements[settlement].id) {
                route_count += 1;
            }
        }
        if (route_count >= 2) {
            branch_location_id = sim.settlements[settlement].id;
            break;
        }
    }
    if (branch_location_id == 0U) {
        return SessionStartupTestFailed(
            session_path, "World session needs a branching settlement.");
    }
    sim.player.location_id = branch_location_id;
    sim.carriage.location_id = branch_location_id;
    CcLocalBindPlace(&sim);

    LocalState saved = {0};
    ResetLocalState(&saved);
    if (!InitializeOpenWorld(&sim, &saved, false)) {
        return SessionStartupTestFailed(
            session_path, "World session setup failed.");
    }
    const CcRoute *route = NULL;
    int32_t route_index = -1;
    int32_t incident_routes = 0;
    for (int32_t i = 0; i < sim.route_count; ++i) {
        if (sim.routes[i].from_id != sim.player.location_id &&
            sim.routes[i].to_id != sim.player.location_id) {
            continue;
        }
        incident_routes += 1;
        if (incident_routes == 2) {
            route = &sim.routes[i];
            route_index = i;
            break;
        }
    }
    if (route == NULL ||
        !EnterOpenWorldAtRoadGate(&sim, &saved, route->id)) {
        return SessionStartupTestFailed(
            session_path, "World session needs a second road branch.");
    }
    saved.world_carriage.camera_weight = 1.0f;
    saved.world_carriage.camera_target = 1.0f;
    saved.agent.facing_yaw = saved.world_carriage.heading_yaw;
    const float saved_x = saved.agent.position.x;
    const float saved_z = saved.agent.position.z;
    const float saved_yaw = saved.agent.facing_yaw;

    char error[192];
    if (!SaveLocalSession(session_path, &sim, &saved,
                          error, sizeof(error))) {
        return SessionStartupTestFailed(session_path, error);
    }
    CcClientSession stored = {0};
    if (!CcClientSessionRead(session_path, &stored,
                             error, sizeof(error)) ||
        stored.coordinate_space != CC_CLIENT_SESSION_WORLD ||
        stored.route_id != route->id ||
        !SessionTestFloatMatches(stored.position_x, saved_x) ||
        !SessionTestFloatMatches(stored.position_z, saved_z)) {
        return SessionStartupTestFailed(
            session_path, "World session was saved in the wrong space.");
    }

    LocalState restored = {0};
    ResetLocalState(&restored);
    if (!InitializeOpenWorld(&sim, &restored, false)) {
        return SessionStartupTestFailed(
            session_path, "World restore stream setup failed.");
    }
    ClientView view = VIEW_LOCAL;
    int32_t selected = -1;
    if (!RestoreClientStartupSession(
            session_path, &sim, &restored, &view, &selected)) {
        return SessionStartupTestFailed(
            session_path, "World session startup restore failed.");
    }
    const CcWorldChunk *focus = CcWorldStreamChunkAt(
        &restored.world_stream,
        restored.world_stream.focus_chunk_x,
        restored.world_stream.focus_chunk_z);
    if (view != VIEW_ROADS || !restored.open_world ||
        selected != route_index ||
        !SessionTestFloatMatches(restored.agent.position.x, saved_x) ||
        !SessionTestFloatMatches(restored.agent.position.z, saved_z) ||
        !SessionTestFloatMatches(restored.agent.facing_yaw, saved_yaw) ||
        !SessionTestFloatMatches(
            restored.agent.position.y,
            CcWorldStreamHeightAt(
                &restored.world_stream, saved_x, saved_z)) ||
        !SessionTestFloatMatches(
            restored.agent.position.y,
            CcLocalTerrainHeightAt(saved_x, saved_z)) ||
        restored.agent.scene != CC_LOCAL_SCENE_STREET ||
        restored.course.scene != CC_LOCAL_SCENE_STREET ||
        !restored.road_choice_active ||
        restored.departure.phase != CC_CLIENT_DEPARTURE_READY ||
        restored.departure.town_progress != 1.0f ||
        restored.departure.road_book_progress != 1.0f ||
        restored.convoy.phase != CC_LOCAL_CONVOY_ROAD ||
        restored.convoy.phase_progress != 1.0f ||
        restored.convoy.pace != 0.0f ||
        !restored.world_carriage.visible ||
        !restored.world_carriage.hero_embarked ||
        restored.world_carriage.route_id != route->id ||
        restored.world_carriage.camera_weight != 1.0f ||
        restored.world_carriage.camera_target != 1.0f ||
        !SessionTestFloatMatches(
            restored.world_carriage.position.x, saved_x) ||
        !SessionTestFloatMatches(
            restored.world_carriage.position.z, saved_z) ||
        !SessionTestFloatMatches(
            restored.world_carriage.heading_yaw, saved_yaw) ||
        focus == NULL || focus->state != CC_WORLD_CHUNK_READY) {
        return SessionStartupTestFailed(
            session_path, "World session restored an invalid road-book state.");
    }

    CcClientSession outside = stored;
    outside.position_x = restored.world_stream.manifest.maximum_x + 1.0f;
    if (!CcClientSessionWrite(session_path, &outside,
                              error, sizeof(error))) {
        return SessionStartupTestFailed(session_path, error);
    }
    LocalState rejected = {0};
    ResetLocalState(&rejected);
    if (!InitializeOpenWorld(&sim, &rejected, false)) {
        return SessionStartupTestFailed(
            session_path, "World bounds test setup failed.");
    }
    view = VIEW_LOCAL;
    int32_t rejected_selected = -1;
    if (RestoreClientStartupSession(
            session_path, &sim, &rejected, &view,
            &rejected_selected) || rejected.open_world) {
        return SessionStartupTestFailed(
            session_path, "World restore accepted a point beyond the kingdom.");
    }

    CcClientSession wrong_road = stored;
    for (int32_t i = 0; i < sim.route_count; ++i) {
        if (sim.routes[i].from_id != sim.player.location_id &&
            sim.routes[i].to_id != sim.player.location_id) {
            wrong_road.route_id = sim.routes[i].id;
            break;
        }
    }
    if (wrong_road.route_id == stored.route_id ||
        !CcClientSessionWrite(session_path, &wrong_road,
                              error, sizeof(error))) {
        return SessionStartupTestFailed(
            session_path, "World road validation setup failed.");
    }
    LocalState wrong_road_restore = {0};
    ResetLocalState(&wrong_road_restore);
    if (!InitializeOpenWorld(&sim, &wrong_road_restore, false)) {
        return SessionStartupTestFailed(
            session_path, "World road restore setup failed.");
    }
    view = VIEW_LOCAL;
    int32_t wrong_road_selected = -1;
    if (RestoreClientStartupSession(
            session_path, &sim, &wrong_road_restore, &view,
            &wrong_road_selected) || wrong_road_restore.open_world) {
        return SessionStartupTestFailed(
            session_path, "World restore accepted an unrelated road.");
    }

    if (!CcClientSessionWrite(session_path, &stored,
                              error, sizeof(error))) {
        return SessionStartupTestFailed(session_path, error);
    }
    LocalState missing_stream = {0};
    ResetLocalState(&missing_stream);
    view = VIEW_LOCAL;
    int32_t missing_selected = -1;
    if (RestoreClientStartupSession(
            session_path, &sim, &missing_stream, &view,
            &missing_selected) ||
        missing_stream.open_world) {
        return SessionStartupTestFailed(
            session_path, "World restore accepted an unprepared stream.");
    }

    const CcWorldRoutePlacement *route_placement =
        CcWorldRoutePlacementForId(
            &restored.world_stream.manifest, route->id);
    CcWorldPoint branch_position;
    float branch_heading = 0.0f;
    if (route_placement == NULL ||
        !CcWorldRoutePose(
            route_placement, sim.player.location_id, 0.18f,
            &branch_position, &branch_heading) ||
        !WriteVersionThreeWorldSession(
            session_path, &sim, branch_position, branch_heading)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 world session setup failed.");
    }
    LocalState version_three_restore = {0};
    ResetLocalState(&version_three_restore);
    if (!InitializeOpenWorld(&sim, &version_three_restore, false)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 world stream setup failed.");
    }
    view = VIEW_LOCAL;
    int32_t version_three_selected = -1;
    if (!RestoreClientStartupSession(
            session_path, &sim, &version_three_restore, &view,
            &version_three_selected) ||
        view != VIEW_ROADS || !version_three_restore.open_world ||
        version_three_restore.world_carriage.route_id != route->id ||
        version_three_selected != route_index ||
        !SessionTestFloatMatches(
            version_three_restore.agent.position.x, branch_position.x) ||
        !SessionTestFloatMatches(
            version_three_restore.agent.position.z, branch_position.z)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 world session chose the wrong branch.");
    }

    int32_t junction_sample = route->from_id == sim.player.location_id ?
        CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE :
        CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE;
    float junction_route_amount = CcWorldRouteSampleAmount(
        route_placement, junction_sample);
    float junction_journey_amount =
        route->from_id == sim.player.location_id ?
            junction_route_amount : 1.0f - junction_route_amount;
    CcWorldPoint shared_gate_position;
    float shared_gate_heading = 0.0f;
    const CcWorldSettlementPlacement *branch_place =
        CcWorldSettlementPlacementForId(
            &restored.world_stream.manifest, sim.player.location_id);
    if (branch_place == NULL ||
        !CcWorldRoutePose(
            route_placement, sim.player.location_id,
            junction_journey_amount,
            &shared_gate_position, &shared_gate_heading) ||
        !SessionTestFloatMatches(
            shared_gate_position.x, branch_place->junction.x) ||
        !SessionTestFloatMatches(
            shared_gate_position.z, branch_place->junction.z) ||
        !WriteVersionThreeWorldSession(
            session_path, &sim, branch_place->gate,
            shared_gate_heading)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 shared gate setup failed.");
    }
    LocalState shared_gate_restore = {0};
    ResetLocalState(&shared_gate_restore);
    if (!InitializeOpenWorld(&sim, &shared_gate_restore, false)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 shared gate stream setup failed.");
    }
    view = VIEW_LOCAL;
    int32_t shared_gate_selected = -1;
    if (!RestoreClientStartupSession(
            session_path, &sim, &shared_gate_restore, &view,
            &shared_gate_selected) ||
        view != VIEW_ROADS || !shared_gate_restore.open_world ||
        shared_gate_restore.world_carriage.route_id != route->id ||
        shared_gate_selected != route_index ||
        !SessionTestFloatMatches(
            shared_gate_restore.agent.position.x, branch_place->gate.x) ||
        !SessionTestFloatMatches(
            shared_gate_restore.agent.position.z, branch_place->gate.z)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 gate heading chose the wrong branch.");
    }

    if (!WriteVersionThreeWorldSession(
            session_path, &sim, branch_place->center,
            shared_gate_heading)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 off-road setup failed.");
    }
    LocalState off_road_restore = {0};
    ResetLocalState(&off_road_restore);
    if (!InitializeOpenWorld(&sim, &off_road_restore, false)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 off-road stream setup failed.");
    }
    view = VIEW_LOCAL;
    int32_t off_road_selected = -1;
    if (RestoreClientStartupSession(
            session_path, &sim, &off_road_restore, &view,
            &off_road_selected) || off_road_restore.open_world) {
        return SessionStartupTestFailed(
            session_path, "Version 3 restore accepted an off-road point.");
    }

    CcLocalBindOpenWorld(NULL);
    (void)remove(session_path);
    (void)puts("World session startup regression passed");
    return 0;
}

static int RunTownSessionStartupRegression(void)
{
    const char *session_path = "town-session-startup-test.state";
    (void)remove(session_path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a7118f));
    CcLocalBindPlace(&sim);

    LocalState saved = {0};
    ResetLocalState(&saved);
    RepositionHero(&saved, (Vector2){31.25f, 22.75f}, false);
    saved.agent.facing_yaw = -0.45f;
    saved.opening_step = CC_LOCAL_OPENING_COMPLETE;
    char error[192];
    if (!SaveLocalSession(session_path, &sim, &saved,
                          error, sizeof(error))) {
        return SessionStartupTestFailed(session_path, error);
    }
    CcClientSession stored = {0};
    if (!CcClientSessionRead(session_path, &stored,
                             error, sizeof(error)) ||
        stored.coordinate_space != CC_CLIENT_SESSION_LEGACY_LOCAL) {
        return SessionStartupTestFailed(
            session_path, "Town session was saved in the wrong space.");
    }

    LocalState restored = {0};
    ResetLocalState(&restored);
    if (!InitializeOpenWorld(&sim, &restored, false)) {
        return SessionStartupTestFailed(
            session_path, "Town restore stream setup failed.");
    }
    ClientView view = VIEW_ROADS;
    int32_t selected = -1;
    if (!RestoreClientStartupSession(
            session_path, &sim, &restored, &view, &selected) ||
        view != VIEW_LOCAL || restored.open_world ||
        restored.world_carriage.visible ||
        restored.world_carriage.camera_weight != 0.0f ||
        !SessionTestFloatMatches(
            restored.agent.position.x, stored.position_x) ||
        !SessionTestFloatMatches(
            restored.agent.position.z, stored.position_z) ||
        !SessionTestFloatMatches(
            restored.agent.facing_yaw, stored.facing_yaw) ||
        restored.agent.scene != CC_LOCAL_SCENE_STREET ||
        restored.course.scene != CC_LOCAL_SCENE_STREET ||
        restored.road_choice_active) {
        return SessionStartupTestFailed(
            session_path, "Town session left the authored town layout.");
    }

    FILE *legacy = fopen(session_path, "wb");
    bool legacy_written = legacy != NULL &&
        fprintf(legacy,
                "CROWNLESS_SESSION 2\n%u %llu 0 18.5 14.25 0.35 2\n",
                sim.world_seed,
                (unsigned long long)sim.player.location_id) > 0;
    if (legacy != NULL && fclose(legacy) != 0) legacy_written = false;
    if (!legacy_written) {
        return SessionStartupTestFailed(
            session_path, "Legacy town session setup failed.");
    }
    LocalState legacy_restored = {0};
    ResetLocalState(&legacy_restored);
    if (!InitializeOpenWorld(&sim, &legacy_restored, false)) {
        return SessionStartupTestFailed(
            session_path, "Legacy town stream setup failed.");
    }
    const CcRoute *route = OpenWorldRouteFromSettlement(
        &sim, sim.player.location_id);
    if (route == NULL ||
        !EnterOpenWorldAtRoadGate(&sim, &legacy_restored, route->id)) {
        return SessionStartupTestFailed(
            session_path, "Legacy town mode setup failed.");
    }
    view = VIEW_ROADS;
    if (!RestoreClientStartupSession(
            session_path, &sim, &legacy_restored, &view, &selected) ||
        view != VIEW_LOCAL || legacy_restored.open_world ||
        legacy_restored.world_carriage.visible ||
        !SessionTestFloatMatches(
            legacy_restored.agent.position.x, 18.5f) ||
        !SessionTestFloatMatches(
            legacy_restored.agent.position.z, 14.25f) ||
        !SessionTestFloatMatches(
            legacy_restored.agent.facing_yaw, 0.35f)) {
        return SessionStartupTestFailed(
            session_path, "Legacy session did not return to the authored town.");
    }

    CcLocalBindOpenWorld(NULL);
    (void)remove(session_path);
    (void)puts("Town session startup regression passed");
    return 0;
}
#endif

static void HandleInput(CcJournal **journal, CcSim *sim, int32_t *selected,
                        int32_t *selected_situation, ClientView *view,
                        ClientView *return_view, LocalState *local,
                        RenderTexture2D local_target, Rectangle local_bounds,
                        float delta_time,
                        const char *save_path, const char *session_path,
                        char *message, size_t message_capacity)
{
    if (journal == NULL || *journal == NULL) {
        if (message[0] == '\0') {
            (void)snprintf(message, message_capacity,
                           "The campaign journal is unavailable.");
        }
        return;
    }
    ContextAction pressed_action = PressedContextAction(
        sim, local, *view, *selected, *selected_situation);
    ContextActionKind context_action = pressed_action.kind;
    CommandActionKind command_action = PressedCommandAction(local, *view);
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if (command_action == COMMAND_ACTION_SAVE || ClientKeyPressed(KEY_F5) ||
        queued_save_shortcut || (control && ClientKeyPressed(KEY_S))) {
        if (local->road_choice_active) {
            (void)snprintf(
                message, message_capacity,
                "Choose a road or turn back before saving.");
            return;
        }
        char error[256];
        bool saved = CcJournalCheckpoint(*journal, sim,
                                         error, sizeof(error));
        if (saved) {
            saved = SaveLocalSession(session_path, sim, local,
                                     error, sizeof(error));
        }
#if defined(PLATFORM_WEB)
        if (saved &&
            ClientFlushBrowserSaves(save_path, session_path) == 0) {
            saved = false;
            (void)snprintf(error, sizeof(error),
                           "The browser could not store this campaign.");
        }
#endif
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
    if (*view == VIEW_DUNGEON) {
        if (!sim->dungeon_expedition.active) {
            *view = VIEW_LOCAL;
            return;
        }
        ContextActionSet dungeon_actions = BuildContextActions(
            sim, local, *view, *selected, *selected_situation);
        if (context_action == CONTEXT_ACTION_NONE) {
            for (int32_t i = 0; i < dungeon_actions.count && i < 8; ++i) {
                if (dungeon_actions.items[i].enabled &&
                    ClientKeyPressed(KEY_ONE + i)) {
                    pressed_action = dungeon_actions.items[i];
                    context_action = pressed_action.kind;
                    break;
                }
            }
        }
        if (ClientKeyPressed(KEY_ESCAPE)) {
            context_action = CONTEXT_ACTION_DUNGEON_RETREAT;
        }
        CcCommand dungeon_command = {0};
        if (context_action == CONTEXT_ACTION_DUNGEON_MOVE) {
            dungeon_command.kind = CC_COMMAND_MOVE_DUNGEON;
            dungeon_command.amount = pressed_action.amount;
        } else if (context_action == CONTEXT_ACTION_DUNGEON_SEARCH) {
            dungeon_command.kind = CC_COMMAND_SEARCH_DUNGEON;
        } else if (context_action ==
                   CONTEXT_ACTION_DUNGEON_OPEN_SHORTCUT) {
            dungeon_command.kind = CC_COMMAND_OPEN_DUNGEON_SHORTCUT;
            dungeon_command.amount = pressed_action.amount;
        } else if (context_action == CONTEXT_ACTION_DUNGEON_PARLEY) {
            dungeon_command.kind = CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER;
            dungeon_command.amount = CC_DUNGEON_APPROACH_PARLEY;
        } else if (context_action == CONTEXT_ACTION_DUNGEON_EVADE) {
            dungeon_command.kind = CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER;
            dungeon_command.amount = CC_DUNGEON_APPROACH_EVADE;
        } else if (context_action == CONTEXT_ACTION_DUNGEON_FORCE) {
            dungeon_command.kind = CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER;
            dungeon_command.amount = CC_DUNGEON_APPROACH_FORCE;
        } else if (context_action == CONTEXT_ACTION_DUNGEON_RETREAT) {
            dungeon_command.kind = CC_COMMAND_RETREAT_DUNGEON;
        }
        if (dungeon_command.kind != CC_COMMAND_NONE &&
            ApplyCommand(*journal, sim, dungeon_command,
                         message, message_capacity)) {
            *selected = 0;
            if (!sim->dungeon_expedition.active) *view = VIEW_LOCAL;
        }
        return;
    }
    if (*view == VIEW_DRAGON_CAVE) {
        ContextActionSet cave_actions = BuildContextActions(
            sim, local, *view, *selected, *selected_situation);
        if (context_action == CONTEXT_ACTION_NONE) {
            for (int32_t i = 0; i < cave_actions.count && i < 6; ++i) {
                if (cave_actions.items[i].enabled &&
                    ClientKeyPressed(KEY_ONE + i)) {
                    context_action = cave_actions.items[i].kind;
                    break;
                }
            }
        }
        if (ClientKeyPressed(KEY_ESCAPE) ||
            context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            *view = VIEW_LOCAL;
            return;
        }
        CcCommand cave_command = {0};
        if (context_action == CONTEXT_ACTION_STEAL_DRAGON_CROWNS) {
            cave_command.kind = CC_COMMAND_STEAL_DRAGON_HOARD;
            cave_command.amount = sim->dragon.hoard < 10 ?
                (int32_t)sim->dragon.hoard : 10;
        } else if (context_action ==
                   CONTEXT_ACTION_RETURN_DRAGON_CROWNS) {
            cave_command.kind = CC_COMMAND_RETURN_DRAGON_TREASURE;
            CcMoney payment = sim->player.coins <
                    sim->dragon.stolen_outstanding ? sim->player.coins :
                    sim->dragon.stolen_outstanding;
            cave_command.amount = payment > INT32_MAX ?
                INT32_MAX : (int32_t)payment;
        } else if (context_action == CONTEXT_ACTION_STEAL_DRAGON_RELIC) {
            const CcTreasure *treasure = FirstDragonTreasure(sim);
            cave_command.kind = CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE;
            cave_command.target_id = treasure != NULL ? treasure->id : 0U;
        } else if (context_action == CONTEXT_ACTION_RETURN_DRAGON_RELIC) {
            cave_command.kind = CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE;
            cave_command.target_id = sim->dragon.stolen_treasure_id;
        } else if (context_action ==
                   CONTEXT_ACTION_INTERCEPT_DRAGON_TRIBUTE) {
            cave_command.kind = CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE;
        }
        if (cave_command.kind != CC_COMMAND_NONE) {
            (void)ApplyCommand(*journal, sim, cave_command,
                               message, message_capacity);
        }
        return;
    }
    if (*view == VIEW_CHARACTER) {
        if (ClientKeyPressed(KEY_ESCAPE) ||
            context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            *view = VIEW_LOCAL;
            return;
        }
        const CcSituation *conversation = CcSimSituation(
            sim, local->conversation_situation_id);
        bool decision = conversation != NULL &&
            conversation->kind == CC_SITUATION_MONSTER_EXPEDITION &&
            conversation->discovery_stage == CC_DISCOVERY_DECISION;
        CcCharacterResponse response = decision ?
            (ClientKeyPressed(KEY_ONE) ||
             context_action == CONTEXT_ACTION_REPORT_EVIDENCE ?
                CC_CHARACTER_RESPONSE_REPORT_EVIDENCE :
             ClientKeyPressed(KEY_TWO) ||
             context_action == CONTEXT_ACTION_KEEP_CONFIDENCE ?
                CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE : 0) :
            (ClientKeyPressed(KEY_ONE) ||
             context_action == CONTEXT_ACTION_LISTEN_CHARACTER ?
                CC_CHARACTER_RESPONSE_LISTEN :
             ClientKeyPressed(KEY_TWO) ||
             context_action == CONTEXT_ACTION_PLEDGE_CHARACTER ?
                CC_CHARACTER_RESPONSE_PLEDGE_HELP : 0);
        if (response != 0) {
            CcCommand reply = {
                .kind = CC_COMMAND_CHARACTER_RESPONSE,
                .target_id = local->conversation_situation_id,
                .amount = (int32_t)response
            };
            if (ApplyCommand(*journal, sim, reply,
                             message, message_capacity)) {
                const CcSituation *updated = CcSimSituation(
                    sim, local->conversation_situation_id);
                const CcCharacter *character = CcSimCharacter(
                    sim, local->conversation_character_id);
                if (updated != NULL &&
                    updated->kind == CC_SITUATION_MONSTER_EXPEDITION &&
                    response != CC_CHARACTER_RESPONSE_PLEDGE_HELP) {
                    char next[160] = "Talk to the witness.";
                    SituationNextAction(sim, updated, next, sizeof(next));
                    (void)snprintf(message, message_capacity,
                                   "New objective: %s", next);
                    *view = VIEW_LOCAL;
                } else {
                    int32_t opening_index = OpeningSituationIndex(sim);
                    bool accepted_opening = response ==
                            CC_CHARACTER_RESPONSE_PLEDGE_HELP &&
                        updated != NULL && opening_index >= 0 &&
                        updated->id == sim->situations[opening_index].id &&
                        sim->player.accepted_situation_id == updated->id;
                    if (accepted_opening) {
                        local->opening_step = CC_LOCAL_OPENING_COMPLETE;
                        *selected_situation = opening_index;
                        (void)snprintf(
                            message, message_capacity,
                            "Mara loads %d food boxes and gives you the carriage key.",
                            updated->quantity - updated->progress);
                    } else {
                        (void)snprintf(
                            message, message_capacity,
                            response == CC_CHARACTER_RESPONSE_LISTEN ?
                                "%.28s remembers that you listened." :
                                "%.28s will hold the company to its promise.",
                            character != NULL ? character->name : "They");
                    }
                }
            }
        }
        return;
    }
    if (IsCommandOverlay(*view) &&
        (ClientKeyPressed(KEY_ESCAPE) ||
         context_action == CONTEXT_ACTION_CLOSE_VIEW)) {
        *view = SafeOverlayReturnView(*return_view);
        return;
    }
    if (ClientKeyPressed(KEY_TAB) ||
        command_action == COMMAND_ACTION_LEDGER) {
        ToggleCommandOverlay(VIEW_LEDGER, view, return_view);
        return;
    }
    bool road_local = local->road_choice_active ||
                      local->journey_travel_active ||
                      local->site_travel_active ||
                      local->journey_combat_active ||
                      local->journey_parley_active;
    bool quests_requested = ClientKeyPressed(KEY_Q) ||
                            command_action == COMMAND_ACTION_QUESTS ||
                            context_action == CONTEXT_ACTION_OPEN_PROMISES;
    if (quests_requested && road_local && !local->open_world) {
        (void)snprintf(message, message_capacity,
                       local->road_choice_active ?
                           "Choose this branch or keep moving first." :
                       local->journey_travel_active ||
                           local->site_travel_active ?
                           "Quests are unavailable while travelling." :
                           "Finish the fight first.");
        return;
    }
    if (quests_requested) {
        if (*view != VIEW_SITUATIONS) {
            if (SelectedActiveSituation(sim, *selected_situation) == NULL) {
                *selected_situation = FirstActiveSituationIndex(sim);
            }
        }
        ToggleCommandOverlay(VIEW_SITUATIONS, view, return_view);
        return;
    }
    bool map_requested = ClientKeyPressed(KEY_M) ||
                         command_action == COMMAND_ACTION_MAP ||
                         context_action == CONTEXT_ACTION_OPEN_MAP;
    if (map_requested && road_local && !local->open_world) {
        (void)snprintf(message, message_capacity,
                       local->road_choice_active ?
                           "The visible road notes are beside the junction." :
                       local->journey_travel_active ||
                           local->site_travel_active ?
                           "Map case unavailable while travelling." :
                           "Finish the fight first.");
        return;
    }
    if (map_requested) {
        if (*view == VIEW_MAP) {
            *view = *return_view == VIEW_CARRIAGE ?
                VIEW_CARRIAGE : VIEW_LOCAL;
            *selected = FirstOutgoingRouteIndex(sim);
        } else {
            ClientView map_origin = IsCommandOverlay(*view) ?
                SafeOverlayReturnView(*return_view) : *view;
            Vector2 carriage = local->site_kind == CC_LOCAL_SITE_NONE ?
                LOCAL_CARRIAGE_BAY :
                (Vector2){CC_LOCAL_SITE_CARRIAGE_X,
                          CC_LOCAL_SITE_CARRIAGE_Z};
            if (map_origin == VIEW_MAP) {
                *view = VIEW_MAP;
            } else if (map_origin == VIEW_CARRIAGE) {
                *return_view = VIEW_CARRIAGE;
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_MAP;
            } else if (map_origin == VIEW_LOCAL && !local->market_interior &&
                       (local->open_world ||
                        GridDistance(LocalPosition(local), carriage) < 1.75f)) {
                *return_view = VIEW_LOCAL;
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_MAP;
            } else {
                (void)snprintf(message, message_capacity,
                               "Walk closer to the carriage.");
            }
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
            bool at_notice = CcClientPromiseCanBeAccepted(
                local->market_interior,
                GridDistance(LocalPosition(local), LOCAL_NOTICE));
            if (local->open_world) {
                at_notice = !local->market_interior &&
                            OpenWorldSettlementDistance(sim, local) < 18.0f;
            }
            if (at_notice) {
                CcCommand accept = {
                    .kind = CC_COMMAND_ACCEPT_SITUATION,
                    .target_id = situation->id
                };
                if (ApplyCommand(*journal, sim, accept, message,
                                 message_capacity)) {
                    *view = *return_view;
                    return;
                }
            } else {
                (void)snprintf(message, message_capacity,
                               "Visit the local board to make that promise.");
            }
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
    if (*view == VIEW_CARRIAGE) {
        if (ClientKeyPressed(KEY_ESCAPE) ||
            context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            CcLocalAgentClearWorldTarget(&local->agent);
            *view = VIEW_LOCAL;
            return;
        }
        if (local->site_kind != CC_LOCAL_SITE_NONE &&
            (ClientKeyPressed(KEY_ENTER) ||
             context_action == CONTEXT_ACTION_RETURN_FROM_SITE)) {
            CcLocalSiteKind site = local->site_kind;
            BeginSiteTravelState(local, site, true);
            *view = VIEW_LOCAL;
            (void)snprintf(message, message_capacity,
                           "The carriage turns back toward town.");
            return;
        }
        if (local->site_kind == CC_LOCAL_SITE_NONE &&
            (ClientKeyPressed(KEY_ENTER) ||
             context_action == CONTEXT_ACTION_CHOOSE_ROAD)) {
            *selected = FirstOutgoingRouteIndex(sim);
            if (local->open_world) {
                const CcRoute *route = SelectedOutgoingRoute(
                    sim, *selected);
                if (route != NULL) {
                    SetOpenWorldCarriageAtRoadGate(
                        sim, local, route->id);
                }
                *view = VIEW_ROADS;
                message[0] = '\0';
            } else {
                BeginRoadChoiceApproachState(local, true);
                *view = VIEW_LOCAL;
                (void)snprintf(
                    message, message_capacity,
                    "You take the reins and leave the loading bay.");
            }
            return;
        }
        return;
    }

    if (ClientKeyPressed(KEY_F9)) {
        char error[256];
        if (!CcJournalClose(journal, sim, error, sizeof(error))) {
            (void)snprintf(message, message_capacity, "%s", error);
            return;
        }
        CcSim loaded_sim;
        CcJournal *loaded_journal = CcJournalResume(
            save_path, &loaded_sim, error, sizeof(error));
        bool loaded = loaded_journal != NULL;
        (void)snprintf(message, message_capacity, "%s",
                       loaded ?
                           "Game loaded." :
                           error);
        if (loaded) {
            *journal = loaded_journal;
            *sim = loaded_sim;
            *selected = FirstOutgoingRouteIndex(sim);
            *selected_situation = FirstActiveSituationIndex(sim);
            CcLocalBindPlace(sim);
            LeaveOpenWorld(local);
            ResetLocalState(local);
            (void)InitializeOpenWorld(sim, local, false);
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                BeginRoadLocalState(sim, local, false);
                *view = VIEW_LOCAL;
            } else {
                if (sim->journey.active) {
                    BeginRoadTravelState(sim, local);
                    *view = VIEW_LOCAL;
                } else {
                    if (!RestoreClientStartupSession(
                            session_path, sim, local, view, selected)) {
                        if (OpeningRequired(sim) && !local->open_world) {
                            BeginOpening(local);
                        }
                        *view = VIEW_LOCAL;
                    }
                }
            }
        }
        return;
    }
    if (ClientKeyPressed(KEY_N)) {
        static double confirmation_deadline = 0.0;
        double now = GetTime();
        if (now > confirmation_deadline) {
            confirmation_deadline = now + 3.0;
            (void)snprintf(message, message_capacity,
                           "Press N again to start a new campaign.");
            return;
        }
        confirmation_deadline = 0.0;
        char error[256];
        if (!CcJournalClose(journal, sim, error, sizeof(error))) {
            (void)snprintf(message, message_capacity, "%s", error);
            return;
        }
        CcSim replacement;
        CcSimInit(&replacement,
                  sim->world_seed + UINT32_C(0x9e3779b9));
        CcJournal *replacement_journal = CcJournalRestart(
            save_path, &replacement, error, sizeof(error));
        if (replacement_journal == NULL) {
            *view = VIEW_LOCAL;
            (void)snprintf(message, message_capacity, "%s", error);
            return;
        }
        *journal = replacement_journal;
        *sim = replacement;
        *selected = FirstOutgoingRouteIndex(sim);
        *selected_situation = FirstActiveSituationIndex(sim);
        CcLocalBindPlace(sim);
        LeaveOpenWorld(local);
        ResetLocalState(local);
        (void)InitializeOpenWorld(sim, local, false);
        BeginOpening(local);
        *view = VIEW_LOCAL;
        message[0] = '\0';
        return;
    }
    if (ClientKeyPressed(KEY_PERIOD) && !sim->journey.active) {
        char error[256];
        bool advanced = CcJournalAdvanceDays(*journal, sim, 1,
                                             error, sizeof(error));
        (void)snprintf(message, message_capacity, "%s",
                       advanced ?
                           "One day passed." :
                           error);
    }
    if (ClientKeyPressed(KEY_K) && !sim->journey.active) {
        char error[256];
        bool advanced = CcJournalAdvanceDays(*journal, sim, 7,
                                             error, sizeof(error));
        (void)snprintf(message, message_capacity, "%s",
                       advanced ?
                           "One week passed." :
                           error);
    }

    if (*view == VIEW_LOCAL) {
        if (local->open_world_market && ClientKeyPressed(KEY_ESCAPE)) {
            local->open_world_market = false;
            message[0] = '\0';
            return;
        }
        if (local->site_travel_active) {
            SiteTravelResult result = UpdateSiteTravelState(
                local, delta_time);
            if (result == SITE_TRAVEL_ARRIVED) {
                (void)snprintf(message, message_capacity,
                               "The carriage stops at %s.",
                               CcLocalSiteName(sim, local->site_kind));
            } else if (result == SITE_TRAVEL_RETURNED) {
                (void)snprintf(message, message_capacity,
                               "The carriage returns to town.");
            }
            return;
        }
        if (local->road_choice_active) {
            if (UpdateRoadChoiceApproach(local, delta_time)) {
                const CcRoute *route = SelectedOutgoingRoute(
                    sim, *selected);
                if (route != NULL && EnterRoadBookFromTownGate(
                        sim, local, route->id)) {
                    *view = VIEW_ROADS;
                    message[0] = '\0';
                }
            }
            return;
        }
        if (local->journey_travel_active) {
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
                int32_t pace_direction =
                    ClientKeyPressed(KEY_W) || ClientKeyPressed(KEY_UP) ? 1 :
                    ClientKeyPressed(KEY_S) || ClientKeyPressed(KEY_DOWN) ? -1 :
                    0;
                int32_t next_pace = CcClientStepConvoyPosture(
                    (int32_t)sim->journey.pace, pace_direction);
                if (pace_direction != 0 &&
                    next_pace != (int32_t)sim->journey.pace) {
                    CcCommand pace = {
                        .kind = CC_COMMAND_SET_JOURNEY_PACE,
                        .amount = next_pace
                    };
                    if (ApplyCommand(*journal, sim, pace, message,
                                     message_capacity)) {
                        int32_t eta = CcSimJourneyEtaMinutes(sim);
                        int32_t eta_hours = (eta + 59) / 60;
                        (void)snprintf(
                            message, message_capacity,
                            "%s pace. ETA %dd %dh. %s",
                            CcJourneyPaceName(sim->journey.pace),
                            eta_hours / 24, eta_hours % 24,
                            sim->journey.pace == CC_JOURNEY_PACE_CAREFUL ?
                                "Lower wear; scouts can evade." :
                            sim->journey.pace == CC_JOURNEY_PACE_PUSH ?
                                "Faster, with more fatigue and wear." :
                                "Balanced speed and strain.");
                    }
                    return;
                }
            }
            if (!sim->journey.active && local->open_world &&
                local->arrival.phase == CC_CLIENT_ARRIVAL_TOWN) {
                BeginTownArrivalState(local);
            }
            bool enter_pressed = ClientKeyPressed(KEY_ENTER);
            if (HandleTownArrivalAction(
                    sim, local, selected, context_action, enter_pressed,
                    message, message_capacity)) {
                return;
            }
            if (RoadBookArrivalInProgress(local)) return;
            if (sim->journey.active &&
                (context_action == CONTEXT_ACTION_SKIP_TRAVEL ||
                 enter_pressed)) {
                char error[256];
                bool advanced = true;
                bool warning_reached = false;
                while (advanced && sim->journey.active &&
                       sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
                    bool warned_before = sim->journey.ambush_warned;
                    advanced = CcJournalAdvanceRuntimeTicks(
                        *journal, sim, CC_WORLD_TICKS_PER_SECOND,
                        error, sizeof(error));
                    if (!warned_before && sim->journey.ambush_warned) {
                        warning_reached = true;
                        break;
                    }
                }
                if (!advanced) {
                    (void)snprintf(message, message_capacity, "%s", error);
                    return;
                }
                if (sim->journey.active &&
                    sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                    BeginRoadLocalState(sim, local, false);
                    *view = VIEW_LOCAL;
                    (void)snprintf(message, message_capacity,
                                   "The road is blocked. Walk to the captain or return to the carriage.");
                } else if (sim->journey.active && warning_reached) {
                    const CcEvent *event = CcSimRecentEvent(sim, 0);
                    (void)snprintf(message, message_capacity, "%s",
                                   event != NULL ? event->text :
                                   "Scouts spot riders shadowing the road.");
                } else {
                    *selected = FirstOutgoingRouteIndex(sim);
                    CcLocalBindPlace(sim);
                    BeginRoadBookArrivalState(sim, local);
                    (void)snprintf(message, message_capacity,
                                   "The road book closes on the destination gate.");
                }
                return;
            }
            ConvoyUpdateResult convoy_update = UpdateDrivenConvoy(
                local, sim, delta_time);
            if (convoy_update == CONVOY_UPDATE_PARKED) {
                FinishTownArrivalState(
                    sim, local, selected, message, message_capacity);
                return;
            }
            if (local->convoy.phase != CC_LOCAL_CONVOY_ROAD) return;

            int32_t fixed_steps = CcLocalWorldUpdate(
                &local->course, &local->agent, sim, delta_time,
                false, false);
            float posture_pace = CcClientConvoyPosturePace(
                (int32_t)sim->journey.pace);
            float road_motion = posture_pace > 0.01f ?
                fmaxf(0.0f, fminf(1.0f,
                    local->convoy.pace / posture_pace)) : 0.0f;
            local->convoy.runtime_tick_accumulator +=
                (float)fixed_steps * road_motion;
            int32_t ticks = (int32_t)floorf(
                local->convoy.runtime_tick_accumulator);
            local->convoy.runtime_tick_accumulator -= (float)ticks;
            bool warned_before = sim->journey.ambush_warned;
            bool ambush_resolved_before = sim->journey.ambush_resolved;
            char error[256];
            bool advanced = CcJournalAdvanceRuntimeTicks(
                *journal, sim, ticks, error, sizeof(error));
            if (!advanced) {
                (void)snprintf(message, message_capacity, "%s", error);
                return;
            }
            if (local->open_world && sim->journey.active) {
                PositionOpenWorldJourney(sim, local);
            }
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                BeginRoadLocalState(sim, local, false);
                *view = VIEW_LOCAL;
                const CcEvent *event = CcSimRecentEvent(sim, 0);
                (void)snprintf(message, message_capacity, "%s",
                               event != NULL ? event->text :
                               "An outlaw company blocked the road.");
            } else if (!sim->journey.active) {
                *selected = FirstOutgoingRouteIndex(sim);
                CcLocalBindPlace(sim);
                BeginRoadBookArrivalState(sim, local);
                const CcEvent *event = CcSimRecentEvent(sim, 0);
                (void)snprintf(message, message_capacity, "%s",
                               event != NULL ? event->text :
                               "Drive the team into the destination yard.");
            } else if ((!warned_before && sim->journey.ambush_warned) ||
                       (!ambush_resolved_before &&
                        sim->journey.ambush_resolved)) {
                const CcEvent *event = CcSimRecentEvent(sim, 0);
                (void)snprintf(message, message_capacity, "%s",
                               event != NULL ? event->text :
                               "The road ahead has changed.");
            }
            return;
        }
        UpdateMovementPreview(
            sim, local, *view, *selected, *selected_situation,
            local_target, local_bounds, delta_time);
        if (LocalCombatActive(local) &&
            (context_action == CONTEXT_ACTION_SELECT_TARGET ||
             ClientKeyPressed(KEY_T))) {
            int32_t target = context_action == CONTEXT_ACTION_SELECT_TARGET ?
                pressed_action.amount : NextCombatTargetIndex(local);
            if (target >= 0 && CcLocalCourseSelectPlayerTarget(
                    &local->course, &local->agent, target)) {
                (void)snprintf(
                    message, message_capacity, "Focused: %s / %s.",
                    local->course.raider_names[target],
                    CcLocalRaiderRoleName(local->course.raider_roles[target]));
            } else {
                (void)snprintf(message, message_capacity,
                               "No outlaw is still standing.");
            }
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
            if (SelectedCombatTargetIndex(local) >= 0) {
                bool guarded = !local->agent.humanoid.guard_requested;
                bool changed = CcLocalCourseSetPlayerGuarded(
                    &local->course, &local->agent, guarded);
                if (changed) {
                    (void)snprintf(
                        message, message_capacity, "%s",
                        guarded ?
                            "Guard raised — posture absorbs the next blow." :
                            "Guard lowered — movement and attacks are open.");
                }
            } else if (LocalCombatActive(local)) {
                (void)snprintf(message, message_capacity,
                               "Choose an outlaw before guarding.");
            }
        }
        if ((ClientKeyPressed(KEY_SPACE) ||
             context_action == CONTEXT_ACTION_BASIC_STRIKE) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED &&
            !local->journey_parley_active) {
            int32_t target = SelectedCombatTargetIndex(local);
            if (target >= 0) {
                bool struck = CcLocalCourseBeginPlayerStrike(
                    &local->course, &local->agent);
                (void)snprintf(
                    message, message_capacity, "%s",
                    struck ? TextFormat(
                        "Striking %s.",
                        local->course.raider_names[target]) :
                        "Recovering — wait for an opening.");
            } else if (LocalCombatActive(local)) {
                (void)snprintf(message, message_capacity,
                               "Choose an outlaw before attacking.");
            }
        }
        if (LocalCombatActive(local)) {
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
                bool has_target = SelectedCombatTargetIndex(local) >= 0;
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
                    !has_target ?
                        "Choose an outlaw before using that skill." :
                    combat_skill == CC_COMBAT_SKILL_SECOND_WIND ?
                        "Posture is already full." :
                        "That skill is not available now.");
            }
        }
        if (context_action == CONTEXT_ACTION_NONE &&
            ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            CcLocalWorldTargetKind world_target = LocalCombatActive(local) ?
                CC_LOCAL_WORLD_TARGET_NONE :
                CcLocalAgentPickWorldTarget(
                    &local->agent, mouse, local_target, local_bounds,
                    local->market_interior);
            if (world_target != CC_LOCAL_WORLD_TARGET_NONE) {
                bool carriage_in_reach =
                    world_target == CC_LOCAL_WORLD_TARGET_CARRIAGE &&
                    GridDistance(LocalPosition(local), LOCAL_CARRIAGE_BAY) <
                        1.85f;
                if (carriage_in_reach) {
                    *return_view = VIEW_LOCAL;
                    *view = VIEW_CARRIAGE;
                    message[0] = '\0';
                    return;
                }
                bool approach_started = CcLocalAgentApproachWorldTarget(
                    &local->agent, world_target);
                local->movement_reticle = mouse;
                local->movement_reticle_age = 0.0f;
                local->movement_reticle_valid = !approach_started;
                local->movement_reticle_accepted = approach_started;
                if (approach_started) {
                    CcLocalCourseClearPlayerTarget(&local->agent);
                }
                (void)snprintf(
                    message, message_capacity,
                    approach_started ?
                        "%s targeted. Walk to its bay, then click again or press F." :
                                       "Can't reach %s from here.",
                    CcLocalWorldTargetName(world_target));
            } else {
                int32_t combat_target = local->market_interior ? -1 :
                    CcLocalCoursePickPlayerTarget(
                        &local->course, &local->agent, mouse, local_target,
                        local_bounds);
                if (combat_target >= 0) {
                    CcLocalAgentClearWorldTarget(&local->agent);
                    local->movement_reticle = mouse;
                    local->movement_reticle_age = 0.0f;
                    local->movement_reticle_valid = true;
                    local->movement_reticle_accepted = true;
                    bool struck = CcLocalCourseBeginPlayerStrike(
                        &local->course, &local->agent);
                    (void)snprintf(
                        message, message_capacity,
                        struck ? "Attacking %s / %s." :
                                 "Focused: %s / %s.",
                        local->course.raider_names[combat_target],
                        CcLocalRaiderRoleName(
                            local->course.raider_roles[combat_target]));
                } else {
                    CcLocalMovementPreview click_preview = {0};
                    (void)CcLocalAgentProbeTarget(
                        &local->agent, mouse, local_target, local_bounds,
                        local->market_interior, &click_preview);
                    bool movement_accepted = CcLocalAgentApplyMovementPreview(
                        &local->agent, &click_preview,
                        local->market_interior);
                    local->movement_preview = click_preview;
                    local->movement_preview_cooldown = 0.08f;
                    bool in_local_view = CheckCollisionPointRec(
                        mouse, local_bounds);
                    if (in_local_view) {
                        local->movement_reticle = mouse;
                        local->movement_reticle_age = 0.0f;
                        local->movement_reticle_valid = !movement_accepted;
                        local->movement_reticle_accepted = movement_accepted;
                    }
                    if (movement_accepted) {
                        if (!local->market_interior &&
                            local->course.alarm_active) {
                            CcLocalCourseClearPlayerTarget(&local->agent);
                        }
                        const char *navigation =
                            CcLocalAgentNavigationName(&local->agent);
                        if (click_preview.adjusted) {
                            (void)snprintf(
                                message, message_capacity,
                                "Walking to the nearest clear ground.");
                        } else if (navigation != NULL) {
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
        }
        bool advance_course = !local->market_interior &&
                              local->site_kind == CC_LOCAL_SITE_NONE &&
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
            (void)snprintf(
                message, message_capacity,
                "That road is for the carriage. Return to the yard to drive out.");
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
                    BeginRoadTravelState(sim, local);
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
                    BeginRoadTravelState(sim, local);
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
                    BeginRoadTravelState(sim, local);
                    *view = VIEW_LOCAL;
                }
            }
            return;
        }
        if (!local->market_interior &&
            local->site_kind == CC_LOCAL_SITE_NONE) {
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
            if (local->open_world && !local->market_interior &&
                context_action == CONTEXT_ACTION_CHOOSE_ROAD) {
                *selected = FirstOutgoingRouteIndex(sim);
                const CcRoute *route = SelectedOutgoingRoute(
                    sim, *selected);
                if (route != NULL) {
                    SetOpenWorldCarriageAtRoadGate(
                        sim, local, route->id);
                }
                *view = VIEW_ROADS;
                message[0] = '\0';
                return;
            }
            if (local->open_world && !local->market_interior &&
                context_action == CONTEXT_ACTION_OPEN_PROMISES) {
                *return_view = VIEW_LOCAL;
                if (SelectedActiveSituation(sim, *selected_situation) == NULL) {
                    *selected_situation = FirstActiveSituationIndex(sim);
                }
                *view = VIEW_SITUATIONS;
                return;
            }
            if (local->open_world && !local->market_interior &&
                context_action == CONTEXT_ACTION_ENTER_MARKET) {
                local->open_world_market = true;
                message[0] = '\0';
                return;
            }
            if (local->open_world_market &&
                context_action == CONTEXT_ACTION_LEAVE_MARKET) {
                local->open_world_market = false;
                message[0] = '\0';
                return;
            }
            Vector2 site_carriage = {CC_LOCAL_SITE_CARRIAGE_X,
                                     CC_LOCAL_SITE_CARRIAGE_Z};
            if (interact && local->site_kind != CC_LOCAL_SITE_NONE &&
                local->site_kind != CC_LOCAL_SITE_DRAGON_CAVE &&
                GridDistance(position, site_carriage) < 1.75f) {
                *return_view = VIEW_LOCAL;
                *view = VIEW_CARRIAGE;
                message[0] = '\0';
                return;
            }
            if (local->site_kind == CC_LOCAL_SITE_GOBLIN_CAVE &&
                context_action == CONTEXT_ACTION_TRAVEL_DRAGON_SITE) {
                CcCommand tunnel = {
                    .kind = CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL,
                    .target_id = sim->dragon.lair_settlement_id
                };
                if (ApplyCommand(*journal, sim, tunnel, message,
                                 message_capacity)) {
                    EnterSiteFromGoblinTunnel(
                        local, CC_LOCAL_SITE_DRAGON_CAVE,
                        (Vector2){CC_LOCAL_SITE_CARRIAGE_X + 3.0f,
                                  CC_LOCAL_SITE_CARRIAGE_Z});
                    *return_view = VIEW_LOCAL;
                    *view = VIEW_DRAGON_CAVE;
                    (void)snprintf(
                        message, message_capacity,
                        "After a day in the goblin dark, a chimney opens inside the dragon roost.");
                }
                return;
            }
            if (local->site_kind == CC_LOCAL_SITE_DRAGON_CAVE &&
                context_action == CONTEXT_ACTION_TRAVEL_GOBLIN_SITE) {
                CcCommand tunnel = {
                    .kind = CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL,
                    .target_id = sim->goblins.lair_settlement_id
                };
                if (ApplyCommand(*journal, sim, tunnel, message,
                                 message_capacity)) {
                    EnterSiteFromGoblinTunnel(
                        local, CC_LOCAL_SITE_GOBLIN_CAVE,
                        (Vector2){CC_LOCAL_SITE_ENTRANCE_X - 3.0f,
                                  CC_LOCAL_SITE_ENTRANCE_Z});
                    (void)snprintf(
                        message, message_capacity,
                        "You descend through the dungeon to the hidden goblin trail.");
                }
                return;
            }
            if (local->site_kind == CC_LOCAL_SITE_NONE &&
                DungeonAtSettlement(sim, sim->player.location_id) != NULL &&
                (context_action == CONTEXT_ACTION_TRAVEL_DUNGEON_SITE ||
                 (interact && GridDistance(position, LOCAL_DUNGEON) <
                                  1.35f))) {
                BeginSiteTravelState(local, CC_LOCAL_SITE_DUNGEON, false);
                (void)snprintf(message, message_capacity,
                               "The carriage takes the mine road.");
                return;
            }
            if (local->site_kind == CC_LOCAL_SITE_NONE &&
                context_action == CONTEXT_ACTION_TRAVEL_GOBLIN_SITE) {
                BeginSiteTravelState(local, CC_LOCAL_SITE_GOBLIN_CAVE,
                                     false);
                (void)snprintf(message, message_capacity,
                               "The carriage stops where the goblin trail leaves the road.");
                return;
            }
            if (local->site_kind == CC_LOCAL_SITE_NONE && interact &&
                GridDistance(position, LOCAL_DRAGON_CAVE) < 1.35f) {
                CcLocalSiteKind site = sim->player.location_id ==
                        sim->goblins.lair_settlement_id ?
                    CC_LOCAL_SITE_GOBLIN_CAVE : CC_LOCAL_SITE_NONE;
                if (site != CC_LOCAL_SITE_NONE) {
                    BeginSiteTravelState(local, site, false);
                    (void)snprintf(message, message_capacity,
                                   "The carriage stops at the hidden trailhead.");
                    return;
                }
            }
            if (!local->market_interior &&
                local->site_kind == CC_LOCAL_SITE_NONE &&
                !local->course.alarm_active &&
                local->course.situation_witness_active &&
                CcClientInteractionActivated(
                    interact ||
                        context_action == CONTEXT_ACTION_TALK_CHARACTER,
                    GridDistance(
                        position,
                        (Vector2){
                            local->course.situation_witness.position.x,
                            local->course.situation_witness.position.z}),
                    1.85f)) {
                local->conversation_character_id =
                    local->course.situation_witness_character_id;
                local->conversation_situation_id =
                    local->course.situation_witness_id;
                Vector3 toward_witness = {
                    local->course.situation_witness.position.x -
                        local->agent.position.x,
                    0.0f,
                    local->course.situation_witness.position.z -
                        local->agent.position.z,
                };
                local->agent.facing_yaw = atan2f(
                    toward_witness.x, toward_witness.z);
                local->course.situation_witness.facing_yaw = atan2f(
                    -toward_witness.x, -toward_witness.z);
                *view = VIEW_CHARACTER;
                message[0] = '\0';
                return;
            } else if (local->market_interior &&
                CcClientInteractionActivated(
                    interact ||
                        context_action == CONTEXT_ACTION_LEAVE_MARKET,
                    GridDistance(position, INTERIOR_EXIT), 1.25f)) {
                local->market_interior = false;
                if (local->open_world) {
                    BindOpenWorldForLocalState(local);
                    PositionOpenWorldAtSettlement(sim, local);
                } else {
                    RepositionHero(local,
                                   (Vector2){CC_LOCAL_MARKET_X,
                                             CC_LOCAL_MARKET_Z + 1.10f},
                                   false);
                }
                message[0] = '\0';
            } else if (!local->market_interior &&
                       local->site_kind == CC_LOCAL_SITE_NONE &&
                       CcClientInteractionActivated(
                           interact ||
                               context_action == CONTEXT_ACTION_ENTER_MARKET,
                           GridDistance(position, LOCAL_MARKET),
                           1.30f)) {
                local->market_interior = true;
                RepositionHero(local, (Vector2){2.05f, 5.35f}, true);
                message[0] = '\0';
            } else if (!local->market_interior &&
                       local->site_kind == CC_LOCAL_SITE_NONE &&
                       CcClientInteractionActivated(
                           interact,
                           GridDistance(position, LOCAL_CARRIAGE_BAY),
                           1.85f)) {
                *return_view = VIEW_LOCAL;
                *view = VIEW_CARRIAGE;
                message[0] = '\0';
            } else if (!local->market_interior &&
                       CcClientInteractionActivated(
                           context_action == CONTEXT_ACTION_OPEN_MAP,
                           GridDistance(
                               position,
                               local->site_kind == CC_LOCAL_SITE_NONE ?
                                   LOCAL_CARRIAGE :
                                   (Vector2){CC_LOCAL_SITE_CARRIAGE_X,
                                             CC_LOCAL_SITE_CARRIAGE_Z}),
                           1.75f)) {
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_MAP;
                message[0] = '\0';
            } else if (!local->market_interior &&
                       CcClientInteractionActivated(
                           interact ||
                               context_action == CONTEXT_ACTION_OPEN_PROMISES,
                           GridDistance(position, LOCAL_NOTICE),
                           1.15f)) {
                *return_view = VIEW_LOCAL;
                if (SelectedActiveSituation(sim, *selected_situation) == NULL) {
                    *selected_situation = FirstActiveSituationIndex(sim);
                }
                *view = VIEW_SITUATIONS;
            }
        }

        bool can_trade = local->open_world_market ||
            (local->market_interior &&
             GridDistance(position, INTERIOR_COUNTER) < 2.25f);
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
            } else if (context_action == CONTEXT_ACTION_BUY_CARGO) {
                CcCommand trade = {
                    .kind = CC_COMMAND_TRADE,
                    .good = pressed_action.good,
                    .amount = pressed_action.amount
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
        if (local->site_kind == CC_LOCAL_SITE_GOBLIN_CAVE &&
            context_action == CONTEXT_ACTION_GOBLIN_TRADE) {
            CcCommand trade = {
                .kind = CC_COMMAND_GOBLIN_TRADE,
                .good = pressed_action.good,
                .amount = pressed_action.amount
            };
            (void)ApplyCommand(*journal, sim, trade, message,
                               message_capacity);
            return;
        }
        const CcDungeon *dungeon = DungeonAtSettlement(sim, sim->player.location_id);
        if (!local->market_interior && dungeon != NULL &&
            local->site_kind == CC_LOCAL_SITE_DUNGEON &&
            GridDistance(position,
                         (Vector2){CC_LOCAL_SITE_ENTRANCE_X,
                                   CC_LOCAL_SITE_ENTRANCE_Z}) < 2.25f) {
            CcDungeonState outcome = CC_DUNGEON_DISTURBED;
            if (context_action == CONTEXT_ACTION_DUNGEON_PUBLIC_ROUTE) {
                outcome = CC_DUNGEON_PUBLIC_ROUTE;
            } else if (context_action ==
                       CONTEXT_ACTION_DUNGEON_SMUGGLER_ROUTE) {
                outcome = CC_DUNGEON_SMUGGLER_ROUTE;
            } else if (context_action == CONTEXT_ACTION_DUNGEON_RESEAL) {
                outcome = CC_DUNGEON_RESEALED;
            }
            if (outcome != CC_DUNGEON_DISTURBED) {
                CcCommand decide = {
                    .kind = CC_COMMAND_CHANGE_DUNGEON,
                    .target_id = dungeon->id,
                    .dungeon_state = outcome
                };
                (void)ApplyCommand(*journal, sim, decide, message,
                                   message_capacity);
                return;
            }
            if (ClientKeyPressed(KEY_E) ||
                context_action == CONTEXT_ACTION_EXPEDITION) {
                if (HandleExpedition(*journal, sim, dungeon, message,
                                     message_capacity)) {
                    *selected = 0;
                    *view = VIEW_DUNGEON;
                }
            }
        }
        return;
    }

    if (*view == VIEW_MAP) {
        if (ClientKeyPressed(KEY_ESCAPE) ||
            context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            *view = *return_view == VIEW_CARRIAGE ?
                VIEW_CARRIAGE : VIEW_LOCAL;
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

    if (*view == VIEW_ROADS && RoadBookDepartureInProgress(local)) return;

    if (ClientKeyPressed(KEY_ESCAPE) ||
        context_action == CONTEXT_ACTION_CLOSE_VIEW) {
        BeginTownArrivalState(local);
        *view = VIEW_LOCAL;
        (void)snprintf(message, message_capacity,
                       "The carriage turns back into town.");
        return;
    }
    bool next_branch = ClientKeyPressed(KEY_RIGHT) ||
        ClientKeyPressed(KEY_DOWN) ||
        context_action == CONTEXT_ACTION_NEXT_BRANCH;
    bool previous_branch = ClientKeyPressed(KEY_LEFT) ||
                           ClientKeyPressed(KEY_UP);
    if ((next_branch || previous_branch) &&
        OutgoingRouteCount(sim) > 1) {
        *selected = StepOutgoingRouteIndex(
            sim, *selected, previous_branch ? -1 : 1);
        if (local->open_world) {
            const CcRoute *facing = SelectedOutgoingRoute(
                sim, *selected);
            if (facing != NULL) {
                FaceOpenWorldRoadChoice(sim, local, facing->id);
            }
        }
        local->fork_turn_progress = 0.0f;
        (void)snprintf(message, message_capacity,
                       "The team faces another visible branch.");
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
            BeginRoadTravelState(sim, local);
            *view = VIEW_LOCAL;
            (void)snprintf(message, message_capacity,
                           "The carriage turns onto the chosen road.");
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

static Rectangle LocalViewportBounds(void)
{
    const float art_width = 630.0f;
    const float art_height = 320.0f;
    const float side_margin = 10.0f;
    const float top_margin = 54.0f;
    const float bottom_margin = 66.0f;
    float available_width = (float)GetScreenWidth() - side_margin * 2.0f;
    float available_height =
        (float)GetScreenHeight() - top_margin - bottom_margin;
    float available_scale = fminf(available_width / art_width,
                                  available_height / art_height);
    float scale = floorf(available_scale);

    if (scale < 2.0f) scale = available_scale;
    if (scale < 0.50f) scale = 0.50f;
    float width = art_width * scale;
    float height = art_height * scale;
    return (Rectangle){((float)GetScreenWidth() - width) * 0.5f,
                       top_margin + (available_height - height) * 0.5f,
                       width, height};
}

int main(int argc, char **argv)
{
#if defined(CC_CLIENT_SELF_TESTS)
    if (argc == 2 &&
        strcmp(argv[1], "--test-town-arrival-parking") == 0) {
        return RunTownArrivalParkingRegression();
    }
    if (argc == 2 &&
        strcmp(argv[1], "--test-town-departure") == 0) {
        return RunTownDepartureRegression();
    }
    if (argc == 2 &&
        strcmp(argv[1], "--test-roadbook-arrival") == 0) {
        return RunRoadBookArrivalRegression();
    }
    if (argc == 2 &&
        strcmp(argv[1], "--test-world-session-startup") == 0) {
        return RunWorldSessionStartupRegression();
    }
    if (argc == 2 &&
        strcmp(argv[1], "--test-town-session-startup") == 0) {
        return RunTownSessionStartupRegression();
    }
#endif
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
    double render_benchmark_p95_budget = 0.0;
    const char *render_benchmark_scene = "street";
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
    if (render_benchmark && argc >= 5 && argv[4][0] != '-') {
        render_benchmark_scene = argv[4];
        if (strcmp(render_benchmark_scene, "street") != 0 &&
            strcmp(render_benchmark_scene, "market") != 0 &&
            strcmp(render_benchmark_scene, "road") != 0 &&
            strcmp(render_benchmark_scene, "roadbook-route") != 0 &&
            strcmp(render_benchmark_scene, "roadbook-network") != 0 &&
            strcmp(render_benchmark_scene, "combat") != 0) {
            (void)fprintf(stderr,
                          "Render benchmark scene must be street, market, road, roadbook-route, roadbook-network, or combat.\n");
            return 1;
        }
    }
    if (render_benchmark && argc >= 6 && argv[5][0] != '-') {
        char *end = NULL;
        double parsed = strtod(argv[5], &end);
        if (end == argv[5] || *end != '\0' || !isfinite(parsed) ||
            parsed <= 0.0) {
            (void)fprintf(stderr,
                          "Render benchmark p95 budget is invalid.\n");
            return 1;
        }
        render_benchmark_p95_budget = parsed;
    }
    bool render_benchmark_roadbook_route = render_benchmark &&
        strcmp(render_benchmark_scene, "roadbook-route") == 0;
    bool render_benchmark_roadbook_network = render_benchmark &&
        strcmp(render_benchmark_scene, "roadbook-network") == 0;
    bool render_benchmark_roadbook = render_benchmark_roadbook_route ||
        render_benchmark_roadbook_network;
    bool capture_board = argc >= 2 && strcmp(argv[1], "--capture-board") == 0;
    bool capture_world = argc >= 2 && strcmp(argv[1], "--capture-world") == 0;
    bool capture_opening = argc >= 2 &&
        strcmp(argv[1], "--capture-opening") == 0;
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
    bool capture_road_departure = argc >= 2 &&
        strcmp(argv[1], "--capture-road-departure") == 0;
    bool capture_road_arrival = argc >= 2 &&
        strcmp(argv[1], "--capture-road-arrival") == 0;
    bool capture_road_zoom = capture_road_departure || capture_road_arrival;
    float capture_road_zoom_weight = 0.5f;
    if (capture_road_zoom) {
        if (argc < 4) {
            (void)fprintf(
                stderr,
                "capture road departure and arrival require a weight from 0 to 1 and a frame path.\n");
            return 1;
        }
        char *end = NULL;
        capture_road_zoom_weight = strtof(argv[2], &end);
        if (end == argv[2] || *end != '\0' ||
            capture_road_zoom_weight < 0.0f ||
            capture_road_zoom_weight > 1.0f) {
            (void)fprintf(stderr,
                          "capture road zoom weight must be from 0 to 1.\n");
            return 1;
        }
    }
    bool capture_map_case = argc >= 2 &&
        strcmp(argv[1], "--capture-map-case") == 0;
    bool capture_dragon_hoard_map = argc >= 2 &&
        strcmp(argv[1], "--capture-dragon-hoard-map") == 0;
    bool capture_carriage = argc >= 2 &&
        strcmp(argv[1], "--capture-carriage") == 0;
    bool capture_carriage_target = argc >= 2 &&
        strcmp(argv[1], "--capture-carriage-target") == 0;
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
    bool capture_character = argc >= 2 &&
        strcmp(argv[1], "--capture-character") == 0;
    bool capture_travel = argc >= 2 &&
        strcmp(argv[1], "--capture-travel") == 0;
    bool capture_route_sight = argc >= 2 &&
        strcmp(argv[1], "--capture-route-sight") == 0;
    float capture_route_sight_progress = 0.0f;
    if (capture_route_sight) {
        if (argc < 4) {
            (void)fprintf(
                stderr,
                "capture route sight requires progress from 0 up to 1 and a frame path.\n");
            return 1;
        }
        char *end = NULL;
        capture_route_sight_progress = strtof(argv[2], &end);
        if (end == argv[2] || *end != '\0' ||
            capture_route_sight_progress < 0.0f ||
            capture_route_sight_progress >= 1.0f) {
            (void)fprintf(stderr,
                          "capture route sight progress must be from 0 up to 1.\n");
            return 1;
        }
    }
    bool capture_road = argc >= 2 &&
        strcmp(argv[1], "--capture-road") == 0;
    bool capture_parley = argc >= 2 &&
        strcmp(argv[1], "--capture-parley") == 0;
    bool capture_aftermath = argc >= 2 &&
        strcmp(argv[1], "--capture-aftermath") == 0;
    bool capture_golden = argc >= 2 &&
        strcmp(argv[1], "--capture-golden") == 0;
    bool capture_town = argc >= 2 &&
        strcmp(argv[1], "--capture-town") == 0;
    bool capture_town_arrival = argc >= 2 &&
        strcmp(argv[1], "--capture-town-arrival") == 0;
    int32_t capture_town_index = 0;
    float capture_town_x = 44.25f;
    float capture_town_z = 28.85f;
    if (capture_town) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "capture town requires an index from 0 to 5 and a frame path.\n");
            return 1;
        }
        char *end = NULL;
        long parsed = strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0' || parsed < 0 || parsed >= 6) {
            (void)fprintf(stderr, "capture town index must be from 0 to 5.\n");
            return 1;
        }
        capture_town_index = (int32_t)parsed;
        if (argc >= 6) {
            char *x_end = NULL;
            char *z_end = NULL;
            capture_town_x = strtof(argv[3], &x_end);
            capture_town_z = strtof(argv[4], &z_end);
            if (x_end == argv[3] || *x_end != '\0' ||
                z_end == argv[4] || *z_end != '\0' ||
                capture_town_x < 0.5f ||
                capture_town_x > CC_LOCAL_WORLD_WIDTH - 0.5f ||
                capture_town_z < 0.5f ||
                capture_town_z > CC_LOCAL_WORLD_DEPTH - 0.5f) {
                (void)fprintf(stderr,
                              "capture town coordinates are invalid.\n");
                return 1;
            }
        }
    }
    float capture_town_arrival_progress = 0.58f;
    if (capture_town_arrival) {
        if (argc < 5) {
            (void)fprintf(
                stderr,
                "capture town arrival requires an index from 0 to 5, progress from 0 to 1, and a frame path.\n");
            return 1;
        }
        char *index_end = NULL;
        char *progress_end = NULL;
        long parsed_index = strtol(argv[2], &index_end, 10);
        float parsed_progress = strtof(argv[3], &progress_end);
        if (index_end == argv[2] || *index_end != '\0' ||
            parsed_index < 0 || parsed_index >= 6 ||
            progress_end == argv[3] || *progress_end != '\0' ||
            parsed_progress < 0.0f || parsed_progress > 1.0f) {
            (void)fprintf(stderr,
                          "capture town arrival values are invalid.\n");
            return 1;
        }
        capture_town_index = (int32_t)parsed_index;
        capture_town_arrival_progress = parsed_progress;
    }
    bool capture_dragon_cave = argc >= 2 &&
        strcmp(argv[1], "--capture-dragon-cave") == 0;
    bool capture_underroad = argc >= 2 &&
        strcmp(argv[1], "--capture-underroad") == 0;
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
    bool capture_npc_review = argc >= 2 &&
        strcmp(argv[1], "--capture-npc-review") == 0;
    int32_t capture_npc_review_view = -1;
    if (capture_npc_review) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "NPC review requires front, side, motion, or state "
                          "and a frame path.\n");
            return 1;
        }
        if (strcmp(argv[2], "front") == 0) capture_npc_review_view = 0;
        else if (strcmp(argv[2], "side") == 0) capture_npc_review_view = 1;
        else if (strcmp(argv[2], "motion") == 0) capture_npc_review_view = 2;
        else if (strcmp(argv[2], "state") == 0) capture_npc_review_view = 3;
        else {
            (void)fprintf(stderr,
                          "NPC review must be front, side, motion, or state.\n");
            return 1;
        }
    }
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
             strcmp(argv[2], "dragon-whelp") != 0 &&
             strcmp(argv[2], "dragon-wanderer") != 0 &&
             strcmp(argv[2], "dragon-deep-wyrm") != 0 &&
             strcmp(argv[2], "animals") != 0 &&
             strcmp(argv[2], "horse") != 0 &&
             strcmp(argv[2], "cow") != 0 &&
             strcmp(argv[2], "sheep") != 0)) {
            (void)fprintf(stderr,
                          "creature capture requires goblins, a dragon stage, "
                          "horse, cow, sheep, or animals and a frame path.\n");
            return 1;
        }
        capture_creature_family = argv[2];
    }
    bool capture_creature_horse = capture_creature_media &&
        strcmp(capture_creature_family, "horse") == 0;
    bool capture_creature_dragon = capture_creature_media &&
        strncmp(capture_creature_family, "dragon", 6) == 0;
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
                   (strcmp(argv[1], "--capture") == 0 || capture_world ||
                    capture_board ||
                    capture_opening ||
                    capture_interior || capture_navigation || capture_limbs ||
                    capture_walk_cycle || capture_defense ||
                    capture_downclimb || capture_road_fork ||
                    capture_road_zoom ||
                    capture_map_case || capture_dragon_hoard_map ||
                    capture_carriage_target || capture_dojo ||
                    capture_carriage ||
                    capture_jump || capture_action_reel ||
                    capture_gameplay_reel || capture_encounter ||
                    capture_witness || capture_character ||
                    capture_travel || capture_route_sight || capture_road ||
                    capture_parley ||
                    capture_aftermath || capture_golden || capture_town ||
                    capture_town_arrival ||
                    capture_dragon_cave || capture_underroad ||
                    capture_atmosphere || capture_face ||
                    capture_room || capture_npc_review ||
                    capture_creature_media);
    const char *capture_path = capture_creature_media ? argv[3] :
                               capture_room ? argv[4] :
                               capture_face ? argv[3] :
                               capture_atmosphere ? argv[3] :
                               capture_town_arrival ? argv[4] :
                               capture_town ? (argc >= 6 ? argv[5] : argv[3]) :
                               capture_npc_review ? argv[3] :
                               capture_route_sight ? argv[3] :
                               capture_road_zoom ? argv[3] :
                               argc >= 3 ? argv[2] :
                               "architecture-proof.png";
    char save_path[640];
    CampaignSavePath(save_path, sizeof(save_path));
    char session_path[704];
    char lock_path[704];
    if (!CampaignCompanionPath(save_path, ".session", session_path,
                               sizeof(session_path)) ||
        !CampaignCompanionPath(save_path, ".lock", lock_path,
                               sizeof(lock_path))) {
        (void)fprintf(stderr, "Campaign companion path is too long.\n");
        return 1;
    }
    bool normal_play = !capture && !render_benchmark;
    CcClientInstanceLock instance_lock = {.descriptor = -1};
    if (normal_play) {
        char lock_error[192];
        if (!CcClientInstanceLockAcquire(lock_path, &instance_lock,
                                         lock_error, sizeof(lock_error))) {
            (void)fprintf(stderr, "%s\n", lock_error);
            return 2;
        }
    }

    if (render_benchmark) SetTraceLogLevel(LOG_ERROR);
    else if (capture) SetTraceLogLevel(LOG_WARNING);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE |
                   (capture ? FLAG_WINDOW_HIDDEN : 0U));
    int32_t initial_width = normal_play ? 1200 : 1280;
    int32_t initial_height = normal_play ? 700 : 760;
    InitWindow(initial_width, initial_height,
               "Crownless Carriage — living world spine");
    if (!IsWindowReady()) {
        (void)fprintf(stderr,
                      "Crownless Carriage could not connect to the desktop window server.\n");
        CcClientInstanceLockRelease(&instance_lock);
        return 1;
    }
    ClientInputInstall();

    SetWindowMinSize(normal_play ? 1040 : 1280,
                     normal_play ? 620 : 760);
    SetTargetFPS(render_benchmark || capture_action_reel ||
                 capture_gameplay_reel || capture_creature_reel ? 0 : 60);
    ClientMapTextures map_textures = {0};
    /* The playable world is authored against a fixed 2x art-pixel grid.
       Render it at half the presentation size, then enlarge with point
       sampling. Screen-space labels and HUD are drawn after presentation. */
    RenderTexture2D local_target = LoadRenderTexture(630, 320);
    SetTextureFilter(local_target.texture, TEXTURE_FILTER_POINT);
    CcLocalRendererSetScreenFirstHero(screen_first_hero);
    CcLocalRendererInit();
#if defined(PLATFORM_WEB)
    int32_t released_asset_bytes = ClientReleaseBrowserAssets();
    if (released_asset_bytes >= 0) {
        TraceLog(LOG_INFO, "WEB: released %.1f MiB of startup files",
                 (double)released_asset_bytes / (1024.0 * 1024.0));
    }
#endif
    CcLocalRendererSetDiagnosticOverlay(capture_limbs);

    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcJournal *journal = NULL;
    char startup_message[256] = "";
    bool resuming_campaign = normal_play && CampaignSaveExists(save_path);
    if (capture || render_benchmark) {
        CcSimAdvanceDays(&sim, 28);
    } else {
        char error[256];
        if (CampaignSaveExists(save_path)) {
            journal = CcJournalResume(save_path, &sim, error, sizeof(error));
            (void)snprintf(startup_message, sizeof(startup_message), "%s",
                           journal != NULL ? "Campaign resumed." : error);
        } else {
            journal = CcJournalStart(save_path, &sim, error, sizeof(error));
            (void)snprintf(startup_message, sizeof(startup_message), "%s",
                           journal != NULL ? "New campaign started." : error);
        }
    }
    CcLocalTerrainSetSeed(sim.world_seed);
    if (capture_creature_media &&
        strcmp(capture_creature_family, "goblins") == 0) {
        sim.player.location_id = sim.goblins.lair_settlement_id;
    } else if (capture_creature_dragon) {
        sim.player.location_id = sim.dragon.lair_settlement_id;
        sim.dragon.omen_days_remaining = 2;
        sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
        if (strcmp(capture_creature_family, "dragon-whelp") == 0) {
            sim.dragon.life_stage = CC_DRAGON_STAGE_WHELP;
        } else if (strcmp(capture_creature_family,
                          "dragon-wanderer") == 0) {
            sim.dragon.life_stage = CC_DRAGON_STAGE_WANDERER;
        } else if (strcmp(capture_creature_family,
                          "dragon-deep-wyrm") == 0) {
            sim.dragon.life_stage = CC_DRAGON_STAGE_DEEP_WYRM;
        }
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
    if (capture_dragon_cave) {
        sim.player.location_id = sim.dragon.lair_settlement_id;
        sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
        sim.goblins.tribute_target_id = sim.dragon.lair_settlement_id;
        sim.goblins.tribute_days_remaining = 2;
    }
    if (capture_road_fork || capture_map_case || capture_dragon_hoard_map) {
        sim.player.location_id = sim.settlements[1].id;
    }
    if (capture_town || capture_town_arrival) {
        sim.player.location_id = sim.settlements[capture_town_index].id;
    }
    if (capture_witness || capture_character) {
        for (int32_t situation = 0; situation < sim.situation_count;
             ++situation) {
            if (sim.situations[situation].status != CC_SITUATION_ACTIVE) {
                continue;
            }
            const CcCharacter *character = CcSimSituationAffectedCharacter(
                &sim, &sim.situations[situation]);
            if (character == NULL) continue;
            sim.player.location_id = character->current_settlement_id;
            break;
        }
    }
    CcLocalBindPlace(&sim);
    if (capture_encounter || capture_travel || capture_route_sight ||
        capture_road || capture_parley ||
        capture_aftermath || capture_creature_horse || capture_road_arrival ||
        render_benchmark_roadbook) {
        int32_t charter_index = FirstActiveSituationIndex(&sim);
        if (charter_index >= 0) {
            char setup_error[192];
            CcSituation *capture_charter = &sim.situations[charter_index];
            capture_charter->kind = CC_SITUATION_RELIEF_DELIVERY;
            capture_charter->target_id = sim.settlements[1].id;
            capture_charter->good = CC_GOOD_FOOD;
            capture_charter->quantity = 1;
            capture_charter->progress = 0;
            sim.player.cargo[CC_GOOD_FOOD] = 0;
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
            if (capture_route_sight) {
                const CcRoute *capture_route = CcSimRouteBetween(
                    &sim, sim.player.location_id, sim.settlements[1].id);
                if (capture_route != NULL) {
                    for (int32_t map_index = 0;
                         map_index < sim.map_count &&
                         map_index < CC_MAX_MAPS;
                         ++map_index) {
                        CcMap *map = &sim.maps[map_index];
                        if (map->route_id != capture_route->id) continue;
                        if (map->owner_id == sim.player.id) {
                            map->owner_id = sim.player.location_id;
                        }
                        uint32_t bit = UINT32_C(1) << (uint32_t)map_index;
                        sim.player.map_catalogue_mask &= ~bit;
                        sim.player.map_archive_mask &= ~bit;
                    }
                    CcSimInitializePlayerRouteKnowledge(&sim);
                }
            }
            CcCommand travel = {
                .kind = CC_COMMAND_TRAVEL,
                .target_id = sim.settlements[1].id
            };
            (void)CcSimApply(&sim, &travel, setup_error,
                             sizeof(setup_error));
            if ((render_benchmark_roadbook || capture_road_arrival ||
                 capture_route_sight) &&
                sim.journey.active) {
                sim.journey.situation_id = 0U;
                sim.journey.encounter_subticks = 0;
                sim.journey.ambush_pending = false;
                sim.journey.encounter_triggered = true;
            }
            if (capture_travel || capture_route_sight ||
                capture_creature_horse ||
                capture_road_arrival ||
                render_benchmark_roadbook) {
                int32_t target_progress = capture_road_arrival ? 850 :
                    render_benchmark_roadbook ? 420 :
                    capture_route_sight ? (int32_t)(
                        capture_route_sight_progress * 1000.0f + 0.5f) :
                    200;
                int32_t setup_ticks = 0;
                while (sim.journey.active &&
                       sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING &&
                       sim.carriage.progress_milli < target_progress &&
                       setup_ticks < 10000) {
                    CcSimAdvanceRuntimeTicks(
                        &sim, CC_WORLD_TICKS_PER_SECOND);
                    setup_ticks += 1;
                }
                if (capture_road_arrival) {
                    while (sim.journey.active &&
                           sim.journey.phase ==
                               CC_JOURNEY_PHASE_TRAVELLING &&
                           setup_ticks < 20000) {
                        CcSimAdvanceRuntimeTicks(
                            &sim, CC_WORLD_TICKS_PER_SECOND);
                        setup_ticks += 1;
                    }
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
    if (render_benchmark_roadbook_network) {
        uint32_t catalogue_mask = 0U;
        for (int32_t map_index = 0;
             map_index < sim.map_count && map_index < CC_MAX_MAPS &&
             map_index < 32; ++map_index) {
            sim.maps[map_index].owner_id = sim.player.id;
            catalogue_mask |= UINT32_C(1) << (uint32_t)map_index;
        }
        sim.player.map_catalogue_mask = catalogue_mask;
        sim.player.map_archive_mask = 0U;
    }
    if (capture_underroad && sim.dungeon_count > 0) {
        sim.player.location_id = sim.dungeons[0].settlement_id;
        sim.carriage.location_id = sim.player.location_id;
        sim.player.cargo[CC_GOOD_FOOD] = 6;
        sim.player.cargo[CC_GOOD_TOOLS] = 2;
        sim.player.cargo[CC_GOOD_WEAPONS] = 1;
        CcCommand enter = {
            .kind = CC_COMMAND_BEGIN_DUNGEON_EXPEDITION,
            .target_id = sim.dungeons[0].id
        };
        char setup_error[192];
        (void)CcSimApply(&sim, &enter, setup_error, sizeof(setup_error));
        for (int32_t room = 0; room <= 11; ++room) {
            sim.dungeons[0].rooms[room].state_flags |=
                CC_DUNGEON_ROOM_DISCOVERED;
        }
        sim.dungeons[0].links[29].flags |= CC_DUNGEON_LINK_DISCOVERED;
        sim.dungeon_expedition.current_room = 9;
        sim.dungeon_expedition.turns_elapsed = 8;
        sim.dungeon_expedition.days_elapsed = 1;
        sim.dungeon_expedition.light_remaining = 10;
        sim.dungeon_expedition.noise = 3;
        sim.dungeon_expedition.strain = 28;
        sim.dungeon_expedition.maximum_depth = 2;
        sim.dungeon_expedition.encounter_kind =
            CC_DUNGEON_ENCOUNTER_TITHE_KEEPERS;
        sim.dungeon_expedition.encounter_reaction = 6;
        sim.dungeon_expedition.encounter_room = 9;
    }
    int32_t selected = capture_dragon_hoard_map ? CC_MAP_DRAGON_HOARD :
        capture_map_case ? CC_MAP_GLOAMGATE_NIGHT_ROAD :
        FirstOutgoingRouteIndex(&sim);
    int32_t selected_situation = FirstActiveSituationIndex(&sim);
    ClientView view = capture_board ? VIEW_SITUATIONS :
                      capture_character ? VIEW_CHARACTER :
                      capture_encounter ? VIEW_ENCOUNTER :
                      capture_underroad ? VIEW_DUNGEON :
                      capture_dragon_cave ? VIEW_DRAGON_CAVE :
                      capture_road_fork ? VIEW_ROADS :
                      (capture_map_case || capture_dragon_hoard_map) ?
                          VIEW_MAP :
                      capture_carriage ? VIEW_CARRIAGE : VIEW_LOCAL;
    ClientView return_view = VIEW_LOCAL;
    LocalState local = {0};
    CcLocalAgent walk_cycle_frames[8] = {0};
    uint32_t walk_cycle_mask = 0;
    ActionReelState action_reel = {0};
    GameplayReelState gameplay_reel = {0};
    ResetLocalState(&local);
    bool roadbook_world_requested = capture_world || capture_travel ||
        capture_route_sight || capture_road_fork || capture_road_zoom ||
        render_benchmark_roadbook;
    if ((normal_play || roadbook_world_requested) &&
        !InitializeOpenWorld(&sim, &local, false)) {
        (void)snprintf(startup_message, sizeof(startup_message),
                       "Could not generate the finite world.");
    }
    if (capture_world) {
        const CcRoute *route = OpenWorldRouteFromSettlement(
            &sim, sim.player.location_id);
        if (route != NULL) {
            (void)EnterOpenWorldAtRoadGate(&sim, &local, route->id);
            local.world_carriage.camera_weight = 1.0f;
        }
    }
    if (normal_play && !resuming_campaign && journal != NULL) {
        view = VIEW_LOCAL;
    }
    if (capture_opening) {
        BeginOpening(&local);
        view = VIEW_LOCAL;
    }
    if (capture_carriage) {
        RepositionHero(&local, LOCAL_CARRIAGE_BAY, false);
        local.agent.world_target = CC_LOCAL_WORLD_TARGET_CARRIAGE;
        local.course.alarm_countdown = 1000.0f;
    }
    if (capture_underroad) {
        local.site_kind = CC_LOCAL_SITE_DUNGEON;
        RepositionHero(
            &local,
            (Vector2){CC_LOCAL_SITE_ENTRANCE_X - 3.0f,
                      CC_LOCAL_SITE_ENTRANCE_Z}, false);
    }
    if (capture_carriage_target) {
        (void)CcLocalAgentApproachWorldTarget(
            &local.agent, CC_LOCAL_WORLD_TARGET_CARRIAGE);
        local.course.alarm_countdown = 1000.0f;
    }
    if (capture_road_fork || capture_road_departure) {
        const CcRoute *route = SelectedOutgoingRoute(&sim, selected);
        if (route != NULL) {
            (void)EnterOpenWorldAtRoadGate(&sim, &local, route->id);
            local.world_carriage.camera_weight = capture_road_departure ?
                capture_road_zoom_weight : 1.0f;
            local.world_carriage.camera_target =
                local.world_carriage.camera_weight;
            local.road_choice_active = true;
            local.departure = (CcClientDepartureTransition){
                .phase = CC_CLIENT_DEPARTURE_READY,
                .town_progress = 1.0f,
                .road_book_progress = capture_road_departure ?
                    capture_road_zoom_weight : 1.0f,
            };
            local.convoy.phase = CC_LOCAL_CONVOY_ROAD;
            local.convoy.phase_progress = local.departure.road_book_progress;
            local.convoy.pace = 0.0f;
            if (capture_road_departure) {
                PositionOpenWorldDeparture(&sim, &local);
                if (capture_road_zoom_weight < 1.0f) {
                    local.convoy.pace = 0.72f;
                    local.world_carriage.pace = local.convoy.pace;
                }
            }
        }
        local.fork_turn_progress = 1.0f;
    }
    if (normal_play && sim.journey.active) {
        if (sim.journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
            BeginRoadLocalState(&sim, &local, false);
            view = VIEW_LOCAL;
        } else {
            BeginRoadTravelState(&sim, &local);
            view = VIEW_LOCAL;
        }
    } else if (resuming_campaign && journal != NULL) {
        if (RestoreClientStartupSession(
                session_path, &sim, &local, &view, &selected)) {
            (void)snprintf(startup_message, sizeof(startup_message),
                           "Campaign resumed where you left off.");
        } else {
            if (OpeningRequired(&sim) && !local.open_world) {
                BeginOpening(&local);
            }
            view = VIEW_LOCAL;
        }
    }
    if (!capture && sim.dungeon_expedition.active) {
        local.site_kind = CC_LOCAL_SITE_DUNGEON;
        return_view = VIEW_LOCAL;
        view = VIEW_DUNGEON;
        (void)snprintf(startup_message, sizeof(startup_message),
                       "The saved expedition resumes below the mine.");
    }
    if (!capture && !sim.journey.active &&
        !sim.dungeon_expedition.active &&
        sim.player.location_id == sim.dragon.lair_settlement_id &&
        sim.carriage.location_id == sim.goblins.lair_settlement_id) {
        if (local.site_kind != CC_LOCAL_SITE_DRAGON_CAVE) {
            EnterSiteFromGoblinTunnel(
                &local, CC_LOCAL_SITE_DRAGON_CAVE,
                (Vector2){CC_LOCAL_SITE_CARRIAGE_X + 3.0f,
                          CC_LOCAL_SITE_CARRIAGE_Z});
        }
        return_view = VIEW_LOCAL;
        view = VIEW_DRAGON_CAVE;
        (void)snprintf(
            startup_message, sizeof(startup_message),
            "The goblin network opens inside the dragon roost.");
    }
    if (capture_dragon_cave) {
        local.site_kind = CC_LOCAL_SITE_DRAGON_CAVE;
        RepositionHero(
            &local,
            (Vector2){CC_LOCAL_SITE_ENTRANCE_X - 3.0f,
                      CC_LOCAL_SITE_ENTRANCE_Z}, false);
        CcLocalAgentSetScene(&local.agent, CC_LOCAL_SCENE_ROAD);
        local.course.scene = CC_LOCAL_SCENE_ROAD;
        local.course.alarm_countdown = 1000.0f;
    }
    if (capture_golden || capture_town || capture_atmosphere) {
        RepositionHero(
            &local,
            capture_town ? (Vector2){capture_town_x, capture_town_z} :
                           (Vector2){44.25f, 28.85f},
            false);
        local.agent.facing_yaw = -0.35f;
        local.course.alarm_countdown = 1000.0f;
    }
    if (capture_town_arrival) {
        BeginTownArrivalState(&local);
        local.convoy.phase_progress = capture_town_arrival_progress;
        SetConvoyTownPose(&local.convoy, 0.0f);
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
            BeginRoadTravelState(&sim, &local);
        } else if (capture_creature_dragon ||
                   strcmp(capture_creature_family, "goblins") == 0) {
            local.site_kind = capture_creature_dragon ?
                CC_LOCAL_SITE_DRAGON_CAVE : CC_LOCAL_SITE_GOBLIN_CAVE;
            RepositionHero(
                &local,
                (Vector2){CC_LOCAL_SITE_ENTRANCE_X -
                              (capture_creature_dragon ? 12.0f : 7.0f),
                          CC_LOCAL_SITE_ENTRANCE_Z}, false);
            CcLocalAgentSetScene(&local.agent, CC_LOCAL_SCENE_ROAD);
            local.course.scene = CC_LOCAL_SCENE_ROAD;
            local.course.alarm_countdown = 1000.0f;
        } else {
            bool animal_view =
                strcmp(capture_creature_family, "animals") == 0 ||
                strcmp(capture_creature_family, "cow") == 0 ||
                strcmp(capture_creature_family, "sheep") == 0;
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
    if (capture_travel || capture_route_sight) {
        BeginRoadTravelState(&sim, &local);
        local.world_carriage.camera_weight = 1.0f;
        local.world_carriage.camera_target = 1.0f;
    }
    if (capture_road_arrival) {
        if (EnterOpenWorldAtRoadGate(
                &sim, &local, sim.journey.route_id)) {
            local.convoy.pace = 0.48f;
            BeginRoadBookArrivalState(&sim, &local);
            local.arrival.road_book_progress =
                1.0f - capture_road_zoom_weight;
            local.arrival.phase = CC_CLIENT_ARRIVAL_ROAD_BOOK;
            local.world_carriage.camera_weight = capture_road_zoom_weight;
            local.world_carriage.camera_target = capture_road_zoom_weight;
            (void)PositionOpenWorldArrival(&sim, &local);
        }
    }
    if (capture && !capture_world && !capture_interior &&
        !capture_walk_cycle &&
        !capture_jump && !capture_defense && !capture_downclimb &&
        !capture_navigation && !capture_limbs && !capture_dojo &&
        !capture_action_reel && !capture_gameplay_reel &&
        !capture_encounter && !capture_travel && !capture_route_sight &&
        !capture_road_fork &&
        !capture_road_zoom &&
        !capture_road &&
        !capture_parley && !capture_carriage &&
        !capture_carriage_target &&
        !capture_golden && !capture_town &&
        !capture_town_arrival &&
        !capture_atmosphere &&
        !capture_face && !capture_room && !capture_creature_media) {
        local.course.alarm_countdown = 1000.0f;
        for (int32_t frame = 0; frame < 1500; ++frame) {
            CcLocalCourseUpdate(&local.course, &local.agent, &sim,
                                1.0f / 60.0f);
        }
    }
    if (capture_character && local.course.situation_witness_active) {
        RepositionHero(
            &local,
            (Vector2){local.course.situation_witness.position.x + 1.15f,
                      local.course.situation_witness.position.z + 0.88f},
            false);
        local.conversation_character_id =
            local.course.situation_witness_character_id;
        local.conversation_situation_id =
            local.course.situation_witness_id;
        Vector3 toward_witness = {
            local.course.situation_witness.position.x -
                local.agent.position.x,
            0.0f,
            local.course.situation_witness.position.z -
                local.agent.position.z,
        };
        local.agent.facing_yaw = atan2f(
            toward_witness.x, toward_witness.z);
        local.course.situation_witness.facing_yaw = atan2f(
            -toward_witness.x, -toward_witness.z);
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
            ReleaseMapTextures(&map_textures);
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
    if (render_benchmark) {
        view = VIEW_LOCAL;
        return_view = VIEW_LOCAL;
        if (strcmp(render_benchmark_scene, "market") == 0) {
            local.market_interior = true;
            RepositionHero(&local, (Vector2){4.60f, 5.10f}, true);
            local.agent.facing_yaw = 0.14f;
        } else if (strcmp(render_benchmark_scene, "road") == 0) {
            BeginRoadTravelState(&sim, &local);
            local.convoy.pace = 0.0f;
        } else if (render_benchmark_roadbook) {
            BeginRoadTravelState(&sim, &local);
            local.convoy.pace = 0.0f;
            local.world_carriage.pace = 0.0f;
            local.world_carriage.camera_weight = 1.0f;
            local.world_carriage.camera_target = 1.0f;
        } else if (strcmp(render_benchmark_scene, "combat") == 0) {
            PrepareRoadCombatReel(&sim, &local);
        } else {
            RepositionHero(&local, (Vector2){44.25f, 28.85f}, false);
            local.agent.facing_yaw = -0.35f;
            local.course.alarm_countdown = 1000.0f;
        }
    }
    bool roadbook_state_ready = !roadbook_world_requested ||
        (local.open_world && local.world_stream.manifest.route_count > 0 &&
         local.world_carriage.visible);
    bool journey_state_ready = capture_road_arrival ?
        (!sim.journey.active && local.journey_travel_active &&
         local.arrival.phase == CC_CLIENT_ARRIVAL_ROAD_BOOK) :
        !(capture_travel || capture_route_sight ||
          render_benchmark_roadbook) ||
        (sim.journey.active &&
         sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING &&
         local.journey_travel_active);
    if (!roadbook_state_ready || !journey_state_ready) {
        (void)fprintf(
            stderr,
            "Road-book review setup failed: world=%d routes=%d carriage=%d journey=%d phase=%d travel=%d.\n",
            local.open_world ? 1 : 0,
            local.world_stream.manifest.route_count,
            local.world_carriage.visible ? 1 : 0,
            sim.journey.active ? 1 : 0,
            (int32_t)sim.journey.phase,
            local.journey_travel_active ? 1 : 0);
        CcLocalRendererShutdown();
        UnloadRenderTexture(local_target);
        ReleaseMapTextures(&map_textures);
        CloseWindow();
        CcClientInstanceLockRelease(&instance_lock);
        return 1;
    }
    char message[256] = "";
    if (!capture && !render_benchmark && startup_message[0] != '\0') {
        (void)snprintf(message, sizeof(message), "%s", startup_message);
    }
    if (capture_golden) {
        (void)snprintf(message, sizeof(message), "Town square.");
    } else if (capture_town || capture_town_arrival) {
        (void)snprintf(message, sizeof(message), "%s town plan.",
                       CcSettlementFunctionName(
                           sim.settlements[capture_town_index].function));
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
    } else if (capture_travel || capture_route_sight) {
        (void)snprintf(message, sizeof(message), "Travelling.");
    }
    int capture_frames = 0;
    int walk_frame_count = 0;
    const int32_t render_benchmark_warmup_frames = 60;
    int32_t render_benchmark_warmup_count = 0;
    int32_t render_benchmark_count = 0;
    double render_benchmark_started = 0.0;
    bool performance_overlay = false;
    float message_age = 0.0f;
#if defined(PLATFORM_WEB)
    bool browser_memory_reported = false;
#endif

    CcLocalRendererSetAtmosphere(
        capture_atmosphere ? capture_atmosphere_preset :
            LocalAtmosphereForSimulation(&sim),
        0.0f);

    Rectangle local_bounds;
    while (render_benchmark || !WindowShouldClose()) {
#if defined(PLATFORM_WEB)
        ClientWaitForAnimationFrame();
#endif
        local_bounds = LocalViewportBounds();
        float frame_delta_time = GetFrameTime();
        CcLocalRendererSetOpeningStep(local.opening_step);
        if (view == VIEW_ROADS) {
            local.fork_turn_progress = fminf(
                1.0f, local.fork_turn_progress + frame_delta_time * 0.78f);
        }
        char previous_message[sizeof(message)];
        (void)snprintf(previous_message, sizeof(previous_message), "%s",
                       message);
        CcLocalRendererSetAtmosphere(
            capture_atmosphere ? capture_atmosphere_preset :
                LocalAtmosphereForSimulation(&sim),
            2.4f);
        CcLocalRendererBeginFrame(frame_delta_time);
        CcLocalBindPlace(&sim);
        BindOpenWorldForLocalState(&local);
        if (!capture && !render_benchmark && ClientKeyPressed(KEY_F3)) {
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
        } else if (render_benchmark) {
            ClientInputClearPressed();
        } else {
            HandleInput(&journal, &sim, &selected, &selected_situation,
                        &view, &return_view, &local,
                        local_target, local_bounds,
                        frame_delta_time,
                        save_path, session_path, message, sizeof(message));
            ClientInputClearPressed();
        }
        if (!capture_road_arrival) {
            UpdateOpenWorldCamera(&sim, &local, frame_delta_time);
        }
        bool persistence_blocked = CcClientCampaignAccessFor(
            normal_play, journal != NULL) == CC_CLIENT_CAMPAIGN_BLOCKED;
        CcLocalRendererSetOpeningStep(local.opening_step);
        CcLocalBindPlace(&sim);
        BindOpenWorldForLocalState(&local);
        bool movement_preview_visible = view == VIEW_LOCAL &&
            !local.site_travel_active && !local.road_choice_active &&
            !local.journey_travel_active && !local.journey_parley_active;
        CcLocalRendererSetMovementPreview(
            movement_preview_visible ? &local.movement_preview : NULL);
        if (strcmp(previous_message, message) != 0) {
            message_age = 0.0f;
        } else {
            message_age += capture_gameplay_reel ? 1.0f / 15.0f :
                           frame_delta_time;
        }
        float clock = render_benchmark ?
            (float)(render_benchmark_warmup_count +
                    render_benchmark_count) / 60.0f :
            capture_gameplay_reel ?
            (float)gameplay_reel.captured_frames / 15.0f :
            capture_creature_reel ? (float)capture_frames / 15.0f :
            (float)GetTime();

        bool map_visible = view == VIEW_MAP ||
            ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
             return_view == VIEW_MAP);
        if (map_visible) {
            PrepareMapTextures(&sim, selected, &map_textures);
        }
#if defined(PLATFORM_WEB)
        else {
            ReleaseMapTextures(&map_textures);
        }
#endif

        BeginDrawing();
        ClearBackground(BACKGROUND);
        CcOverlayBegin(1.0f);
        bool road_choice_underlay = view == VIEW_ROADS ||
                            ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
                             return_view == VIEW_ROADS);
        if (road_choice_underlay) {
            if (local.open_world) {
                CcLocalDrawOpenWorld3D(
                    &sim, &local.world_stream, &local.agent, &local.course,
                    &local.world_carriage,
                    clock, local_target, local_bounds);
            } else {
                CcLocalDrawFork3D(
                    &sim, selected, local.fork_turn_progress, clock,
                    local_target, local_bounds);
            }
            DrawRoadHeader(&sim);
            DrawRoadPanel(&sim, selected);
        } else if (map_visible) {
            DrawMapHeader(&sim);
            DrawMap(&sim, selected, clock, map_textures.illustrated,
                    map_textures.collectible_atlas);
            DrawSettlementPanel(&sim, selected);
        } else {
            if (capture_npc_review) {
                CcLocalDrawNpcReview3D(capture_npc_review_view, clock,
                                       local_target, local_bounds);
            } else if (local.open_world && !local.market_interior) {
                CcLocalDrawOpenWorld3D(
                    &sim, &local.world_stream, &local.agent, &local.course,
                    &local.world_carriage,
                    clock, local_target, local_bounds);
            } else if (local.site_kind != CC_LOCAL_SITE_NONE) {
                CcLocalDrawSite3D(
                    &sim, &local.agent, local.site_kind,
                    local.site_travel_active, local.site_returning,
                    local.site_travel_progress, clock,
                    local_target, local_bounds);
            } else if (view == VIEW_ENCOUNTER) {
                CcLocalDrawRoad3D(&sim, &local.agent, &local.course,
                                  false, false, &local.convoy, clock,
                                  local_target, local_bounds);
            } else if (((local.road_choice_active ||
                         local.journey_travel_active) &&
                        local.convoy.phase == CC_LOCAL_CONVOY_ROAD) ||
                       local.journey_combat_active ||
                local.journey_parley_active) {
                CcLocalDrawRoad3D(&sim, &local.agent, &local.course,
                                  local.road_choice_active ||
                                      local.journey_travel_active,
                                  local.journey_parley_active,
                                  &local.convoy, clock,
                                  local_target, local_bounds);
            } else if (local.market_interior) {
                CcLocalDrawInterior3D(&sim, &local.agent, clock,
                                      local_target, local_bounds);
            } else {
                CcLocalDrawStreet3D(&sim, &local.agent, &local.course,
                                    view == VIEW_CHARACTER,
                                    &local.convoy, clock,
                                    local_target, local_bounds);
            }
            if (!capture_npc_review && view != VIEW_ENCOUNTER) {
                if (view == VIEW_LOCAL) {
                    DrawLocalMovementReticle(&local, local_bounds);
                }
                DrawLocalHeader(&sim, &local, view == VIEW_CHARACTER);
                DrawLocalPanel(&sim, &local);
            }
        }
        if (!capture_gameplay_reel && view == VIEW_LOCAL &&
            LocalCombatActive(&local)) {
            DrawCombatStatusLine(&local, message, message_age);
        }
        if (!persistence_blocked && !capture_gameplay_reel &&
            !capture_road_departure &&
            view == VIEW_LOCAL &&
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
        if (view == VIEW_CARRIAGE) {
            CcOverlayFlush();
            DrawCarriageScreen(&sim, &local);
        }
        if (view == VIEW_SITUATIONS) {
            CcOverlayFlush();
            DrawSituationBoard(&sim, selected_situation);
        }
        if (view == VIEW_CHARACTER) {
            CcOverlayFlush();
            DrawCharacterConversation(&sim, &local);
        }
        if (view == VIEW_ENCOUNTER) {
            CcOverlayFlush();
            DrawJourneyEncounter(&sim);
        }
        if (view == VIEW_DUNGEON) {
            CcOverlayFlush();
            DrawDungeonPanel(&sim);
        }
        if (view == VIEW_DRAGON_CAVE) {
            CcOverlayFlush();
            DrawDragonCavePanel(&sim);
        }
        CcOverlayFlush();
        if (!persistence_blocked && !capture_npc_review &&
            (!capture_gameplay_reel ||
            gameplay_reel.stage != GAMEPLAY_REEL_QUEST_COMPLETE)) {
            DrawContextActionTray(&sim, &local, view, selected,
                                  selected_situation);
        }
        if (!persistence_blocked && !capture_npc_review &&
            view != VIEW_DRAGON_CAVE &&
            view != VIEW_DUNGEON &&
            view != VIEW_CARRIAGE && view != VIEW_CHARACTER) {
            DrawCommandBar(view, &local);
        }
        if (performance_overlay) {
            CcOverlayFlush();
            DrawPerformanceOverlay();
        }
        if (capture_gameplay_reel) {
            CcOverlayFlush();
            DrawGameplayReelQuestComplete(&gameplay_reel);
            DrawGameplayReelTransition(&gameplay_reel);
        }
        if (persistence_blocked) {
            CcOverlayFlush();
            DrawCampaignUnavailable(message);
        }
        CcOverlayEnd();
        EndDrawing();
#if defined(PLATFORM_WEB)
        if (!browser_memory_reported) {
            TraceLog(LOG_INFO, "WEB: using %.1f MiB of linear memory",
                     (double)ClientBrowserHeapBytes() / (1024.0 * 1024.0));
            browser_memory_reported = true;
        }
#endif

        if (render_benchmark) {
            if (render_benchmark_warmup_count <
                render_benchmark_warmup_frames) {
                render_benchmark_warmup_count += 1;
                if (render_benchmark_warmup_count ==
                    render_benchmark_warmup_frames) {
                    CcLocalRendererResetPerformanceMetrics();
                    render_benchmark_started = GetTime();
                }
                continue;
            }
            render_benchmark_count += 1;
            if (render_benchmark_count >= render_benchmark_frames) break;
        } else if (capture_gameplay_reel) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                           capture_path, gameplay_reel.captured_frames);
            ClientTakeScreenshot(frame_path);
            gameplay_reel.captured_frames += 1;
            if (gameplay_reel.complete) break;
        } else if (capture_action_reel) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                           capture_path, action_reel.captured_frames);
            ClientTakeScreenshot(frame_path);
            action_reel.captured_frames += 1;
            if (action_reel.complete) break;
        } else if (capture_walk_cycle) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%02d.png",
                           capture_path, walk_frame_count);
            ClientTakeScreenshot(frame_path);
            walk_frame_count += 1;
            if (walk_frame_count >= 8) break;
        } else if (capture_creature_reel) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            int32_t creature_frame = capture_frames - 3;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                           capture_path, creature_frame);
            ClientTakeScreenshot(frame_path);
            if (creature_frame >= 44) break;
        } else if (capture) {
            capture_frames += 1;
            int32_t settled_frames = capture_road ? 45 :
                                     capture_character ? 120 : 3;
            if (capture_frames >= settled_frames) {
                ClientTakeScreenshot(capture_path);
                break;
            }
        }
    }

    bool campaign_unavailable = CcClientCampaignAccessFor(
        normal_play, journal != NULL) == CC_CLIENT_CAMPAIGN_BLOCKED;
    char journal_error[256];
    if (normal_play && !campaign_unavailable &&
        !SaveLocalSession(session_path, &sim, &local,
                          journal_error, sizeof(journal_error))) {
        (void)fprintf(stderr, "Could not save the local session: %s\n",
                      journal_error);
    }
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
    ReleaseMapTextures(&map_textures);
    CloseWindow();
    CcClientInstanceLockRelease(&instance_lock);
    if (render_benchmark) {
        double frames_per_second =
            (double)render_benchmark_count / render_benchmark_elapsed;
        double p95_budget = render_benchmark_p95_budget > 0.0 ?
            render_benchmark_p95_budget :
            (render_benchmark_minimum_fps > 0.0 ?
                1500.0 / render_benchmark_minimum_fps : 0.0);
        (void)printf("render: scene=%s frames=%d seconds=%.6f ms/frame=%.3f fps=%.1f p95=%.3f p95_budget=%.3f p99=%.3f max=%.3f hitches=%d skin_updates=%d skinned_meshes=%d hero_skin_updates=%d hero_skinned_meshes=%d npc_skin_updates=%d npc_skinned_meshes=%d creature_skin_updates=%d creature_skinned_meshes=%d high_detail=%d lod=%d static_batch_draws=%d static_batch_instances=%d static_batch_vertices=%d\n",
                     render_benchmark_scene, render_benchmark_count,
                     render_benchmark_elapsed,
                     render_benchmark_elapsed * 1000.0 /
                         (double)render_benchmark_count,
                     frames_per_second,
                     final_renderer_stats.p95_frame_milliseconds,
                     p95_budget,
                     final_renderer_stats.p99_frame_milliseconds,
                     final_renderer_stats.maximum_frame_milliseconds,
                     final_renderer_stats.hitch_count,
                     final_renderer_stats.skin_updates,
                     final_renderer_stats.skinned_meshes,
                     final_renderer_stats.hero_skin_updates,
                     final_renderer_stats.hero_skinned_meshes,
                     final_renderer_stats.npc_skin_updates,
                     final_renderer_stats.npc_skinned_meshes,
                     final_renderer_stats.creature_skin_updates,
                     final_renderer_stats.creature_skinned_meshes,
                     final_renderer_stats.high_detail_characters,
                     final_renderer_stats.low_detail_characters,
                     final_renderer_stats.static_batch_draws,
                     final_renderer_stats.static_batch_instances,
                     final_renderer_stats.static_batch_vertices);
        bool performance_failed = render_benchmark_minimum_fps > 0.0 &&
                                  frames_per_second <
                                      render_benchmark_minimum_fps;
        bool frame_time_failed = p95_budget > 0.0 &&
            final_renderer_stats.p95_frame_milliseconds > p95_budget;
        bool scene_expects_lod =
            strcmp(render_benchmark_scene, "combat") == 0;
        bool hero_is_embarked = render_benchmark_roadbook;
        bool hero_layout_failed = hero_is_embarked ?
            final_renderer_stats.high_detail_characters != 0 :
            (final_renderer_stats.high_detail_characters != 1 ||
             (scene_expects_lod &&
              final_renderer_stats.low_detail_characters <= 0));
        bool skin_layout_failed = hero_is_embarked ?
            (final_renderer_stats.hero_skin_updates != 0 ||
             final_renderer_stats.hero_skinned_meshes != 0 ||
             hero_layout_failed) :
            (final_renderer_stats.hero_skin_updates != 1 ||
             final_renderer_stats.hero_skinned_meshes <= 0 ||
             final_renderer_stats.hero_skinned_meshes >
                 CC_LOCAL_HERO_RUNTIME_MESH_BUDGET ||
             hero_layout_failed);
        if (performance_failed) {
            (void)fprintf(stderr,
                          "render performance budget failed: %.1f FPS < %.1f FPS\n",
                          frames_per_second, render_benchmark_minimum_fps);
        }
        if (frame_time_failed) {
            (void)fprintf(stderr,
                          "render frame-time budget failed: p95 %.1f ms > %.1f ms\n",
                          final_renderer_stats.p95_frame_milliseconds,
                          p95_budget);
        }
        if (skin_layout_failed) {
            (void)fprintf(stderr,
                          "render hero skin budget failed: updates=%d meshes=%d hero=%d lod=%d (mesh budget %d)\n",
                          final_renderer_stats.hero_skin_updates,
                          final_renderer_stats.hero_skinned_meshes,
                          final_renderer_stats.high_detail_characters,
                          final_renderer_stats.low_detail_characters,
                          CC_LOCAL_HERO_RUNTIME_MESH_BUDGET);
        }
        if (performance_failed || frame_time_failed || skin_layout_failed) {
            return 2;
        }
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
    return journal_close_failed || campaign_unavailable ? 1 : 0;
}
