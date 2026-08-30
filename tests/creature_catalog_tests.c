#include "client/cc_creature_catalog.h"

#include <stdio.h>
#include <string.h>

#define EXPECT(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", message); \
        failures += 1; \
    } \
} while (0)

int main(void)
{
    int failures = 0;
    for (int32_t variant = 0; variant < CC_CREATURE_VARIANT_COUNT;
         ++variant) {
        const CcCreatureDefinition *definition =
            CcCreatureDefinitionAt((CcCreatureVariant)variant);
        EXPECT(definition != NULL, "every creature has a definition");
        EXPECT(CcCreatureSupportsPose((CcCreatureVariant)variant,
                                      CC_CREATURE_POSE_IDLE),
               "every creature has an idle pose");
        EXPECT(CcCreatureAssetPath((CcCreatureVariant)variant,
                                   CC_CREATURE_POSE_IDLE) != NULL,
               "every creature idle pose has an asset path");
    }
    EXPECT(CcCreaturePoseCount(CC_CREATURE_HORSE) == 1,
           "horse uses one runtime-driven skin");
    EXPECT(CcCreaturePoseCount(CC_CREATURE_COW) == 1,
           "cow uses one runtime-driven skin");
    EXPECT(CcCreaturePoseCount(CC_CREATURE_DRAGON) == 5,
           "dragon has its authored pose set");
    EXPECT(CcCreaturePoseCount(CC_CREATURE_DRAGON_WHELP) == 5,
           "whelp has its authored pose set");
    EXPECT(CcCreaturePoseCount(CC_CREATURE_DRAGON_WANDERER) == 5,
           "wanderer has its authored pose set");
    EXPECT(CcCreaturePoseCount(CC_CREATURE_DRAGON_DEEP_WYRM) == 5,
           "deep wyrm has its authored pose set");
    EXPECT(!CcCreatureSupportsPose(CC_CREATURE_DRAGON,
                                   CC_CREATURE_POSE_CONTACT_A),
           "dragon does not claim the common stepped gait");
    EXPECT(CcCreatureSteppedPose(CC_CREATURE_HORSE, 0.0f, false) ==
               CC_CREATURE_POSE_IDLE,
           "a stopped horse uses idle");
    EXPECT(CcCreatureSteppedPose(CC_CREATURE_HORSE, 0.0f, true) ==
               CC_CREATURE_POSE_IDLE,
           "a moving horse keeps its single skinned asset");
    EXPECT(CcCreatureDefinitionAt(CC_CREATURE_HORSE)->skinned,
           "horse advertises its runtime skin");
    EXPECT(CcCreatureDefinitionAt(CC_CREATURE_COW)->skinned,
           "cow advertises its runtime skin");
    EXPECT(!CcCreatureDefinitionAt(CC_CREATURE_DRAGON)->skinned,
           "the authored dragon remains a held-pose actor");
    EXPECT(strcmp(CcCreatureDefinitionAt(CC_CREATURE_DRAGON_WHELP)->family,
                  "dragon") == 0,
           "whelp belongs to the dragon family");
    EXPECT(strcmp(CcCreatureDefinitionAt(CC_CREATURE_DRAGON_DEEP_WYRM)->family,
                  "dragon") == 0,
           "deep wyrm belongs to the dragon family");
    EXPECT(strstr(CcCreatureAssetPath(CC_CREATURE_GOBLIN_TRIBUTE_BEARER,
                                      CC_CREATURE_POSE_IDLE),
                  "goblin_tribute_bearer") != NULL,
           "tribute bearer resolves its own asset family");
    EXPECT(CcCreatureDefinitionAt(CC_CREATURE_VARIANT_COUNT) == NULL,
           "invalid variants are rejected");
    EXPECT(CcCreatureAssetPath(CC_CREATURE_HORSE,
                               CC_CREATURE_POSE_COUNT) == NULL,
           "invalid poses are rejected");
    if (failures != 0) return 1;
    puts("creature catalog contract passed");
    return 0;
}
