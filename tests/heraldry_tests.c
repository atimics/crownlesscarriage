#include "client/cc_heraldry.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static bool SameColor(Color color, unsigned char red, unsigned char green,
                      unsigned char blue)
{
    return color.r == red && color.g == green && color.b == blue;
}

static CcFaction *FactionFor(CcSim *sim, CcId kingdom_id, CcFactionKind kind)
{
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        CcFaction *faction = &sim->factions[i];
        if (faction->kingdom_id == kingdom_id && faction->kind == kind) {
            return faction;
        }
    }
    return NULL;
}

int main(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x48455241));

    CcId verdant_id = sim.kingdoms[0].id;
    CcId ember_id = sim.kingdoms[1].id;
    CcId indigo_id = sim.kingdoms[2].id;
    CcHeraldryStyle verdant = CcHeraldryForFaction(
        &sim, verdant_id, CC_FACTION_CROWN);
    CcHeraldryStyle ember = CcHeraldryForFaction(
        &sim, ember_id, CC_FACTION_CROWN);
    CcHeraldryStyle indigo = CcHeraldryForFaction(
        &sim, indigo_id, CC_FACTION_CROWN);
    if (verdant.family != CC_HERALDRY_VERDANT ||
        ember.family != CC_HERALDRY_EMBER ||
        indigo.family != CC_HERALDRY_INDIGO ||
        !SameColor(verdant.kingdom, 54U, 173U, 146U) ||
        !SameColor(ember.kingdom, 210U, 101U, 71U) ||
        !SameColor(indigo.kingdom, 102U, 123U, 205U)) {
        (void)fprintf(stderr, "kingdom heraldry colors changed\n");
        return 1;
    }

    CcHeraldryStyle court = CcHeraldryForFaction(
        &sim, verdant_id, CC_FACTION_CROWN);
    CcHeraldryStyle factors = CcHeraldryForFaction(
        &sim, verdant_id, CC_FACTION_GUILD);
    CcHeraldryStyle commons = CcHeraldryForFaction(
        &sim, verdant_id, CC_FACTION_COMMONS);
    if (court.shape != CC_BANNER_COURT ||
        factors.shape != CC_BANNER_FACTORS ||
        commons.shape != CC_BANNER_COMMONS ||
        court.field.r == factors.field.r ||
        factors.field.r == commons.field.r) {
        (void)fprintf(stderr, "faction banner shapes are not distinct\n");
        return 1;
    }

    if (court.mark != CC_BANNER_MARK_ROAD_GRAIN ||
        ember.mark != CC_BANNER_MARK_IRON_WALL ||
        indigo.mark != CC_BANNER_MARK_DEEP_STAR) {
        (void)fprintf(stderr, "initial kingdom calling marks changed\n");
        return 1;
    }
    sim.settlements[4].kingdom_id = verdant_id;
    if (CcHeraldryForFaction(&sim, verdant_id, CC_FACTION_CROWN).mark !=
        CC_BANNER_MARK_DEEP_STAR) {
        (void)fprintf(stderr, "territory did not update the calling mark\n");
        return 1;
    }

    CcFaction *court_faction = FactionFor(
        &sim, verdant_id, CC_FACTION_CROWN);
    CcFaction *factor_faction = FactionFor(
        &sim, verdant_id, CC_FACTION_GUILD);
    CcFaction *commons_faction = FactionFor(
        &sim, verdant_id, CC_FACTION_COMMONS);
    if (court_faction == NULL || factor_faction == NULL ||
        commons_faction == NULL) {
        (void)fprintf(stderr, "kingdom factions are missing\n");
        return 1;
    }
    court_faction->power = 10;
    court_faction->support = 10;
    factor_faction->power = 20;
    factor_faction->support = 20;
    commons_faction->power = 100;
    commons_faction->support = 100;
    if (CcHeraldryLeadingFaction(&sim, verdant_id) != CC_FACTION_COMMONS ||
        CcHeraldryForKingdom(&sim, verdant_id).shape != CC_BANNER_COMMONS) {
        (void)fprintf(stderr, "leading faction did not update the main flag\n");
        return 1;
    }

    sim.kingdoms[0].legitimacy = 20;
    commons_faction->support = 20;
    if (CcHeraldryForFaction(&sim, verdant_id,
                             CC_FACTION_COMMONS).wear != 2) {
        (void)fprintf(stderr, "low support did not weather the banner\n");
        return 1;
    }

    CcHeraldryStyle cinder = CcHeraldryForSpecial(
        CC_SPECIAL_BANNER_CINDER_TITHE);
    CcHeraldryStyle bandits = CcHeraldryForSpecial(
        CC_SPECIAL_BANNER_BANDITS);
    CcHeraldryStyle ledger = CcHeraldryForSpecial(
        CC_SPECIAL_BANNER_IRON_LEDGER);
    CcHeraldryStyle dragon = CcHeraldryForSpecial(
        CC_SPECIAL_BANNER_VARKESH);
    if (cinder.mark != CC_BANNER_MARK_CINDER_TITHE ||
        bandits.mark != CC_BANNER_MARK_BANDIT_PATCH ||
        ledger.mark != CC_BANNER_MARK_IRON_LEDGER ||
        dragon.mark != CC_BANNER_MARK_DRAGON_CROWN ||
        cinder.shape != CC_BANNER_RAGGED ||
        ledger.shape != CC_BANNER_COURT) {
        (void)fprintf(stderr, "special power banners changed\n");
        return 1;
    }

    (void)printf("dynamic kingdom and faction heraldry passed\n");
    return 0;
}
