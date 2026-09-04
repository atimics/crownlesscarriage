#ifndef CROWNLESS_HERALDRY_H
#define CROWNLESS_HERALDRY_H

#include "sim/cc_sim.h"

#include "raylib.h"

#include <stdint.h>

typedef enum CcHeraldryFamily {
    CC_HERALDRY_VERDANT,
    CC_HERALDRY_EMBER,
    CC_HERALDRY_INDIGO,
    CC_HERALDRY_SPECIAL
} CcHeraldryFamily;

typedef enum CcBannerShape {
    CC_BANNER_COURT,
    CC_BANNER_FACTORS,
    CC_BANNER_COMMONS,
    CC_BANNER_RAGGED
} CcBannerShape;

typedef enum CcBannerMark {
    CC_BANNER_MARK_ROAD_GRAIN,
    CC_BANNER_MARK_IRON_WALL,
    CC_BANNER_MARK_DEEP_STAR,
    CC_BANNER_MARK_CINDER_TITHE,
    CC_BANNER_MARK_BANDIT_PATCH,
    CC_BANNER_MARK_IRON_LEDGER,
    CC_BANNER_MARK_DRAGON_CROWN
} CcBannerMark;

typedef enum CcSpecialBanner {
    CC_SPECIAL_BANNER_CINDER_TITHE,
    CC_SPECIAL_BANNER_BANDITS,
    CC_SPECIAL_BANNER_IRON_LEDGER,
    CC_SPECIAL_BANNER_VARKESH
} CcSpecialBanner;

typedef struct CcHeraldryStyle {
    CcHeraldryFamily family;
    CcBannerShape shape;
    CcBannerMark mark;
    Color kingdom;
    Color field;
    Color accent;
    Color device;
    Color shadow;
    int32_t wear;
} CcHeraldryStyle;

CcHeraldryStyle CcHeraldryForFaction(const CcSim *sim, CcId kingdom_id,
                                      CcFactionKind faction);
CcHeraldryStyle CcHeraldryForKingdom(const CcSim *sim, CcId kingdom_id);
CcFactionKind CcHeraldryLeadingFaction(const CcSim *sim, CcId kingdom_id);
CcHeraldryStyle CcHeraldryForSpecial(CcSpecialBanner banner);

#endif
