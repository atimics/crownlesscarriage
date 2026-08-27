#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static CcSituation *ActiveSituation(CcSim *sim, CcSituationKind kind)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE &&
            sim->situations[i].kind == kind) return &sim->situations[i];
    }
    return NULL;
}

static CcSituationStatus SituationStatus(const CcSim *sim,
                                         CcSituationKind kind)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].kind == kind) return sim->situations[i].status;
    }
    return CC_SITUATION_FAILED;
}

static void AdvanceUntilStop(CcSim *sim)
{
    while (sim->journey.active &&
           sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    }
}

static void TravelAndArrive(CcSim *sim, CcId destination, char *error,
                            size_t error_capacity)
{
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = destination
    };
    CC_CHECK(CcSimApply(sim, &travel, error, error_capacity));
    AdvanceUntilStop(sim);
    CC_CHECK(!sim->journey.active);
}

static void PrepareDungeonChoice(CcSim *sim, uint32_t seed, char *error,
                                 size_t error_capacity)
{
    CcSimInit(sim, seed);
    sim->player.location_id = sim->settlements[3].id;
    sim->carriage.location_id = sim->player.location_id;
    sim->player.coins = 100;
    sim->player.cargo[CC_GOOD_TOOLS] = 3;
    CcSituation *warrant = ActiveSituation(
        sim, CC_SITUATION_MONSTER_EXPEDITION);
    CC_CHECK(warrant != NULL);
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = warrant->id
    };
    CC_CHECK(CcSimApply(sim, &accept, error, error_capacity));
}

int main(void)
{
    char error[192];

    CcSim official;
    CcSimInit(&official, UINT32_C(0xc0a71a9e));
    official.player.coins = 120;
    official.routes[0].security = 100;
    official.routes[0].condition = 100;
    official.routes[2].security = 100;
    official.routes[2].condition = 100;

    CcSituation *relief = ActiveSituation(
        &official, CC_SITUATION_RELIEF_DELIVERY);
    CC_CHECK(relief != NULL);
    CcCommand accept_relief = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = relief->id
    };
    CC_CHECK(CcSimApply(&official, &accept_relief, error, sizeof(error)));
    CcCommand food = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = relief->quantity
    };
    CC_CHECK(CcSimApply(&official, &food, error, sizeof(error)));
    TravelAndArrive(&official, official.settlements[1].id,
                    error, sizeof(error));

    CcCommand cross_bridge = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = official.settlements[2].id
    };
    CC_CHECK(CcSimApply(&official, &cross_bridge, error, sizeof(error)));
    AdvanceUntilStop(&official);
    CC_CHECK(official.journey.active);
    CC_CHECK(official.journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    int32_t condition_before = official.carriage.condition;
    CcCommand fight = {.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT};
    CC_CHECK(CcSimApply(&official, &fight, error, sizeof(error)));
    CC_CHECK(official.carriage.condition < condition_before);
    AdvanceUntilStop(&official);
    CC_CHECK(!official.journey.active);
    CC_CHECK(official.routes[1].closed);

    const CcMap *mine_map = CcSimMapForRoute(
        &official, official.routes[2].id, official.player.location_id);
    CC_CHECK(mine_map != NULL);
    CcCommand buy_mine_map = {
        .kind = CC_COMMAND_BUY_MAP,
        .target_id = mine_map->id
    };
    CC_CHECK(CcSimApply(&official, &buy_mine_map, error, sizeof(error)));
    TravelAndArrive(&official, official.settlements[3].id,
                    error, sizeof(error));
    int32_t food_before = official.settlements[3].stock[CC_GOOD_FOOD];
    CcCommand deliver = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -relief->quantity
    };
    CC_CHECK(CcSimApply(&official, &deliver, error, sizeof(error)));
    CC_CHECK(official.settlements[3].stock[CC_GOOD_FOOD] ==
             food_before + relief->quantity);
    CC_CHECK(relief->status == CC_SITUATION_RESOLVED);
    CC_CHECK(SituationStatus(&official, CC_SITUATION_ROUTE_REPAIR) ==
             CC_SITUATION_FAILED);
    CC_CHECK(SituationStatus(&official, CC_SITUATION_BLACK_MARKET_DELIVERY) ==
             CC_SITUATION_FAILED);
    CC_CHECK(official.delayed_echo.active);

    CcSim repair;
    CcSimInit(&repair, UINT32_C(0xc0a71a9e));
    repair.player.coins = 100;
    repair.routes[0].security = 100;
    repair.routes[0].condition = 100;
    TravelAndArrive(&repair, repair.settlements[1].id,
                    error, sizeof(error));
    CcSituation *bridge = ActiveSituation(
        &repair, CC_SITUATION_ROUTE_REPAIR);
    CC_CHECK(bridge != NULL);
    CcCommand accept_bridge = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = bridge->id
    };
    CC_CHECK(CcSimApply(&repair, &accept_bridge, error, sizeof(error)));
    CcCommand paid_repair = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = repair.routes[1].id,
        .amount = 2
    };
    repair.player.cargo[CC_GOOD_TOOLS] = 2;
    CC_CHECK(CcSimApply(&repair, &paid_repair, error, sizeof(error)));
    CC_CHECK(repair.player.cargo[CC_GOOD_TOOLS] == 2);
    CC_CHECK(!repair.routes[1].closed);
    CC_CHECK(repair.routes[1].condition == 76);
    CC_CHECK(bridge->status == CC_SITUATION_RESOLVED);
    CC_CHECK(SituationStatus(&repair, CC_SITUATION_RELIEF_DELIVERY) ==
             CC_SITUATION_FAILED);
    CC_CHECK(SituationStatus(&repair, CC_SITUATION_BLACK_MARKET_DELIVERY) ==
             CC_SITUATION_FAILED);

    CcSim tool_repair;
    CcSimInit(&tool_repair, UINT32_C(0xc0a71a9e));
    tool_repair.routes[0].security = 100;
    tool_repair.routes[0].condition = 100;
    TravelAndArrive(&tool_repair, tool_repair.settlements[1].id,
                    error, sizeof(error));
    CcSituation *tool_bridge = ActiveSituation(
        &tool_repair, CC_SITUATION_ROUTE_REPAIR);
    CC_CHECK(tool_bridge != NULL);
    CcCommand accept_tool_bridge = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = tool_bridge->id
    };
    CC_CHECK(CcSimApply(&tool_repair, &accept_tool_bridge,
                        error, sizeof(error)));
    tool_repair.player.cargo[CC_GOOD_TOOLS] = 2;
    int32_t tool_start_day = tool_repair.current_day;
    CcCommand crew_repair = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = tool_repair.routes[1].id,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&tool_repair, &crew_repair, error, sizeof(error)));
    CC_CHECK(tool_repair.current_day == tool_start_day + 1);
    CC_CHECK(tool_repair.routes[1].condition == 92);

    CcSim public_mine;
    CcSim hidden_mine;
    CcSim sealed_mine;
    PrepareDungeonChoice(&public_mine, UINT32_C(0xd009e0),
                         error, sizeof(error));
    PrepareDungeonChoice(&hidden_mine, UINT32_C(0xd009e0),
                         error, sizeof(error));
    PrepareDungeonChoice(&sealed_mine, UINT32_C(0xd009e0),
                         error, sizeof(error));
    int32_t base_material = public_mine.settlements[3].production[CC_GOOD_MATERIAL];
    int32_t base_bandit_influence = public_mine.bandits[0].influence;
    CcCommand public_route = {
        .kind = CC_COMMAND_CHANGE_DUNGEON,
        .target_id = public_mine.dungeons[0].id,
        .dungeon_state = CC_DUNGEON_PUBLIC_ROUTE
    };
    CcCommand hidden_route = {
        .kind = CC_COMMAND_CHANGE_DUNGEON,
        .target_id = hidden_mine.dungeons[0].id,
        .dungeon_state = CC_DUNGEON_SMUGGLER_ROUTE
    };
    CcCommand lasting_seal = {
        .kind = CC_COMMAND_CHANGE_DUNGEON,
        .target_id = sealed_mine.dungeons[0].id,
        .dungeon_state = CC_DUNGEON_RESEALED
    };
    CC_CHECK(CcSimApply(&public_mine, &public_route, error, sizeof(error)));
    CC_CHECK(CcSimApply(&hidden_mine, &hidden_route, error, sizeof(error)));
    CC_CHECK(CcSimApply(&sealed_mine, &lasting_seal, error, sizeof(error)));
    CC_CHECK(!public_mine.routes[6].smuggler_route);
    CC_CHECK(public_mine.settlements[3].production[CC_GOOD_MATERIAL] >
             base_material);
    CC_CHECK(hidden_mine.bandits[0].influence > base_bandit_influence);
    CC_CHECK(sealed_mine.routes[6].closed);
    CC_CHECK(sealed_mine.settlements[3].production[CC_GOOD_MATERIAL] <
             base_material);
    CC_CHECK(sealed_mine.monsters[0].pressure <
             public_mine.monsters[0].pressure);

    CC_CHECK(CcSimValidate(&official, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&repair, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&tool_repair, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&public_mine, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&hidden_mine, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&sealed_mine, error, sizeof(error)));
    puts("Empty Granary scenario tests passed");
    return 0;
}
