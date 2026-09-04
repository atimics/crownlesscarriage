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

typedef enum CcNpcHeadFamily {
    CC_NPC_HEAD_FAMILY_SQUARE,
    CC_NPC_HEAD_FAMILY_LONG,
    CC_NPC_HEAD_FAMILY_BROAD,
    CC_NPC_HEAD_FAMILY_VETERAN,
    CC_NPC_HEAD_FAMILY_COUNT
} CcNpcHeadFamily;

typedef enum CcNpcHairFamily {
    CC_NPC_HAIR_FAMILY_CROPPED,
    CC_NPC_HAIR_FAMILY_SWEPT,
    CC_NPC_HAIR_FAMILY_BOB,
    CC_NPC_HAIR_FAMILY_CREST,
    CC_NPC_HAIR_FAMILY_BRAIDED,
    CC_NPC_HAIR_FAMILY_REAR_LOCK,
    CC_NPC_HAIR_FAMILY_COUNT
} CcNpcHairFamily;


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
    uint8_t head_family;
    uint8_t hair_family;
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


typedef struct CcFaceRecipe {
    uint32_t seed;
    float width;
    float depth;
    float age;

    uint8_t face_shape;
    uint8_t eye_spacing;
    uint8_t brow_style;
    uint8_t mouth_style;
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


typedef void (*CcNpcFaceBlockPainter)(void *context, int32_t grid_x,
                                      int32_t grid_y, int32_t grid_width,
                                      int32_t grid_height, Color color);

CcNpcAppearance CcNpcAppearanceGenerate(uint32_t seed, CcNpcRole role,
                                        Color accent);

CcNpcAppearance CcNpcCrownlessAppearance(void);
CcNpcAppearance CcNpcPlayerAppearance(uint32_t choices);
CcNpcAppearance CcNpcMaraAppearance(void);
const char *CcNpcRoleName(CcNpcRole role);
bool CcNpcAppearanceEqual(const CcNpcAppearance *first,
                          const CcNpcAppearance *second);
CcFaceRecipe CcNpcFaceRecipe(const CcNpcAppearance *appearance);
void CcNpcPaintFaceBase(const CcFaceRecipe *face, void *context,
                        CcNpcFaceBlockPainter paint);
void CcNpcPaintFaceHairAndBeard(const CcFaceRecipe *face, void *context,
                                CcNpcFaceBlockPainter paint);
void CcNpcPaintFaceFringe(const CcFaceRecipe *face, void *context,
                          CcNpcFaceBlockPainter paint);
void CcNpcPaintFaceBeard(const CcFaceRecipe *face, void *context,
                         CcNpcFaceBlockPainter paint);
void CcNpcPaintFaceFeatures(const CcFaceRecipe *face,
                            CcNpcPortraitExpression expression,
                            void *context, CcNpcFaceBlockPainter paint);
void CcNpcDrawPixelPortrait(const CcNpcAppearance *appearance,
                            Rectangle bounds,
                            CcNpcPortraitExpression expression,
                            bool crowned);

#endif
