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

typedef enum CcTraversalMode {
    CC_TRAVERSAL_IDLE,
    CC_TRAVERSAL_WALK,
    CC_TRAVERSAL_CLIMB,
    CC_TRAVERSAL_DESCEND,
    CC_TRAVERSAL_SWIM,
    CC_TRAVERSAL_DROP,
    CC_TRAVERSAL_RAGDOLL,
    CC_TRAVERSAL_GET_UP
} CcTraversalMode;

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
    bool humanoid_needs_reset;
    bool target_valid;
    bool crowned;
    Color tunic_color;
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

typedef struct CcLocalCourse {
    CcLocalCourseRunner runners[CC_LOCAL_COURSE_RUNNER_COUNT];
    CcLocalAgent raiders[CC_LOCAL_RAIDER_COUNT];
    float alarm_countdown;
    float engagement_time;
    float raider_attack_cooldown[CC_LOCAL_RAIDER_COUNT];
    int32_t raider_resolve;
    int32_t defenses_completed;
    bool alarm_active;
    bool raiders_retreating;
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
void CcLocalCourseInit(CcLocalCourse *course);
void CcLocalCourseUpdate(CcLocalCourse *course, const CcSim *sim,
                         float delta_time);
void CcLocalCourseRaiseAlarm(CcLocalCourse *course);

void CcLocalDrawStreet3D(const CcSim *sim, const CcLocalAgent *agent,
                         const CcLocalCourse *course, float clock,
                         RenderTexture2D target, Rectangle destination);
void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent, float clock,
                         RenderTexture2D target, Rectangle destination);
Vector2 CcLocalMove(Vector2 current, Vector2 delta, bool market_interior);

#endif
