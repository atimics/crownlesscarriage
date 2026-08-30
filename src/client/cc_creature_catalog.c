#include "client/cc_creature_catalog.h"

#include <math.h>
#include <stddef.h>

#include "client/cc_creature_catalog.generated.inc"

static bool IsDragonVariant(CcCreatureVariant variant)
{
    return variant == CC_CREATURE_DRAGON ||
           variant == CC_CREATURE_DRAGON_WHELP ||
           variant == CC_CREATURE_DRAGON_WANDERER ||
           variant == CC_CREATURE_DRAGON_DEEP_WYRM;
}

const CcCreatureDefinition *CcCreatureDefinitionAt(CcCreatureVariant variant)
{
    if (variant < 0 || variant >= CC_CREATURE_VARIANT_COUNT) return NULL;
    return &CC_CREATURE_DEFINITIONS[variant];
}

const char *CcCreatureAssetPath(CcCreatureVariant variant,
                                CcCreaturePose pose)
{
    if (variant < 0 || variant >= CC_CREATURE_VARIANT_COUNT ||
        pose < 0 || pose >= CC_CREATURE_POSE_COUNT) return NULL;
    return CC_CREATURE_ASSET_PATHS[variant][pose];
}

bool CcCreatureSupportsPose(CcCreatureVariant variant, CcCreaturePose pose)
{
    return CcCreatureAssetPath(variant, pose) != NULL;
}

int32_t CcCreaturePoseCount(CcCreatureVariant variant)
{
    if (variant < 0 || variant >= CC_CREATURE_VARIANT_COUNT) return 0;
    int32_t count = 0;
    for (int32_t pose = 0; pose < CC_CREATURE_POSE_COUNT; ++pose) {
        if (CC_CREATURE_ASSET_PATHS[variant][pose] != NULL) count += 1;
    }
    return count;
}

CcCreaturePose CcCreatureSteppedPose(CcCreatureVariant variant, float phase,
                                     bool moving)
{
    if (!moving || IsDragonVariant(variant) ||
        variant < 0 || variant >= CC_CREATURE_VARIANT_COUNT) {
        return CC_CREATURE_POSE_IDLE;
    }
    float cycle = fmodf(phase / (2.0f * 3.14159265358979323846f), 1.0f);
    if (cycle < 0.0f) cycle += 1.0f;
    int32_t pose = 1 + (int32_t)floorf(cycle * 8.0f);
    if (pose > CC_CREATURE_POSE_UP_B) pose = CC_CREATURE_POSE_UP_B;
    CcCreaturePose stepped = (CcCreaturePose)pose;
    return CcCreatureSupportsPose(variant, stepped) ? stepped :
                                                     CC_CREATURE_POSE_IDLE;
}
