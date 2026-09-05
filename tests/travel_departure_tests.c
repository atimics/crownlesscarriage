#include "persistence/cc_save.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static CcSim sim;
static CcSim restored;
static char error[256];

static void CheckFreeRoads(void)
{
    CcSimInit(&sim, 42U);
    int32_t routes = sim.route_count;
    for (int32_t i = 0; i < routes; ++i) {
        for (int reverse = 0; reverse < 2; ++reverse) {
            CcSimInit(&sim, 42U);
            const CcRoute *route = &sim.routes[i];
            sim.player.location_id = reverse ? route->to_id : route->from_id;
            sim.carriage.location_id = sim.player.location_id;
            sim.player.coins = 0;
            CcSettlement *origin = CcSimSettlementMutable(&sim, sim.player.location_id);
            origin->stock[CC_GOOD_WHEAT] = 10000;
            CcId destination = reverse ? route->from_id : route->to_id;
            CcTravelPreview preview = {0};
            CC_CHECK(CcSimTravelPreview(&sim, destination, &preview, error, sizeof(error)));
            CC_CHECK(preview.provision_cost == 0);
            CcMoney gold = CcSimTrackedGold(&sim);
            CcMoney market = origin->market_coins;
            CcCommand depart = {.kind = CC_COMMAND_TRAVEL, .target_id = destination};
            CC_CHECK(CcSimApply(&sim, &depart, error, sizeof(error)));
            CC_CHECK(sim.journey.active && sim.journey.fare_reserved == 0);
            CC_CHECK(sim.player.coins == 0 && origin->market_coins == market);
            CC_CHECK(CcSimTrackedGold(&sim) == gold);
            CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
            CcSimAdvanceRuntimeTicks(&sim, 12);
            CC_CHECK(sim.carriage.progress_milli > 0);
            CC_CHECK(sim.player.coins == 0);
        }
    }
}

static void CheckPaymentsAtEncounter(bool carriage_away)
{
    CcSimInit(&sim, 42U);
    CcSituation *offer = NULL;
    for (int32_t i = 0; i < sim.situation_count; ++i) {
        if (sim.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY &&
            sim.situations[i].status == CC_SITUATION_ACTIVE) {
            offer = &sim.situations[i];
            break;
        }
    }
    CC_CHECK(offer != NULL);
    offer->target_id = sim.settlements[1].id;
    CcCommand accept = {.kind = CC_COMMAND_ACCEPT_SITUATION, .target_id = offer->id};
    CC_CHECK(CcSimApply(&sim, &accept, error, sizeof(error)));
    CcRoute *route = NULL;
    for (int32_t i = 0; i < sim.route_count; ++i) {
        if ((sim.routes[i].from_id == sim.player.location_id && sim.routes[i].to_id == offer->target_id) ||
            (sim.routes[i].to_id == sim.player.location_id && sim.routes[i].from_id == offer->target_id)) {
            route = &sim.routes[i];
            break;
        }
    }
    CC_CHECK(route != NULL);
    route->closed = true;
    sim.player.coins = 0;
    CcCommand depart = {.kind = CC_COMMAND_TRAVEL, .target_id = offer->target_id};
    CC_CHECK(CcSimApply(&sim, &depart, error, sizeof(error)));
    for (int32_t step = 0; step < 2000 && sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING; ++step)
        CcSimAdvanceRuntimeTicks(&sim, 1);
    CC_CHECK(sim.journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    CC_CHECK(sim.carriage.progress_milli >= 350);
    CC_CHECK(sim.player.coins == 0);
    CcCommand pay = {.kind = CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE};
    uint64_t blocked = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &pay, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == blocked);
    if (carriage_away) {
        for (int32_t i = 0; i < sim.royal_carriage_count; ++i) {
            CcRoyalCarriage *carriage = &sim.royal_carriages[i];
            if (carriage->mode == CC_ROYAL_CARRIAGE_IDLE &&
                carriage->location_id == sim.journey.origin_id)
                carriage->location_id = sim.journey.destination_id;
        }
    }
    int32_t shipments = sim.shipment_count;
    sim.player.coins = sim.journey.bargain_cost;
    CcMoney gold = CcSimTrackedGold(&sim);
    CC_CHECK(CcSimApply(&sim, &pay, error, sizeof(error)));
    CC_CHECK(sim.player.coins == 0);
    CC_CHECK(sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    if (carriage_away) CC_CHECK(sim.shipment_count == shipments);
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        fprintf(stderr, "Encounter fixture: %s\n", error);
        CC_CHECK(false);
    }
}

static void CheckJourneySaves(void)
{
    const char *path = "free-departure.ccsave";
    for (uint32_t version = 40U; version <= CC_SIM_SCHEMA_VERSION; ++version) {
        CcSimInit(&sim, 42U);
        sim.schema_version = version;
        sim.player.coins = 100;
        CcCommand depart = {.kind = CC_COMMAND_TRAVEL, .target_id = sim.settlements[1].id};
        CC_CHECK(CcSimApply(&sim, &depart, error, sizeof(error)));
        CC_CHECK(version == 40U ? sim.journey.fare_reserved > 0 : sim.journey.fare_reserved == 0);
        CcSimAdvanceRuntimeTicks(&sim, 12);
        CcMoney purse = sim.player.coins;
        int32_t paid = sim.journey.fare_reserved;
        int32_t progress = sim.carriage.progress_milli;
        CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
        CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
        CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
        CC_CHECK(restored.player.coins == purse);
        CC_CHECK(restored.journey.fare_reserved == paid);
        CC_CHECK(restored.carriage.progress_milli == progress);
        sim.schema_version = CC_SIM_SCHEMA_VERSION;
        CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
        (void)remove(path);
    }
    CcSimInit(&sim, 42U);
    sim.player.coins = 0;
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand depart = {.kind = CC_COMMAND_TRAVEL, .target_id = sim.settlements[1].id};
    CC_CHECK(CcJournalApply(journal, &sim, &depart, error, sizeof(error)));
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(restored.player.coins == 0 && restored.journey.fare_reserved == 0);
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);
}

int main(void)
{
    CheckFreeRoads();
    CheckPaymentsAtEncounter(false);
    CheckPaymentsAtEncounter(true);
    CheckJourneySaves();
    puts("Free departures: every road, both directions, encounter payments and save replay passed");
    return 0;
}
