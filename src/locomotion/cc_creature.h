#ifndef CROWNLESS_CREATURE_RIG_H
#define CROWNLESS_CREATURE_RIG_H

#include "locomotion/cc_biomech.h"
#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_CREATURE_RIG_MAX_LIMBS 4
#define CC_CREATURE_RIG_JOINTS_PER_LIMB 3

typedef enum CcCreatureRigProfile {
    CC_CREATURE_RIG_GOBLIN,
    CC_CREATURE_RIG_HORSE,
    CC_CREATURE_RIG_COW,
    CC_CREATURE_RIG_DRAGON,
    CC_CREATURE_RIG_PROFILE_COUNT
} CcCreatureRigProfile;

typedef struct CcCreatureRigLimbPose {
    CcLimbVec3 joints[CC_CREATURE_RIG_JOINTS_PER_LIMB];
    CcLimbState state;
    float upper_activation;
    float lower_activation;
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
    int32_t limb_count;
    int32_t biomech_bone_count;
    int32_t biomech_joint_count;
    int32_t biomech_muscle_count;
    CcCreatureRigProfile profile;
    bool valid;
} CcCreatureRigPose;

bool CcCreatureRigPoseResolve(CcCreatureRigProfile profile, float phase,
                              float movement, CcLimbVec3 ground_position,
                              float yaw, float scale,
                              CcCreatureRigPose *pose);
const char *CcCreatureRigProfileName(CcCreatureRigProfile profile);

#endif
