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

typedef struct ActionReelState {
    int32_t stage;
    int32_t stage_frame;
    int32_t captured_frames;
    bool jump_started;
    bool complete;
} ActionReelState;

typedef struct CombatReelState {
    int32_t stage;
    int32_t stage_frame;
    int32_t captured_frames;
    int32_t settled_frames;
    bool action_started;
    bool complete;
    bool failed;
} CombatReelState;

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

static void ResetLocalState(LocalState *local, const CcSim *sim)
{
    local->market_interior = false;
    CcLocalAgentInit(&local->agent,
                     (Vector2){CC_LOCAL_START_X, CC_LOCAL_START_Z}, false);
    if (sim != NULL) local->agent.athletics = sim->player.athletics;
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
    opponent->tunic_color = (Color){126, 55, 61, 255};
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
    course->last_outcome = CcLocalCombatResolveStrike(attacker, defender);
    course->last_attacker_team = attacker->combat.team;
    course->combat_event_seconds = 0.72f;
}

static void CombatReelSetStage(CombatReelState *reel, int32_t stage)
{
    reel->stage = stage;
    reel->stage_frame = 0;
    reel->settled_frames = 0;
    reel->action_started = false;
}

static void PrepareCombatReel(LocalState *local, CombatReelState *reel)
{
    *reel = (CombatReelState){0};
    local->market_interior = false;
    CcLocalCourseInit(&local->course);
    Vector3 center = {45.15f, 0.0f, 30.60f};
    local->course.alarm_active = true;
    local->course.alarm_countdown = 1000.0f;
    local->course.combat_origin = center;

    CcLocalAgentInit(&local->agent,
                     (Vector2){center.x - 1.45f, center.z}, false);
    CcLocalCombatSetTeam(&local->agent, CC_COMBAT_PLAYER);
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        CcLocalAgentSetAthleticLevel(
            &local->agent, (CcAthleticDiscipline)discipline,
            CC_ATHLETIC_MAX_LEVEL);
    }
    local->agent.facing_yaw = 0.5f * PI;
    local->agent.combat.auto_attack_cooldown = 100.0f;

    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgentInit(&local->course.runners[i].agent,
                         (Vector2){88.0f + (float)i, 66.0f}, false);
        local->course.runners[i].agent.combat.defeated = true;
        local->course.runners[i].agent.combat.health = 0.0f;
    }

    CcLocalAgent *opponent = &local->course.raiders[0];
    CcLocalAgentInit(opponent,
                     (Vector2){center.x + 0.73f, center.z}, false);
    opponent->crowned = false;
    opponent->tunic_color = (Color){126, 55, 61, 255};
    opponent->facing_yaw = -0.5f * PI;
    CcLocalCombatSetTeam(opponent, CC_COMBAT_RAIDER);
    CcLocalAgentSetAthleticLevel(opponent, CC_ATHLETIC_MOBILITY, 3);
    CcLocalAgentSetAthleticLevel(opponent, CC_ATHLETIC_POWER, 3);
    local->course.raider_response_stage[0] = 3;
    local->course.raider_response_waypoint_active[0] = false;
    local->course.raider_attack_cooldown[0] = 100.0f;

    CcLocalAgent *reserve = &local->course.raiders[1];
    CcLocalAgentInit(reserve, (Vector2){94.0f, 65.0f}, false);
    reserve->crowned = false;
    CcLocalCombatSetTeam(reserve, CC_COMBAT_RAIDER);
    local->course.raider_attack_cooldown[1] = 100.0f;
    (void)CcLocalCourseSelectPlayerTarget(&local->course, &local->agent, 0);
}

static void UpdateCombatReel(LocalState *local, CombatReelState *reel,
                             char *message, size_t message_capacity)
{
    const float delta_time = 1.0f / 60.0f;
    CcLocalAgent *hero = &local->agent;
    CcLocalAgent *opponent = &local->course.raiders[0];
    reel->stage_frame += 1;
    hero->combat.auto_attack_cooldown = fmaxf(
        hero->combat.auto_attack_cooldown, 90.0f);
    local->course.raider_attack_cooldown[0] = 100.0f;
    local->course.raider_attack_cooldown[1] = 100.0f;

    switch (reel->stage) {
        case 0:
            (void)snprintf(message, message_capacity,
                           "COMBAT REEL / click target, navigate, then hand off to paired weapon spacing");
            CcLocalCourseUpdate(&local->course, hero, NULL, delta_time);
            if (local->course.player_pair.active &&
                local->course.player_pair.range_error == 0.0f &&
                fabsf(local->course.player_pair.radial_velocity) < 0.03f) {
                reel->settled_frames += 1;
            } else {
                reel->settled_frames = 0;
            }
            if (reel->settled_frames >= 24) {
                CombatReelSetStage(reel, 1);
            } else if (reel->stage_frame > 300) {
                (void)fprintf(stderr,
                              "combat reel failed to acquire its target pair\n");
                reel->failed = true;
                reel->complete = true;
            }
            break;
        case 1:
            (void)snprintf(message, message_capacity,
                           "COMBAT REEL / stable guard, planted feet, no positional hunting");
            CcLocalCourseSetPlayerGuarded(&local->course, hero, true);
            CcLocalCourseUpdate(&local->course, hero, NULL, delta_time);
            if (reel->stage_frame >= 72) {
                CcLocalCourseSetPlayerGuarded(&local->course, hero, false);
                CombatReelSetStage(reel, 2);
            }
            break;
        case 2:
            (void)snprintf(message, message_capacity,
                           "COMBAT REEL / basic cut, contact, velocity-aware recovery to neutral");
            if (reel->stage_frame == 1) {
                reel->action_started = CcLocalCourseBeginPlayerStrike(
                    &local->course, hero);
            }
            CcLocalCourseUpdate(&local->course, hero, NULL, delta_time);
            if (reel->action_started && reel->stage_frame > 78 &&
                hero->humanoid.action != CC_HUMANOID_ACTION_STRIKE &&
                !CcHumanoidGaitStrikeRecovering(&hero->humanoid)) {
                CombatReelSetStage(reel, 3);
            } else if (!reel->action_started && reel->stage_frame > 180) {
                (void)fprintf(stderr,
                              "combat reel failed to start its basic cut\n");
                reel->failed = true;
                reel->complete = true;
            }
            break;
        case 3:
            (void)snprintf(message, message_capacity,
                           "COMBAT REEL / SUNDER sweep, posture pressure, clean follow-through");
            if (reel->stage_frame == 1) {
                hero->combat.auto_attack_cooldown = 0.0f;
                hero->combat.skill_cooldown[CC_COMBAT_SKILL_SUNDER] = 0.0f;
                (void)CcLocalCourseUsePlayerSkill(
                    &local->course, hero, CC_COMBAT_SKILL_SUNDER);
            }
            if (!reel->action_started) {
                hero->combat.auto_attack_cooldown = 0.0f;
            }
            CcLocalCourseUpdate(&local->course, hero, NULL, delta_time);
            reel->action_started = reel->action_started ||
                (hero->humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
                 hero->combat.active_skill == CC_COMBAT_SKILL_SUNDER);
            if (reel->action_started) hero->combat.auto_attack_cooldown = 90.0f;
            if (reel->stage_frame > 240 && !reel->action_started) {
                (void)fprintf(stderr,
                              "combat reel failed to start SUNDER\n");
                reel->failed = true;
                reel->complete = true;
            }
            if (reel->action_started && reel->stage_frame > 78 &&
                hero->humanoid.action != CC_HUMANOID_ACTION_STRIKE &&
                !CcHumanoidGaitStrikeRecovering(&hero->humanoid)) {
                CombatReelSetStage(reel, 4);
            }
            break;
        case 4:
            (void)snprintf(message, message_capacity,
                           "COMBAT REEL / CRUSHING BLOW, guard break and physical knockdown");
            if (reel->stage_frame == 1) {
                opponent->combat.health = fminf(opponent->combat.health, 22.0f);
                opponent->combat.posture = fminf(opponent->combat.posture, 8.0f);
                hero->combat.auto_attack_cooldown = 0.0f;
                hero->combat.skill_cooldown[CC_COMBAT_SKILL_CRUSHING_BLOW] =
                    0.0f;
                (void)CcLocalCourseUsePlayerSkill(
                    &local->course, hero, CC_COMBAT_SKILL_CRUSHING_BLOW);
            }
            if (!reel->action_started) {
                hero->combat.auto_attack_cooldown = 0.0f;
            }
            CcLocalCourseUpdate(&local->course, hero, NULL, delta_time);
            reel->action_started = reel->action_started ||
                (hero->humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
                 hero->combat.active_skill == CC_COMBAT_SKILL_CRUSHING_BLOW);
            if (reel->action_started) hero->combat.auto_attack_cooldown = 90.0f;
            if (opponent->combat.defeated && reel->stage_frame > 72) {
                CcLocalCourseClearPlayerTarget(hero);
                CombatReelSetStage(reel, 5);
            } else if (reel->stage_frame > 180) {
                (void)fprintf(stderr,
                              "combat reel failed to finish CRUSHING BLOW\n");
                reel->failed = true;
                reel->complete = true;
            }
            break;
        case 5:
            (void)snprintf(message, message_capacity,
                           "COMBAT REEL / disengage, settle, and return cleanly to neutral");
            CcLocalCourseClearPlayerTarget(hero);
            CcLocalCourseUpdate(&local->course, hero, NULL, delta_time);
            if (reel->stage_frame >= 108) reel->complete = true;
            break;
        default:
            reel->complete = true;
            break;
    }
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
                           "HEROIC CONTACT REEL / approach, hand plant, pull and top-out");
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
                           "HEROIC CONTACT REEL / controlled edge turn and down-climb");
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
                           "HEROIC CONTACT REEL / accelerated run-up and live physics jump");
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
                           "HEROIC CONTACT REEL / terrain read, buoyancy and recovery to stride");
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
                           "HEROIC CONTACT REEL / guard contact, recoil, strike and knockdown");
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
    DrawText(place != NULL ? place->name : "THE ROAD", 26, 18, 27, INK);
    DrawText(local->market_interior ? "MARKET HOUSE / PROCEDURAL FOOT CONTACTS" :
             "STREET LEVEL / BIOMECHANICAL MOVEMENT DOJO", 28, 51, 11, TEAL);
    DrawText(TextFormat("DAY %04d", sim->current_day), 790, 24, 15, CC_GOLD);
    DrawText(TextFormat("CROWNS %03" PRId64, sim->player.coins), 895, 24, 15, CC_GOLD);
    DrawText(TextFormat("CARGO %02d/%02d", CcPlayerCargoUsed(&sim->player),
                        sim->player.cargo_capacity), 1062, 24, 15, TEAL);
    DrawText("M  MAP CASE", 1121, 52, 10, MUTED);
}

static void DrawAthleticProgress(const CcLocalAgent *agent,
                                 CcAthleticDiscipline discipline, int y)
{
    float progress = CcLocalAgentAthleticProgress(agent, discipline);
    int32_t level = agent->athletics.level[discipline];
    DrawText(TextFormat("%-8s L%d", CcAthleticDisciplineName(discipline), level),
             966, y, 8, MUTED);
    DrawRectangle(1047, y + 1, 143, 6, (Color){34, 45, 48, 255});
    DrawRectangle(1047, y + 1, (int)(143.0f * progress), 6, CC_GOLD);
    DrawText(level >= CC_ATHLETIC_MAX_LEVEL ? "MAX" :
             TextFormat("%d%%", (int)(progress * 100.0f)),
             1198, y, 8, level >= CC_ATHLETIC_MAX_LEVEL ? CC_GOLD : INK);
}

static void DrawLocalPanel(const CcSim *sim, const LocalState *local)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    DrawPanel((Rectangle){946.0f, 82.0f, 314.0f, 576.0f}, PANEL);
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
        DrawText(TextFormat("HEROIC TIER %d / PHYSICAL DEVELOPMENT",
                            CcLocalAgentHeroicTier(&local->agent)),
                 966, 619, 8, CC_GOLD);
        DrawAthleticProgress(&local->agent, CC_ATHLETIC_MOBILITY, 629);
        DrawAthleticProgress(&local->agent, CC_ATHLETIC_GRIP, 639);
        DrawAthleticProgress(&local->agent, CC_ATHLETIC_POWER, 649);
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
    if (local->agent.combat.defeated) {
        return "DOWNED   the crownless fighter rallies automatically";
    }
    if (!local->market_interior && local->course.alarm_active &&
        local->agent.combat.focus_valid) {
        return "TARGET LOCKED   closing distance and auto-attacking   1-3 use skills";
    }
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

static void DrawCombatSkillCard(const CcLocalAgent *agent,
                                CcCombatSkill skill, int x, int key)
{
    float cooldown = CcLocalCombatSkillCooldown(agent, skill);
    bool queued = agent->combat.queued_skill == (int32_t)skill;
    Color border = queued ? CC_GOLD : cooldown > 0.0f ? MUTED : TEAL;
    DrawRectangleRounded((Rectangle){(float)x, 701.0f, 192.0f, 27.0f},
                         0.18f, 4, (Color){19, 32, 39, 255});
    DrawRectangleRoundedLinesEx((Rectangle){(float)x, 701.0f, 192.0f, 27.0f},
                                0.18f, 4, 1.0f, Fade(border, 0.72f));
    DrawText(TextFormat("%d", key), x + 8, 708, 11, CC_GOLD);
    DrawText(CcLocalCombatSkillName(skill), x + 27, 707, 9, INK);
    DrawText(queued ? "QUEUED" : cooldown > 0.0f ?
             TextFormat("%.1fs", cooldown) : "READY",
             x + 145, 707, 9, border);
}

static void DrawLocalFooter(const CcSim *sim, const LocalState *local)
{
    DrawPanel((Rectangle){20.0f, 664.0f, 1240.0f, 76.0f}, PANEL);
    if (!local->market_interior && local->course.alarm_active) {
        int32_t target = local->agent.combat.target_index;
        if (target >= 0 && target < CC_LOCAL_RAIDER_COUNT &&
            !local->course.raiders[target].combat.defeated) {
            DrawText(TextFormat("TARGET  RAIDER %d   %d HP   /   AUTO ATTACK ENGAGED",
                                target + 1,
                                (int32_t)lroundf(
                                    local->course.raiders[target].combat.health)),
                     38, 679, 12, DANGER);
        } else {
            DrawText("CLICK A RAIDER TO TARGET AND ENGAGE",
                     38, 679, 12, CC_GOLD);
        }
        DrawCombatSkillCard(&local->agent,
                            CC_COMBAT_SKILL_CRUSHING_BLOW, 38, 1);
        DrawCombatSkillCard(&local->agent,
                            CC_COMBAT_SKILL_SUNDER, 240, 2);
        DrawCombatSkillCard(&local->agent,
                            CC_COMBAT_SKILL_SECOND_WIND, 442, 3);
        DrawText("click ground disengages   SPACE manual strike   X guard",
                 780, 709, 9, MUTED);
        return;
    }
    DrawText(LocalPrompt(sim, local), 38, 681, 13, CC_GOLD);
    DrawText("J jump   G alarm   Q situations   TAB ledger",
             38, 708, 10, MUTED);
    DrawText("M map case at carriage   F5 save   F9 load   N new world",
             804, 693, 10, MUTED);
}


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
    sim->player.athletics = local->agent.athletics;
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
            ResetLocalState(local, sim);
            *view = VIEW_LOCAL;
        }
    }
    if (IsKeyPressed(KEY_N)) {
        CcSimInit(sim, sim->world_seed + UINT32_C(0x9e3779b9));
        *selected = FirstVisibleMapIndex(sim);
        ResetLocalState(local, sim);
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
        if (IsKeyPressed(KEY_J) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED) {
            bool jumped = CcLocalAgentJump(&local->agent);
            (void)snprintf(
                message, message_capacity, "%s",
                jumped ? "You drive through both feet into a physical jump; momentum owns the landing." :
                          "No jump: the body needs grounded, unbroken support.");
        }
        if (IsKeyPressed(KEY_X) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED) {
            bool guarded = local->agent.humanoid.action !=
                           CC_HUMANOID_ACTION_GUARD;
            CcLocalCourseSetPlayerGuarded(&local->course, &local->agent,
                                          guarded);
            (void)snprintf(
                message, message_capacity, "%s",
                guarded ? "You settle behind your hands and hips; X releases the guard." :
                          "You release the guarded stance.");
        }
        if (IsKeyPressed(KEY_SPACE) &&
            local->agent.morphology == CC_MORPHOLOGY_BIPED) {
            bool struck = CcLocalCourseBeginPlayerStrike(
                &local->course, &local->agent);
            (void)snprintf(
                message, message_capacity, "%s",
                struck && local->agent.combat.target_index >= 0 ?
                         "You commit toward the nearest raider; range and guard decide the impact." :
                struck ? "You strike into open space; only contact inside the impact window will land." :
                         "No strike: the body is already committed to another action.");
        }
        if (!local->market_interior && local->course.alarm_active) {
            for (int32_t skill = 0; skill < CC_COMBAT_SKILL_COUNT; ++skill) {
                if (!IsKeyPressed(KEY_ONE + skill)) continue;
                CcCombatSkill combat_skill = (CcCombatSkill)skill;
                bool used = CcLocalCourseUsePlayerSkill(
                    &local->course, &local->agent, combat_skill);
                float cooldown = CcLocalCombatSkillCooldown(
                    &local->agent, combat_skill);
                (void)snprintf(
                    message, message_capacity, "%s",
                    used && combat_skill == CC_COMBAT_SKILL_SECOND_WIND ?
                        "Second Wind restores health and posture." :
                    used ? TextFormat("%s queued for your target.",
                                      CcLocalCombatSkillName(combat_skill)) :
                    cooldown > 0.0f ? TextFormat("%s is cooling down: %.1fs.",
                                                 CcLocalCombatSkillName(combat_skill),
                                                 cooldown) :
                    combat_skill == CC_COMBAT_SKILL_SECOND_WIND ?
                        "Second Wind is unnecessary at full health and posture." :
                        "Select a living raider before using that skill.");
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            int32_t combat_target = local->market_interior ? -1 :
                CcLocalCoursePickPlayerTarget(
                    &local->course, &local->agent, mouse, local_target,
                    local_bounds);
            if (combat_target >= 0) {
                (void)snprintf(
                    message, message_capacity,
                    "Raider %d targeted; you close distance and fight automatically.",
                    combat_target + 1);
            } else {
                if (!local->market_interior && local->course.alarm_active) {
                    CcLocalCourseClearPlayerTarget(&local->agent);
                }
                if (CcLocalAgentPickTarget(&local->agent, mouse, local_target,
                                           local_bounds,
                                           local->market_interior)) {
                    (void)snprintf(
                        message, message_capacity,
                        "Moving to %.2f,%.2f; combat target disengaged.",
                        local->agent.target_point.x,
                        local->agent.target_point.z);
                }
            }
        }
        float local_delta_time = GetFrameTime();
        if (local->market_interior) {
            CcLocalAgentUpdate(&local->agent, local_delta_time, true);
        } else {
            if (IsKeyPressed(KEY_G)) {
                if (!local->course.alarm_active) {
                    CcLocalCourseRaiseAlarmNear(&local->course,
                                                &local->agent);
                    (void)snprintf(message, message_capacity,
                                   "The village bell sounds; the trainees break course and form a defense line.");
                } else {
                    (void)snprintf(message, message_capacity,
                                   "The village alarm is already sounding.");
                }
            }
            CcLocalCourseUpdate(&local->course, &local->agent, sim,
                                local_delta_time);
        }
        sim->player.athletics = local->agent.athletics;
        Vector2 position = LocalPosition(local);

        if (IsKeyPressed(KEY_F)) {
            if (local->market_interior &&
                GridDistance(position, INTERIOR_EXIT) < 1.25f) {
                local->market_interior = false;
                RepositionHero(local,
                               (Vector2){CC_LOCAL_MARKET_X,
                                         CC_LOCAL_MARKET_Z + 1.10f}, false);
                (void)snprintf(message, message_capacity,
                               "The market door closes behind you; the street keeps moving.");
            } else if (!local->market_interior &&
                       GridDistance(position, LOCAL_MARKET) < 1.30f) {
                local->market_interior = true;
                RepositionHero(local, (Vector2){2.05f, 5.35f}, true);
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
            ResetLocalState(local, sim);
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
    bool capture_jump = argc >= 2 && strcmp(argv[1], "--capture-jump") == 0;
    bool capture_action_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-action-reel") == 0;
    bool capture_combat_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-combat-reel") == 0;
    bool capture = argc >= 2 &&
                   (strcmp(argv[1], "--capture") == 0 || capture_board ||
                    capture_interior || capture_navigation || capture_limbs ||
                    capture_walk_cycle || capture_defense ||
                    capture_downclimb || capture_map_case || capture_dojo ||
                    capture_jump || capture_action_reel ||
                    capture_combat_reel);
    const char *capture_path = argc >= 3 ? argv[2] : "architecture-proof.png";
    char save_path[640];
    CampaignSavePath(save_path, sizeof(save_path));

    if (render_benchmark || capture_action_reel || capture_combat_reel) {
        SetTraceLogLevel(LOG_WARNING);
    }
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE |
                   (capture ? FLAG_WINDOW_HIDDEN : 0U));
    InitWindow(1280, 760, "Crownless Carriage — living world spine");
    if (!IsWindowReady()) {
        (void)fprintf(stderr,
                      "Crownless Carriage could not connect to the desktop window server.\n");
        return 1;
    }
    SetWindowMinSize(1080, 680);
    SetTargetFPS(render_benchmark || capture_action_reel ||
                 capture_combat_reel ? 0 : 60);
    RenderTexture2D local_target = LoadRenderTexture(914, 570);
    SetTextureFilter(local_target.texture, TEXTURE_FILTER_BILINEAR);
    CcLocalRendererInit();
    CcLocalRendererSetDiagnosticOverlay(capture_limbs);

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
    ActionReelState action_reel = {0};
    CombatReelState combat_reel = {0};
    ResetLocalState(&local, &sim);
    if (capture && !capture_interior && !capture_walk_cycle &&
        !capture_jump && !capture_defense && !capture_downclimb &&
        !capture_navigation && !capture_limbs && !capture_dojo &&
        !capture_action_reel && !capture_combat_reel) {
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
    if (capture_combat_reel) PrepareCombatReel(&local, &combat_reel);
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
        CcLocalCourseRaiseAlarmNear(&local.course, &local.agent);
        Vector3 duel_center = local.course.combat_origin;
        CcLocalAgentInit(&local.agent,
                         (Vector2){duel_center.x - 0.73f, duel_center.z},
                         false);
        CcLocalCombatSetTeam(&local.agent, CC_COMBAT_PLAYER);
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            local.course.runners[i].agent.combat.defeated = true;
            local.course.runners[i].agent.combat.health = 0.0f;
        }
        CcLocalAgentInit(&local.course.raiders[0],
                         (Vector2){duel_center.x + 0.73f, duel_center.z},
                         false);
        CcLocalCombatSetTeam(&local.course.raiders[0], CC_COMBAT_RAIDER);
        local.course.raider_response_stage[0] = 3;
        local.course.raider_response_waypoint_active[0] = false;
        local.course.raiders[1].combat.defeated = true;
        local.course.raiders[1].combat.health = 0.0f;
        (void)CcLocalCourseSelectPlayerTarget(&local.course,
                                               &local.agent, 0);
        (void)CcLocalCourseUsePlayerSkill(
            &local.course, &local.agent, CC_COMBAT_SKILL_CRUSHING_BLOW);
        int32_t fighting_frames = 0;
        bool captured_skill_impact = false;
        for (int32_t frame = 0; frame < 180 && !captured_skill_impact;
             ++frame) {
            CcLocalCourseUpdate(&local.course, &local.agent, &sim,
                                1.0f / 60.0f);
            fighting_frames = local.course.alarm_active &&
                              !local.course.raiders_retreating &&
                              local.course.raider_resolve < 100 ?
                              fighting_frames + 1 : 0;
            captured_skill_impact =
                local.agent.humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
                local.agent.combat.active_skill ==
                    CC_COMBAT_SKILL_CRUSHING_BLOW &&
                local.agent.humanoid.action_time >= 0.54f;
        }
        (void)printf("defense capture: alarm %d retreat %d resolve %d wins %d fight frames %d skill impact %d pair %d range %.3f origin %.2f,%.2f player %.2f,%.2f raider %.2f,%.2f\n",
                     local.course.alarm_active,
                     local.course.raiders_retreating,
                     local.course.raider_resolve,
                     local.course.defenses_completed, fighting_frames,
                     captured_skill_impact, local.course.player_pair.active,
                     local.course.player_pair.distance,
                     local.course.combat_origin.x,
                     local.course.combat_origin.z,
                     local.agent.position.x,
                     local.agent.position.z,
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
        } else if (capture_combat_reel) {
            for (int32_t step = 0; step < 2 && !combat_reel.complete; ++step) {
                UpdateCombatReel(&local, &combat_reel, message,
                                 sizeof(message));
            }
        } else if (capture_action_reel) {
            for (int32_t step = 0; step < 4 && !action_reel.complete; ++step) {
                UpdateActionReel(&local, &action_reel, message,
                                 sizeof(message));
            }
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
        } else if (capture_combat_reel) {
            capture_frames += 1;
            if (capture_frames <= 2) continue;
            char frame_path[768];
            (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                           capture_path, combat_reel.captured_frames);
            TakeScreenshot(frame_path);
            combat_reel.captured_frames += 1;
            if (combat_reel.complete) break;
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
    } else if (capture_combat_reel) {
        (void)printf("captured %d combat-reel frames with prefix %s\n",
                     combat_reel.captured_frames, capture_path);
    } else if (capture_action_reel) {
        (void)printf("captured %d heroic action-reel frames with prefix %s\n",
                     action_reel.captured_frames, capture_path);
    } else if (capture) {
        (void)printf("captured %s\n", capture_path);
    }
    if (capture_combat_reel && combat_reel.failed) return 1;
    return 0;
}
