#ifndef CROWNLESS_LOCAL3D_H
#define CROWNLESS_LOCAL3D_H

#include "client/cc_npc_appearance.h"
#include "locomotion/cc_limb.h"
#include "locomotion/cc_humanoid.h"
#include "sim/cc_sim.h"

#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_LOCAL_COURSE_RUNNER_COUNT 3
#define CC_LOCAL_RAIDER_COUNT 2
#define CC_LOCAL_TRAVELLER_COUNT 4
#define CC_ATHLETIC_MAX_LEVEL 5

/* Exterior coordinates are metres. These landmarks are shared by input,
   collision, rendering, and tests so the continuous world cannot drift apart. */
#define CC_LOCAL_WORLD_WIDTH 96.0f
#define CC_LOCAL_WORLD_DEPTH 72.0f
#define CC_LOCAL_NAVIGATION_POINT_CAPACITY 48
#define CC_LOCAL_START_X 40.5f
#define CC_LOCAL_START_Z 30.0f
#define CC_LOCAL_MARKET_X 50.0f
#define CC_LOCAL_MARKET_Z 26.75f
#define CC_LOCAL_CARRIAGE_X 37.40f
#define CC_LOCAL_CARRIAGE_Z 51.20f
#define CC_LOCAL_NOTICE_X 41.0f
#define CC_LOCAL_NOTICE_Z 27.80f
#define CC_LOCAL_DUNGEON_X 29.0f
#define CC_LOCAL_DUNGEON_Z 51.80f
#define CC_LOCAL_DRAGON_CAVE_X 19.0f
#define CC_LOCAL_DRAGON_CAVE_Z 52.0f
#define CC_LOCAL_ROAD_START_X 46.20f
#define CC_LOCAL_ROAD_START_Z 40.00f
#define CC_LOCAL_ROAD_PARLEY_X 49.70f
#define CC_LOCAL_ROAD_PARLEY_Z 40.00f
#define CC_LOCAL_SITE_CARRIAGE_X 24.0f
#define CC_LOCAL_SITE_CARRIAGE_Z 40.0f
#define CC_LOCAL_SITE_ENTRANCE_X 70.0f
#define CC_LOCAL_SITE_ENTRANCE_Z 40.0f
#define CC_LOCAL_COMBAT_MAX_HEALTH 100.0f
#define CC_LOCAL_COMBAT_MAX_POSTURE 100.0f
#define CC_LOCAL_CAPE_POINT_COUNT 5

typedef enum CcTraversalMode {
    CC_TRAVERSAL_IDLE,
    CC_TRAVERSAL_WALK,
    CC_TRAVERSAL_CLIMB,
    CC_TRAVERSAL_DESCEND,
    CC_TRAVERSAL_SWIM,
    CC_TRAVERSAL_DROP,
    CC_TRAVERSAL_RAGDOLL,
    CC_TRAVERSAL_GET_UP,
    CC_TRAVERSAL_JUMP,
    CC_TRAVERSAL_VAULT
} CcTraversalMode;

typedef enum CcLocalSceneKind {
    CC_LOCAL_SCENE_STREET = 0,
    CC_LOCAL_SCENE_MARKET,
    CC_LOCAL_SCENE_ROAD
} CcLocalSceneKind;

typedef enum CcLocalSiteKind {
    CC_LOCAL_SITE_NONE = 0,
    CC_LOCAL_SITE_DUNGEON,
    CC_LOCAL_SITE_GOBLIN_CAVE,
    CC_LOCAL_SITE_DRAGON_CAVE
} CcLocalSiteKind;

typedef enum CcLocalConvoyPhase {
    CC_LOCAL_CONVOY_PARKED = 0,
    CC_LOCAL_CONVOY_DEPARTING,
    CC_LOCAL_CONVOY_GATE,
    CC_LOCAL_CONVOY_ROAD,
    CC_LOCAL_CONVOY_ARRIVING
} CcLocalConvoyPhase;

/* The strategic simulation owns the durable route and progress. This small
   presentation state keeps the same carriage under the camera while it
   leaves a stable, crosses a gate, travels, and enters the next town. */
typedef struct CcLocalConvoyState {
    CcLocalConvoyPhase phase;
    Vector3 town_position;
    float town_heading_yaw;
    float phase_progress;
    float pace;
    float lateral_offset;
    float runtime_tick_accumulator;
} CcLocalConvoyState;

typedef enum CcLocalAtmospherePreset {
    CC_LOCAL_ATMOSPHERE_CLEAR_DAY = 0,
    CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST,
    CC_LOCAL_ATMOSPHERE_AMBER_DUSK,
    CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT,
    CC_LOCAL_ATMOSPHERE_DRAGON_OMEN,
    CC_LOCAL_ATMOSPHERE_COUNT
} CcLocalAtmospherePreset;

/* A day is one authored lighting beat, not a continuously rotating clock.
   Clear weather remains most common; travel and explicit day advances are
   the moments when the world may move to another mood. */
static inline CcLocalAtmospherePreset CcLocalAtmosphereForDay(int32_t day)
{
    int32_t beat = day % 8;
    if (beat < 0) beat += 8;
    if (beat == 2 || beat == 6) {
        return CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST;
    }
    if (beat == 4) return CC_LOCAL_ATMOSPHERE_AMBER_DUSK;
    if (beat == 7) return CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT;
    return CC_LOCAL_ATMOSPHERE_CLEAR_DAY;
}

typedef enum CcAthleticDiscipline {
    CC_ATHLETIC_MOBILITY,
    CC_ATHLETIC_GRIP,
    CC_ATHLETIC_POWER,
    CC_ATHLETIC_DISCIPLINE_COUNT
} CcAthleticDiscipline;

typedef struct CcAthleticProfile {
    float experience[CC_ATHLETIC_DISCIPLINE_COUNT];
    float travel_training_distance;
    int32_t level[CC_ATHLETIC_DISCIPLINE_COUNT];
} CcAthleticProfile;

typedef enum CcCombatTeam {
    CC_COMBAT_NEUTRAL,
    CC_COMBAT_PLAYER,
    CC_COMBAT_GUARD,
    CC_COMBAT_RAIDER
} CcCombatTeam;

typedef enum CcCombatOutcome {
    CC_COMBAT_OUTCOME_NONE,
    CC_COMBAT_OUTCOME_MISS,
    CC_COMBAT_OUTCOME_HIT,
    CC_COMBAT_OUTCOME_BLOCKED,
    CC_COMBAT_OUTCOME_GUARD_BROKEN,
    CC_COMBAT_OUTCOME_DEFEATED
} CcCombatOutcome;

typedef enum CcCombatSkill {
    CC_COMBAT_SKILL_CRUSHING_BLOW,
    CC_COMBAT_SKILL_SUNDER,
    CC_COMBAT_SKILL_SECOND_WIND,
    CC_COMBAT_SKILL_COUNT
} CcCombatSkill;

typedef enum CcLifeState {
    CC_LIFE_ALIVE,
    CC_LIFE_KNOCKED_DOWN,
    CC_LIFE_DEAD,
    CC_LIFE_RESPAWNING
} CcLifeState;

typedef enum CcWeaponMode {
    CC_WEAPON_NONE,
    CC_WEAPON_HELD,
    CC_WEAPON_RAGDOLL_ATTACHED
} CcWeaponMode;

typedef struct CcCombatState {
    Vector3 focus_point;
    Vector3 knockback_velocity;
    Vector3 impact_point;
    Vector3 impact_direction;
    float health;
    float posture;
    float stagger_seconds;
    float hit_flash_seconds;
    float hitstop_seconds;
    float respawn_seconds;
    float impact_speed;
    float auto_attack_cooldown;
    float skill_cooldown[CC_COMBAT_SKILL_COUNT];
    float strike_damage_scale;
    float strike_posture_scale;
    float strike_knockback_scale;
    int32_t target_index;
    int32_t queued_skill;
    int32_t active_skill;
    CcCombatTeam team;
    CcLifeState life_state;
    CcWeaponMode weapon_mode;
    bool focus_valid;
    bool impact_valid;
    bool strike_resolved;
} CcCombatState;

typedef struct CcLocalCapeState {
    Vector3 point[CC_LOCAL_CAPE_POINT_COUNT];
    Vector3 previous[CC_LOCAL_CAPE_POINT_COUNT];
    Vector3 anchor;
    bool initialized;
} CcLocalCapeState;

typedef struct CcSteppedPoseState {
    CcHumanoidPose from_local;
    CcHumanoidPose target_local;
    int32_t locomotion_bin;
    bool initialized;
} CcSteppedPoseState;

typedef struct CcLocalAgent {
    Vector3 position;
    Vector3 velocity;
    Vector3 separation_velocity;
    Vector3 target_point;
    Vector3 command_point;
    Vector3 command_origin;
    Vector3 navigation_point[CC_LOCAL_NAVIGATION_POINT_CAPACITY];
    Vector3 climb_start;
    Vector3 climb_end;
    Vector3 climb_face;
    Vector3 climb_normal;
    Vector3 climb_hand_left;
    Vector3 climb_hand_right;
    Vector3 climb_foot_left;
    Vector3 climb_foot_right;
    float facing_yaw;
    float ragdoll_visual_blend;
    float climb_progress;
    float climb_settle;
    float climb_duration;
    float climb_start_yaw;
    float climb_end_yaw;
    CcTraversalMode traversal;
    CcHumanoidSupportState support_state;
    CcLocalSceneKind scene;
    bool grounded;
    bool climbing;
    bool climbing_down;
    bool vaulting;
    bool swimming;
    bool allow_downclimb;
    bool exact_target_valid;
    float radius;
    float immersion;
    CcMorphologyPreset morphology;
    CcLimbRig limb_rig;
    CcHumanoidGait humanoid;
    CcHumanoidPose render_pose;
    CcSteppedPoseState stepped_pose;
    CcLocalCapeState cape;
    CcLocalCapeState previous_cape;
    CcLocalCapeState render_cape;
    float simulation_accumulator;
    float movement_stall_seconds;
    int32_t navigation_repath_count;
    int32_t navigation_point_count;
    int32_t navigation_point_index;
    int32_t navigation_destination_room;
    bool render_pose_valid;
    bool humanoid_needs_reset;
    bool target_valid;
    bool command_point_valid;
    bool crowned;
    bool jump_training_pending;
    bool climb_training_pending;
    bool navigation_active;
    bool navigation_world_exit;
    bool world_exit_requested;
    Color tunic_color;
    CcNpcAppearance appearance;
    CcAthleticProfile athletics;
    CcCombatState combat;
} CcLocalAgent;

typedef enum CcGuardDuty {
    CC_GUARD_TRAINING,
    CC_GUARD_RESPONDING,
    CC_GUARD_ENGAGED,
    CC_GUARD_RETURNING
} CcGuardDuty;

typedef enum CcLocalRaiderRole {
    CC_LOCAL_RAIDER_CAPTAIN,
    CC_LOCAL_RAIDER_FORAGER
} CcLocalRaiderRole;

typedef struct CcLocalCourseRunner {
    CcLocalAgent agent;
    int32_t next_waypoint;
    int32_t response_stage;
    float pause_seconds;
    float attack_cooldown;
    Color marker_color;
    CcGuardDuty duty;
    bool response_waypoint_active;
} CcLocalCourseRunner;

typedef struct CcLocalTraveller {
    CcLocalAgent agent;
    Vector3 entry;
    Vector3 exit;
    float respawn_delay;
    bool active;
} CcLocalTraveller;

typedef struct CcLocalCourse {
    CcLocalCourseRunner runners[CC_LOCAL_COURSE_RUNNER_COUNT];
    CcLocalTraveller travellers[CC_LOCAL_TRAVELLER_COUNT];
    Vector3 guard_entry[CC_LOCAL_COURSE_RUNNER_COUNT];
    CcLocalAgent raiders[CC_LOCAL_RAIDER_COUNT];
    CcLocalRaiderRole raider_roles[CC_LOCAL_RAIDER_COUNT];
    char raider_names[CC_LOCAL_RAIDER_COUNT][CC_NAME_CAPACITY];
    char raider_company_name[CC_NAME_CAPACITY];
    CcId raider_company_id;
    CcLocalAgent situation_witness;
    CcId situation_witness_id;
    CcId situation_witness_character_id;
    CcCharacterActivity situation_witness_activity;
    float situation_witness_activity_seconds;
    int32_t situation_witness_activity_stage;
    Vector3 raider_entry[CC_LOCAL_RAIDER_COUNT];
    Vector3 combat_origin;
    float alarm_countdown;
    float engagement_time;
    float raider_attack_cooldown[CC_LOCAL_RAIDER_COUNT];
    int32_t raider_response_stage[CC_LOCAL_RAIDER_COUNT];
    int32_t raider_initial_resolve;
    int32_t raider_resolve;
    int32_t defenses_completed;
    CcCombatOutcome last_outcome;
    CcCombatTeam last_attacker_team;
    CcCombatTeam last_defender_team;
    float last_health_damage;
    float last_posture_damage;
    float combat_event_seconds;
    double world_simulation_accumulator;
    bool alarm_active;
    bool raiders_retreating;
    bool combat_origin_valid;
    bool raider_response_waypoint_active[CC_LOCAL_RAIDER_COUNT];
    bool situation_witness_active;
    bool road_encounter;
    CcLocalSceneKind scene;
} CcLocalCourse;

typedef struct CcLocalRendererStats {
    float frame_milliseconds;
    float smoothed_frame_milliseconds;
    float p95_frame_milliseconds;
    float p99_frame_milliseconds;
    float maximum_frame_milliseconds;
    int32_t hitch_count;
    int32_t biomechanical_characters;
    int32_t high_detail_characters;
    int32_t low_detail_characters;
    int32_t skin_updates;
    int32_t skinned_meshes;
    int32_t hero_skin_updates;
    int32_t hero_skinned_meshes;
    int32_t npc_skin_updates;
    int32_t npc_skinned_meshes;
    int32_t creature_skin_updates;
    int32_t creature_skinned_meshes;
} CcLocalRendererStats;

/* Keep the player skin comfortably below raylib's CPU skinning/upload cliff.
   The authored Blender asset may contain many editable pieces, but the runtime
   export must consolidate them into no more than this many material
   primitives. */
#define CC_LOCAL_HERO_RUNTIME_MESH_BUDGET 32

/* The exterior land is deterministic for a world seed. Rendering, movement,
   picking, roads, and building foundations all use these same samples. */
void CcLocalTerrainSetSeed(uint32_t seed);
void CcLocalBindPlace(const CcSim *sim);
float CcLocalTerrainHeightAt(float x, float z);
Vector3 CcLocalTerrainNormalAt(float x, float z);

void CcLocalAgentInit(CcLocalAgent *agent, Vector2 position, bool market_interior);
void CcLocalAgentSetNpcAppearance(CcLocalAgent *agent, uint32_t seed,
                                  CcNpcRole role, Color accent);
void CcLocalAgentUpdate(CcLocalAgent *agent, float delta_time, bool market_interior);
bool CcLocalAgentSetExactTarget(CcLocalAgent *agent, Vector3 target,
                                bool market_interior);
bool CcLocalAgentSetStreetTarget(CcLocalAgent *agent, Vector3 target);
bool CcLocalAgentPickTarget(CcLocalAgent *agent, Vector2 screen_point,
                            RenderTexture2D target, Rectangle destination,
                            bool market_interior);
int32_t CcLocalAgentStreetPortalCount(const CcLocalAgent *agent);
const char *CcLocalAgentStreetPortalName(const CcLocalAgent *agent,
                                         int32_t portal_index);
bool CcLocalAgentFollowStreetPortal(CcLocalAgent *agent,
                                    int32_t portal_index);
const char *CcLocalAgentNavigationName(const CcLocalAgent *agent);
bool CcLocalAgentConsumeWorldExit(CcLocalAgent *agent);
Vector2 CcLocalAgentPosition(const CcLocalAgent *agent);
const char *CcLocalTraversalName(CcTraversalMode mode);
void CcLocalAgentSetMorphology(CcLocalAgent *agent, CcMorphologyPreset preset,
                               bool market_interior);
void CcLocalAgentSetScene(CcLocalAgent *agent, CcLocalSceneKind scene);
void CcLocalAgentCycleMorphology(CcLocalAgent *agent, bool market_interior);
const char *CcLocalAgentMorphologyName(const CcLocalAgent *agent);
void CcLocalCombatSetTeam(CcLocalAgent *agent, CcCombatTeam team);
void CcLocalCombatSetFocus(CcLocalAgent *agent,
                           const CcLocalAgent *target);
void CcLocalCombatClearFocus(CcLocalAgent *agent);
void CcLocalCombatSetGuarded(CcLocalAgent *agent,
                             const CcLocalAgent *target, bool guarded);
bool CcLocalCombatBeginStrike(CcLocalAgent *agent,
                              const CcLocalAgent *target);
bool CcLocalAgentJump(CcLocalAgent *agent);
void CcLocalAgentTrainAthleticism(CcLocalAgent *agent,
                                  CcAthleticDiscipline discipline,
                                  float experience);
void CcLocalAgentSetAthleticLevel(CcLocalAgent *agent,
                                  CcAthleticDiscipline discipline,
                                  int32_t level);
int32_t CcLocalAgentHeroicTier(const CcLocalAgent *agent);
float CcLocalAgentAthleticProgress(const CcLocalAgent *agent,
                                   CcAthleticDiscipline discipline);
const char *CcAthleticDisciplineName(CcAthleticDiscipline discipline);
CcCombatOutcome CcLocalCombatResolveStrike(CcLocalAgent *attacker,
                                           CcLocalAgent *defender);
const char *CcLocalCombatOutcomeName(CcCombatOutcome outcome);
const char *CcLocalCombatTeamName(CcCombatTeam team);
void CcLocalCourseInit(CcLocalCourse *course);
void CcLocalCourseUpdate(CcLocalCourse *course, CcLocalAgent *player,
                         const CcSim *sim, float delta_time);
int32_t CcLocalWorldUpdate(CcLocalCourse *course, CcLocalAgent *player,
                           const CcSim *sim, float delta_time,
                           bool market_interior, bool advance_course);
void CcLocalCourseRaiseAlarm(CcLocalCourse *course);
void CcLocalCourseRaiseAlarmNear(CcLocalCourse *course,
                                 const CcLocalAgent *player);
void CcLocalCourseStageRoadEncounter(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     bool hostile);
void CcLocalCourseBindRaiderCompany(CcLocalCourse *course,
                                    const CcSim *sim);
const char *CcLocalRaiderRoleName(CcLocalRaiderRole role);
bool CcLocalCourseBeginPlayerStrike(CcLocalCourse *course,
                                    CcLocalAgent *player);
bool CcLocalCourseSetPlayerGuarded(CcLocalCourse *course,
                                   CcLocalAgent *player, bool guarded);
bool CcLocalCourseHasNearbyHostile(const CcLocalCourse *course,
                                   const CcLocalAgent *player);
bool CcLocalCourseCanPlayerEngage(const CcLocalCourse *course,
                                  const CcLocalAgent *player,
                                  int32_t target_index);
bool CcLocalCourseSelectPlayerTarget(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     int32_t target_index);
int32_t CcLocalCoursePickPlayerTarget(CcLocalCourse *course,
                                      CcLocalAgent *player,
                                      Vector2 screen_point,
                                      RenderTexture2D target,
                                      Rectangle destination);
void CcLocalCourseClearPlayerTarget(CcLocalAgent *player);
bool CcLocalCourseUsePlayerSkill(CcLocalCourse *course,
                                 CcLocalAgent *player,
                                 CcCombatSkill skill);
const char *CcLocalCombatSkillName(CcCombatSkill skill);
float CcLocalCombatSkillCooldown(const CcLocalAgent *player,
                                 CcCombatSkill skill);
float CcLocalCombatSkillDuration(CcCombatSkill skill);

void CcLocalRendererInit(void);
void CcLocalRendererSetScreenFirstHero(bool enabled);
void CcLocalRendererBeginFrame(float delta_time);
void CcLocalRendererResetPerformanceMetrics(void);
CcLocalRendererStats CcLocalRendererGetStats(void);
void CcLocalRendererSetDiagnosticOverlay(bool enabled);
void CcLocalRendererSetAtmosphere(CcLocalAtmospherePreset preset,
                                  float transition_seconds);
void CcLocalRendererUpdateAtmosphere(float delta_time);
const char *CcLocalAtmosphereName(CcLocalAtmospherePreset preset);
void CcLocalRendererShutdown(void);
void CcLocalDrawNpcPortrait3D(const CcNpcAppearance *appearance,
                              Rectangle bounds,
                              CcNpcPortraitExpression expression);
void CcLocalDrawAgentPortrait3D(const CcLocalAgent *agent,
                                Rectangle bounds);
void CcLocalDrawNpcReview3D(int32_t view, float clock,
                            RenderTexture2D target, Rectangle destination);
void CcLocalDrawStreet3D(const CcSim *sim, const CcLocalAgent *agent,
                         const CcLocalCourse *course,
                         const CcLocalConvoyState *convoy, float clock,
                         RenderTexture2D target, Rectangle destination);
void CcLocalDrawRoad3D(const CcSim *sim, const CcLocalAgent *agent,
                       const CcLocalCourse *course, bool travelling,
                       bool parley, const CcLocalConvoyState *convoy,
                       float clock, RenderTexture2D target,
                       Rectangle destination);
float CcLocalRoadCarriageX(int32_t progress_milli);
uint32_t CcLocalRoadWildernessSeedInternal(uint32_t world_seed,
                                           CcId route_id,
                                           int32_t segment_index);
Vector2 CcLocalForkBranchEndInternal(int32_t branch_ordinal,
                                     int32_t branch_count);
float CcLocalRoadCheckpointSurfaceYInternal(float x, float z);
float CcLocalRoadHorseLateralSpacingInternal(bool bridge_checkpoint);
float CcLocalRoadHorseLongitudinalOffsetInternal(void);
void CcLocalDrawFork3D(const CcSim *sim, int32_t selected_route,
                       float turn_progress, float clock,
                       RenderTexture2D target,
                       Rectangle destination);
const char *CcLocalSiteName(const CcSim *sim, CcLocalSiteKind site);
void CcLocalDrawSite3D(const CcSim *sim, const CcLocalAgent *agent,
                       CcLocalSiteKind site, bool travelling,
                       bool returning, float progress, float clock,
                       RenderTexture2D target, Rectangle destination);
void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent, float clock,
                         RenderTexture2D target, Rectangle destination);
void CcLocalDrawInterior3D(const CcSim *sim, const CcLocalAgent *agent,
                           float clock, RenderTexture2D target,
                           Rectangle destination);
Vector2 CcLocalMove(Vector2 current, Vector2 delta, bool market_interior);

#endif
