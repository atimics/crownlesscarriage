#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static void ApplySequence(CcSim *sim)
{
    char error[160];
    CcCommand buy = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 3
    };
    CC_CHECK(CcSimApply(sim, &buy, error, sizeof(error)));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim->settlements[1].id
    };
    CC_CHECK(CcSimApply(sim, &travel, error, sizeof(error)));
    CcSimAdvanceDays(sim, 17);
}
int main(void)
{
    CcSim first;
    CcSim second;
    CcSimInit(&first, UINT32_C(0x12345678));
    CcSimInit(&second, UINT32_C(0x12345678));
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    ApplySequence(&first);
    ApplySequence(&second);
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));

    char error[160];
    CC_CHECK(CcSimValidate(&first, error, sizeof(error)));
    CcSim different;
    CcSimInit(&different, UINT32_C(0x87654321));
    CC_CHECK(CcSimHash(&first) != CcSimHash(&different));
    puts("deterministic simulation tests passed");
    return 0;
}
