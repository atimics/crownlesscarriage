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
    char error[160];
    CcSimInit(&first, UINT32_C(0x12345678));
    CcSimInit(&second, UINT32_C(0x12345678));
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        CC_CHECK(first.player.athletics.level[discipline] == 1);
        CC_CHECK(first.player.athletics.experience[discipline] == 0.0f);
    }
    CcSim developed = first;
    developed.player.athletics.experience[CC_ATHLETIC_MOBILITY] = 1.0f;
    CC_CHECK(CcSimHash(&first) != CcSimHash(&developed));
    ApplySequence(&first);
    ApplySequence(&second);
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    CC_CHECK(first.map_count == first.route_count);
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcSimMapForRoute(&first, first.routes[0].id,
                             first.player.id) != NULL);
    CcCommand uncharted_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = first.settlements[2].id
    };
    CC_CHECK(!CcSimApply(&first, &uncharted_travel, error, sizeof(error)));

    const CcMap *offered = CcSimMapForRoute(
        &first, first.routes[1].id, first.player.location_id);
    CC_CHECK(offered != NULL);
    CcId offered_id = offered->id;
    first.player.coins = 100;
    CcCommand buy_map = {.kind = CC_COMMAND_BUY_MAP, .target_id = offered_id};
    CC_CHECK(CcSimApply(&first, &buy_map, error, sizeof(error)));
    CC_CHECK(CcPlayerMapCount(&first) == 2);
    CC_CHECK(CcSimMap(&first, offered_id)->owner_id == first.player.id);
    CcCommand sell_map = {.kind = CC_COMMAND_SELL_MAP, .target_id = offered_id};
    CC_CHECK(CcSimApply(&first, &sell_map, error, sizeof(error)));
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcSimMap(&first, offered_id)->owner_id ==
             first.player.location_id);

    CC_CHECK(CcSimValidate(&first, error, sizeof(error)));
    CcSim invalid_profile = first;
    invalid_profile.player.athletics.level[CC_ATHLETIC_GRIP] = 0;
    CC_CHECK(!CcSimValidate(&invalid_profile, error, sizeof(error)));
    invalid_profile = first;
    invalid_profile.player.athletics.experience[CC_ATHLETIC_POWER] = -1.0f;
    CC_CHECK(!CcSimValidate(&invalid_profile, error, sizeof(error)));
    CcSim different;
    CcSimInit(&different, UINT32_C(0x87654321));
    CC_CHECK(CcSimHash(&first) != CcSimHash(&different));
    puts("deterministic simulation tests passed");
    return 0;
}
