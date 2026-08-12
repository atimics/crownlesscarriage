#ifndef CROWNLESS_LOCAL3D_H
#define CROWNLESS_LOCAL3D_H

#include "locomotion/cc_limb.h"
#include "locomotion/cc_humanoid.h"
#include "sim/cc_sim.h"

#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_LOCAL_COURSE_RUNNER_COUNT 3
#define CC_LOCAL_RAIDER_COUNT 2
#define CC_LOCAL_TRAVELLER_COUNT 4

/* Exterior coordinates are metres. These landmarks are shared by input,
   collision, rendering, and tests so the continuous world cannot drift apart. */
#define CC_LOCAL_WORLD_WIDTH 96.0f
#define CC_LOCAL_WORLD_DEPTH 72.0f
#define CC_LOCAL_START_X 40.5f
#define CC_LOCAL_START_Z 30.0f
#define CC_LOCAL_MARKET_X 50.0f
#define CC_LOCAL_MARKET_Z 26.75f
#define CC_LOCAL_CARRIAGE_X 39.05f
#define CC_LOCAL_CARRIAGE_Z 31.70f
#define CC_LOCAL_NOTICE_X 41.0f
#define CC_LOCAL_NOTICE_Z 27.80f
#define CC_LOCAL_DUNGEON_X 29.0f
#define CC_LOCAL_DUNGEON_Z 51.80f
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
    CC_TRAVERSAL_JUMP
} CcTraversalMode;

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

typedef struct CcCombatState {
    Vector3 focus_point;
    Vector3 knockback_velocity;
    float health;
    float posture;
    float stagger_seconds;
    float hit_flash_seconds;
    float hitstop_seconds;
    float recovery_seconds;
    int32_t target_index;
    CcCombatTeam team;
    bool focus_valid;
    bool strike_resolved;
    bool defeated;
} CcCombatState;

typedef struct CcLocalCapeState {
    Vector3 point[CC_LOCAL_CAPE_POINT_COUNT];
    Vector3 previous[CC_LOCAL_CAPE_POINT_COUNT];
    Vector3 anchor;
    bool initialized;
} CcLocalCapeState;

typedef struct CcLocalAgent {
    Vector3 position;
    Vector3 velocity;
    Vector3 target_point;
    Vector3 climb_start;
    Vector3 climb_end;
    Vector3 climb_face;
    Vector3 climb_normal;
    Vector3 climb_hand_left;
    Vector3 climb_hand_right;
    float facing_yaw;
    float ragdoll_visual_blend;
    float climb_progress;
    float climb_settle;
    float climb_duration;
    float climb_start_yaw;
    float climb_end_yaw;
    CcTraversalMode traversal;
    bool grounded;
    bool climbing;
    bool climbing_down;
    bool swimming;
    bool allow_downclimb;
    bool exact_target_valid;
    float radius;
    float immersion;
    CcMorphologyPreset morphology;
    CcLimbRig limb_rig;
    CcHumanoidGait humanoid;
    CcHumanoidPose render_pose;
    CcLocalCapeState cape;
    float simulation_accumulator;
    bool render_pose_valid;
    bool humanoid_needs_reset;
    bool target_valid;
    bool crowned;
    Color tunic_color;
    CcCombatState combat;
} CcLocalAgent;

typedef enum CcGuardDuty {
    CC_GUARD_TRAINING,
    CC_GUARD_RESPONDING,
    CC_GUARD_ENGAGED,
    CC_GUARD_RETURNING
} CcGuardDuty;

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
    Vector3 raider_entry[CC_LOCAL_RAIDER_COUNT];
    Vector3 combat_origin;
    float alarm_countdown;
    float engagement_time;
    float raider_attack_cooldown[CC_LOCAL_RAIDER_COUNT];
    int32_t raider_response_stage[CC_LOCAL_RAIDER_COUNT];
    int32_t raider_resolve;
    int32_t defenses_completed;
    CcCombatOutcome last_outcome;
    CcCombatTeam last_attacker_team;
    float combat_event_seconds;
    bool alarm_active;
    bool raiders_retreating;
    bool combat_origin_valid;
    bool raider_response_waypoint_active[CC_LOCAL_RAIDER_COUNT];
} CcLocalCourse;

void CcLocalAgentInit(CcLocalAgent *agent, Vector2 position, bool market_interior);
void CcLocalAgentUpdate(CcLocalAgent *agent, float delta_time, bool market_interior);
bool CcLocalAgentSetExactTarget(CcLocalAgent *agent, Vector3 target,
                                bool market_interior);
bool CcLocalAgentPickTarget(CcLocalAgent *agent, Vector2 screen_point,
                            RenderTexture2D target, Rectangle destination,
                            bool market_interior);
Vector2 CcLocalAgentPosition(const CcLocalAgent *agent);
const char *CcLocalTraversalName(CcTraversalMode mode);
void CcLocalAgentSetMorphology(CcLocalAgent *agent, CcMorphologyPreset preset,
                               bool market_interior);
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
CcCombatOutcome CcLocalCombatResolveStrike(CcLocalAgent *attacker,
                                           CcLocalAgent *defender);
const char *CcLocalCombatOutcomeName(CcCombatOutcome outcome);
const char *CcLocalCombatTeamName(CcCombatTeam team);
void CcLocalCourseInit(CcLocalCourse *course);
void CcLocalCourseUpdate(CcLocalCourse *course, CcLocalAgent *player,
                         const CcSim *sim, float delta_time);
void CcLocalCourseRaiseAlarm(CcLocalCourse *course);
void CcLocalCourseRaiseAlarmNear(CcLocalCourse *course,
                                 const CcLocalAgent *player);
bool CcLocalCourseBeginPlayerStrike(CcLocalCourse *course,
                                    CcLocalAgent *player);
void CcLocalCourseSetPlayerGuarded(CcLocalCourse *course,
                                   CcLocalAgent *player, bool guarded);

void CcLocalRendererInit(void);
void CcLocalRendererShutdown(void);
void CcLocalDrawStreet3D(const CcSim *sim, const CcLocalAgent *agent,
                         const CcLocalCourse *course, float clock,
                         RenderTexture2D target, Rectangle destination);
void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent, float clock,
                         RenderTexture2D target, Rectangle destination);
Vector2 CcLocalMove(Vector2 current, Vector2 delta, bool market_interior);

#endif
