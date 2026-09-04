#include "client/cc_heraldry.h"

#include <stddef.h>

static const Color HERALDRY_PARCHMENT = {224, 205, 164, 255};
static const Color HERALDRY_GOLD = {206, 162, 78, 255};
static const Color HERALDRY_INK = {35, 31, 38, 255};

static unsigned char ScaleChannel(unsigned char channel, unsigned int percent)
{
    return (unsigned char)(((unsigned int)channel * percent) / 100U);
}

static Color HeraldryShade(Color color, unsigned int percent)
{
    return (Color){
        ScaleChannel(color.r, percent),
        ScaleChannel(color.g, percent),
        ScaleChannel(color.b, percent),
        color.a,
    };
}

static Color HeraldryBlend(Color first, Color second, unsigned int second_weight)
{
    if (second_weight > 100U) second_weight = 100U;
    unsigned int first_weight = 100U - second_weight;
    return (Color){
        (unsigned char)(((unsigned int)first.r * first_weight +
                         (unsigned int)second.r * second_weight) / 100U),
        (unsigned char)(((unsigned int)first.g * first_weight +
                         (unsigned int)second.g * second_weight) / 100U),
        (unsigned char)(((unsigned int)first.b * first_weight +
                         (unsigned int)second.b * second_weight) / 100U),
        255,
    };
}

static const CcKingdom *HeraldryKingdom(const CcSim *sim, CcId kingdom_id,
                                        int32_t *slot)
{
    if (slot != NULL) *slot = -1;
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id != kingdom_id) continue;
        if (slot != NULL) *slot = i;
        return &sim->kingdoms[i];
    }
    return NULL;
}

static const CcFaction *HeraldryFaction(const CcSim *sim, CcId kingdom_id,
                                        CcFactionKind kind)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *faction = &sim->factions[i];
        if (faction->kingdom_id == kingdom_id && faction->kind == kind) {
            return faction;
        }
    }
    return NULL;
}

static CcBannerMark CallingMark(CcKingdomCalling calling)
{
    switch (calling) {
        case CC_KINGDOM_CALLING_ROAD:
            return CC_BANNER_MARK_ROAD_GRAIN;
        case CC_KINGDOM_CALLING_IRON:
            return CC_BANNER_MARK_IRON_WALL;
        case CC_KINGDOM_CALLING_DEEP:
        case CC_KINGDOM_CALLING_COUNT:
            return CC_BANNER_MARK_DEEP_STAR;
    }
    return CC_BANNER_MARK_DEEP_STAR;
}

CcFactionKind CcHeraldryLeadingFaction(const CcSim *sim, CcId kingdom_id)
{
    CcFactionKind leading = CC_FACTION_CROWN;
    int32_t best_score = -1;
    if (sim == NULL) return leading;
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *faction = &sim->factions[i];
        if (faction->kingdom_id != kingdom_id) continue;
        int32_t score = faction->power * 2 + faction->support;
        if (score > best_score) {
            best_score = score;
            leading = faction->kind;
        }
    }
    return leading;
}

CcHeraldryStyle CcHeraldryForFaction(const CcSim *sim, CcId kingdom_id,
                                      CcFactionKind faction_kind)
{
    int32_t kingdom_slot = -1;
    const CcKingdom *kingdom = HeraldryKingdom(sim, kingdom_id, &kingdom_slot);
    Color kingdom_color = kingdom != NULL ?
        (Color){kingdom->color_r, kingdom->color_g, kingdom->color_b, 255} :
        (Color){112, 105, 112, 255};
    CcHeraldryStyle style = {
        .family = kingdom_slot == 0 ? CC_HERALDRY_VERDANT :
                  kingdom_slot == 1 ? CC_HERALDRY_EMBER :
                  kingdom_slot == 2 ? CC_HERALDRY_INDIGO :
                                      CC_HERALDRY_SPECIAL,
        .shape = faction_kind == CC_FACTION_GUILD ? CC_BANNER_FACTORS :
                 faction_kind == CC_FACTION_COMMONS ? CC_BANNER_COMMONS :
                                                       CC_BANNER_COURT,
        .mark = CallingMark(CcSimKingdomCalling(sim, kingdom_id)),
        .kingdom = kingdom_color,
        .field = kingdom_color,
        .accent = HERALDRY_GOLD,
        .device = HERALDRY_PARCHMENT,
        .shadow = HeraldryShade(kingdom_color, 48U),
        .wear = 0,
    };
    if (faction_kind == CC_FACTION_GUILD) {
        style.field = HERALDRY_PARCHMENT;
        style.accent = kingdom_color;
        style.device = HERALDRY_INK;
        style.shadow = HeraldryBlend(HERALDRY_PARCHMENT, HERALDRY_INK, 42U);
    } else if (faction_kind == CC_FACTION_COMMONS) {
        style.field = HeraldryBlend(kingdom_color, HERALDRY_INK, 64U);
        style.accent = kingdom_color;
        style.device = HERALDRY_PARCHMENT;
        style.shadow = HeraldryShade(style.field, 62U);
    }

    const CcFaction *faction = HeraldryFaction(
        sim, kingdom_id, faction_kind);
    if (kingdom != NULL && kingdom->legitimacy < 45) style.wear += 1;
    if (faction != NULL && faction->support < 40) style.wear += 1;
    return style;
}

CcHeraldryStyle CcHeraldryForKingdom(const CcSim *sim, CcId kingdom_id)
{
    return CcHeraldryForFaction(
        sim, kingdom_id, CcHeraldryLeadingFaction(sim, kingdom_id));
}

CcHeraldryStyle CcHeraldryForSpecial(CcSpecialBanner banner)
{
    switch (banner) {
        case CC_SPECIAL_BANNER_CINDER_TITHE:
            return (CcHeraldryStyle){
                .family = CC_HERALDRY_SPECIAL,
                .shape = CC_BANNER_RAGGED,
                .mark = CC_BANNER_MARK_CINDER_TITHE,
                .kingdom = {178, 79, 45, 255},
                .field = {39, 34, 36, 255},
                .accent = {194, 82, 39, 255},
                .device = {218, 198, 157, 255},
                .shadow = {20, 18, 21, 255},
                .wear = 2,
            };
        case CC_SPECIAL_BANNER_BANDITS:
            return (CcHeraldryStyle){
                .family = CC_HERALDRY_SPECIAL,
                .shape = CC_BANNER_RAGGED,
                .mark = CC_BANNER_MARK_BANDIT_PATCH,
                .kingdom = {126, 74, 61, 255},
                .field = {46, 39, 42, 255},
                .accent = {153, 71, 60, 255},
                .device = {205, 187, 151, 255},
                .shadow = {24, 22, 26, 255},
                .wear = 2,
            };
        case CC_SPECIAL_BANNER_IRON_LEDGER:
            return (CcHeraldryStyle){
                .family = CC_HERALDRY_SPECIAL,
                .shape = CC_BANNER_COURT,
                .mark = CC_BANNER_MARK_IRON_LEDGER,
                .kingdom = {126, 124, 123, 255},
                .field = {218, 205, 176, 255},
                .accent = {83, 82, 86, 255},
                .device = {31, 29, 34, 255},
                .shadow = {116, 109, 101, 255},
                .wear = 0,
            };
        case CC_SPECIAL_BANNER_VARKESH:
            return (CcHeraldryStyle){
                .family = CC_HERALDRY_SPECIAL,
                .shape = CC_BANNER_RAGGED,
                .mark = CC_BANNER_MARK_DRAGON_CROWN,
                .kingdom = {193, 128, 51, 255},
                .field = {35, 31, 35, 255},
                .accent = {190, 139, 54, 255},
                .device = {185, 65, 42, 255},
                .shadow = {17, 16, 20, 255},
                .wear = 1,
            };
    }
    return CcHeraldryForSpecial(CC_SPECIAL_BANNER_BANDITS);
}
