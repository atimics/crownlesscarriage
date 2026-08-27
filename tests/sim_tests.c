#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static void AdvanceTravellingJourney(CcSim *sim)
{
    while (sim->journey.active &&
           sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    }
}

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
    AdvanceTravellingJourney(sim);
    CC_CHECK(!sim->journey.active);
    CcSimAdvanceDays(sim, 17);
}

static CcSituation *FirstActiveSituation(CcSim *sim, int32_t excluded)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (i != excluded &&
            sim->situations[i].status == CC_SITUATION_ACTIVE) {
            return &sim->situations[i];
        }
    }
    return NULL;
}

static CcSituation *PreparePromisedJourney(CcSim *sim, char *error,
                                           size_t error_capacity)
{
    CcSituation *situation = FirstActiveSituation(sim, -1);
    CC_CHECK(situation != NULL);
    situation->kind = CC_SITUATION_RELIEF_DELIVERY;
    situation->target_id = sim->settlements[1].id;
    situation->good = CC_GOOD_FOOD;
    situation->quantity = 1;
    situation->progress = 0;
    situation->reward = 20;
    situation->deadline_day = sim->current_day + 40;
    sim->player.cargo[CC_GOOD_FOOD] = 1;
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = situation->id
    };
    CC_CHECK(CcSimApply(sim, &accept, error, error_capacity));
    sim->routes[0].closed = true;
    const CcMap *map = CcSimMapForRoute(sim, sim->routes[0].id,
                                        sim->player.id);
    CC_CHECK(map != NULL);
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim->settlements[1].id
    };
    CC_CHECK(CcSimApply(sim, &travel, error, error_capacity));
    CC_CHECK(sim->journey.active);
    CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    AdvanceTravellingJourney(sim);
    CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    return situation;
}

int main(void)
{
    CcSim first;
    CcSim second;
    char error[160];
    CcSimInit(&first, UINT32_C(0x12345678));
    CcSimInit(&second, UINT32_C(0x12345678));
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));

    CcSim realtime;
    CcSimInit(&realtime, UINT32_C(0x71ae71e));
    CcId realtime_origin = realtime.player.location_id;
    CcId realtime_destination = realtime.settlements[1].id;
    int32_t realtime_departure_day = realtime.current_day;
    CcMoney realtime_coins = realtime.player.coins;
    CcCommand realtime_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = realtime_destination
    };
    CC_CHECK(CcSimApply(&realtime, &realtime_travel,
                        error, sizeof(error)));
    CC_CHECK(realtime.player.location_id == realtime_origin);
    CC_CHECK(realtime.current_day == realtime_departure_day);
    CC_CHECK(realtime.player.coins ==
             realtime_coins - realtime.journey.fare_reserved);
    CC_CHECK(realtime.carriage.mode == CC_CARRIAGE_MOVING);
    CcSimAdvanceRuntimeTicks(&realtime, 12);
    CC_CHECK(realtime.clock.tick == 12U);
    CC_CHECK(realtime.clock.minute_subticks ==
             CC_TRAVEL_GAME_MINUTES_PER_SECOND * 12);
    CC_CHECK(realtime.carriage.progress_milli > 0);
    int32_t realtime_days = realtime.routes[0].travel_days;
    AdvanceTravellingJourney(&realtime);
    CC_CHECK(!realtime.journey.active);
    CC_CHECK(realtime.player.location_id == realtime_destination);
    CC_CHECK(realtime.current_day == realtime_departure_day + realtime_days);
    CC_CHECK(realtime.carriage.mode == CC_CARRIAGE_PARKED);

    CcSim fine_ticks;
    CcSim batched_ticks;
    CcSimInit(&fine_ticks, UINT32_C(0xf17ed));
    CcSimInit(&batched_ticks, UINT32_C(0xf17ed));
    CcCommand batching_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = fine_ticks.settlements[1].id
    };
    CC_CHECK(CcSimApply(&fine_ticks, &batching_travel,
                        error, sizeof(error)));
    batching_travel.target_id = batched_ticks.settlements[1].id;
    CC_CHECK(CcSimApply(&batched_ticks, &batching_travel,
                        error, sizeof(error)));
    for (int32_t tick = 0; tick < 1200; ++tick) {
        CcSimAdvanceRuntimeTicks(&fine_ticks, 1);
    }
    for (int32_t batch = 0; batch < 20; ++batch) {
        CcSimAdvanceRuntimeTicks(&batched_ticks,
                                 CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(CcSimHash(&fine_ticks) == CcSimHash(&batched_ticks));
    ApplySequence(&first);
    ApplySequence(&second);
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    CC_CHECK(first.map_count == first.route_count);
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcSimMapForRoute(&first, first.routes[0].id,
                             first.player.id) != NULL);
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

    CcSim uncharted;
    CcSimInit(&uncharted, UINT32_C(0x12345678));
    uncharted.player.location_id = uncharted.settlements[1].id;
    uncharted.carriage.location_id = uncharted.player.location_id;
    CcCommand uncharted_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = uncharted.settlements[5].id
    };
    CC_CHECK(CcSimMapForRoute(&uncharted, uncharted.routes[5].id,
                              uncharted.player.id) == NULL);
    CC_CHECK(CcSimApply(&uncharted, &uncharted_travel,
                        error, sizeof(error)));
    CC_CHECK(uncharted.journey.total_subticks ==
             (uncharted.routes[5].travel_days + 2) *
                 CC_WORLD_DAY_SUBTICKS);

    CcSim commitment;
    CcSimInit(&commitment, UINT32_C(0xc011ab1e));
    CcSituation *first_charter = FirstActiveSituation(&commitment, -1);
    CC_CHECK(first_charter != NULL);
    int32_t first_slot = (int32_t)(first_charter - commitment.situations);
    CcSituation *second_charter = FirstActiveSituation(&commitment, first_slot);
    CC_CHECK(second_charter != NULL);
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = first_charter->id
    };
    CC_CHECK(CcSimApply(&commitment, &accept, error, sizeof(error)));
    CC_CHECK(CcSimAcceptedSituation(&commitment) == first_charter);
    CcCommand second_accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = second_charter->id
    };
    CC_CHECK(!CcSimApply(&commitment, &second_accept, error, sizeof(error)));
    CcCommand abandon = {
        .kind = CC_COMMAND_ABANDON_SITUATION,
        .target_id = first_charter->id
    };
    CC_CHECK(CcSimApply(&commitment, &abandon, error, sizeof(error)));
    CC_CHECK(CcSimAcceptedSituation(&commitment) == NULL &&
             first_charter->status == CC_SITUATION_ACTIVE);

    CcSim laundering;
    CcSimInit(&laundering, UINT32_C(0x1a0d3e));
    CcSituation *delivery = NULL;
    for (int32_t i = 0; i < laundering.situation_count; ++i) {
        if (laundering.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            delivery = &laundering.situations[i];
            break;
        }
    }
    CC_CHECK(delivery != NULL);
    CcCommand accept_delivery = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = delivery->id
    };
    CC_CHECK(CcSimApply(&laundering, &accept_delivery,
                        error, sizeof(error)));
    laundering.player.location_id = delivery->target_id;
    laundering.carriage.location_id = delivery->target_id;
    laundering.player.coins = 500;
    CcCommand local_food = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = delivery->quantity
    };
    CC_CHECK(CcSimApply(&laundering, &local_food, error, sizeof(error)));
    int32_t local_cargo = laundering.player.cargo[CC_GOOD_FOOD];
    CcCommand fake_delivery = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -delivery->quantity
    };
    CC_CHECK(!CcSimApply(&laundering, &fake_delivery,
                         error, sizeof(error)));
    CC_CHECK(laundering.player.cargo[CC_GOOD_FOOD] == local_cargo);
    CC_CHECK(delivery->progress == 0 &&
             delivery->status == CC_SITUATION_ACTIVE);

    CcSim unanswered;
    CcSimInit(&unanswered, UINT32_C(0xc011ab1e));
    CcSituation *unanswered_charter = FirstActiveSituation(&unanswered, -1);
    CC_CHECK(unanswered_charter != NULL);
    unanswered.current_day = 6;
    unanswered_charter->deadline_day = 6;
    int32_t reputation_before = unanswered.player.reputation;
    CcSimAdvanceDays(&unanswered, 1);
    CC_CHECK(unanswered.player.reputation == reputation_before);

    CcSim promised;
    CcSimInit(&promised, UINT32_C(0xc011ab1e));
    CcSituation *promised_charter = FirstActiveSituation(&promised, -1);
    CC_CHECK(promised_charter != NULL);
    CcCommand promise = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = promised_charter->id
    };
    CC_CHECK(CcSimApply(&promised, &promise, error, sizeof(error)));
    promised.current_day = 6;
    promised_charter->deadline_day = 6;
    reputation_before = promised.player.reputation;
    CcSimAdvanceDays(&promised, 1);
    CC_CHECK(promised.player.reputation == reputation_before - 1 &&
             CcSimAcceptedSituation(&promised) == NULL);

    CcSim defended_road;
    CcSimInit(&defended_road, UINT32_C(0x50adca11));
    defended_road.bandits[0].route_id = defended_road.routes[0].id;
    CcSituation *journey_charter = PreparePromisedJourney(
        &defended_road, error, sizeof(error));
    CcId journey_origin = defended_road.player.location_id;
    CcId journey_destination = defended_road.journey.destination_id;
    int32_t route_security = defended_road.routes[0].security;
    int32_t destination_population = defended_road.settlements[1].population;
    int32_t bandit_members = defended_road.bandits[0].members;
    int32_t shipment_count = defended_road.shipment_count;
    int32_t carriage_condition = defended_road.carriage.condition;
    CcMoney combat_coins = defended_road.player.coins;
    CC_CHECK(journey_origin != journey_destination);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->kind ==
             CC_EVENT_JOURNEY_ENCOUNTER);
    int32_t blocked_day = defended_road.current_day;
    int32_t blocked_time = defended_road.clock.minute_subticks;
    int32_t blocked_progress = defended_road.carriage.progress_milli;
    CcSimAdvanceRuntimeTicks(&defended_road,
                             CC_WORLD_TICKS_PER_SECOND * 10);
    CC_CHECK(defended_road.current_day == blocked_day);
    CC_CHECK(defended_road.clock.minute_subticks == blocked_time);
    CC_CHECK(defended_road.carriage.progress_milli == blocked_progress);
    CcCommand defend = {.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT};
    CC_CHECK(CcSimApply(&defended_road, &defend, error, sizeof(error)));
    CC_CHECK(defended_road.journey.active);
    CC_CHECK(defended_road.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(defended_road.player.location_id == journey_origin);
    CC_CHECK(defended_road.routes[0].security > route_security);
    CC_CHECK(defended_road.settlements[1].population ==
             destination_population);
    CC_CHECK(defended_road.carriage.condition < carriage_condition);
    CC_CHECK(defended_road.player.coins < combat_coins);
    CC_CHECK(defended_road.bandits[0].members < bandit_members);
    CC_CHECK(defended_road.shipment_count >= shipment_count + 1);
    CC_CHECK(defended_road.shipments[defended_road.shipment_count - 1].status ==
             CC_SHIPMENT_TRAVELLING);
    AdvanceTravellingJourney(&defended_road);
    CC_CHECK(!defended_road.journey.active);
    CC_CHECK(defended_road.player.location_id == journey_destination);

    defended_road.player.cargo[CC_GOOD_FOOD] = 1;
    CcCommand fulfill = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -1
    };
    CC_CHECK(CcSimApply(&defended_road, &fulfill, error, sizeof(error)));
    CC_CHECK(journey_charter->status == CC_SITUATION_RESOLVED);
    CC_CHECK(defended_road.delayed_echo.active);
    CcSimAdvanceDays(&defended_road, 30);
    CC_CHECK(defended_road.delayed_echo.active);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->kind ==
             CC_EVENT_DELAYED_ECHO);
    CcSimAdvanceDays(&defended_road, 30);
    CC_CHECK(!defended_road.delayed_echo.active);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->kind ==
             CC_EVENT_DELAYED_ECHO);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->parent_id != 0U);

    CcSim bargained_road;
    CcSimInit(&bargained_road, UINT32_C(0x50adca11));
    bargained_road.bandits[0].route_id = bargained_road.routes[0].id;
    (void)PreparePromisedJourney(&bargained_road, error, sizeof(error));
    route_security = bargained_road.routes[0].security;
    int32_t bandit_influence = bargained_road.bandits[0].influence;
    CcMoney coins_before_bargain = bargained_road.player.coins;
    int32_t bargain_cost = bargained_road.journey.bargain_cost;
    CcCommand bargain = {
        .kind = CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE
    };
    CC_CHECK(CcSimApply(&bargained_road, &bargain,
                        error, sizeof(error)));
    CC_CHECK(bargained_road.journey.active);
    CC_CHECK(bargained_road.routes[0].security < route_security);
    CC_CHECK(bargained_road.bandits[0].influence > bandit_influence);
    CC_CHECK(bargained_road.player.coins <=
             coins_before_bargain - bargain_cost);

    CcSim conserved;
    CcSimInit(&conserved, UINT32_C(0xc01d1ed6));
    CcMoney initial_gold = CcSimTrackedGold(&conserved);
    CcSimAdvanceDays(&conserved, 3650);
    CC_CHECK(CcSimTrackedGold(&conserved) == initial_gold);
    CC_CHECK(CcSimValidate(&conserved, error, sizeof(error)));

    CC_CHECK(CcSimValidate(&first, error, sizeof(error)));
    CcSim different;
    CcSimInit(&different, UINT32_C(0x87654321));
    CC_CHECK(CcSimHash(&first) != CcSimHash(&different));
    puts("deterministic simulation tests passed");
    return 0;
}
