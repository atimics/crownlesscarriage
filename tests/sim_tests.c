#include "sim/cc_sim.h"

#include <assert.h>
#include <stdio.h>

static void ApplySequence(CcSim *sim)
{
    char error[160];
    CcCommand buy = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 3
    };
    assert(CcSimApply(sim, &buy, error, sizeof(error)));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim->settlements[1].id
    };
    assert(CcSimApply(sim, &travel, error, sizeof(error)));
    CcSimAdvanceDays(sim, 17);
}
int main(void)
{
    CcSim first;
    CcSim second;
    CcSimInit(&first, UINT32_C(0x12345678));
    CcSimInit(&second, UINT32_C(0x12345678));
    assert(CcSimHash(&first) == CcSimHash(&second));
    ApplySequence(&first);
    ApplySequence(&second);
    assert(CcSimHash(&first) == CcSimHash(&second));

    char error[160];
    assert(CcSimValidate(&first, error, sizeof(error)));
    CcSim different;
    CcSimInit(&different, UINT32_C(0x87654321));
    assert(CcSimHash(&first) != CcSimHash(&different));
    puts("deterministic simulation tests passed");
    return 0;
}
