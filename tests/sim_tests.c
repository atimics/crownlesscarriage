#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

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
    int32_t travelling_food = realtime.player.cargo[CC_GOOD_FOOD];
    int32_t origin_food = realtime.settlements[0].stock[CC_GOOD_FOOD];
    CcMoney travelling_coins = realtime.player.coins;
    CcCommand roadside_trade = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 1
    };
    CC_CHECK(!CcSimApply(&realtime, &roadside_trade,
                         error, sizeof(error)));
    CC_CHECK(realtime.player.cargo[CC_GOOD_FOOD] == travelling_food);
    CC_CHECK(realtime.settlements[0].stock[CC_GOOD_FOOD] == origin_food);
    CC_CHECK(realtime.player.coins == travelling_coins);
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
    CC_CHECK(first.map_count == CC_MAP_COLLECTION_COUNT);
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcPlayerMapCollectionCount(&first) == 1);
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
    CC_CHECK(CcPlayerMapCollectionCount(&first) == 2);
    CC_CHECK(CcSimMap(&first, offered_id)->owner_id == first.player.id);
    CcCommand sell_map = {.kind = CC_COMMAND_SELL_MAP, .target_id = offered_id};
    CC_CHECK(CcSimApply(&first, &sell_map, error, sizeof(error)));
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcPlayerMapCollectionCount(&first) == 2);
    CC_CHECK(CcSimMap(&first, offered_id)->owner_id ==
             first.player.location_id);

    const CcMap *illustrated = NULL;
    for (int32_t i = 0; i < first.map_count; ++i) {
        if (strcmp(first.maps[i].name,
                   CC_GLOAMGATE_ALDERWATCH_MAP_NAME) == 0) {
            illustrated = &first.maps[i];
            break;
        }
    }
    CC_CHECK(illustrated != NULL);
    CC_CHECK(strcmp(first.settlements[1].name, "Gloamgate") == 0);
    CC_CHECK(strcmp(first.settlements[2].name, "Alderwatch") == 0);
    CC_CHECK(illustrated->route_id == first.routes[1].id);
    CC_CHECK(illustrated->owner_id == first.settlements[1].id);
    CC_CHECK(illustrated->ask_price == 24);
    first.player.location_id = first.settlements[1].id;
    CcCommand buy_illustrated = {
        .kind = CC_COMMAND_BUY_MAP,
        .target_id = illustrated->id
    };
    CC_CHECK(CcSimApply(&first, &buy_illustrated, error, sizeof(error)));
    CC_CHECK(CcSimMap(&first, illustrated->id)->owner_id == first.player.id);
    CC_CHECK(CcPlayerMapCount(&first) == 2);
    CcCommand archive_illustrated = {
        .kind = CC_COMMAND_ARCHIVE_MAP,
        .target_id = illustrated->id
    };
    CC_CHECK(CcSimApply(&first, &archive_illustrated,
                        error, sizeof(error)));
    CC_CHECK(CcSimMapIsArchived(&first, illustrated));
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcSimMapForRoute(&first, first.routes[1].id,
                             first.player.id) == NULL);
    CcCommand retrieve_illustrated = {
        .kind = CC_COMMAND_RETRIEVE_MAP,
        .target_id = illustrated->id
    };
    CC_CHECK(CcSimApply(&first, &retrieve_illustrated,
                        error, sizeof(error)));
    CC_CHECK(!CcSimMapIsArchived(&first, illustrated));
    CC_CHECK(CcPlayerMapCount(&first) == 2);

    CcSim collector;
    CcSimInit(&collector, UINT32_C(0xc011ec7));
    collector.player.coins = 10000;
    collector.player.location_id = collector.settlements[1].id;
    CcCommand store_starting = {
        .kind = CC_COMMAND_ARCHIVE_MAP,
        .target_id = collector.maps[CC_MAP_THORNFORD_FORDINGS].id
    };
    CC_CHECK(CcSimApply(&collector, &store_starting,
                        error, sizeof(error)));
    for (int32_t i = 0; i < CC_MAP_CROWNLESS_ATLAS; ++i) {
        CcMap *map = &collector.maps[i];
        if (map->owner_id == collector.player.id) continue;
        collector.player.location_id = map->owner_id;
        CcCommand collect = {
            .kind = CC_COMMAND_BUY_MAP,
            .target_id = map->id
        };
        CC_CHECK(CcSimApply(&collector, &collect, error, sizeof(error)));
        collector.player.location_id = collector.settlements[1].id;
        CcCommand store = {
            .kind = CC_COMMAND_ARCHIVE_MAP,
            .target_id = map->id
        };
        CC_CHECK(CcSimApply(&collector, &store, error, sizeof(error)));
    }
    const CcMap *atlas = &collector.maps[CC_MAP_CROWNLESS_ATLAS];
    CC_CHECK(CcPlayerMapCollectionCount(&collector) ==
             CC_MAP_COLLECTION_COUNT);
    CC_CHECK(CcSimMapIsCatalogued(&collector, atlas));
    CC_CHECK(CcSimMapIsArchived(&collector, atlas));
    CcCommand retrieve_atlas = {
        .kind = CC_COMMAND_RETRIEVE_MAP,
        .target_id = atlas->id
    };
    CC_CHECK(CcSimApply(&collector, &retrieve_atlas,
                        error, sizeof(error)));
    CC_CHECK(!CcSimMapIsArchived(&collector, atlas));
    collector.carriage.location_id = collector.player.location_id;
    CC_CHECK(CcSimValidate(&collector, error, sizeof(error)));

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
    CcTravelPreview uncharted_preview = {0};
    CC_CHECK(CcSimTravelPreview(&uncharted,
                                uncharted_travel.target_id,
                                &uncharted_preview,
                                error, sizeof(error)));
    CC_CHECK(!uncharted_preview.charted);
    CC_CHECK(uncharted_preview.destination_known);
    CC_CHECK(uncharted_preview.travel_days ==
             uncharted.routes[5].travel_days + 2);
    CC_CHECK(CcSimApply(&uncharted, &uncharted_travel,
                        error, sizeof(error)));
    CC_CHECK(uncharted.journey.total_subticks ==
             (uncharted.routes[5].travel_days + 2) *
                 CC_WORLD_DAY_SUBTICKS);

    CcSim hidden_fork;
    CcSimInit(&hidden_fork, UINT32_C(0xf04c));
    hidden_fork.player.location_id = hidden_fork.settlements[1].id;
    hidden_fork.carriage.location_id = hidden_fork.player.location_id;
    const CcRoute *night_road = &hidden_fork.routes[6];
    CC_CHECK(night_road->smuggler_route);
    CC_CHECK(CcSimMapForRoute(&hidden_fork, night_road->id,
                              hidden_fork.player.id) == NULL);
    CcId hidden_destination = night_road->from_id ==
            hidden_fork.player.location_id ?
        night_road->to_id : night_road->from_id;
    CcTravelPreview hidden_preview = {0};
    CC_CHECK(CcSimTravelPreview(&hidden_fork, hidden_destination,
                                &hidden_preview, error, sizeof(error)));
    CC_CHECK(!hidden_preview.charted);
    CC_CHECK(!hidden_preview.destination_known);
    CC_CHECK(hidden_preview.travel_days == night_road->travel_days + 2);
    CcCommand take_hidden_fork = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = hidden_destination
    };
    CC_CHECK(CcSimApply(&hidden_fork, &take_hidden_fork,
                        error, sizeof(error)));
    CC_CHECK(hidden_fork.journey.route_id == night_road->id);

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

    CcSim washed_load;
    CcSimInit(&washed_load, UINT32_C(0x1a0d3e));
    CcSituation *washed_delivery = NULL;
    for (int32_t i = 0; i < washed_load.situation_count; ++i) {
        if (washed_load.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            washed_delivery = &washed_load.situations[i];
            break;
        }
    }
    CC_CHECK(washed_delivery != NULL);
    washed_delivery->target_id = washed_load.settlements[3].id;
    washed_delivery->quantity = 1;
    washed_delivery->progress = 0;
    washed_delivery->deadline_day = washed_load.current_day + 60;
    washed_load.player.coins = 500;
    washed_load.player.cargo[CC_GOOD_FOOD] = 1;
    washed_load.routes[0].security = 100;
    washed_load.routes[0].condition = 100;
    washed_load.routes[6].security = 100;
    washed_load.routes[6].condition = 100;
    washed_load.routes[6].smuggler_route = false;
    washed_load.bandit_count = 0;
    washed_load.monster_count = 0;
    CcCommand accept_washed = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = washed_delivery->id
    };
    CC_CHECK(CcSimApply(&washed_load, &accept_washed,
                        error, sizeof(error)));
    CcCommand wrong_way = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = washed_load.settlements[1].id
    };
    CC_CHECK(CcSimApply(&washed_load, &wrong_way, error, sizeof(error)));
    CcCommand roadside_abandon = {
        .kind = CC_COMMAND_ABANDON_SITUATION,
        .target_id = washed_delivery->id
    };
    CC_CHECK(!CcSimApply(&washed_load, &roadside_abandon,
                         error, sizeof(error)));
    CC_CHECK(washed_load.player.accepted_situation_id ==
             washed_delivery->id);
    AdvanceTravellingJourney(&washed_load);
    CC_CHECK(washed_load.resolved_journey_situation_id == 0U);
    CcCommand sell_load = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -1
    };
    CC_CHECK(CcSimApply(&washed_load, &sell_load, error, sizeof(error)));
    CcCommand empty_arrival = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = washed_delivery->target_id
    };
    CC_CHECK(CcSimApply(&washed_load, &empty_arrival,
                        error, sizeof(error)));
    AdvanceTravellingJourney(&washed_load);
    CC_CHECK(washed_load.resolved_journey_situation_id == 0U);
    CcCommand buy_local_replacement = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&washed_load, &buy_local_replacement,
                        error, sizeof(error)));
    CcCommand deliver_replacement = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -1
    };
    CC_CHECK(!CcSimApply(&washed_load, &deliver_replacement,
                         error, sizeof(error)));
    CC_CHECK(washed_delivery->status == CC_SITUATION_ACTIVE &&
             washed_delivery->progress == 0);

    CcSim unanswered;
    CcSimInit(&unanswered, UINT32_C(0xc011ab1e));
    CcSituation *unanswered_charter = FirstActiveSituation(&unanswered, -1);
    CC_CHECK(unanswered_charter != NULL);
    unanswered.current_day = 36;
    unanswered_charter->deadline_day = 36;
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
    promised.current_day = 36;
    promised_charter->deadline_day = 36;
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
    CcMoney gold_before_bargain = CcSimTrackedGold(&bargained_road);
    int32_t food_before_bargain = CcSimTrackedGood(
        &bargained_road, CC_GOOD_FOOD);
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
    CC_CHECK(CcSimTrackedGold(&bargained_road) == gold_before_bargain);
    CC_CHECK(CcSimTrackedGood(&bargained_road, CC_GOOD_FOOD) ==
             food_before_bargain);

    CcSim provisioned_road;
    CcSimInit(&provisioned_road, UINT32_C(0x50adca11));
    provisioned_road.bandits[0].route_id = provisioned_road.routes[0].id;
    (void)PreparePromisedJourney(&provisioned_road, error, sizeof(error));
    CcGood demanded_good = CC_GOOD_COUNT;
    int32_t demanded_quantity = 0;
    CC_CHECK(CcSimBanditProvisionDemand(
        &provisioned_road, provisioned_road.journey.route_id,
        &demanded_good, &demanded_quantity));
    CC_CHECK(demanded_good >= CC_GOOD_FOOD &&
             demanded_good <= CC_GOOD_WEAPONS && demanded_quantity > 0);
    int32_t reaction = CcSimBanditReactionRoll(
        &provisioned_road, provisioned_road.journey.route_id);
    CC_CHECK(reaction >= 2 && reaction <= 12);
    CC_CHECK(CcBanditReactionName(reaction) != NULL);
    provisioned_road.player.cargo[demanded_good] = demanded_quantity;
    CcMoney provision_coins = provisioned_road.player.coins;
    int32_t provision_supplies = provisioned_road.bandits[0].supplies;
    int32_t provision_influence = provisioned_road.bandits[0].influence;
    CcCommand provisions = {
        .kind = CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS
    };
    CC_CHECK(CcSimApply(&provisioned_road, &provisions,
                        error, sizeof(error)));
    CC_CHECK(provisioned_road.journey.phase ==
             CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(provisioned_road.player.cargo[demanded_good] == 0);
    CC_CHECK(provisioned_road.player.coins == provision_coins);
    CC_CHECK(provisioned_road.bandits[0].supplies > provision_supplies);
    CC_CHECK(provisioned_road.bandits[0].influence > provision_influence);
    const CcEvent *provision_event = NULL;
    for (int32_t offset = 0; offset < provisioned_road.event_count; ++offset) {
        const CcEvent *candidate = CcSimRecentEvent(&provisioned_road, offset);
        if (candidate != NULL &&
            candidate->kind == CC_EVENT_ENCOUNTER_NEGOTIATED) {
            provision_event = candidate;
            break;
        }
    }
    CC_CHECK(provision_event != NULL);
    CC_CHECK(strstr(provision_event->text,
                    provisioned_road.bandits[0].name) != NULL);

    CcSim empty_carriage;
    CcSimInit(&empty_carriage, UINT32_C(0x50adca11));
    empty_carriage.bandits[0].route_id = empty_carriage.routes[0].id;
    (void)PreparePromisedJourney(&empty_carriage, error, sizeof(error));
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        empty_carriage.player.cargo[good] = 0;
    }
    CC_CHECK(!CcSimApply(&empty_carriage, &provisions,
                         error, sizeof(error)));
    CC_CHECK(empty_carriage.journey.phase == CC_JOURNEY_PHASE_BLOCKED);

    CcSim withdrawn_road;
    CcSimInit(&withdrawn_road, UINT32_C(0x50adca11));
    withdrawn_road.bandits[0].route_id = withdrawn_road.routes[0].id;
    (void)PreparePromisedJourney(&withdrawn_road, error, sizeof(error));
    CcId withdrawal_origin = withdrawn_road.journey.origin_id;
    int32_t withdrawal_condition = withdrawn_road.carriage.condition;
    int32_t withdrawal_security = withdrawn_road.routes[0].security;
    int32_t withdrawal_influence = withdrawn_road.bandits[0].influence;
    CcMoney withdrawal_gold = CcSimTrackedGold(&withdrawn_road);
    CcCommand withdraw = {
        .kind = CC_COMMAND_WITHDRAW_ENCOUNTER,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&withdrawn_road, &withdraw,
                        error, sizeof(error)));
    CC_CHECK(!withdrawn_road.journey.active);
    CC_CHECK(withdrawn_road.journey.phase == CC_JOURNEY_PHASE_NONE);
    CC_CHECK(withdrawn_road.player.location_id == withdrawal_origin);
    CC_CHECK(withdrawn_road.carriage.mode == CC_CARRIAGE_PARKED);
    CC_CHECK(withdrawn_road.carriage.location_id == withdrawal_origin);
    CC_CHECK(withdrawn_road.carriage.condition < withdrawal_condition);
    CC_CHECK(withdrawn_road.routes[0].security < withdrawal_security);
    CC_CHECK(withdrawn_road.bandits[0].influence > withdrawal_influence);
    CC_CHECK(CcSimTrackedGold(&withdrawn_road) == withdrawal_gold);
    CC_CHECK(CcSimRecentEvent(&withdrawn_road, 0)->kind ==
             CC_EVENT_ENCOUNTER_WITHDRAWN);

    CcSim invalid_state;
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.player.cargo[CC_GOOD_FOOD] = -1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.kingdom_count = CC_MAX_KINGDOMS + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.settlements[0].stock[CC_GOOD_FOOD] =
        CC_SIM_MAX_UNITS + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.settlements[0].production[CC_GOOD_FOOD] = INT32_MAX;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.settlements[0].kingdom_id =
        CcMakeId(CC_ENTITY_KINGDOM, UINT64_C(999999));
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.factions[0].support = 101;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.monsters[0].dungeon_id =
        CcMakeId(CC_ENTITY_DUNGEON, UINT64_C(999999));
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.dungeons[0].regional_pressure = 101;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.current_day = CC_SIM_MAX_DAY + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));

    CcSim trade_edge;
    CcSimInit(&trade_edge, UINT32_C(0x7adee9e));
    uint64_t trade_edge_hash = CcSimHash(&trade_edge);
    CcCommand impossible_sale = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = INT32_MIN
    };
    CC_CHECK(!CcSimApply(&trade_edge, &impossible_sale,
                         error, sizeof(error)));
    CC_CHECK(CcSimHash(&trade_edge) == trade_edge_hash);
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.shipment_count = 1;
    invalid_state.shipments[0] = (CcShipment){
        .id = CcMakeId(CC_ENTITY_SHIPMENT, UINT64_C(9999)),
        .origin_id = invalid_state.settlements[0].id,
        .destination_id = invalid_state.settlements[2].id,
        .final_destination_id = invalid_state.settlements[2].id,
        .route_id = invalid_state.routes[0].id,
        .good = CC_GOOD_FOOD,
        .quantity = 1,
        .departure_day = invalid_state.current_day,
        .arrival_day = invalid_state.current_day +
                       invalid_state.routes[0].travel_days,
        .status = CC_SHIPMENT_TRAVELLING
    };
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    invalid_state.shipments[0].destination_id =
        invalid_state.settlements[1].id;
    invalid_state.shipments[0].final_destination_id =
        invalid_state.settlements[1].id;
    invalid_state.shipments[0].quantity =
        invalid_state.routes[0].capacity * 8 + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    invalid_state.shipments[0].quantity = 1;
    invalid_state.shipments[0].arrival_day += 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));

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
