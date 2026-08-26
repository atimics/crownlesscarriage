#include "client/cc_npc_appearance.h"

#include <stdint.h>
#include <stdio.h>

typedef struct FaceBlock {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} FaceBlock;

typedef struct FaceBlockCapture {
    FaceBlock blocks[16];
    int32_t count;
} FaceBlockCapture;

static void CaptureFaceBlock(void *context, int32_t grid_x, int32_t grid_y,
                             int32_t grid_width, int32_t grid_height,
                             Color color)
{
    (void)color;
    FaceBlockCapture *capture = context;
    if (capture->count >= 16) return;
    capture->blocks[capture->count++] = (FaceBlock){
        grid_x, grid_y, grid_width, grid_height
    };
}

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

    CcNpcAppearance scout = CcNpcAppearanceGenerate(
        UINT32_C(0x4d4f5645), CC_NPC_ROLE_SCOUT,
        (Color){96, 111, 117, 255});
    CcNpcAppearance refugee = CcNpcAppearanceGenerate(
        UINT32_C(0x4d4f5645), CC_NPC_ROLE_REFUGEE,
        (Color){96, 111, 117, 255});
    if (scout.gait_cadence_scale <= refugee.gait_cadence_scale ||
        scout.stride_scale <= refugee.stride_scale ||
        scout.idle_lean <= refugee.idle_lean) {
        (void)fprintf(stderr,
                      "NPC role movement-signature contract failed\n");
        return 1;
    }

    CcNpcAppearance hero = CcNpcHeroPortraitAppearance(&traveller);
    CcFaceRecipe hero_face = CcNpcFaceRecipe(&hero);
    CcFaceRecipe guard_face = CcNpcFaceRecipe(&guard_a);
    if (hero_face.seed != hero.seed || hero_face.skin.r != 177U ||
        hero_face.hair.r != 27U || hero_face.hair_style != 3U ||
        hero_face.width != hero.head_width ||
        hero_face.depth != hero.head_depth ||
        hero_face.face_shape >= 4U || hero_face.eye_spacing >= 3U ||
        hero_face.brow_style >= 4U || hero_face.mouth_style >= 4U ||
        guard_face.headwear !=
            ((guard_a.equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) ||
        guard_face.nose_style != guard_a.nose_style ||
        guard_face.scar_style != guard_a.scar_style) {
        (void)fprintf(stderr,
                      "shared face recipe contract failed\n");
        return 1;
    }

    CcFaceRecipe layout_face = {
        .age = 0.30f,
        .face_shape = 0U,
        .eye_spacing = 1U,
        .brow_style = 0U,
        .mouth_style = 1U,
        .nose_style = 2U,
        .scar_style = 0U,
        .width = 1.0f,
        .skin_shadow = (Color){90, 70, 50, 255},
        .ink = (Color){20, 20, 20, 255},
    };
    FaceBlockCapture capture = {0};
    CcNpcPaintFaceBase(&layout_face, &capture, CaptureFaceBlock);
    const FaceBlock expected_base[] = {
        {4, 7, 1, 5}, {15, 7, 1, 5}, {5, 4, 10, 10},
        {6, 14, 8, 2}, {7, 16, 6, 1},
    };
    if (capture.count !=
        (int32_t)(sizeof(expected_base) / sizeof(expected_base[0]))) {
        (void)fprintf(stderr, "shared face base block count failed\n");
        return 1;
    }
    for (int32_t index = 0; index < capture.count; ++index) {
        FaceBlock actual = capture.blocks[index];
        FaceBlock wanted = expected_base[index];
        if (actual.x != wanted.x || actual.y != wanted.y ||
            actual.width != wanted.width ||
            actual.height != wanted.height) {
            (void)fprintf(stderr,
                          "shared face base block %d layout failed\n", index);
            return 1;
        }
    }

    capture = (FaceBlockCapture){0};
    CcNpcPaintFaceFeatures(&layout_face, CC_NPC_PORTRAIT_NEUTRAL,
                           &capture, CaptureFaceBlock);
    const FaceBlock expected[] = {
        {6, 6, 3, 1}, {11, 6, 3, 1},
        {7, 8, 1, 1}, {12, 8, 1, 1},
        {8, 11, 4, 1}, {8, 13, 4, 1},
    };
    if (capture.count != (int32_t)(sizeof(expected) / sizeof(expected[0]))) {
        (void)fprintf(stderr, "shared face block count failed\n");
        return 1;
    }
    for (int32_t index = 0; index < capture.count; ++index) {
        FaceBlock actual = capture.blocks[index];
        FaceBlock wanted = expected[index];
        if (actual.x != wanted.x || actual.y != wanted.y ||
            actual.width != wanted.width ||
            actual.height != wanted.height) {
            (void)fprintf(stderr,
                          "shared face block %d layout failed\n", index);
            return 1;
        }
    }

    capture = (FaceBlockCapture){0};
    layout_face.hair_style = 3U;
    layout_face.beard_style = 0U;
    CcNpcPaintFaceHairAndBeard(&layout_face, &capture, CaptureFaceBlock);
    const FaceBlock expected_hair[] = {
        {5, 2, 10, 3}, {4, 4, 7, 2},
        {4, 5, 3, 4}, {12, 3, 4, 2},
    };
    if (capture.count !=
        (int32_t)(sizeof(expected_hair) / sizeof(expected_hair[0]))) {
        (void)fprintf(stderr, "shared face hair block count failed\n");
        return 1;
    }
    for (int32_t index = 0; index < capture.count; ++index) {
        FaceBlock actual = capture.blocks[index];
        FaceBlock wanted = expected_hair[index];
        if (actual.x != wanted.x || actual.y != wanted.y ||
            actual.width != wanted.width ||
            actual.height != wanted.height) {
            (void)fprintf(stderr,
                          "shared face hair block %d layout failed\n", index);
            return 1;
        }
    }

    for (uint32_t role = 0U; role < (uint32_t)CC_NPC_ROLE_COUNT; ++role) {
        CcNpcAppearance person = CcNpcAppearanceGenerate(
            UINT32_C(0x504f5000) + role, (CcNpcRole)role,
            (Color){96, 111, 117, 255});
        if (person.role != (CcNpcRole)role || person.skin_tone >= 10U ||
            person.hair_style >= 8U ||
            person.beard_style >= 4U || person.nose_style >= 4U ||
            person.scar_style >= 4U || person.headwear_style >= 4U ||
            person.garment_style >= 5U ||
            person.stature < 0.90f || person.stature > 1.10f ||
            person.body_mass < 0.82f || person.body_mass > 1.18f ||
            person.muscularity < 0.0f || person.muscularity > 1.0f ||
            person.gait_cadence_scale < 0.65f ||
            person.gait_cadence_scale > 1.35f ||
            person.stride_scale < 0.65f || person.stride_scale > 1.35f ||
            person.bob_scale < 0.40f || person.bob_scale > 1.30f ||
            person.idle_lean < -0.08f || person.idle_lean > 0.08f ||
            person.arm_swing_scale < 0.55f ||
            person.arm_swing_scale > 1.40f) {
            (void)fprintf(stderr,
                          "NPC role %u generated an invalid body recipe\n",
                          role);
            return 1;
        }
    }

    (void)puts("npc appearance tests passed");
    return 0;
}
