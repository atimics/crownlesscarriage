#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"

#include <stdint.h>
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

int main(void)
{
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
    SetRelation(&sim, 0, 1, CC_DIPLOMACY_PEACE);
    CC_CHECK(!CcSimRoyalCarriageCanUseRoute(
        &sim, sim.kingdoms[0].id, sim.routes[6].id));

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
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_IRON) == tracked_iron);
    CC_CHECK(sim.player.location_id == player_location);
    CC_CHECK(sim.carriage.mode == player_mode);

    SetRelation(&sim, 1, 2, CC_DIPLOMACY_WAR);
    CcSimAdvanceDays(&sim, 1);
    carriage = CcSimRoyalCarriage(&sim, owner_id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED);
    CC_CHECK(carriage->active_shipment_id == shipment_id);
    CC_CHECK(shipment->status == CC_SHIPMENT_BLOCKED);
    CC_CHECK(shipment->quantity == cargo_quantity);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_IRON) == tracked_iron);

    char error[192];
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

    SetFactionSupport(&restored, 1, CC_FACTION_CROWN, 0);
    SetFactionSupport(&restored, 1, CC_FACTION_GUILD, 0);
    SetRelation(&restored, 1, 2, CC_DIPLOMACY_ALLIANCE);
    CcSimAdvanceDays(&restored, 1);
    carriage = CcSimRoyalCarriage(&restored, owner_id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING);
    CC_CHECK(carriage->active_shipment_id == shipment_id);
    CcSimAdvanceDays(&restored, 1);
    carriage = CcSimRoyalCarriage(&restored, owner_id);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_IDLE);
    CC_CHECK(carriage->location_id == buyer->id);
    CC_CHECK(carriage->trips_completed == 1);
    CC_CHECK(restored.settlements[4].stock[CC_GOOD_IRON] == cargo_quantity);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);

    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x35ca771a));
    legacy.schema_version = 35U;
    legacy.generator_version = 25U;
    legacy.royal_carriage_count = 0;
    for (int32_t i = 0; i < CC_MAX_KINGDOMS; ++i) {
        legacy.royal_carriages[i] = (CcRoyalCarriage){0};
    }
    const char *legacy_path =
        "/tmp/crownless-schema-35-royal-carriage.ccsave";
    (void)remove(legacy_path);
    CC_CHECK(CcSaveWrite(legacy_path, &legacy, error, sizeof(error)));
    CcSim upgraded;
    CC_CHECK(CcSaveRead(legacy_path, &upgraded, error, sizeof(error)));
    CC_CHECK(upgraded.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(upgraded.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(upgraded.royal_carriage_count == upgraded.kingdom_count);
    CC_CHECK(CcSimValidate(&upgraded, error, sizeof(error)));
    CC_CHECK(remove(legacy_path) == 0);

    puts("Royal carriage trade tests passed");
    return 0;
}
