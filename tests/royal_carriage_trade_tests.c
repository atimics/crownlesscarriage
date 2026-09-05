#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"

#include <stdint.h>
#include <sqlite3.h>
#include <stdio.h>

static void SetRelation(CcSim *sim, int32_t first, int32_t second,
                        CcDiplomaticState state)
{
    sim->diplomacy[first][second] = state;
    sim->diplomacy[second][first] = state;
    sim->diplomacy_changed_day[first][second] = sim->current_day;
    sim->diplomacy_changed_day[second][first] = sim->current_day;
}

static void SetFactionSupport(CcSim *sim, int32_t kingdom_slot,
                              CcFactionKind kind, int32_t support)
{
    CcId kingdom_id = sim->kingdoms[kingdom_slot].id;
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        if (sim->factions[i].kingdom_id == kingdom_id &&
            sim->factions[i].kind == kind) {
            sim->factions[i].support = support;
            return;
        }
    }
}

static void ClearTradeNeeds(CcSim *sim)
{
    for (int32_t place = 0; place < sim->settlement_count; ++place) {
        sim->settlements[place].market_coins = 1000;
        sim->settlements[place].war_chest = 1000;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            sim->settlements[place].stock[good] = 0;
            sim->settlements[place].reserve_target[good] = 0;
            sim->settlements[place].production[good] = 0;
            sim->settlements[place].consumption[good] = 0;
            sim->settlements[place].price[good] = 1;
        }
    }
}

static void MakeLegacyRoyalDatabase(const char *path)
{
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open_v2(path, &database, SQLITE_OPEN_READWRITE,
                             NULL) == SQLITE_OK);
    char *sqlite_error = NULL;
    CC_CHECK(sqlite3_exec(
                 database,
                 "DROP TABLE royal_carriage;"
                 "DROP TABLE royal_route_usage;"
                 "PRAGMA user_version=25;",
                 NULL, NULL, &sqlite_error) == SQLITE_OK);
    sqlite3_free(sqlite_error);
    sqlite3_close(database);
}

static int32_t CountRoyalBlockedEvents(const CcSim *sim, CcId carriage_id)
{
    int32_t count = 0;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event != NULL &&
            event->kind == CC_EVENT_ROYAL_CARRIAGE_BLOCKED &&
            event->subject_id == carriage_id) {
            count += 1;
        }
    }
    return count;
}

static void ConfigureCapacityWaitScenario(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0xc4111a9e));
    ClearTradeNeeds(sim);
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = first + 1;
             second < sim->kingdom_count; ++second) {
            SetRelation(sim, first, second, CC_DIPLOMACY_ALLIANCE);
        }
    }
    for (int32_t route = 0; route < sim->route_count; ++route) {
        sim->routes[route].condition = 100;
        sim->routes[route].security = 100;
        sim->routes[route].closed = false;
    }
    sim->routes[1].smuggler_route = true;
    sim->routes[5].smuggler_route = true;
    sim->routes[7].smuggler_route = true;
    sim->routes[2].travel_days = 1;
    sim->routes[2].capacity = 3;
    sim->routes[3].travel_days = 1;
    sim->routes[3].capacity = 3;
    for (int32_t monster = 0; monster < sim->monster_count; ++monster) {
        sim->monsters[monster].pressure = 0;
    }
    for (int32_t bandit = 0; bandit < sim->bandit_count; ++bandit) {
        sim->bandits[bandit].influence = 0;
    }

    CcId shipment_id = CcMakeId(
        CC_ENTITY_SHIPMENT, sim->next_entity_serial++);
    sim->shipment_count = 1;
    sim->shipments[0] = (CcShipment){
        .id = shipment_id,
        .origin_id = sim->settlements[2].id,
        .destination_id = sim->settlements[3].id,
        .final_destination_id = sim->settlements[4].id,
        .route_id = sim->routes[2].id,
        .good = CC_GOOD_BREAD,
        .quantity = 24,
        .departure_day = sim->current_day,
        .arrival_day = sim->current_day + 1,
        .status = CC_SHIPMENT_TRAVELLING
    };
    CcRoyalCarriage *carriage = &sim->royal_carriages[2];
    carriage->location_id = sim->settlements[2].id;
    carriage->route_id = sim->routes[2].id;
    carriage->destination_id = sim->settlements[3].id;
    carriage->target_id = sim->settlements[4].id;
    carriage->active_shipment_id = shipment_id;
    carriage->mode = CC_ROYAL_CARRIAGE_DELIVERING;
    carriage->departure_day = sim->current_day;
    carriage->arrival_day = sim->current_day + 1;
    carriage->blocked_since_day = 0;
    carriage->next_dispatch_day = sim->current_day + 7;
    carriage->condition = 100;
    sim->royal_route_slots_used[2] = 3;
    sim->royal_route_slots_used[3] = 3;
}

static void CheckChangedDestination(void)
{
    static CcSim sim;
    ConfigureCapacityWaitScenario(&sim);
    sim.settlements[4].kingdom_id = sim.kingdoms[0].id;
    CcSimUpgradeHistoryOffices(&sim);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.shipments[0].status == CC_SHIPMENT_ARRIVED);
    CC_CHECK(sim.royal_carriages[2].mode == CC_ROYAL_CARRIAGE_IDLE);
    CC_CHECK(sim.royal_carriages[2].active_shipment_id == 0U);
    const CcSettlement *stopped = CcSimSettlement(&sim, sim.royal_carriages[2].location_id);
    CC_CHECK(stopped != NULL && stopped->stock[CC_GOOD_BREAD] >= 24);
    char error[256];
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

int main(void)
{
    CheckChangedDestination();
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc4111a9e));
    CC_CHECK(sim.royal_carriage_count == sim.kingdom_count);
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        const CcRoyalCarriage *carriage = CcSimRoyalCarriage(
            &sim, sim.kingdoms[i].id);
        CC_CHECK(carriage != NULL);
        CC_CHECK(CcIdKind(carriage->id) == CC_ENTITY_ROYAL_CARRIAGE);
        CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_IDLE);
        CC_CHECK(carriage->active_shipment_id == 0U);
        CC_CHECK(carriage->id != sim.player.id);
        for (int32_t earlier = 0; earlier < i; ++earlier) {
            CC_CHECK(carriage->id != sim.royal_carriages[earlier].id);
        }
    }

    CcRoute *peace_road = &sim.routes[7];
    CcId owner_id = sim.kingdoms[2].id;
    SetRelation(&sim, 1, 2, CC_DIPLOMACY_PEACE);
    SetFactionSupport(&sim, 1, CC_FACTION_CROWN, 80);
    SetFactionSupport(&sim, 1, CC_FACTION_GUILD, 80);
    CC_CHECK(CcSimRoyalCarriageCanUseRoute(
        &sim, owner_id, peace_road->id));
    SetFactionSupport(&sim, 1, CC_FACTION_CROWN,
                      CC_ROYAL_TRADE_SUPPORT_FLOOR - 1);
    CC_CHECK(!CcSimRoyalCarriageCanUseRoute(
        &sim, owner_id, peace_road->id));
    SetRelation(&sim, 1, 2, CC_DIPLOMACY_ALLIANCE);
    CC_CHECK(CcSimRoyalCarriageCanUseRoute(
        &sim, owner_id, peace_road->id));
    SetRelation(&sim, 0, 1, CC_DIPLOMACY_ALLIANCE);
    SetRelation(&sim, 0, 2, CC_DIPLOMACY_ALLIANCE);
    SetRelation(&sim, 1, 2, CC_DIPLOMACY_WAR);
    CC_CHECK(!CcSimRoyalCarriageCanUseRoute(
        &sim, sim.kingdoms[0].id, peace_road->id));
    SetRelation(&sim, 0, 1, CC_DIPLOMACY_PEACE);
    CC_CHECK(!CcSimRoyalCarriageCanUseRoute(
        &sim, sim.kingdoms[0].id, sim.routes[6].id));

    CcSim permit;
    CcSimInit(&permit, UINT32_C(0xc4111a9e));
    ClearTradeNeeds(&permit);
    SetRelation(&permit, 1, 2, CC_DIPLOMACY_PEACE);
    SetFactionSupport(&permit, 1, CC_FACTION_CROWN,
                      CC_ROYAL_TRADE_SUPPORT_FLOOR - 1);
    SetFactionSupport(&permit, 1, CC_FACTION_GUILD,
                      CC_ROYAL_TRADE_SUPPORT_FLOOR - 1);
    permit.settlements[2].stock[CC_GOOD_IRON] = 60;
    permit.settlements[4].reserve_target[CC_GOOD_IRON] = 20;
    permit.routes[7].travel_days = 1;
    permit.routes[7].condition = 100;
    permit.routes[7].security = 100;
    permit.routes[7].closed = false;
    permit.routes[7].smuggler_route = false;
    CcSimAdvanceDays(&permit, 6);
    const CcRoyalCarriage *permit_carriage = CcSimRoyalCarriage(
        &permit, permit.kingdoms[2].id);
    CC_CHECK(permit_carriage != NULL);
    CC_CHECK(permit_carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED);
    CC_CHECK(permit_carriage->active_shipment_id == 0U);
    CcSimAdvanceDays(&permit, 7);
    permit_carriage = CcSimRoyalCarriage(
        &permit, permit.kingdoms[2].id);
    CC_CHECK(permit_carriage->mode != CC_ROYAL_CARRIAGE_BLOCKED);
    CC_CHECK(CcSimRoyalCarriageCanUseRoute(
        &permit, permit.kingdoms[2].id, permit.routes[7].id));

    SetRelation(&sim, 1, 2, CC_DIPLOMACY_PEACE);
    SetFactionSupport(&sim, 1, CC_FACTION_CROWN, 80);
    ClearTradeNeeds(&sim);
    CcSettlement *source = &sim.settlements[2];
    CcSettlement *buyer = &sim.settlements[4];
    source->stock[CC_GOOD_IRON] = 60;
    buyer->reserve_target[CC_GOOD_IRON] = 20;
    peace_road->travel_days = 1;
    peace_road->condition = 100;
    peace_road->security = 100;
    peace_road->closed = false;
    peace_road->smuggler_route = false;
    int32_t tracked_iron = CcSimTrackedGood(&sim, CC_GOOD_IRON);
    CcId player_location = sim.player.location_id;
    CcCarriageMode player_mode = sim.carriage.mode;

    CcSimAdvanceDays(&sim, 6);
    const CcRoyalCarriage *carriage = CcSimRoyalCarriage(&sim, owner_id);
    CC_CHECK(carriage != NULL);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_REPOSITIONING);
    CC_CHECK(carriage->target_id == source->id);
    CC_CHECK(carriage->active_shipment_id == 0U);
    CcSimAdvanceDays(&sim, 1);
    carriage = CcSimRoyalCarriage(&sim, owner_id);
    CC_CHECK(carriage->location_id == source->id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING);
    CC_CHECK(carriage->active_shipment_id != 0U);
    CcId shipment_id = carriage->active_shipment_id;
    const CcShipment *shipment = NULL;
    for (int32_t i = 0; i < sim.shipment_count; ++i) {
        if (sim.shipments[i].id == shipment_id) shipment = &sim.shipments[i];
    }
    CC_CHECK(shipment != NULL);
    CC_CHECK(shipment->good == CC_GOOD_IRON);
    CC_CHECK(shipment->quantity <=
             CC_ROYAL_CARRIAGE_CARGO_SLOTS *
                 CcGoodDefinitionFor(CC_GOOD_IRON)->freight_units_per_slot);
    int32_t cargo_quantity = shipment->quantity;
    int32_t shipment_route_slot = -1;
    for (int32_t route = 0; route < sim.route_count; ++route) {
        if (sim.routes[route].id == shipment->route_id) {
            shipment_route_slot = route;
        }
    }
    CC_CHECK(shipment_route_slot >= 0);
    CC_CHECK(sim.royal_route_slots_used[shipment_route_slot] ==
             (cargo_quantity +
              CcGoodDefinitionFor(CC_GOOD_IRON)->freight_units_per_slot - 1) /
                 CcGoodDefinitionFor(CC_GOOD_IRON)->freight_units_per_slot);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_IRON) == tracked_iron);
    CC_CHECK(sim.player.location_id == player_location);
    CC_CHECK(sim.carriage.mode == player_mode);

    CcSim delivering = sim;
    CcSim forged = delivering;
    for (int32_t i = 0; i < forged.shipment_count; ++i) {
        if (forged.shipments[i].id == shipment_id) {
            forged.shipments[i].quantity =
                (CC_ROYAL_CARRIAGE_CARGO_SLOTS + 1) *
                CcGoodDefinitionFor(CC_GOOD_IRON)->freight_units_per_slot;
        }
    }
    char error[192];
    CC_CHECK(!CcSimValidate(&forged, error, sizeof(error)));

    forged = delivering;
    forged.royal_route_slots_used[7] = -1;
    CC_CHECK(!CcSimValidate(&forged, error, sizeof(error)));

    forged = delivering;
    CcRoyalCarriage *forged_carriage =
        &forged.royal_carriages[2];
    forged_carriage->route_id = 0U;
    forged_carriage->destination_id = 0U;
    forged_carriage->target_id = 0U;
    forged_carriage->active_shipment_id = 0U;
    forged_carriage->mode = CC_ROYAL_CARRIAGE_IDLE;
    forged_carriage->arrival_day = 0;
    CC_CHECK(!CcSimValidate(&forged, error, sizeof(error)));

    SetRelation(&sim, 1, 2, CC_DIPLOMACY_WAR);
    CcSimAdvanceDays(&sim, 1);
    carriage = CcSimRoyalCarriage(&sim, owner_id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED);
    CC_CHECK(carriage->active_shipment_id == shipment_id);
    CC_CHECK(shipment->status == CC_SHIPMENT_BLOCKED);
    CC_CHECK(shipment->quantity == cargo_quantity);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_IRON) == tracked_iron);

    forged = sim;
    forged.royal_carriages[2].route_id = forged.routes[0].id;
    forged.royal_carriages[2].destination_id =
        forged.routes[0].to_id;
    CC_CHECK(!CcSimValidate(&forged, error, sizeof(error)));

    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    const char *path = "/tmp/crownless-royal-carriage-trade.ccsave";
    (void)remove(path);
    uint64_t blocked_hash = CcSimHash(&sim);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == blocked_hash);
    carriage = CcSimRoyalCarriage(&restored, owner_id);
    CC_CHECK(carriage != NULL);
    CC_CHECK(carriage->id == sim.royal_carriages[2].id);
    CC_CHECK(carriage->active_shipment_id == shipment_id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED);
    CC_CHECK(restored.royal_route_slots_used[shipment_route_slot] ==
             sim.royal_route_slots_used[shipment_route_slot]);

    CcSim expired = restored;
    CcRoyalCarriage *expired_carriage =
        &expired.royal_carriages[2];
    int32_t release_stock = expired.settlements[2].stock[CC_GOOD_IRON];
    for (int32_t day = 0; day < 28; ++day) {
        SetRelation(&expired, 1, 2, CC_DIPLOMACY_WAR);
        CcSimAdvanceDays(&expired, 1);
    }
    expired_carriage = &expired.royal_carriages[2];
    CC_CHECK(expired_carriage->mode == CC_ROYAL_CARRIAGE_IDLE);
    CC_CHECK(expired_carriage->active_shipment_id == 0U);
    CC_CHECK(expired.settlements[2].stock[CC_GOOD_IRON] ==
             release_stock + cargo_quantity);
    CC_CHECK(CcSimValidate(&expired, error, sizeof(error)));

    CcSim continued = sim;
    SetFactionSupport(&restored, 1, CC_FACTION_CROWN, 0);
    SetFactionSupport(&restored, 1, CC_FACTION_GUILD, 0);
    SetRelation(&restored, 1, 2, CC_DIPLOMACY_ALLIANCE);
    SetFactionSupport(&continued, 1, CC_FACTION_CROWN, 0);
    SetFactionSupport(&continued, 1, CC_FACTION_GUILD, 0);
    SetRelation(&continued, 1, 2, CC_DIPLOMACY_ALLIANCE);
    CcSimAdvanceDays(&restored, 1);
    CcSimAdvanceDays(&continued, 1);
    CC_CHECK(CcSimHash(&continued) == CcSimHash(&restored));
    carriage = CcSimRoyalCarriage(&restored, owner_id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING);
    CC_CHECK(carriage->active_shipment_id == shipment_id);
    CcSimAdvanceDays(&restored, 1);
    CcSimAdvanceDays(&continued, 1);
    CC_CHECK(CcSimHash(&continued) == CcSimHash(&restored));
    carriage = CcSimRoyalCarriage(&restored, owner_id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_IDLE);
    CC_CHECK(carriage->location_id == buyer->id);
    CC_CHECK(carriage->trips_completed == 1);
    CC_CHECK(carriage->next_dispatch_day > restored.current_day);
    CC_CHECK(restored.settlements[4].stock[CC_GOOD_IRON] == cargo_quantity);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);

    CcSim capacity_wait;
    ConfigureCapacityWaitScenario(&capacity_wait);
    CcId capacity_carriage_id = capacity_wait.royal_carriages[2].id;
    CcId capacity_shipment_id = capacity_wait.shipments[0].id;
    CC_CHECK(CcSimValidate(&capacity_wait, error, sizeof(error)));
    CcSimAdvanceDays(&capacity_wait, 1);
    const CcRoyalCarriage *capacity_carriage = CcSimRoyalCarriage(
        &capacity_wait, capacity_wait.kingdoms[2].id);
    CC_CHECK(capacity_carriage != NULL);
    CC_CHECK(capacity_carriage->mode ==
             CC_ROYAL_CARRIAGE_WAITING_CAPACITY);
    CC_CHECK(capacity_carriage->blocked_since_day == 0);
    CC_CHECK(capacity_wait.shipments[0].status == CC_SHIPMENT_BLOCKED);
    CC_CHECK(CountRoyalBlockedEvents(
                 &capacity_wait, capacity_carriage_id) == 0);
    CC_CHECK(CcSimValidate(&capacity_wait, error, sizeof(error)));

    const char *capacity_path =
        "/tmp/crownless-royal-capacity-wait.ccsave";
    (void)remove(capacity_path);
    CC_CHECK(CcSaveWrite(
        capacity_path, &capacity_wait, error, sizeof(error)));
    CcSim restored_capacity_wait;
    CC_CHECK(CcSaveRead(
        capacity_path, &restored_capacity_wait, error, sizeof(error)));
    capacity_carriage = CcSimRoyalCarriage(
        &restored_capacity_wait, restored_capacity_wait.kingdoms[2].id);
    CC_CHECK(capacity_carriage != NULL);
    CC_CHECK(capacity_carriage->mode ==
             CC_ROYAL_CARRIAGE_WAITING_CAPACITY);
    CC_CHECK(capacity_carriage->blocked_since_day == 0);
    CC_CHECK(CcSimHash(&restored_capacity_wait) ==
             CcSimHash(&capacity_wait));
    CC_CHECK(remove(capacity_path) == 0);

    CcSim held_capacity = restored_capacity_wait;
    int32_t blocked_events = CountRoyalBlockedEvents(
        &held_capacity, capacity_carriage_id);
    for (int32_t day = 0; day < 35; ++day) {
        held_capacity.royal_trade_week =
            (held_capacity.current_day + 1) / 7;
        held_capacity.royal_route_slots_used[3] = 3;
        SetRelation(&held_capacity, 1, 2, CC_DIPLOMACY_ALLIANCE);
        CcSimAdvanceDays(&held_capacity, 1);
        capacity_carriage = CcSimRoyalCarriage(
            &held_capacity, held_capacity.kingdoms[2].id);
        CC_CHECK(capacity_carriage != NULL);
        CC_CHECK(capacity_carriage->mode ==
                 CC_ROYAL_CARRIAGE_WAITING_CAPACITY);
        CC_CHECK(capacity_carriage->active_shipment_id ==
                 capacity_shipment_id);
        CC_CHECK(capacity_carriage->blocked_since_day == 0);
        CC_CHECK(held_capacity.shipments[0].status ==
                 CC_SHIPMENT_BLOCKED);
    }
    CC_CHECK(CountRoyalBlockedEvents(
                 &held_capacity, capacity_carriage_id) == blocked_events);
    CC_CHECK(CcSimValidate(&held_capacity, error, sizeof(error)));

    CcSim priority = restored_capacity_wait;
    priority.current_day = 6;
    priority.royal_trade_week = 0;
    priority.royal_route_slots_used[3] = 3;
    priority.settlements[4].stock[CC_GOOD_IRON] = 16;
    priority.settlements[3].reserve_target[CC_GOOD_IRON] = 16;
    CcRoyalCarriage *fresh = &priority.royal_carriages[1];
    fresh->location_id = priority.settlements[3].id;
    fresh->route_id = priority.routes[3].id;
    fresh->destination_id = priority.settlements[4].id;
    fresh->target_id = priority.settlements[4].id;
    fresh->active_shipment_id = 0U;
    fresh->mode = CC_ROYAL_CARRIAGE_REPOSITIONING;
    fresh->departure_day = 6;
    fresh->arrival_day = 7;
    fresh->blocked_since_day = 0;
    fresh->next_dispatch_day = 0;
    fresh->condition = 100;
    CcSimAdvanceDays(&priority, 1);
    capacity_carriage = CcSimRoyalCarriage(
        &priority, priority.kingdoms[2].id);
    fresh = &priority.royal_carriages[1];
    CC_CHECK(capacity_carriage != NULL);
    CC_CHECK(capacity_carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING);
    CC_CHECK(priority.shipments[0].status == CC_SHIPMENT_TRAVELLING);
    CC_CHECK(priority.shipments[0].route_id == priority.routes[3].id);
    CC_CHECK(priority.royal_route_slots_used[3] == 3);
    CC_CHECK(fresh->mode == CC_ROYAL_CARRIAGE_IDLE);
    CC_CHECK(fresh->active_shipment_id == 0U);
    CC_CHECK(priority.settlements[4].stock[CC_GOOD_IRON] == 16);
    CC_CHECK(CcSimValidate(&priority, error, sizeof(error)));

    for (uint32_t schema = 35U; schema <= 37U; ++schema) {
        CcSim legacy;
        CcSimInit(&legacy, UINT32_C(0x35ca771a));
        legacy.schema_version = schema;
        legacy.generator_version = 25U;
        legacy.royal_carriage_count = 0;
        for (int32_t i = 0; i < CC_MAX_KINGDOMS; ++i) {
            legacy.royal_carriages[i] = (CcRoyalCarriage){0};
        }
        const char *legacy_path =
            "royal-carriage-legacy-migration.ccsave";
        (void)remove(legacy_path);
        CC_CHECK(CcSaveWrite(legacy_path, &legacy, error, sizeof(error)));
        MakeLegacyRoyalDatabase(legacy_path);
        CcSim upgraded;
        CC_CHECK(CcSaveRead(legacy_path, &upgraded, error, sizeof(error)));
        CC_CHECK(upgraded.schema_version == CC_SIM_SCHEMA_VERSION);
        CC_CHECK(upgraded.generator_version == CC_GENERATOR_VERSION);
        CC_CHECK(upgraded.royal_carriage_count == upgraded.kingdom_count);
        CC_CHECK(upgraded.next_entity_serial == legacy.next_entity_serial +
                 (uint64_t)upgraded.kingdom_count);
        CC_CHECK(CcSimValidate(&upgraded, error, sizeof(error)));
        CC_CHECK(CcSaveWrite(legacy_path, &upgraded, error, sizeof(error)));
        CcSim again;
        CC_CHECK(CcSaveRead(legacy_path, &again, error, sizeof(error)));
        CC_CHECK(CcSimHash(&again) == CcSimHash(&upgraded));
        CC_CHECK(remove(legacy_path) == 0);
    }

    puts("Royal carriage trade tests passed");
    return 0;
}
