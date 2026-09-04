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
    char error[192];
    while (sim->journey.active) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
            CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
        } else if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {
                .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                    CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
            };
            CC_CHECK(CcSimApply(sim, &rest, error, sizeof(error)));
        } else {
            break;
        }
    }
}

static void TravelAndArrive(CcSim *sim, CcId destination, char *error,
                            size_t error_capacity)
{
    CcSettlement *origin = CcSimSettlementMutable(
        sim, sim->player.location_id);
    CC_CHECK(origin != NULL);
    if (origin->stock[CC_GOOD_WHEAT] < 16) {
        origin->stock[CC_GOOD_WHEAT] = 16;
    }
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
    CcCommand follow_lead = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = warrant->id,
        .amount = CC_CHARACTER_RESPONSE_LISTEN
    };
    CC_CHECK(CcSimApply(sim, &follow_lead, error, error_capacity));
    CC_CHECK(CcSimApply(sim, &follow_lead, error, error_capacity));
    follow_lead.amount = CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE;
    CC_CHECK(CcSimApply(sim, &follow_lead, error, error_capacity));
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = warrant->id
    };
    CC_CHECK(CcSimApply(sim, &accept, error, error_capacity));
    /* This scenario isolates the strategic outcomes after a successful delve.
       Room-by-room expedition behavior is covered by underroad_tests. */
    sim->dungeons[0].rooms[19].state_flags |=
        CC_DUNGEON_ROOM_OBJECTIVE_REACHED;
    for (int32_t i = 0; i < sim->dungeons[0].link_count; ++i) {
        CcDungeonLink *link = &sim->dungeons[0].links[i];
        if (link->kind == CC_DUNGEON_LINK_SHORTCUT) {
            link->flags |= CC_DUNGEON_LINK_DISCOVERED |
                           CC_DUNGEON_LINK_OPEN;
        }
    }
    sim->dungeons[0].rooms[20].state_flags |=
        CC_DUNGEON_ROOM_SEARCHED;
    sim->dungeons[0].rooms[4].state_flags |=
        CC_DUNGEON_ROOM_SEARCHED;
}

int main(void)
{
    char error[192];

    CcSim tolled_road;
    CcSimInit(&tolled_road, UINT32_C(0x7011ed));
    tolled_road.player.coins = 100;
    tolled_road.routes[0].closed = true;
    tolled_road.routes[0].security = 100;
    tolled_road.routes[0].condition = 100;
    CcMoney tolled_gold_before = CcSimTrackedGold(&tolled_road);
    CcMoney toll_treasury_before = tolled_road.kingdoms[0].treasury;
    CcMoney origin_market_before = tolled_road.settlements[0].market_coins;
    CcCommand pay_toll = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = tolled_road.settlements[1].id
    };
    CC_CHECK(CcSimApply(&tolled_road, &pay_toll,
                        error, sizeof(error)));
    CC_CHECK(tolled_road.journey.active);
    CC_CHECK(tolled_road.journey.fare_reserved ==
             tolled_road.routes[0].travel_days + 4);
    CC_CHECK(tolled_road.kingdoms[0].treasury ==
             toll_treasury_before + 4);
    CC_CHECK(tolled_road.settlements[0].market_coins ==
             origin_market_before + tolled_road.routes[0].travel_days);
    CC_CHECK(CcSimTrackedGold(&tolled_road) == tolled_gold_before);

    CcSim dispatch;
    CcSimInit(&dispatch, UINT32_C(0xc0a71e12));
    for (int32_t kingdom = 0;
         kingdom < dispatch.kingdom_count; ++kingdom) {
        dispatch.dragon.hoard += dispatch.kingdoms[kingdom].treasury;
        dispatch.kingdoms[kingdom].treasury = 0;
        dispatch.kingdoms[kingdom].legitimacy = 100;
    }
    CcSimAdvanceDays(&dispatch, 27);
    CC_CHECK(dispatch.courier_count > 0);
    CcCourier *seal = &dispatch.couriers[0];
    CC_CHECK(seal->status == CC_COURIER_WAITING);
    CC_CHECK(!CcSimKingdomsAllied(
        &dispatch, seal->issuer_kingdom_id,
        seal->recipient_kingdom_id));
    dispatch.player.location_id = seal->current_settlement_id;
    dispatch.carriage.location_id = seal->current_settlement_id;
    dispatch.player.coins = 100;
    dispatch.maps[6].owner_id = dispatch.player.id;
    for (int32_t i = 0; i < dispatch.route_count; ++i) {
        dispatch.routes[i].security = 100;
        dispatch.routes[i].condition = 100;
        dispatch.routes[i].closed = false;
    }
    dispatch.routes[6].smuggler_route = false;
    dispatch.bandit_count = 0;
    CcSituation *dispatch_offer = (CcSituation *)CcSimSituation(
        &dispatch, seal->situation_id);
    CC_CHECK(dispatch_offer != NULL);
    CcCommand carry_seal = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = dispatch_offer->id
    };
    CC_CHECK(CcSimApply(&dispatch, &carry_seal,
                        error, sizeof(error)));
    CC_CHECK(seal->status == CC_COURIER_WITH_PLAYER);
    TravelAndArrive(&dispatch, seal->destination_settlement_id,
                    error, sizeof(error));
    CC_CHECK(seal->status == CC_COURIER_DELIVERED);
    CC_CHECK(dispatch_offer->status == CC_SITUATION_RESOLVED);
    CC_CHECK(CcSimKingdomsAllied(
        &dispatch, seal->issuer_kingdom_id,
        seal->recipient_kingdom_id));

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
    CC_CHECK(official.player.cargo[CC_GOOD_FOOD] == relief->quantity);
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
    tool_repair.player.cargo[CC_GOOD_WOOD] = 0;
    int32_t tool_start_day = tool_repair.current_day;
    CcCommand crew_repair = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = tool_repair.routes[1].id,
        .amount = 1
    };
    CC_CHECK(!CcSimApply(&tool_repair, &crew_repair,
                         error, sizeof(error)));
    CC_CHECK(tool_repair.player.cargo[CC_GOOD_TOOLS] == 2);
    CC_CHECK(tool_repair.routes[1].closed);
    tool_repair.player.cargo[CC_GOOD_WOOD] = 2;
    CC_CHECK(CcSimApply(&tool_repair, &crew_repair, error, sizeof(error)));
    CC_CHECK(tool_repair.current_day == tool_start_day + 1);
    CC_CHECK(tool_repair.routes[1].condition == 92);
    CC_CHECK(tool_repair.player.cargo[CC_GOOD_TOOLS] == 0);
    CC_CHECK(tool_repair.player.cargo[CC_GOOD_WOOD] == 0);

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
    CcMoney public_gold = CcSimTrackedGold(&public_mine);
    CcMoney mine_market_coins = public_mine.settlements[3].market_coins;
    CC_CHECK(CcSimApply(&public_mine, &public_route, error, sizeof(error)));
    CC_CHECK(CcSimApply(&hidden_mine, &hidden_route, error, sizeof(error)));
    CC_CHECK(CcSimApply(&sealed_mine, &lasting_seal, error, sizeof(error)));
    CC_CHECK(!public_mine.routes[6].smuggler_route);
    CC_CHECK(CcSimTrackedGold(&public_mine) == public_gold);
    CC_CHECK(public_mine.settlements[3].market_coins ==
             mine_market_coins + 12);
    CC_CHECK(public_mine.settlements[3].production[CC_GOOD_MATERIAL] >
             base_material);
    CC_CHECK(hidden_mine.bandits[0].influence > base_bandit_influence);
    CC_CHECK(sealed_mine.routes[6].closed);
    CC_CHECK(sealed_mine.settlements[3].production[CC_GOOD_MATERIAL] <
             base_material);
    CC_CHECK(sealed_mine.monsters[0].pressure <
             public_mine.monsters[0].pressure);

    int32_t public_day = public_mine.current_day;
    int32_t public_tools = public_mine.player.cargo[CC_GOOD_TOOLS];
    int32_t public_production =
        public_mine.settlements[3].production[CC_GOOD_MATERIAL];
    int32_t public_capacity = public_mine.routes[6].capacity;
    CcMoney public_coins = public_mine.player.coins;
    CC_CHECK(!CcSimApply(&public_mine, &public_route,
                         error, sizeof(error)));
    CC_CHECK(public_mine.current_day == public_day);
    CC_CHECK(public_mine.player.cargo[CC_GOOD_TOOLS] == public_tools);
    CC_CHECK(public_mine.player.coins == public_coins);
    CC_CHECK(public_mine.settlements[3].production[CC_GOOD_MATERIAL] ==
             public_production);
    CC_CHECK(public_mine.routes[6].capacity == public_capacity);

    CcId dungeon_road_id = public_mine.routes[6].id;
    public_mine.player.cargo[CC_GOOD_TOOLS] += 5;
    CC_CHECK(CcSimApply(&public_mine, &lasting_seal,
                        error, sizeof(error)));
    CC_CHECK(public_mine.routes[6].id == dungeon_road_id);
    CC_CHECK(public_mine.routes[6].closed);
    public_mine.player.cargo[CC_GOOD_TOOLS] += 2;
    CC_CHECK(CcSimApply(&public_mine, &public_route,
                        error, sizeof(error)));
    CC_CHECK(public_mine.routes[6].id == dungeon_road_id);
    CC_CHECK(!public_mine.routes[6].closed &&
             !public_mine.routes[6].smuggler_route);
    CC_CHECK(public_mine.settlements[3].production[CC_GOOD_MATERIAL] ==
             public_production);
    CC_CHECK(public_mine.routes[6].capacity == public_capacity);
    CC_CHECK(CcSimTrackedGold(&public_mine) == public_gold);

    CC_CHECK(CcSimValidate(&official, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&tolled_road, error, sizeof(error)));
    bool dispatch_valid = CcSimValidate(&dispatch, error, sizeof(error));
    if (!dispatch_valid) (void)fprintf(stderr, "%s\n", error);
    CC_CHECK(dispatch_valid);
    CC_CHECK(CcSimValidate(&repair, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&tool_repair, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&public_mine, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&hidden_mine, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&sealed_mine, error, sizeof(error)));
    puts("Empty Granary scenario tests passed");
    return 0;
}
