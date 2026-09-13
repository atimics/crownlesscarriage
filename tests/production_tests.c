#include "sim/cc_production.h"
#include "test_support.h"
#include <limits.h>
#include <string.h>

static void CheckCommonStore(void)
{
    int32_t stock[CC_GOOD_COUNT] = {0};
    int32_t remote[CC_GOOD_COUNT] = {0};
    stock[CC_GOOD_WHEAT] = 20;
    stock[CC_GOOD_TOOLS] = 1;
    remote[CC_GOOD_WHEAT] = 1000;
    CcProductionRecipe recipe = {.output = CC_GOOD_BREAD, .output_units = 2,
        .input_count = 1, .inputs = {{CC_GOOD_WHEAT, 3, 5}},
        .work_per_batch = 2, .minimum_condition = 50, .tools_required = 1,
        .hunger_soft_limit = 35, .hunger_hard_limit = 65,
        .hunger_soft_percent = 86, .hunger_hard_percent = 72};
    CcProductionContext context = {.producer_id = CcMakeId(CC_ENTITY_ROAD_SITE, 2),
        .storage_id = CcMakeId(CC_ENTITY_ROAD_SITE, 2),
        .location_id = CcMakeId(CC_ENTITY_ROAD_SITE, 2), .stock = stock,
        .capacity = 10, .output_limit = 100, .work_available = 6,
        .condition = 70, .enabled = true};
    CcProductionReceipt planned = CcProductionPlan(&recipe, &context);
    CC_CHECK(planned.gate == CC_PRODUCTION_READY && planned.batches == 3);
    CC_CHECK(planned.output == 6 && planned.inputs[0] == 9 && planned.work == 6);
    CC_CHECK(stock[CC_GOOD_WHEAT] == 20 && stock[CC_GOOD_BREAD] == 0);
    CcProductionReceipt made = CcProductionRun(&recipe, &context);
    CC_CHECK(made.producer_id == context.producer_id && made.storage_id == context.storage_id);
    CC_CHECK(made.location_id == context.location_id && made.output == planned.output);
    CC_CHECK(stock[CC_GOOD_WHEAT] == 11 && stock[CC_GOOD_BREAD] == 6);
    CC_CHECK(remote[CC_GOOD_WHEAT] == 1000 && stock[CC_GOOD_TOOLS] == 1);
    /* A second consumer sees only the remaining unreserved grain. */
    made = CcProductionRun(&recipe, &context);
    CC_CHECK(made.batches == 2 && made.inputs[0] == 6);
    CC_CHECK(stock[CC_GOOD_WHEAT] == 5 && stock[CC_GOOD_BREAD] == 10);
    made = CcProductionRun(&recipe, &context);
    CC_CHECK(made.gate == CC_PRODUCTION_INPUT && made.blocked_good == CC_GOOD_WHEAT);
    CC_CHECK(made.output == 0 && stock[CC_GOOD_WHEAT] == 5);
}

static void CheckGates(void)
{
    int32_t stock[CC_GOOD_COUNT] = {0};
    stock[CC_GOOD_WHEAT] = 100;
    stock[CC_GOOD_TOOLS] = 1;
    const CcProductionRecipe original = {.output = CC_GOOD_BREAD, .output_units = 1,
        .input_count = 1, .inputs = {{CC_GOOD_WHEAT, 1, 0}},
        .work_per_batch = 1, .minimum_condition = 50, .tools_required = 1,
        .hunger_soft_limit = 35, .hunger_hard_limit = 65,
        .hunger_soft_percent = 86, .hunger_hard_percent = 72};
    CcProductionRecipe recipe = original;
    CcProductionContext context = {.producer_id = 1, .storage_id = 1, .location_id = 1,
        .stock = stock, .capacity = 10, .output_limit = 100, .work_available = 100,
        .condition = 100, .enabled = true};
#define GATE(expected) do { int32_t before[CC_GOOD_COUNT]; memcpy(before, stock, sizeof(stock)); \
    CcProductionReceipt result = CcProductionRun(&recipe, &context); \
    CC_CHECK(result.gate == (expected) && result.output == 0); \
    CC_CHECK(memcmp(before, stock, sizeof(stock)) == 0); } while (0)
    context.enabled = false; GATE(CC_PRODUCTION_CLOSED); context.enabled = true;
    context.condition = 49; GATE(CC_PRODUCTION_CONDITION); context.condition = 100;
    context.capacity = 0; GATE(CC_PRODUCTION_CAPACITY); context.capacity = 10;
    context.output_limit = 0; GATE(CC_PRODUCTION_OUTPUT_FULL); context.output_limit = 100;
    context.work_available = 0; GATE(CC_PRODUCTION_WORK); context.work_available = 100;
    stock[CC_GOOD_TOOLS] = 0; GATE(CC_PRODUCTION_TOOLS); stock[CC_GOOD_TOOLS] = 1;
    recipe.input_count = 2; recipe.inputs[1] = recipe.inputs[0]; GATE(CC_PRODUCTION_INVALID);
    recipe = original;
    context.hunger = 36; CC_CHECK(CcProductionPlan(&recipe, &context).output == 8);
    context.hunger = 66; CC_CHECK(CcProductionPlan(&recipe, &context).output == 7);
    recipe.hunger_hard_percent = 50; CC_CHECK(CcProductionPlan(&recipe, &context).output == 5);
    recipe.inputs[0].reserve = 98; CC_CHECK(CcProductionPlan(&recipe, &context).output == 2);
    recipe.inputs[0] = (CcRecipeInput){CC_GOOD_TOOLS, 1, 0}; GATE(CC_PRODUCTION_INPUT);
    recipe = original; context.hunger = 0;
    context.capacity = INT32_MAX; context.work_available = INT32_MAX;
    context.output_limit = INT32_MAX; stock[CC_GOOD_BREAD] = INT32_MAX - 1;
    CC_CHECK(CcProductionRun(&recipe, &context).output == 1);
    CC_CHECK(stock[CC_GOOD_BREAD] == INT32_MAX);
#undef GATE
}

static void CheckPartialOutput(void)
{
    for (int32_t limit = 1; limit <= 9; ++limit) {
        int32_t stock[CC_GOOD_COUNT] = {0};
        stock[CC_GOOD_WOOD] = 13;
        CcProductionRecipe recipe = {.output = CC_GOOD_PAPER, .output_units = 4,
            .allow_partial_output = true, .input_count = 1,
            .inputs = {{CC_GOOD_WOOD, 1, 10}}, .work_per_batch = 2,
            .hunger_soft_limit = 100, .hunger_hard_limit = 100};
        CcProductionContext context = {.producer_id = 1, .storage_id = 1, .location_id = 1,
            .stock = stock, .capacity = 3, .output_limit = limit, .work_available = 6,
            .condition = 100, .enabled = true};
        CcProductionReceipt receipt = CcProductionRun(&recipe, &context);
        int32_t batches = (limit + 3) / 4;
        CC_CHECK(receipt.output == limit && receipt.batches == batches);
        CC_CHECK(receipt.inputs[0] == batches && receipt.work == batches * 2);
        CC_CHECK(stock[CC_GOOD_WOOD] == 13 - batches && stock[CC_GOOD_PAPER] == limit);
        CC_CHECK(CcProductionRun(&recipe, &context).gate == CC_PRODUCTION_OUTPUT_FULL);
        stock[CC_GOOD_PAPER] = INT32_MAX - 1;
        context.output_limit = INT32_MAX;
        CC_CHECK(CcProductionRun(&recipe, &context).output == (batches == 3 ? 0 : 1));
    }
}

int main(void)
{
    CheckCommonStore();
    CheckGates();
    CheckPartialOutput();
    puts("Common production: local custody, exact receipts, reserves, work, policy and gates passed");
    return 0;
}
