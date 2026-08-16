#include "client/cc_npc_appearance.h"

#include <math.h>
#include <stddef.h>

static uint32_t Mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return value;
}

static uint32_t Sample(uint32_t seed, uint32_t stream)
{
    return Mix32(seed ^ (UINT32_C(0x9e3779b9) * (stream + 1U)));
}

static float Unit(uint32_t seed, uint32_t stream)
{
    return (float)(Sample(seed, stream) & UINT32_C(0x00ffffff)) /
           16777215.0f;
}

static Color Shade(Color color, float amount)
{
    float red = fmaxf(0.0f, fminf(255.0f, (float)color.r * amount));
    float green = fmaxf(0.0f, fminf(255.0f, (float)color.g * amount));
    float blue = fmaxf(0.0f, fminf(255.0f, (float)color.b * amount));
    return (Color){(unsigned char)lroundf(red),
                   (unsigned char)lroundf(green),
                   (unsigned char)lroundf(blue), color.a};
}

static Color Blend(Color first, Color second, float amount)
{
    amount = fmaxf(0.0f, fminf(1.0f, amount));
    return (Color){
        (unsigned char)lroundf((float)first.r +
                               ((float)second.r - (float)first.r) * amount),
        (unsigned char)lroundf((float)first.g +
                               ((float)second.g - (float)first.g) * amount),
        (unsigned char)lroundf((float)first.b +
                               ((float)second.b - (float)first.b) * amount),
        255
    };
}

static Color PaletteColor(const Color *palette, int32_t count,
                          uint32_t seed, uint32_t stream)
{
    return palette[Sample(seed, stream) % (uint32_t)count];
}

CcNpcAppearance CcNpcAppearanceGenerate(uint32_t seed, CcNpcRole role,
                                        Color accent)
{
    static const Color skin_palette[] = {
        {242, 205, 176, 255}, {229, 184, 148, 255},
        {213, 158, 119, 255}, {191, 132, 94, 255},
        {166, 108, 76, 255},  {137, 84, 61, 255},
        {111, 68, 53, 255},   {86, 53, 44, 255},
        {203, 171, 142, 255}, {154, 120, 96, 255}
    };
    static const Color hair_palette[] = {
        {31, 27, 29, 255}, {55, 39, 31, 255}, {88, 56, 36, 255},
        {126, 75, 45, 255}, {178, 137, 79, 255}, {91, 86, 82, 255},
        {184, 178, 165, 255}, {48, 43, 52, 255}
    };
    static const Color cloth_palette[] = {
        {55, 104, 105, 255}, {99, 62, 69, 255},  {97, 83, 58, 255},
        {63, 81, 72, 255},   {73, 62, 87, 255},  {119, 88, 61, 255},
        {51, 69, 78, 255},   {112, 102, 83, 255}, {84, 97, 67, 255},
        {122, 71, 57, 255},  {67, 91, 103, 255}, {89, 72, 69, 255}
    };
    static const Color under_palette[] = {
        {186, 174, 145, 255}, {145, 151, 137, 255},
        {164, 137, 111, 255}, {112, 125, 121, 255},
        {180, 159, 132, 255}, {126, 119, 117, 255}
    };
    static const Color leather_palette[] = {
        {65, 45, 35, 255}, {88, 57, 39, 255}, {105, 71, 44, 255},
        {54, 49, 43, 255}, {91, 62, 53, 255}
    };

    if (role < CC_NPC_ROLE_WAYFARER || role >= CC_NPC_ROLE_COUNT) {
        role = CC_NPC_ROLE_TRAVELLER;
    }
    if (seed == 0U) seed = UINT32_C(0x4f1bbcdc);
    CcNpcAppearance result = {0};
    result.seed = seed;
    result.role = role;
    result.stature = 0.91f + Unit(seed, 0U) * 0.18f;
    result.body_mass = 0.84f + Unit(seed, 1U) * 0.34f;
    result.muscularity = 0.30f + Unit(seed, 2U) * 0.64f;
    result.shoulder_scale = 0.88f + Unit(seed, 3U) * 0.25f;
    result.head_width = 0.91f + Unit(seed, 4U) * 0.17f;
    result.head_depth = 0.92f + Unit(seed, 5U) * 0.17f;
    result.age = 0.12f + Unit(seed, 6U) * 0.83f;
    result.gait_cadence_scale = 0.90f + Unit(seed, 18U) * 0.22f;
    result.stride_scale = 0.88f + Unit(seed, 19U) * 0.24f;
    result.bob_scale = 0.76f + Unit(seed, 20U) * 0.36f;
    result.idle_lean = (Unit(seed, 21U) - 0.5f) * 0.050f;
    result.arm_swing_scale = 0.84f + Unit(seed, 22U) * 0.32f;
    result.skin_tone = (uint8_t)(Sample(seed, 7U) %
        (uint32_t)(sizeof(skin_palette) / sizeof(skin_palette[0])));
    result.hair_style = (uint8_t)(Sample(seed, 8U) % 6U);
    result.beard_style = (uint8_t)(Sample(seed, 9U) % 4U);
    result.garment_style = (uint8_t)(Sample(seed, 10U) % 5U);
    result.skin = skin_palette[result.skin_tone];
    result.hair = PaletteColor(
        hair_palette, (int32_t)(sizeof(hair_palette) /
                                sizeof(hair_palette[0])), seed, 11U);
    if (result.age > 0.70f) {
        result.hair = Blend(result.hair, (Color){184, 178, 165, 255},
                            (result.age - 0.70f) / 0.30f);
    }
    result.outer = PaletteColor(
        cloth_palette, (int32_t)(sizeof(cloth_palette) /
                                 sizeof(cloth_palette[0])), seed, 12U);
    result.underlayer = PaletteColor(
        under_palette, (int32_t)(sizeof(under_palette) /
                                 sizeof(under_palette[0])), seed, 13U);
    result.trousers = Shade(PaletteColor(
        cloth_palette, (int32_t)(sizeof(cloth_palette) /
                                 sizeof(cloth_palette[0])), seed, 14U), 0.62f);
    result.leather = PaletteColor(
        leather_palette, (int32_t)(sizeof(leather_palette) /
                                   sizeof(leather_palette[0])), seed, 15U);
    result.metal = (Sample(seed, 16U) & 1U) != 0U ?
        (Color){91, 103, 101, 255} : (Color){119, 111, 94, 255};
    result.accent = accent.a != 0U ? accent : Shade(result.outer, 1.22f);

    switch (role) {
        case CC_NPC_ROLE_WAYFARER:
            result.equipment = CC_NPC_EQUIPMENT_MANTLE |
                               CC_NPC_EQUIPMENT_SATCHEL;
            result.outer = Blend(result.outer, (Color){45, 116, 119, 255},
                                 0.42f);
            result.stride_scale *= 1.06f;
            result.arm_swing_scale *= 1.08f;
            break;
        case CC_NPC_ROLE_GUARD:
            result.equipment = CC_NPC_EQUIPMENT_ARMOR |
                               CC_NPC_EQUIPMENT_HEADWEAR |
                               CC_NPC_EQUIPMENT_TOOL;
            result.muscularity = fmaxf(result.muscularity, 0.62f);
            result.outer = Shade(result.accent, 0.76f);
            result.gait_cadence_scale *= 0.94f;
            result.stride_scale *= 0.88f;
            result.bob_scale *= 0.64f;
            result.arm_swing_scale *= 0.72f;
            result.idle_lean = 0.010f;
            break;
        case CC_NPC_ROLE_RAIDER:
            result.equipment = CC_NPC_EQUIPMENT_MANTLE |
                               CC_NPC_EQUIPMENT_ARMOR |
                               CC_NPC_EQUIPMENT_TOOL;
            result.outer = Blend(result.outer, (Color){104, 46, 51, 255},
                                 0.68f);
            result.muscularity = fmaxf(result.muscularity, 0.56f);
            result.gait_cadence_scale *= 1.06f;
            result.stride_scale *= 1.10f;
            result.arm_swing_scale *= 1.16f;
            result.idle_lean = 0.060f;
            break;
        case CC_NPC_ROLE_MERCHANT:
            result.equipment = CC_NPC_EQUIPMENT_APRON |
                               CC_NPC_EQUIPMENT_SATCHEL |
                               CC_NPC_EQUIPMENT_HEADWEAR;
            result.outer = Blend(result.outer, result.accent, 0.36f);
            result.gait_cadence_scale *= 0.92f;
            result.stride_scale *= 0.88f;
            result.arm_swing_scale *= 1.12f;
            result.idle_lean = -0.010f;
            break;
        case CC_NPC_ROLE_LABORER:
            result.equipment = CC_NPC_EQUIPMENT_APRON |
                               CC_NPC_EQUIPMENT_TOOL;
            result.muscularity = fmaxf(result.muscularity, 0.68f);
            result.body_mass = fmaxf(result.body_mass, 0.96f);
            result.gait_cadence_scale *= 0.84f;
            result.bob_scale *= 0.68f;
            result.arm_swing_scale *= 0.80f;
            result.idle_lean = 0.035f;
            break;
        case CC_NPC_ROLE_TRAVELLER:
            result.equipment = CC_NPC_EQUIPMENT_MANTLE |
                               CC_NPC_EQUIPMENT_PACK |
                                ((Sample(seed, 17U) & 1U) != 0U ?
                                 CC_NPC_EQUIPMENT_HEADWEAR : 0U);
            result.stride_scale *= 1.02f;
            break;
        case CC_NPC_ROLE_REFUGEE:
            result.equipment = CC_NPC_EQUIPMENT_MANTLE |
                               CC_NPC_EQUIPMENT_PACK;
            result.outer = Shade(result.outer, 0.76f);
            result.underlayer = Shade(result.underlayer, 0.84f);
            result.gait_cadence_scale *= 0.82f;
            result.stride_scale *= 0.82f;
            result.bob_scale *= 0.62f;
            result.arm_swing_scale *= 0.74f;
            result.idle_lean = -0.040f;
            break;
        case CC_NPC_ROLE_SCOUT:
            result.equipment = CC_NPC_EQUIPMENT_MANTLE |
                               CC_NPC_EQUIPMENT_SATCHEL |
                               CC_NPC_EQUIPMENT_HEADWEAR |
                               CC_NPC_EQUIPMENT_TOOL;
            result.stature = fminf(result.stature, 1.02f);
            result.body_mass = fminf(result.body_mass, 1.00f);
            result.gait_cadence_scale *= 1.15f;
            result.stride_scale *= 1.10f;
            result.bob_scale *= 1.08f;
            result.idle_lean = 0.045f;
            break;
        case CC_NPC_ROLE_HEALER:
            result.equipment = CC_NPC_EQUIPMENT_APRON |
                               CC_NPC_EQUIPMENT_SATCHEL |
                               CC_NPC_EQUIPMENT_MANTLE;
            result.outer = Blend(result.outer, (Color){83, 118, 105, 255},
                                 0.62f);
            result.underlayer = (Color){186, 179, 154, 255};
            result.gait_cadence_scale *= 0.90f;
            result.stride_scale *= 0.92f;
            result.arm_swing_scale *= 0.88f;
            result.idle_lean = -0.015f;
            break;
        case CC_NPC_ROLE_COUNT:
        default:
            break;
    }
    return result;
}

const char *CcNpcRoleName(CcNpcRole role)
{
    switch (role) {
        case CC_NPC_ROLE_WAYFARER: return "WAYFARER";
        case CC_NPC_ROLE_GUARD: return "GUARD";
        case CC_NPC_ROLE_RAIDER: return "RAIDER";
        case CC_NPC_ROLE_MERCHANT: return "MERCHANT";
        case CC_NPC_ROLE_LABORER: return "LABORER";
        case CC_NPC_ROLE_TRAVELLER: return "TRAVELLER";
        case CC_NPC_ROLE_REFUGEE: return "REFUGEE";
        case CC_NPC_ROLE_SCOUT: return "SCOUT";
        case CC_NPC_ROLE_HEALER: return "HEALER";
        case CC_NPC_ROLE_COUNT:
        default: return "PERSON";
    }
}

bool CcNpcAppearanceEqual(const CcNpcAppearance *first,
                          const CcNpcAppearance *second)
{
    if (first == NULL || second == NULL) return false;
    return first->seed == second->seed && first->role == second->role &&
           first->equipment == second->equipment &&
           first->stature == second->stature &&
           first->body_mass == second->body_mass &&
           first->muscularity == second->muscularity &&
           first->shoulder_scale == second->shoulder_scale &&
           first->head_width == second->head_width &&
           first->head_depth == second->head_depth &&
           first->age == second->age &&
           first->gait_cadence_scale == second->gait_cadence_scale &&
           first->stride_scale == second->stride_scale &&
           first->bob_scale == second->bob_scale &&
           first->idle_lean == second->idle_lean &&
           first->arm_swing_scale == second->arm_swing_scale &&
           first->skin_tone == second->skin_tone &&
           first->hair_style == second->hair_style &&
           first->beard_style == second->beard_style &&
           first->garment_style == second->garment_style &&
           ColorToInt(first->skin) == ColorToInt(second->skin) &&
           ColorToInt(first->hair) == ColorToInt(second->hair) &&
           ColorToInt(first->underlayer) == ColorToInt(second->underlayer) &&
           ColorToInt(first->outer) == ColorToInt(second->outer) &&
           ColorToInt(first->trousers) == ColorToInt(second->trousers) &&
           ColorToInt(first->leather) == ColorToInt(second->leather) &&
           ColorToInt(first->metal) == ColorToInt(second->metal) &&
           ColorToInt(first->accent) == ColorToInt(second->accent);
}
