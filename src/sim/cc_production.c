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
