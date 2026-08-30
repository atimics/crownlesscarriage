#ifndef CC_CREATURE_CATALOG_H
#define CC_CREATURE_CATALOG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum CcCreatureVariant {
    CC_CREATURE_GOBLIN_SCAVENGER,
    CC_CREATURE_GOBLIN_RAIDER,
    CC_CREATURE_GOBLIN_TRIBUTE_BEARER,
    CC_CREATURE_HORSE,
    CC_CREATURE_COW,
    CC_CREATURE_DRAGON,
    CC_CREATURE_DRAGON_WHELP,
    CC_CREATURE_DRAGON_WANDERER,
    CC_CREATURE_DRAGON_DEEP_WYRM,
    CC_CREATURE_VARIANT_COUNT
} CcCreatureVariant;

typedef enum CcCreaturePose {
    CC_CREATURE_POSE_IDLE,
    CC_CREATURE_POSE_CONTACT_A,
    CC_CREATURE_POSE_DOWN_A,
    CC_CREATURE_POSE_PASSING_A,
    CC_CREATURE_POSE_UP_A,
    CC_CREATURE_POSE_CONTACT_B,
    CC_CREATURE_POSE_DOWN_B,
    CC_CREATURE_POSE_PASSING_B,
    CC_CREATURE_POSE_UP_B,
    CC_CREATURE_POSE_STALK_A,
    CC_CREATURE_POSE_STALK_B,
    CC_CREATURE_POSE_THREAT,
    CC_CREATURE_POSE_REST,
    CC_CREATURE_POSE_COUNT
} CcCreaturePose;

typedef struct CcCreatureDefinition {
    const char *name;
    const char *family;
    const char *gait;
    bool quadruped;
    bool skinned;
} CcCreatureDefinition;

const CcCreatureDefinition *CcCreatureDefinitionAt(CcCreatureVariant variant);
const char *CcCreatureAssetPath(CcCreatureVariant variant,
                                CcCreaturePose pose);
bool CcCreatureSupportsPose(CcCreatureVariant variant, CcCreaturePose pose);
int32_t CcCreaturePoseCount(CcCreatureVariant variant);
CcCreaturePose CcCreatureSteppedPose(CcCreatureVariant variant, float phase,
                                     bool moving);

#endif
