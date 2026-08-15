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
    uint8_t skin_tone;
    uint8_t hair_style;
    uint8_t beard_style;
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

CcNpcAppearance CcNpcAppearanceGenerate(uint32_t seed, CcNpcRole role,
                                        Color accent);
const char *CcNpcRoleName(CcNpcRole role);
bool CcNpcAppearanceEqual(const CcNpcAppearance *first,
                          const CcNpcAppearance *second);

#endif
