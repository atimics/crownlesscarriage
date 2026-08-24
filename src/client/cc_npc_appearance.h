#ifndef CROWNLESS_NPC_APPEARANCE_H
#define CROWNLESS_NPC_APPEARANCE_H

#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum CcNpcRole {
    CC_NPC_ROLE_WAYFARER,
    CC_NPC_ROLE_GUARD,
    CC_NPC_ROLE_RAIDER,
    CC_NPC_ROLE_MERCHANT,
    CC_NPC_ROLE_LABORER,
    CC_NPC_ROLE_TRAVELLER,
    CC_NPC_ROLE_REFUGEE,
    CC_NPC_ROLE_SCOUT,
    CC_NPC_ROLE_HEALER,
    CC_NPC_ROLE_COUNT
} CcNpcRole;

typedef enum CcNpcEquipment {
    CC_NPC_EQUIPMENT_NONE = 0,
    CC_NPC_EQUIPMENT_MANTLE = 1 << 0,
    CC_NPC_EQUIPMENT_APRON = 1 << 1,
    CC_NPC_EQUIPMENT_ARMOR = 1 << 2,
    CC_NPC_EQUIPMENT_SATCHEL = 1 << 3,
    CC_NPC_EQUIPMENT_PACK = 1 << 4,
    CC_NPC_EQUIPMENT_HEADWEAR = 1 << 5,
    CC_NPC_EQUIPMENT_TOOL = 1 << 6
} CcNpcEquipment;

typedef enum CcNpcPortraitExpression {
    CC_NPC_PORTRAIT_NEUTRAL,
    CC_NPC_PORTRAIT_FOCUSED,
    CC_NPC_PORTRAIT_HURT,
    CC_NPC_PORTRAIT_TALKING
} CcNpcPortraitExpression;

/* A stable, inexpensive visual identity for background and simulated people.
   Values are deliberately bounded around the shared humanoid skeleton so
   animation, hit volumes, and crowd navigation remain authoritative. */
typedef struct CcNpcAppearance {
    uint32_t seed;
    CcNpcRole role;
    uint32_t equipment;
    float stature;
    float body_mass;
    float muscularity;
    float shoulder_scale;
    float head_width;
    float head_depth;
    float age;
    float gait_cadence_scale;
    float stride_scale;
    float bob_scale;
    float idle_lean;
    float arm_swing_scale;
    uint8_t skin_tone;
    uint8_t hair_style;
    uint8_t beard_style;
    uint8_t nose_style;
    uint8_t scar_style;
    uint8_t headwear_style;
    uint8_t garment_style;
    Color skin;
    Color hair;
    Color underlayer;
    Color outer;
    Color trousers;
    Color leather;
    Color metal;
    Color accent;
} CcNpcAppearance;

/* The single identity source used by both the close portrait and the small
   world head. Renderers may simplify it for distance, but may not invent a
   second set of facial traits. */
typedef struct CcFaceRecipe {
    uint32_t seed;
    float width;
    float depth;
    float age;
    uint8_t hair_style;
    uint8_t beard_style;
    uint8_t nose_style;
    uint8_t scar_style;
    uint8_t headwear_style;
    bool headwear;
    Color skin;
    Color skin_shadow;
    Color hair;
    Color ink;
    Color headwear_color;
    Color accent;
} CcFaceRecipe;

CcNpcAppearance CcNpcAppearanceGenerate(uint32_t seed, CcNpcRole role,
                                        Color accent);
const char *CcNpcRoleName(CcNpcRole role);
bool CcNpcAppearanceEqual(const CcNpcAppearance *first,
                          const CcNpcAppearance *second);
CcFaceRecipe CcNpcFaceRecipe(const CcNpcAppearance *appearance);
void CcNpcDrawPixelPortrait(const CcNpcAppearance *appearance,
                            Rectangle bounds,
                            CcNpcPortraitExpression expression,
                            bool crowned);
CcNpcAppearance CcNpcHeroPortraitAppearance(
    const CcNpcAppearance *appearance);

#endif
