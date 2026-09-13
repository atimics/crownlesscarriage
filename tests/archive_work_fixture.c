#define main CcRunnerFixtureMain
#include "../tools/sim_runner.c"
#undef main
#include "sim/cc_food_economy_internal.h"
#include "test_support.h"
static CcSim state, baseline, before;
static void Emit(void)
{
    before = state;
    JsonArchiveWork(&state); putchar('\n');
    CC_CHECK(memcmp(&before, &state, sizeof(state)) == 0);
}
int main(void)
{
    CcSimInit(&state, 42U);
    CcId seat = CcSimMaterialChainSnapshot(&state).scriptorium_id;
    CcSettlement *town = CcSimSettlementMutable(&state, seat);
    CC_CHECK(town != NULL);
    memset(town->stock, 0, sizeof(town->stock));
    town->stock[CC_GOOD_BREAD] = CcEconomyWeeklyFoodUse(&state, town) * 2;
    town->stock[CC_GOOD_WHEAT] = 4;
    town->stock[CC_GOOD_PAPER] = 20;
    town->stock[CC_GOOD_TOOLS] = 20;
    state.archives.scribes = 3;
    baseline = state;
    Emit(); /* Wheat supports two of three scribes. */
    town->stock[CC_GOOD_WHEAT] = 0; Emit();
    state = baseline; Emit();
    town->stock[CC_GOOD_PAPER] = 0; Emit();
    state = baseline; town->stock[CC_GOOD_TOOLS] = 0; Emit();
    state = baseline; state.archives.scribes = 0; Emit();
    state = baseline;
    for (int32_t i = 0; i < state.settlement_count; ++i) state.settlements[i].population = 0;
    Emit();
    state = baseline; state.schema_version = 57; Emit();
    state.schema_version = 33; Emit();
    CcArchiveWorkPlan empty = CcSimArchiveWorkPlan(NULL);
    CC_CHECK(empty.seat_id == 0 && empty.eligible_scribes == 0 &&
             empty.wheat_required == 0 && !empty.recording_ready);
    return 0;
}
