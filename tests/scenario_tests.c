#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static CcId ActiveSituationId(const CcSim *sim, CcSituationKind kind,
                              CcId target)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status == CC_SITUATION_ACTIVE &&
            situation->kind == kind && situation->target_id == target) {
            return situation->id;
        }
    }
    return 0U;
}

static void FinishJourney(CcSim *sim)
{
    while (sim->journey.active) {
        CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
        CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    }
}

int main(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    char error[192];
    CC_CHECK(sim.routes[1].closed);
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
    CC_CHECK(CcSimApply(&sim, &food, error, sizeof(error)));

    CcCommand to_market = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[1].id
    };
    CC_CHECK(CcSimApply(&sim, &to_market, error, sizeof(error)));
    FinishJourney(&sim);

    const CcMap *bridge_map = CcSimMapForRoute(
        &sim, sim.routes[1].id, sim.player.location_id);
    CC_CHECK(bridge_map != NULL);
    CcCommand buy_bridge_map = {
        .kind = CC_COMMAND_BUY_MAP,
        .target_id = bridge_map->id
    };
    CC_CHECK(CcSimApply(&sim, &buy_bridge_map, error, sizeof(error)));

    CcCommand repair = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = sim.routes[1].id
    };
    CcCommand accept_repair = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = ActiveSituationId(
            &sim, CC_SITUATION_ROUTE_REPAIR, sim.routes[1].id)
    };
    CC_CHECK(CcSimApply(&sim, &accept_repair, error, sizeof(error)));
    CC_CHECK(CcSimApply(&sim, &repair, error, sizeof(error)));
    CC_CHECK(!sim.routes[1].closed);

    CcCommand to_fortress = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[2].id
    };
    CC_CHECK(CcSimApply(&sim, &to_fortress, error, sizeof(error)));
    FinishJourney(&sim);

    const CcMap *mine_map = CcSimMapForRoute(
        &sim, sim.routes[2].id, sim.player.location_id);
    CC_CHECK(mine_map != NULL);
    CcCommand buy_mine_map = {
        .kind = CC_COMMAND_BUY_MAP,
        .target_id = mine_map->id
    };
    CC_CHECK(CcSimApply(&sim, &buy_mine_map, error, sizeof(error)));
    CcCommand to_mine = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[3].id
    };
    CC_CHECK(CcSimApply(&sim, &to_mine, error, sizeof(error)));
    FinishJourney(&sim);
    int32_t food_before = sim.settlements[3].stock[CC_GOOD_FOOD];

    CcCommand accept_relief = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = ActiveSituationId(
            &sim, CC_SITUATION_RELIEF_DELIVERY, sim.settlements[3].id)
    };
    CC_CHECK(CcSimApply(&sim, &accept_relief, error, sizeof(error)));

    CcCommand deliver = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -8
    };
    CC_CHECK(CcSimApply(&sim, &deliver, error, sizeof(error)));
    CC_CHECK(sim.settlements[3].stock[CC_GOOD_FOOD] == food_before + 8);
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
    CC_CHECK(relief_resolved);
    CC_CHECK(repair_resolved);
    CC_CHECK(sim.player.reputation >= 20);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    puts("Empty Granary scenario tests passed");
    return 0;
}
