#include "client/cc_local3d.h"
#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "raylib.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Color BACKGROUND = {7, 14, 21, 255};
static const Color PANEL = {10, 21, 29, 238};
static const Color INK = {231, 232, 211, 255};
static const Color MUTED = {132, 154, 148, 255};
static const Color TEAL = {104, 234, 207, 255};
static const Color CC_GOLD = {249, 197, 75, 255};
static const Color DANGER = {218, 75, 86, 255};
static const Color CC_VIOLET = {195, 105, 221, 255};

typedef enum ClientView {
    VIEW_LOCAL,
    VIEW_MAP,
    VIEW_LEDGER,
    VIEW_SITUATIONS
} ClientView;

typedef struct LocalState {
    CcLocalAgent agent;
    CcLocalCourse course;
    bool market_interior;
} LocalState;

static const Vector2 LOCAL_MARKET = {7.15f, 3.35f};
static const Vector2 LOCAL_CARRIAGE = {1.35f, 6.55f};
static const Vector2 LOCAL_NOTICE = {3.05f, 2.75f};
static const Vector2 LOCAL_DUNGEON = {8.35f, 7.20f};
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

static const CcKingdom *KingdomById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == id) return &sim->kingdoms[i];
    }
    return NULL;
}

static const CcFaction *FactionById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        if (sim->factions[i].id == id) return &sim->factions[i];
    }
    return NULL;
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

static Color KingdomColor(const CcSim *sim, CcId id)
{
    const CcKingdom *kingdom = KingdomById(sim, id);
    if (kingdom == NULL) return MUTED;
    return (Color){kingdom->color_r, kingdom->color_g, kingdom->color_b, 255};
}

static void DrawPanel(Rectangle bounds, Color color)
{
    DrawRectangleRounded(bounds, 0.08f, 8, color);
    DrawRectangleRoundedLinesEx(bounds, 0.08f, 8, 1.0f,
                                (Color){87, 115, 112, 180});
}

static void DrawBar(int x, int y, int width, const char *label,
                    int32_t value, Color color)
{
    DrawText(label, x, y, 11, MUTED);
    DrawRectangle(x + 76, y + 1, width, 10, (Color){34, 45, 48, 255});
    int fill = (int)((float)width * (float)value / 100.0f);
    DrawRectangle(x + 76, y + 1, fill, 10, color);
    DrawText(TextFormat("%d", value), x + 81 + width, y - 2, 12, INK);
}

static Color SituationColor(CcSituationKind kind);

static float GridDistance(Vector2 a, Vector2 b)
{
    float x = a.x - b.x;
    float y = a.y - b.y;
    return sqrtf(x * x + y * y);
}

static void ResetLocalState(LocalState *local)
{
    local->market_interior = false;
    CcLocalAgentInit(&local->agent, (Vector2){4.75f, 5.85f}, false);
    CcLocalCourseInit(&local->course);
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

#if 0
/* Retained temporarily as a visual reference while the 3D local renderer settles. */
static Vector2 IsoPoint(Vector2 grid, Vector2 origin, float half_width, float half_height)
{
    return (Vector2){origin.x + (grid.x - grid.y) * half_width,
                     origin.y + (grid.x + grid.y) * half_height};
}

static Vector2 StreetPoint(Vector2 grid)
{
    return IsoPoint(grid, (Vector2){474.0f, 112.0f}, 43.0f, 21.0f);
}

static Vector2 InteriorPoint(Vector2 grid)
{
    return IsoPoint(grid, (Vector2){465.0f, 144.0f}, 48.0f, 24.0f);
}

static void DrawTriangle2D(Vector2 a, Vector2 b, Vector2 c, Color color)
{
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross < 0.0f) DrawTriangle(a, b, c, color);
    else DrawTriangle(a, c, b, color);
}

static void DrawQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color)
{
    DrawTriangle2D(a, b, c, color);
    DrawTriangle2D(a, c, d, color);
}

static void DrawIsoTile(Vector2 point, float half_width, float half_height,
                        Color near_color, Color far_color)
{
    Vector2 top = {point.x, point.y - half_height};
    Vector2 right = {point.x + half_width, point.y};
    Vector2 bottom = {point.x, point.y + half_height};
    Vector2 left = {point.x - half_width, point.y};
    DrawTriangle2D(left, top, right, far_color);
    DrawTriangle2D(left, right, bottom, near_color);
}

static void DrawIsoBlock(Vector2 grid, float width, float depth, float height,
                         Color front, Color side, Color roof, bool interior)
{
    Vector2 origin = interior ? (Vector2){465.0f, 144.0f} : (Vector2){474.0f, 112.0f};
    float half_width = interior ? 48.0f : 43.0f;
    float half_height = interior ? 24.0f : 21.0f;
    Vector2 a = IsoPoint(grid, origin, half_width, half_height);
    Vector2 b = IsoPoint((Vector2){grid.x + width, grid.y}, origin, half_width, half_height);
    Vector2 c = IsoPoint((Vector2){grid.x + width, grid.y + depth}, origin,
                         half_width, half_height);
    Vector2 d = IsoPoint((Vector2){grid.x, grid.y + depth}, origin, half_width, half_height);
    Vector2 at = {a.x, a.y - height};
    Vector2 bt = {b.x, b.y - height};
    Vector2 ct = {c.x, c.y - height};
    Vector2 dt = {d.x, d.y - height};
    DrawQuad(d, c, ct, dt, front);
    DrawQuad(b, c, ct, bt, side);
    DrawQuad(at, bt, ct, dt, roof);
    DrawLineEx(dt, ct, 1.5f, Fade(INK, 0.20f));
    DrawLineEx(bt, ct, 1.5f, Fade(INK, 0.12f));
}

static void DrawPuppet(Vector2 ground, float scale, Color tunic, Color rig,
                       const char *label, float phase, bool player)
{
    float stride = sinf(phase) * 4.0f * scale;
    float bob = fabsf(sinf(phase)) * 1.8f * scale;
    Vector2 hip = {ground.x, ground.y - 28.0f * scale - bob};
    Vector2 chest = {ground.x, hip.y - 22.0f * scale};
    Vector2 neck = {ground.x, chest.y - 9.0f * scale};
    Vector2 shoulder_l = {chest.x - 11.0f * scale, chest.y + 2.0f * scale};
    Vector2 shoulder_r = {chest.x + 11.0f * scale, chest.y + 2.0f * scale};
    Vector2 elbow_l = {shoulder_l.x - 5.0f * scale, shoulder_l.y + 15.0f * scale + stride};
    Vector2 elbow_r = {shoulder_r.x + 5.0f * scale, shoulder_r.y + 15.0f * scale - stride};
    Vector2 hand_l = {elbow_l.x + 1.0f * scale, elbow_l.y + 12.0f * scale};
    Vector2 hand_r = {elbow_r.x - 1.0f * scale, elbow_r.y + 12.0f * scale};
    Vector2 knee_l = {hip.x - 7.0f * scale, hip.y + 16.0f * scale + stride};
    Vector2 knee_r = {hip.x + 7.0f * scale, hip.y + 16.0f * scale - stride};
    Vector2 foot_l = {knee_l.x - 4.0f * scale, ground.y - 2.0f * scale};
    Vector2 foot_r = {knee_r.x + 4.0f * scale, ground.y - 2.0f * scale};

    DrawEllipse((int)ground.x, (int)ground.y, 17.0f * scale, 6.0f * scale,
                (Color){2, 7, 10, 115});
    DrawLineEx(knee_l, foot_l, 7.0f * scale, (Color){42, 48, 52, 255});
    DrawLineEx(knee_r, foot_r, 7.0f * scale, (Color){42, 48, 52, 255});
    DrawTriangle2D((Vector2){hip.x, hip.y - 8.0f * scale},
                   (Vector2){hip.x - 15.0f * scale, hip.y + 17.0f * scale},
                   (Vector2){hip.x + 15.0f * scale, hip.y + 17.0f * scale}, tunic);
    DrawLineEx(shoulder_l, hand_l, 8.0f * scale, Fade(tunic, 0.95f));
    DrawLineEx(shoulder_r, hand_r, 8.0f * scale, Fade(tunic, 0.95f));
    DrawCircleV((Vector2){neck.x, neck.y - 9.0f * scale}, 11.0f * scale,
                (Color){222, 174, 139, 255});
    DrawCircleV((Vector2){neck.x - 3.4f * scale, neck.y - 11.0f * scale},
                1.3f * scale, BACKGROUND);
    DrawCircleV((Vector2){neck.x + 3.4f * scale, neck.y - 11.0f * scale},
                1.3f * scale, BACKGROUND);

    DrawLineEx(hip, chest, 2.0f * scale, rig);
    DrawLineEx(shoulder_l, shoulder_r, 2.0f * scale, rig);
    DrawLineEx(shoulder_l, elbow_l, 2.0f * scale, rig);
    DrawLineEx(elbow_l, hand_l, 2.0f * scale, rig);
    DrawLineEx(shoulder_r, elbow_r, 2.0f * scale, rig);
    DrawLineEx(elbow_r, hand_r, 2.0f * scale, rig);
    DrawLineEx(hip, knee_l, 2.0f * scale, rig);
    DrawLineEx(knee_l, foot_l, 2.0f * scale, rig);
    DrawLineEx(hip, knee_r, 2.0f * scale, rig);
    DrawLineEx(knee_r, foot_r, 2.0f * scale, rig);
    DrawLineEx(neck, (Vector2){neck.x, neck.y - 9.0f * scale}, 2.0f * scale, rig);
    Vector2 joints[] = {hip, chest, neck, shoulder_l, shoulder_r, elbow_l, elbow_r,
                        knee_l, knee_r};
    for (int32_t i = 0; i < (int32_t)(sizeof(joints) / sizeof(joints[0])); ++i) {
        DrawCircleV(joints[i], 2.5f * scale, rig);
    }
    if (player) {
        DrawPoly((Vector2){ground.x, neck.y - 29.0f * scale}, 4, 4.0f * scale,
                 45.0f, CC_GOLD);
    }
    if (label != NULL) {
        int width = MeasureText(label, 9);
        DrawRectangleRounded((Rectangle){ground.x - (float)width * 0.5f - 4.0f,
                                         ground.y + 7.0f,
                                         (float)width + 8.0f, 14.0f},
                             0.35f, 4, (Color){4, 10, 14, 205});
        DrawText(label, (int)ground.x - width / 2, (int)ground.y + 9, 9,
                 player ? CC_GOLD : INK);
    }
}

static void DrawLocalCarriage(Vector2 ground)
{
    DrawEllipse((int)ground.x, (int)ground.y + 7, 48.0f, 12.0f,
                (Color){2, 7, 10, 130});
    DrawIsoBlock((Vector2){1.05f, 5.55f}, 1.35f, 1.0f, 39.0f,
                 (Color){118, 62, 49, 255}, (Color){86, 44, 46, 255},
                 (Color){226, 174, 85, 255}, false);
    DrawCircle((int)ground.x - 25, (int)ground.y + 5, 13.0f,
               (Color){34, 28, 28, 255});
    DrawCircle((int)ground.x + 25, (int)ground.y + 5, 13.0f,
               (Color){34, 28, 28, 255});
    DrawCircleLines((int)ground.x - 25, (int)ground.y + 5, 10.0f, CC_GOLD);
    DrawCircleLines((int)ground.x + 25, (int)ground.y + 5, 10.0f, CC_GOLD);
    DrawText("THE CROWNLESS CARRIAGE", (int)ground.x - 74, (int)ground.y + 24,
             9, CC_GOLD);
}

static void DrawNoticeBoard(Vector2 ground, const CcSim *sim)
{
    DrawLineEx((Vector2){ground.x - 15.0f, ground.y - 27.0f},
               (Vector2){ground.x - 15.0f, ground.y}, 5.0f,
               (Color){82, 52, 39, 255});
    DrawLineEx((Vector2){ground.x + 15.0f, ground.y - 27.0f},
               (Vector2){ground.x + 15.0f, ground.y}, 5.0f,
               (Color){82, 52, 39, 255});
    DrawRectangleRounded((Rectangle){ground.x - 27.0f, ground.y - 54.0f,
                                     54.0f, 32.0f}, 0.12f, 4,
                         (Color){142, 91, 55, 255});
    int32_t live = CcSimActiveSituationCount(sim);
    for (int32_t i = 0; i < live && i < 4; ++i) {
        DrawRectangle((int)ground.x - 20 + i * 10, (int)ground.y - 47 + (i % 2) * 4,
                      8, 12, i == 3 ? Fade(DANGER, 0.8f) : (Color){229, 218, 177, 255});
    }
}

static bool SettlementHasSmugglerRoad(const CcSim *sim, CcId settlement_id)
{
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (!route->smuggler_route) continue;
        if (route->from_id == settlement_id || route->to_id == settlement_id) return true;
    }
    return false;
}

static void DrawStreetScene(const CcSim *sim, const LocalState *local, float clock)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    DrawRectangleGradientV(17, 81, 914, 570, (Color){22, 52, 59, 255}, BACKGROUND);
    BeginScissorMode(18, 82, 912, 568);

    for (int32_t sum = 0; sum <= 17; ++sum) {
        for (int32_t x = 0; x < 10; ++x) {
            int32_t y = sum - x;
            if (y < 0 || y >= 9) continue;
            Color near_color = ((x + y) & 1) ? (Color){39, 66, 59, 255} :
                                                (Color){42, 72, 63, 255};
            if (x >= 4 && x <= 6) near_color = (Color){86, 85, 72, 255};
            DrawIsoTile(StreetPoint((Vector2){(float)x, (float)y}), 43.0f, 21.0f,
                        near_color, Fade(near_color, 0.86f));
        }
    }

    Color kingdom = KingdomColor(sim, place->kingdom_id);
    DrawIsoBlock((Vector2){0.65f, 1.10f}, 2.05f, 1.75f, 72.0f,
                 (Color){86, 94, 91, 255}, (Color){61, 72, 73, 255}, kingdom, false);
    DrawIsoBlock((Vector2){3.85f, 0.50f}, 2.15f, 1.55f,
                 place->function == CC_SETTLEMENT_CAPITAL ? 119.0f : 91.0f,
                 (Color){99, 92, 83, 255}, (Color){69, 70, 72, 255},
                 Fade(kingdom, 0.92f), false);
    DrawIsoBlock((Vector2){6.60f, 1.00f}, 2.35f, 2.00f, 67.0f,
                 (Color){118, 79, 63, 255}, (Color){82, 58, 58, 255},
                 (Color){216, 158, 68, 255}, false);
    DrawText("MARKET", (int)StreetPoint((Vector2){7.35f, 2.55f}).x - 23,
             (int)StreetPoint((Vector2){7.35f, 2.55f}).y - 52, 10, CC_GOLD);
    DrawIsoBlock((Vector2){7.05f, 5.15f}, 2.10f, 1.85f, 73.0f,
                 (Color){72, 87, 94, 255}, (Color){55, 63, 72, 255},
                 (Color){123, 79, 126, 255}, false);

    int32_t market_crates = place->stock[CC_GOOD_FOOD] / 12;
    if (market_crates > 4) market_crates = 4;
    for (int32_t i = 0; i < market_crates; ++i) {
        Vector2 crate_grid = {6.25f + (float)(i % 2) * 0.43f,
                              3.35f + (float)(i / 2) * 0.48f};
        DrawIsoBlock(crate_grid, 0.36f, 0.36f, 18.0f,
                     (Color){142, 91, 53, 255}, (Color){101, 64, 48, 255},
                     (Color){196, 142, 69, 255}, false);
    }

    DrawNoticeBoard(StreetPoint(LOCAL_NOTICE), sim);
    DrawLocalCarriage(StreetPoint(LOCAL_CARRIAGE));

    const CcDungeon *dungeon = DungeonAtSettlement(sim, place->id);
    if (dungeon != NULL) {
        Vector2 entrance = StreetPoint(LOCAL_DUNGEON);
        DrawEllipse((int)entrance.x, (int)entrance.y, 39.0f, 20.0f,
                    (Color){8, 5, 14, 255});
        DrawPoly((Vector2){entrance.x, entrance.y - 3.0f}, 5, 24.0f, 0.0f,
                 Fade(CC_VIOLET, 0.45f));
        DrawText(TextFormat("%s / %d", CcDungeonStateName(dungeon->state),
                            dungeon->regional_pressure),
                 (int)entrance.x - 52, (int)entrance.y + 24, 9, CC_VIOLET);
    }

    DrawPuppet(StreetPoint((Vector2){6.30f, 3.95f}), 0.84f,
               (Color){223, 151, 68, 255}, TEAL, "merchant", clock * 1.4f, false);
    DrawPuppet(StreetPoint((Vector2){4.10f, 3.85f}), 0.86f, kingdom,
               CC_GOLD, "guard", clock * 1.1f + 1.0f, false);
    DrawPuppet(StreetPoint((Vector2){5.55f, 6.15f}), 0.80f,
               (Color){97, 154, 137, 255}, TEAL, "carter", clock * 1.2f + 2.0f, false);
    DrawPuppet(StreetPoint((Vector2){2.15f, 4.35f}), 0.78f,
               (Color){168, 112, 128, 255}, TEAL, "local", clock * 1.0f + 3.0f, false);
    if (place->hunger >= 30) {
        DrawPuppet(StreetPoint((Vector2){6.00f, 4.75f}), 0.72f,
                   (Color){91, 102, 104, 255}, DANGER, "food queue",
                   clock * 0.7f, false);
    }
    if (SettlementHasSmugglerRoad(sim, place->id) || place->security < 50) {
        DrawPuppet(StreetPoint((Vector2){8.05f, 5.00f}), 0.78f,
                   (Color){74, 60, 91, 255}, CC_VIOLET, "broker?",
                   clock * 0.8f + 4.0f, false);
    }
    DrawPuppet(StreetPoint(local->player), 0.92f, (Color){42, 128, 136, 255},
               CC_GOLD, "you", clock * 4.0f, true);
    EndScissorMode();
}

static void DrawMarketInterior(const CcSim *sim, const LocalState *local, float clock)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    DrawRectangleGradientV(17, 81, 914, 570, (Color){52, 34, 31, 255}, BACKGROUND);
    BeginScissorMode(18, 82, 912, 568);
    for (int32_t sum = 0; sum <= 14; ++sum) {
        for (int32_t x = 0; x < 9; ++x) {
            int32_t y = sum - x;
            if (y < 0 || y >= 7) continue;
            Color tile = ((x + y) & 1) ? (Color){108, 83, 64, 255} :
                                            (Color){119, 91, 68, 255};
            DrawIsoTile(InteriorPoint((Vector2){(float)x, (float)y}), 48.0f, 24.0f,
                        tile, Fade(tile, 0.84f));
        }
    }
    DrawIsoBlock((Vector2){0.20f, 0.15f}, 8.55f, 0.45f, 112.0f,
                 (Color){87, 57, 48, 255}, (Color){64, 43, 43, 255},
                 (Color){111, 73, 57, 255}, true);
    DrawIsoBlock((Vector2){6.05f, 1.85f}, 2.10f, 0.72f, 32.0f,
                 (Color){129, 80, 48, 255}, (Color){86, 57, 44, 255},
                 (Color){190, 128, 58, 255}, true);
    DrawIsoBlock((Vector2){1.10f, 1.15f}, 0.72f, 3.45f, 48.0f,
                 (Color){102, 68, 49, 255}, (Color){74, 50, 45, 255},
                 (Color){146, 99, 55, 255}, true);

    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t count = place->stock[good] / 18;
        if (count > 3) count = 3;
        for (int32_t i = 0; i < count; ++i) {
            Color top = good == CC_GOOD_FOOD ? CC_GOLD :
                        good == CC_GOOD_MATERIAL ? (Color){170, 139, 112, 255} : TEAL;
            DrawIsoBlock((Vector2){2.35f + (float)good * 1.00f,
                                   1.10f + (float)i * 0.55f},
                         0.52f, 0.52f, 22.0f,
                         Fade(top, 0.72f), Fade(top, 0.52f), top, true);
        }
    }
    DrawPuppet(InteriorPoint((Vector2){6.55f, 1.60f}), 0.92f,
               (Color){218, 148, 61, 255}, TEAL, "Mara / factor",
               clock * 1.1f, false);
    DrawPuppet(InteriorPoint(local->player), 0.94f, (Color){42, 128, 136, 255},
               CC_GOLD, "you", clock * 4.0f, true);

    Vector2 exit = InteriorPoint(INTERIOR_EXIT);
    DrawRectangleRounded((Rectangle){exit.x - 31.0f, exit.y - 66.0f, 62.0f, 66.0f},
                         0.08f, 4, (Color){31, 22, 23, 255});
    DrawText("STREET", (int)exit.x - 20, (int)exit.y - 39, 9, MUTED);
    EndScissorMode();
}
#endif

static void DrawLocalHeader(const CcSim *sim, const LocalState *local)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    DrawText(place != NULL ? place->name : "THE ROAD", 26, 18, 27, INK);
    DrawText(local->market_interior ? "MARKET HOUSE / PROCEDURAL FOOT CONTACTS" :
             "STREET LEVEL / BIOMECHANICAL MOVEMENT DOJO", 28, 51, 11, TEAL);
    DrawText(TextFormat("DAY %04d", sim->current_day), 790, 24, 15, CC_GOLD);
    DrawText(TextFormat("CROWNS %03" PRId64, sim->player.coins), 895, 24, 15, CC_GOLD);
    DrawText(TextFormat("CARGO %02d/%02d", CcPlayerCargoUsed(&sim->player),
                        sim->player.cargo_capacity), 1062, 24, 15, TEAL);
    DrawText("M  KINGDOM MAP", 1091, 52, 10, MUTED);
}

static void DrawLocalPanel(const CcSim *sim, const LocalState *local)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    DrawPanel((Rectangle){946.0f, 82.0f, 314.0f, 568.0f}, PANEL);
    const CcKingdom *kingdom = KingdomById(sim, place->kingdom_id);
    DrawText(kingdom != NULL ? kingdom->name : "UNCLAIMED", 966, 102, 11,
             KingdomColor(sim, place->kingdom_id));
    DrawText(CcSettlementFunctionName(place->function), 966, 123, 18, INK);
    DrawText(TextFormat("%d people live with these conditions", place->population),
             966, 149, 10, MUTED);
    DrawBar(966, 176, 124, "HUNGER", place->hunger, DANGER);
    DrawBar(966, 197, 124, "SECURITY", place->security, TEAL);
    DrawBar(966, 218, 124, "PROSPERITY", place->prosperity, CC_GOLD);

    DrawText("WHAT THE MARKET KNOWS", 966, 254, 11, TEAL);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int y = 278 + good * 30;
        DrawText(TextFormat("%d  %-9s", good + 1, CcGoodName((CcGood)good)),
                 966, y, 12, INK);
        DrawText(TextFormat("%2d cr", place->price[good]), 1086, y, 12, CC_GOLD);
        DrawText(TextFormat("%3d here", place->stock[good]), 1152, y, 11,
                 place->stock[good] < place->reserve_target[good] ? DANGER : MUTED);
    }
    bool can_trade = local->market_interior &&
                     GridDistance(LocalPosition(local), INTERIOR_COUNTER) < 2.25f;
    DrawText(can_trade ? "1-3 BUY  /  SHIFT+1-3 SELL" :
             local->market_interior ? "Walk to the factor to trade." :
             "Enter the market house to trade.",
             966, 374, 10, can_trade ? CC_GOLD : MUTED);

    const CcSituation *situation = CcSimSituationForSettlement(sim, place->id);
    DrawText("TOWN TALK", 966, 411, 11, TEAL);
    if (situation != NULL) {
        const CcFaction *issuer = FactionById(sim, situation->issuer_faction_id);
        DrawText(CcSituationKindName(situation->kind), 966, 435, 13,
                 SituationColor(situation->kind));
        DrawText(issuer != NULL ? issuer->name : "Quiet sponsors", 966, 456, 10, MUTED);
        if (situation->quantity > 0) {
            DrawText(TextFormat("%d/%d before day %d   +%" PRId64,
                                situation->progress, situation->quantity,
                                situation->deadline_day, situation->reward),
                     966, 477, 10, INK);
        } else {
            DrawText(TextFormat("Act before day %d   +%" PRId64,
                                situation->deadline_day, situation->reward),
                     966, 477, 10, INK);
        }
    } else {
        DrawText("No charter here today.", 966, 435, 12, MUTED);
    }
    const CcEvent *event = CcSimRecentEvent(sim, 0);
    DrawText("LAST CONSEQUENCE", 966, 522, 11, TEAL);
    if (event != NULL) {
        DrawText(TextFormat("DAY %d / %s", event->day, CcEventKindName(event->kind)),
                 966, 545, 9, MUTED);
        const char *text = event->text;
        char first[44] = {0};
        char second[44] = {0};
        size_t length = strlen(text);
        size_t split = length > 39U ? 39U : length;
        while (split > 0U && split < length && text[split] != ' ') split -= 1U;
        if (split == 0U) split = length > 39U ? 39U : length;
        (void)snprintf(first, sizeof(first), "%.*s", (int)split, text);
        if (split < length) (void)snprintf(second, sizeof(second), "%s", text + split + 1U);
        DrawText(first, 966, 566, 10, INK);
        if (second[0] != '\0') DrawText(second, 966, 582, 10, INK);
    }
    if (local->agent.morphology == CC_MORPHOLOGY_BIPED) {
        DrawText(TextFormat("BIO BIPED / %s / %s / muscles %.0f%%",
                            CcLocalTraversalName(local->agent.traversal),
                            CcHumanoidActionName(local->agent.humanoid.action),
                            CcBiomechRigMeanActivation(
                                &local->agent.humanoid.body) * 100.0f),
                 966, 605, 9, CC_VIOLET);
        DrawText(TextFormat("L %s / R %s / GRF %.0fN",
                            CcHumanoidContactName(
                                local->agent.humanoid.feet[0].contact),
                            CcHumanoidContactName(
                                local->agent.humanoid.feet[1].contact),
                            local->agent.humanoid.ground_reaction.y),
                 966, 621, 9, CC_VIOLET);
    } else if (local->agent.limb_rig.planted_count > 0 &&
               isfinite(local->agent.limb_rig.support_margin)) {
        DrawText(TextFormat("ROBOTIC %s / %s / traction %.0f%%",
                            CcLocalAgentMorphologyName(&local->agent),
                            CcLocalTraversalName(local->agent.traversal),
                            local->agent.limb_rig.traction * 100.0f),
                 966, 605, 9, CC_VIOLET);
        DrawText(TextFormat("support %d/%d / margin %+.2f / gait %.0f%%",
                            local->agent.limb_rig.planted_count,
                            local->agent.limb_rig.morphology.limb_count,
                            local->agent.limb_rig.support_margin,
                            local->agent.limb_rig.drive_scale * 100.0f),
                 966, 621, 9, CC_VIOLET);
    } else {
        DrawText(TextFormat("ROBOTIC %s / %s",
                            CcLocalAgentMorphologyName(&local->agent),
                            CcLocalTraversalName(local->agent.traversal)),
                 966, 605, 9, CC_VIOLET);
        DrawText("support airborne / feet searching", 966, 621, 9, CC_VIOLET);
    }
}

static const char *LocalPrompt(const CcSim *sim, const LocalState *local)
{
    Vector2 position = LocalPosition(local);
    if (local->market_interior) {
        if (GridDistance(position, INTERIOR_EXIT) < 1.25f) return "F  step back into the street";
        if (GridDistance(position, INTERIOR_COUNTER) < 2.25f) {
            return "1-3 buy   SHIFT+1-3 sell   prices reflect real regional supply";
        }
        return "LEFT CLICK any floor point   the biped steers around solid furniture";
    }
    if (GridDistance(position, LOCAL_MARKET) < 1.30f) return "F  enter the market house";
    if (GridDistance(position, LOCAL_CARRIAGE) < 1.35f) {
        return "F  unlatch the carriage map case";
    }
    if (GridDistance(position, LOCAL_NOTICE) < 1.15f) return "F  read the live situation board";
    if (DungeonAtSettlement(sim, sim->player.location_id) != NULL &&
        GridDistance(position, LOCAL_DUNGEON) < 1.35f) return "E  mount a three-day expedition";
    if (local->agent.traversal != CC_TRAVERSAL_IDLE) {
        return TextFormat("%s toward %.2f,%.2f   feet choose contacts continuously",
                          CcLocalTraversalName(local->agent.traversal),
                          local->agent.target_point.x, local->agent.target_point.z);
    }
    return "LEFT CLICK any point   the biped plants and swings its own feet";
}

static void DrawLocalFooter(const CcSim *sim, const LocalState *local)
{
    DrawPanel((Rectangle){20.0f, 664.0f, 1240.0f, 76.0f}, PANEL);
    DrawText(LocalPrompt(sim, local), 38, 681, 13, CC_GOLD);
    DrawText("SPACE strike   X guard   G alarm   Q situations   TAB ledger",
             38, 708, 10, MUTED);
    DrawText("M map case at carriage   F5 save   F9 load   N new world",
             804, 693, 10, MUTED);
}

#if 0
/* The former omniscient kingdom map is intentionally retired. Physical route
   charts below are the only travel-planning projection. */
static void DrawTerritories(const CcSim *sim)
{
    for (int32_t kingdom = 0; kingdom < sim->kingdom_count; ++kingdom) {
        Color color = Fade(KingdomColor(sim, sim->kingdoms[kingdom].id), 0.09f);
        for (int32_t settlement = 0; settlement < sim->settlement_count; ++settlement) {
            const CcSettlement *place = &sim->settlements[settlement];
            if (place->kingdom_id != sim->kingdoms[kingdom].id) continue;
            DrawCircleV(SettlementPoint(place), 132.0f, color);
            DrawCircleLinesV(SettlementPoint(place), 132.0f,
                             Fade(KingdomColor(sim, place->kingdom_id), 0.12f));
        }
    }
}

static void DrawRiver(void)
{
    Vector2 points[26];
    for (int32_t i = 0; i < 26; ++i) {
        float amount = (float)i / 25.0f;
        points[i] = (Vector2){83.0f + amount * 790.0f,
                              180.0f + amount * 360.0f + sinf(amount * 11.0f) * 32.0f};
    }
    DrawLineStrip(points, 26, (Color){34, 93, 112, 190});
    for (int32_t i = 0; i < 25; ++i) {
        DrawLineEx(points[i], points[i + 1], 9.0f, (Color){34, 93, 112, 130});
    }
}

static void DrawRoutes(const CcSim *sim)
{
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        if (from == NULL || to == NULL) continue;
        Vector2 a = SettlementPoint(from);
        Vector2 b = SettlementPoint(to);
        Color color = route->closed ? DANGER :
                      route->smuggler_route ? CC_VIOLET : (Color){156, 139, 100, 220};
        int32_t danger = CcSimRouteDanger(sim, route->id);
        if (!route->closed && danger >= 55) {
            DrawDashedLine(a, b, 6.0f, Fade(DANGER, 0.15f), route->smuggler_route);
        }
        DrawDashedLine(a, b, route->closed ? 5.0f : 3.0f,
                       Fade((Color){2, 8, 12, 255}, 0.75f), route->smuggler_route);
        DrawDashedLine(a, b, route->closed ? 2.6f : 1.6f,
                       color, route->smuggler_route);
        if (route->closed) {
            Vector2 center = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            DrawLineEx((Vector2){center.x - 8.0f, center.y - 8.0f},
                       (Vector2){center.x + 8.0f, center.y + 8.0f}, 3.0f, DANGER);
            DrawLineEx((Vector2){center.x + 8.0f, center.y - 8.0f},
                       (Vector2){center.x - 8.0f, center.y + 8.0f}, 3.0f, DANGER);
        }
    }
}

static void DrawShipments(const CcSim *sim, float clock)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        if (shipment->status != CC_SHIPMENT_TRAVELLING) continue;
        const CcSettlement *from = CcSimSettlement(sim, shipment->origin_id);
        const CcSettlement *to = CcSimSettlement(sim, shipment->destination_id);
        if (from == NULL || to == NULL) continue;
        int32_t duration = shipment->arrival_day - shipment->departure_day;
        float progress = duration > 0 ?
            (float)(sim->current_day - shipment->departure_day) / (float)duration : 1.0f;
        progress = fmaxf(0.08f, fminf(0.92f, progress + sinf(clock * 1.4f) * 0.018f));
        Vector2 a = SettlementPoint(from);
        Vector2 b = SettlementPoint(to);
        Vector2 point = {a.x + (b.x - a.x) * progress,
                         a.y + (b.y - a.y) * progress};
        Color cargo = shipment->good == CC_GOOD_FOOD ? GOLD :
                      shipment->good == CC_GOOD_MATERIAL ? (Color){181, 151, 119, 255} : TEAL;
        DrawRectangleRounded((Rectangle){point.x - 8.0f, point.y - 5.0f, 16.0f, 10.0f},
                             0.25f, 4, cargo);
        DrawCircle((int)point.x - 5, (int)point.y + 6, 3.0f, (Color){32, 28, 27, 255});
        DrawCircle((int)point.x + 5, (int)point.y + 6, 3.0f, (Color){32, 28, 27, 255});
    }
}

static void DrawThreats(const CcSim *sim, float clock)
{
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcRoute *route = CcSimRoute(sim, sim->bandits[i].route_id);
        if (route == NULL) continue;
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        if (from == NULL || to == NULL) continue;
        Vector2 a = SettlementPoint(from);
        Vector2 b = SettlementPoint(to);
        Vector2 point = {(a.x + b.x) * 0.5f + 18.0f,
                         (a.y + b.y) * 0.5f - 15.0f};
        float pulse = 7.0f + sinf(clock * 3.0f) * 1.5f;
        DrawCircleV(point, pulse + 4.0f, Fade(DANGER, 0.22f));
        DrawCircleV(point, pulse, DANGER);
        DrawText("B", (int)point.x - 4, (int)point.y - 6, 12, INK);
    }
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *dungeon = &sim->dungeons[i];
        const CcSettlement *place = CcSimSettlement(sim, dungeon->settlement_id);
        if (place == NULL) continue;
        Vector2 point = SettlementPoint(place);
        point.x += 48.0f;
        point.y -= 35.0f;
        DrawPoly(point, 4, 11.0f + sinf(clock * 2.0f), 45.0f, CC_VIOLET);
        DrawPolyLines(point, 4, 15.0f, 45.0f, Fade(CC_VIOLET, 0.45f));
    }
}

static void DrawCarriage(Vector2 position)
{
    DrawRectangleRounded((Rectangle){position.x - 15.0f, position.y - 12.0f,
                                     30.0f, 19.0f}, 0.20f, 5,
                         (Color){234, 200, 120, 255});
    DrawRectangleRec((Rectangle){position.x - 11.0f, position.y - 8.0f,
                                 22.0f, 4.0f},
                     (Color){34, 105, 110, 255});
    DrawCircle((int)position.x - 10, (int)position.y + 9, 5.0f,
               (Color){40, 31, 32, 255});
    DrawCircle((int)position.x + 10, (int)position.y + 9, 5.0f,
               (Color){40, 31, 32, 255});
    DrawCircleLines((int)position.x - 10, (int)position.y + 9, 5.0f, CC_GOLD);
    DrawCircleLines((int)position.x + 10, (int)position.y + 9, 5.0f, CC_GOLD);
}

static void DrawSettlements(const CcSim *sim, int32_t selected, float clock)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        Vector2 point = SettlementPoint(place);
        Color color = KingdomColor(sim, place->kingdom_id);
        if (i == selected) {
            DrawCircleLinesV(point, 28.0f + sinf(clock * 3.0f) * 2.0f, CC_GOLD);
        }
        DrawCircleV(point, 19.0f, Fade((Color){2, 7, 11, 255}, 0.88f));
        DrawPoly(point, place->function == CC_SETTLEMENT_CAPITAL ? 6 : 5,
                 place->function == CC_SETTLEMENT_CAPITAL ? 14.0f : 11.0f,
                 0.0f, color);
        if (place->hunger >= 40) {
            DrawCircleV((Vector2){point.x + 17.0f, point.y - 17.0f}, 6.0f, DANGER);
            DrawText("!", (int)point.x + 15, (int)point.y - 22, 10, INK);
        }
        const CcSituation *situation = CcSimSituationForSettlement(sim, place->id);
        if (situation != NULL) {
            Vector2 marker = {point.x - 18.0f, point.y - 18.0f};
            DrawPoly(marker, 4, 7.0f, 45.0f, CC_GOLD);
            DrawPolyLines(marker, 4, 10.0f, 45.0f, Fade(CC_GOLD, 0.42f));
        }
        int width = MeasureText(place->name, 13);
        DrawRectangleRounded((Rectangle){point.x - (float)width * 0.5f - 7.0f,
                                         point.y + 23.0f,
                                         (float)width + 14.0f, 20.0f},
                             0.30f, 5, (Color){7, 15, 21, 215});
        DrawText(place->name, (int)point.x - width / 2, (int)point.y + 27, 13, INK);
    }
    const CcSettlement *current = CcSimSettlement(sim, sim->player.location_id);
    if (current != NULL) {
        Vector2 point = SettlementPoint(current);
        point.y -= 34.0f;
        DrawCarriage(point);
    }
}

static void DrawMap(const CcSim *sim, int32_t selected, float clock)
{
    DrawPanel((Rectangle){20.0f, 82.0f, 900.0f, 568.0f},
              (Color){11, 25, 31, 246});
    BeginScissorMode(21, 83, 898, 566);
    DrawTerritories(sim);
    DrawRiver();
    DrawRoutes(sim);
    DrawShipments(sim, clock);
    DrawThreats(sim, clock);
    DrawSettlements(sim, selected, clock);
    EndScissorMode();

    DrawText("KNOWN KINGDOMS", 38, 98, 11, MUTED);
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        int x = 38 + i * 185;
        DrawCircle(x + 5, 127, 5.0f, KingdomColor(sim, sim->kingdoms[i].id));
        DrawText(sim->kingdoms[i].name, x + 17, 120, 13, INK);
        DrawText(TextFormat("L%02d", sim->kingdoms[i].legitimacy), x + 17, 136, 9, MUTED);
    }
}

static void DrawSettlementPanel(const CcSim *sim, int32_t selected)
{
    Rectangle panel = {938.0f, 82.0f, 322.0f, 568.0f};
    DrawPanel(panel, PANEL);
    const CcSettlement *place = selected >= 0 && selected < sim->settlement_count ?
                                &sim->settlements[selected] : NULL;
    if (place == NULL) return;
    const CcKingdom *kingdom = KingdomById(sim, place->kingdom_id);
    DrawText(kingdom != NULL ? kingdom->name : "UNCLAIMED", 958, 102, 12,
             KingdomColor(sim, place->kingdom_id));
    if (kingdom != NULL) {
        DrawText(TextFormat("TREASURY %" PRId64 "   LEGITIMACY %d", kingdom->treasury,
                            kingdom->legitimacy), 958, 119, 9, MUTED);
    }
    DrawText(place->name, 958, 137, 24, INK);
    DrawText(CcSettlementFunctionName(place->function), 958, 168, 12, MUTED);
    DrawText(TextFormat("POPULATION  %d", place->population), 958, 188, 11, INK);

    DrawBar(958, 214, 132, "HUNGER", place->hunger, DANGER);
    DrawBar(958, 235, 132, "SECURITY", place->security, TEAL);
    DrawBar(958, 256, 132, "PROSPERITY", place->prosperity, CC_GOLD);

    DrawText("LOCAL MARKET", 958, 291, 12, TEAL);
    DrawText("KEY GOOD      PRICE STOCK  IN CARGO", 958, 313, 9, MUTED);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int y = 335 + good * 27;
        DrawText(TextFormat("%d", good + 1), 961, y, 13, INK);
        DrawText(CcGoodName((CcGood)good), 982, y, 12, INK);
        DrawText(TextFormat("%2d", place->price[good]), 1071, y, 12, CC_GOLD);
        DrawText(TextFormat("%3d", place->stock[good]), 1117, y, 12, INK);
        DrawText(TextFormat("%2d", CcSimIncomingGood(sim, place->id, (CcGood)good)),
                 1170, y, 12, MUTED);
        DrawText(TextFormat("%2d", sim->player.cargo[good]), 1212, y, 12, TEAL);
    }

    int32_t primary = 0;
    for (int32_t good = 1; good < CC_GOOD_COUNT; ++good) {
        if (place->production[good] > place->production[primary]) primary = good;
    }
    DrawText(TextFormat("MAKES %s +%d/w   USES FOOD %d/w",
                        CcGoodName((CcGood)primary), place->production[primary],
                        place->consumption[CC_GOOD_FOOD]), 958, 417, 10, MUTED);

    const CcRoute *route = CcSimRouteBetween(sim, sim->player.location_id, place->id);
    DrawText("JOURNEY", 958, 445, 12, TEAL);
    if (place->id == sim->player.location_id) {
        DrawText("The carriage is here.", 958, 466, 13, INK);
    } else if (route == NULL) {
        DrawText("No direct route.", 958, 466, 13, MUTED);
    } else {
        DrawText(TextFormat("%d days  condition %d  danger %d%%", route->travel_days,
                            route->condition, CcSimRouteDanger(sim, route->id)),
                 958, 466, 12, INK);
        DrawText(route->closed ? "CLOSED BY CURRENT EVENTS" :
                 route->smuggler_route ? "UNLICENSED OLD ROAD" : "OPEN",
                 958, 486, 12,
                 route->closed ? DANGER : route->smuggler_route ? CC_VIOLET : TEAL);
        if (route->closed) DrawText("R  repair: 3 tools / 18 crowns", 958, 505, 11, CC_GOLD);
        else DrawText("ENTER  commit to journey", 958, 505, 11, CC_GOLD);
    }

    const CcSituation *situation = CcSimSituationForSettlement(sim, place->id);
    if (situation != NULL) {
        const CcFaction *issuer = FactionById(sim, situation->issuer_faction_id);
        DrawText("LIVE SITUATION", 958, 536, 11, CC_GOLD);
        DrawText(CcSituationKindName(situation->kind), 958, 555, 14, INK);
        if (situation->quantity > 0) {
            DrawText(TextFormat("Progress %d/%d   due %d   +%" PRId64,
                                situation->progress, situation->quantity,
                                situation->deadline_day, situation->reward),
                     958, 578, 11, TEAL);
        } else {
            DrawText(TextFormat("Due day %d   reward +%" PRId64,
                                situation->deadline_day, situation->reward),
                     958, 578, 11, TEAL);
        }
        DrawText(issuer != NULL ? issuer->name : "Anonymous sponsors", 958, 598, 10, MUTED);
        const char *instruction = situation->kind == CC_SITUATION_ROUTE_REPAIR ?
                                  "R repair from either endpoint" :
                                  situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
                                  "E mount an expedition here" :
                                  situation->kind == CC_SITUATION_RELIEF_DELIVERY ?
                                  "Sell food here to fulfill" : "Sell tools here quietly";
        DrawText(instruction, 958, 619, 11, CC_GOLD);
    } else {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            const CcDungeon *dungeon = &sim->dungeons[i];
            if (dungeon->settlement_id != place->id) continue;
            DrawText("REGIONAL DUNGEON", 958, 536, 11, CC_VIOLET);
            DrawText(dungeon->name, 958, 557, 13, INK);
            DrawText(TextFormat("%s  pressure %d", CcDungeonStateName(dungeon->state),
                                dungeon->regional_pressure), 958, 580, 11, MUTED);
            if (place->id == sim->player.location_id) {
                DrawText("E  mount a 3-day expedition", 958, 605, 11, CC_GOLD);
            }
        }
    }
}
#endif

static bool MapVisibleAtCarriage(const CcSim *sim, const CcMap *map)
{
    return sim != NULL && map != NULL &&
           (map->owner_id == sim->player.id ||
            map->owner_id == sim->player.location_id);
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

static CcId RouteOtherEnd(const CcRoute *route, CcId here)
{
    if (route == NULL) return 0U;
    if (route->from_id == here) return route->to_id;
    if (route->to_id == here) return route->from_id;
    return 0U;
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
    int width = MeasureText(name, 15);
    DrawText(name, (int)point.x - width / 2, (int)point.y + 24, 15, ink);
    if (current) DrawText("CARRIAGE", (int)point.x - 34, (int)point.y - 38, 10, DANGER);
}

static void DrawMap(const CcSim *sim, int32_t selected, float clock)
{
    (void)clock;
    DrawPanel((Rectangle){20.0f, 82.0f, 900.0f, 568.0f},
              (Color){11, 20, 24, 248});
    DrawText("CARRIAGE MAP CASE", 38, 101, 18, CC_GOLD);
    DrawText(TextFormat("%d / %d PHYSICAL CHARTS", CcPlayerMapCount(sim),
                        sim->player.map_capacity), 38, 126, 10, MUTED);

    int32_t row = 0;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (!MapVisibleAtCarriage(sim, map)) continue;
        bool owned = map->owner_id == sim->player.id;
        int y = 154 + row * 51;
        Color paper_color = map->contraband ? (Color){57, 34, 55, 255} :
                                              (Color){52, 48, 37, 255};
        if (i == selected) {
            paper_color = map->contraband ? (Color){96, 49, 91, 255} :
                                            (Color){91, 78, 49, 255};
        }
        DrawRectangleRounded((Rectangle){37.0f, (float)y, 226.0f, 43.0f},
                             0.16f, 5, paper_color);
        DrawText(map->name, 48, y + 7, 11, INK);
        DrawText(owned ? "IN THE CASE" : TextFormat("FOR SALE  %d C", map->ask_price),
                 48, y + 25, 9, owned ? TEAL : CC_GOLD);
        if (map->contraband) DrawText("ILLICIT", 205, y + 25, 8, CC_VIOLET);
        row += 1;
    }
    if (row == 0) DrawText("No charts are present at this stop.", 42, 172, 11, MUTED);

    const CcMap *map = SelectedVisibleMap(sim, selected);
    Rectangle paper = {282.0f, 105.0f, 620.0f, 525.0f};
    DrawRectangleRounded(paper, 0.025f, 4,
                         map != NULL && map->contraband ?
                         (Color){158, 132, 119, 255} : (Color){214, 197, 151, 255});
    DrawRectangleRoundedLinesEx(paper, 0.025f, 4, 2.0f,
                                (Color){91, 69, 46, 220});
    if (map == NULL) {
        DrawText("THE CASE IS EMPTY", 452, 335, 22, (Color){83, 65, 46, 255});
        return;
    }
    const CcRoute *route = CcSimRoute(sim, map->route_id);
    const CcSettlement *from = route != NULL ? CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ? CcSimSettlement(sim, route->to_id) : NULL;
    Color chart_ink = map->contraband ? (Color){86, 31, 72, 255} :
                                       (Color){66, 57, 43, 255};
    DrawText(map->name, 307, 125, 20, chart_ink);
    DrawText(TextFormat("surveyed day %d  /  hand %02d", map->surveyed_day,
                        map->accuracy), 309, 151, 10, Fade(chart_ink, 0.75f));
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
    DrawText("N?", 838, 202, 10, chart_ink);
    DrawText(map->contraband ? "Copied under shuttered lanterns" :
                              "Distances are the cartographer's argument",
             309, 596, 10, Fade(chart_ink, 0.74f));
}

static void DrawSettlementPanel(const CcSim *sim, int32_t selected)
{
    Rectangle panel = {938.0f, 82.0f, 322.0f, 568.0f};
    DrawPanel(panel, PANEL);
    const CcMap *map = SelectedVisibleMap(sim, selected);
    const CcSettlement *here = CcSimSettlement(sim, sim->player.location_id);
    DrawText("CARTOGRAPHER'S CASE", 958, 102, 12, TEAL);
    DrawText(here != NULL ? here->name : "Unknown stop", 958, 125, 22, INK);
    DrawText(TextFormat("CROWNS %" PRId64 "   CASE %d/%d", sim->player.coins,
                        CcPlayerMapCount(sim), sim->player.map_capacity),
             958, 154, 10, MUTED);
    if (map == NULL) {
        DrawText("No carried or locally offered", 958, 204, 12, MUTED);
        DrawText("route maps are available.", 958, 223, 12, MUTED);
        return;
    }
    const CcRoute *route = CcSimRoute(sim, map->route_id);
    const CcSettlement *from = route != NULL ? CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ? CcSimSettlement(sim, route->to_id) : NULL;
    const CcSettlement *maker = CcSimSettlement(sim, map->maker_settlement_id);
    DrawText("THIS OBJECT DEPICTS", 958, 193, 10, MUTED);
    DrawText(from != NULL ? from->name : "Unknown", 958, 214, 16, INK);
    DrawText("TO", 958, 235, 9, MUTED);
    DrawText(to != NULL ? to->name : "Unknown", 958, 252, 16, INK);
    DrawText(TextFormat("MAKER  %s", maker != NULL ? maker->name : "unknown"),
             958, 289, 10, MUTED);
    DrawText(TextFormat("AGE    %d days", sim->current_day - map->surveyed_day),
             958, 308, 10, MUTED);
    DrawBar(958, 337, 124, "ACCURACY", map->accuracy, TEAL);
    DrawBar(958, 361, 124, "ROAD INK", map->recorded_condition, CC_GOLD);
    DrawBar(958, 385, 124, "DANGER", map->recorded_danger, DANGER);
    DrawText("These are recorded claims,", 958, 421, 10, MUTED);
    DrawText("not live world-state telemetry.", 958, 437, 10, MUTED);

    bool owned = map->owner_id == sim->player.id;
    CcId destination_id = route != NULL ? RouteOtherEnd(route, sim->player.location_id) : 0U;
    const CcSettlement *destination = CcSimSettlement(sim, destination_id);
    if (!owned) {
        DrawText(TextFormat("B  BUY FOR %d CROWNS", map->ask_price),
                 958, 486, 13, CC_GOLD);
    } else {
        DrawText(TextFormat("S  SELL FOR %d CROWNS", map->ask_price * 2 / 3),
                 958, 486, 11, MUTED);
        DrawText(destination != NULL ?
                 TextFormat("ENTER  FOLLOW TO %s", destination->name) :
                 "This sheet begins elsewhere.",
                 958, 511, destination != NULL ? 12 : 11,
                 destination != NULL ? CC_GOLD : MUTED);
        if (route != NULL && route->closed && destination != NULL) {
            DrawText("R  attempt work at the route", 958, 535, 10, TEAL);
        }
    }
    DrawText("LEFT/RIGHT  leaf through objects", 958, 584, 9, MUTED);
    DrawText("M  close case   Q situations", 958, 604, 9, MUTED);
    DrawText("No chart reveals the whole world.", 958, 624, 9,
             map->contraband ? CC_VIOLET : MUTED);
}

static void DrawHeader(const CcSim *sim)
{
    DrawText("CROWNLESS CARRIAGE", 26, 20, 27, INK);
    DrawText("PHYSICAL CARTOGRAPHY / ONE OBJECT, ONE ROUTE", 28, 53, 11, TEAL);
    DrawText(TextFormat("DAY %04d", sim->current_day), 790, 25, 16, CC_GOLD);
    DrawText(TextFormat("CROWNS %03" PRId64, sim->player.coins), 900, 25, 16, CC_GOLD);
    DrawText(TextFormat("MAPS %02d/%02d", CcPlayerMapCount(sim),
                        sim->player.map_capacity), 1060, 25, 16, TEAL);
    DrawText(TextFormat("WORLD %08X  STATE %08" PRIx64 "  REP %d  LIVE %d",
                        sim->world_seed, CcSimHash(sim) & UINT64_C(0xffffffff),
                        sim->player.reputation, CcSimActiveSituationCount(sim)),
             790, 53, 10, MUTED);
}

static void DrawEventRibbon(const CcSim *sim)
{
    DrawPanel((Rectangle){20.0f, 664.0f, 1240.0f, 76.0f}, PANEL);
    const CcEvent *event = CcSimRecentEvent(sim, 0);
    if (event != NULL) {
        DrawText(TextFormat("DAY %d  %s", event->day, CcEventKindName(event->kind)),
                 38, 680, 11, event->kind == CC_EVENT_MONSTER_PRESSURE ? CC_VIOLET : TEAL);
        DrawText(event->text, 38, 704, 14, INK);
    }
    DrawText("LEFT/RIGHT leaf charts   B buy   S sell   ENTER follow",
             735, 679, 10, MUTED);
    DrawText("M close case   Q situations   TAB ledger   F5 save   F9 load",
             765, 704, 10, MUTED);
}

static void DrawLedger(const CcSim *sim)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){3, 7, 11, 210});
    Rectangle bounds = {130.0f, 70.0f, 1020.0f, 620.0f};
    DrawPanel(bounds, (Color){8, 17, 24, 250});
    DrawText("CAUSAL EVENT LEDGER", 160, 100, 26, INK);
    DrawText("The player sees symptoms; this inspector retains auditable provenance.",
             160, 136, 13, MUTED);
    DrawText("DAY   TYPE        MAG   PARENT       EVENT", 160, 174, 11, TEAL);
    int32_t shown = sim->event_count < 15 ? sim->event_count : 15;
    for (int32_t i = 0; i < shown; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) continue;
        int y = 202 + i * 29;
        DrawText(TextFormat("%4d", event->day), 160, y, 11, CC_GOLD);
        DrawText(CcEventKindName(event->kind), 211, y, 11,
                 event->kind == CC_EVENT_MONSTER_PRESSURE ? CC_VIOLET : TEAL);
        DrawText(TextFormat("%3d", event->magnitude), 303, y, 11, INK);
        DrawText(TextFormat("%08" PRIx64, event->parent_id & UINT64_C(0xffffffff)),
                 348, y, 11, MUTED);
        DrawText(event->text, 435, y, 11, INK);
    }
    DrawText("TAB  return to where you were", 160, 657, 12, CC_GOLD);
}

static Color SituationColor(CcSituationKind kind)
{
    if (kind == CC_SITUATION_RELIEF_DELIVERY) return TEAL;
    if (kind == CC_SITUATION_ROUTE_REPAIR) return CC_GOLD;
    if (kind == CC_SITUATION_MONSTER_EXPEDITION) return CC_VIOLET;
    return DANGER;
}

static void DrawSituationBoard(const CcSim *sim)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){3, 7, 11, 218});
    Rectangle bounds = {110.0f, 60.0f, 1060.0f, 640.0f};
    DrawPanel(bounds, (Color){8, 17, 24, 252});
    DrawText("THE ROAD IS ASKING", 142, 91, 26, INK);
    DrawText("Conditions become dated opportunities. Ignore them and other powers act.",
             142, 128, 13, MUTED);
    DrawText(TextFormat("%d LIVE SITUATIONS", CcSimActiveSituationCount(sim)),
             930, 101, 12, CC_GOLD);
    DrawText("CHARTER                  TARGET                    PROGRESS      DUE    REWARD",
             142, 166, 11, TEAL);
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->situation_count && shown < 10; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE) continue;
        int y = 198 + shown * 43;
        char target[96];
        SituationTargetLabel(sim, situation, target, sizeof(target));
        DrawText(CcSituationKindName(situation->kind), 142, y, 13,
                 SituationColor(situation->kind));
        DrawText(target, 356, y, 12, INK);
        DrawText(situation->quantity > 0 ?
                 TextFormat("%d / %d", situation->progress, situation->quantity) : "INTERVENE",
                 665, y, 12, MUTED);
        DrawText(TextFormat("%4d", situation->deadline_day), 813, y, 12, CC_GOLD);
        DrawText(TextFormat("+%" PRId64, situation->reward), 910, y, 12, TEAL);
        const CcFaction *issuer = FactionById(sim, situation->issuer_faction_id);
        DrawText(issuer != NULL ? issuer->name : "Unrecorded sponsors", 356, y + 18, 9, MUTED);
        DrawLine(142, y + 33, 1125, y + 33, Fade(MUTED, 0.14f));
        shown += 1;
    }
    if (shown == 0) {
        DrawText("No power has posted a live charter. Advance the world and conditions will change.",
                 142, 220, 14, MUTED);
    }
    DrawText("Q  return to where you were", 142, 665, 12, CC_GOLD);
}

static bool ApplyCommand(CcSim *sim, CcCommand command,
                         char *message, size_t message_capacity)
{
    char error[192];
    if (!CcSimApply(sim, &command, error, sizeof(error))) {
        (void)snprintf(message, message_capacity, "%s", error);
        return false;
    }
    const CcEvent *event = CcSimRecentEvent(sim, 0);
    (void)snprintf(message, message_capacity, "%s",
                   event != NULL ? event->text : "World state changed.");
    return true;
}

static void HandleExpedition(CcSim *sim, const CcDungeon *dungeon,
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
    (void)ApplyCommand(sim, expedition, message, message_capacity);
}

static void HandleInput(CcSim *sim, int32_t *selected, ClientView *view,
                        ClientView *return_view, LocalState *local,
                        RenderTexture2D local_target, Rectangle local_bounds,
                        const char *save_path, char *message, size_t message_capacity)
{
    if (IsKeyPressed(KEY_TAB)) {
        if (*view == VIEW_LEDGER) {
            *view = *return_view;
        } else {
            *return_view = *view;
            *view = VIEW_LEDGER;
        }
        return;
    }
    if (IsKeyPressed(KEY_Q)) {
        if (*view == VIEW_SITUATIONS) {
            *view = *return_view;
        } else {
            *return_view = *view;
            *view = VIEW_SITUATIONS;
        }
        return;
    }
    if (IsKeyPressed(KEY_M)) {
        if (*view == VIEW_MAP) {
            *view = VIEW_LOCAL;
        } else if (*view == VIEW_LOCAL && !local->market_interior &&
                   GridDistance(LocalPosition(local), LOCAL_CARRIAGE) < 1.35f) {
            *selected = FirstVisibleMapIndex(sim);
            *view = VIEW_MAP;
        } else {
            (void)snprintf(message, message_capacity,
                           "The map case is a physical carriage fitting; approach it first.");
        }
        return;
    }
    if (*view == VIEW_LEDGER || *view == VIEW_SITUATIONS) return;

    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if (IsKeyPressed(KEY_F5) || (control && IsKeyPressed(KEY_S))) {
        char error[256];
        bool saved = CcSaveWrite(save_path, sim, error, sizeof(error));
        (void)snprintf(message, message_capacity, "%s",
                       saved ? "Campaign committed atomically to SQLite." : error);
    }
    if (IsKeyPressed(KEY_F9)) {
        char error[256];
        bool loaded = CcSaveRead(save_path, sim, error, sizeof(error));
        (void)snprintf(message, message_capacity, "%s",
                       loaded ? "Campaign restored with matching state hash." : error);
        if (loaded) {
            *selected = FirstVisibleMapIndex(sim);
            ResetLocalState(local);
            *view = VIEW_LOCAL;
        }
    }
    if (IsKeyPressed(KEY_N)) {
        CcSimInit(sim, sim->world_seed + UINT32_C(0x9e3779b9));
        *selected = FirstVisibleMapIndex(sim);
        ResetLocalState(local);
        *view = VIEW_LOCAL;
        (void)snprintf(message, message_capacity, "A new deterministic region is founded.");
    }
    if (IsKeyPressed(KEY_PERIOD)) {
        CcSimAdvanceDays(sim, 1);
        (void)snprintf(message, message_capacity, "One day passes; local prices and pressures react.");
    }
    if (IsKeyPressed(KEY_K)) {
        CcSimAdvanceDays(sim, 7);
        (void)snprintf(message, message_capacity, "One strategic week passes beneath street life.");
    }

    if (*view == VIEW_LOCAL) {
        if (IsKeyPressed(KEY_X) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED) {
            bool guarded = local->agent.humanoid.action !=
                           CC_HUMANOID_ACTION_GUARD;
            CcHumanoidGaitSetGuarded(&local->agent.humanoid, guarded);
            (void)snprintf(
                message, message_capacity, "%s",
                guarded ? "You settle behind your hands and hips; X releases the guard." :
                          "You release the guarded stance.");
        }
        if (IsKeyPressed(KEY_SPACE) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED) {
            bool struck = CcHumanoidGaitBeginStrike(
                &local->agent.humanoid, 1);
            (void)snprintf(
                message, message_capacity, "%s",
                struck ? "The thrust travels from feet through hip, spine, shoulder, and hand." :
                         "No strike: the body is already committed to another action.");
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CcLocalAgentPickTarget(&local->agent, GetMousePosition(), local_target,
                                   local_bounds, local->market_interior)) {
            (void)snprintf(message, message_capacity,
                           "Target %.2f,%.2f; the biped is choosing foot contacts.",
                           local->agent.target_point.x, local->agent.target_point.z);
        }
        float local_delta_time = GetFrameTime();
        CcLocalAgentUpdate(&local->agent, local_delta_time,
                           local->market_interior);
        if (!local->market_interior) {
            if (IsKeyPressed(KEY_G)) {
                if (!local->course.alarm_active) {
                    CcLocalCourseRaiseAlarm(&local->course);
                    (void)snprintf(message, message_capacity,
                                   "The village bell sounds; the trainees break course and form a defense line.");
                } else {
                    (void)snprintf(message, message_capacity,
                                   "The village alarm is already sounding.");
                }
            }
            CcLocalCourseUpdate(&local->course, sim, local_delta_time);
        }
        Vector2 position = LocalPosition(local);

        if (IsKeyPressed(KEY_F)) {
            if (local->market_interior &&
                GridDistance(position, INTERIOR_EXIT) < 1.25f) {
                local->market_interior = false;
                CcLocalAgentInit(&local->agent, (Vector2){6.80f, 4.25f}, false);
                (void)snprintf(message, message_capacity,
                               "The market door closes behind you; the street keeps moving.");
            } else if (!local->market_interior &&
                       GridDistance(position, LOCAL_MARKET) < 1.30f) {
                local->market_interior = true;
                CcLocalAgentInit(&local->agent, (Vector2){2.05f, 5.35f}, true);
                (void)snprintf(message, message_capacity,
                               "You enter a market whose shelves are filled by the real simulation.");
            } else if (!local->market_interior &&
                       GridDistance(position, LOCAL_CARRIAGE) < 1.35f) {
                *selected = FirstVisibleMapIndex(sim);
                *view = VIEW_MAP;
                (void)snprintf(message, message_capacity,
                               "You unlatch the carriage map case; each sheet depicts one route.");
            } else if (!local->market_interior &&
                       GridDistance(position, LOCAL_NOTICE) < 1.15f) {
                *return_view = VIEW_LOCAL;
                *view = VIEW_SITUATIONS;
            }
        }

        bool can_trade = local->market_interior &&
                         GridDistance(position, INTERIOR_COUNTER) < 2.25f;
        if (can_trade) {
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                if (!IsKeyPressed(KEY_ONE + good)) continue;
                CcCommand trade = {
                    .kind = CC_COMMAND_TRADE,
                    .good = (CcGood)good,
                    .amount = shift ? -1 : 1
                };
                (void)ApplyCommand(sim, trade, message, message_capacity);
            }
        }
        const CcDungeon *dungeon = DungeonAtSettlement(sim, sim->player.location_id);
        if (!local->market_interior && dungeon != NULL && IsKeyPressed(KEY_E) &&
            GridDistance(position, LOCAL_DUNGEON) < 1.35f) {
            HandleExpedition(sim, dungeon, message, message_capacity);
        }
        return;
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)) {
        *selected = StepVisibleMapIndex(sim, *selected, 1);
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) {
        *selected = StepVisibleMapIndex(sim, *selected, -1);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        int32_t row = 0;
        for (int32_t i = 0; i < sim->map_count; ++i) {
            if (!MapVisibleAtCarriage(sim, &sim->maps[i])) continue;
            Rectangle item = {37.0f, (float)(154 + row * 51), 226.0f, 43.0f};
            if (CheckCollisionPointRec(mouse, item)) *selected = i;
            row += 1;
        }
    }

    const CcMap *map = SelectedVisibleMap(sim, *selected);
    if (map == NULL) return;
    CcId map_id = map->id;
    if (IsKeyPressed(KEY_B) && map->owner_id == sim->player.location_id) {
        CcCommand buy = {.kind = CC_COMMAND_BUY_MAP, .target_id = map_id};
        (void)ApplyCommand(sim, buy, message, message_capacity);
        return;
    }
    if (IsKeyPressed(KEY_S) && map->owner_id == sim->player.id) {
        CcCommand sell = {.kind = CC_COMMAND_SELL_MAP, .target_id = map_id};
        (void)ApplyCommand(sim, sell, message, message_capacity);
        return;
    }

    const CcRoute *route = CcSimRoute(sim, map->route_id);
    CcId destination_id = RouteOtherEnd(route, sim->player.location_id);
    if (IsKeyPressed(KEY_ENTER) && map->owner_id == sim->player.id &&
        destination_id != 0U) {
        CcCommand travel = {
            .kind = CC_COMMAND_TRAVEL,
            .target_id = destination_id
        };
        if (ApplyCommand(sim, travel, message, message_capacity)) {
            *selected = FirstVisibleMapIndex(sim);
            ResetLocalState(local);
            *view = VIEW_LOCAL;
        }
    }
    if (IsKeyPressed(KEY_R) && map->owner_id == sim->player.id &&
        destination_id != 0U) {
        CcCommand repair = {
            .kind = CC_COMMAND_REPAIR_ROUTE,
            .target_id = route != NULL ? route->id : 0U
        };
        (void)ApplyCommand(sim, repair, message, message_capacity);
    }
}

int main(int argc, char **argv)
{
    bool render_benchmark = argc >= 2 &&
                            strcmp(argv[1], "--benchmark-render") == 0;
    int32_t render_benchmark_frames = 600;
    if (render_benchmark && argc >= 3) {
        char *end = NULL;
        long parsed = strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0' || parsed <= 0 || parsed > INT32_MAX) {
            (void)fprintf(stderr, "Render benchmark frame count is invalid.\n");
            return 1;
        }
        render_benchmark_frames = (int32_t)parsed;
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
    bool capture_map_case = argc >= 2 &&
                            strcmp(argv[1], "--capture-map-case") == 0;
    bool capture_dojo = argc >= 2 && strcmp(argv[1], "--capture-dojo") == 0;
    bool capture = argc >= 2 &&
                   (strcmp(argv[1], "--capture") == 0 || capture_board ||
                    capture_interior || capture_navigation || capture_limbs ||
                    capture_walk_cycle || capture_defense ||
                    capture_downclimb || capture_map_case || capture_dojo);
    const char *capture_path = argc >= 3 ? argv[2] : "architecture-proof.png";
    char save_path[640];
    CampaignSavePath(save_path, sizeof(save_path));

    if (render_benchmark) SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE |
                   (capture ? FLAG_WINDOW_HIDDEN : 0U));
    InitWindow(1280, 760, "Crownless Carriage — living world spine");
    if (!IsWindowReady()) {
        (void)fprintf(stderr,
                      "Crownless Carriage could not connect to the desktop window server.\n");
        return 1;
    }
    SetWindowMinSize(1080, 680);
    SetTargetFPS(render_benchmark ? 0 : 60);
    RenderTexture2D local_target = LoadRenderTexture(914, 570);
    SetTextureFilter(local_target.texture, TEXTURE_FILTER_BILINEAR);
    CcLocalRendererInit();

    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    if (capture || render_benchmark) CcSimAdvanceDays(&sim, 28);
    if (capture_map_case) sim.player.location_id = sim.settlements[1].id;
    int32_t selected = FirstVisibleMapIndex(&sim);
    ClientView view = capture_board ? VIEW_SITUATIONS :
                      capture_map_case ? VIEW_MAP : VIEW_LOCAL;
    ClientView return_view = VIEW_LOCAL;
    LocalState local;
    CcLocalAgent walk_cycle_frames[8] = {0};
    uint32_t walk_cycle_mask = 0;
    ResetLocalState(&local);
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
        CcLocalCourseRaiseAlarm(&local.course);
        int32_t fighting_frames = 0;
        for (int32_t frame = 0; frame < 2400 && fighting_frames < 20; ++frame) {
            CcLocalCourseUpdate(&local.course, &sim, 1.0f / 60.0f);
            fighting_frames = local.course.alarm_active &&
                              !local.course.raiders_retreating &&
                              local.course.raider_resolve < 100 ?
                              fighting_frames + 1 : 0;
        }
        (void)printf("defense capture: alarm %d retreat %d resolve %d wins %d fight frames %d\n",
                     local.course.alarm_active,
                     local.course.raiders_retreating,
                     local.course.raider_resolve,
                     local.course.defenses_completed, fighting_frames);
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
            CcLocalCourseUpdate(&local.course, &sim, 1.0f / 60.0f);
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
    char message[256] = "Click any surface: the biped walks there with planted procedural feet.";
    int capture_frames = 0;
    int walk_frame_count = 0;
    int32_t render_benchmark_count = 0;
    double render_benchmark_started = GetTime();

    Rectangle local_bounds = {17.0f, 81.0f, 914.0f, 570.0f};
    while (!WindowShouldClose()) {
        if (capture_walk_cycle) {
            local.agent = walk_cycle_frames[walk_frame_count];
        } else {
            HandleInput(&sim, &selected, &view, &return_view, &local,
                        local_target, local_bounds,
                        save_path, message, sizeof(message));
        }
        float clock = (float)GetTime();

        BeginDrawing();
        ClearBackground(BACKGROUND);
        bool map_underlay = view == VIEW_MAP ||
                            ((view == VIEW_LEDGER || view == VIEW_SITUATIONS) &&
                             return_view == VIEW_MAP);
        if (map_underlay) {
            DrawHeader(&sim);
            DrawMap(&sim, selected, clock);
            DrawSettlementPanel(&sim, selected);
            DrawEventRibbon(&sim);
        } else {
            DrawLocalHeader(&sim, &local);
            if (local.market_interior) {
                CcLocalDrawMarket3D(&sim, &local.agent, clock,
                                    local_target, local_bounds);
            } else {
                CcLocalDrawStreet3D(&sim, &local.agent, &local.course, clock,
                                    local_target, local_bounds);
            }
            DrawLocalPanel(&sim, &local);
            DrawLocalFooter(&sim, &local);
        }
        if (message[0] != '\0') {
            int width = MeasureText(message, 12) + 24;
            int maximum = GetScreenWidth() - 80;
            if (width > maximum) width = maximum;
            DrawRectangleRounded((Rectangle){40.0f, 625.0f, (float)width, 27.0f},
                                 0.25f, 5, (Color){6, 12, 18, 230});
            DrawText(message, 52, 632, 12, INK);
        }
        if (view == VIEW_LEDGER) DrawLedger(&sim);
        if (view == VIEW_SITUATIONS) DrawSituationBoard(&sim);
        EndDrawing();

        if (render_benchmark) {
            render_benchmark_count += 1;
            if (render_benchmark_count >= render_benchmark_frames) break;
        } else if (capture_walk_cycle) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%02d.png",
                           capture_path, walk_frame_count);
            TakeScreenshot(frame_path);
            walk_frame_count += 1;
            if (walk_frame_count >= 8) break;
        } else if (capture) {
            capture_frames += 1;
            if (capture_frames >= 3) {
                TakeScreenshot(capture_path);
                break;
            }
        }
    }

    double render_benchmark_elapsed = render_benchmark ?
        GetTime() - render_benchmark_started : 0.0;
    CcLocalRendererShutdown();
    UnloadRenderTexture(local_target);
    CloseWindow();
    if (render_benchmark) {
        (void)printf("render: frames=%d seconds=%.6f ms/frame=%.3f fps=%.1f\n",
                     render_benchmark_count, render_benchmark_elapsed,
                     render_benchmark_elapsed * 1000.0 /
                         (double)render_benchmark_count,
                     (double)render_benchmark_count /
                         render_benchmark_elapsed);
    } else if (capture_walk_cycle) {
        (void)printf("captured %d walk-cycle frames with prefix %s\n",
                     walk_frame_count, capture_path);
    } else if (capture) {
        (void)printf("captured %s\n", capture_path);
    }
    return 0;
}
