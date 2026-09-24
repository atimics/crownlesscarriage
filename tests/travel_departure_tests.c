#include "persistence/cc_save.h"
#include "sim/cc_mine.h"
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
    CC_CHECK(CcTestLoadReliefCrates(&sim, offer, error, sizeof(error)));
    CcRoute *route = NULL;
    for (int32_t i = 0; i < sim.route_count; ++i) {
        if ((sim.routes[i].from_id == sim.player.location_id && sim.routes[i].to_id == offer->target_id) ||
            (sim.routes[i].to_id == sim.player.location_id && sim.routes[i].from_id == offer->target_id)) {
            route = &sim.routes[i];
            break;
        }
    }
    CC_CHECK(route != NULL);
    sim.bandits[0].route_id = route->id;
    route->closed = true;
    sim.player.coins = 0;
    CcCommand depart = {.kind = CC_COMMAND_TRAVEL, .target_id = offer->target_id};
    CC_CHECK(CcSimApply(&sim, &depart, error, sizeof(error)));
    for (int32_t step = 0;
         step < 4000 && sim.journey.phase != CC_JOURNEY_PHASE_BLOCKED;
         ++step) {
        if (sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING)
            CcSimAdvanceRuntimeTicks(&sim, 1);
        else
            CC_CHECK(CcTestContinueJourneyPause(
                &sim, error, sizeof(error)));
    }
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
    int32_t members = sim.bandits[0].members;
    sim.bandits[0].members = 0;
    uint64_t vanished = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &pay, error, sizeof(error)));
    CC_CHECK(strstr(error, "nobody to pay") != NULL);
    CC_CHECK(CcSimHash(&sim) == vanished);
    sim.bandits[0].members = members;
    CcMoney gold = CcSimTrackedGold(&sim);
    CcMoney bandit_coins = sim.bandits[0].coins;
    CcMoney destination_coins = CcSimSettlement(
        &sim, sim.journey.destination_id)->market_coins;
    int32_t passage_cost = sim.journey.bargain_cost;
    CC_CHECK(CcSimApply(&sim, &pay, error, sizeof(error)));
    CC_CHECK(sim.player.coins == 0);
    CC_CHECK(sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(sim.bandits[0].coins == bandit_coins + passage_cost);
    CC_CHECK(CcSimSettlement(&sim, sim.journey.destination_id)->market_coins ==
             destination_coins);
    CC_CHECK(route->closed);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    if (carriage_away) CC_CHECK(sim.shipment_count == shipments);
    const char *path = "paid-road-encounter.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    CC_CHECK(restored.bandits[0].coins == sim.bandits[0].coins);
    uint64_t paid_hash = CcSimHash(&restored);
    CC_CHECK(!CcSimApply(&restored, &pay, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == paid_hash);
    (void)remove(path);
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
        if (version < 103U) {
            sim.mine = (CcMineVisit){0};
            CcCustodyInit(&sim.custody);
            sim.custody.capacity = CC_CUSTODY_LEGACY_CAPACITY;
        }
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
        if (version < 103U) {
            sim.custody.capacity = CC_CUSTODY_CAPACITY;
            CcMineInitializeLoad(&sim);
        }
        if (version < 105U) CC_CHECK(CcRoadMigrateLegacyJourney(&sim));
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

static void CheckAbandonedTownExit(void)
{
    const char *path = "abandoned-company-exit.ccsave";
    (void)remove(path);
    (void)remove("abandoned-company-exit.ccsave-wal");
    (void)remove("abandoned-company-exit.ccsave-shm");
    CcSimInit(&sim, 42U);
    CcRoute *route = &sim.routes[0];
    CC_CHECK(route->from_id == sim.player.location_id);
    CcSettlement *origin = CcSimSettlementMutable(&sim, route->from_id);
    CcSettlement *destination = CcSimSettlementMutable(&sim, route->to_id);
    CC_CHECK(origin != NULL && destination != NULL);
    origin->population = 0;
    origin->hunger = 100;
    origin->service_mask = 0U;
    origin->service_project = CC_SERVICE_NONE;
    origin->service_project_days = 0;
    origin->security = 0;
    origin->prosperity = 0;
    memset(origin->stock, 0, sizeof(origin->stock));
    CC_CHECK(CcSettlementIsAbandoned(origin));
    destination->stock[CC_GOOD_BREAD] = 80;
    route->closed = true;
    route->condition = 0;
    for (int32_t i = 0; i < sim.bandit_count; ++i)
        sim.bandits[i].route_id = sim.routes[1].id;
    memset(sim.player.cargo, 0, sizeof(sim.player.cargo));
    sim.player.cargo[CC_GOOD_BREAD] = 10;
    sim.player.cargo[CC_GOOD_WOOD] = 2;
    sim.player.coins = 0;
    sim.player.feed_tray_wheat = 0;
    sim.carriage.condition = 0;
    for (int32_t i = 0; i < CcSimHorseTeamCount(&sim); ++i) {
        sim.horse_team[i].health = 1;
        sim.horse_team[i].hunger = 100;
        sim.horse_team[i].fatigue = 0;
    }
    CC_CHECK(CcSimHorseTeamReadiness(&sim) == 0);
    CcTravelPreview preview = {0};
    CC_CHECK(CcSimTravelPreview(&sim, destination->id, &preview,
                                error, sizeof(error)));
    CC_CHECK(preview.provision_cost == 0 && preview.travel_watches > 0);
    CcId disconnected = 0U;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        CcId candidate = sim.settlements[i].id;
        if (candidate != origin->id &&
            CcSimRouteBetween(&sim, origin->id, candidate) == NULL) {
            disconnected = candidate;
            break;
        }
    }
    CC_CHECK(disconnected != 0U);
    uint64_t before = CcSimHash(&sim);
    CC_CHECK(!CcSimTravelPreview(&sim, disconnected, &preview,
                                 error, sizeof(error)));
    CC_CHECK(strstr(error, "No direct carriage route") != NULL);
    CC_CHECK(CcSimHash(&sim) == before);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = destination->id};
    CC_CHECK(CcJournalApply(journal, &sim, &travel, error, sizeof(error)));
    CC_CHECK(sim.journey.active && !sim.journey.ambush_pending);
    CC_CHECK(sim.journey.pace == CC_JOURNEY_PACE_CAREFUL);
    CC_CHECK(route->closed && sim.player.coins == 0);
    uint64_t departed = CcSimHash(&sim);
    CC_CHECK(!CcJournalApply(journal, &sim, &travel, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == departed);
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&restored) == departed);
    uint64_t start_tick = restored.clock.tick;
    for (int32_t step = 0; step < 12000 && restored.journey.active; ++step) {
        if (restored.journey.phase == CC_JOURNEY_PHASE_TRAVELLING)
            CcSimAdvanceRuntimeTicks(&restored, 1);
        else
            CC_CHECK(CcTestContinueJourneyPause(
                &restored, error, sizeof(error)));
    }
    CC_CHECK(!restored.journey.active);
    CC_CHECK(restored.player.location_id == destination->id);
    CC_CHECK(restored.carriage.location_id == destination->id);
    CC_CHECK(restored.clock.tick > start_tick);
    CC_CHECK(restored.player.cargo[CC_GOOD_WOOD] == 2);
    CC_CHECK(restored.player.cargo[CC_GOOD_BREAD] >= 0);
    CC_CHECK(restored.player.coins == 0 && restored.carriage.condition == 0);
    CC_CHECK(CcSimRoute(&restored, route->id)->closed);
    CC_CHECK(CcJournalCheckpoint(journal, &restored, error, sizeof(error)));
    uint64_t arrived = CcSimHash(&restored);
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&sim) == arrived);
    CcJournalAbandon(&journal);
    (void)remove(path);
    (void)remove("abandoned-company-exit.ccsave-wal");
    (void)remove("abandoned-company-exit.ccsave-shm");
}

int main(void)
{
    CheckFreeRoads();
    CheckPaymentsAtEncounter(false);
    CheckPaymentsAtEncounter(true);
    CheckJourneySaves();
    CheckAbandonedTownExit();
    puts("Free departures: every road, both directions, encounter payments and save replay passed");
    return 0;
}
