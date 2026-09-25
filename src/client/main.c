#include "sim/cc_scriven.h"
#include "persistence/cc_starting_campaign.h"
#include "client/cc_audio.h"
#include "client/cc_client_policy.h"
#include "client/cc_interaction.h"
#include "client/cc_coop_client.h"
#include "client/cc_company.h"
#include "client/cc_client_session.h"
#include "client/cc_local3d.h"
#include "client/cc_local_place.h"
#include "client/cc_music_player.h"
#include "client/cc_overlay.h"
#include "client/cc_road_book.h"
#include "client/cc_style_pack.h"
#include "client/cc_visual_style.h"
#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "sim/cc_census.h"
#include "sim/cc_census_layout.h"
#include "sim/cc_production.h"
#include "sim/cc_road_position.h"
#include "sim/cc_mine.h"
#include "sim/cc_oven_court.h"
#include "story/cc_story.h"
#include "story/cc_core_conversation.h"
#include "story/cc_core_participant.h"
#include "world/cc_world.h"

#include "raylib.h"
#include "GLFW/glfw3.h"
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CcClientPreferences *adventure_preferences;

static CcCoreConversation core_conversation;
static CcId core_conversation_speaker;

#if defined(PLATFORM_WEB)
EMSCRIPTEN_KEEPALIVE int CrownlessRoadGeometrySelfTest(void)
{
    return CcRoadGeometryKnownFixtures() ? 1 : 0;
}
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#endif
EM_JS(void, ClientBrowserLocalNavigation,
    (float x, float y, float z, float terrain_y,
     float command_x, float command_z, int target_valid,
     int navigation_active, int path_index, int path_count,
     float stall_seconds, int interaction_approaching,
     int interaction_navigation, int descent_pending, int life_state,
     float health, int traversal, int grounded), {
    Module.crownlessLocalNavigation = {x, y, z, terrain_y,
        command_x, command_z,
        target_valid: !!target_valid, navigation_active: !!navigation_active,
        path_index, path_count, stall_seconds,
        interaction_approaching: !!interaction_approaching,
        interaction_navigation: !!interaction_navigation,
        descent_pending: !!descent_pending, life_state, health, traversal,
        grounded: !!grounded};
});
EM_JS(void, ClientBrowserContextAction, (int kind), {
    Module.crownlessLastContextAction = kind;
});
EM_JS(void, ClientBrowserReliefApproach, (int walking), {
    Module.crownlessLastReliefApproach = !!walking;
});
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

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
#define CC_ECONOMIC_GOODS_ATLAS_ASSET \
    "assets/ui/economic_goods_v01.png"
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
    VIEW_DRAGON_CAVE,
    VIEW_TRADE,
    VIEW_PAUSE,
    VIEW_OVEN_COURT
} ClientView;

typedef enum CarriageTab {
    CARRIAGE_OVERVIEW,
    CARRIAGE_PONIES,
    CARRIAGE_TAB_COUNT
} CarriageTab;

typedef struct ClientMapTextures {
    Texture2D illustrated;
    Texture2D collectible_atlas;
    Texture2D economic_goods;
    bool illustrated_attempted;
    bool collectible_atlas_attempted;
    bool economic_goods_attempted;
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

typedef enum ContextActionKind {
    CONTEXT_ACTION_NONE = 0,
    CONTEXT_ACTION_WORLD_TARGET,
    CONTEXT_ACTION_STOP_APPROACH,
    CONTEXT_ACTION_HOLD_TRAVEL,
    CONTEXT_ACTION_ROAD_OPTIONS,
    CONTEXT_ACTION_SET_PACE,
    CONTEXT_ACTION_APPROACH_ENTRANCE,
    CONTEXT_ACTION_PONY_MEET,
    CONTEXT_ACTION_PONY_HELP,
    CONTEXT_ACTION_PONY_SWAP,
    CONTEXT_ACTION_PONY_LEAVE,
    CONTEXT_ACTION_ENTER_MARKET,
    CONTEXT_ACTION_OPEN_TRADE,
    CONTEXT_ACTION_LEAVE_MARKET,
    CONTEXT_ACTION_CHOOSE_ROAD,
    CONTEXT_ACTION_OPEN_MAP,
    CONTEXT_ACTION_OPEN_PROMISES,
    CONTEXT_ACTION_REST_TEAM,
    CONTEXT_ACTION_CARE_HORSES,
    CONTEXT_ACTION_EXPEDITION,
    CONTEXT_ACTION_BUY_CARGO,
    CONTEXT_ACTION_SELL_CARGO,
    CONTEXT_ACTION_DELIVER_CARGO,
    CONTEXT_ACTION_ABANDON_PROMISE,
    CONTEXT_ACTION_NEXT_PROMISE,
    CONTEXT_ACTION_CLOSE_VIEW,
    CONTEXT_ACTION_GOSSIP_CHAT,
    CONTEXT_ACTION_FIGHT,
    CONTEXT_ACTION_PAY,
    CONTEXT_ACTION_TRAVEL,
    CONTEXT_ACTION_NEXT_BRANCH,
    CONTEXT_ACTION_REPAIR_ROUTE,
    CONTEXT_ACTION_PAY_COLLECTOR,
    CONTEXT_ACTION_APPROACH_COLLECTOR,
    CONTEXT_ACTION_OFFER_PROVISIONS,
    CONTEXT_ACTION_RETURN_TO_CHOICE,
    CONTEXT_ACTION_SKIP_TRAVEL,
    CONTEXT_ACTION_TAKE_BREAK,
    CONTEXT_ACTION_PRESS_ON,
    CONTEXT_ACTION_MAKE_CAMP,
    CONTEXT_ACTION_LODGE_ROAD_HOUSE,
    CONTEXT_ACTION_CAMP_ROAD_SITE,
    CONTEXT_ACTION_PASS_ROAD_SITE,
    CONTEXT_ACTION_CLEAR_ROAD_SITE,
    CONTEXT_ACTION_TRANSFER_ROAD_SITE,
    CONTEXT_ACTION_REPAIR_ROAD_SITE,
    CONTEXT_ACTION_CHOOSE_ROAD_LEG,
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
    CONTEXT_ACTION_DUNGEON_RESEAL,
    CONTEXT_ACTION_VISIT_MINE,
    CONTEXT_ACTION_STEP_DOWN,
    CONTEXT_ACTION_BOARD_CARRIAGE,
    CONTEXT_ACTION_MAKE_ROAD_CAMP,
    CONTEXT_ACTION_INSPECT_CARRIAGE,
    CONTEXT_ACTION_MINE_LEAD,
    CONTEXT_ACTION_MINE_SHIFT_RECORD,
    CONTEXT_ACTION_MINE_REPORT,
    CONTEXT_ACTION_PICKUP_RELIEF_CRATE,
    CONTEXT_ACTION_STOW_RELIEF_CRATE,
    CONTEXT_ACTION_APPROACH_RELIEF_CRATES,
    CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE,
    CONTEXT_ACTION_OVEN_QUESTION
} ContextActionKind;

typedef struct ContextAction {
    ContextActionKind kind;
    CcGood good;
    int32_t amount;
    CcInteractionKey target;
    char label[64];
    char key_hint[16];
    char detail[48];
    bool enabled;
    bool active;
} ContextAction;

typedef struct LocalState {
    bool adventure_ui;
    CcInteractionPlan interactions;
    CcInteractionState interaction;
    /* A presented town figure keeps its resident while the renderer rebuilds
       moving people every frame. */
    CcId presented_person_binding_place[64];
    uint64_t presented_person_binding_object[64];
    CcId presented_person_binding_character[64];
    int32_t card_page;
    ContextAction presented_road_actions[4];
    Rectangle presented_road_bounds[4];
    int32_t presented_road_count;
    CcId presented_road_route;
    bool world_cards_presented;
    int32_t presented_target_count;
    CcInteractionKey presented_targets[4];
    CcId presented_target_characters[4];
    Rectangle presented_target_bounds[4];
    Vector2 presented_card_origin;
    ClientView interaction_view;
    bool carriage_stopped;
    bool relief_carriage_descent_pending;
    ContextActionKind relief_approach;
    bool road_actions_expanded;
    CcClientTravelSample shared_travel_sample;
    CcId shared_travel_route, shared_travel_origin, shared_travel_segment;
    int32_t shared_travel_direction;
    /* A roadside inspection is a read-only overlay.  Keep the route and
       progress that selected the current carriage so boarding can reject a
       carriage that has since moved. */
    bool carriage_inspection_road;
    CcId road_carriage_route_id;
    int32_t road_carriage_progress_milli;
    uint64_t conversation_object;
    char conversation_name[64];
    char conversation_line[192];
    bool conversation_report_response;
    bool conversation_oven_response;
    CcId conversation_oven_event;
    CcOvenCourtObservation oven_observation;
    CcId oven_note_event;
    int32_t oven_page;
    int32_t conversation_gossip_slot;
    bool conversation_gossip_source;
    Vector3 conversation_position;
    int32_t book_page;
    int32_t book_offset;
    bool district_map_open;
    int32_t selected_district;
    int32_t selected_dwelling;
    int32_t trade_mode;
    CcGood trade_good;
    int32_t trade_quantity;
    bool trade_confirmed;
    CcId trade_presented_promise_id;
    bool trade_quote_presented;
    CcMoney trade_presented_total;
    CcMoney trade_presented_reward;
    CcCommand trade_presented_command;
    char receipt[256];
    uint32_t receipt_serial;
    double voice_ambient_after, voice_effort_after;
    bool voice_alarm;
    uint64_t voice_read_page;
    CcId voice_place;
    ClientView pause_return_view;
    bool request_save;
    bool settings_requested;
    double caravan_tap_deadline;
    CcWorldStream world_stream;
    CcLocalAgent agent;
    CcLocalCourse course;
    int32_t mine_facing;
    CcMinePhase mine_view_phase;
    float mine_cooldown;
    int32_t mine_target_x, mine_target_y;
    /* Mine routes and named uses are local presentation state.  They are
       rebuilt after loading and never become campaign-facing state. */
    int32_t mine_intent_target;
    int32_t mine_intent_revision;
    uint8_t mine_known[CC_MINE_WIDTH * CC_MINE_HEIGHT];
    int32_t mine_visibility_x, mine_visibility_y;
    CcMinePhase mine_visibility_phase;
    bool mine_visibility_bar_open;
    uint32_t mine_visibility_scans;
    uint32_t mine_visibility_rays;
    bool mine_combat_active;
    CcLocalConvoyState convoy;
    CcLocalWorldCarriageState world_carriage;
    CcClientDepartureTransition departure;
    CcClientArrivalTransition arrival;
    float travel_time_blend;
    bool travel_fast_forward;
    bool travel_pointer_down;
    bool travel_hold_armed;
    CarriageTab carriage_tab;
    float carriage_overview_scroll;
    bool travel_attention;
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

static bool AdventureScene(const LocalState *local);
static bool ApplyCommand(CcJournal *journal, CcSim *sim, CcCommand command,
                         char *message, size_t message_capacity);
static void DrawAdventureConversation(const CcSim *sim, const LocalState *local);
static int AdventureTextSize(int base);
static int AdventureWrap(const char *text, int x, int y, int width, int size, Color color);
static int AdventureText(const char *text, int x, int y, int width, int size, Color color, bool draw);
static int HeaderFitText(const char *text, int width, int initial_size,
                         int minimum_size, char *output, size_t capacity);
static int HeaderFitCaptionText(const char *text, int width, int initial_size,
                                int minimum_size, char *output, size_t capacity);
static void DrawAdventureHeader(const CcSim *sim, const LocalState *local,
                                ClientView view);
static Rectangle SpeechControlBounds(ClientView view, bool skip);
static Rectangle AdventureNavBounds(int index);
static void DrawAdventurePromises(const CcSim *sim, int32_t selected);
static void DrawAdventureFeedback(const char *message);
static void AdventureButton(Rectangle bounds, const char *label, bool enabled, bool active);
static void DrawCarriagePonies(const CcSim *sim);
#if defined(CC_CLIENT_SELF_TESTS)
static int ClientRegressionFailure(const char *message);
#endif

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

typedef struct ContextActionSet {
    ContextAction items[CC_INTERACTION_CAPACITY + CC_GOOD_COUNT + 8];
    int32_t count;
    bool combat;
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

#if defined(CC_CLIENT_SELF_TESTS)
static bool client_test_pointer_active;
static Vector2 client_test_pointer;
#endif
static void ClientInputInstall(void)
{
    GLFWwindow *window = glfwGetCurrentContext();
    if (window == NULL) return;
    previous_key_callback = glfwSetKeyCallback(window, ClientKeyCallback);
    previous_mouse_button_callback = glfwSetMouseButtonCallback(
        window, ClientMouseButtonCallback);
}

#include "cc_touch.inc"

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
#if defined(PLATFORM_WEB)
    touch_pointer_pending = false;
#endif
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
EM_ASYNC_JS(int, ClientDeleteBrowserCampaign, (), {
    try {
        await Module.deleteCrownlessCampaign();
        return 1;
    } catch (error) {
        console.error("World deletion failed", error);
        return 0;
    }
});
EM_ASYNC_JS(int, ClientFlushBrowserPreferences,
            (const char *preferences_path), {
    try {
        await Module.persistCrownlessPreferences(
            UTF8ToString(preferences_path));
        console.info("Crownless Carriage preferences stored.");
        return 1;
    } catch (error) {
        console.error("Could not store Crownless Carriage preferences",
                      error);
        return 0;
    }
});
EM_JS(void, ClientBrowserFrontend, (const char *screen, int focus, int avatar), {
    Module.crownlessScreen = UTF8ToString(screen);
    Module.crownlessMenuFocus = focus;
    Module.crownlessAvatarDraft = avatar;
});
EM_JS(void, ClientBrowserFullscreen, (), {
    Module.toggleCrownlessFullscreen();
});
EM_JS(void, ClientBrowserTouchControls, (), { Module.toggleCrownlessTouch(); });
EM_JS(int, ClientBrowserCampaignAccess, (), {
    return Number.isInteger(Module.crownlessCampaignAccess)
        ? Module.crownlessCampaignAccess : 1;
});

static const char *ClientBrowserCampaignAccessMessage(int32_t access)
{
    return access == 1 ?
        "This campaign is open in another tab. This tab is read-only." :
        "Another tab changed this campaign. Reload before saving.";
}
EM_JS(int, ClientReleaseBrowserAssets, (), {
    let releasedBytes = 0;
    const removeTree = (path, keep = false) => {
        for (const name of FS.readdir(path)) {
            if (name === "." || name === "..") continue;
            const child = path + "/" + name;
            const keepChild = keep || child === "/assets/audio";
            const stat = FS.stat(child);
            if (FS.isDir(stat.mode)) {
                removeTree(child, keepChild);
                if (!keepChild) FS.rmdir(child);
            } else if (keepChild) {
                // Preloaded files share one pack buffer. Give retained audio
                // its own bytes so the browser can collect the startup pack.
                const bytes = FS.readFile(child);
                FS.writeFile(child, bytes, {canOwn: true});
            } else {
                releasedBytes += stat.size;
                FS.unlink(child);
            }
        }
    };
    try {
        removeTree("/assets");
        if (FS.readdir("/assets").length === 2) FS.rmdir("/assets");
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

static const Vector2 LOCAL_MARKET = {CC_LOCAL_MARKET_X, CC_LOCAL_MARKET_Z};
static const Vector2 LOCAL_CARRIAGE = {CC_LOCAL_CARRIAGE_X,
                                      CC_LOCAL_CARRIAGE_Z};
static const Vector2 LOCAL_CARRIAGE_BAY = {
    CC_LOCAL_CARRIAGE_APPROACH_X, CC_LOCAL_CARRIAGE_APPROACH_Z
};
static const Vector2 LOCAL_RELIEF_CRATES = {44.40f,26.80f};
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
    return;
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
    if (sim != NULL && sim->world_seed == CC_DEEP_WYRM_SEED &&
        sim->current_day >= CC_DEEP_WYRM_DAY) return false;
    int32_t index = OpeningSituationIndex(sim);
    return sim != NULL && index >= 0 && sim->player.reputation == 0 &&
        sim->player.accepted_situation_id == 0U &&
        CcSimSituationOfferSettlementId(
            sim, &sim->situations[index]) == sim->player.location_id;
}

static const char *OpeningDirective(const CcSim *sim, int32_t *step_out)
{
    int32_t index = OpeningSituationIndex(sim);
    const CcSituation *opening = index >= 0 ? &sim->situations[index] : NULL;
    bool accepted = sim->player.accepted_situation_id != 0U &&
        opening != NULL && opening->id == sim->player.accepted_situation_id;
    if (step_out != NULL) *step_out = accepted ? 2 : 1;
    return accepted ? "Bring the listed food to the market, then board the carriage." :
        opening != NULL && CcSimSituationCanAccept(sim, opening) ?
            "Read the notice board, then talk to the giver." : "Read the notice board.";
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

/* The board holds dated postings: who to find and where, not the promise
   itself. The promise is made in person, so the posting names the giver. */
static const CcCharacter *SituationGiver(const CcSim *sim,
                                         const CcSituation *situation)
{
    if (sim == NULL || situation == NULL) return NULL;
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        switch (situation->discovery_stage) {
            case CC_DISCOVERY_RUMOR:
            case CC_DISCOVERY_DECISION:
                return CcSimSituationAffectedCharacter(sim, situation);
            case CC_DISCOVERY_WITNESS:
                return CcSimSituationWitnessCharacter(sim, situation);
            case CC_DISCOVERY_AUTHORITY:
                return CcSimSituationSponsorCharacter(sim, situation);
            case CC_DISCOVERY_OFFER:
                return situation->lead_path == CC_LEAD_PATH_CONFIDENCE ?
                    CcSimSituationAffectedCharacter(sim, situation) :
                    CcSimSituationSponsorCharacter(sim, situation);
        }
    }
    return CcSimSituationSponsorCharacter(sim, situation);
}

static const char *SituationGiverPlace(const CcSim *sim,
                                        const CcSituation *situation)
{
    const CcCharacter *giver = SituationGiver(sim, situation);
    const CcSettlement *place = giver != NULL ?
        CcSimSettlement(sim, giver->current_settlement_id) : NULL;
    return place != NULL ? place->name : "the road";
}

static const char *SituationPostingKind(CcSituationKind kind)
{
    switch (kind) {
        case CC_SITUATION_RELIEF_DELIVERY: return "Help wanted";
        case CC_SITUATION_ROUTE_REPAIR: return "Road work";
        case CC_SITUATION_MONSTER_EXPEDITION: return "Wanted";
        case CC_SITUATION_BLACK_MARKET_DELIVERY: return "Quiet delivery";
        case CC_SITUATION_COURIER_DELIVERY: return "Letter to carry";
    }
    return "Notice";
}

static void SituationFindLine(const CcSim *sim, const CcSituation *situation,
                              char *line, size_t capacity)
{
    if (sim == NULL || situation == NULL || line == NULL || capacity == 0U) {
        return;
    }
    const CcCharacter *giver = SituationGiver(sim, situation);
    const char *name = giver != NULL ? giver->name : "the sponsor";
    (void)snprintf(line, capacity, "Find %s in %s.",
        name, SituationGiverPlace(sim, situation));
}

static void SituationNextAction(const CcSim *sim,
                                const CcSituation *situation,
                                char *label, size_t capacity)
{
    if (sim == NULL || situation == NULL || label == NULL || capacity == 0U) {
        return;
    }
    if (situation->id != sim->player.accepted_situation_id && CcSimSituationCanAccept(sim, situation)) {
        SituationFindLine(sim, situation, label, capacity);
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
        if (situation->kind == CC_SITUATION_RELIEF_DELIVERY &&
            !CcSimReliefLoadingComplete(situation) &&
            sim->player.location_id == CcSimSituationOfferSettlementId(
                sim,situation)) {
            if (situation->loading_crate_carried)
                (void)snprintf(label,capacity,
                    "Carry the relief crate to the carriage.");
            else
                (void)snprintf(label,capacity,
                    "Load %d food boxes from the granary stack.",
                    CcSimReliefCratesToLoad(situation));
            return;
        }
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
                       "Repair: 2 Tools, 2 Wood, and 2 Stone, or 18 crowns." :
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

static void DrawTwoLineCaption(const char *text, int x, int y,
                               size_t line_capacity, int font_size, Color color,
                               float scale)
{
    if (text == NULL || line_capacity < 2U) return;
    size_t length = strlen(text);
    size_t split = length > line_capacity ? line_capacity : length;
    while (split > 0U && split < length && text[split] != ' ') split -= 1U;
    if (split == 0U) split = length > line_capacity ? line_capacity : length;
    CcOverlayDrawCaption(TextFormat("%.*s", (int)split, text), x, y, font_size, color);
    if (split < length) {
        size_t next = text[split] == ' ' ? split + 1U : split;
        CcOverlayDrawCaption(text + next, x, y + (int)((float)font_size * scale) + 5,
                             font_size, color);
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
                             float *heading, float *travelled)
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
    /* The path is walked by arc length, so this is the ground the wheels have
       rolled over and the team has stepped through. */
    if (travelled != NULL) *travelled = remaining;
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
    Vector2 town_path[CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY];
    int32_t town_count = CcLocalTownCarriagePath(
        convoy->phase == CC_LOCAL_CONVOY_ARRIVING, town_path,
        CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY);
    if (town_count >= 2) {
        path = town_path;
        count = town_count;
    }
    Vector2 position = {0};
    float heading = convoy->town_heading_yaw;
    SampleConvoyPath(path, count, convoy->phase_progress,
                     &position, &heading, &convoy->travelled);
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
    bool stopped = local->carriage_stopped ||
        (sim != NULL && sim->journey.active &&
         CcSimJourneyRequiresRoadChoice(sim));
    bool journey_halt = local->journey_travel_active && sim != NULL &&
        sim->journey.active &&
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING &&
        convoy->phase == CC_LOCAL_CONVOY_ROAD;
    bool captain_pace = local->journey_travel_active && sim != NULL &&
        sim->journey.active &&
        sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING &&
        convoy->phase == CC_LOCAL_CONVOY_ROAD;
    if (journey_halt || captain_pace) {
        if (journey_halt) stopped = true;
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

static const Vector3 *LocalConversationFocus(const LocalState *local, ClientView view)
{
    return local->adventure_ui && view == VIEW_CHARACTER ?
        &local->conversation_position : NULL;
}

static void ResetLocalState(LocalState *local)
{
    local->interactions = (CcInteractionPlan){0};
    local->interaction = (CcInteractionState){0};
    memset(local->presented_person_binding_place, 0,
           sizeof(local->presented_person_binding_place));
    memset(local->presented_person_binding_object, 0,
           sizeof(local->presented_person_binding_object));
    memset(local->presented_person_binding_character, 0,
           sizeof(local->presented_person_binding_character));
    local->card_page = 0;
    local->presented_road_count = 0;
    local->world_cards_presented = false;
    local->presented_target_count = 0;
    local->carriage_stopped = false;
    local->road_actions_expanded = false;
    local->shared_travel_sample = (CcClientTravelSample){0};
    local->conversation_gossip_slot = -1;
    local->conversation_gossip_source = false;
    local->conversation_object = 0;
    local->conversation_position = (Vector3){0};
    local->conversation_name[0] = '\0';
    local->conversation_line[0] = '\0';
    local->conversation_report_response = false;
    local->conversation_oven_response = false;
    CcCoreConversationReset(&core_conversation);
    core_conversation_speaker = 0U;
    local->trade_quantity = 1;
    local->trade_good = CC_GOOD_FOOD;
    local->trade_mode = 0;
    local->trade_confirmed = false;
    local->trade_quote_presented = false;
    local->book_offset = 0;
    local->book_page = 0;
    local->carriage_tab = CARRIAGE_OVERVIEW;
    local->receipt[0] = '\0';
    local->request_save = false;
    local->caravan_tap_deadline = 0.0;
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
    local->travel_time_blend = 0.0f;
    local->travel_fast_forward = false;
    local->travel_hold_armed = false;
    local->travel_pointer_down = false;
    local->travel_attention = false;
    local->world_carriage.storybook_travel = false;
    local->journey_combat_active = false;
    local->journey_parley_active = false;
    local->mine_facing = 0;
    local->mine_view_phase = CC_MINE_NONE;
    local->mine_cooldown = 0.0f;
    local->mine_target_x = -1;
    local->mine_target_y = -1;
    local->mine_intent_target = 0;
    local->mine_intent_revision = -1;
    memset(local->mine_known, 0, sizeof(local->mine_known));
    local->mine_visibility_x = -1;
    local->mine_visibility_y = -1;
    local->mine_visibility_phase = CC_MINE_NONE;
    local->mine_visibility_bar_open = false;
    local->mine_visibility_scans = 0;
    local->mine_visibility_rays = 0;
    local->mine_combat_active = false;
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

static void ResetLocalStatePreservingAthletics(LocalState *local)
{
    CcAthleticProfile athletics = local->agent.athletics;
    ResetLocalState(local);
    local->agent.athletics = athletics;
}

#if defined(CC_CLIENT_SELF_TESTS)
static bool AthleticProfilesMatch(const CcAthleticProfile *first,
                                  const CcAthleticProfile *second)
{
    if (first == NULL || second == NULL ||
        fabsf(first->travel_training_distance -
              second->travel_training_distance) >= 0.0001f) {
        return false;
    }
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        if (first->level[discipline] != second->level[discipline] ||
            fabsf(first->experience[discipline] -
                  second->experience[discipline]) >= 0.0001f) {
            return false;
        }
    }
    return true;
}

static CcAthleticProfile NonDefaultAthleticProfile(void)
{
    return (CcAthleticProfile){
        .experience = {12.5f, 23.5f, 34.5f},
        .travel_training_distance = 8.25f,
        .level = {2, 3, 4}
    };
}
#endif

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

static CcWorldPoint LegacyVersionThreeRoutePoint(
    CcWorldPoint first, CcWorldPoint control, CcWorldPoint last,
    float amount)
{
    amount = ClampUnit(amount);
    float inverse = 1.0f - amount;
    return (CcWorldPoint){
        inverse * inverse * first.x +
            2.0f * inverse * amount * control.x +
            amount * amount * last.x,
        inverse * inverse * first.z +
            2.0f * inverse * amount * control.z +
            amount * amount * last.z,
    };
}

static bool LegacyVersionThreeRoadGatePosition(
    const CcWorldManifest *manifest,
    const CcWorldRoutePlacement *route, CcId settlement_id,
    CcWorldPoint *position)
{
    if (manifest == NULL || route == NULL || position == NULL ||
        (route->from_id != settlement_id &&
         route->to_id != settlement_id)) {
        return false;
    }
    const CcWorldSettlementPlacement *from =
        CcWorldSettlementPlacementForId(manifest, route->from_id);
    const CcWorldSettlementPlacement *to =
        CcWorldSettlementPlacementForId(manifest, route->to_id);
    const CcWorldSettlementPlacement *origin =
        CcWorldSettlementPlacementForId(manifest, settlement_id);
    if (from == NULL || to == NULL || origin == NULL) return false;

    float dx = to->center.x - from->center.x;
    float dz = to->center.z - from->center.z;
    float direct_length = sqrtf(dx * dx + dz * dz);
    if (direct_length <= 0.001f) return false;
    float bend_value =
        (float)(route->seed & UINT32_C(0x00ffffff)) /
            (float)UINT32_C(0x00ffffff) * 2.0f - 1.0f;
    float bend = bend_value * fminf(32.0f, direct_length * 0.14f);
    CcWorldPoint control = {
        (from->center.x + to->center.x) * 0.5f -
            dz / direct_length * bend,
        (from->center.z + to->center.z) * 0.5f +
            dx / direct_length * bend,
    };
    float route_length = 0.0f;
    CcWorldPoint previous = from->center;
    const int32_t legacy_sample_count = 17;
    for (int32_t sample = 1;
         sample < legacy_sample_count; ++sample) {
        float sample_amount = (float)sample /
            (float)(legacy_sample_count - 1);
        CcWorldPoint current = LegacyVersionThreeRoutePoint(
            from->center, control, to->center, sample_amount);
        float segment_x = current.x - previous.x;
        float segment_z = current.z - previous.z;
        route_length += sqrtf(
            segment_x * segment_x + segment_z * segment_z);
        previous = current;
    }
    if (route_length <= 0.001f) return false;

    float journey_amount = fminf(
        0.22f, ClampUnit((origin->radius + 5.0f) / route_length));
    bool reverse = route->to_id == settlement_id;
    float route_amount = reverse ? 1.0f - journey_amount : journey_amount;
    *position = LegacyVersionThreeRoutePoint(
        from->center, control, to->center, route_amount);
    return true;
}

static bool WorldSessionRouteScore(
    const CcWorldRoutePlacement *route, CcId settlement_id,
    CcWorldPoint position, float facing_yaw,
    float *distance_squared, float *heading_distance,
    float *nearest_route_amount)
{
    if (route == NULL || distance_squared == NULL ||
        heading_distance == NULL ||
        (route->from_id != settlement_id &&
         route->to_id != settlement_id)) {
        return false;
    }
    float total_length = CcWorldRouteLength(route);
    if (total_length <= 0.0001f) return false;
    float best_distance = INFINITY;
    float best_heading = INFINITY;
    float best_route_amount = 0.0f;
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
            SessionAngleDistance(heading, facing_yaw) : INFINITY;
        if (candidate_distance < best_distance - 0.0001f ||
            (fabsf(candidate_distance - best_distance) <= 0.0001f &&
             candidate_heading < best_heading)) {
            best_distance = candidate_distance;
            best_heading = candidate_heading;
            best_route_amount = route_amount;
        }
        travelled += segment_length;
    }
    *distance_squared = best_distance;
    *heading_distance = best_heading;
    if (nearest_route_amount != NULL) {
        *nearest_route_amount = ClampUnit(best_route_amount);
    }
    return isfinite(best_distance);
}

static const CcRoute *InferWorldSessionRoute(
    const CcSim *sim, const CcWorldManifest *manifest,
    const CcClientSession *session)
{
    const float distance_epsilon = 0.0001f;
    const float heading_epsilon = 0.001f;
    if (sim == NULL || manifest == NULL || session == NULL) return NULL;
    const CcRoute *best_route = NULL;
    float best_distance = INFINITY;
    float best_heading = INFINITY;
    CcWorldPoint position = {session->position_x, session->position_z};
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id != session->location_id &&
            route->to_id != session->location_id) {
            continue;
        }
        const CcWorldRoutePlacement *placement =
            CcWorldRoutePlacementForId(manifest, route->id);
        float distance = INFINITY;
        float heading = INFINITY;
        if (!WorldSessionRouteScore(
                placement, session->location_id, position,
                session->facing_yaw, &distance, &heading, NULL)) {
            continue;
        }
        if (distance < best_distance - distance_epsilon ||
            (fabsf(distance - best_distance) <= distance_epsilon &&
             heading < best_heading - heading_epsilon)) {
            best_route = route;
            best_distance = distance;
            best_heading = heading;
        }
    }
    return best_distance <= 49.0f ? best_route : NULL;
}

static bool LegacyVersionThreeGateRestorePose(
    const CcSim *sim, const CcWorldManifest *manifest,
    const CcClientSession *session, const CcRoute **route,
    CcWorldPoint *position, float *heading_yaw)
{
    if (sim == NULL || manifest == NULL || session == NULL ||
        session->coordinate_space != CC_CLIENT_SESSION_WORLD ||
        session->route_id != 0U || route == NULL ||
        position == NULL || heading_yaw == NULL) {
        return false;
    }
    CcWorldPoint saved = {session->position_x, session->position_z};
    const CcRoute *matched_route = NULL;
    float matched_distance_squared = INFINITY;
    const float distance_tie_epsilon = 0.000001f;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *candidate = &sim->routes[i];
        if (candidate->from_id != session->location_id &&
            candidate->to_id != session->location_id) {
            continue;
        }
        const CcWorldRoutePlacement *candidate_placement =
            CcWorldRoutePlacementForId(manifest, candidate->id);
        CcWorldPoint legacy_gate;
        if (!LegacyVersionThreeRoadGatePosition(
                manifest, candidate_placement, session->location_id,
                &legacy_gate)) {
            continue;
        }
        float dx = saved.x - legacy_gate.x;
        float dz = saved.z - legacy_gate.z;
        float distance_squared = dx * dx + dz * dz;
        if (distance_squared <
                matched_distance_squared - distance_tie_epsilon) {
            matched_route = candidate;
            matched_distance_squared = distance_squared;
        }
    }
    if (matched_route == NULL || matched_distance_squared > 0.01f) {
        return false;
    }

    const CcWorldRoutePlacement *matched_placement =
        CcWorldRoutePlacementForId(manifest, matched_route->id);
    if (matched_placement == NULL) return false;
    bool forward = matched_route->from_id == session->location_id;
    int32_t junction_sample = forward ?
        CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE :
        CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE;
    float junction_route_amount = CcWorldRouteSampleAmount(
        matched_placement, junction_sample);
    float junction_journey_amount = forward ?
        junction_route_amount : 1.0f - junction_route_amount;
    if (!CcWorldRoutePose(matched_placement, session->location_id,
                          junction_journey_amount,
                          position, heading_yaw)) {
        return false;
    }
    *route = matched_route;
    return true;
}

static void PublishCarriagePose(LocalState *local, Vector3 position,
                                float heading_yaw, float travelled,
                                bool advance_sample, float alpha);

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
    CcWorldStreamFollowRouteTimed(&local->world_stream, route, origin_id, amount, 4, 0.0015);
    PublishCarriagePose(local, (Vector3){
        point.x, CcWorldStreamHeightAt(&local->world_stream, point.x, point.z),
        point.z}, heading, amount * length, false, 1.0f);
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
    PublishCarriagePose(local, (Vector3){place->center.x,
        CcWorldStreamHeightAt(&local->world_stream, place->center.x, place->center.z),
        place->center.z}, 0.0f, 0.0f, false, 1.0f);
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
    CcWorldStreamFollowRouteTimed(&local->world_stream, route, sim->player.location_id, journey_amount, 4, 0.0015);
    PublishCarriagePose(local, (Vector3){
        point.x, CcWorldStreamHeightAt(&local->world_stream, point.x, point.z),
        point.z}, heading, journey_amount * CcWorldRouteLength(route), false, 1.0f);
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
    CcWorldStreamFollowRouteTimed(&local->world_stream, route, origin_id, journey_amount, 4, 0.0015);
    Vector3 position = {
        point.x,
        CcWorldStreamHeightAt(&local->world_stream, point.x, point.z),
        point.z,
    };
    float travelled = journey_amount * CcWorldRouteLength(route);
    /* Arrival is frame-driven, so each frame is a fresh sample at full blend. */
    PublishCarriagePose(local, position, heading, travelled, false, 1.0f);
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
    if (local->world_carriage.storybook_travel &&
        !RoadBookArrivalInProgress(local)) {
        local->world_carriage.camera_weight = local->travel_time_blend;
        local->world_carriage.camera_target = local->travel_time_blend;
        float turn = remainderf(local->world_carriage.heading_yaw -
            local->world_carriage.camera_heading_yaw, 2.0f * PI);
        local->world_carriage.camera_heading_yaw +=
            turn * (1.0f - expf(-4.0f * fmaxf(0.0f, delta_time)));
        return;
    }
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
static void PublishCarriagePose(LocalState *local, Vector3 position,
                                float heading_yaw, float travelled,
                                bool advance_sample, float alpha)
{
    CcLocalCarriagePublishPose(&local->world_carriage, position, heading_yaw,
                              travelled, advance_sample, alpha);
}

static void PositionOpenWorldJourneyAt(const CcSim *sim, LocalState *local,
                                       int32_t sample_steps, float alpha);

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
    local->world_carriage.camera_target = 0.0f;
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

static void PositionOpenWorldJourneyAt(const CcSim *sim, LocalState *local,
                                       int32_t sample_steps, float alpha)
{
    if (sim == NULL || local == NULL || !local->open_world ||
        !sim->journey.active) return;
    const CcWorldRoutePlacement *route = CcWorldRoutePlacementForId(
        &local->world_stream.manifest, sim->journey.route_id);
    if (route == NULL) return;
    float progress = sim->journey.total_subticks > 0 ? ClampUnit(
        (float)sim->journey.elapsed_subticks /
        (float)sim->journey.total_subticks) : 0.0f;
    if (CcCoopClientActive()) {
        bool reset = local->shared_travel_route != sim->journey.route_id ||
            local->shared_travel_origin != sim->journey.origin_id ||
            local->shared_travel_segment != sim->journey.road_segment_id ||
            local->shared_travel_direction != sim->journey.road_direction;
        bool boundary = local->carriage_stopped ||
            sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING ||
            CcSimJourneyRequiresRoadChoice(sim);
        progress = (float)CcClientTravelSampleStep(&local->shared_travel_sample,
            progress, sim->clock.tick, sample_steps > 0 ? 1.0f / 60.0f : 0.0f,
            reset || boundary);
        local->shared_travel_route = sim->journey.route_id;
        local->shared_travel_origin = sim->journey.origin_id;
        local->shared_travel_segment = sim->journey.road_segment_id;
        local->shared_travel_direction = sim->journey.road_direction;
    }
    float amount = CcWorldRouteJourneyAmount(
        route, sim->journey.origin_id, progress);
    CcWorldPoint point;
    float heading = 0.0f;
    if (!CcWorldRoutePose(route, sim->journey.origin_id, amount,
                          &point, &heading)) return;
    CcWorldStreamFollowRouteTimed(&local->world_stream, route, sim->journey.origin_id, amount, 4, 0.0015);
    Vector3 position = {
        point.x,
        CcWorldStreamHeightAt(&local->world_stream, point.x, point.z),
        point.z,
    };
    /* Measured from the origin, not from the route's own start, so that a
       route walked the other way still counts up. */
    float travelled = amount * CcWorldRouteLength(route);
    PublishCarriagePose(local, position, heading, travelled, sample_steps > 0, alpha);
    if (local->world_carriage.hero_embarked || !local->world_carriage.storybook_travel) {
        local->agent.position = position;
        local->agent.target_valid = false;
        local->agent.exact_target_valid = false;
        local->agent.facing_yaw = heading;
    }
    local->world_carriage.route_amount =
        route->from_id == sim->journey.origin_id ? amount : 1.0f - amount;
    local->world_carriage.pace =
        sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING &&
            !local->carriage_stopped && !sim->journey.road_waiting_choice &&
            (!CcCoopClientActive() || local->shared_travel_sample.since_update < 1.5) &&
            !CcSimJourneyRequiresRoadChoice(sim) ?
            local->convoy.pace : 0.0f;
    local->world_carriage.camera_target = local->travel_time_blend;
    local->world_carriage.route_id = sim->journey.route_id;
    local->world_carriage.visible = true;
    local->world_carriage.storybook_travel = true;
}

static void PositionOpenWorldJourney(const CcSim *sim, LocalState *local)
{
    PositionOpenWorldJourneyAt(sim, local, 0, 1.0f);
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

static bool StableWorldRoadChoice(const LocalState *local)
{
    return local != NULL && local->open_world &&
           local->road_choice_active &&
           local->departure.phase == CC_CLIENT_DEPARTURE_READY &&
           local->departure.road_book_progress >= 1.0f &&
           local->world_carriage.visible &&
           local->world_carriage.route_id != 0U;
}

_Static_assert(CC_CLIENT_SESSION_GUARD_COUNT == CC_LOCAL_COURSE_RUNNER_COUNT,
               "session guard count must match the local encounter");
_Static_assert(CC_CLIENT_SESSION_RAIDER_COUNT == CC_LOCAL_RAIDER_COUNT,
               "session raider count must match the local encounter");
_Static_assert(CC_CLIENT_SESSION_SKILL_COUNT == CC_COMBAT_SKILL_COUNT,
               "session skill count must match local combat");
_Static_assert(CC_CLIENT_SESSION_ATHLETIC_COUNT ==
                   CC_ATHLETIC_DISCIPLINE_COUNT,
               "session athletic count must match local athletics");
_Static_assert(CC_CLIENT_SESSION_ATHLETIC_MAX_LEVEL ==
                   CC_ATHLETIC_MAX_LEVEL,
               "session athletic maximum must match local athletics");

static void CaptureAthleticProfile(CcClientAthleticProfile *saved,
                                   const CcAthleticProfile *profile)
{
    if (saved == NULL || profile == NULL) return;
    saved->travel_training_distance = profile->travel_training_distance;
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        saved->level[discipline] = profile->level[discipline];
        saved->experience[discipline] = profile->experience[discipline];
    }
}

static void RestoreAthleticProfile(CcAthleticProfile *profile,
                                   const CcClientAthleticProfile *saved)
{
    if (profile == NULL || saved == NULL) return;
    profile->travel_training_distance = saved->travel_training_distance;
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        profile->level[discipline] = saved->level[discipline];
        profile->experience[discipline] = saved->experience[discipline];
    }
}

static CcClientRoadEncounterMode RoadEncounterMode(
    const LocalState *local)
{
    if (local != NULL && local->mine_combat_active &&
        local->course.mine_encounter) return CC_CLIENT_ROAD_ENCOUNTER_MINE;
    if (local == NULL || !local->course.road_encounter) {
        return CC_CLIENT_ROAD_ENCOUNTER_NONE;
    }
    if (local->journey_combat_active && !local->journey_parley_active) {
        return CC_CLIENT_ROAD_ENCOUNTER_FIGHT;
    }
    if (local->journey_parley_active && !local->journey_combat_active) {
        return CC_CLIENT_ROAD_ENCOUNTER_PARLEY;
    }
    return CC_CLIENT_ROAD_ENCOUNTER_NONE;
}

static bool LocalSessionEligible(const LocalState *local)
{
    if (local == NULL) return false;
    if (RoadEncounterMode(local) != CC_CLIENT_ROAD_ENCOUNTER_NONE) {
        return !local->open_world && !local->market_interior &&
               !local->site_travel_active &&
               !local->journey_travel_active;
    }
    return
           (!local->road_choice_active || StableWorldRoadChoice(local)) &&
           (!local->journey_travel_active ||
            (!local->site_travel_active && !local->market_interior)) &&
           !local->journey_combat_active && !local->journey_parley_active &&
           (!local->open_world || !local->market_interior);
}

static void CaptureEncounterActor(CcClientEncounterActor *saved,
                                  const CcLocalAgent *actor)
{
    *saved = (CcClientEncounterActor){
        .position_x = actor->position.x,
        .position_y = actor->position.y,
        .position_z = actor->position.z,
        .velocity_x = actor->velocity.x,
        .velocity_y = actor->velocity.y,
        .velocity_z = actor->velocity.z,
        .facing_yaw = actor->facing_yaw,
        .focus_x = actor->combat.focus_point.x,
        .focus_y = actor->combat.focus_point.y,
        .focus_z = actor->combat.focus_point.z,
        .knockback_x = actor->combat.knockback_velocity.x,
        .knockback_y = actor->combat.knockback_velocity.y,
        .knockback_z = actor->combat.knockback_velocity.z,
        .health = actor->combat.health,
        .posture = actor->combat.posture,
        .stagger_seconds = actor->combat.stagger_seconds,
        .hit_flash_seconds = actor->combat.hit_flash_seconds,
        .hitstop_seconds = actor->combat.hitstop_seconds,
        .respawn_seconds = actor->combat.respawn_seconds,
        .auto_attack_cooldown = actor->combat.auto_attack_cooldown,
        .target_index = actor->combat.target_index,
        .queued_skill = actor->combat.queued_skill,
        .active_skill = actor->combat.active_skill,
        .life_state = (int32_t)actor->combat.life_state,
        .weapon_mode = (int32_t)actor->combat.weapon_mode,
        .focus_valid = actor->combat.focus_valid,
        .strike_resolved = actor->combat.strike_resolved,
    };
    for (int32_t skill = 0; skill < CC_COMBAT_SKILL_COUNT; ++skill) {
        saved->skill_cooldown[skill] = actor->combat.skill_cooldown[skill];
    }
}

static void CaptureRoadEncounter(CcClientRoadEncounter *saved,
                                 const LocalState *local)
{
    saved->mode = RoadEncounterMode(local);
    if (saved->mode == CC_CLIENT_ROAD_ENCOUNTER_NONE)
        saved->mode = CC_CLIENT_ROAD_ENCOUNTER_LOCAL;
    CaptureEncounterActor(&saved->player, &local->agent);
    saved->engagement_time = local->course.engagement_time;
    saved->alarm_countdown = local->course.alarm_countdown;
    saved->combat_event_seconds = local->course.combat_event_seconds;
    saved->raider_initial_resolve = local->course.raider_initial_resolve;
    saved->raider_resolve = local->course.raider_resolve;
    saved->defenses_completed = local->course.defenses_completed;
    saved->alarm_active = local->course.alarm_active;
    saved->raiders_retreating = local->course.raiders_retreating;
    for (int32_t guard = 0;
         guard < CC_LOCAL_COURSE_RUNNER_COUNT; ++guard) {
        const CcLocalCourseRunner *runner = &local->course.runners[guard];
        CaptureEncounterActor(&saved->guards[guard], &runner->agent);
        saved->guard_pause_seconds[guard] = runner->pause_seconds;
        saved->guard_attack_cooldown[guard] = runner->attack_cooldown;
        saved->guard_duty[guard] = (int32_t)runner->duty;
        saved->guard_response_stage[guard] = runner->response_stage;
    }
    for (int32_t raider = 0;
         raider < CC_LOCAL_RAIDER_COUNT; ++raider) {
        CaptureEncounterActor(&saved->raiders[raider],
                              &local->course.raiders[raider]);
        saved->raider_attack_cooldown[raider] =
            local->course.raider_attack_cooldown[raider];
        saved->raider_response_stage[raider] =
            local->course.raider_response_stage[raider];
    }
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
        case CC_CLIENT_SESSION_ROAD_TRAVEL:
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
    if (!LocalSessionEligible(local) ||
        (local->journey_travel_active && (!sim->journey.active ||
         (sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING &&
          sim->journey.phase != CC_JOURNEY_PHASE_RESTING &&
          sim->journey.phase != CC_JOURNEY_PHASE_ROAD_CHOICE)))) {
        if (error != NULL && error_capacity > 0U) {
            (void)snprintf(error, error_capacity,
                           "Finish the current movement before saving.");
        }
        return false;
    }
    CcClientSession session = {
        .version = CC_CLIENT_SESSION_VERSION,
        .world_seed = sim->world_seed,
        .location_id = sim->player.location_id,
        .scene = local->journey_travel_active ? CC_CLIENT_SESSION_ROAD_TRAVEL :
            ClientSceneForLocalState(local),
        .coordinate_space = local->open_world ?
            CC_CLIENT_SESSION_WORLD : CC_CLIENT_SESSION_LEGACY_LOCAL,
        .route_id = local->journey_travel_active ? sim->journey.route_id :
            local->open_world ? local->world_carriage.route_id : 0U,
        .position_x = local->agent.position.x,
        .position_z = local->agent.position.z,
        .facing_yaw = local->agent.facing_yaw,
        .opening_step = (uint32_t)local->opening_step,
        .site_travel_progress = local->site_travel_progress,
        .site_travel_active = local->site_travel_active,
        .site_returning = local->site_returning
    };
    CaptureAthleticProfile(&session.athletics, &local->agent.athletics);
    CaptureRoadEncounter(&session.road_encounter, local);
    if (session.road_encounter.mode == CC_CLIENT_ROAD_ENCOUNTER_MINE) {
        session.road_encounter.mine_source_id=sim->mine.source_id;
        session.road_encounter.mine_group_id=sim->goblins.id;
        session.road_encounter.mine_revision=sim->mine.revision;
    }
    return CcClientSessionWrite(path, &session, error, error_capacity);
}

static const CcSim *coop_checkpoint_sim = NULL;
static const LocalState *coop_checkpoint_local = NULL;
static const char *coop_checkpoint_path = NULL;

#if defined(PLATFORM_WEB)
EMSCRIPTEN_KEEPALIVE
#endif
void CcCoopCheckpointNow(void)
{
    if (!CcCoopClientActive() || coop_checkpoint_sim == NULL ||
        coop_checkpoint_local == NULL || coop_checkpoint_path == NULL) return;
    char error[192];
    if (SaveLocalSession(coop_checkpoint_path, coop_checkpoint_sim,
                         coop_checkpoint_local, error, sizeof(error))) {
        CcCoopClientCheckpoint(coop_checkpoint_path);
    }
}

static bool WorldStreamCanRestoreSession(
    const CcSim *sim, const LocalState *local,
    const CcClientSession *session, const CcRoute **route,
    CcWorldPoint *position, float *facing_yaw, float *route_amount)
{
    const CcWorldSettlementPlacement *settlement =
        sim != NULL && local != NULL && session != NULL ?
            CcWorldSettlementPlacementForId(
                &local->world_stream.manifest, session->location_id) : NULL;
    if (sim == NULL || local == NULL || session == NULL ||
        local->world_stream.manifest.world_seed != sim->world_seed ||
        local->world_stream.manifest.generator_version !=
            sim->generator_version ||
        settlement == NULL ||
        !CcWorldManifestContains(&local->world_stream.manifest,
                                 session->position_x,
                                 session->position_z)) {
        return false;
    }
    CcWorldPoint restored_position = {
        session->position_x, session->position_z
    };
    float restored_facing_yaw = session->facing_yaw;
    const CcRoute *restored_route = NULL;
    bool migrated_legacy_gate = LegacyVersionThreeGateRestorePose(
        sim, &local->world_stream.manifest, session,
        &restored_route, &restored_position, &restored_facing_yaw);
    if (!migrated_legacy_gate) {
        restored_route = session->route_id != 0U ?
            CcSimRoute(sim, session->route_id) :
            InferWorldSessionRoute(
                sim, &local->world_stream.manifest, session);
    }
    if (restored_route == NULL ||
        (restored_route->from_id != session->location_id &&
         restored_route->to_id != session->location_id) ||
        CcWorldRoutePlacementForId(
            &local->world_stream.manifest, restored_route->id) == NULL) {
        return false;
    }
    const CcWorldRoutePlacement *placement = CcWorldRoutePlacementForId(
        &local->world_stream.manifest, restored_route->id);
    float road_distance_squared = INFINITY;
    float heading_distance = INFINITY;
    float restored_route_amount = 0.0f;
    if (!WorldSessionRouteScore(
            placement, session->location_id, restored_position,
            restored_facing_yaw, &road_distance_squared,
            &heading_distance, &restored_route_amount) ||
        road_distance_squared > 49.0f) {
        return false;
    }
    if (route != NULL) *route = restored_route;
    if (position != NULL) *position = restored_position;
    if (facing_yaw != NULL) *facing_yaw = restored_facing_yaw;
    if (route_amount != NULL) *route_amount = restored_route_amount;
    return true;
}

static bool RestoreWorldSession(const CcSim *sim, LocalState *local,
                                const CcClientSession *session)
{
    const CcRoute *route = NULL;
    CcWorldPoint position = {0};
    float facing_yaw = 0.0f;
    float route_amount = 0.0f;
    if (!WorldStreamCanRestoreSession(
            sim, local, session, &route, &position,
            &facing_yaw, &route_amount)) {
        return false;
    }

    LeaveOpenWorld(local);
    ResetLocalState(local);
    local->world_carriage = (CcLocalWorldCarriageState){0};
    local->open_world = true;
    BindOpenWorldForLocalState(local);
    CcWorldStreamUpdate(&local->world_stream,
                        position.x, position.z,
                        CC_WORLD_STREAM_CAPACITY);
    RepositionHero(local,
                   (Vector2){position.x, position.z}, false);
    CcLocalAgentSetScene(&local->agent, CC_LOCAL_SCENE_STREET);
    local->course.scene = CC_LOCAL_SCENE_STREET;
    local->course.alarm_countdown = 1000.0f;
    local->agent.facing_yaw = facing_yaw;
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
        .heading_yaw = facing_yaw,
        .route_amount = route_amount,
        .pace = 0.0f,
        .camera_weight = 1.0f,
        .camera_target = 1.0f,
        .route_id = route->id,
        .visible = true,
        .hero_embarked = true,
    };
    return true;
}

static void BeginRoadLocalState(const CcSim *sim, LocalState *local,
                                bool hostile);

static void RestoreEncounterActor(CcLocalAgent *actor,
                                  const CcClientEncounterActor *saved)
{
    actor->position = (Vector3){saved->position_x, saved->position_y,
                                saved->position_z};
    actor->velocity = (Vector3){saved->velocity_x, saved->velocity_y,
                                saved->velocity_z};
    actor->facing_yaw = saved->facing_yaw;
    actor->combat.focus_point = (Vector3){saved->focus_x, saved->focus_y,
                                          saved->focus_z};
    actor->combat.knockback_velocity = (Vector3){
        saved->knockback_x, saved->knockback_y, saved->knockback_z
    };
    actor->combat.health = saved->health;
    actor->combat.posture = saved->posture;
    actor->combat.stagger_seconds = saved->stagger_seconds;
    actor->combat.hit_flash_seconds = saved->hit_flash_seconds;
    actor->combat.hitstop_seconds = saved->hitstop_seconds;
    actor->combat.respawn_seconds = saved->respawn_seconds;
    actor->combat.auto_attack_cooldown = saved->auto_attack_cooldown;
    for (int32_t skill = 0; skill < CC_COMBAT_SKILL_COUNT; ++skill) {
        actor->combat.skill_cooldown[skill] = saved->skill_cooldown[skill];
    }
    actor->combat.target_index = saved->target_index;
    actor->combat.queued_skill = saved->queued_skill;
    actor->combat.active_skill = saved->active_skill;
    actor->combat.life_state = (CcLifeState)saved->life_state;
    actor->combat.weapon_mode = (CcWeaponMode)saved->weapon_mode;
    actor->combat.focus_valid = saved->focus_valid;
    actor->combat.strike_resolved = saved->strike_resolved;
    actor->humanoid_needs_reset = true;
    if (actor->combat.life_state != CC_LIFE_ALIVE &&
        actor->morphology == CC_MORPHOLOGY_BIPED) {
        CcLimbVec3 position = {actor->position.x, actor->position.y,
                               actor->position.z};
        CcLimbVec3 direction = {saved->knockback_x, saved->knockback_y,
                                saved->knockback_z};
        CcLimbVec3 impact = {saved->focus_x, saved->focus_y,
                             saved->focus_z};
        CcHumanoidGaitInit(&actor->humanoid, position,
                           actor->facing_yaw, NULL, NULL);
        actor->humanoid_needs_reset = false;
        if (actor->combat.life_state == CC_LIFE_KNOCKED_DOWN) {
            (void)CcHumanoidGaitKnockDown(&actor->humanoid);
        } else {
            (void)CcHumanoidGaitDie(&actor->humanoid, direction,
                                    impact, 4.0f);
            if (actor->combat.life_state == CC_LIFE_RESPAWNING) {
                CcHumanoidGaitBeginResurrection(&actor->humanoid);
            }
        }
    }
}

static void RestoreRoadEncounter(LocalState *local,
                                 const CcClientRoadEncounter *saved)
{
    RestoreEncounterActor(&local->agent, &saved->player);
    local->course.engagement_time = saved->engagement_time;
    local->course.alarm_countdown = saved->alarm_countdown;
    local->course.combat_event_seconds = saved->combat_event_seconds;
    local->course.raider_initial_resolve = saved->raider_initial_resolve;
    local->course.raider_resolve = saved->raider_resolve;
    local->course.defenses_completed = saved->defenses_completed;
    local->course.alarm_active = saved->alarm_active;
    local->course.raiders_retreating = saved->raiders_retreating;
    for (int32_t guard = 0;
         guard < CC_LOCAL_COURSE_RUNNER_COUNT; ++guard) {
        CcLocalCourseRunner *runner = &local->course.runners[guard];
        RestoreEncounterActor(&runner->agent, &saved->guards[guard]);
        runner->pause_seconds = saved->guard_pause_seconds[guard];
        runner->attack_cooldown = saved->guard_attack_cooldown[guard];
        runner->duty = (CcGuardDuty)saved->guard_duty[guard];
        runner->response_stage = saved->guard_response_stage[guard];
    }
    for (int32_t raider = 0;
         raider < CC_LOCAL_RAIDER_COUNT; ++raider) {
        RestoreEncounterActor(&local->course.raiders[raider],
                              &saved->raiders[raider]);
        local->course.raider_attack_cooldown[raider] =
            saved->raider_attack_cooldown[raider];
        local->course.raider_response_stage[raider] =
            saved->raider_response_stage[raider];
    }
}

static void BeginRoadTravelState(const CcSim *sim, LocalState *local);

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
    if (session.road_encounter.mode == CC_CLIENT_ROAD_ENCOUNTER_FIGHT ||
        session.road_encounter.mode == CC_CLIENT_ROAD_ENCOUNTER_PARLEY) {
        if (!sim->journey.active ||
            sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED) {
            return false;
        }
        BeginRoadLocalState(
            sim, local,
            session.road_encounter.mode == CC_CLIENT_ROAD_ENCOUNTER_FIGHT);
        RestoreRoadEncounter(local, &session.road_encounter);
        RestoreAthleticProfile(&local->agent.athletics,
                               &session.athletics);
        return true;
    }
    if (session.road_encounter.mode == CC_CLIENT_ROAD_ENCOUNTER_MINE) {
        if (!sim->mine.contest_active ||
            session.road_encounter.mine_source_id != sim->mine.source_id ||
            session.road_encounter.mine_group_id != sim->goblins.id ||
            session.road_encounter.mine_revision != sim->mine.revision) return false;
        ResetLocalStatePreservingAthletics(local);
        CcLocalCourseStageMineEncounter(&local->course,&local->agent,sim);
        RestoreRoadEncounter(local,&session.road_encounter);
        RestoreAthleticProfile(&local->agent.athletics,&session.athletics);
        local->mine_combat_active=true;
        local->mine_view_phase=sim->mine.phase;
        return true;
    }
    if (session.scene == CC_CLIENT_SESSION_ROAD_TRAVEL) {
        if (!sim->journey.active || session.route_id != sim->journey.route_id ||
            (sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING &&
             sim->journey.phase != CC_JOURNEY_PHASE_RESTING &&
             sim->journey.phase != CC_JOURNEY_PHASE_ROAD_CHOICE)) return false;
        BeginRoadTravelState(sim, local);
        RestoreRoadEncounter(local, &session.road_encounter);
        RestoreAthleticProfile(&local->agent.athletics, &session.athletics);
        /* Resume at the saved road progress with the team held for the player. */
        local->convoy.pace = 0.0f;
        local->world_carriage.pace = 0.0f;
        local->carriage_stopped = true;
        local->travel_fast_forward = false;
        local->travel_attention = true;
        return true;
    }
    if (sim->journey.active) return false;
    if (session.coordinate_space == CC_CLIENT_SESSION_WORLD) {
        bool restored = RestoreWorldSession(sim, local, &session);
        if (restored) {
            RestoreAthleticProfile(&local->agent.athletics,
                                   &session.athletics);
            if (session.road_encounter.mode == CC_CLIENT_ROAD_ENCOUNTER_LOCAL)
                RestoreRoadEncounter(local, &session.road_encounter);
        }
        return restored;
    }

    LeaveOpenWorld(local);
    ResetLocalState(local);
    RestoreAthleticProfile(&local->agent.athletics, &session.athletics);
    bool market = session.scene == CC_CLIENT_SESSION_MARKET;
    CcLocalSiteKind site = LocalSiteForClientScene(session.scene);
    /* Movement can save the hero inside the old half-unit edge margin. */
    bool in_bounds = market ?
        session.position_x >= 0.5f && session.position_x <= 12.0f &&
        session.position_z >= 0.5f && session.position_z <= 8.0f :
        session.position_x >= 0.0f &&
        session.position_x <= CC_LOCAL_WORLD_WIDTH &&
        session.position_z >= 0.0f &&
        session.position_z <= CC_LOCAL_WORLD_DEPTH;
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
    local->site_travel_progress = session.site_travel_progress;
    local->site_travel_active = session.site_travel_active;
    local->site_returning = session.site_returning;
    if (session.road_encounter.mode == CC_CLIENT_ROAD_ENCOUNTER_LOCAL)
        RestoreRoadEncounter(local, &session.road_encounter);
    if (sim->player.accepted_situation_id != 0U ||
        sim->player.reputation != 0) {
        local->opening_step = CC_LOCAL_OPENING_COMPLETE;
    } else if (local->opening_step != CC_LOCAL_OPENING_COMPLETE) {
        local->opening_step = CC_LOCAL_OPENING_MEET_MARA;
    }
    return true;
}

static bool RestoreClientStartupSession(const char *path, const CcSim *sim,
                                        LocalState *local, ClientView *view,
                                        int32_t *selected)
{
    if (view == NULL || !RestoreLocalSession(path, sim, local)) return false;
    *view = local->open_world && !local->journey_travel_active ? VIEW_ROADS : VIEW_LOCAL;
    if (local->open_world && selected != NULL) {
        *selected = OpenWorldRouteIndex(
            sim, local->world_carriage.route_id);
    }
    return true;
}

static void BeginRoadLocalState(const CcSim *sim, LocalState *local,
                                bool hostile)
{
    float lateral_offset = local->convoy.lateral_offset;
    float pace = local->convoy.pace;
    LeaveOpenWorld(local);
    ResetLocalStatePreservingAthletics(local);
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
    /* Loading/re-entering a road is a discontinuity even on the same route. */
    CcLocalCarriageResetGaitsInternal();
    float lateral_offset = local->convoy.lateral_offset;
    float pace = local->convoy.pace;
    ResetLocalStatePreservingAthletics(local);
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
        local->world_carriage.camera_weight = 0.0f;
        local->world_carriage.camera_target = 0.0f;
        local->world_carriage.camera_heading_yaw =
            local->world_carriage.heading_yaw;
    }
    local->journey_travel_active = true;
}

static bool TravelNeedsSlowTime(const CcSim *sim)
{
    return !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING ||
        sim->pony_company.encounter >= 0 || CcSimJourneyRequiresRoadChoice(sim);
}

/* Advance one tick at a time so a warning remains a visible travel beat. */
static bool AdvanceStorybookTravel(CcJournal *journal, CcSim *sim,
                                   LocalState *local, int32_t ticks,
                                   char *error, size_t error_capacity)
{
    bool slow_before = TravelNeedsSlowTime(sim);
    if (local->carriage_stopped || CcSimJourneyRequiresRoadChoice(sim)) return true;
    bool warned = sim->journey.ambush_warned;
    bool resolved = sim->journey.ambush_resolved;
    for (int32_t tick = 0; tick < ticks; ++tick) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {.kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP};
            if (!ApplyCommand(journal, sim, rest, error, error_capacity)) return false;
        }
        if (!CcJournalAdvanceRuntimeTicks(journal, sim, 1,
                                          error, error_capacity)) {
            local->travel_fast_forward = false;
            local->travel_attention = true;
            return false;
        }
        if (sim->pony_company.encounter >= 0 || !sim->journey.active ||
            CcSimJourneyRequiresRoadChoice(sim) ||
            (sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING &&
             sim->journey.phase != CC_JOURNEY_PHASE_RESTING) ||
            (local->travel_fast_forward && !slow_before &&
             sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING &&
             TravelNeedsSlowTime(sim)) ||
            sim->journey.ambush_warned != warned ||
            sim->journey.ambush_resolved != resolved) {
            local->travel_fast_forward = false;
            local->travel_attention = true;
            local->convoy.runtime_tick_accumulator = 0.0f;
            break;
        }
    }
    if (sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING &&
        sim->journey.phase != CC_JOURNEY_PHASE_RESTING) {
        local->world_carriage.pace = 0.0f;
    }
    return true;
}

static bool AdvanceCarriagePresentationSteps(CcJournal *journal, CcSim *sim,
    LocalState *local, int32_t fixed_steps, float road_motion, bool road_stop,
    char *error, size_t error_capacity)
{
    bool warned_before = sim->journey.ambush_warned;
    bool ambush_resolved_before = sim->journey.ambush_resolved;
    for (int32_t step = 0; step < fixed_steps; ++step) {
        bool held = local->carriage_stopped || CcSimJourneyRequiresRoadChoice(sim);
        if (held) local->convoy.runtime_tick_accumulator = 0.0f;
        local->convoy.runtime_tick_accumulator += (held ? 0.0f : road_motion) *
            (local->travel_fast_forward ?
                CcClientTravelTimeScale(local->travel_time_blend) : 1.0f) *
            (road_stop ? 0.5f : 1.0f);
        int32_t ticks = (int32_t)floorf(local->convoy.runtime_tick_accumulator);
        local->convoy.runtime_tick_accumulator -= (float)ticks;
        bool advanced = CcCoopClientActive() || AdvanceStorybookTravel(
            journal, sim, local, ticks, error, error_capacity);
        if (!advanced) return false;
        if (local->open_world && sim->journey.active) {
            /* Publish this local step even when it consumed no journey
               tick. That unchanged sample prevents replaying an old
               interval backwards when the local alpha wraps. */
            PositionOpenWorldJourneyAt(sim, local, 1, 1.0f);
        }
        float road_clock = local->world_carriage.storybook_travel ?
            (float)fmod((double)sim->clock.tick /
                        (double)CC_WORLD_TICKS_PER_SECOND, 3600.0) :
            (float)GetTime();
        if (local->open_world) {
            CcLocalOpenWorldCarriageTargetsInternal(
                sim, &local->world_carriage, road_clock, 1.0f / 60.0f);
        } else {
            CcLocalRoadTravelHorseTargetsInternal(sim, &local->convoy, road_clock);
        }
        CcLocalCreatureGaitsAdvanceInternal(1);
        if (!sim->journey.active ||
            (sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING &&
             sim->journey.phase != CC_JOURNEY_PHASE_RESTING) ||
            sim->pony_company.encounter >= 0 ||
            sim->journey.ambush_warned != warned_before ||
            sim->journey.ambush_resolved != ambush_resolved_before) break;
    }
    return true;
}

static void BeginRoadChoiceApproachState(LocalState *local, bool from_town)
{
    float pace = local->convoy.pace;
    ResetLocalStatePreservingAthletics(local);
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
    float pace = local->convoy.pace;
    CcClientArrivalTransition arrival = local->arrival;
    if (arrival.phase != CC_CLIENT_ARRIVAL_TOWN) {
        arrival = (CcClientArrivalTransition){
            .phase = CC_CLIENT_ARRIVAL_TOWN,
            .road_book_progress = 1.0f,
        };
    }
    LeaveOpenWorld(local);
    ResetLocalStatePreservingAthletics(local);
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
    local->world_carriage.arrival_travel_weight = local->travel_time_blend;
    local->travel_fast_forward = false;
    local->travel_time_blend = 0.0f;
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
    CcNpcAppearance appearance = local->agent.appearance;
    CcMorphologyPreset morphology = local->agent.morphology;
    Color tunic_color = local->agent.tunic_color;
    bool crowned = local->agent.crowned;
    ResetLocalStatePreservingAthletics(local);
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
        ResetLocalStatePreservingAthletics(local);
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
    CcNpcAppearance appearance = local->agent.appearance;
    CcMorphologyPreset morphology = local->agent.morphology;
    Color tunic_color = local->agent.tunic_color;
    bool crowned = local->agent.crowned;
    ResetLocalStatePreservingAthletics(local);
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


static const char *TravelForecastLine(const CcSim *sim);
static const char *TravelActionDetail(const CcSim *sim, const LocalState *local);

static int HeaderMeasureText(const char *text, int font_size, bool caption)
{
    return caption ? CcOverlayMeasureCaptionText(text, font_size) :
                     CcOverlayMeasureText(text, font_size);
}

static int HeaderFitTextMeasured(const char *text, int width, int initial_size,
                                 int minimum_size, char *output,
                                 size_t capacity, bool caption)
{
    if (output == NULL || capacity == 0U) return minimum_size;
    output[0] = '\0';
    if (text == NULL || width <= 0) return minimum_size;

    int font_size = initial_size;
    while (font_size > minimum_size &&
           HeaderMeasureText(text, font_size, caption) > width) {
        --font_size;
    }
    (void)snprintf(output, capacity, "%s", text);
    if (HeaderMeasureText(output, font_size, caption) <= width) return font_size;

    size_t length = strlen(output);
    while (length > 0U) {
        while (length > 0U &&
               (((unsigned char)output[length - 1U] & 0xc0U) == 0x80U)) {
            --length;
        }
        if (length > 0U) --length;
        while (length + 4U > capacity && length > 0U) --length;
        if (length + 4U > capacity) {
            output[0] = '\0';
            return font_size;
        }
        memcpy(output + length, "...", 4U);
        if (HeaderMeasureText(output, font_size, caption) <= width) return font_size;
        /* Restore the source prefix before removing another whole codepoint. */
        size_t source_length = strlen(text);
        if (source_length < length) length = source_length;
        (void)snprintf(output, capacity, "%.*s", (int)length, text);
    }
    (void)snprintf(output, capacity, "...");
    return font_size;
}

static int HeaderFitText(const char *text, int width, int initial_size,
                         int minimum_size, char *output, size_t capacity)
{
    return HeaderFitTextMeasured(text, width, initial_size, minimum_size,
                                 output, capacity, false);
}

static int HeaderFitCaptionText(const char *text, int width, int initial_size,
                                int minimum_size, char *output, size_t capacity)
{
    return HeaderFitTextMeasured(text, width, initial_size, minimum_size,
                                 output, capacity, true);
}

static void DrawLocalHeader(const CcSim *sim, const LocalState *local,
                            ClientView view, bool conversation)
{
    if (local->adventure_ui) { DrawAdventureHeader(sim, local, view); return; }
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
        int32_t opening_step = 1;
        const char *beat_text = OpeningDirective(sim, &opening_step);
        const char *beat = TextFormat("OBJECTIVE  /  Step %d of 2  /  %s",
                                      opening_step, beat_text);
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
    const char *title = site ?
             local->site_travel_active ?
                 TextFormat("%s  ->  %s",
                            origin != NULL ? origin->name : "Town",
                            CcLocalSiteName(sim, local->site_kind)) :
                 TextFormat("%s  /  LOCAL SITE",
                            CcLocalSiteName(sim, local->site_kind)) :
             finding_road ?
             TextFormat("%s  /  CHOOSE A ROAD",
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
                 "Crownless";
    const char *summary = TextFormat(
        "DAY %d     %" PRId64 " cr     CARGO %d/%d",
        sim->current_day, sim->player.coins,
        CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity);
    int summary_width = CcOverlayMeasureText(summary, 10);
    int summary_x = GetScreenWidth() - summary_width - 22;
    char fitted_title[256];
    int title_size = HeaderFitText(title, summary_x - 44, 18, 12,
                                   fitted_title, sizeof(fitted_title));
    CcOverlayDrawText(fitted_title, 22, 18, title_size, INK);
    CcOverlayDrawText(summary, summary_x, 22, 10, road ? TEAL : CC_GOLD);
    if (!road && !site && place != NULL && !local->market_interior) {
        char condition_text[96];
        CcLocalTownConditionText(CcSimTownConditions(sim, place->id),
                                 condition_text, sizeof(condition_text));
        int condition_width = CcOverlayMeasureText(condition_text, 8);
        CcOverlayDrawText(condition_text, GetScreenWidth() - condition_width - 22,
                          40, 8, MUTED);
    } else if (road) {
        const char *forecast = TravelForecastLine(sim);
        int forecast_width = CcOverlayMeasureText(forecast, 8);
        CcOverlayDrawText(forecast, GetScreenWidth() - forecast_width - 22,
                          40, 8, TEAL);
    }
    const CcRoadSite *road_stop = CcSimJourneyRoadSiteStop(sim);
    if (local->journey_travel_active && road_stop != NULL) {
        DrawPanel((Rectangle){22.0f, 86.0f, 460.0f, 68.0f}, PANEL_DEEP);
        CcOverlayDrawText(road_stop->name, 38, 100, 16, INK);
        CcOverlayDrawText(road_stop == CcMineSite(sim) ?
                          "MINE TURN-OFF / ENTER YARD OR CONTINUE" :
                          "ROADSIDE STOP / CHOOSE OR CONTINUE",
                          38, 131, 9, TEAL);
    }
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

static const char *TravelForecastLine(const CcSim *sim)
{
    if (!sim->journey.active) {
        const CcSettlement *from = CcSimSettlement(sim, sim->player.location_id);
        const CcSettlement *to = CcSimSettlement(sim, sim->journey.destination_id);
        return TextFormat(
            "PREPARE  /  %s -> %s   %d WATCHES   HORSES %d%%   CARGO %d/%d",
            from != NULL ? from->name : "Town", to != NULL ? to->name : "?",
            CcSimJourneyWatchCount(sim),
            sim->pony_company.ponies[0].health,
            CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity);
    }
    if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
        return "ROAD AHEAD  /  BLOCKED  /  FIGHT, PARLEY OR RETURN";
    }
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
        return "RESTING  /  THE TEAM RECOVERS  /  RISE WHEN READY";
    }
    const CcRoadSite *road_stop = CcSimJourneyRoadSiteStop(sim);
    if (road_stop != NULL) {
        if (road_stop == CcMineSite(sim)) {
            return TextFormat(
                "ROAD AHEAD  /  %s  %s  /  ENTER YARD OR CONTINUE",
                road_stop->name,
                road_stop->accessible ? "CLEAR" : "BLOCKED");
        }
        return TextFormat(
            "ROAD AHEAD  /  %s  %s  /  CHOOSE OR CONTINUE",
            road_stop->name,
            road_stop->accessible ? (road_stop->condition < 100 ? "REPAIRABLE" : "CLEAR") : "BLOCKED");
    }
    if (sim->journey.ambush_warned && !sim->journey.ambush_resolved) {
        const CcBanditGroup *bandits = CcSimBanditGroupOnRoute(
            sim, sim->journey.route_id);
        return TextFormat("SCOUTS  /  %.24s  /  CAREFUL PACE MAY BYPASS",
            bandits != NULL ? bandits->name : "ARMED RIDERS");
    }
    if (sim->pony_company.encounter >= 0) {
        return "ROAD AHEAD  /  PONY COMPANY  /  MEET THEM";
    }
    const CcSettlement *to = CcSimSettlement(sim, sim->journey.destination_id);
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    return TextFormat(
        "ROAD AHEAD  /  %s  %d WATCHES   %s",
        to != NULL ? to->name : "?", CcSimJourneyWatchCount(sim),
        route != NULL && route->closed ? "CLOSED" :
        route != NULL && route->condition < 60 ? "ROAD POOR" : "ROAD CLEAR");
}

static const char *TravelActionDetail(const CcSim *sim, const LocalState *local)
{
    if (sim == NULL || local == NULL) return "Travel";
    if (local->carriage_stopped) return "RESUME THIS JOURNEY / SPACE";
    if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED ||
        CcSimJourneyRequiresRoadChoice(sim)) return "CHOOSE HOW TO PROCEED";
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) return "ROUTINE REST / SPACE TO STOP";
    if (CcCoopClientActive() && local->shared_travel_sample.valid &&
        local->shared_travel_sample.since_update >= 1.5) return "WAITING FOR SHARED WORLD";
    return "MOVING AUTOMATICALLY / SPACE TO STOP";
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

static const char *JourneyRoadBeatName(const CcSim *sim)
{
    CcJourneyStopKind stop = CcSimJourneyStop(sim);
    if (stop == CC_JOURNEY_STOP_MIDDAY) {
        return "MIDDAY HALT · WATER, SCOUT, OR PRESS ON";
    }
    if (stop == CC_JOURNEY_STOP_OVERNIGHT) {
        return CcSimJourneyRoadHouseAvailable(sim) ?
            "EVENING · ROAD HOUSE OR CAMP" :
            "EVENING · SET CAMP AND LANTERN WATCH";
    }
    int32_t watch = CcSimJourneyWatchNumber(sim);
    return watch % 2 == 1 ?
        "MORNING ROAD · WATCH FOR SIGNS" :
        "AFTERNOON ROAD · FIND THE NIGHT STOP";
}

static void DrawLocalPanel(const CcSim *sim, const LocalState *local)
{
    if (local->adventure_ui && !LocalCombatActive(local) &&
        (AdventureScene(local) || local->journey_travel_active || local->road_choice_active ||
         local->site_kind != CC_LOCAL_SITE_NONE)) return;
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
            CcJourneyStopKind journey_stop = CcSimJourneyStop(sim);
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
                              journey_stop == CC_JOURNEY_STOP_MIDDAY ?
                                  "MIDDAY HALT" :
                              journey_stop == CC_JOURNEY_STOP_OVERNIGHT ?
                                  "EVENING STOP" :
                              "ON THE ROAD",
                              content_x, 91, 9, TEAL);
            CcOverlayDrawText(destination != NULL ? destination->name : "THE FAR GATE",
                     content_x, 112, 15, INK);
            DrawBar(content_x, 145, 74, "PROGRESS", progress, TEAL);
            if (sim->journey.active) {
                int32_t eta_hours =
                    (CcSimJourneyEtaMinutes(sim) + 59) / 60;
                int32_t clock_minutes =
                    sim->clock.minute_subticks /
                        CC_WORLD_MINUTE_SUBTICKS + 6 * 60;
                CcOverlayDrawText(
                    TextFormat("DAY %d  %02d:%02d  /  WATCH %d OF %d",
                               sim->current_day,
                               (clock_minutes / 60) % 24,
                               clock_minutes % 60,
                               CcSimJourneyWatchNumber(sim),
                               CcSimJourneyWatchCount(sim)),
                    content_x, 170, 8, CC_GOLD);
                CcOverlayDrawText(
                    TextFormat("TEAM %d%%  /  WAGON %d%%  /  ETA %dH",
                               CcSimHorseTeamReadiness(sim),
                               sim->carriage.condition, eta_hours),
                    content_x, 189, 8, INK);
                const CcBanditGroup *warned_bandits =
                    sim->journey.ambush_warned &&
                    sim->journey.ambush_pending ?
                        CcSimBanditGroupOnRoute(
                            sim, sim->journey.route_id) : NULL;
                CcOverlayDrawText(
                    sim->journey.ambush_warned &&
                    sim->journey.ambush_pending ?
                        TextFormat("SCOUTS: %.24s RIDERS", warned_bandits != NULL ?
                            warned_bandits->name : "ARMED") :
                    journey_stop == CC_JOURNEY_STOP_OVERNIGHT &&
                    CcSimJourneyRoadHouseAvailable(sim) ?
                        TextFormat("%s · %d MILES OUT",
                                   CcSimRoadHouseName(
                                       sim, sim->journey.route_id),
                                   CcSimRoadHouseDistanceMiles(
                                       sim, sim->journey.route_id)) :
                        JourneyRoadBeatName(sim),
                    content_x, 208, 7,
                    sim->journey.ambush_warned &&
                    sim->journey.ambush_pending ? DANGER : TEAL);
                if (sim->journey.ambush_warned &&
                    sim->journey.ambush_pending && warned_bandits != NULL) {
                    CcGood demanded_good = CC_GOOD_FOOD;
                    int32_t demanded_quantity = 0;
                    bool demand = CcSimBanditProvisionDemand(
                        sim, sim->journey.route_id,
                        &demanded_good, &demanded_quantity);
                    CcOverlayDrawText(demand ?
                        TextFormat("POSSIBLE TOLL: %d %s OR %d CROWNS",
                            demanded_quantity, CcGoodName(demanded_good),
                            sim->journey.bargain_cost) :
                        TextFormat("POSSIBLE TOLL: %d CROWNS",
                            sim->journey.bargain_cost),
                        content_x, 223, 7, DANGER);
                }
                CcOverlayDrawText(
                    journey_stop == CC_JOURNEY_STOP_MIDDAY ?
                        "ENTER BREAK  /  P PRESS ON" :
                    journey_stop == CC_JOURNEY_STOP_OVERNIGHT ?
                        CcSimJourneyRoadHouseAvailable(sim) ?
                            "ENTER CAMP  /  L LODGE" :
                            "ENTER MAKE CAMP" :
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

static void ReleaseMapPageTextures(ClientMapTextures *textures)
{
    if (textures == NULL) return;
    ReleaseMapTexture(&textures->illustrated,
                      &textures->illustrated_attempted);
    ReleaseMapTexture(&textures->collectible_atlas,
                      &textures->collectible_atlas_attempted);
}

static void ReleaseMapTextures(ClientMapTextures *textures)
{
    if (textures == NULL) return;
    ReleaseMapPageTextures(textures);
    ReleaseMapTexture(&textures->economic_goods,
                      &textures->economic_goods_attempted);
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
        TextFormat("STRENGTH %d   CULT %d humans / %d goblins   CROWN %s",
                   CcSimDragonBattleStrength(sim), CcSimCultMembers(sim, CC_CULT_HUMAN),
                   CcSimCultMembers(sim, CC_CULT_GOBLIN), CcGoblinColorName(sim->goblin_politics.crown_faction)),
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
                  265.0f + (float)room->map_y * 38.0f};
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

    CcStoryUnderroadExcerpt excerpt = CcStoryUnderroadCurrentExcerpt(sim);
    if (excerpt.line_count > 0U) {
        CcOverlayDrawText(excerpt.title, 66, 420, 12, CC_GOLD);
        for (size_t i = 0U; i < excerpt.line_count; ++i) {
            CcOverlayDrawText(excerpt.lines[i], 66, 443 + (int)i * 17, 10, INK);
        }
        CcOverlayDrawText(excerpt.searched ? "Search fragment recovered." :
            "Search chamber to read the hidden fragment; normal turn and noise costs apply.",
            66, 549, 9, MUTED);
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
        "Each move or search spends a turn. Six turns consume Bread or Meat and a day.",
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
    if (player->combat.queued_skill == (int32_t)skill) return "QUEUED";
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
    if (set != NULL) set->combat = true;
    int32_t target = SelectedCombatTargetIndex(local);
    bool has_target = target >= 0;
    const CcCombatState *combat = &local->agent.combat;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        bool available = CcLocalCourseCanPlayerEngage(
            &local->course, &local->agent, i);
        int32_t previous_count = set != NULL ? set->count : 0;
        AddDetailedContextAction(
            set, CONTEXT_ACTION_SELECT_TARGET,
            local->course.raider_names[i], i == target ? "TARGET" : "",
            available ? CcLocalRaiderRoleName(local->course.raider_roles[i]) :
                local->course.raiders[i].combat.life_state == CC_LIFE_ALIVE ?
                    "MOVE CLOSER" : "DOWN",
            available, i == target);
        if (set != NULL && set->count > previous_count) {
            set->items[set->count - 1].amount = i;
        }
    }

    {
        AddDetailedContextAction(
            set, CONTEXT_ACTION_BASIC_STRIKE, "Attack", "SPACE",
            has_target ? local->course.raider_names[target] : "CHOOSE TARGET",
            has_target, false);
        AddDetailedContextAction(
            set, CONTEXT_ACTION_TOGGLE_GUARD, "Guard", "X",
            local->agent.humanoid.guard_requested ?
                "GUARD UP" : "GUARD DOWN",
            has_target, local->agent.humanoid.guard_requested);
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

/* A parked company only recovers when a day turns, so the town card is the
   one place the team can rest between journeys. */
static void AddRestTeamAction(ContextActionSet *set, const CcSim *sim)
{
    if (sim == NULL || sim->journey.active ||
        sim->dungeon_expedition.active) return;
    AddDetailedContextAction(
        set, CONTEXT_ACTION_REST_TEAM, "Rest the team", "R",
        "ONE DAY / NORMAL UPKEEP",
        true, CcSimHorseTeamReadiness(sim) < 30);
}

static bool NearParkedCarriage(const CcSim *sim, const LocalState *local)
{
    if (sim == NULL || local == NULL || sim->journey.active ||
        local->journey_travel_active || local->market_interior ||
        local->site_kind != CC_LOCAL_SITE_NONE ||
        sim->carriage.mode != CC_CARRIAGE_PARKED ||
        sim->carriage.location_id != sim->player.location_id) return false;
    if (local->open_world) {
        if (!local->world_carriage.visible) return false;
        float dx = local->agent.position.x - local->world_carriage.position.x;
        float dz = local->agent.position.z - local->world_carriage.position.z;
        return dx * dx + dz * dz < 16.0f;
    }
    Vector2 position = LocalPosition(local);
    return GridDistance(position, LOCAL_CARRIAGE_BAY) < 1.85f ||
        GridDistance(position, LOCAL_CARRIAGE) < 1.85f;
}

static const char *HorseCareSourceLabel(CcHorseCareSource source)
{
    switch (source) {
        case CC_HORSE_CARE_TRAY: return "feed tray";
        case CC_HORSE_CARE_CARGO: return "carriage cargo";
        case CC_HORSE_CARE_MARKET: return "stable market";
        default: return "none";
    }
}

static void AddHorseCareAction(ContextActionSet *set, const CcSim *sim)
{
    CcHorseCarePreview offer;
    if (!CcSimHorseCarePreview(sim, &offer)) return;
    char label[64] = "Care for horses";
    char detail[48];
    if (offer.available) {
        const char *source = offer.source == CC_HORSE_CARE_TRAY ? "tray" :
            offer.source == CC_HORSE_CARE_CARGO ? "cargo" : "market";
        (void)snprintf(label, sizeof(label), "Care for horses: %lld crowns",
                       (long long)offer.cost);
        (void)snprintf(detail, sizeof(detail),
            "1 Wheat from %s / 1 day%s", source,
            offer.weekly_feed_due ? " + upkeep" : "");
    } else if (offer.source == CC_HORSE_CARE_NO_FEED &&
               strstr(offer.reason, "1 Wheat") != NULL) {
        (void)snprintf(detail, sizeof(detail),
                       "Bring 1 Wheat in tray, cargo, or market.");
    } else {
        (void)snprintf(detail, sizeof(detail), "%s", offer.reason);
    }
    char hint[16];
    (void)snprintf(hint, sizeof(hint), "Shift+%c",
        adventure_preferences != NULL ?
            adventure_preferences->key_interact : KEY_F);
    AddDetailedContextAction(set, CONTEXT_ACTION_CARE_HORSES,
        label, hint, detail, offer.available, false);
}

static const CcSituation *AdventureDeliveryAtHand(const CcSim *sim)
{
    const CcSituation *promise = CcSimAcceptedSituation(sim);
    if (promise == NULL ||
        (promise->kind != CC_SITUATION_RELIEF_DELIVERY &&
         promise->kind != CC_SITUATION_BLACK_MARKET_DELIVERY) ||
        promise->target_id != sim->player.location_id ||
        sim->carriage.location_id != sim->player.location_id ||
        promise->good < 0 || promise->good >= CC_GOOD_COUNT) return NULL;
    int32_t remaining = promise->quantity - promise->progress;
    return remaining > 0 && sim->player.cargo[promise->good] >= remaining ? promise : NULL;
}

static bool AdventureHandoffTarget(const CcSim *sim, const LocalState *local,
                                   const CcInteractionTarget *target)
{
    return target != NULL && AdventureDeliveryAtHand(sim) != NULL &&
        target->key.kind == (local->market_interior ? CC_INTERACTION_COUNTER : CC_INTERACTION_DOOR);
}

static int AdventurePriorityRank(const CcSim *sim, const LocalState *local,
                                 const CcInteractionTarget *target)
{
    if (AdventureHandoffTarget(sim, local, target)) return 3;
    if (target != NULL && target->key.kind == CC_INTERACTION_PERSON &&
        local->course.situation_witness_active &&
        target->character_id == local->course.situation_witness_character_id)
        return 2;
    if (target != NULL &&
        target->key.kind == (local->market_interior ?
            CC_INTERACTION_COUNTER : CC_INTERACTION_DOOR)) {
        const CcSituation *promise = CcSimAcceptedSituation(sim);
        bool partial_delivery_at_destination = promise != NULL &&
            (promise->kind == CC_SITUATION_RELIEF_DELIVERY ||
             promise->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) &&
            promise->target_id == sim->player.location_id &&
            sim->carriage.location_id == sim->player.location_id;
        return partial_delivery_at_destination ? 0 : 1;
    }
    return 0;
}

static bool FirstDeliveryComplete(const CcSim *sim)
{
    return sim != NULL && sim->player.reputation > 0;
}

static ContextActionSet BuildContextActions(
    const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation)
{
    ContextActionSet set = {0};
    if (sim == NULL || local == NULL) return set;
    if (sim->mine.phase != CC_MINE_NONE) return set;
    if (local->adventure_ui && (view == VIEW_TRADE || view == VIEW_PAUSE || view == VIEW_LEDGER || view == VIEW_OVEN_COURT)) return set;
    int32_t pony = CcPonyOnRoad(sim);
    if (view == VIEW_LOCAL && pony >= 0 && !LocalCombatActive(local)) {
        if (sim->pony_company.encounter < 0) {
            AddDetailedContextAction(&set, CONTEXT_ACTION_PONY_MEET,
                sim->pony_company.ponies[pony].seen ? TextFormat("Talk to %s", CcPonyName(pony)) : "Meet a rainbow pony",
                "", "Stop for a conversation", true, false);
        } else if (sim->pony_company.ponies[pony].ready) {
            for (int32_t seat = 0; seat < CcSimHorseTeamCount(sim); ++seat) {
                AddDetailedContextAction(&set, CONTEXT_ACTION_PONY_SWAP,
                    TextFormat("Release %s", CcPonyName(CcSimTeamPony(sim, seat))), "",
                    TextFormat("Invite %s", CcPonyName(pony)), true, false);
                set.items[set.count - 1].amount = seat;
            }
        } else {
            AddDetailedContextAction(&set, CONTEXT_ACTION_PONY_HELP,
                TextFormat("Help %s", CcPonyName(pony)), "ENTER", "Earn this pony's trust", true, false);
        }
        if (sim->pony_company.encounter >= 0) {
            AddDetailedContextAction(&set, CONTEXT_ACTION_PONY_LEAVE, "Say farewell", "BKSP",
                "Return to the road", true, false);
            return set;
        }
    }
    if (view == VIEW_LOCAL && AdventureScene(local) && !LocalCombatActive(local)) {
        const CcSituation *relief = CcSimAcceptedSituation(sim);
        if (relief != NULL && relief->kind == CC_SITUATION_RELIEF_DELIVERY &&
            sim->player.location_id == CcSimSituationOfferSettlementId(sim, relief)) {
            Vector2 position = LocalPosition(local);
            if (!relief->loading_crate_carried &&
                CcSimReliefCratesToLoad(relief) > 0) {
                bool near = GridDistance(position, LOCAL_RELIEF_CRATES) < 2.1f;
                AddDetailedContextAction(&set, near ?
                    CONTEXT_ACTION_PICKUP_RELIEF_CRATE :
                    CONTEXT_ACTION_APPROACH_RELIEF_CRATES,
                    near ? "Lift one relief crate" : "Walk to granary stack",
                    near ? TextFormat("%c", adventure_preferences != NULL ?
                        adventure_preferences->key_interact : KEY_F) : "",
                    TextFormat("%d STILL AT GRANARY", CcSimReliefCratesToLoad(relief)),
                    true, false);
            } else if (relief->loading_crate_carried) {
                bool near = GridDistance(position, LOCAL_CARRIAGE_BAY) < 1.85f ||
                    GridDistance(position, LOCAL_CARRIAGE) < 1.85f;
                AddDetailedContextAction(&set, near ?
                    CONTEXT_ACTION_STOW_RELIEF_CRATE :
                    CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE,
                    near ? "Place crate in carriage" : "Walk to carriage",
                    near ? TextFormat("%c", adventure_preferences != NULL ?
                        adventure_preferences->key_interact : KEY_F) : "",
                    TextFormat("%d OF %d ABOARD", relief->loading_progress,
                        relief->quantity), true, false);
            }
        }
        for (int32_t i = 0; i < local->interactions.count; ++i) {
            const CcInteractionTarget *target = &local->interactions.targets[i];
            if (target->key.kind == CC_INTERACTION_ACTION) continue;
            bool active = CcInteractionKeyEqual(target->key, local->interaction.approaching ?
                local->interaction.pending : local->interaction.focus);
            AddDetailedContextAction(&set, CONTEXT_ACTION_WORLD_TARGET, target->name,
                active ? "F" : "", target->available ? target->verb : target->reason,
                target->available, active);
            set.items[set.count - 1].target = target->key;
            int32_t at = set.count - 1;
            float distance = GridDistance(LocalPosition(local),
                (Vector2){target->approach_x, target->approach_z});
            while (at > 0) {
                const CcInteractionTarget *previous = CcInteractionFind(&local->interactions,
                    set.items[at - 1].target);
                int priority = AdventurePriorityRank(sim, local, target);
                int previous_priority = AdventurePriorityRank(sim, local,
                                                               previous);
                float previous_distance = previous != NULL ?
                    GridDistance(LocalPosition(local),
                        (Vector2){previous->approach_x, previous->approach_z}) : 0.0f;
                if (previous == NULL || previous_priority > priority ||
                    (previous_priority == priority && (priority > 0 ||
                        previous_distance <= distance))) break;
                ContextAction swap = set.items[at - 1];
                set.items[at - 1] = set.items[at]; set.items[at] = swap;
                --at;
            }
        }
        const CcSettlement *place = CcSimSettlement(
            sim, sim->player.location_id);
        if (!local->market_interior &&
            local->site_kind == CC_LOCAL_SITE_NONE &&
            !sim->journey.active && place != NULL && place->population <= 0 &&
            OutgoingRouteCount(sim) > 0) {
            bool passable = false;
            char reason[192] = "";
            for (int32_t i = 0; i < sim->route_count; ++i) {
                const CcRoute *route = SelectedOutgoingRoute(sim, i);
                if (route == NULL) continue;
                char option_reason[192] = "";
                CcTravelPreview option = {0};
                if (CcSimTravelPreview(sim,
                        RouteOtherEnd(route, sim->player.location_id),
                        &option, option_reason, sizeof(option_reason))) {
                    passable = true;
                    break;
                }
                if (reason[0] == '\0')
                    (void)snprintf(reason, sizeof(reason), "%s", option_reason);
            }
            AddDetailedContextAction(&set, CONTEXT_ACTION_CHOOSE_ROAD,
                "Choose a road", "", passable ? "OPEN THE DEPARTURE ROAD" :
                reason, passable, false);
        }
        if (NearParkedCarriage(sim, local)) AddHorseCareAction(&set, sim);
        if (local->world_cards_presented && (local->interaction.approaching ||
            GridDistance(LocalPosition(local), local->presented_card_origin) <= 3.0f)) {
            ContextActionSet steady = {0};
            bool care_present = false;
            for (int i = 0; i < set.count; ++i)
                if (set.items[i].kind == CONTEXT_ACTION_CARE_HORSES)
                    care_present = true;
            /* Keep the current relief step visible while nearby town cards change. */
            for (int pass = 0; pass < 4; ++pass) {
                int count = pass == 1 ? local->presented_target_count : set.count;
                int limit = pass < 2 && care_present ? 3 : 4;
                for (int i = 0; i < count && steady.count < limit; ++i) {
                    int candidate = i;
                    if (pass == 1) {
                        candidate = -1;
                        for (int j = 0; j < set.count; ++j)
                            if (CcInteractionKeyEqual(set.items[j].target, local->presented_targets[i])) {
                                const CcInteractionTarget *target = CcInteractionFind(
                                    &local->interactions, set.items[j].target);
                                if (target != NULL && target->character_id == local->presented_target_characters[i])
                                    candidate = j;
                                break;
                            }
                    }
                    if (candidate < 0) continue;
                    const ContextAction *action = &set.items[candidate];
                    if (pass == 0 &&
                        action->kind != CONTEXT_ACTION_APPROACH_RELIEF_CRATES &&
                        action->kind != CONTEXT_ACTION_PICKUP_RELIEF_CRATE &&
                        action->kind != CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE &&
                        action->kind != CONTEXT_ACTION_STOW_RELIEF_CRATE &&
                        AdventurePriorityRank(sim, local,
                            CcInteractionFind(&local->interactions,
                                              action->target)) == 0) continue;
                    if (pass == 2 && action->kind != CONTEXT_ACTION_CARE_HORSES)
                        continue;
                    bool included = false;
                    for (int j = 0; j < steady.count; ++j)
                        if (steady.items[j].kind == action->kind &&
                            CcInteractionKeyEqual(steady.items[j].target,
                                                  action->target)) included = true;
                    if (!included) steady.items[steady.count++] = *action;
                }
            }
            set = steady;
        }
        if (set.count > 4) {
            for (int i = 4; i < set.count; ++i)
                if (set.items[i].kind == CONTEXT_ACTION_CARE_HORSES) {
                    set.items[3] = set.items[i];
                    break;
                }
            set.count = 4;
        }
        return set;
    }
    if (local->adventure_ui && view == VIEW_CHARACTER && local->conversation_situation_id == 0U) {
        if (CcOvenCourtCanDiscuss(sim, local->conversation_character_id))
            AddDetailedContextAction(&set, CONTEXT_ACTION_OVEN_QUESTION,
                "What do the ovens need?", TextFormat("%d", set.count + 1),
                "ASK ABOUT THE LOCAL TALLY", true, false);
        const CcCharacter *contact=CcSimMineEvidenceContact(sim);
        bool jory=contact != NULL && contact->id == local->conversation_character_id;
        if (jory && sim->mine.lead_event_id == 0U && CcSimMineLeadSupported(sim))
            AddDetailedContextAction(&set,CONTEXT_ACTION_MINE_LEAD,
                "Ask about Low Silver Pit",TextFormat("%d",set.count+1),
                "TURNOUT + WORKERS' RECORDS",true,false);
        if (jory && sim->mine.report_event_id == 0U) {
            CcMineReturnKind evidence=CcSimMineReturnEvidence(sim);
            CcGood haul_good=CC_GOOD_BREAD;
            int32_t haul_quantity=0;
            bool haul=CcSimMineReturnHaul(sim,&haul_good,&haul_quantity);
            AddDetailedContextAction(&set,CONTEXT_ACTION_MINE_REPORT,
                "Tell Jory what the company found",TextFormat("%d",set.count+1),
                evidence == CC_MINE_RETURN_HAUL && haul ?
                    TextFormat("%d %s FROM MINE",haul_quantity,CcGoodName(haul_good)) :
                evidence == CC_MINE_RETURN_INFORMATION ? "ATTRIBUTED ROUTE ACCOUNT" :
                "BRING A LOAD OR ROUTE OBSERVATION",
                evidence != CC_MINE_RETURN_NONE,false);
        }
        AddDetailedContextAction(&set, CONTEXT_ACTION_GOSSIP_CHAT,
            "Chat", TextFormat("%d",set.count+1), "",
            !core_conversation.pending && core_conversation.round_phase == 0U,
            false);
        AddDetailedContextAction(&set, CONTEXT_ACTION_CLOSE_VIEW, "Farewell", "ESC", "", true, false);
        return set;
    }
    if (view == VIEW_ENCOUNTER) {
        AddDetailedContextAction(&set, CONTEXT_ACTION_FIGHT,
                                 "Draw steel", "1",
                                 "ENTER COMBAT", true, false);
        AddDetailedContextAction(&set, CONTEXT_ACTION_PAY,
                                 "Hear them out", "2",
                                 "ENTER PARLEY", true, false);
        const CcSettlement *origin = CcSimSettlement(
            sim, sim->journey.origin_id);
        int32_t return_minutes = CcSimJourneyWithdrawalMinutes(sim);
        AddDetailedContextAction(&set, CONTEXT_ACTION_WITHDRAW,
                                 TextFormat("Withdraw to %.16s: 0 crowns",
                                     origin != NULL ? origin->name : "origin"),
                                 "3", TextFormat(
                                     "RETURN %dH %02dM / ROAD SECURITY MAY FALL",
                                     return_minutes / 60, return_minutes % 60),
                                 true, false);
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
            "BKSP", room->depth == 0 ? "SAFE WITHDRAWAL" :
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
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Return to goblin chimney", "BKSP",
            sim->dragon.slain ? "LEAVE THE ASHEN ROOST" :
                                "THE ONLY EXIT", true, false);
        return set;
    }
    if (view == VIEW_LEDGER) {
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Close ledger", "BKSP",
            "RETURN TO PREVIOUS VIEW", true, false);
        return set;
    }
    if (view == VIEW_SITUATIONS) {
        const CcSituation *detail = SelectedActiveSituation(
            sim, selected_situation);
        if (detail != NULL &&
            detail->id == sim->player.accepted_situation_id) {
            AddContextAction(&set, CONTEXT_ACTION_ABANDON_PROMISE,
                             "Leave promise");
        }
        if (ActiveSituationCount(sim) > 1) {
            AddContextAction(&set, CONTEXT_ACTION_NEXT_PROMISE,
                             "Next notice");
        }
        if (sim->dungeon_count > 0 &&
            sim->player.location_id == sim->dungeons[0].settlement_id &&
            sim->mine.lead_event_id == 0U && !CcSimMineLeadSupported(sim)) {
            AddDetailedContextAction(&set,CONTEXT_ACTION_MINE_SHIFT_RECORD,
                "Read Low Silver Pit shift record","R",
                "DATED TURNOUT DOCUMENT",true,false);
        }
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Close notices", "ESC",
            "RETURN TO PREVIOUS VIEW", true, false);
        return set;
    }
    if (view == VIEW_CARRIAGE) {
        if (local->carriage_inspection_road) {
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_CLOSE_VIEW, "Return to road", "BKSP",
                "KEEP THIS JOURNEY IN PLACE", true, false);
            return set;
        }
        bool away_from_town = local->site_kind != CC_LOCAL_SITE_NONE;
        AddDetailedContextAction(
            &set,
            away_from_town ? CONTEXT_ACTION_RETURN_FROM_SITE :
                             CONTEXT_ACTION_CHOOSE_ROAD,
            away_from_town ? "Travel back to town" : "Travel",
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
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Step away", "BKSP",
            "RETURN TO THE STREET", true, false);
        return set;
    }
    if (view == VIEW_CHARACTER) {
        const CcSituation *situation = CcSimSituation(
            sim, local->conversation_situation_id);
        const CcCharacter *character = CcSimCharacter(
            sim, local->conversation_character_id);
        if (CcOvenCourtCanDiscuss(sim, local->conversation_character_id))
            AddDetailedContextAction(&set, CONTEXT_ACTION_OVEN_QUESTION,
                "What do the ovens need?", TextFormat("%d", set.count + 1),
                "ASK ABOUT THE LOCAL TALLY", true, false);
        const CcCharacter *contact=CcSimMineEvidenceContact(sim);
        bool jory=character != NULL && contact != NULL && character->id == contact->id;
        if (jory && sim->mine.lead_event_id == 0U && CcSimMineLeadSupported(sim))
            AddDetailedContextAction(&set,CONTEXT_ACTION_MINE_LEAD,
                "Ask about Low Silver Pit",TextFormat("%d",set.count+1),
                "TURNOUT + WORKERS' RECORDS",true,false);
        if (jory && sim->mine.report_event_id == 0U) {
            CcMineReturnKind evidence=CcSimMineReturnEvidence(sim);
            CcGood haul_good=CC_GOOD_BREAD;
            int32_t haul_quantity=0;
            bool haul=CcSimMineReturnHaul(sim,&haul_good,&haul_quantity);
            AddDetailedContextAction(&set,CONTEXT_ACTION_MINE_REPORT,
                "Tell Jory what the company found",TextFormat("%d",set.count+1),
                evidence == CC_MINE_RETURN_HAUL && haul ?
                    TextFormat("%d %s FROM MINE",haul_quantity,CcGoodName(haul_good)) :
                evidence == CC_MINE_RETURN_INFORMATION ? "ATTRIBUTED ROUTE ACCOUNT" :
                "BRING A LOAD OR ROUTE OBSERVATION",
                evidence != CC_MINE_RETURN_NONE,false);
        }
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
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Close map case", "BKSP",
            "RETURN TO THE CARRIAGE", true, false);
        return set;
    }
    if (view == VIEW_ROADS) {
        if (RoadBookDepartureInProgress(local)) {
            AddDetailedContextAction(&set, CONTEXT_ACTION_NONE,
                "Leaving town", "", "FOLLOWING THE DEPARTURE ROAD", false, false);
            return set;
        }
        for (int32_t i = 0; i < sim->route_count; ++i) {
            const CcRoute *route = SelectedOutgoingRoute(sim, i);
            if (route == NULL) continue;
            char label[64];
            char detail[64];
            RoadChoiceLabel(sim, route, label, sizeof(label));
            CcTravelPreview preview = {0};
            char reason[192] = "";
            bool available = CcSimTravelPreview(sim,
                RouteOtherEnd(route, sim->player.location_id),
                &preview, reason, sizeof(reason));
            if (available) {
                (void)snprintf(detail, sizeof(detail),
                    "%d HOURS ON THE ROAD", preview.travel_watches * 8);
            } else {
                (void)snprintf(detail, sizeof(detail), "%s", reason);
            }
            AddDetailedContextAction(&set, CONTEXT_ACTION_TRAVEL,
                label, i == selected ? "ENTER" : "", detail, available,
                i == selected);
            set.items[set.count - 1].amount = i;
            set.items[set.count - 1].target = (CcInteractionKey){sim->player.location_id, route->id, CC_INTERACTION_ACTION};
        }
        AddDetailedContextAction(
            &set, CONTEXT_ACTION_CLOSE_VIEW, "Return to town", "BKSP",
            "RETURN THROUGH THE GATE", true, false);
        return set;
    }

    if (local->road_choice_active || local->site_travel_active) {
        AddDetailedContextAction(&set, CONTEXT_ACTION_HOLD_TRAVEL,
            local->carriage_stopped ? "Travel" : "Stop", "SPACE",
            TravelActionDetail(sim, local), true, false);
        return set;
    }

    if (local->journey_travel_active) {
        if (sim->journey.road_position_active &&
            (sim->journey.road_waiting_choice ||
             (local->carriage_stopped && local->road_actions_expanded))) {
            CcRoadLegPreview previews[3];
            int32_t preview_count = CcRoadNextLegPreviews(
                sim, previews, 3);
            for (int32_t i = 0; i < preview_count; ++i) {
                const CcSettlement *town = CcSimSettlement(
                    sim, previews[i].destination_anchor_id);
                const CcRoadSite *site = CcSimRoadSite(
                    sim, previews[i].destination_anchor_id);
                const CcRoute *current_route = CcSimRoute(sim, sim->journey.route_id);
                const CcSettlement *toward = current_route != NULL ? CcSimSettlement(sim,
                    previews[i].direction > 0 ? current_route->to_id : current_route->from_id) : NULL;
                const char *name = town != NULL ? town->name :
                    site != NULL ? site->name :
                    previews[i].destination_anchor_id ==
                        CC_PILOT_ROAD_JUNCTION_ID ? "Stag's Mill junction" :
                    previews[i].destination_anchor_id ==
                        CC_PILOT_ROAD_CHECKPOINT_ID && toward != NULL ? toward->name :
                        "the next road marker";
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_CHOOSE_ROAD_LEG,
                    sim->journey.road_waiting_choice ?
                        TextFormat("Drive to %s", name) :
                        TextFormat("Turn back to %s", name),
                    TextFormat("%d", set.count + 1),
                    TextFormat("%d UNITS / ABOUT %d MIN",
                        previews[i].length_units,
                        previews[i].travel_subticks /
                            CC_WORLD_MINUTE_SUBTICKS),
                    true, false);
                set.items[set.count - 1].target = (CcInteractionKey){
                    sim->journey.road_anchor_id,
                    previews[i].decision_token, CC_INTERACTION_ACTION};
            }
            if (sim->journey.road_waiting_choice) return set;
        }
        const CcRoadSite *road_stop = CcSimJourneyRequiresRoadChoice(sim) ?
            CcSimJourneyRoadSiteStop(sim) : NULL;
        if (road_stop == NULL && sim->journey.active) {
            if (!local->open_world || local->world_carriage.hero_embarked) {
                if (!local->carriage_stopped || !local->road_actions_expanded) {
                    AddDetailedContextAction(&set, CONTEXT_ACTION_HOLD_TRAVEL,
                        local->carriage_stopped ? "Travel" : "Stop", "SPACE",
                        TravelActionDetail(sim, local), true, false);
                    AddDetailedContextAction(&set, CONTEXT_ACTION_INSPECT_CARRIAGE,
                        "Inspect carriage", "F", "TEAM AND CARRIED GOODS", true, false);
                }
                if (local->carriage_stopped) {
                    AddDetailedContextAction(&set, CONTEXT_ACTION_ROAD_OPTIONS,
                        local->road_actions_expanded ? "Back to road" : "Road options", "",
                        "STEP DOWN / CAMP / TURN BACK", true, false);
                    if (local->road_actions_expanded) {
                        if (local->open_world) AddDetailedContextAction(&set,
                            CONTEXT_ACTION_STEP_DOWN, "Step down", "", "WALK THE ROADSIDE", true, false);
                        if (CcSimJourneyCanCampOnRoad(sim)) AddDetailedContextAction(&set,
                            CONTEXT_ACTION_MAKE_ROAD_CAMP, "Camp until morning", "",
                            "ADVANCES TIME / USES PROVISIONS", true, false);
                    }
                }
            } else {
                AddDetailedContextAction(&set, CONTEXT_ACTION_BOARD_CARRIAGE,
                    "Board carriage", "F", "RETURN TO YOUR WAITING TEAM", true, false);
            }
            return set;
        }
        if (road_stop != NULL && !local->road_actions_expanded) {
            bool mine = road_stop == CcMineSite(sim) && CcMineBranchSubtick(sim) >= 0 &&
                sim->journey.elapsed_subticks >= CcMineBranchSubtick(sim);
            if (mine) {
                AddDetailedContextAction(&set, CONTEXT_ACTION_VISIT_MINE,
                    "Enter Low Silver Pit", "", FirstDeliveryComplete(sim) ?
                    "PARK IN THE MINE YARD" : "AFTER YOUR FIRST DELIVERY", FirstDeliveryComplete(sim), false);
            }
            AddDetailedContextAction(&set, CONTEXT_ACTION_PASS_ROAD_SITE,
                "Travel on", "", TextFormat("PASS %.28s", road_stop->name), true, false);
            set.items[set.count - 1].target = (CcInteractionKey){
                sim->player.location_id, road_stop->id, CC_INTERACTION_ACTION};
            AddDetailedContextAction(&set, CONTEXT_ACTION_ROAD_OPTIONS,
                "Site options", "", "INSPECT / CLEAR / STORE GOODS", true, false);
            return set;
        }
        if (road_stop != NULL) AddDetailedContextAction(&set, CONTEXT_ACTION_ROAD_OPTIONS,
            "Back to road", "", "RETURN TO THE TURN-OFF CHOICE", true, false);
        if (road_stop != NULL && !road_stop->accessible) {
            bool tree = road_stop->blocker == CC_ROAD_SITE_BLOCKER_TREE;
            AddDetailedContextAction(&set, CONTEXT_ACTION_CLEAR_ROAD_SITE,
                tree ? "Clear fallen tree" : "Clear rocks", "",
                tree ? "1 TOOL / USE 1 WOOD / 1 WATCH" : "USE 2 TOOLS / 2 WATCHES",
                sim->player.cargo[CC_GOOD_TOOLS] >= (tree ? 1 : 2) &&
                (!tree || sim->player.cargo[CC_GOOD_WOOD] >= 1), false);
            set.items[set.count - 1].target = (CcInteractionKey){
                sim->player.location_id, road_stop->id, CC_INTERACTION_ACTION};
        }
        if (road_stop != NULL && road_stop->accessible) {
            if (road_stop->condition < 100) {
                CcProductionReceipt repair = CcSimPlanRoadSiteRepair(sim, road_stop->id);
                AddDetailedContextAction(&set, CONTEXT_ACTION_REPAIR_ROAD_SITE,
                    "Repair site", "", "2 TOOLS / USE 1 TOOL + 1 WOOD / 2 WATCHES",
                    repair.gate == CC_PRODUCTION_READY, false);
                set.items[set.count - 1].target = (CcInteractionKey){
                    sim->player.location_id, road_stop->id, CC_INTERACTION_ACTION};
            }
            if (FirstDeliveryComplete(sim)) {
                for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                    for (int32_t direction = -1; direction <= 1; direction += 2) {
                        int32_t held = direction > 0 ? sim->player.cargo[good] : road_stop->stock[good];
                        if (held <= 0) continue;
                        AddDetailedContextAction(&set, CONTEXT_ACTION_TRANSFER_ROAD_SITE,
                            TextFormat("%s 1 %s", direction > 0 ? "Unload" : "Load", CcGoodName((CcGood)good)), "",
                            TextFormat("STORE %d / CARRIAGE %d", road_stop->stock[good], sim->player.cargo[good]), true, false);
                        ContextAction *action = &set.items[set.count - 1];
                        action->good = (CcGood)good;
                        action->amount = direction;
                        action->target = (CcInteractionKey){sim->player.location_id, road_stop->id, CC_INTERACTION_ACTION};
                    }
                }
            }
        }
        /* The stop window holds the carriage for many subticks, but the turn
           used to be offered on exactly one of them. Everywhere else in the
           window the company fell through to "Camp at ..." -- which spends the
           stop and takes the branch with it. Reaching the branch is enough. */
        if (road_stop != NULL && road_stop == CcMineSite(sim) &&
            CcMineBranchSubtick(sim) >= 0 &&
            sim->journey.elapsed_subticks >= CcMineBranchSubtick(sim)) {
            const CcRoute *route=CcSimRoute(sim,road_stop->route_id);
            if (FirstDeliveryComplete(sim)) {
                bool right=route != NULL && (road_stop->side > 0) == (sim->journey.origin_id == route->from_id);
                AddDetailedContextAction(&set,CONTEXT_ACTION_VISIT_MINE,
                    right ? "Turn right to Low Silver Pit" : "Turn left to Low Silver Pit", "",
                    "MINE YARD / PARK AND WALK",true,false);
                AddDetailedContextAction(&set,CONTEXT_ACTION_PASS_ROAD_SITE,
                    "Travel on", "", "PASS THE MINE BRANCH",true,false);
            } else {
                AddDetailedContextAction(&set, CONTEXT_ACTION_VISIT_MINE,
                    "Low Silver Pit", "",
                    "AFTER YOUR FIRST DELIVERY",false,false);
                AddDetailedContextAction(&set,CONTEXT_ACTION_PASS_ROAD_SITE,
                    "Travel on", "", "PASS THE MINE BRANCH",true,false);
            }
            return set;
        }
        if (road_stop != NULL) {
            /* Every roadside place is a turn-off, not a bed: the company either
               has business here or drives on. Camping used to live on this
               stop, which spent it -- resting is now a decision about the hour
               and belongs to the road, not to the place. Continuing must stay
               offered at every site, because the carriage holds here and a site
               with nothing else to give would otherwise strand it. */
            AddDetailedContextAction(&set, CONTEXT_ACTION_PASS_ROAD_SITE,
                "Travel on", "",
                TextFormat("STAY ON THE ROAD PAST %.20s", road_stop->name),
                true, false);
            set.items[set.count - 1].target = (CcInteractionKey){sim->player.location_id, road_stop->id, CC_INTERACTION_ACTION};
        }
        bool parking = !sim->journey.active &&
            (RoadBookArrivalInProgress(local) ||
             local->convoy.phase == CC_LOCAL_CONVOY_ARRIVING);
        if (road_stop == NULL && parking) {
            AddDetailedContextAction(&set, CONTEXT_ACTION_SKIP_TRAVEL,
                "Park carriage", "ENTER", "Finish arriving", true, false);
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
        if (GridDistance(LocalPosition(local), collector) >= 1.55f) {
            AddDetailedContextAction(&set, CONTEXT_ACTION_APPROACH_COLLECTOR,
                "Approach captain", "F", "WALK TO THE BRIDGE", true,
                local->agent.exact_target_valid);
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
        if (GridDistance(position, entrance) >= 2.25f) {
            AddDetailedContextAction(&set, CONTEXT_ACTION_APPROACH_ENTRANCE,
                "Walk to the entrance", "", CcLocalSiteName(sim, local->site_kind), true, false);
        }
        if (local->site_kind != CC_LOCAL_SITE_DRAGON_CAVE) {
            AddDetailedContextAction(&set, CONTEXT_ACTION_RETURN_FROM_SITE,
                "Drive back to town", "", "Board the carriage and return", true, false);
        }
        if (local->site_kind == CC_LOCAL_SITE_DRAGON_CAVE &&
            GridDistance(position, tunnel) < 2.25f) {
            AddDetailedContextAction(
                &set, CONTEXT_ACTION_TRAVEL_GOBLIN_SITE,
                "Descend through goblin dungeon", "F",
                "RETURN THROUGH THE CAVE NETWORK", true, false);
        }
        if (GridDistance(position, entrance) < 2.25f) {
            if (local->site_kind == CC_LOCAL_SITE_DUNGEON) {
                const CcDungeon *site_dungeon = DungeonAtSettlement(
                    sim, sim->player.location_id);
                AddDetailedContextAction(
                    &set, CONTEXT_ACTION_EXPEDITION,
                    TextFormat("Enter %s",
                               site_dungeon != NULL ? site_dungeon->name :
                                   "the Underroad"), "E",
                    "NEEDS 1 BREAD OR MEAT ABOARD",
                    CcNutritionAvailable(sim->player.cargo,
                                         CC_NUTRITION_TRAVEL) >=
                        CC_NUTRITION_PER_RATION, false);
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
            if (local->adventure_ui) {
                AddDetailedContextAction(&set, CONTEXT_ACTION_OPEN_TRADE, "Trade with the keeper", "F", "BUY / SELL / DELIVER", true, false);
                return set;
            }
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
                            TextFormat("Bread boxes %dc · %d local · %d aboard",
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
            AddRestTeamAction(&set, sim);
            if (OutgoingRouteCount(sim) > 0) {
                AddContextAction(&set, CONTEXT_ACTION_CHOOSE_ROAD,
                                 "Choose a road");
            }
        }
        if (NearParkedCarriage(sim, local)) AddHorseCareAction(&set, sim);
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
    const CcSituation *relief=CcSimAcceptedSituation(sim);
    if (relief != NULL && relief->kind == CC_SITUATION_RELIEF_DELIVERY &&
        sim->player.location_id == CcSimSituationOfferSettlementId(sim,relief)) {
        if (!relief->loading_crate_carried &&
            CcSimReliefCratesToLoad(relief)>0) {
            bool near=GridDistance(position,LOCAL_RELIEF_CRATES)<2.1f;
            AddDetailedContextAction(&set,near ? CONTEXT_ACTION_PICKUP_RELIEF_CRATE :
                CONTEXT_ACTION_APPROACH_RELIEF_CRATES,
                near ? "Lift one relief crate" : "Walk to granary stack",
                near ? TextFormat("%c", adventure_preferences != NULL ?
                    adventure_preferences->key_interact : KEY_F) : "",
                TextFormat("%d STILL AT GRANARY",CcSimReliefCratesToLoad(relief)),
                true,false);
        }
        if (relief->loading_crate_carried) {
            bool near=GridDistance(position,LOCAL_CARRIAGE_BAY)<1.85f ||
                GridDistance(position,LOCAL_CARRIAGE)<1.85f;
            AddDetailedContextAction(&set,near ? CONTEXT_ACTION_STOW_RELIEF_CRATE :
                CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE,
                near ? "Place crate in carriage" : "Walk to carriage",
                near ? TextFormat("%c", adventure_preferences != NULL ?
                    adventure_preferences->key_interact : KEY_F) : "",
                TextFormat("%d OF %d ABOARD",relief->loading_progress,
                    relief->quantity),true,false);
        }
    }
    if (GridDistance(position, LOCAL_NOTICE) < 1.15f) {
        AddContextAction(&set, CONTEXT_ACTION_OPEN_PROMISES,
                         "Read board");
    }
    if (GridDistance(position, LOCAL_CARRIAGE_BAY) < 1.85f ||
        GridDistance(position, LOCAL_CARRIAGE) < 1.85f) {
        AddHorseCareAction(&set, sim);
        AddRestTeamAction(&set, sim);
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
            "Find the mine road", "F", "LOW SILVER PIT / ALDERWATCH ROAD", true, false);
    }
    return set;
}

static int ContextViewportWidth(void) { return GetScreenWidth() > 0 ? GetScreenWidth() : 1040; }
static int ContextViewportHeight(void) { return GetScreenHeight() > 0 ? GetScreenHeight() : 620; }

static int32_t ContextCardsPerPage(void)
{
    int32_t count = (ContextViewportWidth() - 128) / 184;
    return count < 1 ? 1 : count > 4 ? 4 : count;
}

static int32_t ContextActionPageSize(const ContextActionSet *actions)
{
    return actions->combat && ContextViewportWidth() >= 900 ?
        10 : ContextCardsPerPage();
}

static int32_t ContextCardFirst(const LocalState *local, const ContextActionSet *actions)
{
    int32_t per_page = ContextActionPageSize(actions);
    int32_t pages = (actions->count + per_page - 1) / per_page;
    return pages > 0 ? (local->card_page % pages) * per_page : 0;
}

static int32_t ContextCardCount(const ContextActionSet *actions, int32_t first)
{
    int32_t remaining = actions->count - first;
    int32_t per_page = ContextActionPageSize(actions);
    return remaining < per_page ? remaining : per_page;
}

static Rectangle ContextPageBounds(bool next)
{
    return (Rectangle){next ? (float)ContextViewportWidth() - 56.0f : 12.0f,
        (float)ContextViewportHeight() - 88.0f, 44.0f, 64.0f};
}

static Rectangle ContextActionBounds(int32_t index, int32_t count, bool combat)
{
    int32_t row = 0;
    if (combat && ContextViewportWidth() >= 900) {
        row = index / 5;
        index %= 5;
        count = 5;
    }
    float width = fminf(220.0f, ((float)ContextViewportWidth() - 128.0f -
        (float)(count - 1) * 8.0f) / (float)(count > 0 ? count : 1));
    float total = (float)count * width + (float)(count - 1) * 8.0f;
    return (Rectangle){((float)ContextViewportWidth() - total) * 0.5f +
        (float)index * (width + 8.0f), (float)ContextViewportHeight() - 94.0f -
            (combat && ContextViewportWidth() >= 900 ? (float)(1 - row) * 82.0f : 0.0f), width, 74.0f};
}

static ContextAction WorldContextActionAt(const CcSim *sim, const LocalState *local,
    ClientView view, const ContextActionSet *actions, Vector2 mouse);

static void UpdateTravelHold(const CcSim *sim, LocalState *local,
    ClientView view, int32_t selected, int32_t selected_situation,
    Vector2 pointer, bool down, float delta_time)
{
    (void)selected; (void)selected_situation; (void)pointer; (void)down;
    bool visible = view == VIEW_LOCAL || view == VIEW_ROADS;
    local->travel_hold_armed = visible && local->journey_travel_active &&
        !local->carriage_stopped && !TravelNeedsSlowTime(sim);
    local->travel_fast_forward = local->travel_hold_armed;
    local->travel_time_blend = CcClientTravelBlendStep(
        local->travel_time_blend, local->travel_fast_forward, delta_time);
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
    if (kind == CONTEXT_ACTION_TRAVEL ||
        kind == CONTEXT_ACTION_SKIP_TRAVEL ||
        kind == CONTEXT_ACTION_TAKE_BREAK ||
        kind == CONTEXT_ACTION_MAKE_CAMP ||
        kind == CONTEXT_ACTION_LODGE_ROAD_HOUSE ||
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

/* Remember the target and bounds the player actually saw on each card. */
static void RememberPresentedTargets(LocalState *local, ClientView view,
                                      const ContextActionSet *actions)
{
    bool changed = !local->world_cards_presented;
    int old_count = local->presented_target_count;
    local->world_cards_presented = false;
    local->presented_target_count = 0;
    if (view != VIEW_LOCAL || !AdventureScene(local) || actions->combat) return;
    local->world_cards_presented = true;
    int32_t first = ContextCardFirst(local, actions);
    int32_t shown = ContextCardCount(actions, first);
    for (int32_t i = first; i < first + shown; ++i) {
        if (actions->items[i].kind != CONTEXT_ACTION_WORLD_TARGET) continue;
        int32_t slot = local->presented_target_count;
        if (slot >= 4) break;
        if (slot >= old_count || !CcInteractionKeyEqual(local->presented_targets[slot], actions->items[i].target))
            changed = true;
        local->presented_targets[slot] = actions->items[i].target;
        const CcInteractionTarget *target = CcInteractionFind(
            &local->interactions, actions->items[i].target);
        local->presented_target_characters[slot] = target != NULL ? target->character_id : 0;
        local->presented_target_bounds[slot] = ContextActionBounds(i - first, shown, false);
        local->presented_target_count += 1;
    }
    if (changed || old_count != local->presented_target_count ||
        (!local->interaction.approaching &&
         GridDistance(LocalPosition(local), local->presented_card_origin) > 3.0f))
        local->presented_card_origin = LocalPosition(local);
}

static int32_t PresentedTargetAt(const LocalState *local, ClientView view, Vector2 mouse)
{
    if (view != VIEW_LOCAL || !AdventureScene(local) || LocalCombatActive(local)) return -1;
    for (int32_t i = 0; i < local->presented_target_count; ++i)
        if (CheckCollisionPointRec(mouse, local->presented_target_bounds[i])) return i;
    return -1;
}

static void RememberRoadActions(const CcSim *sim, LocalState *local,
                                ClientView view, const ContextActionSet *actions)
{
    local->presented_road_count = 0;
    if (view != VIEW_LOCAL || !local->journey_travel_active) return;
    local->presented_road_route = sim->journey.route_id;
    int32_t first = ContextCardFirst(local, actions);
    int32_t shown = ContextCardCount(actions, first);
    for (int32_t i = 0; i < shown && i < 4; ++i) {
        local->presented_road_actions[i] = actions->items[first + i];
        local->presented_road_bounds[i] = ContextActionBounds(i, shown, actions->combat);
        local->presented_road_count++;
    }
}

static bool PresentedRoadActionAt(const CcSim *sim, const LocalState *local,
    ClientView view, const ContextActionSet *actions, Vector2 mouse,
    ContextAction *pressed)
{
    if (view != VIEW_LOCAL) return false;
    for (int32_t i = 0; i < local->presented_road_count; ++i) {
        if (!CheckCollisionPointRec(mouse, local->presented_road_bounds[i])) continue;
        *pressed = (ContextAction){.kind = CONTEXT_ACTION_NONE};
        if (!local->journey_travel_active ||
            local->presented_road_route != sim->journey.route_id) return true;
        const ContextAction *shown = &local->presented_road_actions[i];
        for (int32_t j = 0; j < actions->count; ++j) {
            const ContextAction *current = &actions->items[j];
            if (shown->kind == current->kind && shown->good == current->good &&
                shown->amount == current->amount && shown->active == current->active &&
                CcInteractionKeyEqual(shown->target, current->target) &&
                strcmp(shown->label, current->label) == 0) {
                *pressed = *current;
                break;
            }
        }
        return true;
    }
    return false;
}

static const char *ContextChoiceWords(const char *label)
{
    const char *words = label;
    if (words[0] >= '1' && words[0] <= '9') words += 1;
    else if (strncmp(words, "Esc", 3) == 0) words += 3;
    while (*words == ' ') words += 1;
    return words;
}

static const char *ContextTouchActionLabel(
    const LocalState *local, ClientView view, int32_t action_index,
    const ContextAction *action, char *label, size_t label_capacity)
{
    if (action->kind == CONTEXT_ACTION_WORLD_TARGET) {
        (void)snprintf(label, label_capacity, "%s %s",
                       action->detail, action->label);
        return label;
    }
    if (action->kind == CONTEXT_ACTION_CARE_HORSES) {
        (void)snprintf(label, label_capacity, "%s. %s",
                       action->label, action->detail);
        return label;
    }
    if (action->kind == CONTEXT_ACTION_CHOOSE_ROAD && !action->enabled) {
        (void)snprintf(label, label_capacity, "%s. %s",
                       action->label, action->detail);
        return label;
    }
    if (local->adventure_ui && view == VIEW_CHARACTER) {
        const char *words = ContextChoiceWords(action->label);
        (void)snprintf(label, label_capacity, "%d %s",
                       action_index + 1, words);
        return label;
    }
    return action->label;
}

static void DrawContextActionTray(const CcSim *sim, LocalState *local,
                                  ClientView view, int32_t selected,
                                  int32_t selected_situation)
{
    ContextActionSet actions = BuildContextActions(
        sim, local, view, selected, selected_situation);
    RememberPresentedTargets(local, view, &actions);
    RememberRoadActions(sim, local, view, &actions);
    Vector2 mouse = ClientPointerPosition();
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
                          GetScreenHeight() -
                              (actions.count > 6 ? 160 : 94),
                          9, MUTED);
    }
    int32_t first = ContextCardFirst(local, &actions);
    int32_t shown = ContextCardCount(&actions, first);
    if (actions.count > shown) {
        Rectangle previous = ContextPageBounds(false), next = ContextPageBounds(true);
        ClientTouchAdd(previous, "Previous objects", true, false);
        ClientTouchAdd(next, "More objects", true, false);
        DrawPanel(previous, PANEL_DEEP); DrawPanel(next, PANEL_DEEP);
        CcOverlayDrawText("<", (int)previous.x + 16, (int)previous.y + 22, 18, CC_GOLD);
        CcOverlayDrawText(">", (int)next.x + 16, (int)next.y + 22, 18, CC_GOLD);
        const char *page = TextFormat("%d / %d", first / ContextCardsPerPage() + 1,
            (actions.count + ContextCardsPerPage() - 1) / ContextCardsPerPage());
        CcOverlayDrawText(page, (GetScreenWidth() - CcOverlayMeasureText(page, 11)) / 2,
            GetScreenHeight() - 111, 11, MUTED);
    }
    for (int32_t i = first; i < first + shown; ++i) {
        Rectangle bounds = ContextActionBounds(i - first, shown, actions.combat);
        const ContextAction *action = &actions.items[i];
        char touch_label[128];
        ClientTouchAdd(bounds, ContextTouchActionLabel(
            local, view, i, action, touch_label, sizeof(touch_label)),
            action->enabled, action->active);
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
        if (local->adventure_ui && view == VIEW_CHARACTER) {
            CcOverlayDrawText(TextFormat("%d", i + 1), (int)bounds.x + 9, (int)bounds.y + 6, 12, CC_GOLD);
        }
        bool detailed = action->detail[0] != '\0' ||
                        action->key_hint[0] != '\0';
        const char *action_label = action->label;
        if (local->adventure_ui && view == VIEW_CHARACTER) {
            action_label = ContextChoiceWords(action_label);
        }
        int label_size = local->adventure_ui ? AdventureTextSize(18) : detailed ? 10 : 11;
        if (local->adventure_ui) {
            while (label_size > 12 && AdventureText(action_label, 0, 0,
                (int)bounds.width - 24, label_size, label_color, false) > 44) --label_size;
            int label_y = (int)bounds.y + (view == VIEW_CHARACTER ? 26 : 12);
            (void)AdventureWrap(action_label, (int)bounds.x + 12, label_y,
                (int)bounds.width - 24, label_size, label_color);
        } else {
            int width = CcOverlayMeasureText(action_label, label_size);
            CcOverlayDrawText(action_label, (int)(bounds.x + (bounds.width - (float)width) * 0.5f),
                (int)bounds.y + (detailed ? 10 : 22), label_size, label_color);
        }
        if (!local->adventure_ui && action->key_hint[0] != '\0') {
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
            int (*measure_detail)(const char *, int) = local->adventure_ui ?
                CcOverlayMeasureBodyText : CcOverlayMeasureText;
            int detail_size = local->adventure_ui ? AdventureTextSize(14) : 7;
            while (detail_size > 11 && measure_detail(action->detail, detail_size) > (int)bounds.width - 20)
                --detail_size;
            char detail[48];
            (void)snprintf(detail, sizeof(detail), "%s", action->detail);
            while (strlen(detail) > 3 && measure_detail(detail, detail_size) > (int)bounds.width - 20) {
                size_t length = strlen(detail);
                detail[length - 1] = '\0';
                detail[length - 2] = '.'; detail[length - 3] = '.'; detail[length - 4] = '.';
            }
            void (*draw_detail)(const char *, int, int, int, Color) = local->adventure_ui ?
                CcOverlayDrawBodyText : CcOverlayDrawText;
            draw_detail(detail, (int)bounds.x + 10, (int)bounds.y + 56,
                detail_size, action->enabled ? MUTED : Fade(MUTED, 0.46f));
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
    Vector2 mouse = ClientPointerPosition();
    ContextAction road = {.kind = CONTEXT_ACTION_NONE};
    if (PresentedRoadActionAt(sim, local, view, &actions, mouse, &road))
        return right ? none : road;
    int32_t presented = PresentedTargetAt(local, view, mouse);
    if (presented >= 0) {
        if (right) return none;
        const CcInteractionTarget *target = CcInteractionFind(
            &local->interactions, local->presented_targets[presented]);
        if (target == NULL || target->character_id !=
            local->presented_target_characters[presented]) return none;
        ContextAction pressed = {.kind = CONTEXT_ACTION_WORLD_TARGET,
            .target = target->key, .enabled = target->available};
        (void)snprintf(pressed.label, sizeof(pressed.label), "%s", target->name);
        (void)snprintf(pressed.detail, sizeof(pressed.detail), "%.*s",
            (int)sizeof(pressed.detail) - 1,
            target->available ? target->verb : target->reason);
        return pressed;
    }
    int32_t first = ContextCardFirst(local, &actions);
    int32_t shown = ContextCardCount(&actions, first);
    for (int32_t i = first; i < first + shown; ++i) {
        if (CheckCollisionPointRec(mouse, ContextActionBounds(i - first, shown, actions.combat))) {
            ContextAction pressed = actions.items[i];
            if (pressed.kind == CONTEXT_ACTION_WORLD_TARGET && local->world_cards_presented) return none;
            if (right && pressed.kind != CONTEXT_ACTION_BUY_CARGO) return none;
            if (right) pressed.amount = -1;
            return pressed;
        }
    }
    if (left) return WorldContextActionAt(sim, local, view, &actions, mouse);
    return none;
}

static Rectangle AudioControlBounds(void)
{
    return (Rectangle){(float)GetScreenWidth() * 0.5f + 228.0f, 11.0f, 104.0f, 30.0f};
}

static bool PointerOverContextAction(
    const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation, Vector2 mouse)
{
    if (PresentedTargetAt(local, view, mouse) >= 0) return true;
    ContextActionSet actions = BuildContextActions(
        sim, local, view, selected, selected_situation);
    ContextAction road = {.kind = CONTEXT_ACTION_NONE};
    if (PresentedRoadActionAt(sim, local, view, &actions, mouse, &road)) return true;
    int32_t first = ContextCardFirst(local, &actions);
    int32_t shown = ContextCardCount(&actions, first);
    if (actions.count > shown && (CheckCollisionPointRec(mouse, ContextPageBounds(false)) ||
        CheckCollisionPointRec(mouse, ContextPageBounds(true)))) return true;
    for (int32_t index = 0; index < shown; ++index) {
        if (CheckCollisionPointRec(mouse, ContextActionBounds(index, shown, actions.combat))) return true;
    }
    return false;
}

static void UpdateMovementPreview(
    const CcSim *sim, LocalState *local, ClientView view,
    int32_t selected, int32_t selected_situation,
    RenderTexture2D local_target, Rectangle local_bounds, float delta_time)
{
    if (local == NULL) return;
    Vector2 mouse = ClientPointerPosition();
    bool unavailable = view != VIEW_LOCAL || local->site_travel_active ||
        local->road_choice_active || (local->journey_travel_active &&
        (!local->open_world || local->world_carriage.hero_embarked)) ||
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
    local->movement_preview_cooldown = 0.02f;

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
    float y = (float)GetScreenHeight() -
        (ContextViewportWidth() >= 900 ? 210.0f : 128.0f);
    Color accent = target != NULL ? TEAL : CC_GOLD;
    DrawRectangleRounded((Rectangle){x, y, (float)width, 27.0f},
                         0.20f, 5, Fade(PANEL_DEEP, 0.96f));
    DrawRectangleRoundedLinesEx((Rectangle){x, y, (float)width, 27.0f},
                                0.20f, 5, 1.0f, Fade(accent, 0.62f));
    CcOverlayDrawText(shown, (int)x + 15, (int)y + 8, 9, INK);
}

static const float SAVE_FEEDBACK_VISIBLE_SECONDS = 5.0f;

#if !defined(PLATFORM_WEB)
static void DrawSaveFeedbackToast(const char *message, float message_age)
{
    const float fade_seconds = 0.8f;
    if (message == NULL || message[0] == '\0' ||
        message_age >= SAVE_FEEDBACK_VISIBLE_SECONDS) {
        return;
    }
    const char *toast = TextFormat("%.96s", message);
    int width = CcOverlayMeasureText(toast, 10) + 30;
    if (width > 760) width = 760;
    float opacity =
        message_age > SAVE_FEEDBACK_VISIBLE_SECONDS - fade_seconds ?
            (SAVE_FEEDBACK_VISIBLE_SECONDS - message_age) /
                fade_seconds : 1.0f;
    float x = ((float)GetScreenWidth() - (float)width) * 0.5f;
    float y = (float)GetScreenHeight() - 107.0f;
    DrawRectangleRounded((Rectangle){x, y, (float)width, 30.0f},
                         0.22f, 5, Fade(PANEL_DEEP, opacity));
    DrawRectangleRoundedLinesEx(
        (Rectangle){x, y, (float)width, 30.0f},
        0.22f, 5, 1.0f, Fade(TEAL, opacity * 0.72f));
    CcOverlayDrawText(toast, (int)x + 15, (int)y + 9, 10,
                      Fade(INK, opacity));
}
#endif

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
        case COMMAND_ACTION_LEDGER: return "Book";
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
    if (action == COMMAND_ACTION_SAVE) {
        return !choosing_road || StableWorldRoadChoice(local);
    }
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
    Vector2 mouse = ClientPointerPosition();
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
        ClientTouchAdd(bounds, label, enabled, active);
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
    Vector2 mouse = ClientPointerPosition();
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
                 owned ? "IN THE CASE" : "GLOAMGATE ATLAS",
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
        CcOverlayDrawText("NO ROUTE MAP", 958, 193, 15, MUTED);
        if (here != NULL && sim->schema_version >= 114U)
            CcOverlayDrawText("D  view the districts", 958, 215, 12, TEAL);
        CcOverlayDrawText("M  close case", 958, 604, 10, MUTED);
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
    CcOverlayDrawText("D  districts   M  close case   Q  situations",
                      958, 604, 9, MUTED);
}

typedef struct {
    float scale;
    float centre_east_m;
    float centre_north_m;
} DistrictMapProjection;

static DistrictMapProjection DistrictMapFit(const CcSim *sim, int32_t first)
{
    int32_t min_east = 0, max_east = 0, min_north = 0, max_north = 0;
    for (int32_t i = 0; i < CC_CENSUS_DISTRICTS_PER_TOWN; ++i) {
        const CcCensusDistrict *district = &sim->census.districts[first + i];
        CcCensusPoint point;
        if (CcCensusDistrictCentre(sim, district->id, &point)) {
            if (point.east_m < min_east) min_east = point.east_m;
            if (point.east_m > max_east) max_east = point.east_m;
            if (point.north_m < min_north) min_north = point.north_m;
            if (point.north_m > max_north) max_north = point.north_m;
        }
        for (int32_t home = 0; home < district->dwelling_count; ++home) {
            if (!CcCensusDwellingEntrance(sim, district->id, home, &point))
                continue;
            if (point.east_m < min_east) min_east = point.east_m;
            if (point.east_m > max_east) max_east = point.east_m;
            if (point.north_m < min_north) min_north = point.north_m;
            if (point.north_m > max_north) max_north = point.north_m;
        }
    }
    DistrictMapProjection fit = {
        .scale = fminf(830.0f / (float)(max_east - min_east + 80),
                       440.0f / (float)(max_north - min_north + 80)),
        .centre_east_m = (float)(min_east + max_east) * 0.5f,
        .centre_north_m = (float)(min_north + max_north) * 0.5f
    };
    return fit;
}

static Vector2 DistrictMapScreenPoint(CcCensusPoint point,
                                      DistrictMapProjection fit)
{
    return (Vector2){475.0f +
                         ((float)point.east_m - fit.centre_east_m) * fit.scale,
                     387.0f -
                         ((float)point.north_m - fit.centre_north_m) * fit.scale};
}

static void DrawDistrictMap(const CcSim *sim, int32_t selected,
                            int32_t selected_dwelling)
{
    const CcSettlement *town = CcSimSettlement(sim, sim->player.location_id);
    if (town == NULL || sim->schema_version < 114U) return;
    int32_t first = -1;
    for (int32_t i = 0; i < sim->census.district_count;
         i += CC_CENSUS_DISTRICTS_PER_TOWN)
        if (sim->census.districts[i].settlement_id == town->id) {
            first = i;
            break;
        }
    if (first < 0) return;
    if (selected < 0 || selected >= CC_CENSUS_DISTRICTS_PER_TOWN)
        selected = 0;
    DistrictMapProjection fit = DistrictMapFit(sim, first);
    DrawPanel((Rectangle){28, 76, 895, 574}, PANEL);
    DrawPanel((Rectangle){938, 82, 322, 538}, PANEL);
    CcOverlayDrawText(TextFormat("%s / THE DISTRICTS", town->name),
                      50, 97, 20, INK);
    CcOverlayDrawText("Each mark is one saved dwelling. Lanes are measured in metres.",
                      50, 122, 10, MUTED);
    for (int32_t i = 0; i < CC_CENSUS_ROADS_PER_TOWN; ++i) {
        CcCensusRoad road;
        if (!CcCensusRoadAt(sim, town->id, i, &road)) continue;
        Vector2 a = DistrictMapScreenPoint(road.from, fit);
        Vector2 turn = DistrictMapScreenPoint(road.corner, fit);
        Vector2 b = DistrictMapScreenPoint(road.to, fit);
        DrawLineEx(a, turn, 3.0f, Fade(CC_GOLD, 0.78f));
        DrawLineEx(turn, b, 3.0f, Fade(CC_GOLD, 0.78f));
    }
    for (int32_t i = 0; i < CC_CENSUS_DISTRICTS_PER_TOWN; ++i) {
        const CcCensusDistrict *district = &sim->census.districts[first + i];
        for (int32_t home = 0; home < district->dwelling_count; ++home) {
            CcCensusPoint entrance;
            if (!CcCensusDwellingEntrance(sim, district->id, home,
                                          &entrance)) continue;
            Vector2 marker = DistrictMapScreenPoint(entrance, fit);
            if (i == selected && home == selected_dwelling)
                DrawCircleV(marker, 6.0f, CC_GOLD);
            DrawCircleV(marker, i == selected ? 2.5f : 1.7f,
                        i == selected ? TEAL : Fade(INK, 0.55f));
        }
        CcCensusPoint centre;
        if (!CcCensusDistrictCentre(sim, district->id, &centre)) continue;
        Vector2 point = DistrictMapScreenPoint(centre, fit);
        DrawCircleV(point, i == selected ? 11.0f : 8.0f,
                    i == selected ? TEAL : INK);
        CcOverlayDrawText(district->name, (int)point.x + 13,
                          (int)point.y - 11, 11,
                          i == selected ? TEAL : INK);
    }
    const CcCensusDistrict *chosen = &sim->census.districts[first + selected];
    CcOverlayDrawText("A TOWN AND ITS ROADS", 958, 103, 12, TEAL);
    CcOverlayDrawText(chosen->name, 958, 135, 19, INK);
    CcOverlayDrawText(TextFormat("%d residents / %d homes",
        CcCensusDistrictPopulation(sim, chosen->id), chosen->dwelling_count),
        958, 169, 12, INK);
    CcOverlayDrawText(TextFormat("%d m by road from the centre",
        CcCensusDistrictPathMeters(sim, sim->census.districts[first].id,
                                    chosen->id)), 958, 192, 11, MUTED);
    int32_t shelter = 0, named = 0, occupants = 0;
    if (selected_dwelling >= 0 &&
        selected_dwelling < chosen->dwelling_count)
        CcOverlayDrawText(TextFormat("HOME %d", selected_dwelling + 1),
                          958, 244, 12, TEAL);
    else
        CcOverlayDrawText("SELECT A HOME OR DISTRICT", 958, 244, 10, MUTED);
    for (int32_t i = 0; i < sim->census.resident_count; ++i) {
        const CcCensusResident *person = &sim->census.residents[i];
        if (person->left_day != 0 || person->district_slot != first + selected)
            continue;
        if (person->sheltered) ++shelter;
        if (selected_dwelling >= 0 &&
            selected_dwelling < chosen->dwelling_count) {
            if (person->dwelling_slot != selected_dwelling) continue;
            const CcCharacter *character = CcSimCharacter(sim, person->id);
            CcOverlayDrawText(character != NULL ? character->name :
                TextFormat("Resident #%" PRIu64,
                           person->id & UINT64_C(0x00ffffffffffffff)),
                958, 271 + occupants * 31, 12, INK);
            ++occupants;
            continue;
        }
        if (!person->rich_identity || named >= 8) continue;
        const CcCharacter *character = CcSimCharacter(sim, person->id);
        if (character == NULL) continue;
        CcOverlayDrawText(character->name, 958, 271 + named * 24,
                          12, INK);
        ++named;
    }
    if (selected_dwelling >= 0 && occupants == 0)
        CcOverlayDrawText("This home is vacant.", 958, 271, 12, MUTED);
    if (shelter > 0)
        CcOverlayDrawText(TextFormat("%d people in shelter", shelter),
                          958, 474, 11, CC_GOLD);
    CcOverlayDrawText("CLICK  home or district   ARROWS  districts",
                      958, 552, 9, MUTED);
    CcOverlayDrawText("D  route maps   M  close case", 958, 578, 10, MUTED);
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
    Rectangle bounds = {330.0f, 160.0f, 620.0f, 440.0f};
    DrawPanel(bounds, PANEL_DEEP);
    const CcSituation *detail = SelectedActiveSituation(sim, selected);
    int32_t active_count = 0;
    int32_t active_ordinal = 0;
    int32_t indexes[8];
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (!SituationVisibleToPlayer(sim, i)) continue;
        if (i == selected) active_ordinal = active_count;
        if (active_count < 8) indexes[active_count] = i;
        active_count += 1;
    }
    const CcSettlement *town = CcSimSettlement(sim, sim->player.location_id);
    CcOverlayDrawText("NOTICE BOARD", 360, 186, 10, TEAL);
    CcOverlayDrawText(town != NULL ? TextFormat("%s", town->name) : "",
                      700, 186, 10, MUTED);
    CcOverlayDrawText(active_count > 0 ?
             TextFormat("%d / %d", active_ordinal + 1, active_count) : "0 / 0",
             874, 186, 10, CC_GOLD);
    /* The board keeps a dated list of names to find and where they are. */
    int32_t shown = active_count < 8 ? active_count : 8;
    for (int32_t row = 0; row < shown; ++row) {
        const CcSituation *notice = &sim->situations[indexes[row]];
        bool chosen = indexes[row] == selected;
        int y = 214 + row * 24;
        if (chosen) DrawRectangle(352, y - 4, 576, 22, Fade(PANEL_HOVER, 0.9f));
        bool accepted = notice->id == sim->player.accepted_situation_id;
        const char *kind = accepted ? "Your promise" :
            SituationPostingKind(notice->kind);
        CcOverlayDrawText(TextFormat("%s: %s", kind,
            SituationGiver(sim, notice) != NULL ?
                SituationGiver(sim, notice)->name : "the sponsor"),
            360, y, 11, chosen ? CC_GOLD : INK);
    }
    if (active_count == 0) {
        CcOverlayDrawText("NO NOTICES", 360, 238, 17, MUTED);
        return;
    }
    if (detail != NULL) {
        int y = 420;
        bool accepted = detail->id == sim->player.accepted_situation_id;
        bool offer = CcSimSituationCanAccept(sim, detail);
        const char *title = detail->kind == CC_SITUATION_MONSTER_EXPEDITION &&
                !offer ? "Strange noises in the mine" :
            SituationTitle(detail->kind);
        CcOverlayDrawText(title, 360, y, 19, SituationColor(detail->kind));
        y += 30;
        if (accepted) {
            CcOverlayDrawText(TextFormat("Promised day %d  /  due day %d",
                detail->created_day, detail->deadline_day), 360, y, 10, MUTED);
        } else if (offer) {
            const CcSettlement *posted_at = CcSimSettlement(
                sim, CcSimSituationOfferSettlementId(sim, detail));
            const CcNotice *notice = CcSimSituationNotice(sim, detail->id);
            CcOverlayDrawText(TextFormat(
                "Posted day %d%s  /  due day %d", notice != NULL ? notice->day : detail->created_day,
                posted_at != NULL && posted_at->id != sim->player.location_id ?
                    TextFormat(" at %s", posted_at->name) : "",
                detail->deadline_day), 360, y, 10, MUTED);
        } else {
            CcOverlayDrawText("A lead from conversation", 360, y, 10, MUTED);
        }
        y += 26;
        char next[192];
        SituationNextAction(sim, detail, next, sizeof(next));
        DrawTwoLineText(next, 360, y, 58U, 11, CC_GOLD);
        y += 40;
        if (offer || accepted) {
            CcOverlayDrawText(
                TextFormat("REWARD  +%" PRId64 " CROWNS", detail->reward),
                360, y, 10, TEAL);
        }
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

static bool DrawEconomicGoodIcon(Texture2D atlas, CcGood good,
                                 Rectangle destination, Color tint)
{
    if (atlas.id == 0U || good < 0 || good >= CC_GOOD_COUNT ||
        atlas.width < ((int32_t)good + 1) * 32 || atlas.height < 32) {
        return false;
    }
    Rectangle source = {(float)good * 32.0f, 0.0f, 32.0f, 32.0f};
    DrawTexturePro(atlas, source, destination, (Vector2){0.0f, 0.0f},
                   0.0f, tint);
    return true;
}

static Rectangle CarriageTabBounds(int32_t tab)
{
    return (Rectangle){(float)ContextViewportWidth() - 340.0f + (float)tab * 144.0f,
        94.0f, 136.0f, 40.0f};
}

static const char *RoadCarriageStatus(const CcSim *sim)
{
    if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) return "BLOCKED";
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) return "RESTING";
    return sim->carriage.mode == CC_CARRIAGE_MOVING ? "MOVING" : "STOPPED";
}

static void CarriageReadingSummary(const CcSim *sim, char *summary,
                                   size_t summary_capacity)
{
    if (summary == NULL || summary_capacity == 0U) return;
    summary[0] = '\0';
    if (sim == NULL) return;
    int32_t used = CcPlayerCargoUsed(&sim->player);
    int32_t free_slots = sim->player.cargo_capacity - used;
    if (free_slots < 0) free_slots = 0;
    size_t length = (size_t)snprintf(summary, summary_capacity, "Manifest: ");
    if (length >= summary_capacity) length = summary_capacity - 1U;
    bool has_goods = false;
    for (int32_t good = 0; good < CC_GOOD_COUNT && length < summary_capacity - 1U;
         ++good) {
        if (sim->player.cargo[good] <= 0) continue;
        int written = snprintf(summary + length, summary_capacity - length,
            "%s%s %d", has_goods ? ", " : "", CcGoodName((CcGood)good),
            sim->player.cargo[good]);
        if (written < 0) break;
        size_t added = (size_t)written;
        length += added < summary_capacity - length ?
            added : summary_capacity - length - 1U;
        has_goods = true;
    }
    if (!has_goods && length < summary_capacity - 1U) {
        int written = snprintf(summary + length, summary_capacity - length, "empty");
        if (written > 0) {
            size_t added = (size_t)written;
            length += added < summary_capacity - length ?
                added : summary_capacity - length - 1U;
        }
    }
    if (length < summary_capacity - 1U) {
        (void)snprintf(summary + length, summary_capacity - length,
            ". Load %d of %d, free %d. Team %d ponies, readiness %d of 100. "
            "Carriage condition %d of 100.",
            used, sim->player.cargo_capacity, free_slots,
            CcSimHorseTeamCount(sim), CcSimHorseTeamReadiness(sim),
            sim->carriage.condition);
    }
}

#include "cc_carriage_overview.inc"

static void DrawCarriageScreen(const CcSim *sim, const LocalState *local,
                               Texture2D economic_goods)
{
    if (sim == NULL || local == NULL) return;
    const CcSettlement *place = CcSimSettlement(
        sim, sim->player.location_id);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BACKGROUND, 0.78f));
    ClientTouchBegin();
    ClientTouchHeading(
        "The Crownless Carriage",
        local->carriage_inspection_road ?
            "On the road. Review this route, cargo, promise, and team." :
            "Choose Overview or Ponies.");
    char reading_summary[512];
    CarriageReadingSummary(sim, reading_summary, sizeof(reading_summary));
    ClientTouchRecordText(reading_summary);
    DrawPanel((Rectangle){24.0f, 78.0f, (float)GetScreenWidth() - 48.0f,
        (float)GetScreenHeight() - 94.0f}, PANEL_DEEP);
    CcOverlayDrawText("THE CROWNLESS CARRIAGE", 52, 102, 23, INK);
    const CcSettlement *road_origin = CcSimSettlement(
        sim, sim->journey.origin_id);
    const CcSettlement *road_destination = CcSimSettlement(
        sim, sim->journey.destination_id);
    CcOverlayDrawText(
        TextFormat("%s / %s",
                   local->carriage_inspection_road ? "ON THE ROAD" :
                   local->site_kind == CC_LOCAL_SITE_NONE ?
                       "LOADING BAY" : "ROADSIDE BAY",
                   local->carriage_inspection_road ?
                       TextFormat("%s to %s",
                                  road_origin != NULL ? road_origin->name : "ROAD",
                                  road_destination != NULL ? road_destination->name : "ROAD") :
                   local->site_kind == CC_LOCAL_SITE_NONE ?
                       (place != NULL ? place->name : "TOWN") :
                       CcLocalSiteName(sim, local->site_kind)),
        52, 136, 10, TEAL);
    CcOverlayDrawText(
        TextFormat("DAY %d / %" PRId64 " CROWNS",
                   sim->current_day, sim->player.coins),
        GetScreenWidth() - 254, 142, 9, CC_GOLD);
    const char *tabs[CARRIAGE_TAB_COUNT] = {"Overview", "Ponies"};
    for (int32_t tab = 0; tab < CARRIAGE_TAB_COUNT; ++tab) {
        AdventureButton(CarriageTabBounds(tab), tabs[tab], true,
            local->carriage_tab == (CarriageTab)tab);
    }
    DrawRectangle(52, 156, GetScreenWidth() - 104, 1, Fade(CC_GOLD, 0.42f));
    if (local->carriage_tab == CARRIAGE_PONIES) {
        DrawCarriagePonies(sim);
        return;
    }

    DrawCarriageOverview(sim, local, economic_goods);
}

static void ClientSpeechPath(const CcSpeech *speech, char *path, size_t capacity)
{
    path[0] = '\0';
    char relative[256];
    if (speech == NULL || !CcSpeechPath(speech, relative, sizeof(relative))) return;
    (void)ResolveClientAssetPath(relative, path, capacity);
#if !defined(PLATFORM_WEB)
    if (!FileExists(path) && CcSoundVoicePath(speech->line_id, speech->speaker,
            speech->text, relative, sizeof(relative))) {
        char legacy[768];
        (void)ResolveClientAssetPath(relative, legacy, sizeof(legacy));
        if (FileExists(legacy)) (void)snprintf(path, capacity, "%s", legacy);
    }
#endif
}

static void ClientSaySpeech(const CcSpeech *speech)
{
    if (speech != NULL && strcmp(speech->line_id, "gossip.thinking") == 0) return;
    char path[768];
    ClientSpeechPath(speech, path, sizeof(path));
    CcAudioSay(speech, path);
}

static void ClientReadSpeech(const CcSim *sim, const char *text, CcId source)
{
    CcSpeech speech;
    if (CcSpeechCompose(&speech, "reader.page", sim->player.id, "Reader", 5, text,
        CC_SPEECH_PLAIN, CC_SPEECH_FEEDBACK, source)) ClientSaySpeech(&speech);
}

static void ClientMindFor(const CcSim *sim, const CcCharacter *character,
                          CcId listener, CcId event, const CcGossipVersion *version, CcCoreMind *mind,
                          CcGossipLanguage *memory_language);

static bool ClientConversationSpeech(const CcSim *sim, const LocalState *local,
                                     CcSpeech *speech)
{
    const CcSituation *situation = CcSimSituation(sim, local->conversation_situation_id);
    const CcCharacter *person = CcSimCharacter(sim, local->conversation_character_id);
    if (local->conversation_oven_response && person != NULL &&
        local->conversation_line[0] != '\0') {
        return CcSpeechCompose(speech, "oven.court.response", person->id,
            person->name, CcSpeechCharacterVoice(sim, person),
            local->conversation_line, CC_SPEECH_PLAIN,
            CC_SPEECH_CONVERSATION, local->conversation_oven_event);
    }
    if (local->conversation_report_response && person != NULL &&
        local->conversation_line[0] != '\0') {
        return CcSpeechCompose(speech, "mine.report.response", person->id,
            person->name, CcSpeechCharacterVoice(sim, person),
            local->conversation_line, CC_SPEECH_PLAIN,
            CC_SPEECH_CONVERSATION, sim->mine.report_event_id);
    }
    if (CcSpeechCharacter(sim, situation, person, speech)) return true;
    if (core_conversation_speaker != local->conversation_character_id) {
        CcCoreConversationReset(&core_conversation);
        core_conversation_speaker = local->conversation_character_id;
    }
    if (CcCoreConversationShown(&core_conversation, speech)) return true;
    const CcGossipCarrier *carrier = CcSimGossipCarrier(
        sim, local->conversation_character_id);
    int32_t slot = local->conversation_gossip_slot;
    if (slot < 0) {
        const CcGossipVersion *version = NULL;
        slot = CcSimNextUntoldStory(sim, local->conversation_character_id,
                                    &version);
    }
    if (slot >= 0 && carrier != NULL) {
        const CcGossip *story = CcSimGossipStory(sim, slot);
        if (story != NULL &&
            CcSpeechStory(sim, local->conversation_character_id, story,
                &carrier->versions[slot], local->conversation_gossip_source,
                speech)) {
            if (!local->conversation_gossip_source && core_conversation.model != NULL) {
                if (core_conversation_speaker != local->conversation_character_id) {
                    CcCoreConversationReset(&core_conversation);
                    core_conversation_speaker = local->conversation_character_id;
                }
                CcGossipLanguage language;
                CcCoreAccount account;
                if (CcSpeechPrepareGossip(sim, story, &carrier->versions[slot], 0U, &language) &&
                    CcCoreAccountPrepare(story->kind, language.account, language.confidence,
                        language.retellings, &account)) {
                    if (person != NULL) {
                        /* The speaker's own stance and situation, not the
                           defaults: a starving traveller should sound like one.
                           Memories render into the prompt synchronously, so the
                           stack buffer below does not outlive the call. */
                        CcCoreMind mind;
                        CcGossipLanguage mind_memories[CC_CORE_MIND_LINES];
                        ClientMindFor(sim, person, sim->player.id, story->event_id, &carrier->versions[slot],
                            &mind, mind_memories);
                        CcCoreControl move = CcCoreConversationReplyMove(
                            &core_conversation, &account, speech->source_event_id, speech->speaker_id);
                        /* Share a held memory the conversation named, when the
                           speaker trusts the listener enough to offer it. The
                           chosen memory is presented alone and cued as recall,
                           so the reply reproduces it exactly. */
                        const char *share = CcCoreMemoryShare(&mind,
                            core_conversation.history, core_conversation.count,
                            mind.trusts_listener);
                        if (share != NULL) {
                            mind.memories[0] = share;
                            mind.memory_count = 1;
                            move = CC_CORE_CONTROL_RECALL;
                        }
                        (void)CcCoreConversationPrepareMind(&core_conversation, &account,
                            &mind, move, speech);
                    } else {
                        (void)CcCoreConversationPrepare(&core_conversation, &account, speech);
                    }
                }
            }
            return true;
        }
    }
    const CcLocalPlaceProfile *place = CcLocalPlaceProfileForSettlement(
        CcSimSettlement(sim, sim->player.location_id));
    const char *name = person != NULL ? person->name :
        local->conversation_name[0] != '\0' ? local->conversation_name : "Neighbour";
    CcSpeech greeting;
    if (!CcSpeechGreeting(sim, sim->player.location_id,
            person != NULL ? person->id : local->conversation_object,
            name, place->primary_hall, &greeting)) return false;
    if (person != NULL) {
        return CcSpeechCompose(speech, greeting.line_id, person->id, person->name,
            CcSpeechCharacterVoice(sim, person), greeting.text, greeting.delivery,
            greeting.priority, 0);
    }
    *speech = greeting;
    return true;
}

static void ClientMindFor(const CcSim *sim, const CcCharacter *character,
                          CcId listener, CcId event, const CcGossipVersion *version, CcCoreMind *mind,
                          CcGossipLanguage *memory_language)
{
    CcCoreParticipant participant;
    *mind = (CcCoreMind){0};
    if (character == NULL || !CcCoreParticipantBuild(sim, character->id, listener, &participant)) return;
    CcCoreParticipantMind(&participant, event,
        version != NULL && version->source_character_id == character->id, mind, memory_language);
}

static bool ClientStartChat(const CcSim *sim, LocalState *local, uint32_t voice)
{
    const CcGossipCarrier *player = CcSimGossipCarrier(sim, sim->player.id);
    const CcGossipCarrier *listener = CcSimGossipCarrier(sim, local->conversation_character_id);
    if (player == NULL || listener == NULL || core_conversation.model == NULL) return false;
    int32_t preferred = local->conversation_gossip_slot >= 0 ? local->conversation_gossip_slot :
        CcSimNextUntoldStory(sim, local->conversation_character_id, NULL);
    for (int32_t attempt = -1; attempt < CC_MAX_GOSSIP; ++attempt) {
        int32_t slot = attempt < 0 ? preferred : attempt;
        if (slot < 0 || slot >= CC_MAX_GOSSIP) continue;
        uint32_t bit = UINT32_C(1) << (uint32_t)slot;
        if ((player->stories & listener->stories & bit) == 0U) continue;
        const CcGossip *story = CcSimGossipStory(sim, slot);
        CcGossipLanguage language[2];
        CcGossipLanguage player_memories[CC_CORE_MIND_LINES];
        CcGossipLanguage listener_memories[CC_CORE_MIND_LINES];
        CcCoreAccount account[2]; CcSpeech speech[2];
        if (story == NULL ||
            !CcSpeechPrepareGossip(sim, story, &player->versions[slot], 0U, &language[0]) ||
            !CcSpeechPrepareGossip(sim, story, &listener->versions[slot], 0U, &language[1])) continue;
        if (!CcCoreAccountPrepare(story->kind, language[0].account, language[0].confidence,
                language[0].retellings, &account[0]) ||
            !CcCoreAccountPrepare(story->kind, language[1].account, language[1].confidence,
                language[1].retellings, &account[1])) continue;
        char words[CC_SPEECH_TEXT_CAPACITY];
        if (!CcSpeechCoreGossip(&language[0], words, sizeof(words))) continue;
        if (!CcSpeechCompose(&speech[0], "gossip.account", sim->player.id, "You", voice,
                words, CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, story->event_id) ||
            !CcSpeechStory(sim, local->conversation_character_id, story, &listener->versions[slot],
                false, &speech[1])) continue;
        CcCoreMind player_mind, listener_mind;
        ClientMindFor(sim, CcSimCharacter(sim, sim->player.id),
            local->conversation_character_id, story->event_id, &player->versions[slot], &player_mind,
            player_memories);
        ClientMindFor(sim, CcSimCharacter(sim, local->conversation_character_id),
            sim->player.id, story->event_id, &listener->versions[slot], &listener_mind, listener_memories);
        if (CcCoreConversationStartRoundMind(&core_conversation, &account[0], &player_mind, &speech[0],
                &account[1], &listener_mind, &speech[1])) {
            local->conversation_gossip_slot = slot;
            return true;
        }
        return false;
    }
    return false;
}

static void DrawCharacterConversation(const CcSim *sim,
                                      const LocalState *local)
{
    if (sim == NULL || local == NULL) return;
    if (local->adventure_ui) { DrawAdventureConversation(sim, local); return; }
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
    CcSpeech speech;
    bool has_spoken = ClientConversationSpeech(sim, local, &speech);
    const CcSpeech *playing = CcAudioCurrentSpeech();
    if (playing != NULL) { speech = *playing; has_spoken = true; }
    const char *spoken = speech.text;
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
    float caption_scale = 1.0f + (adventure_preferences != NULL ? (float)adventure_preferences->caption_size * 0.2f : 0.0f);
    size_t line_capacity = (size_t)((float)available_width / (9.0f * caption_scale));
    if (line_capacity < 16U) line_capacity = 16U;
    if (line_capacity > 104U) line_capacity = 104U;
    DrawTwoLineCaption(has_spoken ? spoken :
                        "They have nothing more to ask.",
                    speech_x + 26, (int)panel_y + 48,
                    line_capacity, 17, INK, caption_scale);
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
    int32_t return_minutes = CcSimJourneyWithdrawalMinutes(sim);
    ClientTouchHeading(bandits != NULL ?
        TextFormat("%.24s blocks the road", bandits->name) :
        "The road is closed",
        has_demand ? TextFormat("Demand: %d %s or %d crowns. Withdraw to %.16s: %dh%02dm, 0 crowns.",
            demanded_quantity, CcGoodName(demanded_good),
            sim->journey.bargain_cost, from != NULL ? from->name : "origin",
            return_minutes / 60, return_minutes % 60) : bandits != NULL ?
        TextFormat("Demand: %d crowns. Withdraw to %.16s: %dh%02dm, 0 crowns.",
            sim->journey.bargain_cost, from != NULL ? from->name : "origin",
            return_minutes / 60, return_minutes % 60) :
        TextFormat("Closed road. Withdraw to %.16s: %dh%02dm, 0 crowns.",
            from != NULL ? from->name : "origin",
            return_minutes / 60, return_minutes % 60));
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
        "Withdraw now: 0 crowns, no extra clock time; road security may fall.",
        386, 455, 9, MUTED);
}

static bool ApplyCommand(CcJournal *journal, CcSim *sim, CcCommand command,
                         char *message, size_t message_capacity)
{
    CcSpeech road_answer;
    bool road_reply = (command.kind == CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS ||
        command.kind == CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE) && CcSpeechRoad(sim, &road_answer);
    CcMoney coins_before = sim->player.coins;
    const CcSituation *accepted_before = CcSimAcceptedSituation(sim);
    CcId promise_before = accepted_before != NULL ? accepted_before->id : 0U;
    char error[192];
    bool applied = CcCoopClientActive() ?
        CcCoopClientApply(sim, &command, error, sizeof(error)) : journal != NULL ?
        CcJournalApply(journal, sim, &command, error, sizeof(error)) :
        CcSimApply(sim, &command, error, sizeof(error));
    if (!applied) {
        (void)snprintf(message, message_capacity, "%s", error);
        return false;
    }
    if (command.kind == CC_COMMAND_GOBLIN_TRADE) {
        CcSpeech goblin;
        if (CcSpeechGoblinTrade(sim, sim->player.coins - coins_before, &goblin))
            ClientSaySpeech(&goblin);
    }
    if (road_reply) {
        CcSpeech answer;
        if (CcSpeechCompose(&answer, "road.paid", road_answer.speaker_id, road_answer.speaker,
            road_answer.voice_index, "We have what we asked for. Move along.", CC_SPEECH_FIRM,
            CC_SPEECH_FEEDBACK, 0)) ClientSaySpeech(&answer);
    }
    if (journal != NULL) {
        switch (command.kind) {
            case CC_COMMAND_TRADE:
            case CC_COMMAND_BUY_MAP:
            case CC_COMMAND_SELL_MAP:
            case CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE:
                CcAudioPlay(CC_SOUND_COINS); break;
            case CC_COMMAND_ACCEPT_SITUATION:
            case CC_COMMAND_CHARACTER_RESPONSE:
                CcAudioPlay(CC_SOUND_PROMISE); break;
            default: break;
        }
    }
    if (command.kind == CC_COMMAND_TRADE) {
        CcMoney change = sim->player.coins - coins_before;
        (void)snprintf(message, message_capacity,
            "%s %d %s. %s%" PRId64 " crowns. Purse: %" PRId64 ".%s",
            command.amount > 0 ? "Bought" : "Sold", abs(command.amount),
            CcGoodName(command.good), change >= 0 ? "+" : "", change,
            sim->player.coins,
            promise_before != 0U && CcSimAcceptedSituation(sim) == NULL ?
                " Promise settled." : "");
        return true;
    }
    const char *confirmation = "Done.";
    switch (command.kind) {
        case CC_COMMAND_TRADE:
            confirmation = command.amount > 0 ? "Cargo bought." :
                                                "Cargo delivered.";
            break;
        case CC_COMMAND_TRAVEL: confirmation = "Journey started."; break;
        case CC_COMMAND_REPAIR_ROUTE: confirmation = "Road repaired."; break;
        case CC_COMMAND_DELIVER_PROPHECY: confirmation = "The council receives your prophecy book."; break;
        case CC_COMMAND_RESERVE_ARCHIVE_RECRUITMENT:
            confirmation = "Recruitment funds and supplies reserved.";
            break;
        case CC_COMMAND_CANCEL_ARCHIVE_RECRUITMENT:
            confirmation = "Unused recruitment resources returned.";
            break;
        case CC_COMMAND_FUND_GRAIN_SUPPLY:
            confirmation = command.amount < 0 ? "Orders ended. The unspent fund is back in your purse." : "The organiser has pay and a grain fund.";
            break;
        case CC_COMMAND_SUPPORT_BAKERY:
            confirmation = command.amount > 0 ? "Supplies received. Building starts today." : "Grain and wages received by the town.";
            break;
        case CC_COMMAND_CHANGE_DUNGEON: confirmation = "Mine updated."; break;
        case CC_COMMAND_MINE_LEARN_LEAD:
            confirmation = command.amount == 1 ?
                "Jory marks the Low Silver Pit turnout and its workers' records in the Company Book." :
                "The shift record marks the Low Silver Pit turnout and its workers' records in the Company Book.";
            break;
        case CC_COMMAND_MINE_REPORT_RETURN:
            if (sim->mine.report_kind == CC_MINE_RETURN_HAUL)
                (void)snprintf(message,message_capacity,
                    "Jory sees %d %s from Low Silver Pit: 'This proves the turnout can still yield.' Oren can handle the sale.",
                    sim->mine.report_quantity,CcGoodName((CcGood)sim->mine.report_good));
            else
                (void)snprintf(message,message_capacity,
                    "Jory reads the sourced route account: 'This gives the next company a fair path in.'");
            return true;
            break;
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

static bool ApplyHorseCare(CcJournal *journal, CcSim *sim,
                           char *message, size_t message_capacity)
{
    CcHorseCarePreview offer = {0};
    if (!CcSimHorseCarePreview(sim, &offer)) return false;
    if (!offer.available) {
        (void)snprintf(message, message_capacity, "%s", offer.reason);
        return false;
    }
    if (!ApplyCommand(journal, sim,
                      (CcCommand){.kind = CC_COMMAND_CARE_HORSES},
                      message, message_capacity)) return false;
    (void)snprintf(message, message_capacity,
        "Stable care: 1 Wheat from %s, %lld crowns, 1 day.%s",
        HorseCareSourceLabel(offer.source), (long long)offer.cost,
        offer.weekly_feed_due ? " Normal day upkeep also ran." : "");
    return true;
}

static void DrawAdventureCourtNotes(const CcSim *sim, const LocalState *local);
#include "client/cc_adventure.inc"
#include "client/cc_oven_court.inc"
#include "client/cc_mine_view.inc"
#include "client/cc_world_actions.inc"
static bool StartOnlyOutgoingRoad(CcJournal *journal, CcSim *sim,
                                   LocalState *local, ClientView *view,
                                   int32_t *selected, char *message,
                                   size_t message_capacity)
{
    if (OutgoingRouteCount(sim) != 1) return false;
    *selected = FirstOutgoingRouteIndex(sim);
    const CcRoute *route = SelectedOutgoingRoute(sim, *selected);
    if (route == NULL) return false;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = RouteOtherEnd(route, sim->player.location_id)
    };
    if (!ApplyCommand(journal, sim, travel, message, message_capacity))
        return false;
    BeginRoadTravelState(sim, local);
    *view = VIEW_LOCAL;
    return true;
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
    ResetLocalStatePreservingAthletics(local);
    (void)snprintf(message, message_capacity,
                   "The carriage is parked in town.");
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
static int RunTravelHoldRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    sim.road_site_count = 0;
    sim.journey.route_id = sim.routes[0].id;
    sim.journey.active = true;
    sim.journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim.pony_company.encounter = -1;
    local.journey_travel_active = true;
    sim.carriage.progress_milli = 0;
    uint64_t before = CcSimHash(&sim);
    /* Every part of an uneventful road moves without held input. */
    const int progress[] = {0, 100, 500, 900, 999};
    for (unsigned i = 0; i < sizeof(progress)/sizeof(progress[0]); ++i) {
        sim.carriage.progress_milli = progress[i];
        for (int f = 0; f < 90; ++f)
            UpdateTravelHold(&sim, &local, VIEW_LOCAL, 0, 0, (Vector2){0}, false, 1.0f/60);
        ContextActionSet actions = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
        int toggle = 0;
        for (int j = 0; j < actions.count; ++j) {
            if (actions.items[j].kind == CONTEXT_ACTION_HOLD_TRAVEL) {
                toggle++;
                if (strcmp(actions.items[j].label, "Stop") != 0) return 1;
            }
            if (actions.items[j].kind == CONTEXT_ACTION_MAKE_ROAD_CAMP ||
                actions.items[j].kind == CONTEXT_ACTION_PASS_ROAD_SITE) return 1;
        }
        if (!local.travel_fast_forward || CcClientTravelTimeScale(local.travel_time_blend) != 8 ||
            toggle != 1 || actions.count > 3) return 1;
    }
    sim.carriage.progress_milli = 0;
    if (CcSimHash(&sim) != before) { fprintf(stderr,"Travel control assertion line %d\n",__LINE__); return 1; }
    local.carriage_stopped = true;
    for (int f = 0; f < 120; ++f)
        UpdateTravelHold(&sim, &local, VIEW_LOCAL, 0, 0, (Vector2){0}, true, 1.0f/60);
    ContextActionSet stopped = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    int toggle = 0;
    for (int j = 0; j < stopped.count; ++j)
        if (stopped.items[j].kind == CONTEXT_ACTION_HOLD_TRAVEL &&
            strcmp(stopped.items[j].label, "Travel") == 0) toggle++;
    if (local.travel_fast_forward || !local.carriage_stopped || toggle != 1 || stopped.count > 4) return 1;
    local.carriage_stopped = false;
    sim.journey.road_waiting_choice = true;
    UpdateTravelHold(&sim, &local, VIEW_LOCAL, 0, 0, (Vector2){0}, false, 1.0f/60);
    if (local.travel_fast_forward) { fprintf(stderr,"Travel control assertion line %d\n",__LINE__); return 1; }
    sim.journey.road_waiting_choice = false;
    sim.pony_company.encounter = 0;
    UpdateTravelHold(&sim, &local, VIEW_LOCAL, 0, 0, (Vector2){0}, false, 1.0f/60);
    if (local.travel_fast_forward) { fprintf(stderr,"Travel control assertion line %d\n",__LINE__); return 1; }
    sim.pony_company.encounter = -1;
    UpdateTravelHold(&sim, &local, VIEW_PAUSE, 0, 0, (Vector2){0}, false, 1.0f/60);
    if (local.travel_fast_forward) { fprintf(stderr,"Travel control assertion line %d\n",__LINE__); return 1; }
    puts("Continuous travel: two moving actions, explicit stopped options, no hold-to-resume.");
    return 0;
}

static int RunStorybookTravelRegression(void)
{
    static CcSim sim;
    static CcSim expected;
    static CcSim restored;
    static LocalState local;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    ResetLocalState(&local);
    ClientView departure_view = VIEW_ROADS;
    int32_t departure_route = FirstOutgoingRouteIndex(&sim);
    char departure_message[192];
    if (!StartOnlyOutgoingRoad(NULL, &sim, &local, &departure_view,
            &departure_route, departure_message, sizeof(departure_message)) ||
        departure_view != VIEW_LOCAL || !local.journey_travel_active ||
        !sim.journey.active) {
        (void)fprintf(stderr, "Direct departure failed.\n");
        return 1;
    }
    for (int scenario = 0; scenario < 7; ++scenario) {
        char path[96];
        char error[256] = "";
        (void)snprintf(path, sizeof(path), "storybook-travel-%d.sqlite", scenario);
        (void)remove(path);
        CcSimInit(&sim, UINT32_C(0xc0a71a9e));
        CcRoute *route = &sim.routes[0];
        route->closed = false;
        route->security = 100;
        sim.player.location_id = route->from_id;
        sim.carriage.location_id = route->from_id;
        CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = route->to_id};
        if (!CcSimApply(&sim, &travel, error, sizeof(error))) {
            (void)fprintf(stderr, "Storybook setup: %s\n", error);
            return 1;
        }
        /* This regression covers the older watch and encounter boundaries.
           Physical road boundaries have their own runtime regression. */
        sim.journey.road_position_active = false;
        sim.journey.road_waiting_choice = false;
        sim.journey.situation_id = 0U;
        sim.journey.encounter_subticks = 0;
        sim.journey.encounter_triggered = true;
        sim.journey.ambush_pending = scenario == 1;
        sim.journey.ambush_warned = false;
        sim.journey.ambush_resolved = false;
        sim.journey.total_subticks = CC_WORLD_WATCH_SUBTICKS * 2;
        sim.journey.elapsed_subticks = scenario == 6 ?
            sim.journey.total_subticks * 9 / 10 - 1 : scenario == 1 ?
            sim.journey.total_subticks * 45 / 100 - 1 :
            scenario == 2 ? CC_WORLD_WATCH_SUBTICKS - 1 :
            scenario == 3 ? sim.journey.total_subticks - 1 :
            scenario >= 4 ? (int32_t)(
                (int64_t)sim.journey.total_subticks *
                sim.road_sites[0].progress_milli / 1000) -
                    (scenario == 4 ? 1 : 0) : 0;
        sim.carriage.progress_milli = (int32_t)(
            (int64_t)sim.journey.elapsed_subticks * 1000 /
            sim.journey.total_subticks);
        CcCommand pace = {.kind = CC_COMMAND_SET_JOURNEY_PACE,
                           .amount = (int32_t)sim.journey.pace};
        if (!CcSimApply(&sim, &pace, error, sizeof(error))) return 1;
        if (scenario < 4 || scenario == 6)
            sim.journey.road_site_stop_mask = (UINT32_C(1) << sim.road_site_count) - 1U;
        expected = sim;
        if (scenario == 2) {
            CcSimAdvanceRuntimeTicks(&expected, 1);
            CcCommand rest = {.kind = CC_COMMAND_TAKE_JOURNEY_BREAK};
            if (!CcSimApply(&expected, &rest, error, sizeof(error))) return 1;
            CcSimAdvanceRuntimeTicks(&expected, 119);
        } else CcSimAdvanceRuntimeTicks(&expected,
            scenario == 4 || scenario == 5 ? 0 : scenario == 1 || scenario == 3 ? 1 : 120);
        CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
        if (journal == NULL) {
            (void)fprintf(stderr, "Storybook journal: %s\n", error);
            return 1;
        }
        local = (LocalState){0};
        ResetLocalState(&local);
        if (scenario == 3) {
            if (!InitializeOpenWorld(&sim, &local, false)) return 1;
            BeginRoadTravelState(&sim, &local);
        }
        local.journey_travel_active = true;
        local.travel_fast_forward = scenario == 1 || scenario == 3 || scenario == 6;
        local.travel_time_blend = 1.0f;
        local.convoy.runtime_tick_accumulator = 0.75f;
        bool passed = AdvanceStorybookTravel(journal, &sim, &local,
                                             120, error, sizeof(error));
        passed = passed && CcSimHash(&sim) == CcSimHash(&expected);
        if (scenario == 1 || scenario == 3) {
            passed = passed && !local.travel_fast_forward &&
                local.travel_attention && local.convoy.runtime_tick_accumulator == 0.0f;
        }
        if (scenario == 4 || scenario == 5) {
            passed = passed && CcSimJourneyRoadSiteStop(&sim) != NULL;
            ContextActionSet stop_actions = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
            for (int32_t i = 0; i < stop_actions.count; ++i)
                if (stop_actions.items[i].kind == CONTEXT_ACTION_HOLD_TRAVEL) passed = false;
        }
        if (scenario == 3) {
            Vector3 before_arrival = local.world_carriage.position;
            BeginRoadBookArrivalState(&sim, &local);
            float arrival_distance = hypotf(
                before_arrival.x - local.world_carriage.position.x,
                before_arrival.z - local.world_carriage.position.z);
            passed = passed && local.world_carriage.storybook_travel &&
                local.world_carriage.town_arrival &&
                local.world_carriage.arrival_travel_weight == 1.0f &&
                arrival_distance < 0.1f;
            for (int frame = 0; frame < 180 && RoadBookArrivalInProgress(&local); ++frame) {
                UpdateOpenWorldCamera(&sim, &local, 1.0f / 60.0f);
            }
            passed = passed && local.arrival.phase == CC_CLIENT_ARRIVAL_TOWN;
            LeaveOpenWorld(&local);
        }
        if (!CcJournalClose(&journal, &sim, error, sizeof(error))) passed = false;
        journal = CcJournalResume(path, &restored, error, sizeof(error));
        passed = passed && journal != NULL &&
            CcSimHash(&sim) == CcSimHash(&restored);
        if (journal != NULL &&
            !CcJournalClose(&journal, &restored, error, sizeof(error))) passed = false;
        (void)remove(path);
        char sidecar[104];
        (void)snprintf(sidecar, sizeof(sidecar), "%s-wal", path);
        (void)remove(sidecar);
        (void)snprintf(sidecar, sizeof(sidecar), "%s-shm", path);
        (void)remove(sidecar);
        if (!passed) {
            (void)fprintf(stderr, "Storybook scenario %d failed: %s\n", scenario, error);
            return 1;
        }
    }
    {
        /* Stopping hands the road back to the town's own walk, so the two are
           one world: travelling offers the step down, and standing on the
           verge offers the way back aboard. */
        local.journey_travel_active = true;
        local.open_world = true;
        sim.journey.active = true;
        sim.journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
        sim.road_site_count = 0;
        local.world_carriage.hero_embarked = true;
        local.carriage_stopped = true;
        local.road_actions_expanded = true;
        ContextActionSet aboard = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
        bool offers_step_down = false, offers_board = false;
        for (int32_t i = 0; i < aboard.count; ++i)
            if (aboard.items[i].kind == CONTEXT_ACTION_STEP_DOWN) offers_step_down = true;
        local.world_carriage.hero_embarked = false;
        ContextActionSet afoot = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
        for (int32_t i = 0; i < afoot.count; ++i)
            if (afoot.items[i].kind == CONTEXT_ACTION_BOARD_CARRIAGE) offers_board = true;
        local.world_carriage.hero_embarked = true;
        if (!offers_step_down) {
            (void)fprintf(stderr, "Travel must offer the step down.\n");
            return 1;
        }
        if (!offers_board) {
            (void)fprintf(stderr, "Standing on the verge must offer the way back aboard.\n");
            return 1;
        }
    }
    for (int32_t turn = 0; turn < 4; ++turn) {
        /* The travel view is a side-scroller: the camera stands square to the
           heading rather than behind it, so the company passes in profile and
           the land parallaxes. Checked on four headings because the road
           curves and the framing must hold all the way round. */
        float heading = (float)turn * 0.5f * PI + 0.37f;
        CcLocalWorldCarriageState framed = local.world_carriage;
        framed.camera_heading_yaw = heading;
        framed.camera_weight = 0.0f;
        float forward_x = sinf(heading), forward_z = cosf(heading);
        Camera3D camera = CcLocalStorybookCameraInternal(
            &local.world_stream.manifest, &framed);
        float flat_x = camera.position.x - framed.position.x;
        float flat_z = camera.position.z - framed.position.z;
        float span = sqrtf(flat_x * flat_x + flat_z * flat_z);
        float view_x = camera.target.x - camera.position.x;
        float view_z = camera.target.z - camera.position.z;
        float view_span = sqrtf(view_x * view_x + view_z * view_z);
        float lead = (camera.target.x - framed.position.x) * forward_x +
                     (camera.target.z - framed.position.z) * forward_z;
        if (span < 12.0f)
            { (void)fprintf(stderr, "The travel camera must stand off the verge.\n"); return 1; }
        if (fabsf(flat_x * forward_x + flat_z * forward_z) > span * 0.02f)
            { (void)fprintf(stderr, "The travel camera must stand square to the heading, not behind it.\n"); return 1; }
        if (view_span < 0.001f ||
            (-view_z * forward_x + view_x * forward_z) / view_span < 0.9f)
            { (void)fprintf(stderr, "Travel must read left to right.\n"); return 1; }
        if (lead <= 0.0f)
            { (void)fprintf(stderr, "The frame must lead the company down the road.\n"); return 1; }
        if (camera.target.y <= framed.position.y)
            { (void)fprintf(stderr, "The company must sit below the centre of the frame.\n"); return 1; }
    }
    (void)puts("Storybook travel: time, warning, rest, arrival, profile framing and journal replay passed");
    return 0;
}

static int RunRoadCarriageTargetRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char error[192] = "", message[192] = "";
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    int32_t opening = OpeningSituationIndex(&sim);
    const CcSituation *offer = opening >= 0 ? &sim.situations[opening] : NULL;
    const CcSettlement *origin = CcSimSettlement(&sim, sim.player.location_id);
    const CcRoute *road = NULL;
    for (int32_t i = 0; i < sim.route_count; ++i)
        if (sim.routes[i].from_id == sim.player.location_id) { road = &sim.routes[i]; break; }
    const CcSettlement *destination = road != NULL ?
        CcSimSettlement(&sim, RouteOtherEnd(road, sim.player.location_id)) : NULL;
    CcCommand listen = {.kind = CC_COMMAND_CHARACTER_RESPONSE,
                        .target_id = offer != NULL ? offer->id : 0U,
                        .amount = CC_CHARACTER_RESPONSE_LISTEN};
    CcCommand pledge = {.kind = CC_COMMAND_CHARACTER_RESPONSE,
                        .target_id = offer != NULL ? offer->id : 0U,
                        .amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP};
    if (offer == NULL || origin == NULL || road == NULL || destination == NULL ||
        strcmp(origin->name, "Thornford") != 0 ||
        strcmp(destination->name, "Gloamgate") != 0 ||
        offer->good != CC_GOOD_BREAD || offer->quantity != 8) {
        return ClientRegressionFailure("Mara's eight Bread delivery must load at Thornford.");
    }
    if (!CcSimApply(&sim, &listen, error, sizeof(error)) ||
        !CcSimApply(&sim, &pledge, error, sizeof(error)) ||
        sim.player.cargo[CC_GOOD_BREAD] != 8) {
        (void)fprintf(stderr, "Mara delivery setup: %s / cargo %d\n", error,
                      sim.player.cargo[CC_GOOD_BREAD]);
        return ClientRegressionFailure("Mara's eight Bread delivery must load at Thornford.");
    }
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = destination->id};
    if (!CcSimApply(&sim, &travel, error, sizeof(error)) ||
        !InitializeOpenWorld(&sim, &local, false)) {
        return ClientRegressionFailure("Set up the road carriage target.");
    }
    if (strcmp(RoadCarriageStatus(&sim), "MOVING") != 0)
        return ClientRegressionFailure("A moving road inspection must report the saved carriage mode.");
    sim.journey.phase = CC_JOURNEY_PHASE_RESTING;
    sim.carriage.mode = CC_CARRIAGE_STOPPED;
    if (strcmp(RoadCarriageStatus(&sim), "RESTING") != 0)
        return ClientRegressionFailure("A road watch must report its saved resting phase.");
    sim.journey.phase = CC_JOURNEY_PHASE_BLOCKED;
    if (strcmp(RoadCarriageStatus(&sim), "BLOCKED") != 0)
        return ClientRegressionFailure("A captain stop must report its saved blocked phase.");
    sim.journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim.carriage.mode = CC_CARRIAGE_MOVING;
    BeginRoadTravelState(&sim, &local);
    if (!local.world_carriage.visible || !local.world_carriage.hero_embarked) {
        return ClientRegressionFailure("Show the carriage on the active road.");
    }
    uint64_t journey_before = CcSimHash(&sim);
    Vector3 anchor = local.world_carriage.position;
    local.world_carriage.hero_embarked = false;
    local.carriage_stopped = true;
    local.agent.position.x += 18.0f;
    Vector3 before_approach = local.agent.position;
    ClientView view = VIEW_LOCAL;
    if (!HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_BOARD_CARRIAGE, (CcInteractionKey){0},
        1.0f / 60.0f, message, sizeof(message)) ||
        !local.interaction.approaching || local.world_carriage.hero_embarked ||
        fabsf(local.agent.position.x - before_approach.x) > 0.001f) {
        return ClientRegressionFailure("Distant boarding must begin an approach without moving the traveller.");
    }
    queued_key_press[KEY_W] = true;
    bool cancelled = HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_NONE, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message));
    ClientInputClearPressed();
    if (cancelled || local.interaction.approaching) {
        return ClientRegressionFailure("Manual movement must cancel the carriage approach.");
    }
    queued_key_press[KEY_F] = true;
    bool keyboard_started = HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_NONE, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message));
    ClientInputClearPressed();
    if (!keyboard_started || !local.interaction.approaching) {
        return ClientRegressionFailure("F must start the same carriage approach as the visible target.");
    }
    queued_key_press[KEY_ESCAPE] = true;
    cancelled = HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_NONE, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message));
    ClientInputClearPressed();
    if (cancelled || local.interaction.approaching) {
        return ClientRegressionFailure("Escape must cancel the carriage approach.");
    }
    queued_key_press[KEY_F] = true;
    keyboard_started = HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_NONE, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message));
    ClientInputClearPressed();
    if (!keyboard_started || !local.interaction.approaching) {
        return ClientRegressionFailure("F must restart a cancelled carriage approach.");
    }
    cancelled = HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_HOLD_TRAVEL, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message));
    if (cancelled || local.interaction.approaching) {
        return ClientRegressionFailure("Selecting another road action must cancel the carriage approach.");
    }
    queued_key_press[KEY_F] = true;
    keyboard_started = HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_NONE, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message));
    ClientInputClearPressed();
    if (!keyboard_started || !local.interaction.approaching) {
        return ClientRegressionFailure("The carriage approach must resume after another action.");
    }
    CcInteractionTarget target = {0};
    if (!RoadCarriageTarget(&sim, &local, &target)) {
        return ClientRegressionFailure("Build the visible road carriage target.");
    }
    local.agent.position.x = target.approach_x;
    local.agent.position.z = target.approach_z;
    if (!HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_NONE, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message)) || !local.world_carriage.hero_embarked ||
        CcSimHash(&sim) != journey_before) {
        return ClientRegressionFailure("Arrival at the carriage must board without changing the journey.");
    }
    local.world_carriage.hero_embarked = false;
    local.carriage_stopped = true;
    local.agent.position.x = target.approach_x - 12.0f;
    local.agent.position.z = target.approach_z;
    if (!HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_BOARD_CARRIAGE, (CcInteractionKey){0},
        1.0f / 60.0f, message, sizeof(message))) {
        return ClientRegressionFailure("Start the stale carriage approach.");
    }
    sim.carriage.progress_milli += 1;
    if (!HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_NONE, (CcInteractionKey){0}, 1.0f / 60.0f,
        message, sizeof(message)) || local.interaction.approaching ||
        local.world_carriage.hero_embarked || strstr(message, "moved") == NULL) {
        return ClientRegressionFailure("A changed carriage revision must reject boarding.");
    }
    sim.carriage.progress_milli -= 1;
    local.world_carriage.hero_embarked = true;
    if (!HandleRoadCarriageInteraction(&sim, &local, &view,
        CONTEXT_ACTION_INSPECT_CARRIAGE, (CcInteractionKey){0},
        1.0f / 60.0f, message, sizeof(message)) || view != VIEW_CARRIAGE ||
        !local.carriage_inspection_road || CcSimHash(&sim) != journey_before ||
        hypotf(anchor.x - local.world_carriage.position.x,
               anchor.z - local.world_carriage.position.z) > 0.001f) {
        return ClientRegressionFailure("Carriage inspection must preserve the road anchor and journey.");
    }
    local.carriage_inspection_road = false;
    LeaveOpenWorld(&local);
    (void)puts("Road carriage target: approach, revision, inspection, and Bread manifest passed");
    return 0;
}

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
        strcmp(message, "The carriage is parked in town.") != 0) {
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

static bool CarriagePublishedMatchesPhysical(const CcLocalWorldCarriageState *c)
{
    return c->presentation_valid &&
        fabsf(c->position.x - c->render_position.x) < 0.0001f &&
        fabsf(c->position.y - c->render_position.y) < 0.0001f &&
        fabsf(c->position.z - c->render_position.z) < 0.0001f &&
        fabsf(remainderf(c->heading_yaw - c->render_heading_yaw, 2 * PI)) < 0.0001f &&
        fabsf(c->travelled - c->render_travelled) < 0.0001f;
}

#include "../../tests/carriage_overview_tests.inc"

static int RunCarriageClientRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcRoute *route = &sim.routes[0];
    route->closed = false;
    route->security = 100;
    sim.player.location_id = route->from_id;
    sim.carriage.location_id = route->from_id;
    char error[256] = "";
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = route->to_id};
    if (!CcSimApply(&sim, &travel, error, sizeof(error))) return 1;
    sim.journey.road_position_active = false;
    sim.journey.road_waiting_choice = false;
    sim.journey.encounter_triggered = true;
    sim.journey.ambush_pending = false;
    /* Retain the generated duration, carriage speed and mandatory site
       records so the journal validates the same state the client uses. */
    ResetLocalState(&local);
    if (!InitializeOpenWorld(&sim, &local, false)) return 1;
    BeginRoadTravelState(&sim, &local);
    const char *path = "carriage-client-regression.sqlite";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) {
        fprintf(stderr, "Carriage fixture journal: %s\n", error);
        return 1;
    }
    bool passed = true;
    float previous = local.world_carriage.render_travelled;
    float initial_distance = previous;
    for (int frame = 0; frame < 360 && passed; ++frame) {
        const float cadence[] = {1.0f/120, 1.0f/30, 1.0f/120, 1.0f/40};
        float dt = cadence[frame % 4];
        int32_t steps = CcLocalWorldUpdateNoGaits(&local.course, NULL, &sim, dt, false, false);
        passed = AdvanceCarriagePresentationSteps(journal, &sim, &local, steps,
            frame < 120 ? 0.5f : frame < 240 ? 1.0f : 2.0f, false,
            error, sizeof(error));
        float alpha = CcLocalCourseAlpha(&local.course);
        CcLocalCarriageInterpolate(&local.world_carriage, alpha);
        CcLocalCarriageGaitInterpolateInternal(alpha);
        float current = local.world_carriage.render_travelled;
        passed = passed && current + 0.0001f >= previous;
        previous = current;
    }
    if (previous <= initial_distance + 0.001f) {
        (void)snprintf(error, sizeof(error), "fixture never moved");
        passed = false;
    }
    if (!CcJournalClose(&journal, &sim, error, sizeof(error))) passed = false;
    (void)remove(path);
    (void)remove("carriage-client-regression.sqlite-wal");
    (void)remove("carriage-client-regression.sqlite-shm");
    LeaveOpenWorld(&local);
    if (!passed) fprintf(stderr, "Carriage client-order regression: %s\n", error);
    else puts("PASS carriage client order: journal, local steps, route publication and pony targets");
    return passed ? 0 : 1;
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
    if (rising_actions.count != 1 || rising_actions.items[0].kind != CONTEXT_ACTION_NONE || rising_actions.items[0].enabled) {
        (void)fprintf(stderr,
                      "The departure needs its Travel control until the junction.\n");
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
    if (!CarriagePublishedMatchesPhysical(&local.world_carriage)) {
        fprintf(stderr, "Departure did not publish its displayed heading and distance.\n");
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
    CcAthleticProfile expected_athletics = NonDefaultAthleticProfile();
    natural.agent.athletics = expected_athletics;
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
    if (!CarriagePublishedMatchesPhysical(&natural.world_carriage)) {
        fprintf(stderr, "Arrival left stale presentation fields.\n");
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
        !AthleticProfilesMatch(&natural.agent.athletics,
                               &expected_athletics) ||
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
    if (!AthleticProfilesMatch(&natural.agent.athletics,
                               &expected_athletics)) {
        (void)fprintf(stderr,
                      "Town parking changed the athletics profile.\n");
        return 1;
    }

    LocalState reduced_motion = {0};
    ResetLocalState(&reduced_motion);
    if (!InitializeOpenWorld(&sim, &reduced_motion, false) ||
        !EnterOpenWorldAtRoadGate(&sim, &reduced_motion, route->id)) {
        (void)fprintf(stderr, "Reduced-motion arrival setup failed.\n");
        return 1;
    }
    reduced_motion.agent.athletics = expected_athletics;
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
        !AthleticProfilesMatch(&reduced_motion.agent.athletics,
                               &expected_athletics) ||
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
    const CcRoute *fallback_route = NULL;
    int32_t fallback_route_index = -1;
    const CcRoute *route = NULL;
    int32_t route_index = -1;
    int32_t incident_routes = 0;
    for (int32_t i = 0; i < sim.route_count; ++i) {
        if (sim.routes[i].from_id != sim.player.location_id &&
            sim.routes[i].to_id != sim.player.location_id) {
            continue;
        }
        incident_routes += 1;
        if (incident_routes == 1) {
            fallback_route = &sim.routes[i];
            fallback_route_index = i;
        }
        if (incident_routes == 2) {
            route = &sim.routes[i];
            route_index = i;
            break;
        }
    }
    if (fallback_route == NULL || route == NULL) {
        return SessionStartupTestFailed(
            session_path, "World session needs a second road branch.");
    }
    BeginRoadChoiceApproachState(&saved, true);
    saved.convoy.pace = 1.0f;
    bool reached_road_book = false;
    for (int32_t step = 0; step < 80 && !reached_road_book; ++step) {
        reached_road_book = UpdateRoadChoiceApproach(&saved, 0.25f);
    }
    if (!reached_road_book ||
        !EnterRoadBookFromTownGate(&sim, &saved, route->id)) {
        return SessionStartupTestFailed(
            session_path, "World session departure setup failed.");
    }
    if (LocalSessionEligible(&saved) ||
        CommandActionEnabled(COMMAND_ACTION_SAVE, &saved, VIEW_ROADS)) {
        return SessionStartupTestFailed(
            session_path, "A moving road-book transition was saveable.");
    }
    for (int32_t step = 0;
         step < 40 && RoadBookDepartureInProgress(&saved); ++step) {
        UpdateOpenWorldCamera(&sim, &saved, 0.10f);
    }
    if (!StableWorldRoadChoice(&saved) ||
        !LocalSessionEligible(&saved) ||
        !CommandActionEnabled(COMMAND_ACTION_SAVE, &saved, VIEW_ROADS)) {
        return SessionStartupTestFailed(
            session_path, "The ready road-book state was not saveable.");
    }
    const CcWorldRoutePlacement *route_placement =
        CcWorldRoutePlacementForId(
            &saved.world_stream.manifest, route->id);
    if (route_placement == NULL) {
        return SessionStartupTestFailed(
            session_path, "World session road placement was missing.");
    }
    const float saved_x = saved.agent.position.x;
    const float saved_z = saved.agent.position.z;
    const float saved_yaw = saved.agent.facing_yaw;
    const float saved_route_amount = saved.world_carriage.route_amount;

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
        restored.arrival.phase != CC_CLIENT_ARRIVAL_PARKED ||
        restored.convoy.phase != CC_LOCAL_CONVOY_ROAD ||
        restored.convoy.phase_progress != 1.0f ||
        restored.convoy.pace != 0.0f ||
        !restored.world_carriage.visible ||
        !restored.world_carriage.hero_embarked ||
        restored.world_carriage.town_arrival ||
        restored.world_carriage.route_id != route->id ||
        !SessionTestFloatMatches(
            restored.world_carriage.route_amount, saved_route_amount) ||
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

    CcWorldPoint branch_position;
    float branch_heading = 0.0f;
    int32_t junction_sample = route->from_id == sim.player.location_id ?
        CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE :
        CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE;
    float junction_route_amount = CcWorldRouteSampleAmount(
        route_placement, junction_sample);
    float junction_journey_amount =
        route->from_id == sim.player.location_id ?
            junction_route_amount : 1.0f - junction_route_amount;
    const float branch_journey_amount = junction_journey_amount +
        (1.0f - junction_journey_amount) * 0.18f;
    const float branch_route_amount =
        route->from_id == sim.player.location_id ?
            branch_journey_amount : 1.0f - branch_journey_amount;
    if (route_placement == NULL ||
        !CcWorldRoutePose(
            route_placement, sim.player.location_id, branch_journey_amount,
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
        !SessionTestFloatMatches(
            version_three_restore.world_carriage.route_amount,
            branch_route_amount) ||
        version_three_selected != route_index ||
        !SessionTestFloatMatches(
            version_three_restore.agent.position.x, branch_position.x) ||
        !SessionTestFloatMatches(
            version_three_restore.agent.position.z, branch_position.z)) {
        return SessionStartupTestFailed(
            session_path, "Version 3 world session chose the wrong branch.");
    }

    CcWorldPoint shared_gate_position;
    float shared_gate_heading = 0.0f;
    const CcWorldSettlementPlacement *branch_place =
        CcWorldSettlementPlacementForId(
            &restored.world_stream.manifest, sim.player.location_id);
    if (branch_place == NULL ||
        !CcWorldRoutePose(
            route_placement, sim.player.location_id,
            0.0f,
            &shared_gate_position, &shared_gate_heading) ||
        fabsf(shared_gate_position.x - branch_place->gate.x) > 0.0011f ||
        fabsf(shared_gate_position.z - branch_place->gate.z) > 0.0011f ||
        !WriteVersionThreeWorldSession(
            session_path, &sim, shared_gate_position,
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
    float shared_gate_route_amount =
        fallback_route->from_id == sim.player.location_id ? 0.0f : 1.0f;
    bool shared_gate_restored = RestoreClientStartupSession(
            session_path, &sim, &shared_gate_restore, &view,
            &shared_gate_selected);
    if (!shared_gate_restored || view != VIEW_ROADS ||
        !shared_gate_restore.open_world ||
        shared_gate_restore.world_carriage.route_id != fallback_route->id ||
        !SessionTestFloatMatches(
            shared_gate_restore.world_carriage.route_amount,
            shared_gate_route_amount) ||
        shared_gate_selected != fallback_route_index ||
        fabsf(shared_gate_restore.agent.position.x -
              branch_place->gate.x) > 0.0011f ||
        fabsf(shared_gate_restore.agent.position.z -
              branch_place->gate.z) > 0.0011f) {
        (void)fprintf(
            stderr,
            "Version 3 shared gate: restored=%d route=%llu expected=%llu "
            "selected=%d expected_selected=%d amount=%.9g expected_amount=%.9g\n",
            shared_gate_restored ? 1 : 0,
            (unsigned long long)shared_gate_restore.world_carriage.route_id,
            (unsigned long long)fallback_route->id,
            shared_gate_selected, fallback_route_index,
            shared_gate_restore.world_carriage.route_amount,
            shared_gate_route_amount);
        return SessionStartupTestFailed(
            session_path, "Version 3 gate heading chose the wrong branch.");
    }

    CcWorldPoint legacy_gate_position = {0};
    const float legacy_gate_heading = 2.15923429f;
    CcWorldPoint migrated_gate_position;
    float migrated_gate_heading = 0.0f;
    if (route->id != UINT64_C(216172782113783819) ||
        !LegacyVersionThreeRoadGatePosition(
            &restored.world_stream.manifest, route_placement,
            sim.player.location_id, &legacy_gate_position) ||
        !CcWorldRoutePose(
            route_placement, sim.player.location_id,
            junction_journey_amount,
            &migrated_gate_position, &migrated_gate_heading) ||
        fabsf(migrated_gate_position.x -
              branch_place->junction.x) > 0.0011f ||
        fabsf(migrated_gate_position.z -
              branch_place->junction.z) > 0.0011f ||
        !WriteVersionThreeWorldSession(
            session_path, &sim, legacy_gate_position,
            legacy_gate_heading)) {
        return SessionStartupTestFailed(
            session_path, "Legacy version 3 gate setup failed.");
    }
    LocalState legacy_gate_restore = {0};
    ResetLocalState(&legacy_gate_restore);
    if (!InitializeOpenWorld(&sim, &legacy_gate_restore, false)) {
        return SessionStartupTestFailed(
            session_path, "Legacy version 3 stream setup failed.");
    }
    view = VIEW_LOCAL;
    int32_t legacy_gate_selected = -1;
    bool legacy_gate_restored = RestoreClientStartupSession(
            session_path, &sim, &legacy_gate_restore, &view,
            &legacy_gate_selected);
    if (!legacy_gate_restored || view != VIEW_ROADS ||
        !legacy_gate_restore.open_world ||
        legacy_gate_restore.world_carriage.route_id != route->id ||
        !SessionTestFloatMatches(
            legacy_gate_restore.world_carriage.route_amount,
            junction_route_amount) ||
        legacy_gate_selected != route_index ||
        !SessionTestFloatMatches(
            legacy_gate_restore.agent.position.x,
            migrated_gate_position.x) ||
        !SessionTestFloatMatches(
            legacy_gate_restore.agent.position.z,
            migrated_gate_position.z) ||
        !SessionTestFloatMatches(
            legacy_gate_restore.agent.facing_yaw,
            migrated_gate_heading)) {
        (void)fprintf(
            stderr,
            "Legacy version 3 gate: restored=%d view=%d world=%d "
            "route=%llu expected=%llu selected=%d expected_selected=%d\n"
            "amount=%.9g expected_amount=%.9g position=(%.9g, %.9g) "
            "expected_position=(%.9g, %.9g) yaw=%.9g expected_yaw=%.9g\n",
            legacy_gate_restored ? 1 : 0, (int)view,
            legacy_gate_restore.open_world ? 1 : 0,
            (unsigned long long)legacy_gate_restore.world_carriage.route_id,
            (unsigned long long)route->id,
            legacy_gate_selected, route_index,
            legacy_gate_restore.world_carriage.route_amount,
            junction_route_amount,
            legacy_gate_restore.agent.position.x,
            legacy_gate_restore.agent.position.z,
            migrated_gate_position.x, migrated_gate_position.z,
            legacy_gate_restore.agent.facing_yaw,
            migrated_gate_heading);
        return SessionStartupTestFailed(
            session_path, "Legacy version 3 gate did not migrate.");
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
    CcAthleticProfile expected_athletics = NonDefaultAthleticProfile();
    saved.agent.athletics = expected_athletics;
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
        !AthleticProfilesMatch(&restored.agent.athletics,
                               &expected_athletics) ||
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

    /* These edge positions can be saved after normal town movement. */
    const Vector2 edge_positions[] = {
        {28.9274921f, 0.340259194f},
        {0.34f, 22.75f},
        {CC_LOCAL_WORLD_WIDTH - 0.34f, 22.75f},
        {31.25f, CC_LOCAL_WORLD_DEPTH - 0.34f},
        {0.0f, 0.0f},
        {CC_LOCAL_WORLD_WIDTH, CC_LOCAL_WORLD_DEPTH}
    };
    for (size_t i = 0; i < sizeof(edge_positions) / sizeof(edge_positions[0]);
         ++i) {
        RepositionHero(&saved, edge_positions[i], false);
        saved.agent.athletics = expected_athletics;
        saved.agent.facing_yaw = 0.982618093f;
        if (!SaveLocalSession(session_path, &sim, &saved,
                              error, sizeof(error))) {
            return SessionStartupTestFailed(session_path, error);
        }
        view = VIEW_ROADS;
        if (!RestoreClientStartupSession(
                session_path, &sim, &restored, &view, &selected) ||
            view != VIEW_LOCAL || restored.open_world ||
            !SessionTestFloatMatches(restored.agent.position.x,
                                     edge_positions[i].x) ||
            !SessionTestFloatMatches(restored.agent.position.z,
                                     edge_positions[i].y) ||
            !SessionTestFloatMatches(restored.agent.facing_yaw,
                                     saved.agent.facing_yaw) ||
            !AthleticProfilesMatch(&restored.agent.athletics,
                                   &expected_athletics)) {
            return SessionStartupTestFailed(
                session_path, "Saved town edge position needs recovery.");
        }
    }

    const Vector2 outside_positions[] = {
        {-0.01f, 22.75f}, {CC_LOCAL_WORLD_WIDTH + 0.01f, 22.75f},
        {31.25f, -0.01f}, {31.25f, CC_LOCAL_WORLD_DEPTH + 0.01f}
    };
    for (size_t i = 0;
         i < sizeof(outside_positions) / sizeof(outside_positions[0]); ++i) {
        stored.position_x = outside_positions[i].x;
        stored.position_z = outside_positions[i].y;
        if (!CcClientSessionWrite(session_path, &stored,
                                  error, sizeof(error))) {
            return SessionStartupTestFailed(session_path, error);
        }
        if (RestoreClientStartupSession(
                session_path, &sim, &restored, &view, &selected)) {
            return SessionStartupTestFailed(
                session_path, "Town restore accepted an outside position.");
        }
    }

    CcLocalBindOpenWorld(NULL);
    (void)remove(session_path);
    (void)puts("Town session startup regression passed");
    return 0;
}

static bool EncounterActorTestMatches(
    const CcClientEncounterActor *first,
    const CcClientEncounterActor *second)
{
    if (!SessionTestFloatMatches(first->position_x, second->position_x) ||
        !SessionTestFloatMatches(first->position_y, second->position_y) ||
        !SessionTestFloatMatches(first->position_z, second->position_z) ||
        !SessionTestFloatMatches(first->velocity_x, second->velocity_x) ||
        !SessionTestFloatMatches(first->velocity_y, second->velocity_y) ||
        !SessionTestFloatMatches(first->velocity_z, second->velocity_z) ||
        !SessionTestFloatMatches(first->facing_yaw, second->facing_yaw) ||
        !SessionTestFloatMatches(first->health, second->health) ||
        !SessionTestFloatMatches(first->posture, second->posture) ||
        !SessionTestFloatMatches(first->stagger_seconds,
                                 second->stagger_seconds) ||
        !SessionTestFloatMatches(first->respawn_seconds,
                                 second->respawn_seconds) ||
        !SessionTestFloatMatches(first->auto_attack_cooldown,
                                 second->auto_attack_cooldown) ||
        first->target_index != second->target_index ||
        first->queued_skill != second->queued_skill ||
        first->active_skill != second->active_skill ||
        first->life_state != second->life_state ||
        first->weapon_mode != second->weapon_mode) {
        return false;
    }
    for (int32_t skill = 0;
         skill < CC_CLIENT_SESSION_SKILL_COUNT; ++skill) {
        if (!SessionTestFloatMatches(first->skill_cooldown[skill],
                                     second->skill_cooldown[skill])) {
            return false;
        }
    }
    return true;
}

static bool RoadEncounterTestMatches(
    const CcClientRoadEncounter *first,
    const CcClientRoadEncounter *second)
{
    if (first->mode != second->mode ||
        !EncounterActorTestMatches(&first->player, &second->player) ||
        !SessionTestFloatMatches(first->engagement_time,
                                 second->engagement_time) ||
        first->raider_resolve != second->raider_resolve ||
        first->alarm_active != second->alarm_active ||
        first->raiders_retreating != second->raiders_retreating) {
        return false;
    }
    for (int32_t guard = 0;
         guard < CC_CLIENT_SESSION_GUARD_COUNT; ++guard) {
        if (!EncounterActorTestMatches(&first->guards[guard],
                                       &second->guards[guard]) ||
            first->guard_duty[guard] != second->guard_duty[guard] ||
            first->guard_response_stage[guard] !=
                second->guard_response_stage[guard]) {
            return false;
        }
    }
    for (int32_t raider = 0;
         raider < CC_CLIENT_SESSION_RAIDER_COUNT; ++raider) {
        if (!EncounterActorTestMatches(&first->raiders[raider],
                                       &second->raiders[raider]) ||
            first->raider_response_stage[raider] !=
                second->raider_response_stage[raider]) {
            return false;
        }
    }
    return true;
}

static int RunRoadEncounterSessionRegression(void)
{
    const char *session_path = "road-encounter-session-test.state";
    (void)remove(session_path);
    for (int32_t path = 0; path < 4; ++path) {
        bool town = path >= 2;
        bool hostile = path % 2 == 0;
        CcSim sim;
        CcSimInit(&sim, UINT32_C(0xc0a71360) + (uint32_t)path);
        sim.journey.active = !town;
        sim.journey.phase = CC_JOURNEY_PHASE_BLOCKED;
        sim.journey.origin_id = sim.player.location_id;

        LocalState saved = {0};
        ResetLocalState(&saved);
        if (!town) BeginRoadLocalState(&sim, &saved, hostile);
        else saved.course.alarm_active = true;
        CcAthleticProfile expected_athletics = NonDefaultAthleticProfile();
        saved.agent.athletics = expected_athletics;
        saved.agent.position.x += 0.75f;
        saved.agent.position.z -= 0.35f;
        saved.agent.velocity.x = 0.42f;
        saved.agent.facing_yaw = -0.72f;
        saved.agent.combat.health = hostile ? 63.5f : 88.0f;
        saved.agent.combat.posture = hostile ? 41.25f : 76.0f;
        saved.agent.combat.stagger_seconds = hostile ? 0.28f : 0.0f;
        saved.agent.combat.auto_attack_cooldown = hostile ? 0.44f : 0.0f;
        saved.agent.combat.skill_cooldown[CC_COMBAT_SKILL_SUNDER] = 1.25f;
        saved.course.engagement_time = hostile ? 7.5f : 0.0f;
        saved.course.raider_resolve = hostile ? 52 :
                                               saved.course.raider_resolve;
        saved.course.runners[1].agent.position.z += 0.66f;
        saved.course.runners[1].agent.combat.health = 54.0f;
        saved.course.runners[1].agent.combat.posture = 32.0f;
        saved.course.runners[1].response_stage = hostile ? 2 : 1;
        saved.course.raiders[0].position.x -= 0.48f;
        saved.course.raiders[0].combat.health = hostile ? 38.0f : 93.0f;
        saved.course.raiders[0].combat.posture = hostile ? 17.0f : 81.0f;
        saved.course.raider_response_stage[0] = hostile ? 3 : 1;
        saved.course.raiders[1].combat.health = 0.0f;
        saved.course.raiders[1].combat.posture = 0.0f;
        saved.course.raiders[1].combat.life_state = CC_LIFE_DEAD;
        saved.course.raiders[1].combat.weapon_mode =
            CC_WEAPON_RAGDOLL_ATTACHED;
        if (path == 3) {
            saved.agent.combat.health = 0.0f;
            saved.agent.combat.life_state = CC_LIFE_DEAD;
            saved.site_travel_active = true;
            saved.site_travel_progress = 0.42f;
            saved.site_returning = true;
        }

        CcClientRoadEncounter expected = {0};
        CaptureRoadEncounter(&expected, &saved);
        char error[192];
        if (!SaveLocalSession(session_path, &sim, &saved,
                              error, sizeof(error))) {
            return SessionStartupTestFailed(session_path, error);
        }

        LocalState restored = {0};
        ResetLocalState(&restored);
        ClientView view = VIEW_ROADS;
        int32_t selected = -1;
        if (!RestoreClientStartupSession(
                session_path, &sim, &restored, &view, &selected)) {
            return SessionStartupTestFailed(
                session_path, "Road encounter did not resume.");
        }
        CcClientRoadEncounter actual = {0};
        CaptureRoadEncounter(&actual, &restored);
        if (view != VIEW_LOCAL || restored.open_world ||
            restored.course.road_encounter != !town ||
            restored.journey_combat_active != (!town && hostile) ||
            restored.journey_parley_active != (!town && !hostile) ||
            restored.site_travel_active != saved.site_travel_active ||
            !SessionTestFloatMatches(restored.site_travel_progress, saved.site_travel_progress) ||
            !AthleticProfilesMatch(&restored.agent.athletics,
                                   &expected_athletics) ||
            !RoadEncounterTestMatches(&expected, &actual)) {
            return SessionStartupTestFailed(
                session_path,
                hostile ? "Fight state changed after resume." :
                          "Parley state changed after resume.");
        }
    }
    (void)remove(session_path);
    (void)puts("Road encounter session regression passed");
    return 0;
}
#endif

static bool SaveClientWorld(CcJournal *journal, CcSim *sim,
                            const LocalState *local,
                            const char *save_path, const char *session_path,
                            char *save_feedback, size_t save_feedback_capacity)
{
    if (CcCoopClientActive()) {
        (void)snprintf(save_feedback, save_feedback_capacity,
                       "The host saves each company action and the shared clock.");
        return true;
    }
#if !defined(PLATFORM_WEB)
    (void)save_path;
#endif
#if defined(PLATFORM_WEB)
    int32_t browser_access = ClientBrowserCampaignAccess();
    if (browser_access != 0) {
        (void)snprintf(
            save_feedback, save_feedback_capacity, "Save blocked. %s",
            ClientBrowserCampaignAccessMessage(browser_access));
        return false;
    }
#endif
    if (local->road_choice_active &&
        !StableWorldRoadChoice(local)) {
        (void)snprintf(
            save_feedback, save_feedback_capacity,
            "Save paused. Choose a road or turn back first.");
        return false;
    }
    if (!LocalSessionEligible(local)) {
        (void)snprintf(
            save_feedback, save_feedback_capacity,
            "Save paused. Finish the current movement first.");
        return false;
    }
    char error[256];
    if (!CcJournalCheckpoint(journal, sim, error, sizeof(error))) {
        (void)snprintf(save_feedback, save_feedback_capacity,
                       "Journal save failed: %.72s", error);
        return false;
    }
    if (!SaveLocalSession(session_path, sim, local,
                          error, sizeof(error))) {
#if defined(PLATFORM_WEB)
        (void)snprintf(
            save_feedback, save_feedback_capacity,
            "Journal is ready in this tab. "
            "Local scene checkpoint failed: %.44s",
            error);
#else
        (void)snprintf(
            save_feedback, save_feedback_capacity,
            "Journal saved. Local scene checkpoint failed: %.52s",
            error);
#endif
        return false;
    }
#if defined(PLATFORM_WEB)
    if (ClientFlushBrowserSaves(save_path, session_path) == 0) {
        int32_t save_access = ClientBrowserCampaignAccess();
        if (save_access == 0) {
            (void)snprintf(
                save_feedback, save_feedback_capacity,
                "Browser storage failed. Journal and local scene "
                "remain in this tab.");
        } else {
            (void)snprintf(
                save_feedback, save_feedback_capacity,
                "Save blocked. %s",
                ClientBrowserCampaignAccessMessage(save_access));
        }
        return false;
    }
#endif
    (void)snprintf(
        save_feedback, save_feedback_capacity,
        "Journal saved. Local scene checkpoint saved.");
    return true;
}

#include "client/cc_frontend.inc"
#include "pony_ui.inc"

static bool HandleCaravanRecovery(LocalState *local, ClientView *view,
                                   ClientView *return_view, double now,
                                   char *message, size_t capacity)
{
    if (now >= local->caravan_tap_deadline)
        local->caravan_tap_deadline = 0.0;
    if (*view != VIEW_LOCAL && *view != VIEW_ROADS && *view != VIEW_CARRIAGE) {
        local->caravan_tap_deadline = 0.0;
        return false;
    }
    if (!ClientKeyPressed(KEY_N)) return false;
    if (local->caravan_tap_deadline <= 0.0) {
        local->caravan_tap_deadline = now + 0.45;
        (void)snprintf(message, capacity,
                       "Tap [N] again to return to the caravan.");
        return true;
    }
    local->caravan_tap_deadline = 0.0;
    Vector2 position = LOCAL_CARRIAGE_BAY;
    float heading = atan2f(LOCAL_CARRIAGE.x - position.x,
                           LOCAL_CARRIAGE.y - position.y);
    CcLocalSceneKind scene = CC_LOCAL_SCENE_STREET;
    if (local->open_world && local->world_carriage.visible) {
        heading = local->world_carriage.heading_yaw;
        position = (Vector2){
            local->world_carriage.position.x + 3.0f * cosf(heading),
            local->world_carriage.position.z - 3.0f * sinf(heading)};
    } else if (local->site_travel_active) {
        Vector3 caravan = WorldActionCarriagePosition(local);
        position = (Vector2){caravan.x + 3.0f, caravan.z};
        scene = CC_LOCAL_SCENE_ROAD;
    } else if (local->site_kind != CC_LOCAL_SITE_NONE) {
        position = (Vector2){CC_LOCAL_SITE_CARRIAGE_X + 3.0f,
                             CC_LOCAL_SITE_CARRIAGE_Z};
        scene = CC_LOCAL_SCENE_ROAD;
    } else if (local->journey_combat_active || local->journey_parley_active ||
               local->journey_travel_active) {
        position = (Vector2){CC_LOCAL_ROAD_START_X, CC_LOCAL_ROAD_START_Z};
        scene = CC_LOCAL_SCENE_ROAD;
    } else if (local->convoy.phase != CC_LOCAL_CONVOY_PARKED) {
        heading = local->convoy.town_heading_yaw;
        position = (Vector2){local->convoy.town_position.x + 3.0f * cosf(heading),
                             local->convoy.town_position.z - 3.0f * sinf(heading)};
    }
    RepositionHero(local, position, false);
    CcLocalAgentSetScene(&local->agent, scene);
    local->agent.facing_yaw = heading;
    local->course.scene = scene;
    local->market_interior = false;
    local->open_world_market = false;
    local->movement_preview = (CcLocalMovementPreview){0};
    local->movement_reticle_valid = false;
    CcInteractionCancel(&local->interaction, "");
    local->interactions = (CcInteractionPlan){0};
    local->request_save = true;
    *view = VIEW_LOCAL;
    *return_view = VIEW_LOCAL;
    (void)snprintf(message, capacity, "Back at the caravan.");
    return true;
}

static bool ResolveSoloPartyWipe(CcJournal *journal, CcSim *sim,
                                  LocalState *local, ClientView *view,
                                  char *message, size_t capacity)
{
    if (local->agent.combat.life_state != CC_LIFE_DEAD) return false;
    CcCommand wipe = {
        .kind = CC_COMMAND_PARTY_WIPE,
        .target_id = (CcId)sim->current_day
    };
    if (!ApplyCommand(journal, sim, wipe, message, capacity)) return false;
    CcAudioClearSpeech();
    LeaveOpenWorld(local);
    ResetLocalStatePreservingAthletics(local);
    CcLocalBindPlace(sim);
    (void)InitializeOpenWorld(sim, local, false);
    *view = VIEW_LOCAL;
    (void)snprintf(message, capacity,
                   "Twenty years later. A new company takes up the carriage.");
    return true;
}

#if defined(CC_CLIENT_SELF_TESTS)
static int RunMineHaulerVisualRegression(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x71a7e5));
    sim.mine.source_owner_id = sim.goblins.id;
    sim.mine.source_x = 26;
    sim.mine.source_y = 16;
    sim.mine.x = 25;
    sim.mine.y = 16;
    CcLocalCourse mine_course = {0};
    CcLocalAgent mine_hero = {0};
    CcLocalCourseStageMineEncounter(&mine_course, &mine_hero, &sim);
    for (int32_t raider = 0; raider < CC_LOCAL_RAIDER_COUNT; ++raider) {
        const CcLocalAgent *hauler = &mine_course.raiders[raider];
        Vector3 ground = CcLocalAgentVisualGroundContact(hauler);
        if (CcLocalAgentVisualFamily(hauler) != CC_LOCAL_ACTOR_VISUAL_GOBLIN ||
            CcLocalAgentVisualScale(hauler) != 0.5f ||
            fabsf(ground.x - hauler->position.x) > 0.0001f ||
            fabsf(ground.y - hauler->position.y) > 0.0001f ||
            fabsf(ground.z - hauler->position.z) > 0.0001f) return 1;
    }
    CcLocalCourse road_course = {0};
    CcLocalAgent road_hero = {0};
    CcLocalCourseStageRoadEncounter(&road_course, &road_hero, false);
    for (int32_t raider = 0; raider < CC_LOCAL_RAIDER_COUNT; ++raider) {
        const CcLocalAgent *road_raider = &road_course.raiders[raider];
        if (CcLocalAgentVisualFamily(road_raider) !=
            CC_LOCAL_ACTOR_VISUAL_HUMAN ||
            CcLocalAgentVisualScale(road_raider) != 1.0f) return 1;
    }
    (void)puts("Mine goblins render at half scale on their ground contact; road raiders stay human at full scale.");
    return 0;
}

static int RunSoloPartyWipeRegression(void)
{
    CcSim sim;
    CcSimInit(&sim, 42U);
    LocalState local = {0};
    ResetLocalState(&local);
    ClientView view = VIEW_LOCAL;
    char message[256];
    int32_t day = sim.current_day;
    local.agent.combat.life_state = CC_LIFE_KNOCKED_DOWN;
    if (ResolveSoloPartyWipe(NULL, &sim, &local, &view, message, sizeof(message)) ||
        sim.current_day != day) return 1;
    CcLocalAgentDie(&local.agent);
    if (!ResolveSoloPartyWipe(NULL, &sim, &local, &view, message, sizeof(message)) ||
        sim.current_day != day + CC_PARTY_WIPE_DAYS || view != VIEW_LOCAL ||
        local.agent.combat.life_state != CC_LIFE_ALIVE ||
        !CcSimValidate(&sim, message, sizeof(message))) return 1;
    uint64_t hash = CcSimHash(&sim);
    if (ResolveSoloPartyWipe(NULL, &sim, &local, &view, message, sizeof(message)) ||
        CcSimHash(&sim) != hash) return 1;
    LeaveOpenWorld(&local);
    static CcSim mine_sim;
    CcSimInit(&mine_sim,UINT32_C(0x51e7));
    const CcRoadSite *site=CcMineSite(&mine_sim);
    const CcRoute *road=CcSimRoute(&mine_sim,site->route_id);
    mine_sim.player.location_id=road->from_id;
    mine_sim.carriage.location_id=road->from_id;
    CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=road->to_id};
    if(!ApplyCommand(NULL,&mine_sim,travel,message,sizeof(message))) return 1;
    mine_sim.pony_company.encounter=-1;
    mine_sim.journey.ambush_pending=false;
    mine_sim.journey.elapsed_subticks=CcMineBranchSubtick(&mine_sim);
    mine_sim.carriage.progress_milli=(int32_t)((int64_t)
        mine_sim.journey.elapsed_subticks*1000/mine_sim.journey.total_subticks);
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=site->id};
    if(!ApplyCommand(NULL,&mine_sim,visit,message,sizeof(message))) return 1;
    mine_sim.mine.phase=CC_MINE_LEVEL;
    mine_sim.mine.x=25;
    mine_sim.mine.y=16;
    mine_sim.mine.light=18;
    mine_sim.mine.seen=UINT32_C(1)<<5;
    mine_sim.player.cargo[CC_GOOD_BREAD]=2;
    mine_sim.player.cargo[CC_GOOD_BREAD]-=2;
    mine_sim.mine.pack[CC_GOOD_BREAD]=2;
    int32_t source_gold=CcMineSourceGood(&mine_sim,CC_GOOD_GOLD);
    mine_sim.mine.encounter_outcome=CC_MINE_ENCOUNTER_CONTESTED;
    mine_sim.mine.source_released=true;
    CcCommand take={.kind=CC_COMMAND_MINE_TAKE,
        .target_id=(CcId)mine_sim.mine.revision,.good=CC_GOOD_GOLD,.amount=1};
    if(!ApplyCommand(NULL,&mine_sim,take,message,sizeof(message)) ||
       CcMinePackGood(&mine_sim,CC_GOOD_GOLD)!=1) return 1;
    mine_sim.mine.contest_active=true;
    ResetLocalState(&local);
    CcLocalCourseStageMineEncounter(&local.course,&local.agent,&mine_sim);
    local.mine_combat_active=true;
    int32_t mine_day=mine_sim.current_day;
    CcId source_id=mine_sim.mine.source_id;
    CcLocalAgentDie(&local.agent);
    if(!ResolveSoloPartyWipe(NULL,&mine_sim,&local,&view,message,sizeof(message)) ||
       mine_sim.current_day!=mine_day+CC_PARTY_WIPE_DAYS ||
       mine_sim.mine.phase!=CC_MINE_NONE || mine_sim.mine.contest_active ||
       mine_sim.mine.encounter_outcome!=CC_MINE_ENCOUNTER_BROKEN_CONTACT ||
       mine_sim.mine.player_injury!=0 || mine_sim.mine.source_id!=source_id ||
       mine_sim.player.cargo[CC_GOOD_BREAD]!=2 ||
       CcMinePackUsed(&mine_sim)!=0 ||
       CcMineSourceGood(&mine_sim,CC_GOOD_GOLD)!=source_gold ||
       !CcSimValidate(&mine_sim,message,sizeof(message))) return 1;
    LeaveOpenWorld(&local);
    travel.target_id=road->to_id;
    if(!ApplyCommand(NULL,&mine_sim,travel,message,sizeof(message))) return 1;
    mine_sim.pony_company.encounter=-1;
    mine_sim.journey.ambush_pending=false;
    mine_sim.journey.elapsed_subticks=CcMineBranchSubtick(&mine_sim);
    mine_sim.carriage.progress_milli=(int32_t)((int64_t)
        mine_sim.journey.elapsed_subticks*1000/mine_sim.journey.total_subticks);
    if(!ApplyCommand(NULL,&mine_sim,visit,message,sizeof(message))) return 1;
    mine_sim.mine.phase=CC_MINE_LEVEL;
    mine_sim.mine.x=25;
    mine_sim.mine.y=16;
    CcCommand contest={.kind=CC_COMMAND_MINE_CONTEST,
        .target_id=(CcId)mine_sim.mine.revision};
    if(!ApplyCommand(NULL,&mine_sim,contest,message,sizeof(message))) return 1;
    ResetLocalState(&local);
    CcLocalCourseStageMineEncounter(&local.course,&local.agent,&mine_sim);
    if(local.agent.combat.life_state!=CC_LIFE_ALIVE ||
       local.agent.combat.health!=CC_LOCAL_COMBAT_MAX_HEALTH ||
       local.course.raider_company_id!=mine_sim.goblins.id) return 1;
    LeaveOpenWorld(&local);
    (void)puts("Solo and mine deaths advance the world once and preserve the mine outcome.");
    return 0;
}
#endif

static bool SetRoadTravelStopped(CcSim *sim, LocalState *local, bool stopped,
    char *message, size_t capacity)
{
    if (CcCoopClientActive() && sim->journey.active &&
        !CcCoopClientSetTravelStopped(sim, stopped, message, capacity)) return false;
    local->carriage_stopped = stopped;
    local->road_actions_expanded = false;
    local->card_page = 0;
    local->travel_hold_armed = false;
    local->travel_fast_forward = false;
    local->convoy.runtime_tick_accumulator = 0.0f;
    if (stopped) {
        local->convoy.pace = 0.0f;
        local->world_carriage.pace = 0.0f;
    }
    (void)snprintf(message, capacity, "%s", stopped ?
        "Stopped. Travel resumes this road; camp spends time and provisions." :
        "Travelling. The team keeps moving until a decision or you stop.");
    return true;
}

static void HandleInput(CcJournal **journal, CcSim *sim, int32_t *selected,
                        int32_t *selected_situation, ClientView *view,
                        ClientView *return_view, LocalState *local,
                        RenderTexture2D local_target, Rectangle local_bounds,
                        float delta_time,
                        const char *save_path, const char *session_path,
                        char *message, size_t message_capacity,
                        char *save_feedback,
                        size_t save_feedback_capacity,
                        float *save_feedback_age)
{
    if (journal == NULL || *journal == NULL) {
        if (message[0] == '\0') {
            (void)snprintf(message, message_capacity,
                           "The campaign journal is unavailable.");
        }
        return;
    }
    if (sim->mine.phase != CC_MINE_NONE && (*view == VIEW_LOCAL || *view == VIEW_ROADS)) {
        bool save_requested=ClientKeyPressed(KEY_F5) || queued_save_shortcut ||
            AdventureHit(MineNavigationControl(0));
        bool menu_requested=ClientKeyPressed(KEY_ESCAPE) ||
            AdventureHit(MineNavigationControl(1));
        if (save_requested) {
            *save_feedback_age=0.0f;
            (void)SaveClientWorld(*journal,sim,local,save_path,session_path,save_feedback,save_feedback_capacity);
#if !defined(PLATFORM_WEB)
            (void)snprintf(message,message_capacity,"%s",save_feedback);
#endif
        } else if(menu_requested) {
            local->pause_return_view=*view;
            *view=VIEW_PAUSE;
        } else HandleMineInput(*journal,sim,local,local_target,delta_time,message,message_capacity);
        return;
    }
    if (HandleCaravanRecovery(local, view, return_view, GetTime(),
                               message, message_capacity)) return;
    if (local->adventure_ui) {
        if (local->interaction.approaching &&
            (ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_promises : KEY_Q) || ClientKeyPressed(KEY_M) || ClientKeyPressed(KEY_TAB))) {
            CcInteractionCancel(&local->interaction, "");
            CcLocalAgentStop(&local->agent);
        }
        if ((*view == VIEW_TRADE || *view == VIEW_CHARACTER || *view == VIEW_OVEN_COURT) &&
            (ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_book : KEY_B) || ClientKeyPressed(KEY_TAB))) {
            if (*view == VIEW_OVEN_COURT) { local->book_page = 3; local->book_offset = 0; }
            *return_view = *view;
            *view = VIEW_LEDGER;
            return;
        }
        if (HandleOvenCourt(sim, local, view, return_view, message, message_capacity)) return;
        bool pause_handled = HandleAdventurePause(local, view, return_view);
        if (pause_handled ||
            HandleAdventureTrade(*journal, sim, local, view, message, message_capacity) ||
            HandleAdventureBook(*journal, sim, local, view, return_view, message, message_capacity)) return;
        if (*view == VIEW_SITUATIONS && AdventureHit(AdventureClose(AdventurePromisesPanel()))) {
            *view = SafeOverlayReturnView(*return_view);
            return;
        }
        if (*view == VIEW_LOCAL && (ClientKeyPressed(KEY_ESCAPE) ||
            AdventureHit(AdventureNavBounds(3)))) {
            if (local->interaction.approaching) {
                CcInteractionCancel(&local->interaction, "");
                CcLocalAgentStop(&local->agent);
            } else {
                local->pause_return_view = *view;
                *view = VIEW_PAUSE;
            }
            return;
        }
        if ((ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_book : KEY_B) || (*view == VIEW_LOCAL && AdventureHit(AdventureNavBounds(0)))) && *view != VIEW_CHARACTER) {
            CcInteractionCancel(&local->interaction, "");
            CcLocalAgentStop(&local->agent);
            ToggleCommandOverlay(VIEW_LEDGER, view, return_view);
            return;
        }
    }
    ContextActionSet available_cards = BuildContextActions(sim, local, *view, *selected, *selected_situation);
    if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) && available_cards.count > ContextActionPageSize(&available_cards)) {
        Vector2 pointer = ClientPointerPosition();
        bool previous = CheckCollisionPointRec(pointer, ContextPageBounds(false));
        bool next = CheckCollisionPointRec(pointer, ContextPageBounds(true));
        if (previous || next) {
            int32_t pages = (available_cards.count + ContextActionPageSize(&available_cards) - 1) / ContextActionPageSize(&available_cards);
            local->card_page = (local->card_page + pages + (next ? 1 : -1)) % pages;
            return;
        }
    }
    ContextAction pressed_action = PressedContextAction(
        sim, local, *view, *selected, *selected_situation);
    if (pressed_action.kind != CONTEXT_ACTION_NONE && !pressed_action.enabled) {
        (void)snprintf(message, message_capacity,
            available_cards.combat ? "%s" :
                "%s Double-tap [N] to return to the caravan.", pressed_action.detail);
        return;
    }
    ContextActionKind context_action = pressed_action.kind;
#if defined(PLATFORM_WEB)
    if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT))
        ClientBrowserContextAction((int)context_action);
#endif
    bool care_key = (IsKeyDown(KEY_LEFT_SHIFT) ||
                     IsKeyDown(KEY_RIGHT_SHIFT)) &&
        ClientKeyPressed(adventure_preferences != NULL ?
            adventure_preferences->key_interact : KEY_F);
    if (*view == VIEW_LOCAL && NearParkedCarriage(sim, local) &&
        (context_action == CONTEXT_ACTION_CARE_HORSES || care_key)) {
        (void)ApplyHorseCare(*journal, sim, message, message_capacity);
        return;
    }
    if (context_action == CONTEXT_ACTION_NONE && ClientKeyPressed(KEY_SPACE)) {
        for (int32_t i = 0; i < available_cards.count; ++i) {
            if (available_cards.items[i].kind == CONTEXT_ACTION_HOLD_TRAVEL &&
                available_cards.items[i].enabled) {
                context_action = CONTEXT_ACTION_HOLD_TRAVEL;
                pressed_action = available_cards.items[i];
                break;
            }
        }
    }
    if (context_action == CONTEXT_ACTION_NONE && *view == VIEW_LOCAL &&
        local->journey_travel_active &&
        sim->journey.road_position_active) {
        for (int32_t i = 0; i < available_cards.count && i < 8; ++i) {
            if (available_cards.items[i].enabled &&
                available_cards.items[i].kind ==
                    CONTEXT_ACTION_CHOOSE_ROAD_LEG &&
                ClientKeyPressed(KEY_ONE + i)) {
                pressed_action = available_cards.items[i];
                context_action = pressed_action.kind;
                break;
            }
        }
    }
    if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) && context_action == CONTEXT_ACTION_NONE &&
        PointerOverContextAction(sim, local, *view, *selected, *selected_situation, ClientPointerPosition())) return;
    CommandActionKind command_action = local->adventure_ui ? COMMAND_ACTION_NONE : PressedCommandAction(local, *view);
    if (local->adventure_ui && *view == VIEW_LOCAL) {
        if (AdventureHit(AdventureNavBounds(1))) command_action = COMMAND_ACTION_MAP;
        if (AdventureHit(AdventureNavBounds(2))) command_action = COMMAND_ACTION_SAVE;
        if (local->journey_travel_active && sim->journey.active && sim->pony_company.encounter < 0) {
            for (int32_t pace = CC_JOURNEY_PACE_CAREFUL; pace <= CC_JOURNEY_PACE_PUSH; ++pace) {
                if (AdventureHit(AdventurePaceBounds(pace))) {
                    context_action = CONTEXT_ACTION_SET_PACE;
                    pressed_action.amount = pace;
                }
            }
        }
    }
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if (local->request_save || command_action == COMMAND_ACTION_SAVE || ClientKeyPressed(KEY_F5) ||
        queued_save_shortcut || (control && ClientKeyPressed(KEY_S))) {
        local->request_save = false;
        *save_feedback_age = 0.0f;
        (void)SaveClientWorld(*journal, sim, local, save_path, session_path,
                              save_feedback, save_feedback_capacity);
        return;
    }

    if (context_action == CONTEXT_ACTION_TRANSFER_ROAD_SITE) {
        CcCommand command = {.kind = CC_COMMAND_TRANSFER_ROAD_SITE,
            .target_id = CcSimJourneyRoadSiteStop(sim) != NULL ? CcSimJourneyRoadSiteStop(sim)->id : 0,
            .good = pressed_action.good, .amount = pressed_action.amount};
        (void)ApplyCommand(*journal, sim, command, message, message_capacity);
        return;
    }
    if (context_action == CONTEXT_ACTION_CLEAR_ROAD_SITE || context_action == CONTEXT_ACTION_REPAIR_ROAD_SITE) {
        const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
        if (site != NULL) {
            CcCommand command = {.kind = context_action == CONTEXT_ACTION_REPAIR_ROAD_SITE ?
                CC_COMMAND_REPAIR_ROAD_SITE : CC_COMMAND_CLEAR_ROAD_SITE, .target_id = site->id};
            (void)ApplyCommand(*journal, sim, command, message, message_capacity);
        }
        return;
    }
    if (context_action == CONTEXT_ACTION_ROAD_OPTIONS) {
        local->road_actions_expanded = !local->road_actions_expanded;
        local->card_page = 0;
        return;
    }
    if (context_action == CONTEXT_ACTION_HOLD_TRAVEL) {
        (void)SetRoadTravelStopped(sim, local, !local->carriage_stopped,
            message, message_capacity);
        return;
    }
    if (context_action == CONTEXT_ACTION_MAKE_ROAD_CAMP) {
        CcCommand camp = {.kind = CC_COMMAND_MAKE_CAMP};
        if (ApplyCommand(*journal, sim, camp, message, message_capacity)) {
            local->carriage_stopped = true;
            local->travel_fast_forward = false;
            local->travel_attention = true;
        }
        return;
    }
    if (context_action == CONTEXT_ACTION_CHOOSE_ROAD_LEG) {
        CcCommand choice = {
            .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
            .target_id = pressed_action.target.object
        };
        if (ApplyCommand(*journal, sim, choice, message, message_capacity)) {
            local->carriage_stopped = false;
            local->road_actions_expanded = false;
            local->card_page = 0;
        }
        return;
    }
    if (HandleRoadCarriageInteraction(sim, local, view, context_action,
                                      pressed_action.target, delta_time,
                                      message, message_capacity)) {
        return;
    }
    if (context_action == CONTEXT_ACTION_STEP_DOWN && local->open_world) {
        local->carriage_stopped = true;
        local->travel_hold_armed = false;
        local->travel_fast_forward = false;
        local->convoy.pace = 0.0f;
        local->world_carriage.pace = 0.0f;
        local->world_carriage.hero_embarked = false;
        local->world_carriage.camera_target = 0.38f;
        local->agent.position = local->world_carriage.position;
        local->agent.facing_yaw = local->world_carriage.heading_yaw;
        CcLocalAgentStop(&local->agent);
        (void)snprintf(message, message_capacity,
                       "The team halts. Walk where you like; the carriage waits.");
        return;
    }
    if (context_action == CONTEXT_ACTION_VISIT_MINE || context_action == CONTEXT_ACTION_PASS_ROAD_SITE) {
        const CcRoadSite *site=CcSimJourneyRoadSiteStop(sim);
        if(site != NULL) {
            CcCommand command={.kind=context_action == CONTEXT_ACTION_VISIT_MINE ? CC_COMMAND_VISIT_MINE : CC_COMMAND_PASS_ROAD_SITE,.target_id=site->id};
            if (ApplyCommand(*journal,sim,command,message,message_capacity)) {
                local->carriage_stopped = false;
                local->road_actions_expanded = false;
                local->card_page = 0;
            }
        }
        return;
    }
    if (HandleCarriageTabs(local, *view)) return;
    if (HandleCarriageOverviewScroll(sim, local, *view)) return;
    if (HandlePonyInput(*journal, sim, local, *view, pressed_action, message, message_capacity)) return;
    if (context_action == CONTEXT_ACTION_STOP_APPROACH) {
        CcInteractionCancel(&local->interaction, "");
        CcLocalAgentStop(&local->agent);
        return;
    }

    if (context_action == CONTEXT_ACTION_APPROACH_ENTRANCE) {
        (void)CcLocalAgentSetExactTarget(&local->agent,
            (Vector3){CC_LOCAL_SITE_ENTRANCE_X, 0, CC_LOCAL_SITE_ENTRANCE_Z}, false);
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
        } else if (ClientKeyPressed(KEY_THREE) ||
                   context_action == CONTEXT_ACTION_WITHDRAW) {
            CcCommand withdraw = {
                .kind = CC_COMMAND_WITHDRAW_ENCOUNTER,
                .amount = 0
            };
            if (ApplyCommand(*journal, sim, withdraw, message,
                             message_capacity)) {
                ResetLocalStatePreservingAthletics(local);
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_LOCAL;
            }
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
        if (ClientKeyPressed(KEY_BACKSPACE)) {
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
        if (ClientKeyPressed(KEY_BACKSPACE) ||
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
    if (local->adventure_ui && *view == VIEW_LOCAL &&
        (context_action == CONTEXT_ACTION_OPEN_TRADE || (ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_interact : KEY_F) && local->open_world_market))) {
        local->trade_quantity = 1;
        local->trade_good = ContextCargoGood(sim);
        local->trade_mode = 0;
        local->trade_confirmed = false;
        local->trade_quote_presented = false;
        *view = VIEW_TRADE;
        return;
    }
    if (command_action == COMMAND_ACTION_NONE &&
        (context_action == CONTEXT_ACTION_NONE || context_action == CONTEXT_ACTION_WORLD_TARGET) &&
        HandleAdventureScene(*journal, sim, local, view, return_view,
            selected_situation,
            context_action == CONTEXT_ACTION_WORLD_TARGET ?
                CcInteractionFind(&local->interactions, pressed_action.target) : NULL,
            delta_time, message, message_capacity)) {
        if (*view == VIEW_CHARACTER && local->conversation_situation_id == 0U &&
            local->conversation_character_id != 0U) {
            if (ApplyCommand(*journal, sim, (CcCommand){.kind = CC_COMMAND_EXCHANGE_GOSSIP,
                    .target_id = local->conversation_character_id}, message, message_capacity)) {
                const CcCharacter *person = CcSimCharacter(sim, local->conversation_character_id);
                if (person != NULL) (void)snprintf(local->conversation_name,
                    sizeof(local->conversation_name), "%s", person->name);
            } else {
                *view = VIEW_LOCAL;
            }
        }
        return;
    }
    if (*view == VIEW_CHARACTER) {
        ContextActionSet replies=BuildContextActions(
            sim,local,VIEW_CHARACTER,*selected,*selected_situation);
        for (int32_t i=0;i<replies.count;++i)
            if (replies.items[i].enabled && ClientKeyPressed(KEY_ONE+i))
                context_action=replies.items[i].kind;
        if (context_action == CONTEXT_ACTION_OVEN_QUESTION) {
            if (ApplyCommand(*journal, sim,
                (CcCommand){.kind = CC_COMMAND_OBSERVE_OVEN_COURT,
                    .target_id = local->conversation_character_id, .amount = 1},
                message, message_capacity)) {
                CcOvenCourtObservation observation;
                if (CcOvenCourtRead(sim, &observation)) {
                    local->conversation_report_response = false;
                    local->conversation_oven_response = true;
                    const CcEvent *note = CcOvenCourtNote(sim, 0);
                    local->conversation_oven_event = note != NULL ? note->id : 0U;
                    (void)snprintf(local->conversation_line, sizeof(local->conversation_line),
                        "%s", CcOvenCourtAdvice(&observation));
                    CcAudioClearSpeech();
                    CcSpeech answer;
                    if (ClientConversationSpeech(sim, local, &answer)) ClientSaySpeech(&answer);
                }
            }
            return;
        }
        if (context_action == CONTEXT_ACTION_MINE_LEAD ||
            context_action == CONTEXT_ACTION_MINE_REPORT) {
            CcCommand command={
                .kind=context_action == CONTEXT_ACTION_MINE_LEAD ?
                    CC_COMMAND_MINE_LEARN_LEAD : CC_COMMAND_MINE_REPORT_RETURN,
                .target_id=(CcId)sim->mine.return_revision,
                .amount=context_action == CONTEXT_ACTION_MINE_LEAD ? 1 : 0};
            if (ApplyCommand(*journal,sim,command,message,message_capacity) &&
                context_action == CONTEXT_ACTION_MINE_REPORT) {
                local->conversation_report_response = true;
                if (sim->mine.report_kind == CC_MINE_RETURN_HAUL) {
                    (void)snprintf(local->conversation_line,
                        sizeof(local->conversation_line),
                        "This proves the turnout can still yield. Oren can handle the sale.");
                } else {
                    (void)snprintf(local->conversation_line,
                        sizeof(local->conversation_line),
                        "This gives the next company a fair path in.");
                }
                CcAudioClearSpeech();
                CcSpeech answer;
                if (ClientConversationSpeech(sim, local, &answer))
                    ClientSaySpeech(&answer);
            }
            return;
        }
        if ((local->adventure_ui && ClientKeyPressed(KEY_ESCAPE)) || ClientKeyPressed(KEY_BACKSPACE) ||
            context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            CcAudioClearSpeech();
            CcCoreConversationReset(&core_conversation); core_conversation_speaker = 0U;
            CcSpeech goodbye;
            const CcSituation *leaving = CcSimSituation(sim, local->conversation_situation_id);
            if (adventure_preferences != NULL && adventure_preferences->player_voice >= 5 &&
                CcSpeechPlayerChoice(sim, leaving, CC_STORY_PLAYER_LEAVE,
                    (uint32_t)adventure_preferences->player_voice, &goodbye)) ClientSaySpeech(&goodbye);
            *view = VIEW_LOCAL;
            return;
        }
        if (local->adventure_ui && local->conversation_situation_id == 0U) {
            replies = BuildContextActions(sim, local, VIEW_CHARACTER, *selected, *selected_situation);
            for (int32_t i = 0; i < replies.count; ++i)
                if (replies.items[i].enabled && ClientKeyPressed(KEY_ONE + i)) context_action = replies.items[i].kind;
            if (context_action == CONTEXT_ACTION_CLOSE_VIEW) {
                CcAudioClearSpeech();
                CcCoreConversationReset(&core_conversation); core_conversation_speaker = 0U;
                *view = VIEW_LOCAL; return;
            }
            if (context_action == CONTEXT_ACTION_GOSSIP_CHAT &&
                !core_conversation.pending && core_conversation.round_phase == 0U) {
                local->conversation_report_response = false;
    local->conversation_oven_response = false;
                (void)ApplyCommand(*journal, sim, (CcCommand){.kind = CC_COMMAND_EXCHANGE_GOSSIP,
                    .target_id = local->conversation_character_id}, message, message_capacity);
                if (local->conversation_gossip_slot < 0)
                    local->conversation_gossip_slot = CcSimNextUntoldStory(
                        sim, local->conversation_character_id, NULL);
                local->conversation_gossip_source = false;
                CcAudioClearSpeech();
                uint32_t voice = adventure_preferences != NULL && adventure_preferences->player_voice >= 5 ?
                    (uint32_t)adventure_preferences->player_voice : 5U;
                bool started = ClientStartChat(sim, local, voice);
                if (local->conversation_gossip_slot >= 0)
                    (void)ApplyCommand(*journal, sim, (CcCommand){.kind = CC_COMMAND_HEARD_STORY,
                        .target_id = local->conversation_character_id,
                        .amount = local->conversation_gossip_slot}, message, message_capacity);
                if (started) {
                    /* One story per pair: the next Chat opens the next untold account. */
                    local->conversation_gossip_slot = CcSimNextUntoldStory(
                        sim, local->conversation_character_id, NULL);
                    if (local->conversation_gossip_slot < 0) {
                        (void)snprintf(message, message_capacity, "%s",
                            "That is all I would share across this road.");
                    }
                } else {
                    local->conversation_gossip_slot = CcSimNextUntoldStory(
                        sim, local->conversation_character_id, NULL);
                    CcSpeech answer;
                    if (ClientConversationSpeech(sim, local, &answer)) ClientSaySpeech(&answer);
                }
            }
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
        if (local->adventure_ui) {
            ContextActionSet situation_replies = BuildContextActions(sim, local, VIEW_CHARACTER, *selected, *selected_situation);
            ContextActionKind chosen_reply = context_action;
            for (int32_t i = 0; i < situation_replies.count; ++i) {
                if (situation_replies.items[i].enabled && ClientKeyPressed(KEY_ONE + i)) chosen_reply = situation_replies.items[i].kind;
            }
            response = 0;
            for (int32_t i = 0; i < situation_replies.count; ++i) {
                if (!situation_replies.items[i].enabled || situation_replies.items[i].kind != chosen_reply) continue;
                switch (chosen_reply) {
                    case CONTEXT_ACTION_LISTEN_CHARACTER: response = CC_CHARACTER_RESPONSE_LISTEN; break;
                    case CONTEXT_ACTION_PLEDGE_CHARACTER: response = CC_CHARACTER_RESPONSE_PLEDGE_HELP; break;
                    case CONTEXT_ACTION_REPORT_EVIDENCE: response = CC_CHARACTER_RESPONSE_REPORT_EVIDENCE; break;
                    case CONTEXT_ACTION_KEEP_CONFIDENCE: response = CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE; break;
                    case CONTEXT_ACTION_CLOSE_VIEW: *view = VIEW_LOCAL; break;
                    default: break;
                }
            }
        }
        if (response != 0) {
            CcSpeech player_reply;
            CcStoryPlayerChoice choice = response == CC_CHARACTER_RESPONSE_LISTEN ? CC_STORY_PLAYER_ASK :
                response == CC_CHARACTER_RESPONSE_PLEDGE_HELP ? CC_STORY_PLAYER_PROMISE :
                response == CC_CHARACTER_RESPONSE_REPORT_EVIDENCE ? CC_STORY_PLAYER_REPORT : CC_STORY_PLAYER_KEEP_CONFIDENCE;
            bool voiced_reply = adventure_preferences != NULL && adventure_preferences->player_voice >= 5 &&
                CcSpeechPlayerChoice(sim, conversation, choice, (uint32_t)adventure_preferences->player_voice, &player_reply);
            CcCommand reply = {
                .kind = CC_COMMAND_CHARACTER_RESPONSE,
                .target_id = local->conversation_situation_id,
                .amount = (int32_t)response
            };
            if (ApplyCommand(*journal, sim, reply,
                             message, message_capacity)) {
                local->conversation_report_response = false;
    local->conversation_oven_response = false;
                if (voiced_reply) ClientSaySpeech(&player_reply);
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
                            "Carry %d food boxes from the granary stack to the carriage. Mara will steady each load.",
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
        ((local->adventure_ui && ClientKeyPressed(KEY_ESCAPE)) || ClientKeyPressed(KEY_BACKSPACE) ||
         context_action == CONTEXT_ACTION_CLOSE_VIEW)) {
        *view = SafeOverlayReturnView(*return_view);
        return;
    }
    if (ClientKeyPressed(KEY_TAB) ||
        command_action == COMMAND_ACTION_LEDGER) {
        CcInteractionCancel(&local->interaction, "");
        CcLocalAgentStop(&local->agent);
        ToggleCommandOverlay(VIEW_LEDGER, view, return_view);
        return;
    }
    bool road_local = local->road_choice_active ||
                      local->journey_travel_active ||
                      local->site_travel_active ||
                      local->journey_combat_active ||
                      local->journey_parley_active;
    bool quests_requested = ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_promises : KEY_Q) ||
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
            local->district_map_open = false;
            local->selected_dwelling = -1;
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
        if (context_action == CONTEXT_ACTION_MINE_SHIFT_RECORD ||
            ClientKeyPressed(KEY_R)) {
            CcCommand record={.kind=CC_COMMAND_MINE_LEARN_LEAD,
                .target_id=(CcId)sim->mine.return_revision,.amount=2};
            (void)ApplyCommand(*journal,sim,record,message,message_capacity);
            return;
        }
        if (ClientKeyPressed(KEY_ENTER) && situation != NULL &&
            situation->id != sim->player.accepted_situation_id &&
            CcSimSituationCanAccept(sim, situation)) {
            /* The board is a directory of people to find. The promise itself
               is made in person, wherever the giver is. */
            char find[192];
            SituationFindLine(sim, situation, find, sizeof(find));
            (void)snprintf(message, message_capacity, "%s", find);
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
    if (local->site_kind != CC_LOCAL_SITE_NONE &&
        (*view == VIEW_LOCAL || *view == VIEW_CARRIAGE) &&
        (context_action == CONTEXT_ACTION_RETURN_FROM_SITE ||
         (*view == VIEW_CARRIAGE && ClientKeyPressed(KEY_ENTER)))) {
        CcLocalSiteKind site = local->site_kind;
        BeginSiteTravelState(local, site, true);
        *view = VIEW_LOCAL;
        (void)snprintf(message, message_capacity,
                       "The carriage turns back toward town.");
        return;
    }
    if (*view == VIEW_CARRIAGE) {
        if (ClientKeyPressed(KEY_BACKSPACE) || ClientKeyPressed(KEY_ESCAPE) ||
            context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            if (local->carriage_inspection_road) {
                local->carriage_inspection_road = false;
                *view = VIEW_LOCAL;
                (void)snprintf(message, message_capacity,
                               "Back on the road beside the carriage.");
                return;
            }
            CcLocalAgentClearWorldTarget(&local->agent);
            *view = VIEW_LOCAL;
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
                (void)StartOnlyOutgoingRoad(*journal, sim, local, view,
                    selected, message, message_capacity);
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

    if (CcCoopClientActive() &&
        (ClientKeyPressed(KEY_F9) || context_action == CONTEXT_ACTION_REST_TEAM ||
         ClientKeyPressed(KEY_PERIOD) || ClientKeyPressed(KEY_K))) {
        (void)snprintf(message, message_capacity,
                       "Use the company road book to manage the shared world.");
        return;
    }
    if (ClientKeyPressed(KEY_F9)) {
        char error[256];
        if (!CcJournalClose(journal, sim, error, sizeof(error))) {
            (void)snprintf(message, message_capacity, "%s", error);
            return;
        }
        static CcSim loaded_sim;
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
            if (sim->mine.phase != CC_MINE_NONE &&
                RestoreClientStartupSession(
                    session_path,sim,local,view,selected)) {
                *view=VIEW_LOCAL;
            } else if (sim->journey.active &&
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
    bool rest_requested = context_action == CONTEXT_ACTION_REST_TEAM ||
        (local->adventure_ui && *view == VIEW_LOCAL &&
         !local->market_interior &&
         local->site_kind == CC_LOCAL_SITE_NONE &&
         ClientKeyPressed(KEY_R));
    if ((ClientKeyPressed(KEY_PERIOD) && !local->adventure_ui) ||
        rest_requested) {
        char error[256];
        bool advanced = !sim->journey.active &&
            CcJournalAdvanceDays(*journal, sim, 1, error, sizeof(error));
        if (!advanced && sim->journey.active) {
            (void)snprintf(error, sizeof(error), "%s",
                           "The team cannot rest on the road.");
        }
        (void)snprintf(message, message_capacity, "%s",
                       advanced ?
                           "The team rests one day with normal upkeep." :
                           error);
        if (advanced) return;
    }
    if (ClientKeyPressed(KEY_K) && !local->adventure_ui && !sim->journey.active) {
        char error[256];
        bool advanced = CcJournalAdvanceDays(*journal, sim, 7,
                                             error, sizeof(error));
        (void)snprintf(message, message_capacity, "%s",
                       advanced ?
                           "One week passed." :
                           error);
    }

    if (*view == VIEW_LOCAL) {
        if (local->open_world_market && ClientKeyPressed(KEY_BACKSPACE)) {
            local->open_world_market = false;
            message[0] = '\0';
            return;
        }
        if (local->site_travel_active) {
            if (local->carriage_stopped) return;
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
            if (local->carriage_stopped) return;
            if (UpdateRoadChoiceApproach(local, delta_time)) {
                const CcRoute *route = SelectedOutgoingRoute(
                    sim, *selected);
                if (route != NULL && EnterRoadBookFromTownGate(
                        sim, local, route->id)) {
                    *view = VIEW_ROADS;
                    message[0] = '\0';
                    (void)StartOnlyOutgoingRoad(*journal, sim, local, view,
                        selected, message, message_capacity);
                }
            }
            return;
        }
        if (local->journey_travel_active && local->open_world &&
            !local->world_carriage.hero_embarked && CcCoopClientActive()) {
            (void)UpdateDrivenConvoy(local, sim, delta_time);
            PositionOpenWorldJourneyAt(sim, local, 1, 1.0f);
            CcLocalOpenWorldCarriageTargetsInternal(sim, &local->world_carriage,
                (float)GetTime(), delta_time);
        }
        if (local->journey_travel_active &&
            (!local->open_world || local->world_carriage.hero_embarked)) {
            const CcRoadSite *road_stop = CcSimJourneyRoadSiteStop(sim);
            if (road_stop != NULL && context_action == CONTEXT_ACTION_CAMP_ROAD_SITE) {
                CcCommand choice = {.kind = CC_COMMAND_CAMP_ROAD_SITE, .target_id = road_stop->id};
                (void)ApplyCommand(*journal, sim, choice, message, message_capacity);
                return;
            }
            if (sim->journey.active &&
                sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
                int32_t pace_direction =
                    ClientKeyPressed(KEY_W) || ClientKeyPressed(KEY_UP) ? 1 :
                    ClientKeyPressed(KEY_S) || ClientKeyPressed(KEY_DOWN) ? -1 :
                    0;
                int32_t next_pace = CcClientStepConvoyPosture(
                    (int32_t)sim->journey.pace, pace_direction);
                if (context_action == CONTEXT_ACTION_SET_PACE) next_pace = pressed_action.amount;
                if ((pace_direction != 0 || context_action == CONTEXT_ACTION_SET_PACE) &&
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
            if (sim->journey.active && sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
                if (local->carriage_stopped) return;
                if (!CcCoopClientActive()) {
                    CcCommand rest = {.kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                        CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP};
                    (void)ApplyCommand(*journal, sim, rest, message, message_capacity);
                }
                return;
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
            ConvoyUpdateResult convoy_update = UpdateDrivenConvoy(
                local, sim, delta_time);
            if (convoy_update == CONVOY_UPDATE_PARKED) {
                FinishTownArrivalState(
                    sim, local, selected, message, message_capacity);
                return;
            }
            /* Step the world before the road-only work below. The creature
               gaits are deferred to the end of this block so the team can be
               told where the wagon is now, not where it was last frame; every
               early return still has to advance them or the rigs freeze where
               they last stood. */
            int32_t fixed_steps = CcLocalWorldUpdateNoGaits(
                &local->course, &local->agent, sim, delta_time,
                false, false);
            if (local->convoy.phase != CC_LOCAL_CONVOY_ROAD) {
                CcLocalCreatureGaitsAdvanceInternal(fixed_steps);
                return;
            }
            float posture_pace = CcClientConvoyPosturePace(
                (int32_t)sim->journey.pace);
            float road_motion = posture_pace > 0.01f ?
                fmaxf(0.0f, fminf(1.0f,
                    local->convoy.pace / posture_pace)) : 0.0f;
            bool warned_before = sim->journey.ambush_warned;
            bool ambush_resolved_before = sim->journey.ambush_resolved;
            char error[256];
            if (!AdvanceCarriagePresentationSteps(*journal, sim, local, fixed_steps,
                    road_motion, road_stop != NULL, error, sizeof(error))) {
                (void)snprintf(message, message_capacity, "%s", error);
                return;
            }
            if (local->open_world && sim->journey.active) {
                float alpha = CcLocalCourseAlpha(&local->course);
                CcLocalCarriageInterpolate(&local->world_carriage, alpha);
                CcLocalCarriageGaitInterpolateInternal(alpha);
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
                ResetLocalStatePreservingAthletics(local);
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
                        "Closing in. Attack follows the next opening.");
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
            Vector2 mouse = ClientPointerPosition();
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
                    local->movement_preview_cooldown = 0.02f;
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
                            "Choose clear ground. Double-tap [N] to return to the caravan.");
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
        /* A relief walk can first step down from the raised granary platform.
           Continue toward the original stop once that descent has settled. */
        if (local->relief_carriage_descent_pending &&
            local->agent.interaction_navigation &&
            !local->agent.exact_target_valid &&
            !local->agent.navigation_active) {
            const CcSituation *relief = CcSimAcceptedSituation(sim);
            local->relief_carriage_descent_pending = false;
            if (relief != NULL && relief->kind == CC_SITUATION_RELIEF_DELIVERY &&
                relief->loading_crate_carried &&
                sim->player.location_id == CcSimSituationOfferSettlementId(sim, relief) &&
                GridDistance(LocalPosition(local), LOCAL_CARRIAGE_BAY) > 1.6f)
                    (void)CcLocalAgentApproachInteraction(&local->agent,
                        LOCAL_CARRIAGE_BAY, 1.6f, false);
        }
        if (local->relief_approach == CONTEXT_ACTION_APPROACH_RELIEF_CRATES ||
            local->relief_approach == CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE) {
            bool carrying = local->relief_approach ==
                CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE;
            Vector2 stop = carrying ? LOCAL_CARRIAGE_BAY : LOCAL_RELIEF_CRATES;
            /* Match the Lift and Stow card radii, so a visible action is safe. */
            float reach = carrying ? 1.85f : 2.10f;
            if (GridDistance(LocalPosition(local), stop) <= reach) {
                CcLocalAgentStop(&local->agent);
                local->relief_carriage_descent_pending = false;
                local->relief_approach = CONTEXT_ACTION_NONE;
            } else if (!local->agent.interaction_navigation &&
                       !local->relief_carriage_descent_pending) {
                local->relief_approach = CONTEXT_ACTION_NONE;
            }
        }
        if (local->movement_reticle_valid) {
            local->movement_reticle_age += delta_time;
            float reticle_lifetime = local->movement_reticle_accepted ?
                                     0.48f : 0.75f;
            if (local->movement_reticle_age > reticle_lifetime) {
                local->movement_reticle_valid = false;
            }
        }
        if (local->journey_travel_active && local->open_world &&
            !local->world_carriage.hero_embarked) return;
        if (!local->market_interior &&
            CcLocalAgentConsumeWorldExit(&local->agent)) {
            (void)snprintf(
                message, message_capacity,
                "That road is for the carriage. Return to the yard to drive out.");
            return;
        }
        if (local->agent.combat.life_state == CC_LIFE_DEAD) return;
        if (local->journey_parley_active) {
            Vector2 collector = {CC_LOCAL_ROAD_PARLEY_X,
                                 CC_LOCAL_ROAD_PARLEY_Z};
            if (ClientKeyPressed(KEY_BACKSPACE) ||
                context_action == CONTEXT_ACTION_RETURN_TO_CHOICE) {
                ResetLocalStatePreservingAthletics(local);
                *view = VIEW_ENCOUNTER;
                (void)snprintf(message, message_capacity,
                               "Back at the carriage.");
                return;
            }
            if ((context_action == CONTEXT_ACTION_APPROACH_COLLECTOR ||
                 ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_interact : KEY_F)) &&
                GridDistance(LocalPosition(local), collector) >= 1.55f) {
                bool walking = CcLocalAgentSetExactTarget(&local->agent,
                    (Vector3){collector.x, 0.0f, collector.y}, false);
                (void)snprintf(message, message_capacity, "%s",
                    walking ? "Walking to the captain." : "Choose a clear path to the captain.");
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
            if ((ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_interact : KEY_F) ||
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
            if ((!local->adventure_ui && ClientKeyPressed(KEY_G)) ||
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

        bool interact = ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_interact : KEY_F);
        if (interact && context_action == CONTEXT_ACTION_NONE) {
            const CcSituation *relief = CcSimAcceptedSituation(sim);
            if (relief != NULL &&
                relief->kind == CC_SITUATION_RELIEF_DELIVERY &&
                sim->player.location_id ==
                    CcSimSituationOfferSettlementId(sim, relief)) {
                if (!relief->loading_crate_carried &&
                    CcSimReliefCratesToLoad(relief) > 0 &&
                    GridDistance(position, LOCAL_RELIEF_CRATES) < 2.1f) {
                    context_action = CONTEXT_ACTION_PICKUP_RELIEF_CRATE;
                } else if (relief->loading_crate_carried &&
                    (GridDistance(position, LOCAL_CARRIAGE_BAY) < 1.85f ||
                     GridDistance(position, LOCAL_CARRIAGE) < 1.85f)) {
                    context_action = CONTEXT_ACTION_STOW_RELIEF_CRATE;
                }
            }
        }
        if (context_action == CONTEXT_ACTION_PICKUP_RELIEF_CRATE ||
            context_action == CONTEXT_ACTION_STOW_RELIEF_CRATE) {
            CcLocalAgentStop(&local->agent);
            local->relief_carriage_descent_pending = false;
            local->relief_approach = CONTEXT_ACTION_NONE;
            const CcSituation *relief=CcSimAcceptedSituation(sim);
            CcCommand carry={
                .kind=context_action == CONTEXT_ACTION_PICKUP_RELIEF_CRATE ?
                    CC_COMMAND_PICKUP_RELIEF_CRATE : CC_COMMAND_STOW_RELIEF_CRATE,
                .target_id=relief != NULL ? relief->id : 0U};
            (void)ApplyCommand(*journal,sim,carry,message,message_capacity);
            return;
        }
        if (context_action == CONTEXT_ACTION_APPROACH_RELIEF_CRATES ||
            context_action == CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE) {
            CcInteractionCancel(&local->interaction, "");
            Vector2 destination=context_action == CONTEXT_ACTION_APPROACH_RELIEF_CRATES ?
                LOCAL_RELIEF_CRATES : LOCAL_CARRIAGE_BAY;
            float radius=context_action == CONTEXT_ACTION_APPROACH_RELIEF_CRATES ?
                1.8f : 1.6f;
            bool walking=CcLocalAgentApproachInteraction(&local->agent,
                destination,radius,false);
            local->relief_approach = walking ? context_action : CONTEXT_ACTION_NONE;
#if defined(PLATFORM_WEB)
            ClientBrowserReliefApproach(walking);
#endif
            local->relief_carriage_descent_pending = walking &&
                context_action == CONTEXT_ACTION_APPROACH_RELIEF_CARRIAGE &&
                !local->agent.navigation_active &&
                local->agent.command_point_valid &&
                GridDistance((Vector2){local->agent.command_point.x,
                    local->agent.command_point.z}, destination) > radius;
            (void)snprintf(message,message_capacity,"%s",walking ?
                "Walking to the next relief stop." :
                "Choose a clear path toward the relief stop.");
            return;
        }
        if (interact || context_action != CONTEXT_ACTION_NONE) {
            if (!local->market_interior &&
                context_action == CONTEXT_ACTION_CHOOSE_ROAD) {
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
                    (void)StartOnlyOutgoingRoad(*journal, sim, local, view,
                        selected, message, message_capacity);
                } else {
                    BeginRoadChoiceApproachState(local, true);
                    *view = VIEW_LOCAL;
                    (void)snprintf(message, message_capacity,
                        "You take the reins and leave the loading bay.");
                }
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
                const CcRoadSite *mine=CcMineSite(sim);
                CcId route_id = 0U;
                CcId destination_id = 0U;
                if (mine != NULL && CcRoadSiteJourneyTarget(
                        sim, mine->id, &route_id, &destination_id)) {
                    const CcRoute *road = CcSimRoute(sim, route_id);
                    CcCommand travel = {
                        .kind = CC_COMMAND_TRAVEL,
                        .target_id = destination_id
                    };
                    if (road != NULL && ApplyCommand(
                            *journal, sim, travel, message,
                            message_capacity)) {
                        *selected = (int32_t)(road - sim->routes);
                        SetOpenWorldCarriageAtRoadGate(sim, local, route_id);
                        BeginRoadTravelState(sim, local);
                        *view = VIEW_LOCAL;
                        (void)snprintf(
                            message, message_capacity,
                            "The carriage takes the Alderwatch road toward the Low Silver Pit branch.");
                    }
                }
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
                !LocalCombatActive(local) &&
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
                local->conversation_object = local->course.situation_witness_character_id;
                local->conversation_position = local->course.situation_witness.position;
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
        if (can_trade && !local->adventure_ui) {
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
            for (int32_t good = 0; good < 9; ++good) {
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
        if (ClientKeyPressed(KEY_BACKSPACE) ||
            context_action == CONTEXT_ACTION_CLOSE_VIEW) {
            local->district_map_open = false;
            local->selected_dwelling = -1;
            *view = *return_view == VIEW_CARRIAGE ?
                VIEW_CARRIAGE : VIEW_LOCAL;
            *selected = FirstOutgoingRouteIndex(sim);
            return;
        }
        if (ClientKeyPressed(KEY_D) && sim->schema_version >= 114U &&
            CcSimSettlement(sim, sim->player.location_id) != NULL) {
            local->district_map_open = !local->district_map_open;
            local->selected_district = 0;
            local->selected_dwelling = -1;
            return;
        }
        if (local->district_map_open) {
            if (ClientKeyPressed(KEY_RIGHT) || ClientKeyPressed(KEY_DOWN)) {
                local->selected_district =
                    (local->selected_district + 1) % CC_CENSUS_DISTRICTS_PER_TOWN;
                local->selected_dwelling = -1;
            }
            if (ClientKeyPressed(KEY_LEFT) || ClientKeyPressed(KEY_UP)) {
                local->selected_district =
                    (local->selected_district + CC_CENSUS_DISTRICTS_PER_TOWN - 1) %
                    CC_CENSUS_DISTRICTS_PER_TOWN;
                local->selected_dwelling = -1;
            }
            if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mouse = ClientPointerPosition();
                for (int32_t i = 0; i < sim->census.district_count;
                     i += CC_CENSUS_DISTRICTS_PER_TOWN) {
                    if (sim->census.districts[i].settlement_id !=
                        sim->player.location_id) continue;
                    DistrictMapProjection fit = DistrictMapFit(sim, i);
                    for (int32_t j = 0; j < CC_CENSUS_DISTRICTS_PER_TOWN; ++j) {
                        CcCensusPoint centre;
                        if (!CcCensusDistrictCentre(sim,
                                sim->census.districts[i + j].id,
                                &centre)) continue;
                        if (CheckCollisionPointCircle(mouse,
                                DistrictMapScreenPoint(centre, fit), 14.0f)) {
                            local->selected_district = j;
                            local->selected_dwelling = -1;
                            return;
                        }
                    }
                    float nearest = 7.0f * 7.0f;
                    for (int32_t j = 0; j < CC_CENSUS_DISTRICTS_PER_TOWN; ++j) {
                        const CcCensusDistrict *district =
                            &sim->census.districts[i + j];
                        for (int32_t home = 0; home < district->dwelling_count;
                             ++home) {
                            CcCensusPoint entrance;
                            if (!CcCensusDwellingEntrance(sim, district->id,
                                    home, &entrance)) continue;
                            Vector2 point = DistrictMapScreenPoint(entrance, fit);
                            float dx = mouse.x - point.x, dy = mouse.y - point.y;
                            float distance = dx * dx + dy * dy;
                            if (distance >= nearest) continue;
                            nearest = distance;
                            local->selected_district = j;
                            local->selected_dwelling = home;
                        }
                    }
                    break;
                }
            }
            return;
        }
        if (ClientKeyPressed(KEY_RIGHT) || ClientKeyPressed(KEY_DOWN)) {
            *selected = StepVisibleMapIndex(sim, *selected, 1);
        }
        if (ClientKeyPressed(KEY_LEFT) || ClientKeyPressed(KEY_UP)) {
            *selected = StepVisibleMapIndex(sim, *selected, -1);
        }
        if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = ClientPointerPosition();
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
        if (selected_map == NULL) {
            return;
        }
        CcId map_id = selected_map->id;
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

    if (ClientKeyPressed(KEY_BACKSPACE) ||
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

    if (context_action == CONTEXT_ACTION_TRAVEL) {
        *selected = pressed_action.amount;
    }
    const CcRoute *route = SelectedOutgoingRoute(sim, *selected);
    if (route == NULL) return;
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
    return CcLocalAtmosphereForClock(
        sim != NULL ? sim->current_day : 0,
        sim != NULL ? sim->clock.minute_subticks : 0);
}

static CcMusicContext LocalMusicContext(const CcSim *sim, const LocalState *local)
{
    CcLocalAtmospherePreset atmosphere = LocalAtmosphereForSimulation(sim);
    bool fighting = LocalCombatActive(local) ||
        ((local->journey_combat_active || local->course.alarm_active) &&
         !local->course.raiders_retreating && StandingRaiders(local) > 0);
    CcMusicScene scene = {
        .road = local->journey_travel_active || local->journey_combat_active ||
                local->journey_parley_active || local->site_travel_active ||
                local->road_choice_active,
        .market = local->market_interior || local->open_world_market,
        .bandit_attack = fighting && local->course.road_encounter,
        .town_attack = fighting && !local->course.road_encounter,
        .goblin_cave = local->site_kind == CC_LOCAL_SITE_GOBLIN_CAVE &&
                       !local->site_travel_active,
        .dragon_cave = local->site_kind == CC_LOCAL_SITE_DRAGON_CAVE &&
                       !local->site_travel_active,
        .loss = local->agent.combat.life_state == CC_LIFE_DEAD,
        .rain = atmosphere == CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST,
        .night = atmosphere == CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT,
    };
    if (local->site_kind == CC_LOCAL_SITE_DUNGEON && !local->site_travel_active) {
        scene.nearby_theme = CC_MUSIC_MINE;
        scene.nearby_weight = 1.0f;
    }
    CcMusicContext context = CcMusicContextFor(sim, scene);
    if (scene.loss) return context;
    if (atmosphere == CC_LOCAL_ATMOSPHERE_DRAGON_OMEN) {
        context.theme[CC_MUSIC_DRAGON] = 0.6f;
        context.theme[CC_MUSIC_DANGER] = 0.8f;
    }
    /* Use the same site positions as the road renderer. Each site adds a
       smooth attraction, so nearby places can overlap. */
    if (local->open_world && !sim->dungeon_expedition.active &&
        local->site_kind == CC_LOCAL_SITE_NONE) {
        Vector3 focus = local->world_carriage.hero_embarked ?
            local->world_carriage.position : local->agent.position;
        static const CcMusicTheme site_themes[CC_ROAD_SITE_KIND_COUNT] = {
            CC_MUSIC_FARM, CC_MUSIC_FARM, CC_MUSIC_WOOD, CC_MUSIC_QUARRY,
            CC_MUSIC_MINE, CC_MUSIC_MILL, CC_MUSIC_BAKERY, CC_MUSIC_FORGE,
            CC_MUSIC_INN, CC_MUSIC_TRAVEL
        };
        for (int i = 0; i < local->world_stream.manifest.road_site_count; ++i) {
            const CcWorldRoadSitePlacement *placement =
                &local->world_stream.manifest.road_sites[i];
            const CcRoadSite *site = CcSimRoadSite(sim, placement->road_site_id);
            if (site == NULL || (unsigned int)site->kind >= CC_ROAD_SITE_KIND_COUNT) continue;
            float dx = focus.x - placement->destination.x;
            float dz = focus.z - placement->destination.z;
            float weight = ClampUnit(1.0f - sqrtf(dx * dx + dz * dz) / 28.0f);
            weight = weight * weight * (3.0f - 2.0f * weight);
            CcMusicTheme theme = site_themes[site->kind];
            context.theme[theme] = fmaxf(context.theme[theme], weight);
            CcMusicAttractPlace(&context, site->name, weight);
        }
    }
    return context;
}

static Rectangle LocalViewportBounds(void)
{
    Rectangle bounds = CcLocalViewportBounds(ContextViewportWidth(), ContextViewportHeight());
    if (adventure_preferences != NULL) {
        float available = (float)ContextViewportHeight() - 200.0f;
        float scale = fminf(((float)ContextViewportWidth() - 20.0f) / 630.0f, available / 320.0f);
        bounds = (Rectangle){((float)ContextViewportWidth() - 630.0f * scale) * 0.5f,
            88.0f, 630.0f * scale, 320.0f * scale};
    }
    return bounds;
}

#if defined(CC_CLIENT_SELF_TESTS)
static int ClientRegressionFailure(const char *message)
{
    (void)fprintf(stderr, "%s\n", message);
    return 1;
}

#include "../../tests/client_interaction_flow.inc"
#include "../../tests/client_oven_court.inc"
#include "../../tests/client_mine_flow.inc"
#include "../../tests/client_world_cards.inc"
#include "../../tests/client_stable_care.inc"
#include "../../tests/client_road_block.inc"
#include "../../tests/client_bridge_scene.inc"
#include "../../tests/map_texture_lifetime.inc"

static int RunMapCaseCutsRegression(void)
{
    static CcSim sim;
    static LocalState local;
    const char *path = "map-case-cuts.ccsave";
    char error[192];
    int32_t selected = 0;
    ClientView view = VIEW_MAP;
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    sim.maps[0].owner_id = sim.player.id;
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
    ContextActionSet actions = BuildContextActions(&sim, &local, view, selected, 0);
    for (int32_t i = 0; i < actions.count; ++i) {
        if (strstr(actions.items[i].label, "Buy") != NULL ||
            strstr(actions.items[i].label, "Sell") != NULL ||
            strstr(actions.items[i].detail, "ROUTE GUIDANCE") != NULL) {
            CcJournalAbandon(&journal);
            return ClientRegressionFailure("The early map case must not sell charts.");
        }
    }
    if (actions.count < 1 || actions.items[actions.count - 1].kind != CONTEXT_ACTION_CLOSE_VIEW) {
        CcJournalAbandon(&journal);
        return ClientRegressionFailure("The map case keeps its close action.");
    }
    if (!CcJournalClose(&journal, &sim, error, sizeof(error))) return ClientRegressionFailure(error);
    (void)remove(path);
    (void)puts("Map case keeps viewing and archived charts only");
    return 0;
}
#endif

static void ReadCompanyPage(const CcSim *sim, const LocalState *local)
{
    char words[CC_SPEECH_TEXT_CAPACITY];
    if (local->book_page == 0) {
        const CcSituation *promise = CcSimAcceptedSituation(sim);
        if (promise != NULL) {
            char next[192];
            SituationNextAction(sim, promise, next, sizeof(next));
            (void)snprintf(words, sizeof(words), "Next, %s.", next);
            ClientReadSpeech(sim, words, promise->cause_event_id);
        } else {
            ClientReadSpeech(sim, "Nothing promised. Read the board or listen in the tavern.", 0);
        }
    } else if (local->book_page == 1) {
        int32_t shown = 0;
        for (int32_t i = 0; i < sim->character_count && shown < 5; ++i) {
            const CcCharacter *person = &sim->characters[i];
            if (!AdventureKnownPerson(sim, person)) continue;
            const CcSettlement *where = CcSimSettlement(sim, person->current_settlement_id);
            (void)snprintf(words, sizeof(words), "%s, the %s. Now %s.",
                person->name, CcOccupationName(person->occupation),
                where != NULL ? where->name : "on the road");
            ClientReadSpeech(sim, words, 0);
            ++shown;
        }
        if (shown == 0) ClientReadSpeech(sim, "No one you know has been named yet.", 0);
    } else if (local->book_page == 4) {
        CcScrivenDescribe(sim, words, sizeof(words));
        ClientReadSpeech(sim, words, 0);
    } else if (local->book_page == 3) {
        const CcEvent *note = CcOvenCourtNote(sim, local->book_offset);
        ClientReadSpeech(sim, note != NULL ? note->text :
            "No court note. Inspect Silverwick's public oven tally.", note != NULL ? note->id : 0U);
    } else {
        if (sim->mine.lead_event_id != 0U) {
            (void)snprintf(words,sizeof(words),
                "Low Silver Pit turnout on the Alderwatch-Silverwick road, day %d, from %s. Workers' records hold route guidance.",sim->mine.lead_day,
                sim->mine.lead_document ? "Silverwick's shift record" : "Jory Fen");
            ClientReadSpeech(sim,words,sim->mine.lead_event_id);
        }
        if (sim->mine.survey_event_id != 0U) {
            ClientReadSpeech(sim,
                "Workers' records say the western store passage goes around the barred middle passage. The stair to Lamp Hall is marked blocked.",
                sim->mine.survey_event_id);
        } else if (sim->mine.bypass_event_id != 0U) {
            ClientReadSpeech(sim,
                "The company observed that the western store passage goes around the barred middle passage.",
                sim->mine.bypass_event_id);
        }
        if (sim->mine.report_event_id != 0U) {
            if (sim->mine.report_kind == CC_MINE_RETURN_HAUL)
                (void)snprintf(words,sizeof(words),
                    "Jory received %d %s from the tracked mine haul on day %d. He says this proves the turnout can still yield.",
                    sim->mine.report_quantity,CcGoodName((CcGood)sim->mine.report_good),
                    sim->mine.report_day);
            else
                (void)snprintf(words,sizeof(words),
                    "Jory received the sourced route account on day %d. He says this gives the next company a fair path in.",
                    sim->mine.report_day);
            ClientReadSpeech(sim,words,sim->mine.report_event_id);
        }
    }
}

static void UpdateFieldVoices(const CcSim *sim, LocalState *local, ClientView view)
{
    bool alarm = local->course.alarm_active || local->journey_combat_active;
    int player_voice = adventure_preferences != NULL ? adventure_preferences->player_voice : -1;
    CcSpeech speech;
    if (view == VIEW_LOCAL && player_voice >= 5) {
        const char *words = NULL;
        CcSpeechPriority priority = CC_SPEECH_FEEDBACK;
        CcSpeechDelivery delivery = CC_SPEECH_PLAIN;
        char arrival[160];
        if (alarm && !local->voice_alarm) {
            words = "Raiders! Stay close!"; priority = CC_SPEECH_WARNING; delivery = CC_SPEECH_URGENT;
        } else if (alarm && local->agent.humanoid.action == CC_HUMANOID_ACTION_STRIKE && GetTime() >= local->voice_effort_after) {
            words = "Back!"; delivery = CC_SPEECH_FIRM;
            local->voice_effort_after = GetTime() + 12.0;
        } else if (local->voice_place != 0 && local->voice_place != sim->player.location_id) {
            const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
            if (place != NULL) {
                (void)snprintf(arrival, sizeof(arrival), "We have reached %s.", place->name);
                words = arrival;
            }
        }
        if (words != NULL && CcSpeechCompose(&speech, "player.field", sim->player.id, "You", (uint32_t)player_voice,
                words, delivery, priority, 0)) ClientSaySpeech(&speech);
    }
    local->voice_alarm = alarm;
    local->voice_place = sim->player.location_id;
    if (view != VIEW_LOCAL || alarm || local->journey_travel_active || adventure_preferences == NULL ||
        !adventure_preferences->ambient_voices || GetTime() < local->voice_ambient_after || CcAudioCurrentSpeech() != NULL) return;
    const CcLocalPlaceProfile *place = CcLocalPlaceProfileForSettlement(CcSimSettlement(sim, sim->player.location_id));
    for (int32_t i = 0; i < local->interactions.count; ++i) {
        const CcInteractionTarget *target = &local->interactions.targets[i];
        if (target->key.kind != CC_INTERACTION_PERSON || !target->visible || !target->available ||
            GridDistance(LocalPosition(local), (Vector2){target->x, target->z}) > 2.5f) continue;
        const CcGossipVersion *heard_version = NULL;
        int32_t heard_slot = CcSimNextUntoldStory(sim, target->character_id,
                                                  &heard_version);
        const CcGossip *heard_story = heard_slot >= 0 ?
            CcSimGossipStory(sim, heard_slot) : NULL;
        if ((heard_slot >= 0 && heard_story != NULL && heard_version != NULL &&
             CcSpeechStory(sim, target->character_id, heard_story,
                           heard_version, false, &speech)) ||
            CcSpeechGreeting(sim, sim->player.location_id, target->key.object,
            target->name, place->primary_hall, &speech)) {
            const CcCharacter *person = CcSimCharacter(sim, target->character_id);
            if (person != NULL) {
                CcSpeech named;
                if (CcSpeechCompose(&named, speech.line_id, person->id, person->name,
                    CcSpeechCharacterVoice(sim, person), speech.text, speech.delivery, CC_SPEECH_BACKGROUND, speech.source_event_id)) speech = named;
            }
            speech.priority = CC_SPEECH_BACKGROUND;
            ClientSaySpeech(&speech);
            local->voice_ambient_after = GetTime() + 60.0;
        }
        break;
    }
}

static void UpdatePlayAudio(CcSoundscape *soundscape, const CcSim *sim,
                             LocalState *local, ClientView view,
                             int32_t selected_situation, float dt)
{
    bool travel = local->journey_travel_active &&
        ((sim->journey.active && sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) ||
         local->arrival.phase == CC_CLIENT_ARRIVAL_ROAD_BOOK);
    CcLocalAgent *agent = &local->agent;
    uint32_t markers = CcHumanoidGaitConsumeMotionMarkers(&agent->humanoid);
    CcSoundFrame frame = {
        .x = agent->position.x, .z = agent->position.z,
        .place = sim->player.location_id, .scene = (int)agent->scene,
        .walking = view == VIEW_LOCAL && !travel && !local->site_travel_active,
        .grounded = agent->grounded || agent->swimming,
        .swimming = agent->swimming,
        .jumping = !agent->grounded && !agent->swimming && agent->velocity.y > 1.0f,
        .striking = agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE,
        .strike_time = agent->humanoid.action_time,
        .impact_time = local->course.last_outcome >= CC_COMBAT_OUTCOME_HIT ?
            local->course.combat_event_seconds : 0.0f,
        .blocked = local->course.last_outcome == CC_COMBAT_OUTCOME_BLOCKED ||
                   local->course.last_outcome == CC_COMBAT_OUTCOME_GUARD_BROKEN,
        .travel_pace = view == VIEW_LOCAL ?
            (local->open_world && local->world_carriage.hero_embarked ?
                local->world_carriage.pace :
                (travel || local->site_travel_active ||
                 local->convoy.phase == CC_LOCAL_CONVOY_DEPARTING ?
                    local->convoy.pace : 0.0f)) : 0.0f
    };
    for (int foot = 0; foot < CC_HUMANOID_LEG_COUNT; ++foot) {
        uint32_t contact = foot == 0 ? CC_MOTION_MARKER_LEFT_CONTACT :
                                      CC_MOTION_MARKER_RIGHT_CONTACT;
        frame.footfall[foot] = (markers & contact) != 0U &&
            !agent->climbing && !agent->humanoid.ragdoll.active;
        if (frame.footfall[foot]) {
            CcLimbVec3 point = agent->humanoid.feet[foot].planted_point;
            frame.foot_surface[foot] = CcLocalFootstepSurfaceAt(agent->scene, point.x, point.z);
        }
    }
    uint32_t cues = CcSoundscapeStep(soundscape, frame, dt);
    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
        if ((cues & (UINT32_C(1) << (unsigned int)cue)) != 0U) CcAudioPlay((CcSoundCue)cue);
    }
    CcAudioSetContext(((uint64_t)sim->world_seed << 32U) ^ sim->player.location_id);
    uint64_t page = 0;
    if (adventure_preferences != NULL && adventure_preferences->read_aloud) {
        if (view == VIEW_LEDGER) page = UINT64_C(1) + (uint64_t)local->book_page * 1000U + (uint64_t)local->book_offset;
        else if (view == VIEW_SITUATIONS) page = UINT64_C(1000000) + (uint64_t)selected_situation;
    }
    if (page != local->voice_read_page) {
        CcAudioClearSpeech();
        local->voice_read_page = page;
        if (page != 0 && view == VIEW_LEDGER) ReadCompanyPage(sim, local);
        else if (page != 0 && view == VIEW_SITUATIONS) {
            const CcSituation *promise = SelectedActiveSituation(sim, selected_situation);
            if (promise == NULL) ClientReadSpeech(sim, "Listen to the people in town for your next lead.", 0);
            else {
                char target[96], next[192], words[CC_SPEECH_TEXT_CAPACITY];
                SituationTargetLabel(sim, promise, target, sizeof(target));
                SituationNextAction(sim, promise, next, sizeof(next));
                bool accepted = promise->id == sim->player.accepted_situation_id;
                bool offer = CcSimSituationCanAccept(sim, promise);
                if (offer && !accepted) {
                    SituationFindLine(sim, promise, next, sizeof(next));
                }
                (void)snprintf(words, sizeof(words), "%s. %s. %s. %s.", accepted ? "Accepted promise" : offer ? "Offer" : "Lead",
                    SituationTitle(promise->kind), target, promise->affected_name);
                ClientReadSpeech(sim, words, promise->cause_event_id);
                if (offer || accepted) {
                    if (promise->kind == CC_SITUATION_RELIEF_DELIVERY || promise->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
                        (void)snprintf(words, sizeof(words), "%d %s. Progress %d of %d.",
                            promise->quantity, CcGoodName(promise->good), promise->progress, promise->quantity);
                        ClientReadSpeech(sim, words, promise->cause_event_id);
                    }
                    (void)snprintf(words, sizeof(words), "Due day %d. Reward %" PRId64 " crowns.", promise->deadline_day, promise->reward);
                    ClientReadSpeech(sim, words, promise->cause_event_id);
                }
                ClientReadSpeech(sim, next, promise->cause_event_id);
            }
        }
    }
    CcSpeech speech = {0};
    bool has_speech = false;
    if (view == VIEW_CHARACTER) {
        has_speech = ClientConversationSpeech(sim, local, &speech);
    } else if (view == VIEW_ENCOUNTER || (view == VIEW_LOCAL && local->journey_parley_active &&
        GridDistance(LocalPosition(local), (Vector2){CC_LOCAL_ROAD_PARLEY_X, CC_LOCAL_ROAD_PARLEY_Z}) < 1.55f)) {
        has_speech = CcSpeechRoad(sim, &speech);
    } else if (view == VIEW_TRADE && local->adventure_ui) {
        AdventureQuote quote = AdventureTradeQuote(sim, local);
        const CcLocalPlaceProfile *place = CcLocalPlaceProfileForSettlement(
            CcSimSettlement(sim, sim->player.location_id));
        int32_t quantity = quote.command.amount < 0 ? -quote.command.amount : quote.command.amount;
        has_speech = CcSpeechTrade(sim, place->keeper_name, quote.command.good,
            quantity, quote.total, local->trade_mode, quote.reason, &speech);
    }
    char voice_path[768];
    bool player_muted = has_speech && speech.speaker_id == sim->player.id &&
        adventure_preferences != NULL && adventure_preferences->player_voice < 5;
    ClientSpeechPath(has_speech ? &speech : NULL, voice_path, sizeof(voice_path));
    CcAudioSpeech(has_speech && !player_muted && strcmp(speech.line_id, "gossip.thinking") != 0 ? &speech : NULL, voice_path);
    UpdateFieldVoices(sim, local, view);
    CcAudioUpdate();
}

#if defined(CC_CLIENT_SELF_TESTS)
static int RunTravelAudioRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    local.voice_ambient_after = 1.0e30;
    local.convoy.pace = 0.72f;
    local.open_world = true;
    local.world_carriage.hero_embarked = true;
    local.world_carriage.pace = 0.48f;
    CcSoundscape soundscape = {0};
    UpdatePlayAudio(&soundscape, &sim, &local, VIEW_LOCAL, 0, 1.0f / 60.0f);
    if (soundscape.previous.travel_pace != 0.48f)
        return ClientRegressionFailure("Road departure sound must follow the visible carriage.");
    local.journey_travel_active = true;
    sim.journey.active = true;
    sim.journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    local.world_carriage.pace = 0;
    UpdatePlayAudio(&soundscape, &sim, &local, VIEW_LOCAL, 0, 1.0f / 60.0f);
    if (soundscape.previous.travel_pace != 0)
        return ClientRegressionFailure("A roadside stop must settle the travel sound clocks.");
    local.world_carriage.pace = 0.72f;
    UpdatePlayAudio(&soundscape, &sim, &local, VIEW_LEDGER, 0, 1.0f / 60.0f);
    if (soundscape.previous.travel_pace != 0)
        return ClientRegressionFailure("The book view must keep its quiet sound setting.");
    local.open_world = false;
    local.journey_travel_active = false;
    local.site_travel_active = true;
    UpdatePlayAudio(&soundscape, &sim, &local, VIEW_LOCAL, 0, 1.0f / 60.0f);
    if (soundscape.previous.travel_pace != 0.72f)
        return ClientRegressionFailure("Site travel must play carriage sounds.");
    local.site_travel_active = false;
    local.convoy.phase = CC_LOCAL_CONVOY_DEPARTING;
    UpdatePlayAudio(&soundscape, &sim, &local, VIEW_LOCAL, 0, 1.0f / 60.0f);
    if (soundscape.previous.travel_pace != 0.72f)
        return ClientRegressionFailure("Town departure must play carriage sounds.");
    (void)puts("PASS travel audio: departure, road stop, book, and site travel");
    return 0;
}
#endif

#if defined(CC_CLIENT_SELF_TESTS)
#include "../../tests/client_core_language.inc"
#endif

#include "cc_review_journey.inc"
#include "cc_capture_request.inc"
#if defined(CC_CLIENT_SELF_TESTS)
#include "cc_capture_reels.inc"
#endif
#include "cc_capture_frames.inc"
#include "cc_capture_scenes.inc"
#include "cc_capture_presentation.inc"
#include "cc_render_benchmark.inc"
#if defined(CC_CLIENT_SELF_TESTS)
#include "cc_capture_request_tests.inc"
#include "cc_render_benchmark_tests.inc"
#endif

#if defined(CC_CLIENT_SELF_TESTS)
#include "../../tests/road_travel_input_tests.inc"
#endif

int main(int argc, char **argv)
{
    /* --style <id> can be typed anywhere on the command line, but almost
       every capture/test/benchmark flag below this point reads argv by a
       *fixed position* (argv[1], argv[2], and so on -- some even branch on
       the exact argc), not by scanning for a name. Rather than teach each
       of those parsers about one more optional flag, strip "--style <id>"
       out of argv/argc here, before anything else looks at them, so the
       rest of main() runs exactly as if the flag had never been on the
       command line. CROWNLESS_STYLE (an environment variable, not an argv
       entry) needs no such filtering. */
    const char *style_flag_value = NULL;
    {
        int32_t filtered_argc = 0;
        for (int32_t argument = 0; argument < argc; ++argument) {
            if (strcmp(argv[argument], "--style") == 0 &&
                argument + 1 < argc) {
                style_flag_value = argv[argument + 1];
                ++argument;
                continue;
            }
            argv[filtered_argc] = argv[argument];
            ++filtered_argc;
        }
        argc = filtered_argc;
    }
#if defined(CC_CLIENT_SELF_TESTS)
    if (argc == 2 && strcmp(argv[1], "--test-continuous-road") == 0)
        return RunRoadTravelInputRegression(NULL);
    if (argc == 3 && strcmp(argv[1], "--capture-continuous-road") == 0)
        return RunRoadTravelInputRegression(argv[2]);
    if (argc == 3 && strcmp(argv[1], "--test-core-language") == 0)
        return RunCoreLanguageRegression(argv[2]);
#endif
#if defined(CC_CLIENT_SELF_TESTS)
    if (argc == 2 && strcmp(argv[1], "--test-render-benchmark") == 0) return RunRenderBenchmarkRegression();
    if (argc == 2 && strcmp(argv[1], "--test-capture-presentation") == 0) return RunCapturePresentationRegression();
    if (argc == 2 && strcmp(argv[1], "--test-capture-scenes") == 0) return RunCaptureSceneRegression();
    if (argc == 2 && strcmp(argv[1], "--test-capture-frames") == 0) return RunCaptureFrameRegression();
    if (argc == 2 && strcmp(argv[1], "--test-capture-request") == 0) return RunCaptureRequestRegression();
    if (argc == 2 && strcmp(argv[1], "--test-map-texture-lifetime") == 0) return RunMapTextureLifetimeRegression();
    if (argc == 2 && strcmp(argv[1], "--test-travel-audio") == 0) return RunTravelAudioRegression();
    if (argc == 2 && strcmp(argv[1], "--test-bridge-scene") == 0) return RunBridgeSceneRegression();
    if (argc == 2 && strcmp(argv[1], "--test-world-cards") == 0) return RunWorldCardRegression();
    if (argc == 2 && strcmp(argv[1], "--test-stable-care-card") == 0) return RunStableCareCardRegression();
    if (argc == 2 && strcmp(argv[1], "--test-road-block-choice") == 0) return RunRoadBlockChoiceRegression();
    if (argc == 2 && strcmp(argv[1], "--test-mine-input") == 0) return RunMineInputRegression();
    if (argc == 2 && strcmp(argv[1], "--test-mine-hauler-visual") == 0) return RunMineHaulerVisualRegression();
    if (argc == 2 && strcmp(argv[1], "--test-road-journey-save") == 0) return RunRoadJourneySaveRegression();
    if (argc == 2 && strcmp(argv[1], "--test-abandoned-town") == 0) return RunAbandonedTownRegression();
    if (argc == 2 && strcmp(argv[1], "--test-oven-court") == 0) return RunOvenCourtInputRegression();
    if (argc == 2 && strcmp(argv[1], "--test-adventure-input") == 0) return RunAdventureInputRegression();
    if (argc == 2 && strcmp(argv[1], "--test-adventure-trade") == 0) return RunAdventureTradeTermsRegression();
    if (argc == 2 && strcmp(argv[1], "--test-adventure-town-routes") == 0) return RunAdventureTownRoutesRegression();
    if (argc == 2 && strcmp(argv[1], "--test-frontend") == 0) {
        return RunFrontendRegression();
    }
    if (argc == 2 && strcmp(argv[1], "--test-carriage-client") == 0) return RunCarriageClientRegression();
    if (argc == 2 && strcmp(argv[1], "--test-carriage-overview") == 0) return RunCarriageOverviewRegression();
    if (argc == 2 && strcmp(argv[1], "--test-travel-hold") == 0) return RunTravelHoldRegression();
    if (argc == 2 && strcmp(argv[1], "--test-storybook-travel") == 0) {
        return RunStorybookTravelRegression();
    }
    if (argc == 2 && strcmp(argv[1], "--test-road-carriage-target") == 0) {
        return RunRoadCarriageTargetRegression();
    }
    if (argc == 2 && strcmp(argv[1], "--test-map-case-cuts") == 0) {
        return RunMapCaseCutsRegression();
    }
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
    if (argc == 2 &&
        strcmp(argv[1], "--test-road-encounter-session") == 0) {
        return RunRoadEncounterSessionRegression();
    }
    if (argc == 2 && strcmp(argv[1], "--test-party-wipe") == 0) {
        return RunSoloPartyWipeRegression();
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
    CcStylePackLoad(style_flag_value != NULL ? style_flag_value
                                             : getenv("CROWNLESS_STYLE"));
    CcRenderBenchmark benchmark = {0};
    if (!CcRenderBenchmarkParse(argc, argv, &benchmark)) return 1;
    CcCaptureRequest capture_request = {0};
    if (!CcCaptureRequestParse(argc, argv, &capture_request)) return 1;
    char save_path[640];
    CampaignSavePath(save_path, sizeof(save_path));
#if !defined(PLATFORM_WEB)
    for (int32_t i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--campaign") == 0 || strcmp(argv[i], "--save-path") == 0) {
            if (strlen(argv[i + 1]) >= sizeof(save_path)) return 1;
            (void)snprintf(save_path, sizeof(save_path), "%s", argv[i + 1]);
            break;
        }
    }
#endif
    const char *shared_world = "";
#if !defined(PLATFORM_WEB)
    for (int32_t i = 1; i + 1 < argc; ++i)
        if (strcmp(argv[i], "--shared-world") == 0) shared_world = argv[i + 1];
#endif
    char program_path[1024];
    (void)snprintf(program_path, sizeof(program_path), "%s%s", GetApplicationDirectory(), GetFileName(argv[0]));
    CcCoopClientConfigure(program_path, save_path, shared_world);
    char company_identity[768];
    (void)snprintf(company_identity, sizeof(company_identity), "%s.company-identity", save_path);
    CcCompanyConfigure(company_identity);
    if (CcCoopClientActive()) {
#if defined(PLATFORM_WEB)
        (void)snprintf(save_path, sizeof(save_path), "/tmp/crownless-coop.ccsave");
#else
        char local_campaign[640];
        (void)snprintf(local_campaign, sizeof(local_campaign), "%s", save_path);
        (void)snprintf(save_path, sizeof(save_path), "%.560s.shared-%.32s", local_campaign, shared_world);
#endif
    }
    char session_path[704];
    char lock_path[704];
    char preferences_path[704];
    if (!CampaignCompanionPath(save_path, ".session", session_path,
                               sizeof(session_path)) ||
        !CampaignCompanionPath(save_path, ".lock", lock_path,
                               sizeof(lock_path)) ||
        !CampaignCompanionPath(save_path, ".preferences", preferences_path,
                               sizeof(preferences_path))) {
        (void)fprintf(stderr, "Campaign companion path is too long.\n");
        return 1;
    }
    bool capture_active = CcCaptureActive(&capture_request);
    bool normal_play = !capture_active && !benchmark.active;
    CcClientPreferences preferences;
    CcClientPreferencesDefault(&preferences);
    if (normal_play) {
        char preferences_error[192];
        if (!CcClientPreferencesLoad(
                preferences_path, &preferences,
                preferences_error, sizeof(preferences_error))) {
            (void)fprintf(stderr, "%s\n", preferences_error);
        }
    }
    CcClientInstanceLock instance_lock = {.descriptor = -1};
    if (normal_play) {
        char lock_error[192];
        if (!CcClientInstanceLockAcquire(lock_path, &instance_lock,
                                         lock_error, sizeof(lock_error))) {
            (void)fprintf(stderr, "%s\n", lock_error);
            return 2;
        }
    }

    if (benchmark.active) SetTraceLogLevel(LOG_ERROR);
    else if (capture_active) SetTraceLogLevel(LOG_WARNING);

    unsigned int window_flags = capture_active ? FLAG_WINDOW_HIDDEN : 0U;
    /* Stable frame times without tearing; the 60 FPS target below still caps
       high-refresh displays. */
#if !defined(PLATFORM_WEB)
    if (!capture_active && !benchmark.active) window_flags |= FLAG_VSYNC_HINT;
#endif
#if defined(__APPLE__)
    if (capture_active) window_flags |= FLAG_WINDOW_HIGHDPI;
#endif
#if !defined(PLATFORM_WEB)
    window_flags |= FLAG_WINDOW_RESIZABLE;
#endif
    SetConfigFlags(window_flags);
    int32_t initial_width = normal_play ? 1200 : 1280;
    int32_t initial_height = normal_play ? 700 : 760;
#if defined(PLATFORM_WEB)
    initial_width = 1280;
    initial_height = 720;
#endif
    CcCaptureWindow window = {
        .width = initial_width, .height = initial_height,
        .minimum_width = normal_play ? 1040 : 1280,
        .minimum_height = normal_play ? 620 : 760,
        .fps = benchmark.active ? 0 : 60,
    };
    if (!CcCaptureConfigureWindow(&capture_request, argc, argv, &window)) return 1;
    /* Start hidden captures within a desktop-sized frame before resizing.
     * macOS can report invalid placement for an oversized initial window. */
    int32_t opening_width = capture_active && window.width > 1200 ? 1200 : window.width;
    int32_t opening_height = capture_active && window.height > 700 ? 700 : window.height;
    InitWindow(opening_width, opening_height,
               "Crownless Carriage — living world spine");
    if (!IsWindowReady()) {
        (void)fprintf(stderr,
                      "Crownless Carriage could not connect to the desktop window server.\n");
        CcClientInstanceLockRelease(&instance_lock);
        return 1;
    }
    char body_font_path[1024];
    if (ResolveClientAssetPath("assets/fonts/AtkinsonHyperlegible-Regular.ttf",
                               body_font_path, sizeof(body_font_path))) {
        CcOverlayLoadBodyFont(body_font_path);
    }

    if (opening_width != window.width || opening_height != window.height)
        SetWindowSize(window.width, window.height);
#if defined(PLATFORM_WEB)
    /* Keep the model as a separately cached download. */
    if (emscripten_wget("assets/language/core.ccv2", "/tmp/crownless-core.ccv2") == 0) {
        core_conversation.model = CcCoreModelLoad("/tmp/crownless-core.ccv2");
        (void)remove("/tmp/crownless-core.ccv2");
    }
#else
    char core_model_path[768];
    if (ResolveClientAssetPath("assets/language/core.ccv2", core_model_path, sizeof(core_model_path)))
        core_conversation.model = CcCoreModelLoad(core_model_path);
#endif
    SetExitKey(KEY_NULL);
    ClientInputInstall();
#if defined(PLATFORM_WEB)
    CcOverlaySetTextObserver(ClientTouchRecordText);
#endif
    SetExitKey(KEY_NULL);

    SetWindowMinSize(window.minimum_width, window.minimum_height);
    SetTargetFPS(window.fps);
    ClientMapTextures map_textures = {0};
    (void)LoadMapTexture(&map_textures.economic_goods,
                         &map_textures.economic_goods_attempted,
                         CC_ECONOMIC_GOODS_ATLAS_ASSET,
                         "economic goods atlas");
    if (map_textures.economic_goods.id != 0U) {
        SetTextureFilter(map_textures.economic_goods, TEXTURE_FILTER_POINT);
    }
    RenderTexture2D local_target = LoadRenderTexture(CcArtWidth(), CcArtHeight());
    SetTextureFilter(local_target.texture, CcArtUpscaleFilter());
    CcLocalRendererSetScreenFirstHero(screen_first_hero);
    CcLocalRendererSetReducedMotion(preferences.reduced_motion);
    CcLocalRendererInit();
#if defined(PLATFORM_WEB)
    int32_t released_asset_bytes = ClientReleaseBrowserAssets();
    if (released_asset_bytes >= 0) {
        TraceLog(LOG_INFO, "WEB: released %.1f MiB of startup files",
                 (double)released_asset_bytes / (1024.0 * 1024.0));
    }
#endif
    CcCaptureConfigureRenderer(&capture_request);

    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcJournal *journal = NULL;
    char startup_message[256] = "";
    char saved_world_load_error[256] = "";
    bool resuming_campaign = normal_play && CampaignSaveExists(save_path);
    if (normal_play && CcCoopClientActive()) {
        char error[256] = "Connect through the company road book to join this world.";
        if (CcCoopClientConnect(&sim, error, sizeof(error))) {
            journal = CcJournalStart(save_path, &sim, error, sizeof(error));
            resuming_campaign = true;
        }
        (void)snprintf(startup_message, sizeof(startup_message), "%s",
                       journal != NULL ? "The company shares this carriage and clock." : error);
        CcCoopClientReady(journal != NULL ? "" : error);
    } else if (capture_active || benchmark.active) {
        CcSimAdvanceDays(&sim, 28);
    } else {
        char error[256];
        if (CampaignSaveExists(save_path)) {
            journal = CcJournalResume(save_path, &sim, error, sizeof(error));
            bool repaired = false;
            if (journal == NULL) {
                char original_error[256];
                char repair_error[256];
                (void)snprintf(original_error, sizeof(original_error), "%s", error);
                if (CcSaveRepairHash(save_path, repair_error, sizeof(repair_error))) {
                    journal = CcJournalResume(save_path, &sim, error, sizeof(error));
                    repaired = journal != NULL;
                }
                if (journal == NULL) {
                    (void)snprintf(error, sizeof(error), "%s",
                                   original_error[0] != '\0' ? original_error : repair_error);
                }
            }
            (void)snprintf(startup_message, sizeof(startup_message), "%s",
                           journal != NULL ? (repaired ?
                               "Campaign repaired and resumed." : "Campaign resumed.") : error);
            if (journal == NULL)
                (void)snprintf(saved_world_load_error,
                               sizeof(saved_world_load_error), "%s", error);
        }
    }
#if defined(PLATFORM_WEB)
    if (normal_play && ClientBrowserCampaignAccess() != 0) {
        (void)snprintf(
            startup_message, sizeof(startup_message), "%s",
            ClientBrowserCampaignAccessMessage(
                ClientBrowserCampaignAccess()));
    }
#endif
    CcLocalTerrainSetSeed(sim.world_seed);
    if (!CcCapturePrepareWorld(&capture_request, &sim, argv)) {
        CcCaptureSceneAbort(local_target, &map_textures, &instance_lock);
        return 1;
    }
    CcLocalBindPlace(&sim);
    CcCapturePrepareJourney(&capture_request, &sim);
    CcRenderBenchmarkPrepareWorld(&benchmark, &sim);
    if (!CcCapturePrepareDungeon(&capture_request, &sim)) {
        CcCaptureSceneAbort(local_target, &map_textures, &instance_lock);
        return 1;
    }
    int32_t selected = FirstOutgoingRouteIndex(&sim);
    int32_t selected_situation = FirstActiveSituationIndex(&sim);
    ClientView view = VIEW_LOCAL;
    CcCaptureSelectStart(&capture_request, &selected, &view);
    ClientView return_view = VIEW_LOCAL;
    LocalState local = {0};
    CcCaptureState capture_state = {0};
#if defined(CC_CLIENT_SELF_TESTS)
    CcCaptureScene capture_scene = {
        .sim = &sim, .local = &local, .state = &capture_state,
        .selected = &selected, .selected_situation = &selected_situation,
        .view = &view, .return_view = &return_view,
        .preferences = &preferences, .argc = argc, .argv = argv,
    };
#endif
    ResetLocalState(&local);
    bool roadbook_world_requested = CcCaptureNeedsRoadbook(&capture_request) ||
        benchmark.roadbook;
    if ((normal_play || roadbook_world_requested) &&
        !InitializeOpenWorld(&sim, &local, false)) {
        (void)snprintf(startup_message, sizeof(startup_message),
                       "Could not generate the finite world.");
    }
    if (!CcCaptureEnterWorld(&capture_request, &capture_scene)) {
        CcCaptureSceneAbort(local_target, &map_textures, &instance_lock);
        return 1;
    }
    if (normal_play && !resuming_campaign && journal != NULL) {
        view = VIEW_LOCAL;
    }
    if (!CcCapturePrepareDeparture(&capture_request, &capture_scene)) {
        CcCaptureSceneAbort(local_target, &map_textures, &instance_lock);
        return 1;
    }
    bool restored_local_session = resuming_campaign && journal != NULL &&
        RestoreClientStartupSession(session_path, &sim, &local, &view, &selected);
    if (restored_local_session) {
        (void)snprintf(startup_message, sizeof(startup_message),
                       "Campaign resumed where you left off.");
    } else if (normal_play && sim.journey.active) {
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
    if (CcCoopClientHasSession() && !restored_local_session) {
        char close_error[192];
        (void)CcJournalClose(&journal, &sim, close_error, sizeof(close_error));
        (void)snprintf(startup_message, sizeof(startup_message),
            "Your saved place needs recovery. Reconnect after the host recovers it.");
        CcCoopClientReady(startup_message);
    }
    if (!capture_active && sim.dungeon_expedition.active) {
        local.site_kind = CC_LOCAL_SITE_DUNGEON;
        return_view = VIEW_LOCAL;
        view = VIEW_DUNGEON;
        (void)snprintf(startup_message, sizeof(startup_message),
                       "The saved expedition resumes below the mine.");
    }
    if (!capture_active && !sim.journey.active &&
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
    if (!CcCapturePrepareLocal(&capture_request, &capture_scene)) {
        CcCaptureSceneAbort(local_target, &map_textures, &instance_lock);
        return 1;
    }
    CcRenderBenchmarkPrepareScene(&benchmark, &sim, &local, &view, &return_view);
    if (!CcCapturePrepareUX(&capture_request, &capture_scene)) {
        CcCaptureSceneAbort(local_target, &map_textures, &instance_lock);
        return 1;
    }
    bool roadbook_state_ready = !roadbook_world_requested ||
        (local.open_world && local.world_stream.manifest.route_count > 0 &&
         local.world_carriage.visible);
    bool journey_state_ready = CcCaptureJourneyReady(&capture_request, &sim, &local,
        !benchmark.roadbook || (sim.journey.active &&
         sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING && local.journey_travel_active));
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
        CcCoopClientShutdown();
        CcCoreModelFree(core_conversation.model);
        core_conversation.model = NULL;
        CcOverlayUnloadBodyFont();
        CloseWindow();
        CcClientInstanceLockRelease(&instance_lock);
        return 1;
    }
    char message[256] = "";
    char save_feedback[128] = "";
    if (!capture_active && !benchmark.active && startup_message[0] != '\0') {
        (void)snprintf(message, sizeof(message), "%s", startup_message);
    }
    if (!CcCapturePrepareMessage(&capture_request, &capture_scene, message, sizeof(message))) {
        CcCaptureSceneAbort(local_target, &map_textures, &instance_lock);
        return 1;
    }
    bool performance_overlay = false;
    float message_age = 0.0f;
    float save_feedback_age = SAVE_FEEDBACK_VISIBLE_SECONDS;
#if defined(PLATFORM_WEB)
    bool browser_memory_reported = false;
#endif

    CcLocalRendererSetAtmosphere(
        CcCaptureAtmosphere(&capture_request, &sim),
        0.0f);

    FrontendState frontend = {
        .focus = normal_play ? 2 : 0,
        .online = CcCoopClientActive(),
        .screen = CcCoopClientActive() ? journal != NULL ? FRONTEND_PLAYING : FRONTEND_TITLE :
                  normal_play ? FRONTEND_TITLE : FRONTEND_PLAYING,
        .has_world = resuming_campaign,
        .world_load_failed = normal_play && resuming_campaign &&
                             journal == NULL && !CcCoopClientActive(),
    };
    if (frontend.world_load_failed) {
        frontend.focus = 0;
        (void)snprintf(frontend.world_load_error,
                       sizeof(frontend.world_load_error), "%s",
                       saved_world_load_error[0] != '\0' ? saved_world_load_error :
                       "Saved world could not load. Delete it to begin again.");
    }
    CcCaptureConfigureFrontend(&capture_request, &frontend);
    if (capture_request.capture_ux && capture_request.capture_ux_view != 24 && argc >= 6) {
        int capture_text_size = atoi(argv[5]);
        preferences.text_size = capture_text_size >= 0 && capture_text_size <= 2 ? capture_text_size : 0;
        preferences.caption_size = preferences.text_size;
    }
    if ((resuming_campaign || CcCoopClientActive()) && journal == NULL) {
        (void)snprintf(frontend.feedback, sizeof(frontend.feedback), "%s",
                       frontend.world_load_failed ? frontend.world_load_error :
                       startup_message);
    }
    if (normal_play && !CcCoopClientActive()) {
        CompanyInvitation(frontend.invitation, sizeof(frontend.invitation));
        if (frontend.invitation[0] != '\0') FrontendOpen(&frontend, FRONTEND_JOIN_WORLD);
    }
    CcSoundscape soundscape = {0};
    CcAudioSetMode(preferences.audio_mode);
    CcAudioSetVoiceVolume(preferences.voice_volume);
    Rectangle local_bounds;
    double coop_checkpoint_time = 0.0;
    int32_t coop_party_wipes = CcCoopClientPartyWipes();
    if (CcCoopClientActive() && journal != NULL) {
        coop_checkpoint_sim = &sim;
        coop_checkpoint_local = &local;
        coop_checkpoint_path = session_path;
        CcLocalCrewSetExchange(CcCoopClientExchange);
        CcLocalCrewSetSeat(CcCoopClientSeat());
        if (!restored_local_session && !sim.journey.active &&
            local.agent.scene == CC_LOCAL_SCENE_STREET) {
            int32_t seat = CcCoopClientSeat();
            RepositionHero(&local, (Vector2){local.agent.position.x + (float)seat * 0.85f,
                local.agent.position.z - (float)seat * 0.35f}, false);
        }
    }
    while (benchmark.active || (!frontend.quit && !WindowShouldClose())) {
#if defined(PLATFORM_WEB)
        ClientWaitForAnimationFrame();
#endif
        if (normal_play) {
            local.agent.appearance = CcNpcPlayerAppearance(CcCoopClientActive() ?
                CcCoopClientAppearance() : preferences.avatar);
        }
        if (CcCoopClientActive()) {
            CcId old_location = sim.player.location_id;
            CcId old_route = sim.journey.route_id;
            bool old_journey = sim.journey.active;
            CcJourneyPhase old_phase = sim.journey.phase;
            bool old_dungeon = sim.dungeon_expedition.active;
            char sync_error[256] = "";
            if (CcCoopClientPoll(&sim, sync_error, sizeof(sync_error)) &&
                (coop_party_wipes != CcCoopClientPartyWipes() ||
                 old_location != sim.player.location_id || old_journey != sim.journey.active ||
                 old_route != sim.journey.route_id || old_phase != sim.journey.phase ||
                 old_dungeon != sim.dungeon_expedition.active)) {
                CcAudioClearSpeech();
                LeaveOpenWorld(&local);
                ResetLocalStatePreservingAthletics(&local);
                CcLocalBindPlace(&sim);
                (void)InitializeOpenWorld(&sim, &local, false);
                if (sim.journey.active && sim.journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
                    BeginRoadLocalState(&sim, &local, false);
                } else if (sim.journey.active) {
                    BeginRoadTravelState(&sim, &local);
                }
                selected = FirstOutgoingRouteIndex(&sim);
                selected_situation = FirstActiveSituationIndex(&sim);
                view = sim.dungeon_expedition.active ? VIEW_DUNGEON : VIEW_LOCAL;
                if (coop_party_wipes != CcCoopClientPartyWipes()) {
                    coop_party_wipes = CcCoopClientPartyWipes();
                    (void)snprintf(message, sizeof(message),
                        "Twenty years later. A new company takes up the carriage.");
                }
            }
            if (sim.journey.active && local.journey_travel_active) {
                local.carriage_stopped = CcCoopClientTravelStopped();
                if (local.carriage_stopped) local.convoy.pace = local.world_carriage.pace = 0;
            }
            if (CcCoopClientDead()) CcLocalAgentDie(&local.agent);
        }
        CcCapturePresentation presentation = CcCapturePresentationFor(
            &capture_request, &capture_state, normal_play);
        local.adventure_ui = presentation.adventure_ui;
        if (normal_play && AdventureScene(&local)) local.course.automatic_alarm = false;
        adventure_preferences = &preferences;
        CcLocalRendererSetInteractionUI(local.adventure_ui);
        ClientTouchBegin();
        const CcSettlement *touch_place = CcSimSettlement(&sim, sim.player.location_id);
        ClientTouchHeading(touch_place != NULL ? touch_place->name : "The road",
            "Tap clear ground to walk. Tap a person or doorway to approach.");
        local_bounds = LocalViewportBounds();
        float frame_delta_time = GetFrameTime();
        bool music_play_input = false;
        bool menu_frame = frontend.screen != FRONTEND_PLAYING;
        FrontendAction menu_action = FRONTEND_ACTION_NONE;
        bool scene_owns_escape = sim.mine.phase == CC_MINE_NONE &&
            (local.interaction.approaching || view == VIEW_CARRIAGE || view == VIEW_CHARACTER ||
             view == VIEW_TRADE || view == VIEW_LEDGER || view == VIEW_SITUATIONS);
        if (normal_play && (frontend.screen != FRONTEND_PLAYING || !scene_owns_escape)) {
            menu_action = FrontendInput(&frontend);
            menu_frame = menu_frame || frontend.screen != FRONTEND_PLAYING;
            if (menu_action != FRONTEND_ACTION_NONE) menu_frame = true;
            HandleFrontendAction(&frontend, menu_action, &journal, &sim,
                                 &local, &view, &return_view, &selected,
                                 &selected_situation, save_path, session_path, preferences_path);
        }
        float world_delta_time = menu_frame ? 0.0f : frame_delta_time;
        UpdateTravelHold(&sim, &local, view, selected, selected_situation,
            ClientPointerPosition(), !menu_frame && IsWindowFocused() &&
                ClientPointerHeld(), frame_delta_time);
        CcCoopClientTravelScale(local.travel_fast_forward ?
            (int32_t)CcClientTravelTimeScale(local.travel_time_blend) : 1);
        save_feedback_age = fminf(
            SAVE_FEEDBACK_VISIBLE_SECONDS,
            save_feedback_age + frame_delta_time);
        CcLocalRendererSetOpeningStep(local.opening_step);
        if (view == VIEW_ROADS) {
            local.fork_turn_progress = fminf(
                1.0f, local.fork_turn_progress + world_delta_time * 0.78f);
        }
        char previous_message[sizeof(message)];
        (void)snprintf(previous_message, sizeof(previous_message), "%s",
                       message);
        ClientView audio_previous_view = view;
        bool audio_clicked = normal_play && !local.adventure_ui && !menu_frame && ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(ClientPointerPosition(), AudioControlBounds());
        if (normal_play) {
            bool input = ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                         ClientMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                         GetTouchPointCount() > 0;
            for (int key = 32; key < 349 && !input; ++key) input = ClientKeyPressed(key);
            music_play_input = input;
            if (input) CcAudioInit();
            CcAudioSetFocused(IsWindowFocused());
            CcAudioSetVoiceVolume(preferences.voice_volume);
            CcAudioSetReadingTime(preferences.reading_time);
            CcAudioSetContext(((uint64_t)sim.world_seed << 32U) ^ sim.player.location_id);
            if (menu_frame) { CcAudioClearSpeech(); local.voice_read_page = 0; }
        }
        bool speech_clicked = normal_play && !menu_frame && CcAudioCurrentSpeech() != NULL &&
            ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            (CheckCollisionPointRec(ClientPointerPosition(), SpeechControlBounds(view, false)) ||
             CheckCollisionPointRec(ClientPointerPosition(), SpeechControlBounds(view, true)));
        if (normal_play && !menu_frame && CcAudioCurrentSpeech() != NULL) {
            if (ClientKeyPressed(KEY_F7) || (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(ClientPointerPosition(), SpeechControlBounds(view, false)))) CcAudioReplaySpeech();
            if (ClientKeyPressed(KEY_F8) || (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(ClientPointerPosition(), SpeechControlBounds(view, true)))) CcAudioSkipSpeech();
        }
        bool change_audio = normal_play && (ClientKeyPressed(KEY_F6) || audio_clicked ||
            menu_action == FRONTEND_ACTION_SOUND);
        if (change_audio) {
            preferences.audio_mode = (preferences.audio_mode + 1) % 3;
            CcAudioSetMode(preferences.audio_mode);
        }
        if (adventure_preferences_dirty || change_audio || (normal_play &&
            (ClientKeyPressed(KEY_F4) || menu_action == FRONTEND_ACTION_MOTION))) {
            if (!change_audio && !adventure_preferences_dirty) preferences.reduced_motion = !preferences.reduced_motion;
            adventure_preferences_dirty = false;
            CcAudioSetMode(preferences.audio_mode);
            CcLocalRendererSetReducedMotion(preferences.reduced_motion);
            char preferences_error[192];
            bool preferences_saved = CcClientPreferencesSave(
                preferences_path, &preferences,
                preferences_error, sizeof(preferences_error));
#if defined(PLATFORM_WEB)
            if (preferences_saved &&
                ClientFlushBrowserPreferences(preferences_path) == 0) {
                preferences_saved = false;
                (void)snprintf(
                    preferences_error, sizeof(preferences_error),
                    "The browser could not store client preferences.");
            }
#endif
            (void)snprintf(
                message, sizeof(message), "%s",
                preferences_saved ? "Settings saved." : preferences_error);
        }
        bool preference_menu_change =
            (menu_action >= FRONTEND_ACTION_SETTING_BODY &&
             menu_action <= FRONTEND_ACTION_SETTING_DEFAULTS) ||
            menu_action == FRONTEND_ACTION_TEXT || menu_action == FRONTEND_ACTION_HINTS ||
            menu_action == FRONTEND_ACTION_MOTION;
        if (menu_frame && (change_audio || preference_menu_change)) {
            (void)snprintf(frontend.feedback, sizeof(frontend.feedback), "%s", message);
        }
        CcLocalRendererSetAtmosphere(
            CcCaptureAtmosphere(&capture_request, &sim),
            2.4f);
        int32_t local_target_width=CcArtWidth();
        int32_t local_target_height=CcArtHeight();
        if(sim.mine.phase!=CC_MINE_NONE)
            MineRenderTargetSize(&sim,&local,&local_target_width,&local_target_height);
        if(local_target.texture.width!=local_target_width ||
           local_target.texture.height!=local_target_height) {
            UnloadRenderTexture(local_target);
            local_target=LoadRenderTexture(local_target_width,local_target_height);
            SetTextureFilter(local_target.texture,TEXTURE_FILTER_POINT);
        }
        CcLocalRendererBeginFrame(frame_delta_time);
        CcLocalBindPlace(&sim);
        BindOpenWorldForLocalState(&local);
        if (!capture_active && !benchmark.active && ClientKeyPressed(KEY_F3)) {
            performance_overlay = !performance_overlay;
            (void)snprintf(message, sizeof(message), "%s",
                           performance_overlay ?
                           "Performance overlay enabled." :
                           "Performance overlay hidden.");
        }
        if (CcCaptureBeforeFrame(&capture_request, &capture_state, &sim, &local,
                &selected, &selected_situation, &view, &return_view,
                message, sizeof(message))) {
            /* The capture hook supplied this frame's input. */
        } else if (benchmark.active || !presentation.accept_input) {
            ClientInputClearPressed();
        } else {
            if (local.agent.combat.life_state == CC_LIFE_DEAD) {
                (void)CcLocalWorldUpdate(&local.course, &local.agent, &sim,
                    world_delta_time, local.market_interior,
                    !local.market_interior && local.site_kind == CC_LOCAL_SITE_NONE);
            } else if (!menu_frame && !audio_clicked && !speech_clicked) HandleInput(&journal, &sim, &selected, &selected_situation,
                        &view, &return_view, &local,
                        local_target, local_bounds,
                        frame_delta_time,
                        save_path, session_path, message, sizeof(message),
                        save_feedback, sizeof(save_feedback),
                        &save_feedback_age);
            if (local.settings_requested) {
                local.settings_requested = false;
                frontend.settings_return = FRONTEND_PLAYING;
                FrontendOpen(&frontend, FRONTEND_SETTINGS);
                menu_frame = true;
            }
            ClientInputClearPressed();
        }
        if (normal_play && journal != NULL) {
            if (CcCoopClientActive()) {
                CcCoopClientLife(local.agent.combat.life_state == CC_LIFE_DEAD);
            } else if (ResolveSoloPartyWipe(journal, &sim, &local, &view,
                                           message, sizeof(message))) {
                selected = FirstOutgoingRouteIndex(&sim);
                selected_situation = FirstActiveSituationIndex(&sim);
                return_view = VIEW_LOCAL;
                (void)SaveClientWorld(journal, &sim, &local, save_path,
                    session_path, save_feedback, sizeof(save_feedback));
            }
        }
        if (normal_play && view == VIEW_PAUSE && frontend.screen == FRONTEND_PLAYING) {
            FrontendReturnFromBook(&frontend, &local, &view);
            menu_frame = true;
        }
        if (CcCoopClientActive() && GetTime() - coop_checkpoint_time >= 0.25) {
            CcCoopCheckpointNow();
            coop_checkpoint_time = GetTime();
        }
        if (!menu_frame && presentation.update_convoy) {
            UpdateOpenWorldCamera(&sim, &local, frame_delta_time);
        }
        if (view == VIEW_CHARACTER && !menu_frame)
            CcCoreConversationAdvance(&core_conversation, 2U, frame_delta_time);
        else {
            CcCoreConversationReset(&core_conversation);
            core_conversation_speaker = 0U;
        }
        if (normal_play) {
            if (view != audio_previous_view) CcAudioPlay(CC_SOUND_PAGE);
            if (!menu_frame) UpdatePlayAudio(&soundscape, &sim, &local, view, selected_situation, world_delta_time);
            else CcAudioUpdate();
        }
        bool persistence_blocked = CcClientCampaignAccessFor(
            normal_play && (frontend.has_world || frontend.screen == FRONTEND_PLAYING),
            journal != NULL) == CC_CLIENT_CAMPAIGN_BLOCKED;
        CcLocalRendererSetOpeningStep(local.opening_step);
        CcLocalBindPlace(&sim);
        BindOpenWorldForLocalState(&local);
        bool movement_preview_visible = view == VIEW_LOCAL && !local.interaction.approaching &&
            !local.site_travel_active && !local.road_choice_active &&
            (!local.journey_travel_active ||
                (local.open_world && !local.world_carriage.hero_embarked)) &&
            !local.journey_parley_active;
        CcLocalRendererSetMovementPreview(
            movement_preview_visible ? &local.movement_preview : NULL);
        if (strcmp(previous_message, message) != 0) {
            message_age = 0.0f;
        } else {
            message_age += presentation.message_step > 0.0f ? presentation.message_step : frame_delta_time;
        }
        float clock = benchmark.active ?
            ((float)benchmark.warmup_count + (float)benchmark.count) / 60.0f :
            CcCaptureClock(&capture_request, &capture_state, (float)GetTime());
        if (local.world_carriage.storybook_travel) {
            clock = (float)fmod((double)sim.clock.tick /
                                (double)CC_WORLD_TICKS_PER_SECOND, 3600.0);
        }

        if (local.open_world && !local.market_interior) {
            if (!sim.journey.active) {
                CcLocalOpenWorldCarriageTargetsInternal(&sim, &local.world_carriage, clock,
                    !menu_frame && presentation.update_convoy ? frame_delta_time : 0.0f);
                if (!menu_frame && presentation.update_convoy &&
                    (RoadBookDepartureInProgress(&local) || RoadBookArrivalInProgress(&local))) {
                    /* These transitions do not run the travel input's world
                       step. Use its accumulator here, exactly once. */
                    int32_t steps = CcLocalWorldUpdateNoGaits(&local.course, NULL,
                        &sim, frame_delta_time, false, false);
                    CcLocalCreatureGaitsAdvanceInternal(steps);
                }
                CcLocalCarriageGaitInterpolateInternal(1.0f);
            }
            CcLocalCarriageRoll(&local.world_carriage,
                CcLocalOpenWorldCarriageScaleInternal(&sim, &local.world_carriage));
        }

        /* The remaining local-only road scenes (fork, encounter/combat/
           parley, remote site, town-street convoy) still draw a hitched
           team, but the open-world and travelling cases above already own
           their targets. Publish each one here, once, from the same state
           its own draw call below will use, so those draw calls can stay
           read-only. Each publisher no-ops when its scene is not the one
           actually active this frame. */
        if (!local.open_world && (view == VIEW_ROADS ||
                ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
                 return_view == VIEW_ROADS))) {
            CcLocalRoadForkHorseTargetsInternal(
                &sim, selected, local.fork_turn_progress, clock);
        }
        {
            bool road_scene_travelling = local.road_choice_active ||
                local.journey_travel_active;
            bool road_scene_active = view == VIEW_ENCOUNTER ||
                (road_scene_travelling &&
                 local.convoy.phase == CC_LOCAL_CONVOY_ROAD) ||
                local.journey_combat_active || local.journey_parley_active;
            if (road_scene_active && !road_scene_travelling) {
                CcLocalRoadEncounterHorseTargetsInternal(&sim, clock);
            }
        }
        CcLocalRoadSiteHorseTargetsInternal(&sim, local.site_kind,
            local.site_travel_active, local.site_returning,
            local.site_travel_progress, clock);
        CcLocalRoadConvoyHorseTargetsInternal(&sim, &local.convoy, clock);

        bool map_visible = view == VIEW_MAP ||
            ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
             return_view == VIEW_MAP);
        if (map_visible && !local.district_map_open) {
            PrepareMapTextures(&sim, selected, &map_textures);
        }
#if defined(PLATFORM_WEB)
        else {
            ReleaseMapPageTextures(&map_textures);
        }
#endif

        CcLocalRendererSetConversationFocus(
            LocalConversationFocus(&local, view),
            local.conversation_object >= UINT64_C(0x100000000) && local.conversation_object < UINT64_C(0x200000000) ? (uint32_t)local.conversation_object : 0U,
            local.agent.facing_yaw + PI);
        CcLocalRendererSetPonyConversation(view == VIEW_LOCAL && sim.pony_company.encounter >= 0);
        if (normal_play) {
            CcMusicContext music_context = LocalMusicContext(&sim, &local);
            CcMusicPlayerUpdate(&music_context, frame_delta_time,
                                IsWindowFocused(), music_play_input,
                                CcAudioMusicGain(), sim.world_seed);
        }
        CcLocalCrewBeginFrame(frame_delta_time);
        BeginDrawing();
        ClearBackground(BACKGROUND);
        CcOverlayBegin(1.0f + (float)preferences.text_size * 0.18f);
        CcOverlaySetCaptionScale(1.0f + (float)preferences.caption_size * 0.2f);
        bool road_choice_underlay = view == VIEW_ROADS ||
                            ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
                             return_view == VIEW_ROADS);
        if (road_choice_underlay) {
            if (local.open_world) {
                CcLocalWorldCarriageState carriage = local.world_carriage;
                carriage.storybook_travel = true;
                carriage.camera_weight = 0.0f;
                carriage.camera_target = 0.0f;
                carriage.camera_heading_yaw = carriage.heading_yaw;
                CcLocalDrawOpenWorld3D(
                    &sim, &local.world_stream, &local.agent, &local.course,
                    &carriage,
                    clock, local_target, local_bounds);
            } else {
                CcLocalDrawFork3D(
                    &sim, &local.agent, selected, local.fork_turn_progress, clock,
                    local_target, local_bounds);
            }
            DrawLocalHeader(&sim, &local, view, false);
        } else if (map_visible) {
            DrawMapHeader(&sim);
            if (local.district_map_open) {
                DrawDistrictMap(&sim, local.selected_district,
                                local.selected_dwelling);
            } else {
                DrawMap(&sim, selected, clock, map_textures.illustrated,
                        map_textures.collectible_atlas);
                DrawSettlementPanel(&sim, selected);
            }
        } else {
            if (sim.mine.phase != CC_MINE_NONE) {
                /* DrawMineScene presents this frame below. */
            } else if (CcCaptureDrawScene(&capture_request, &sim, clock,
                                   local_target, local_bounds)) {
                /* The capture hook drew its review scene. */
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
            if (sim.mine.phase == CC_MINE_NONE && presentation.local_panels &&
                view != VIEW_ENCOUNTER) {
                if (view == VIEW_LOCAL && sim.pony_company.encounter < 0) {
                    DrawLocalMovementReticle(&local, local_bounds);
                }
                DrawLocalHeader(&sim, &local, view, view == VIEW_CHARACTER);
                DrawLocalPanel(&sim, &local);
            }
        }
        if (view == VIEW_LOCAL || view == VIEW_ROADS) {
            if (AdventureScene(&local)) BuildAdventureTargets(&sim, &local, local_target, local_bounds);
            else BuildWorldActionTargets(&sim, &local, view, selected, selected_situation, local_target, local_bounds);
            local.interaction_view = view;
        }
        if (presentation.combat_status && view == VIEW_LOCAL &&
            LocalCombatActive(&local)) {
            DrawCombatStatusLine(&local, message, message_age);
        }
        if (!persistence_blocked && presentation.feedback &&
            (view == VIEW_LOCAL || view == VIEW_ROADS || view == VIEW_SITUATIONS) &&
            !LocalCombatActive(&local) &&
            (view == VIEW_ROADS || message_age < (local.adventure_ui ? 7.0f : 2.2f)) &&
            message[0] != '\0' &&
            !local.journey_travel_active) {
            if (local.adventure_ui) DrawAdventureFeedback(message);
            else {
            const char *toast = TextFormat("%.48s", message);
            int width = CcOverlayMeasureText(toast, 10) + 26;
            if (width > 660) width = 660;
            float opacity = message_age > 1.6f ?
                1.0f - (message_age - 1.6f) / 0.6f : 1.0f;
            float x = ((float)GetScreenWidth() - (float)width) * 0.5f;
            /* The context action tray starts 94px above the bottom edge
               (see ContextActionBounds); a 107px toast offset let its 28px
               panel dip 15px into the tray whenever a card was showing at
               the same time as a message ("Creature settlement." over the
               local-site action cards, for instance). 128px clears it. */
            float toast_y = (float)GetScreenHeight() - 128.0f;
            DrawRectangleRounded((Rectangle){x, toast_y,
                                              (float)width, 28.0f},
                                 0.22f, 5,
                                 Fade(BACKGROUND, opacity));
            CcOverlayDrawText(toast, (int)x + 13,
                              (int)toast_y + 8, 10,
                              Fade(INK, opacity));
            }
        }
        if (view == VIEW_LEDGER) {
            CcOverlayFlush();
            if (local.adventure_ui) DrawAdventureBook(&sim, &local);
            else DrawLedger(&sim);
        }
        if (view == VIEW_OVEN_COURT) { CcOverlayFlush(); DrawOvenCourt(&sim, &local); }
        if (view == VIEW_TRADE) { CcOverlayFlush(); DrawAdventureTrade(&sim, &local, map_textures.economic_goods); }
        if (view == VIEW_PAUSE) { CcOverlayFlush(); DrawAdventurePause(&local); }
        if (view == VIEW_CARRIAGE) {
            CcOverlayFlush();
            DrawCarriageScreen(&sim, &local,
                               map_textures.economic_goods);
        }
        if (view == VIEW_SITUATIONS) {
            CcOverlayFlush();
            if (local.adventure_ui) DrawAdventurePromises(&sim, selected_situation);
            else DrawSituationBoard(&sim, selected_situation);
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
#if !defined(PLATFORM_WEB)
        if (!persistence_blocked && presentation.save_feedback) {
            DrawSaveFeedbackToast(save_feedback, save_feedback_age);
        }
#endif
        if (!persistence_blocked && presentation.context_actions) {
            if ((view == VIEW_LOCAL || view == VIEW_ROADS) && !LocalCombatActive(&local) &&
                sim.pony_company.encounter < 0) DrawAdventureFocus(&sim, &local, view, selected, selected_situation);
            DrawContextActionTray(&sim, &local, view, selected, selected_situation);
        }
        if (!persistence_blocked && presentation.commands &&
            view != VIEW_DRAGON_CAVE && view != VIEW_TRADE && view != VIEW_PAUSE && view != VIEW_LEDGER &&
            view != VIEW_OVEN_COURT &&
            view != VIEW_DUNGEON &&
            view != VIEW_CARRIAGE && view != VIEW_CHARACTER) {
            if (!local.adventure_ui) DrawCommandBar(view, &local);
        }
        if (view == VIEW_LOCAL && sim.pony_company.encounter >= 0) {
            CcOverlayFlush();
            DrawPonyEncounter(&sim);
        }
        if (performance_overlay) {
            CcOverlayFlush();
            DrawPerformanceOverlay();
        }
        CcCaptureDrawOverlay(&capture_request, &capture_state);
        if (persistence_blocked) {
            CcOverlayFlush();
            DrawCampaignUnavailable(message);
        }
        if (presentation.navigation && view == VIEW_LOCAL) {
            const char *navigation[] = {"Book  B", "Map  M", "Save F5", "Menu Esc"};
            for (int i = 0; i < 4; ++i) AdventureButton(AdventureNavBounds(i), navigation[i], true, false);
            if (local.journey_travel_active && sim.journey.active) {
                for (int32_t pace = CC_JOURNEY_PACE_CAREFUL; pace <= CC_JOURNEY_PACE_PUSH; ++pace)
                    AdventureButton(AdventurePaceBounds(pace), CcJourneyPaceName((CcJourneyPace)pace),
                        sim.pony_company.encounter < 0, pace == (int32_t)sim.journey.pace);
            }
        }
        if (normal_play && !menu_frame && CcAudioCurrentSpeech() != NULL) {
            const CcSpeech *spoken = CcAudioCurrentSpeech();
            AdventureButton(SpeechControlBounds(view, false), "Replay F7", true, false);
            AdventureButton(SpeechControlBounds(view, true), "Skip F8", true, false);
            if (view != VIEW_CHARACTER) {
                bool modal_caption = view != VIEW_LOCAL;
                if (modal_caption) {
                    DrawRectangle(0, GetScreenHeight() - 50, GetScreenWidth(), 50, PANEL_DEEP);
                    /* The footer lane sits outside modal panels; size the full
                       caption to fit there rather than over the panel choices. */
                    char caption[512];
                    int caption_size = HeaderFitCaptionText(spoken->text,
                        GetScreenWidth() - 44, 32, 18, caption,
                        sizeof(caption));
                    CcOverlayDrawCaption(caption, 22,
                        GetScreenHeight() - 42, caption_size, INK);
                } else {
                    (void)AdventureWrap(spoken->text, 22, 134,
                        GetScreenWidth() - 44, AdventureTextSize(14), INK);
                }
            }
        }
        CcOverlayEnd();
        if (normal_play && !local.adventure_ui && frontend.screen == FRONTEND_PLAYING) {
            Rectangle audio_bounds = AudioControlBounds();
            DrawRectangleRounded(audio_bounds, 0.25f, 4, PANEL_DEEP);
            DrawRectangleRoundedLinesEx(audio_bounds, 0.25f, 4, 1.0f, Fade(TEAL, 0.62f));
            const char *audio_label = preferences.audio_mode == 0 ? "Sound full F6" :
                preferences.audio_mode == 1 ? "Effects F6" : "Muted F6";
            DrawText(audio_label, (int)audio_bounds.x + 10, (int)audio_bounds.y + 10, 10, INK);
        }
        if (CcCoopClientActive()) {
            int32_t crew_count;
            const CcCrewMember *crew = CcLocalCrewDrawn(&crew_count);
            CcCoopClientDrawn(crew, crew_count);
        }
        if(sim.mine.phase != CC_MINE_NONE && !persistence_blocked &&
            (view == VIEW_LOCAL || view == VIEW_ROADS)) {
            CcOverlayFlush();
            DrawMineScene(&sim,&local,local_target,message);
            CcOverlayFlush();
        }
        if (frontend.screen != FRONTEND_PLAYING) {
            DrawFrontend(&frontend, &preferences, &sim, local_target);
        }
#if defined(PLATFORM_WEB)
        ClientBrowserFrontend(frontend.screen == FRONTEND_TITLE ? "title" :
            frontend.screen == FRONTEND_PAUSED ? "paused" :
            frontend.screen == FRONTEND_DELETE ? "delete" :
            frontend.screen == FRONTEND_AVATAR ? "avatar" :
            frontend.screen == FRONTEND_SOUND ? "sound" :
            frontend.screen == FRONTEND_CAMPAIGN ? "campaign" :
            frontend.screen == FRONTEND_WORLDS ? "worlds" :
            frontend.screen == FRONTEND_CREATE_WORLD ? "create" :
            frontend.screen == FRONTEND_JOIN_WORLD ? "join" :
            frontend.screen == FRONTEND_COMPANY ? "company" :
            frontend.screen == FRONTEND_INVITATION ? "invitation" :
            frontend.screen == FRONTEND_REMOVE_MEMBER ? "remove" :
            frontend.screen == FRONTEND_SETTINGS ? "settings" : "playing", frontend.focus, (int)frontend.avatar);
#endif
        ClientTouchScene(frontend.screen != FRONTEND_PLAYING ? "menu" :
            sim.mine.phase != CC_MINE_NONE &&
                (view == VIEW_LOCAL || view == VIEW_ROADS) ? "mine" :
            view == VIEW_OVEN_COURT ? "oven-court" :
            view == VIEW_LEDGER || view == VIEW_SITUATIONS ? "book" :
            view == VIEW_DUNGEON || view == VIEW_DRAGON_CAVE ? "dungeon" :
            view == VIEW_CARRIAGE ? "carriage" :
            view == VIEW_CHARACTER ? "conversation" :
            view == VIEW_TRADE ? "trade" :
            sim.journey.active || local.journey_travel_active ? "road" : "town");
#if defined(PLATFORM_WEB)
        ClientBrowserLocalNavigation(local.agent.position.x,
            local.agent.position.y, local.agent.position.z,
            CcLocalTerrainHeightAt(local.agent.position.x,
                local.agent.position.z), local.agent.command_point.x,
            local.agent.command_point.z, local.agent.target_valid,
            local.agent.navigation_active, local.agent.navigation_point_index,
            local.agent.navigation_point_count,
            local.agent.movement_stall_seconds,
            local.interaction.approaching,
            local.agent.interaction_navigation,
            local.relief_carriage_descent_pending,
            (int)local.agent.combat.life_state, local.agent.combat.health,
            (int)local.agent.traversal, local.agent.grounded);
#endif
        ClientTouchEnd();
        EndDrawing();
#if defined(PLATFORM_WEB)
        if (!browser_memory_reported) {
            TraceLog(LOG_INFO, "WEB: using %.1f MiB of linear memory",
                     (double)ClientBrowserHeapBytes() / (1024.0 * 1024.0));
            browser_memory_reported = true;
        }
#endif

        if (benchmark.active) {
            if (CcRenderBenchmarkAfterFrame(&benchmark, GetTime)) break;
        } else if (CcCaptureAfterFrame(&capture_request, &capture_state,
                                       ClientTakeScreenshot)) {
            break;
        }
    }

    bool campaign_unavailable = CcClientCampaignAccessFor(
        normal_play && frontend.has_world, journal != NULL) == CC_CLIENT_CAMPAIGN_BLOCKED;
    char journal_error[256];
    if (normal_play && journal != NULL && !campaign_unavailable &&
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
    double render_benchmark_elapsed = benchmark.active ?
        GetTime() - benchmark.started : 0.0;
    CcLocalRendererStats final_renderer_stats =
        CcLocalRendererGetStats();
    CcMusicPlayerShutdown();
    CcAudioShutdown();
    CcLocalRendererShutdown();
    UnloadRenderTexture(local_target);
    ReleaseMapTextures(&map_textures);
    CcCoopClientShutdown();
    CcCoreModelFree(core_conversation.model);
    core_conversation.model = NULL;
    CcOverlayUnloadBodyFont();
    CloseWindow();
    CcClientInstanceLockRelease(&instance_lock);
    if (benchmark.active) {
        int benchmark_result = CcRenderBenchmarkReport(&benchmark, render_benchmark_elapsed,
            final_renderer_stats, stdout, stderr);
        if (benchmark_result != 0) return benchmark_result;
    } else {
        CcCaptureReport(&capture_request, &capture_state);
    }
    return journal_close_failed || campaign_unavailable ? 1 : 0;
}
