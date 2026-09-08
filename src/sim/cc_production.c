#include "sim/cc_production.h"
#include <limits.h>

CcProductionReceipt CcProductionPlan(const CcProductionRecipe *recipe,
                                     const CcProductionContext *context)
{
    CcProductionReceipt result = {.gate = CC_PRODUCTION_INVALID,
                                  .blocked_good = CC_GOOD_COUNT};
    if (recipe == NULL || context == NULL || context->stock == NULL ||
        context->producer_id == 0 || context->storage_id == 0 || context->location_id == 0 ||
        (recipe->work_only ? recipe->output != CC_GOOD_COUNT :
         (recipe->output < 0 || recipe->output >= CC_GOOD_COUNT)) ||
        recipe->output_units <= 0 || recipe->work_per_batch <= 0 ||
        recipe->input_count < 0 || recipe->input_count > CC_RECIPE_INPUTS ||
        recipe->minimum_condition < 0 || recipe->minimum_condition > 100 ||
        recipe->tools_required < 0 || context->capacity < 0 ||
        context->output_limit < 0 || context->work_available < 0 ||
        context->condition < 0 || context->condition > 100 ||
        context->hunger < 0 || context->hunger > 100 ||
        recipe->hunger_soft_limit < 0 || recipe->hunger_hard_limit < recipe->hunger_soft_limit ||
        recipe->hunger_hard_limit > 100 || recipe->hunger_soft_percent < 0 ||
        recipe->hunger_soft_percent > 100 || recipe->hunger_hard_percent < 0 ||
        recipe->hunger_hard_percent > 100) return result;
    for (int32_t i = 0; i < recipe->input_count; ++i) {
        CcRecipeInput input = recipe->inputs[i];
        if (input.good < 0 || input.good >= CC_GOOD_COUNT || input.units <= 0 ||
            input.reserve < 0 || input.good == recipe->output ||
            context->stock[input.good] < 0) return result;
        for (int32_t j = 0; j < i; ++j)
            if (recipe->inputs[j].good == input.good) return result;
    }
    if ((!recipe->work_only && context->stock[recipe->output] < 0) || context->stock[CC_GOOD_TOOLS] < 0) return result;
    result.producer_id = context->producer_id;
    result.storage_id = context->storage_id;
    result.location_id = context->location_id;
#define STOP(g) do { result.gate = (g); return result; } while (0)
    if (!context->enabled) STOP(CC_PRODUCTION_CLOSED);
    if (context->condition < recipe->minimum_condition) STOP(CC_PRODUCTION_CONDITION);
    int32_t batches = context->capacity;
    if (context->hunger > recipe->hunger_hard_limit)
        batches = (int32_t)((int64_t)batches * recipe->hunger_hard_percent / 100);
    else if (context->hunger > recipe->hunger_soft_limit)
        batches = (int32_t)((int64_t)batches * recipe->hunger_soft_percent / 100);
    if (batches == 0) STOP(CC_PRODUCTION_CAPACITY);
    int32_t held_output = recipe->work_only ? 0 : context->stock[recipe->output];
    int32_t output_space = context->output_limit > held_output ?
        context->output_limit - held_output : 0;
    int32_t space = output_space / recipe->output_units;
    if (recipe->allow_partial_output && output_space % recipe->output_units != 0) space++;
    if (space == 0) STOP(CC_PRODUCTION_OUTPUT_FULL);
    if (batches > space) batches = space;
    int32_t work = context->work_available / recipe->work_per_batch;
    if (work == 0) STOP(CC_PRODUCTION_WORK);
    if (batches > work) batches = work;
    if (context->stock[CC_GOOD_TOOLS] < recipe->tools_required) STOP(CC_PRODUCTION_TOOLS);
    for (int32_t i = 0; i < recipe->input_count; ++i) {
        CcRecipeInput input = recipe->inputs[i];
        int32_t reserve = input.reserve;
        if (input.good == CC_GOOD_TOOLS && reserve < recipe->tools_required)
            reserve = recipe->tools_required;
        int32_t available = context->stock[input.good] > reserve ?
            (context->stock[input.good] - reserve) / input.units : 0;
        if (available == 0) {
            result.blocked_good = input.good;
            STOP(CC_PRODUCTION_INPUT);
        }
        if (batches > available) batches = available;
    }
    result.gate = CC_PRODUCTION_READY;
    result.batches = batches;
    int64_t output = (int64_t)batches * recipe->output_units;
    result.output = recipe->work_only ? 0 :
        output > output_space ? output_space : (int32_t)output;
    result.work = batches * recipe->work_per_batch;
    for (int32_t i = 0; i < recipe->input_count; ++i)
        result.inputs[i] = batches * recipe->inputs[i].units;
#undef STOP
    return result;
}

CcProductionReceipt CcProductionRun(const CcProductionRecipe *recipe,
                                    const CcProductionContext *context)
{
    CcProductionReceipt result = CcProductionPlan(recipe, context);
    if (result.gate != CC_PRODUCTION_READY) return result;
    for (int32_t i = 0; i < recipe->input_count; ++i)
        context->stock[recipe->inputs[i].good] -= result.inputs[i];
    if (!recipe->work_only) context->stock[recipe->output] += result.output;
    return result;
}
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
