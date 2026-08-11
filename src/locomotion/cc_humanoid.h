#ifndef CROWNLESS_HUMANOID_H
#define CROWNLESS_HUMANOID_H

#include "locomotion/cc_biomech.h"
#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_HUMANOID_LEG_COUNT 2
#define CC_HUMANOID_ARM_COUNT 2

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

typedef struct CcHumanoidGait {
    CcHumanoidFoot feet[CC_HUMANOID_LEG_COUNT];
    CcHumanoidPose pose;
    CcHumanoidPose previous_pose;
    CcHumanoidPose recovery_start_pose;
    CcHumanoidPose recovery_target_pose;
    CcHumanoidPose climb_entry_pose;
    CcLimbVec3 climb_entry_foot_normal[CC_HUMANOID_LEG_COUNT];
    CcBiomechRig body;
    CcBiomechRagdoll ragdoll;
    CcLimbVec3 root_velocity;
    CcLimbVec3 ground_reaction;
    CcHumanoidSpring speed;
    CcHumanoidSpring pelvis_height;
    CcHumanoidSpring pelvis_sway;
    CcHumanoidSpring pelvis_roll;
    CcHumanoidSpring pelvis_yaw;
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
    CcLimbVec3 recovery_origin;
    int32_t support_leg;
    int32_t planted_count;
    bool grounded;
    bool recovering;
    bool climbing;
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
void CcHumanoidGaitBeginClimb(CcHumanoidGait *gait);
void CcHumanoidGaitAdvanceClimb(
    CcHumanoidGait *gait, CcLimbVec3 body_position, float body_yaw,
    const CcLimbVec3 hand_targets[CC_HUMANOID_ARM_COUNT],
    const CcLimbVec3 foot_targets[CC_HUMANOID_LEG_COUNT],
    const CcLimbVec3 foot_normals[CC_HUMANOID_LEG_COUNT],
    float climb_progress, float delta_time,
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

#endif
