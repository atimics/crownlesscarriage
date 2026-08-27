#ifndef CROWNLESS_QUADRUPED_H
#define CROWNLESS_QUADRUPED_H

#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum CcQuadrupedMorphology {
    CC_QUADRUPED_HORSE,
    CC_QUADRUPED_COW,
    CC_QUADRUPED_MORPHOLOGY_COUNT
} CcQuadrupedMorphology;

typedef enum CcQuadrupedBone {
    CC_QUADRUPED_ROOT,
    CC_QUADRUPED_BODY,
    CC_QUADRUPED_CHEST,
    CC_QUADRUPED_NECK,
    CC_QUADRUPED_HEAD,
    CC_QUADRUPED_UPPER_LEG_FL,
    CC_QUADRUPED_LOWER_LEG_FL,
    CC_QUADRUPED_HOOF_FL,
    CC_QUADRUPED_UPPER_LEG_FR,
    CC_QUADRUPED_LOWER_LEG_FR,
    CC_QUADRUPED_HOOF_FR,
    CC_QUADRUPED_UPPER_LEG_HL,
    CC_QUADRUPED_LOWER_LEG_HL,
    CC_QUADRUPED_HOOF_HL,
    CC_QUADRUPED_UPPER_LEG_HR,
    CC_QUADRUPED_LOWER_LEG_HR,
    CC_QUADRUPED_HOOF_HR,
    CC_QUADRUPED_TAIL_ROOT,
    CC_QUADRUPED_TAIL,
    CC_QUADRUPED_BONE_COUNT
} CcQuadrupedBone;

typedef struct CcQuadrupedBonePose {
    CcLimbVec3 head;
    CcLimbVec3 tail;
    CcLimbVec3 up;
    int32_t parent;
} CcQuadrupedBonePose;

typedef struct CcQuadrupedPose {
    CcQuadrupedBonePose bones[CC_QUADRUPED_BONE_COUNT];
    bool valid;
} CcQuadrupedPose;

const char *CcQuadrupedBoneName(CcQuadrupedBone bone);
int32_t CcQuadrupedBoneParent(CcQuadrupedBone bone);
int32_t CcQuadrupedBoneFind(const char *name);
void CcQuadrupedPoseResolve(CcQuadrupedMorphology morphology, float phase,
                            bool moving, CcQuadrupedPose *result);

#endif
