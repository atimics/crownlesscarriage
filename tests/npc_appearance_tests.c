#include "client/cc_npc_appearance.h"

#include <stdint.h>
#include <stdio.h>

int main(void)
{
    CcNpcAppearance guard_a = CcNpcAppearanceGenerate(
        UINT32_C(0x47554101), CC_NPC_ROLE_GUARD,
        (Color){50, 151, 160, 255});
    CcNpcAppearance guard_b = CcNpcAppearanceGenerate(
        UINT32_C(0x47554101), CC_NPC_ROLE_GUARD,
        (Color){50, 151, 160, 255});
    CcNpcAppearance traveller = CcNpcAppearanceGenerate(
        UINT32_C(0x54524101), CC_NPC_ROLE_TRAVELLER,
        (Color){118, 134, 145, 255});
    if (!CcNpcAppearanceEqual(&guard_a, &guard_b) ||
        CcNpcAppearanceEqual(&guard_a, &traveller) ||
        (guard_a.equipment & CC_NPC_EQUIPMENT_ARMOR) == 0U ||
        (traveller.equipment & CC_NPC_EQUIPMENT_PACK) == 0U ||
        guard_a.skin_tone >= 10U || guard_a.stature < 0.90f ||
        guard_a.stature > 1.10f) {
        (void)fprintf(stderr,
                      "deterministic NPC appearance contract failed\n");
        return 1;
    }

    for (uint32_t role = 0U; role < (uint32_t)CC_NPC_ROLE_COUNT; ++role) {
        CcNpcAppearance person = CcNpcAppearanceGenerate(
            UINT32_C(0x504f5000) + role, (CcNpcRole)role,
            (Color){96, 111, 117, 255});
        if (person.role != (CcNpcRole)role || person.skin_tone >= 10U ||
            person.garment_style >= 5U ||
            person.stature < 0.90f || person.stature > 1.10f ||
            person.body_mass < 0.82f || person.body_mass > 1.18f ||
            person.muscularity < 0.0f || person.muscularity > 1.0f) {
            (void)fprintf(stderr,
                          "NPC role %u generated an invalid body recipe\n",
                          role);
            return 1;
        }
    }

    (void)puts("npc appearance tests passed");
    return 0;
}
