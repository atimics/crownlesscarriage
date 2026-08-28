#ifndef CROWNLESS_CREATURE_RIG_H
#define CROWNLESS_CREATURE_RIG_H

#include "locomotion/cc_biomech.h"
#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_CREATURE_RIG_MAX_LIMBS 8
#define CC_CREATURE_RIG_MAX_SEGMENTS CC_LIMB_MAX_SEGMENTS
#define CC_CREATURE_RIG_MAX_JOINTS_PER_LIMB CC_LIMB_MAX_JOINTS

typedef enum CcCreatureRigProfile {
    CC_CREATURE_RIG_GOBLIN,
    CC_CREATURE_RIG_HORSE,
    CC_CREATURE_RIG_COW,
    CC_CREATURE_RIG_DRAGON,
    CC_CREATURE_RIG_HEXAPOD,
    CC_CREATURE_RIG_OCTOPOD,
    CC_CREATURE_RIG_PROFILE_COUNT
} CcCreatureRigProfile;

typedef struct CcCreatureRigLimbPose {
    CcLimbVec3 joints[CC_CREATURE_RIG_MAX_JOINTS_PER_LIMB];
    float segment_activation[CC_CREATURE_RIG_MAX_SEGMENTS];
    CcLimbState state;
    int32_t segment_count;
} CcCreatureRigLimbPose;

typedef struct CcCreatureRigPose {
    CcCreatureRigLimbPose limbs[CC_CREATURE_RIG_MAX_LIMBS];
    CcLimbVec3 body;
    CcLimbVec3 forward;
    CcLimbVec3 right;
    float body_width;
    float body_depth;
    float body_length;
    float mean_activation;
    float phase;
    float movement;
    float support_margin;
    float drive_scale;
    int32_t limb_count;
    int32_t planted_count;
    int32_t swinging_count;
    int32_t biomech_bone_count;
    int32_t biomech_joint_count;
    int32_t biomech_muscle_count;
    CcCreatureRigProfile profile;
    bool valid;
} CcCreatureRigPose;

typedef struct CcCreatureRigController {
    CcLimbRig skeleton;
    CcBiomechRig muscles;
    CcLimbVec3 ground_position;
    CcCreatureRigProfile profile;
    float scale;
    float movement;
    bool initialized;
} CcCreatureRigController;

bool CcCreatureRigPoseResolve(CcCreatureRigProfile profile, float phase,
                              float movement, CcLimbVec3 ground_position,
                              float yaw, float scale,
                              CcCreatureRigPose *pose);
bool CcCreatureRigControllerInit(CcCreatureRigController *controller,
                                 CcCreatureRigProfile profile,
                                 float phase, float scale);
bool CcCreatureRigControllerStep(CcCreatureRigController *controller,
                                 float forward_speed, float movement,
                                 float delta_time,
                                 CcCreatureRigPose *pose);
const char *CcCreatureRigProfileName(CcCreatureRigProfile profile);

#endif
