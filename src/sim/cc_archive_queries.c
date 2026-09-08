#include "sim/cc_archive_internal.h"
#include "sim/cc_food_economy_internal.h"

#include <string.h>

static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }

int32_t CcArchiveSpareGrain(const CcSim *sim, const CcSettlement *place)
{
    if (place == NULL) return 0;
    if (sim->schema_version < 58U) return MaximumI32(
        0, place->stock[CC_GOOD_WHEAT] - CcEconomyWeeklyFoodUse(sim, place) * 2);
    int32_t rations = CcEconomyNutritionRations(place->stock, CC_NUTRITION_CIVILIAN);
    int32_t spare = MaximumI32(0, rations - CcEconomyWeeklyFoodUse(sim, place) * 2);
    /* Two wheat units provide one civilian ration. */
    return MinimumI32(place->stock[CC_GOOD_WHEAT], spare * 2);
}

const CcSettlement *CcArchiveSeat(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    const CcSettlement *fallback = NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) continue;
        if (strcmp(place->name, "Gloamgate") == 0) return place;
        if (fallback == NULL &&
            (place->function == CC_SETTLEMENT_MARKET ||
             place->function == CC_SETTLEMENT_CAPITAL)) {
            fallback = place;
        }
    }
    return fallback;
}

CcMaterialChainSnapshot CcSimMaterialChainSnapshot(const CcSim *sim)
{
    CcMaterialChainSnapshot snapshot = {0};
    const CcSettlement *place = CcArchiveSeat(sim);
    if (sim == NULL || place == NULL) {
        snapshot.blocker = CC_MATERIAL_CHAIN_NO_SCRIBES;
        return snapshot;
    }
    snapshot.scriptorium_id = place->id;
    snapshot.scribes = sim->archives.scribes;
    snapshot.wheat = place->stock[CC_GOOD_WHEAT];
    snapshot.paper = place->stock[CC_GOOD_PAPER];
    snapshot.tools = place->stock[CC_GOOD_TOOLS];
    snapshot.iron = place->stock[CC_GOOD_IRON];
    snapshot.incoming_tools = CcSimIncomingGood(
        sim, place->id, CC_GOOD_TOOLS);
    snapshot.incoming_iron = CcSimIncomingGood(
        sim, place->id, CC_GOOD_IRON);
    bool binding_available = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *vault = &sim->settlements[i];
        if (CcSettlementIsAbandoned(vault)) continue;
        int64_t gold = (int64_t)snapshot.gold +
            vault->stock[CC_GOOD_GOLD];
        int64_t gems = (int64_t)snapshot.gems +
            vault->stock[CC_GOOD_GEMS];
        snapshot.gold = gold > CC_SIM_MAX_UNITS ?
            CC_SIM_MAX_UNITS : (int32_t)gold;
        snapshot.gems = gems > CC_SIM_MAX_UNITS ?
            CC_SIM_MAX_UNITS : (int32_t)gems;
        if (vault->stock[CC_GOOD_GOLD] > 0 &&
            vault->stock[CC_GOOD_GEMS] > 0) {
            binding_available = true;
        }
    }
    int32_t scribe_grain = CcArchiveSpareGrain(sim, place);
    snapshot.blocker = snapshot.scribes <= 0 ?
        CC_MATERIAL_CHAIN_NO_SCRIBES :
        !binding_available ? CC_MATERIAL_CHAIN_BINDING :
        snapshot.tools <= 0 ? CC_MATERIAL_CHAIN_TOOLS :
        scribe_grain < 2 ? CC_MATERIAL_CHAIN_GRAIN :
        snapshot.paper <= 0 ? CC_MATERIAL_CHAIN_PAPER :
        CC_MATERIAL_CHAIN_READY;
    return snapshot;
}

const char *CcMaterialChainBlockerName(CcMaterialChainBlocker blocker)
{
    switch (blocker) {
        case CC_MATERIAL_CHAIN_READY: return "ready";
        case CC_MATERIAL_CHAIN_NO_SCRIBES: return "scribes";
        case CC_MATERIAL_CHAIN_GRAIN: return "grain";
        case CC_MATERIAL_CHAIN_PAPER: return "paper";
        case CC_MATERIAL_CHAIN_TOOLS: return "tools";
        case CC_MATERIAL_CHAIN_BINDING: return "binding";
    }
    return "unknown";
}

