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

CcFaceRecipe CcNpcFaceRecipe(const CcNpcAppearance *appearance)
{
    if (appearance == NULL) return (CcFaceRecipe){0};
    Color headwear = appearance->headwear_style == 0U ?
                     appearance->metal : appearance->outer;
    return (CcFaceRecipe){
        .seed = appearance->seed,
        .width = appearance->head_width,
        .depth = appearance->head_depth,
        .age = appearance->age,
        .hair_style = appearance->hair_style,
        .beard_style = appearance->beard_style,
        .nose_style = appearance->nose_style,
        .scar_style = appearance->scar_style,
        .headwear_style = appearance->headwear_style,
        .headwear = (appearance->equipment &
                     CC_NPC_EQUIPMENT_HEADWEAR) != 0U,
        .skin = appearance->skin,
        .skin_shadow = Shade(appearance->skin, 0.70f),
        .hair = appearance->hair,
        .ink = Shade(appearance->hair, 0.48f),
        .headwear_color = headwear,
        .accent = appearance->accent,
    };
}

typedef void (*PortraitBlockPainter)(void *context, int32_t grid_x,
                                     int32_t grid_y, int32_t grid_width,
                                     int32_t grid_height, Color color);

typedef struct ScreenPortraitCanvas {
    int32_t x;
    int32_t y;
    int32_t pixel;
} ScreenPortraitCanvas;

static void PaintScreenBlock(void *context, int32_t grid_x, int32_t grid_y,
                             int32_t grid_width, int32_t grid_height,
                             Color color)
{
    const ScreenPortraitCanvas *canvas = context;
    DrawRectangle(canvas->x + grid_x * canvas->pixel,
                  canvas->y + grid_y * canvas->pixel,
                  grid_width * canvas->pixel,
                  grid_height * canvas->pixel, color);
}

static void PaintPortraitFeatures(const CcFaceRecipe *face,
                                  CcNpcPortraitExpression expression,
                                  void *context,
                                  PortraitBlockPainter paint)
{
    Color shadow = face->skin_shadow;
    Color ink = face->ink;
    int32_t brow_y = expression == CC_NPC_PORTRAIT_FOCUSED ? 7 : 6;
    paint(context, 6, brow_y, 3, 1, ink);
    paint(context, 11, brow_y, 3, 1, ink);
    if (expression == CC_NPC_PORTRAIT_HURT) {
        paint(context, 7, 8, 1, 1, ink);
        paint(context, 12, 9, 1, 1, ink);
    } else {
        paint(context, 7, 8, 1, 1, ink);
        paint(context, 12, 8, 1, 1, ink);
    }
    switch (face->nose_style % 4U) {
        case 0: paint(context, 9, 10, 2, 1, shadow); break;
        case 1: paint(context, 9, 10, 2, 2, shadow); break;
        case 2: paint(context, 8, 11, 4, 1, shadow); break;
        case 3: paint(context, 9, 9, 2, 3, shadow); break;
    }
    if (face->scar_style == 1U) {
        paint(context, 13, 8, 1, 4, Shade(face->skin, 0.52f));
    } else if (face->scar_style == 2U) {
        paint(context, 6, 8, 1, 4, Shade(face->skin, 0.52f));
    } else if (face->scar_style == 3U) {
        paint(context, 12, 11, 3, 1, Shade(face->skin, 0.52f));
    }
    if (face->age > 0.68f) {
        paint(context, 5, 11, 2, 1, shadow);
        paint(context, 13, 11, 2, 1, shadow);
    }
    if (expression == CC_NPC_PORTRAIT_TALKING) {
        paint(context, 8, 13, 4, 2, ink);
        paint(context, 9, 13, 2, 1, Shade(face->skin, 1.18f));
    } else if (expression == CC_NPC_PORTRAIT_HURT) {
        paint(context, 8, 14, 4, 1, ink);
        paint(context, 8, 13, 1, 1, ink);
    } else {
        paint(context, 8, 13, 4, 1, ink);
    }
}

static void PaintPortraitFace(const CcFaceRecipe *face,
                              CcNpcPortraitExpression expression,
                              bool crowned, void *context,
                              PortraitBlockPainter paint)
{
    Color shadow = face->skin_shadow;
    int32_t face_left = face->width < 1.0f ? 6 :
                        face->width > 1.045f ? 4 : 5;
    int32_t face_width = 20 - face_left * 2;
    paint(context, face_left - 1, 7, 1, 5, shadow);
    paint(context, face_left + face_width, 7, 1, 5, shadow);
    paint(context, face_left, 4, face_width, 10, face->skin);
    paint(context, face_left + 1, 14, face_width - 2, 2,
          face->skin);
    paint(context, 7, 16, 6, 1, shadow);

    paint(context, 5, 2, 10, 3, face->hair);
    switch (face->hair_style % 8U) {
        case 0:
            paint(context, 5, 4, 2, 5, face->hair);
            paint(context, 13, 4, 2, 3, face->hair);
            break;
        case 1:
            paint(context, 4, 4, 2, 10, face->hair);
            paint(context, 14, 4, 2, 10, face->hair);
            break;
        case 2:
            paint(context, 8, 0, 4, 3, face->hair);
            paint(context, 5, 4, 2, 5, face->hair);
            break;
        case 3:
            paint(context, 4, 4, 7, 2, face->hair);
            paint(context, 4, 5, 3, 4, face->hair);
            paint(context, 12, 3, 4, 2, face->hair);
            break;
        case 4:
            paint(context, 3, 4, 2, 11, face->hair);
            paint(context, 15, 4, 2, 11, face->hair);
            break;
        case 5:
            paint(context, 5, 4, 7, 2, face->hair);
            paint(context, 12, 4, 3, 4, face->hair);
            break;
        case 6:
            paint(context, 9, 0, 2, 4, face->hair);
            paint(context, 5, 4, 2, 4, face->hair);
            break;
        case 7:
        default:
            paint(context, 3, 4, 3, 8, face->hair);
            paint(context, 14, 4, 3, 8, face->hair);
            break;
    }
    switch (face->beard_style % 4U) {
        case 1:
            paint(context, 7, 12, 6, 1, face->hair);
            break;
        case 2:
            paint(context, 7, 13, 6, 2, face->hair);
            paint(context, 9, 15, 2, 2, face->hair);
            break;
        case 3:
            paint(context, 6, 12, 8, 3, face->hair);
            paint(context, 7, 15, 6, 2, face->hair);
            break;
        default:
            break;
    }

    PaintPortraitFeatures(face, expression, context, paint);

    if (face->headwear) {
        Color headwear = face->headwear_color;
        switch (face->headwear_style % 4U) {
            case 0:
                paint(context, 4, 1, 12, 4, headwear);
                paint(context, 3, 4, 14, 1, Shade(headwear, 0.70f));
                break;
            case 1:
                paint(context, 7, 0, 7, 4, headwear);
                paint(context, 2, 3, 16, 1, Shade(headwear, 0.76f));
                break;
            case 2:
                paint(context, 4, 1, 12, 3, headwear);
                paint(context, 3, 3, 3, 10, headwear);
                paint(context, 14, 3, 3, 10, headwear);
                break;
            case 3:
            default:
                paint(context, 4, 1, 12, 3, headwear);
                paint(context, 4, 4, 12, 2, Shade(headwear, 0.78f));
                break;
        }
    }

    if (crowned) {
        Color gold = (Color){224, 177, 78, 255};
        paint(context, 7, 0, 6, 1, gold);
        paint(context, 7, 0, 1, 2, gold);
        paint(context, 9, 0, 1, 2, gold);
        paint(context, 12, 0, 1, 2, gold);
    }
}

void CcNpcDrawPixelPortrait(const CcNpcAppearance *appearance,
                            Rectangle bounds,
                            CcNpcPortraitExpression expression,
                            bool crowned)
{
    if (appearance == NULL) return;
    int32_t pixel = bounds.width >= 60.0f && bounds.height >= 70.0f ? 3 : 2;
    int32_t width = pixel * 20;
    int32_t height = pixel * 24;
    int32_t x = (int32_t)bounds.x + ((int32_t)bounds.width - width) / 2;
    int32_t y = (int32_t)bounds.y + ((int32_t)bounds.height - height) / 2;
    ScreenPortraitCanvas canvas = {x, y, pixel};
    DrawRectangle((int32_t)bounds.x, (int32_t)bounds.y,
                  (int32_t)bounds.width, (int32_t)bounds.height,
                  (Color){8, 16, 21, 255});
    DrawRectangleLines((int32_t)bounds.x, (int32_t)bounds.y,
                       (int32_t)bounds.width, (int32_t)bounds.height,
                       Shade(appearance->accent, 0.88f));
    DrawRectangle(x, y, width, height, (Color){17, 28, 32, 255});
    PaintScreenBlock(&canvas, 2, 18, 16, 6, appearance->outer);
    PaintScreenBlock(&canvas, 0, 21, 20, 3,
                     Shade(appearance->outer, 0.68f));
    PaintScreenBlock(&canvas, 5, 19, 10, 2, appearance->underlayer);
    PaintScreenBlock(&canvas, 8, 14, 4, 6,
                     Shade(appearance->skin, 0.70f));
    CcFaceRecipe face = CcNpcFaceRecipe(appearance);
    PaintPortraitFace(&face, expression, crowned, &canvas,
                      PaintScreenBlock);
}

CcNpcAppearance CcNpcHeroPortraitAppearance(
    const CcNpcAppearance *appearance)
{
    CcNpcAppearance result = appearance != NULL ? *appearance :
        CcNpcAppearanceGenerate(UINT32_C(0xc0a71a9e),
                                CC_NPC_ROLE_WAYFARER,
                                (Color){181, 135, 49, 255});
    result.skin = (Color){177, 131, 93, 255};
    result.hair = (Color){27, 31, 32, 255};
    result.underlayer = (Color){47, 108, 106, 255};
    result.outer = (Color){111, 48, 55, 255};
    result.accent = (Color){181, 135, 49, 255};
    result.hair_style = 3U;
    result.beard_style = 0U;
    return result;
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
    result.stature = 0.94f + Unit(seed, 0U) * 0.14f;
    result.body_mass = 0.91f + Unit(seed, 1U) * 0.24f;
    result.muscularity = 0.34f + Unit(seed, 2U) * 0.58f;
    result.shoulder_scale = 0.94f + Unit(seed, 3U) * 0.18f;
    result.head_width = 0.96f + Unit(seed, 4U) * 0.12f;
    result.head_depth = 0.95f + Unit(seed, 5U) * 0.12f;
    result.age = 0.12f + Unit(seed, 6U) * 0.83f;
    result.gait_cadence_scale = 0.90f + Unit(seed, 18U) * 0.22f;
    result.stride_scale = 0.88f + Unit(seed, 19U) * 0.24f;
    result.bob_scale = 0.76f + Unit(seed, 20U) * 0.36f;
    result.idle_lean = (Unit(seed, 21U) - 0.5f) * 0.050f;
    result.arm_swing_scale = 0.84f + Unit(seed, 22U) * 0.32f;
    result.skin_tone = (uint8_t)(Sample(seed, 7U) %
        (uint32_t)(sizeof(skin_palette) / sizeof(skin_palette[0])));
    result.hair_style = (uint8_t)(Sample(seed, 8U) % 8U);
    result.beard_style = (uint8_t)(Sample(seed, 9U) % 4U);
    result.nose_style = (uint8_t)(Sample(seed, 23U) % 4U);
    uint32_t scar_sample = Sample(seed, 24U) % 8U;
    result.scar_style = scar_sample < 3U ? (uint8_t)(scar_sample + 1U) : 0U;
    result.headwear_style = (uint8_t)(Sample(seed, 25U) % 4U);
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
    result.outer = Shade(result.outer, 1.08f);
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
            result.body_mass = fmaxf(result.body_mass, 1.02f);
            result.shoulder_scale = fmaxf(result.shoulder_scale, 1.09f);
            result.outer = Shade(result.accent, 0.76f);
            result.gait_cadence_scale *= 0.94f;
            result.stride_scale *= 0.88f;
            result.bob_scale *= 0.64f;
            result.arm_swing_scale *= 0.72f;
            result.idle_lean = 0.010f;
            result.headwear_style = 0U;
            break;
        case CC_NPC_ROLE_RAIDER:
            result.equipment = CC_NPC_EQUIPMENT_MANTLE |
                               CC_NPC_EQUIPMENT_ARMOR |
                               CC_NPC_EQUIPMENT_TOOL;
            result.outer = Blend(result.outer, (Color){104, 46, 51, 255},
                                 0.68f);
            result.muscularity = fmaxf(result.muscularity, 0.56f);
            result.shoulder_scale = fmaxf(result.shoulder_scale, 1.06f);
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
            result.underlayer = Blend(result.underlayer,
                                      (Color){132, 141, 126, 255}, 0.68f);
            result.body_mass = fmaxf(result.body_mass, 1.04f);
            result.head_width = fmaxf(result.head_width, 1.02f);
            result.gait_cadence_scale *= 0.92f;
            result.stride_scale *= 0.88f;
            result.arm_swing_scale *= 1.12f;
            result.idle_lean = -0.010f;
            result.headwear_style = 1U +
                (uint8_t)(Sample(seed, 25U) % 3U);
            break;
        case CC_NPC_ROLE_LABORER:
            result.equipment = CC_NPC_EQUIPMENT_APRON |
                               CC_NPC_EQUIPMENT_TOOL;
            result.muscularity = fmaxf(result.muscularity, 0.68f);
            result.body_mass = fmaxf(result.body_mass, 1.04f);
            result.shoulder_scale = fmaxf(result.shoulder_scale, 1.04f);
            result.underlayer = Blend(result.underlayer,
                                      (Color){111, 112, 99, 255}, 0.58f);
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
            result.outer = Blend(result.outer, result.accent, 0.34f);
            result.underlayer = Shade(result.underlayer, 1.08f);
            result.stride_scale *= 1.02f;
            result.headwear_style = (Sample(seed, 25U) & 1U) != 0U ? 1U : 3U;
            break;
        case CC_NPC_ROLE_REFUGEE:
            result.equipment = CC_NPC_EQUIPMENT_MANTLE |
                               CC_NPC_EQUIPMENT_PACK;
            result.outer = Shade(result.outer, 0.76f);
            result.underlayer = Shade(result.underlayer, 0.84f);
            result.body_mass *= 0.92f;
            result.shoulder_scale *= 0.94f;
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
            result.shoulder_scale = fminf(result.shoulder_scale, 1.02f);
            result.outer = Blend(result.outer, result.accent, 0.48f);
            result.gait_cadence_scale *= 1.15f;
            result.stride_scale *= 1.10f;
            result.bob_scale *= 1.08f;
            result.idle_lean = 0.045f;
            result.headwear_style = 2U;
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
           first->nose_style == second->nose_style &&
           first->scar_style == second->scar_style &&
           first->headwear_style == second->headwear_style &&
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
