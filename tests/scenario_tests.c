#include "sim/cc_sim.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    char error[192];
    assert(sim.routes[1].closed);
    sim.player.coins = 100;
    for (int32_t route = 0; route <= 2; ++route) {
        sim.routes[route].security = 100;
        sim.routes[route].condition = 100;
    }

    CcCommand food = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 8
    };
    assert(CcSimApply(&sim, &food, error, sizeof(error)));

    CcCommand to_market = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[1].id
    };
    assert(CcSimApply(&sim, &to_market, error, sizeof(error)));

    CcCommand repair = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = sim.routes[1].id
    };
    assert(CcSimApply(&sim, &repair, error, sizeof(error)));
    assert(!sim.routes[1].closed);

    CcCommand to_fortress = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[2].id
    };
    CcCommand to_mine = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[3].id
    };
    assert(CcSimApply(&sim, &to_fortress, error, sizeof(error)));
    assert(CcSimApply(&sim, &to_mine, error, sizeof(error)));
    int32_t food_before = sim.settlements[3].stock[CC_GOOD_FOOD];

    CcCommand deliver = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -8
    };
    assert(CcSimApply(&sim, &deliver, error, sizeof(error)));
    assert(sim.settlements[3].stock[CC_GOOD_FOOD] == food_before + 8);
    bool relief_resolved = false;
    bool repair_resolved = false;
    for (int32_t i = 0; i < sim.situation_count; ++i) {
        const CcSituation *situation = &sim.situations[i];
        if (situation->kind == CC_SITUATION_RELIEF_DELIVERY &&
            situation->target_id == sim.settlements[3].id &&
            situation->status == CC_SITUATION_RESOLVED) relief_resolved = true;
        if (situation->kind == CC_SITUATION_ROUTE_REPAIR &&
            situation->target_id == sim.routes[1].id &&
            situation->status == CC_SITUATION_RESOLVED) repair_resolved = true;
    }
    assert(relief_resolved);
    assert(repair_resolved);
    assert(sim.player.reputation >= 20);
    assert(CcSimValidate(&sim, error, sizeof(error)));
    puts("Empty Granary scenario tests passed");
    return 0;
}
