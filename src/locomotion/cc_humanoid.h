#ifndef CROWNLESS_HUMANOID_H
#define CROWNLESS_HUMANOID_H

#include "locomotion/cc_biomech.h"
#include "locomotion/cc_limb.h"
#include "locomotion/cc_motion.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_HUMANOID_LEG_COUNT 2
#define CC_HUMANOID_ARM_COUNT 2
#define CC_HUMANOID_TRACE_CAPACITY 64

typedef enum CcHumanoidJoint {
    CC_HUMANOID_LEFT_HIP,
    CC_HUMANOID_LEFT_KNEE,
    CC_HUMANOID_LEFT_ANKLE,
    CC_HUMANOID_RIGHT_HIP,
    CC_HUMANOID_RIGHT_KNEE,
    CC_HUMANOID_RIGHT_ANKLE,
    CC_HUMANOID_SPINE_PITCH,
    CC_HUMANOID_SPINE_ROLL,
    CC_HUMANOID_SPINE_YAW,
    CC_HUMANOID_LEFT_SHOULDER,
    CC_HUMANOID_LEFT_ELBOW,
    CC_HUMANOID_RIGHT_SHOULDER,
    CC_HUMANOID_RIGHT_ELBOW,
    CC_HUMANOID_JOINT_COUNT
} CcHumanoidJoint;

typedef enum CcHumanoidContact {
    CC_HUMANOID_CONTACT_AIR,
    CC_HUMANOID_CONTACT_HEEL,
    CC_HUMANOID_CONTACT_FLAT,
    CC_HUMANOID_CONTACT_TOE,
    CC_HUMANOID_CONTACT_SWING
} CcHumanoidContact;

typedef enum CcHumanoidAction {
    CC_HUMANOID_ACTION_LOCOMOTION,
    CC_HUMANOID_ACTION_GUARD,
    CC_HUMANOID_ACTION_STRIKE,
    CC_HUMANOID_ACTION_CLAMBER,
    CC_HUMANOID_ACTION_SWIM,
    CC_HUMANOID_ACTION_FALL,
    CC_HUMANOID_ACTION_RECOVER,
    CC_HUMANOID_ACTION_JUMP
} CcHumanoidAction;

typedef enum CcHumanoidStrikeStyle {
    CC_HUMANOID_STRIKE_CUT,
    CC_HUMANOID_STRIKE_HEAVY,
    CC_HUMANOID_STRIKE_SWEEP
} CcHumanoidStrikeStyle;

/* One owner writes the base pose during a simulation tick. Layers such as
   skinning, sockets, cloth and render interpolation only consume the finalized
   snapshots and never become pose owners. */
typedef enum CcHumanoidPoseOwner {
    CC_HUMANOID_POSE_OWNER_NONE,
    CC_HUMANOID_POSE_OWNER_PROCEDURAL,
    CC_HUMANOID_POSE_OWNER_TRAVERSAL,
    CC_HUMANOID_POSE_OWNER_RAGDOLL,
    CC_HUMANOID_POSE_OWNER_RECOVERY,
    CC_HUMANOID_POSE_OWNER_PAIRED_INTERACTION
} CcHumanoidPoseOwner;

typedef enum CcHumanoidRecoveryOrientation {
    CC_HUMANOID_RECOVERY_SUPINE,
    CC_HUMANOID_RECOVERY_PRONE,
    CC_HUMANOID_RECOVERY_LEFT,
    CC_HUMANOID_RECOVERY_RIGHT
} CcHumanoidRecoveryOrientation;

typedef struct CcHumanoidSpring {
    float value;
    float velocity;
} CcHumanoidSpring;

typedef struct CcHumanoidFoot {
    CcLimbVec3 planted_point;
    CcLimbVec3 swing_start;
    CcLimbVec3 swing_target;
    CcLimbVec3 current_point;
    CcLimbVec3 normal;
    CcHumanoidSpring pitch;
    CcHumanoidContact contact;
    float local_phase;
} CcHumanoidFoot;

typedef struct CcHumanoidPose {
    CcLimbVec3 pelvis;
    CcLimbVec3 spine;
    CcLimbVec3 chest;
    CcLimbVec3 neck;
    CcLimbVec3 head;
    CcLimbVec3 hip[CC_HUMANOID_LEG_COUNT];
    CcLimbVec3 knee[CC_HUMANOID_LEG_COUNT];
    CcLimbVec3 ankle[CC_HUMANOID_LEG_COUNT];
    CcLimbVec3 heel[CC_HUMANOID_LEG_COUNT];
    CcLimbVec3 ball[CC_HUMANOID_LEG_COUNT];
    CcLimbVec3 toe[CC_HUMANOID_LEG_COUNT];
    CcLimbVec3 shoulder[CC_HUMANOID_ARM_COUNT];
    CcLimbVec3 elbow[CC_HUMANOID_ARM_COUNT];
    CcLimbVec3 hand[CC_HUMANOID_ARM_COUNT];
    float pelvis_yaw;
    float pelvis_roll;
    float pelvis_pitch;
    float chest_yaw;
    float chest_roll;
    float chest_pitch;
    float foot_pitch[CC_HUMANOID_LEG_COUNT];
    float knee_flexion[CC_HUMANOID_LEG_COUNT];
} CcHumanoidPose;

typedef struct CcHumanoidPoseSnapshot {
    CcHumanoidPose pose;
    CcLimbVec3 root_linear_velocity;
    CcLimbVec3 root_angular_velocity;
    CcHumanoidPoseOwner owner;
    uint64_t sequence;
    bool valid;
} CcHumanoidPoseSnapshot;

typedef struct CcHumanoidIdleState {
    CcLimbVec3 foot_anchor[CC_HUMANOID_LEG_COUNT];
    CcLimbVec3 foot_normal[CC_HUMANOID_LEG_COUNT];
    float still_time;
    float locked_phase;
    bool stable;
    bool pose_locked;
} CcHumanoidIdleState;

typedef struct CcHumanoidAnimationTraceRecord {
    uint64_t sequence;
    CcLimbVec3 root_position;
    CcLimbVec3 root_velocity;
    float motion_time;
    float speed;
    float phase;
    CcHumanoidPoseOwner owner;
    CcHumanoidAction action;
    CcMotionClipId clip;
    CcHumanoidContact contact[CC_HUMANOID_LEG_COUNT];
    uint32_t markers;
    bool idle_stable;
    bool idle_locked;
} CcHumanoidAnimationTraceRecord;

typedef struct CcHumanoidAnimationTrace {
    CcHumanoidAnimationTraceRecord records[CC_HUMANOID_TRACE_CAPACITY];
    int32_t next;
    int32_t count;
} CcHumanoidAnimationTrace;

typedef struct CcHumanoidGait {
    CcHumanoidFoot feet[CC_HUMANOID_LEG_COUNT];
    CcHumanoidPose pose;
    CcHumanoidPose previous_pose;
    CcHumanoidPoseSnapshot snapshot;
    CcHumanoidPoseSnapshot previous_snapshot;
    CcHumanoidPose recovery_start_pose;
    CcHumanoidPose recovery_target_pose;
    CcHumanoidPose climb_entry_pose;
    CcHumanoidPose swim_entry_pose;
    CcLimbVec3 climb_entry_foot_normal[CC_HUMANOID_LEG_COUNT];
    CcBiomechRig body;
    CcBiomechRagdoll ragdoll;
    CcLimbVec3 root_velocity;
    CcLimbVec3 ground_reaction;
    CcLimbVec3 impact_direction;
    CcHumanoidSpring speed;
    CcHumanoidSpring pelvis_height;
    CcHumanoidSpring pelvis_sway;
    CcHumanoidSpring pelvis_roll;
    CcHumanoidSpring pelvis_yaw;
    CcMotionPlayer motion;
    CcHumanoidIdleState idle;
    CcHumanoidAnimationTrace trace;
    float phase;
    float travel_yaw;
    float cadence;
    float last_delta_time;
    float ragdoll_time;
    float ragdoll_settled_time;
    float recovery_time;
    float recovery_error;
    float recovery_speed;
    float recovery_yaw;
    float action_time;
    float action_blend;
    float swim_phase;
    float immersion;
    float impact_response;
    CcLimbVec3 recovery_origin;
    CcHumanoidAction action;
    CcHumanoidAction previous_action;
    CcHumanoidPoseOwner pose_owner;
    CcHumanoidRecoveryOrientation recovery_orientation;
    uint32_t motion_markers;
    int32_t support_leg;
    int32_t planted_count;
    int32_t strike_side;
    CcHumanoidStrikeStyle strike_style;
    bool strike_impact_pending;
    bool strike_impact_emitted;
    bool guard_requested;
    bool grounded;
    bool recovering;
    bool ragdoll_recovery_allowed;
    bool climbing;
    bool jump_airborne;
    bool initialized;
} CcHumanoidGait;

void CcHumanoidGaitInit(CcHumanoidGait *gait, CcLimbVec3 body_position,
                        float body_yaw, CcLimbTerrainProbe probe,
                        void *probe_context);
void CcHumanoidGaitAdvance(CcHumanoidGait *gait, CcLimbVec3 body_position,
                           float body_yaw, CcLimbVec3 desired_velocity,
                           bool grounded, float delta_time,
                           CcLimbTerrainProbe probe, void *probe_context);
void CcHumanoidGaitResolvePose(CcHumanoidGait *gait,
                               CcLimbVec3 body_position, float body_yaw);
void CcHumanoidGaitSetGuarded(CcHumanoidGait *gait, bool guarded);
bool CcHumanoidGaitBeginStrike(CcHumanoidGait *gait, int32_t striking_arm);
void CcHumanoidGaitSetStrikeStyle(CcHumanoidGait *gait,
                                  CcHumanoidStrikeStyle style);
bool CcHumanoidGaitBeginJump(CcHumanoidGait *gait);
void CcHumanoidGaitApplyImpact(CcHumanoidGait *gait,
                               CcLimbVec3 direction, float strength);
bool CcHumanoidGaitKnockDown(CcHumanoidGait *gait);
bool CcHumanoidGaitDie(CcHumanoidGait *gait, CcLimbVec3 impact_direction,
                       CcLimbVec3 impact_point, float impact_speed);
void CcHumanoidGaitBeginResurrection(CcHumanoidGait *gait);
bool CcHumanoidGaitConsumeStrikeImpact(CcHumanoidGait *gait);
uint32_t CcHumanoidGaitConsumeMotionMarkers(CcHumanoidGait *gait);
void CcHumanoidGaitAdvanceSwim(CcHumanoidGait *gait,
                               CcLimbVec3 body_position, float body_yaw,
                               CcLimbVec3 desired_velocity,
                               float water_surface, float immersion,
                               float delta_time);
void CcHumanoidGaitEndSwim(CcHumanoidGait *gait,
                           CcLimbVec3 body_position, float body_yaw,
                           CcLimbTerrainProbe probe, void *probe_context);
void CcHumanoidGaitBeginClimb(CcHumanoidGait *gait);
void CcHumanoidGaitAdvanceClimb(
    CcHumanoidGait *gait, CcLimbVec3 body_position, float body_yaw,
    const CcLimbVec3 hand_targets[CC_HUMANOID_ARM_COUNT],
    const CcLimbVec3 foot_targets[CC_HUMANOID_LEG_COUNT],
    const CcLimbVec3 foot_normals[CC_HUMANOID_LEG_COUNT],
    const float foot_support[CC_HUMANOID_LEG_COUNT],
    float climb_progress, float delta_time,
    CcLimbTerrainProbe probe, void *probe_context);
void CcHumanoidGaitAdvanceMantle(
    CcHumanoidGait *gait, CcLimbVec3 body_position, float body_yaw,
    CcLimbVec3 ledge, CcLimbVec3 wall_normal,
    const CcLimbVec3 takeoff_feet[CC_HUMANOID_LEG_COUNT],
    float mantle_progress, float delta_time,
    CcLimbTerrainProbe probe, void *probe_context);
void CcHumanoidGaitFinishClimb(CcHumanoidGait *gait,
                               CcLimbVec3 body_position, float body_yaw,
                               CcLimbTerrainProbe probe,
                               void *probe_context);
bool CcHumanoidGaitClimbReady(const CcHumanoidGait *gait,
                              CcLimbVec3 body_position, float body_yaw,
                              CcLimbTerrainProbe probe,
                              void *probe_context, float tolerance);
void CcHumanoidGaitConstrainMotion(CcHumanoidGait *gait,
                                  CcLimbVec3 actual_position,
                                  CcLimbVec3 actual_velocity, bool grounded);
const char *CcHumanoidContactName(CcHumanoidContact contact);
const char *CcHumanoidActionName(CcHumanoidAction action);
const char *CcHumanoidPoseOwnerName(CcHumanoidPoseOwner owner);
const char *CcHumanoidRecoveryOrientationName(
    CcHumanoidRecoveryOrientation orientation);
const CcHumanoidPoseSnapshot *CcHumanoidGaitCurrentSnapshot(
    const CcHumanoidGait *gait);
const CcHumanoidPoseSnapshot *CcHumanoidGaitPreviousSnapshot(
    const CcHumanoidGait *gait);
const CcHumanoidAnimationTraceRecord *CcHumanoidGaitTraceLatest(
    const CcHumanoidGait *gait);

#endif
