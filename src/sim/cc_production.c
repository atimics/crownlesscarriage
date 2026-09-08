#include "sim/cc_production_internal.h"
#include "sim/cc_food_economy_internal.h"
#include "sim/cc_route_rules_internal.h"

#include <stdio.h>

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


void CcEconomyWearOneTool(CcSettlement *settlement, int32_t *wear,
                        int32_t batches_per_tool)
{
    if (settlement->stock[CC_GOOD_TOOLS] <= 0) return;
    *wear += 1;
    if (*wear < batches_per_tool) return;
    settlement->stock[CC_GOOD_TOOLS] -= 1;
    *wear = 0;
}

int32_t CcEconomyRunBakery(CcSim *sim, CcSettlement *place,
                         CcId scriptorium_id,
    const CcProductionServices *services)
{
    int32_t capacity = CcEconomyBakeryCapacity(place);
    if (capacity <= 0 || place->stock[CC_GOOD_WHEAT] <= 0) return 0;
    if (place->hunger > 65) capacity = capacity * 72 / 100;
    else if (place->hunger > 35) capacity = capacity * 86 / 100;
    int32_t grain_floor = 0;
    if (scriptorium_id != 0U && place->id == scriptorium_id &&
        place->hunger == 0 &&
        place->stock[CC_GOOD_TOOLS] > 0 &&
        CcSettlementHasService(place, CC_SERVICE_MILL)) {
        grain_floor = MaximumI32(
            place->reserve_target[CC_GOOD_WHEAT],
            CcEconomyWeeklyFoodUse(sim, place) * 2 + sim->archives.scribes * 2);
    }
    int32_t baked = MinimumI32(
        capacity, MaximumI32(
            0, place->stock[CC_GOOD_WHEAT] - grain_floor));
    baked = MinimumI32(baked,
                       CC_SIM_MAX_UNITS - place->stock[CC_GOOD_BREAD]);
    if (baked <= 0) return 0;
    place->stock[CC_GOOD_WHEAT] -= baked;
    place->stock[CC_GOOD_BREAD] += baked;
    if (sim->current_day % 28 == 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s's bakery mills %d Wheat into %d Bread.",
                       place->name, baked, baked);
        (void)services->record_event(sim, CC_EVENT_BAKERY_PRODUCTION, place->id,
                        place->id, 0U, baked, text);
    }
    return baked;
}

void CcEconomyRunSmithy(CcSim *sim, CcSettlement *settlement,
    const CcProductionServices *services)
{
    if (!CcSettlementHasService(settlement, CC_SERVICE_SMITHY)) return;
    bool legacy_smithy = sim->schema_version < 27U;
    int32_t iron_before = settlement->stock[CC_GOOD_IRON];
    int32_t wood_before = settlement->stock[CC_GOOD_WOOD];
    int32_t tools_made = 0;
    int32_t tool_gap = MaximumI32(0, settlement->reserve_target[CC_GOOD_TOOLS] * 2 -
                                     settlement->stock[CC_GOOD_TOOLS]);
    int32_t tool_capacity = MaximumI32(0, settlement->production[CC_GOOD_TOOLS]);
    int32_t tool_material = settlement->stock[CC_GOOD_IRON] / 2;
    if (!legacy_smithy) {
        tool_material = MinimumI32(
            tool_material, settlement->stock[CC_GOOD_WOOD]);
    }
    tools_made = MinimumI32(
        tool_capacity, MinimumI32(tool_gap, tool_material));
    settlement->stock[CC_GOOD_IRON] -= tools_made * 2;
    if (!legacy_smithy) settlement->stock[CC_GOOD_WOOD] -= tools_made;
    settlement->stock[CC_GOOD_TOOLS] += tools_made;

    int32_t weapons_made = 0;
    int32_t weapon_gap = MaximumI32(0,
        CcEconomyEffectiveReserveTarget(sim, settlement, CC_GOOD_WEAPONS) * 2 -
        settlement->stock[CC_GOOD_WEAPONS]);
    int32_t weapon_capacity = MaximumI32(
        0, settlement->production[CC_GOOD_WEAPONS]);
    int32_t weapon_material = settlement->stock[CC_GOOD_IRON] / 3;
    if (!legacy_smithy) {
        weapon_material = MinimumI32(
            weapon_material, settlement->stock[CC_GOOD_WOOD] / 2);
    }
    weapons_made = MinimumI32(
        weapon_capacity, MinimumI32(weapon_gap, weapon_material));
    settlement->stock[CC_GOOD_IRON] -= weapons_made * 3;
    if (!legacy_smithy) settlement->stock[CC_GOOD_WOOD] -= weapons_made * 2;
    settlement->stock[CC_GOOD_WEAPONS] += weapons_made;

    int32_t smith_batches = tools_made + weapons_made;
    for (int32_t batch = 0; batch < smith_batches; ++batch) {
        CcEconomyWearOneTool(settlement, &settlement->smith_tool_wear, 4);
    }
    if (smith_batches > 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        if (legacy_smithy) {
            (void)snprintf(
                text, sizeof(text),
                "%s's smithy turns %d Iron into %d Tools and %d Weapons.",
                settlement->name,
                iron_before - settlement->stock[CC_GOOD_IRON],
                tools_made, weapons_made);
        } else {
            (void)snprintf(
                text, sizeof(text),
                "%s's smithy uses %d Iron and %d Wood to make %d Tools and %d Weapons.",
                settlement->name,
                iron_before - settlement->stock[CC_GOOD_IRON],
                wood_before - settlement->stock[CC_GOOD_WOOD],
                tools_made, weapons_made);
        }
        (void)services->record_event(sim, CC_EVENT_SMITH_PRODUCTION, settlement->id,
                        settlement->id, services->latest_local_cause(sim, settlement->id),
                        tools_made + weapons_made, text);
    }

    bool treasure_town = settlement->function == CC_SETTLEMENT_MARKET ||
                         settlement->function == CC_SETTLEMENT_CAPITAL;
    if (!treasure_town) return;
    if (settlement->treasure_work == 0 &&
        settlement->stock[CC_GOOD_GOLD] >= 1 &&
        settlement->stock[CC_GOOD_GEMS] >= 1) {
        settlement->stock[CC_GOOD_GOLD] -= 1;
        settlement->stock[CC_GOOD_GEMS] -= 1;
        settlement->treasure_gold_committed = 1;
        settlement->treasure_gems_committed = 1;
        settlement->treasure_work = 1;
    } else if (settlement->treasure_work > 0 &&
               settlement->treasure_work < 3) {
        settlement->treasure_work += 1;
    }
    if (settlement->treasure_work >= 3 &&
        sim->treasure_count < CC_MAX_TREASURES) {
        services->complete_treasure(sim, settlement);
    }
}

void CcEconomyRunPaperMill(CcSim *sim, CcSettlement *settlement,
    const CcProductionServices *services)
{
    if (sim == NULL || sim->schema_version < 33U) return;
    if (sim->schema_version < 34U) {
        int32_t capacity = MaximumI32(
            0, settlement->production[CC_GOOD_PAPER]);
        int32_t gap = MaximumI32(
            0, settlement->reserve_target[CC_GOOD_PAPER] * 2 -
               settlement->stock[CC_GOOD_PAPER]);
        int32_t wood_available = MaximumI32(
            0, settlement->stock[CC_GOOD_WOOD] -
               settlement->reserve_target[CC_GOOD_WOOD]);
        int32_t paper_made = MinimumI32(
            capacity, MinimumI32(gap, wood_available * 4));
        if (paper_made <= 0) return;
        int32_t wood_used = (paper_made + 3) / 4;
        settlement->stock[CC_GOOD_WOOD] -= wood_used;
        settlement->stock[CC_GOOD_PAPER] += paper_made;
        CcEconomyRefreshSettlementGoodPrice(sim, settlement, CC_GOOD_WOOD);
        CcEconomyRefreshSettlementGoodPrice(sim, settlement, CC_GOOD_PAPER);
        return;
    }
    if (!CcSettlementHasService(settlement, CC_SERVICE_MILL) ||
        settlement->hunger > 0 ||
        settlement->stock[CC_GOOD_TOOLS] <= 0) return;
    /* Keep mill work behind the town's food buffer, across all edible goods. */
    if (sim->schema_version >= 37U &&
        CcEconomyNutritionRations(settlement->stock, CC_NUTRITION_CIVILIAN) <
            CcEconomyWeeklyFoodUse(sim, settlement) * 4) return;
    int32_t capacity = MaximumI32(
        0, settlement->production[CC_GOOD_PAPER]);
    int32_t gap = MaximumI32(
        0, settlement->reserve_target[CC_GOOD_PAPER] * 2 -
           settlement->stock[CC_GOOD_PAPER]);
    CcGood input = sim->schema_version < 37U ?
        CC_GOOD_WHEAT : CC_GOOD_WOOD;
    int32_t protected_input = settlement->reserve_target[input];
    if (input == CC_GOOD_WHEAT) {
        protected_input += CcEconomyWeeklyFoodUse(sim, settlement) * 4;
    }
    int32_t input_available = MaximumI32(
        0, settlement->stock[input] - protected_input);
    int32_t paper_made = MinimumI32(
        capacity, MinimumI32(gap, input_available * 4));
    if (paper_made <= 0) return;
    int32_t input_used = (paper_made + 3) / 4;
    settlement->stock[input] -= input_used;
    settlement->stock[CC_GOOD_PAPER] += paper_made;
    CcEconomyWearOneTool(settlement, &settlement->paper_tool_wear, 8);
    CcEconomyRefreshSettlementGoodPrice(sim, settlement, input);
    CcEconomyRefreshSettlementGoodPrice(sim, settlement, CC_GOOD_PAPER);
    CcEconomyRefreshSettlementGoodPrice(sim, settlement, CC_GOOD_TOOLS);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%s's mill uses %d %s to make %d Paper.",
        settlement->name, input_used, CcGoodName(input), paper_made);
    (void)services->record_event(
        sim, CC_EVENT_PAPER_MILLED, settlement->id, settlement->id,
        services->latest_local_cause(sim, settlement->id), paper_made, text);
}

void CcEconomyAdvanceRareMineWork(CcSim *sim, CcSettlement *settlement,
                                int32_t iron_mined,
    const CcProductionServices *services)
{
    if (iron_mined <= 0) return;
    if (settlement->gold_seam && settlement->stock[CC_GOOD_TOOLS] > 0) {
        settlement->gold_progress += 1;
        if (settlement->gold_progress >= 12) {
            settlement->gold_progress -= 12;
            settlement->stock[CC_GOOD_GOLD] += 1;
        }
    }
    if (settlement->gem_seam && settlement->stock[CC_GOOD_TOOLS] > 0) {
        settlement->gem_progress += 1;
        if (settlement->gem_progress >= 48) {
            settlement->gem_progress -= 48;
            settlement->stock[CC_GOOD_GEMS] += 1;
        }
    }
    if (sim->current_day % 28 == 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s extracts %d Iron; seams stand at gold %d/12 and gems %d/48.",
                       settlement->name, iron_mined,
                       settlement->gold_progress, settlement->gem_progress);
        (void)services->record_event(sim, CC_EVENT_RESOURCE_EXTRACTED, settlement->id,
                        settlement->id, services->latest_local_cause(sim, settlement->id),
                        iron_mined, text);
    }
}

