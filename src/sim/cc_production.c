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

#include <stdio.h>
#include <string.h>

void CcEconomyWearOneTool(CcSettlement *settlement, int32_t *wear,
                        int32_t batches_per_tool)
{
    if (settlement->stock[CC_GOOD_TOOLS] <= 0) return;
    *wear += 1;
    if (*wear < batches_per_tool) return;
    settlement->stock[CC_GOOD_TOOLS] -= 1;
    *wear = 0;
}

static void RecordRecipe(CcRecipeAccounting *accounting,
    const CcProductionRecipe *recipe, const CcProductionReceipt *receipt)
{
    if (accounting == NULL) return;
    accounting->gates[receipt->gate]++;
    accounting->work += (uint64_t)receipt->work;
    for (int32_t i = 0; i < recipe->input_count; ++i)
        accounting->input[recipe->inputs[i].good] += (uint64_t)receipt->inputs[i];
    if (!recipe->work_only) accounting->output[recipe->output] += (uint64_t)receipt->output;
}

int32_t CcEconomyRunBakery(CcSim *sim, CcSettlement *place,
                         CcId scriptorium_id, CcRecipeAccounting *accounting,
    const CcProductionServices *services)
{
    int32_t capacity = CcEconomyBakeryCapacity(place);
    if (capacity <= 0 || place->stock[CC_GOOD_WHEAT] <= 0) {
        if (accounting != NULL) accounting->gates[capacity <= 0 ? CC_PRODUCTION_CAPACITY : CC_PRODUCTION_INPUT]++;
        return 0;
    }
    int32_t grain_floor = 0;
    if (scriptorium_id != 0U && place->id == scriptorium_id &&
        place->hunger == 0 &&
        place->stock[CC_GOOD_TOOLS] > 0 &&
        CcSettlementHasService(place, CC_SERVICE_MILL)) {
        grain_floor = MaximumI32(
            place->reserve_target[CC_GOOD_WHEAT],
            CcEconomyWeeklyFoodUse(sim, place) * 2 + sim->archives.scribes * 2);
    }
    const CcProductionRecipe recipe = {
        .output = CC_GOOD_BREAD, .output_units = 1, .input_count = 1,
        .inputs = {{CC_GOOD_WHEAT, 1, grain_floor}}, .work_per_batch = 1,
        .hunger_soft_limit = 35, .hunger_hard_limit = 65,
        .hunger_soft_percent = 86, .hunger_hard_percent = 72
    };
    const CcProductionContext context = {
        .producer_id = place->id, .storage_id = place->id, .location_id = place->id,
        .stock = place->stock, .capacity = capacity, .output_limit = CC_SIM_MAX_UNITS,
        .work_available = capacity, .condition = 100, .hunger = place->hunger, .enabled = true
    };
    CcProductionReceipt receipt = CcProductionRun(&recipe, &context);
    RecordRecipe(accounting, &recipe, &receipt);
    int32_t baked = receipt.output;
    if (baked <= 0) return 0;
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

const char *CcSmithyStatusName(CcSmithyStatus status)
{
    switch (status) {
        case CC_SMITHY_READY: return "Ready";
        case CC_SMITHY_SERVICE_UNAVAILABLE: return "Smithy service required";
        case CC_SMITHY_ZERO_CAPACITY: return "Production capacity required";
        case CC_SMITHY_RESERVE_MET: return "Reserve target met";
        case CC_SMITHY_IRON_REQUIRED: return "Iron required";
        case CC_SMITHY_WOOD_REQUIRED: return "Wood required";
        case CC_SMITHY_ABANDONED: return "Workers required";
        case CC_SMITHY_REPAIRS_REQUIRED: return "Fire repairs required";
        case CC_SMITHY_STATUS_COUNT: break;
    }
    return "Unknown smithy state";
}

static CcSmithyStatus SmithyLineStatus(int32_t capacity, int32_t gap,
                                      int32_t iron, int32_t wood,
                                      int32_t iron_cost, int32_t wood_cost)
{
    if (capacity <= 0) return CC_SMITHY_ZERO_CAPACITY;
    if (gap <= 0) return CC_SMITHY_RESERVE_MET;
    if (iron < iron_cost) return CC_SMITHY_IRON_REQUIRED;
    if (wood_cost > 0 && wood < wood_cost) return CC_SMITHY_WOOD_REQUIRED;
    return CC_SMITHY_READY;
}

static CcSmithyPlan RunSmithyRecipes(const CcSim *sim,
                                    const CcSettlement *settlement, int32_t *stock)
{
    CcSmithyPlan plan = {0};
    plan.tools_status = CC_SMITHY_SERVICE_UNAVAILABLE;
    plan.weapons_status = CC_SMITHY_SERVICE_UNAVAILABLE;
    if (sim == NULL || settlement == NULL ||
        !CcSettlementHasService(settlement, CC_SERVICE_SMITHY)) return plan;
    if (sim->schema_version >= 61U &&
        (CcSettlementIsAbandoned(settlement) || settlement->fire_damage >= 100)) {
        plan.tools_status = CcSettlementIsAbandoned(settlement) ?
            CC_SMITHY_ABANDONED : CC_SMITHY_REPAIRS_REQUIRED;
        plan.weapons_status = plan.tools_status;
        return plan;
    }
    bool legacy_smithy = sim->schema_version < 27U;
    CcProductionRecipe recipe = {
        .output = CC_GOOD_TOOLS, .output_units = 1,
        .input_count = legacy_smithy ? 1 : 2,
        .inputs = {{CC_GOOD_IRON, 2, 0}, {CC_GOOD_WOOD, 1, 0}},
        .work_per_batch = 1, .hunger_soft_limit = 100, .hunger_hard_limit = 100
    };
    CcProductionContext context = {
        .producer_id = settlement->id, .storage_id = settlement->id,
        .location_id = settlement->id, .stock = stock,
        .capacity = MaximumI32(0, settlement->production[CC_GOOD_TOOLS]),
        .output_limit = settlement->reserve_target[CC_GOOD_TOOLS] * 2,
        .work_available = INT32_MAX, .condition = 100, .enabled = true
    };
    plan.tools_status = SmithyLineStatus(context.capacity,
        context.output_limit - stock[CC_GOOD_TOOLS], stock[CC_GOOD_IRON],
        stock[CC_GOOD_WOOD], 2, legacy_smithy ? 0 : 1);
    CcProductionReceipt tools = CcProductionRun(&recipe, &context);
    recipe.output = CC_GOOD_WEAPONS;
    recipe.inputs[0].units = 3;
    recipe.inputs[1].units = 2;
    context.capacity = MaximumI32(0, settlement->production[CC_GOOD_WEAPONS]);
    context.output_limit = CcEconomyEffectiveReserveTarget(sim, settlement, CC_GOOD_WEAPONS) * 2;
    plan.weapons_status = SmithyLineStatus(context.capacity,
        context.output_limit - stock[CC_GOOD_WEAPONS], stock[CC_GOOD_IRON],
        stock[CC_GOOD_WOOD], 3, legacy_smithy ? 0 : 2);
    CcProductionReceipt weapons = CcProductionRun(&recipe, &context);
    plan.tools_made = tools.output;
    plan.weapons_made = weapons.output;
    plan.iron_used = tools.inputs[0] + weapons.inputs[0];
    plan.wood_used = tools.inputs[1] + weapons.inputs[1];
    return plan;
}

CcSmithyPlan CcSimPlanSmithy(const CcSim *sim, const CcSettlement *settlement)
{
    int32_t stock[CC_GOOD_COUNT] = {0};
    if (settlement != NULL) memcpy(stock, settlement->stock, sizeof(stock));
    return RunSmithyRecipes(sim, settlement, stock);
}

void CcEconomyRunSmithy(CcSim *sim, CcSettlement *settlement,
                       CcTownSmithyAccounting *accounting, CcTownProductionAccounting *ledger,
    const CcProductionServices *services)
{
    int32_t iron_before = settlement->stock[CC_GOOD_IRON];
    int32_t wood_before = settlement->stock[CC_GOOD_WOOD];
    CcSmithyPlan plan = RunSmithyRecipes(sim, settlement, settlement->stock);
    if (accounting != NULL) {
        accounting->settlement_id = settlement->id;
        accounting->tools_status[plan.tools_status]++;
        accounting->weapons_status[plan.weapons_status]++;
        accounting->tools_made += (uint64_t)plan.tools_made;
        accounting->weapons_made += (uint64_t)plan.weapons_made;
        accounting->iron_used += (uint64_t)plan.iron_used;
        accounting->wood_used += (uint64_t)plan.wood_used;
    }
    if (!CcSettlementHasService(settlement, CC_SERVICE_SMITHY)) {
        if (ledger != NULL) ledger->treasure.gates[CC_PRODUCTION_CLOSED]++;
        return;
    }
    bool legacy_smithy = sim->schema_version < 27U;
    int32_t tools_made = plan.tools_made;
    int32_t weapons_made = plan.weapons_made;

    int32_t tools_before_wear = settlement->stock[CC_GOOD_TOOLS];
    int32_t smith_batches = tools_made + weapons_made;
    for (int32_t batch = 0; batch < smith_batches; ++batch) {
        CcEconomyWearOneTool(settlement, &settlement->smith_tool_wear, 4);
    }
    if (accounting != NULL) {
        accounting->tools_worn += (uint64_t)(tools_before_wear -
                                             settlement->stock[CC_GOOD_TOOLS]);
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
    if (!treasure_town) {
        if (ledger != NULL) ledger->treasure.gates[CC_PRODUCTION_CLOSED]++;
        return;
    }
    CcProductionReceipt craft = services->run_treasure_work(sim, settlement, settlement->stock);
    if (ledger != NULL) {
        ledger->treasure.gates[craft.gate]++;
        ledger->treasure.input[CC_GOOD_GOLD] += (uint64_t)craft.inputs[0];
        ledger->treasure.input[CC_GOOD_GEMS] += (uint64_t)craft.inputs[1];
        ledger->treasure.work += (uint64_t)craft.work;
    }
    if (craft.gate == CC_PRODUCTION_READY) {
        if (settlement->treasure_work == 0) {
            settlement->treasure_gold_committed = craft.inputs[0];
            settlement->treasure_gems_committed = craft.inputs[1];
        }
        settlement->treasure_work += craft.work;
    }
    if (settlement->treasure_work >= 3 &&
        sim->treasure_count < CC_MAX_TREASURES) {
        services->complete_treasure(sim, settlement);
        if (ledger != NULL && settlement->treasure_work == 0) ledger->treasures_completed++;
    }
}

void CcEconomyRunPaperMill(CcSim *sim, CcSettlement *settlement, CcRecipeAccounting *accounting,
    const CcProductionServices *services)
{
    if (sim == NULL || sim->schema_version < 33U) {
        if (accounting != NULL) accounting->gates[CC_PRODUCTION_CLOSED]++;
        return;
    }
    bool legacy = sim->schema_version < 34U;
    if (!legacy && (!CcSettlementHasService(settlement, CC_SERVICE_MILL) ||
        settlement->hunger > 0 || settlement->stock[CC_GOOD_TOOLS] <= 0)) {
        if (accounting != NULL) accounting->gates[!CcSettlementHasService(settlement, CC_SERVICE_MILL) ?
            CC_PRODUCTION_CLOSED : settlement->hunger > 0 ? CC_PRODUCTION_CAPACITY : CC_PRODUCTION_TOOLS]++;
        return;
    }
    /* Keep mill work behind the town's food buffer, across all edible goods. */
    if (sim->schema_version >= 37U &&
        CcEconomyNutritionRations(settlement->stock, CC_NUTRITION_CIVILIAN) <
            CcEconomyWeeklyFoodUse(sim, settlement) * 4) {
        if (accounting != NULL) accounting->gates[CC_PRODUCTION_INPUT]++;
        return;
    }
    int32_t capacity = MaximumI32(0, settlement->production[CC_GOOD_PAPER]);
    CcGood input = !legacy && sim->schema_version < 37U ?
        CC_GOOD_WHEAT : CC_GOOD_WOOD;
    int32_t protected_input = settlement->reserve_target[input];
    if (input == CC_GOOD_WHEAT) protected_input += CcEconomyWeeklyFoodUse(sim, settlement) * 4;
    const CcProductionRecipe recipe = {
        .output = CC_GOOD_PAPER, .output_units = 4, .allow_partial_output = true,
        .input_count = 1, .inputs = {{input, 1, protected_input}},
        .work_per_batch = 1, .tools_required = legacy ? 0 : 1,
        .hunger_soft_limit = 100, .hunger_hard_limit = 100
    };
    int64_t capacity_limit = (int64_t)settlement->stock[CC_GOOD_PAPER] + capacity;
    const CcProductionContext context = {
        .producer_id = settlement->id, .storage_id = settlement->id,
        .location_id = settlement->id, .stock = settlement->stock,
        .capacity = capacity / 4 + (capacity % 4 != 0),
        .output_limit = MinimumI32(settlement->reserve_target[CC_GOOD_PAPER] * 2,
            capacity_limit > INT32_MAX ? INT32_MAX : (int32_t)capacity_limit),
        .work_available = INT32_MAX, .condition = 100, .enabled = true
    };
    CcProductionReceipt receipt = CcProductionRun(&recipe, &context);
    RecordRecipe(accounting, &recipe, &receipt);
    int32_t paper_made = receipt.output;
    if (paper_made <= 0) return;
    int32_t input_used = receipt.inputs[0];
    if (legacy) {
        CcEconomyRefreshSettlementGoodPrice(sim, settlement, CC_GOOD_WOOD);
        CcEconomyRefreshSettlementGoodPrice(sim, settlement, CC_GOOD_PAPER);
        return;
    }
    int32_t tools_before = settlement->stock[CC_GOOD_TOOLS];
    CcEconomyWearOneTool(settlement, &settlement->paper_tool_wear, 8);
    if (accounting != NULL) accounting->tools_worn += (uint64_t)(tools_before - settlement->stock[CC_GOOD_TOOLS]);
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
