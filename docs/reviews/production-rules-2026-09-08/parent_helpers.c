#include "sim/cc_production_internal.h"
#include "sim/cc_food_economy_internal.h"
#include "sim/cc_route_rules_internal.h"

static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t ClampI32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

int32_t CcEconomyBakeryCapacity(const CcSettlement *place)
{
    if (!CcSettlementHasService(place, CC_SERVICE_BAKERY)) return 0;
    return MaximumI32(0, place->production[CC_GOOD_BREAD]);
}

static int32_t GrainSeasonFactor(const CcSim *sim)
{
    int32_t week = (sim->current_day / 7) % 52;
    if (week < 13) return 72;
    if (week < 26) return 112;
    if (week < 39) return 148;
    return 58;
}

int32_t CcEconomyEffectiveProduction(const CcSim *sim,
                                   const CcSettlement *settlement,
                                   int32_t index, CcGood good)
{
    if (CcSettlementIsAbandoned(settlement)) return 0;
    bool legacy_food_economy = sim->schema_version < 29U;
    int32_t production = settlement->production[good];
    CcGood staple = legacy_food_economy ? CC_GOOD_BREAD : CC_GOOD_WHEAT;
    bool subsistence_muster = good == staple &&
        (settlement->hunger > 65 ||
         (settlement->population < 600 && settlement->hunger >= 20));
    int32_t subsistence_food = subsistence_muster ?
        MaximumI32(1, CcEconomyCivilianFoodUse(settlement) * 2 / 3) : 0;
    if (production <= 0 && subsistence_food <= 0) return 0;
    if (settlement->hunger > 65) production = production * 72 / 100;
    else if (settlement->hunger > 35) production = production * 86 / 100;

    if (!legacy_food_economy && good == CC_GOOD_BREAD) return 0;
    if (good == staple) {
        if (!CcSettlementHasService(settlement, CC_SERVICE_FARM) ||
            settlement->field_yield <= 0) return subsistence_food;
        production = production * GrainSeasonFactor(sim) / 100;
        production = production * settlement->field_yield / 100;
        production = production * CcSimClimateFactor(sim) / 100;
        int32_t labor_factor = ClampI32(
            35 + settlement->population / 20, 35, 100);
        production = production * labor_factor / 100;
        if (index == 0 && sim->current_day < 112) production = production * 64 / 100;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) {
            production = production * 50 / 100;
        }
        production = MaximumI32(production, subsistence_food);
    }
    if (legacy_food_economy && good == CC_GOOD_BREAD) {
        return MaximumI32(0, production);
    }
    if (good == CC_GOOD_IRON) {
        if (!CcSettlementHasService(settlement, CC_SERVICE_MINE) ||
            settlement->iron_deposit <= 0) return 0;
        int32_t monster_pressure = CcRouteSettlementMonsterPressure(sim, settlement->id);
        production = production * (100 - monster_pressure / 2) / 100;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) production = MaximumI32(1, production / 4);
        for (int32_t dungeon = 0; dungeon < sim->dungeon_count; ++dungeon) {
            if (sim->dungeons[dungeon].settlement_id == settlement->id &&
                sim->dungeons[dungeon].state == CC_DUNGEON_PUBLIC_ROUTE) {
                production = production * 125 / 100;
            }
        }
        production = MinimumI32(production, settlement->iron_deposit);
    }
    if (good == CC_GOOD_WOOD && settlement->stock[CC_GOOD_TOOLS] <= 0) {
        production = MaximumI32(1, production / 4);
    }
    if (good == CC_GOOD_STONE) {
        if (!CcSettlementHasService(settlement, CC_SERVICE_MINE)) return 0;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) {
            production = MaximumI32(1, production / 4);
        }
    }
    if (legacy_food_economy && good >= CC_GOOD_TOOLS) return 0;
    if (good != CC_GOOD_WHEAT && good != CC_GOOD_IRON &&
        good != CC_GOOD_WOOD && good != CC_GOOD_STONE) return 0;
    return MaximumI32(0, production);
}

int32_t CcRouteSettlementMonsterPressure(const CcSim *sim, CcId settlement_id)
{
    int32_t pressure = 0;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].settlement_id != settlement_id) continue;
        for (int32_t monster = 0; monster < sim->monster_count; ++monster) {
            if (sim->monsters[monster].dungeon_id == sim->dungeons[i].id) {
                pressure = MaximumI32(pressure, sim->monsters[monster].pressure);
            }
        }
    }
    return pressure;
}

