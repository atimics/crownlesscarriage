#include "persistence/cc_save.h"
#include "test_support.h"
#include "sim/cc_production.h"
#include "metagame/cc_metagame.h"

#include <sqlite3.h>
#include <string.h>

static CcSim sim;
static CcSim restored;
static char error[256];

static void PrepareStop(int32_t slot, bool reverse)
{
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    const CcRoadSite *site = &sim.road_sites[slot];
    const CcRoute *route = CcSimRoute(&sim, site->route_id);
    CC_CHECK(route != NULL);
    sim.player.location_id = reverse ? route->to_id : route->from_id;
    sim.carriage.location_id = sim.player.location_id;
    sim.player.coins = 10000;
    sim.player.cargo[CC_GOOD_WHEAT] = 8;
    sim.journey = (CcJourneyEncounter){
        .active = true,
        .phase = CC_JOURNEY_PHASE_TRAVELLING,
        .origin_id = sim.player.location_id,
        .destination_id = reverse ? route->from_id : route->to_id,
        .route_id = route->id,
        .total_subticks = CC_WORLD_WATCH_SUBTICKS * 2,
        .pace = CC_JOURNEY_PACE_STEADY,
        .encounter_triggered = true,
        .ambush_resolved = true,
        .danger = 5,
        .bargain_cost = 1,
        .fare_reserved = 1,
        .departure_day = sim.current_day
    };
    sim.carriage.route_id = route->id;
    sim.carriage.mode = CC_CARRIAGE_MOVING;
    sim.clock.game_minutes_per_second = CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim.carriage.origin_id = sim.journey.origin_id;
    sim.carriage.destination_id = sim.journey.destination_id;
    sim.carriage.progress_milli = reverse ?
        1000 - site->progress_milli : site->progress_milli;
    sim.journey.elapsed_subticks = (int32_t)(
        ((int64_t)sim.journey.total_subticks * sim.carriage.progress_milli + 999) / 1000);
    CcCommand pace = {.kind = CC_COMMAND_SET_JOURNEY_PACE,
                       .amount = CC_JOURNEY_PACE_STEADY};
    CC_CHECK(CcSimApply(&sim, &pace, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckChoices(void)
{
    for (int32_t slot = 0; slot < CC_MAX_ROAD_SITES; ++slot) {
        for (int32_t reverse = 0; reverse < 2; ++reverse) {
            PrepareStop(slot, reverse != 0);
            const CcRoadSite *site = &sim.road_sites[slot];
            CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == site);
            CcCommand choice = {.kind = CC_COMMAND_CAMP_ROAD_SITE,
                                 .target_id = site->id};
            sim.horse_team[0].fatigue = 40;
            sim.horse_team[0].hunger = 30;
            int32_t progress = sim.carriage.progress_milli;
            int32_t elapsed = sim.journey.elapsed_subticks;
            int64_t time = (int64_t)sim.current_day * CC_WORLD_DAY_SUBTICKS +
                sim.clock.minute_subticks;
            CC_CHECK(CcSimApply(&sim, &choice, error, sizeof(error)));
            CC_CHECK(sim.horse_team[0].fatigue == 32);
            CC_CHECK(sim.horse_team[0].hunger == 25);
            CC_CHECK(sim.journey.danger == 8);
            CC_CHECK(sim.carriage.progress_milli == progress);
            CC_CHECK(sim.journey.elapsed_subticks == elapsed);
            CC_CHECK((int64_t)sim.current_day * CC_WORLD_DAY_SUBTICKS +
                sim.clock.minute_subticks == time + CC_WORLD_WATCH_SUBTICKS);
            CC_CHECK(!site->accessible);
            CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == NULL);
            uint64_t hash = CcSimHash(&sim);
            CC_CHECK(!CcSimApply(&sim, &choice, error, sizeof(error)));
            CC_CHECK(CcSimHash(&sim) == hash);
            CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));

            PrepareStop(slot, reverse != 0);
            choice.kind = CC_COMMAND_PASS_ROAD_SITE;
            time = sim.clock.minute_subticks;
            int32_t fatigue = sim.horse_team[0].fatigue;
            CC_CHECK(CcSimApply(&sim, &choice, error, sizeof(error)));
            CC_CHECK(sim.clock.minute_subticks == time);
            CC_CHECK(sim.horse_team[0].fatigue == fatigue);
            CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == NULL);
        }
    }
    PrepareStop(0, false);
    CcCommand choice = {.kind = CC_COMMAND_CAMP_ROAD_SITE,
                         .target_id = sim.road_sites[3].id};
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &choice, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    sim.carriage.progress_milli -= 21;
    CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == NULL);
    sim.carriage.progress_milli += 52;
    CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == NULL);
    PrepareStop(0, false);
    sim.journey.phase = CC_JOURNEY_PHASE_BLOCKED;
    CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == NULL);
}

static void CheckPersistence(void)
{
    const char *path = "road-stop-test.ccsave";
    (void)remove(path);
    PrepareStop(0, false);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimJourneyRoadSiteStop(&restored) != NULL);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);
    for (int32_t camp = 0; camp < 2; ++camp) {
        PrepareStop(0, false);
        CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
        CC_CHECK(journal != NULL);
        CcCommand choice = {
            .kind = camp != 0 ? CC_COMMAND_CAMP_ROAD_SITE : CC_COMMAND_PASS_ROAD_SITE,
            .target_id = sim.road_sites[0].id
        };
        CC_CHECK(CcJournalApply(journal, &sim, &choice, error, sizeof(error)));
        CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
        CcJournalAbandon(&journal);
        journal = CcJournalResume(path, &restored, error, sizeof(error));
        CC_CHECK(journal != NULL);
        CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
        CC_CHECK(CcSimJourneyRoadSiteStop(&restored) == NULL);
        CC_CHECK(!CcJournalApply(journal, &restored, &choice, error, sizeof(error)));
        CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
        (void)remove(path);
    }
    for (uint32_t version = 37U; version <= 38U; ++version) {
        PrepareStop(0, false);
        CcSimAdvanceDays(&sim, 2);
        sim.schema_version = version;
        uint64_t legacy_hash = CcSimHash(&sim);
        CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
        sqlite3 *database = NULL;
        CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
        CC_CHECK(sqlite3_exec(database,
            "ALTER TABLE runtime_state DROP COLUMN road_site_stop_mask;",
            NULL, NULL, NULL) == SQLITE_OK);
        CC_CHECK(sqlite3_close(database) == SQLITE_OK);
        CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
        CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
        CC_CHECK(restored.journey.road_site_stop_mask == 0U);
        CC_CHECK(CcSimJourneyRoadSiteStop(&restored) != NULL);
        if (version == 38U) {
            restored.schema_version = version;
            CC_CHECK(CcSimHash(&restored) == legacy_hash);
        }
        (void)remove(path);
    }
}

static void CheckClearing(void)
{
    const char *path = "road-clearing-test.ccsave";
    for (int32_t slot = 0; slot < CC_MAX_ROAD_SITES; ++slot) {
        for (int32_t reverse = 0; reverse < 2; ++reverse) {
            PrepareStop(slot, reverse != 0);
            CcRoadSite *site = &sim.road_sites[slot];
            bool tree = site->blocker == CC_ROAD_SITE_BLOCKER_TREE;
            sim.player.cargo[CC_GOOD_TOOLS] = 2;
            sim.player.cargo[CC_GOOD_WOOD] = 1;
            int32_t condition = site->condition;
            int32_t progress = sim.carriage.progress_milli;
            int32_t elapsed = sim.journey.elapsed_subticks;
            /* Cross midnight to cover a world tick during the work. */
            sim.clock.minute_subticks = CC_WORLD_DAY_SUBTICKS - 1;
            int64_t time = (int64_t)sim.current_day * CC_WORLD_DAY_SUBTICKS +
                sim.clock.minute_subticks;
            CcCommand clear = {.kind = CC_COMMAND_CLEAR_ROAD_SITE,
                .target_id = site->id};
            (void)remove(path);
            CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
            CC_CHECK(journal != NULL);
            CC_CHECK(CcJournalApply(journal, &sim, &clear, error, sizeof(error)));
            CC_CHECK(site->accessible && site->blocker == CC_ROAD_SITE_BLOCKER_NONE);
            int32_t expected = condition + (tree ? 3 : 6);
            CC_CHECK(site->condition == (expected > 100 ? 100 : expected));
            CC_CHECK(sim.player.cargo[CC_GOOD_TOOLS] == (tree ? 2 : 0));
            CC_CHECK(sim.player.cargo[CC_GOOD_WOOD] == (tree ? 0 : 1));
            CC_CHECK(sim.carriage.progress_milli == progress);
            CC_CHECK(sim.journey.elapsed_subticks == elapsed);
            CC_CHECK((int64_t)sim.current_day * CC_WORLD_DAY_SUBTICKS +
                sim.clock.minute_subticks == time + (tree ? 1 : 2) * CC_WORLD_WATCH_SUBTICKS);
            CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == site);
            const CcEvent *event = CcSimEvent(&sim, sim.journey.parent_event_id);
            CC_CHECK(event != NULL && strstr(event->text, tree ? "fallen tree" : "rocks") != NULL);
            CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
            uint64_t hash = CcSimHash(&sim);
            CC_CHECK(!CcJournalApply(journal, &sim, &clear, error, sizeof(error)));
            CC_CHECK(CcSimHash(&sim) == hash);
            CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
            CcJournalAbandon(&journal);
            journal = CcJournalResume(path, &restored, error, sizeof(error));
            CC_CHECK(journal != NULL && CcSimHash(&restored) == hash);
            CC_CHECK(restored.road_sites[slot].accessible);
            CC_CHECK(restored.road_sites[slot].blocker == CC_ROAD_SITE_BLOCKER_NONE);
            CC_CHECK(restored.road_sites[slot].condition == site->condition);
            CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
            CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
            CC_CHECK(CcSimHash(&restored) == hash && restored.road_sites[slot].accessible);
            (void)remove(path);
        }
    }
    for (int32_t slot = 0; slot < CC_MAX_ROAD_SITES; ++slot) {
        PrepareStop(slot, false);
        CcCommand clear = {.kind = CC_COMMAND_CLEAR_ROAD_SITE,
            .target_id = sim.road_sites[slot].id};
        sim.player.cargo[CC_GOOD_TOOLS] = 0;
        uint64_t hash = CcSimHash(&sim);
        CC_CHECK(!CcSimApply(&sim, &clear, error, sizeof(error)));
        CC_CHECK(CcSimHash(&sim) == hash);
        sim.player.cargo[CC_GOOD_TOOLS] = 1;
        sim.player.cargo[CC_GOOD_WOOD] = 0;
        hash = CcSimHash(&sim);
        CC_CHECK(!CcSimApply(&sim, &clear, error, sizeof(error)));
        CC_CHECK(CcSimHash(&sim) == hash);
        sim.player.cargo[CC_GOOD_TOOLS] = 2;
        sim.player.cargo[CC_GOOD_WOOD] = 1;
        clear.target_id = sim.road_sites[(slot + 1) % CC_MAX_ROAD_SITES].id;
        hash = CcSimHash(&sim);
        CC_CHECK(!CcSimApply(&sim, &clear, error, sizeof(error)));
        CC_CHECK(CcSimHash(&sim) == hash);
        clear.target_id = sim.road_sites[slot].id;
        sim.carriage.progress_milli -= 21;
        hash = CcSimHash(&sim);
        CC_CHECK(!CcSimApply(&sim, &clear, error, sizeof(error)));
        CC_CHECK(CcSimHash(&sim) == hash);
    }
}

static void CheckPre62Save(void)
{
    const char *path = "road-clearing-legacy-test.ccsave";
    PrepareStop(0, false);
    sim.schema_version = 62U;
    sim.player.cargo[CC_GOOD_TOOLS] = 2;
    sim.player.cargo[CC_GOOD_WOOD] = 1;
    uint64_t hash = CcSimHash(&sim);
    CcCommand clear = {.kind = CC_COMMAND_CLEAR_ROAD_SITE,
        .target_id = sim.road_sites[0].id};
    CC_CHECK(!CcSimApply(&sim, &clear, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    restored.schema_version = 62U;
    CC_CHECK(CcSimHash(&restored) == hash);
    restored.schema_version = CC_SIM_SCHEMA_VERSION;
    CC_CHECK(CcSimApply(&restored, &clear, error, sizeof(error)));
    CC_CHECK(restored.road_sites[0].accessible);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    (void)remove(path);
}

static void CheckStoreTransfers(void)
{
    const char *path = "road-store-test.ccsave";
    for (int32_t slot = 0; slot < CC_MAX_ROAD_SITES; ++slot) {
        PrepareStop(slot, slot % 2 != 0);
        CcRoadSite *site = &sim.road_sites[slot];
        site->accessible = true;
        site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
        memset(sim.player.cargo, 0, sizeof(sim.player.cargo));
        sim.player.treasure_cargo_slots = 0;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            sim.player.cargo[good] = 2;
            uint64_t initial = CcSimHash(&sim);
            site->stock[good]++;
            CC_CHECK(CcSimHash(&sim) != initial);
            site->stock[good]--;
            (void)remove(path);
            CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
            CC_CHECK(journal != NULL);
            CcCommand transfer = {.kind = CC_COMMAND_TRANSFER_ROAD_SITE,
                .target_id = site->id, .good = (CcGood)good, .amount = 1};
            int32_t time = sim.clock.minute_subticks;
            CcSettlement town = *CcSimSettlement(&sim, site->home_settlement_id);
            CC_CHECK(CcJournalApply(journal, &sim, &transfer, error, sizeof(error)));
            CC_CHECK(site->stock[good] == 1 && sim.player.cargo[good] == 1);
            CC_CHECK(sim.clock.minute_subticks == time);
            CC_CHECK(memcmp(&town, CcSimSettlement(&sim, site->home_settlement_id), sizeof(town)) == 0);
            CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
            CcJournalAbandon(&journal);
            journal = CcJournalResume(path, &restored, error, sizeof(error));
            CC_CHECK(journal != NULL && CcSimHash(&restored) == CcSimHash(&sim));
            CC_CHECK(restored.road_sites[slot].stock[good] == 1);
            transfer.amount = -1;
            CC_CHECK(CcJournalApply(journal, &restored, &transfer, error, sizeof(error)));
            CC_CHECK(restored.road_sites[slot].stock[good] == 0 && restored.player.cargo[good] == 2);
            uint64_t hash = CcSimHash(&restored);
            CC_CHECK(!CcJournalApply(journal, &restored, &transfer, error, sizeof(error)));
            CC_CHECK(CcSimHash(&restored) == hash);
            CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
            CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
            CC_CHECK(restored.road_sites[slot].stock[good] == 0);
            site->stock[good] = 0;
            sim.player.cargo[good] = 0;
        }
    }
    PrepareStop(0, false);
    sim.road_sites[0].accessible = true;
    sim.road_sites[0].blocker = CC_ROAD_SITE_BLOCKER_NONE;
    sim.schema_version = 63;
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    restored.schema_version = 63;
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    restored.schema_version = CC_SIM_SCHEMA_VERSION;
    for (int32_t slot = 0; slot < restored.road_site_count; ++slot)
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
            CC_CHECK(restored.road_sites[slot].stock[good] == 0);
    (void)remove(path);
}

static void CheckStoreRejections(void)
{
    PrepareStop(0, false);
    CcRoadSite *site = &sim.road_sites[0];
    site->accessible = true;
    site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
    memset(sim.player.cargo, 0, sizeof(sim.player.cargo));
    sim.player.treasure_cargo_slots = 0;
    sim.player.cargo[CC_GOOD_TOOLS] = 1;
    int32_t units = CcGoodDefinitionFor(CC_GOOD_TOOLS)->player_units_per_slot;
    site->stock[CC_GOOD_TOOLS] = CC_ROAD_SITE_CAPACITY * units;
    CcCommand transfer = {.kind = CC_COMMAND_TRANSFER_ROAD_SITE,
        .target_id = site->id, .good = CC_GOOD_TOOLS, .amount = 1};
#define STORE_REFUSAL() do { uint64_t before = CcSimHash(&sim);     CC_CHECK(!CcSimApply(&sim, &transfer, error, sizeof(error)));     CC_CHECK(CcSimHash(&sim) == before); } while (0)
    STORE_REFUSAL();
    site->stock[CC_GOOD_TOOLS] = 1;
    sim.player.cargo[CC_GOOD_TOOLS] = sim.player.cargo_capacity * units;
    transfer.amount = -1; STORE_REFUSAL();
    sim.player.cargo[CC_GOOD_TOOLS] = 1;
    site->accessible = false; site->blocker = CC_ROAD_SITE_BLOCKER_TREE; STORE_REFUSAL();
    site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
    sim.carriage.progress_milli -= 21; STORE_REFUSAL();
    sim.carriage.progress_milli += 21;
    transfer.good = CC_GOOD_COUNT; STORE_REFUSAL();
    transfer.good = CC_GOOD_TOOLS; transfer.amount = 0; STORE_REFUSAL();
    transfer.amount = INT32_MIN; STORE_REFUSAL();
#undef STORE_REFUSAL
    const char *path = "road-store-missing-row.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "DELETE FROM road_site_stock WHERE site_slot=0 AND good=0;",
        NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &restored, error, sizeof(error)));
    (void)remove(path);
}

static void CheckCarriageDelivery(void)
{
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcRoadSite *site = &sim.road_sites[0];
    const CcRoute *route = CcSimRoute(&sim, site->route_id);
    CC_CHECK(route != NULL);
    sim.player.location_id = route->from_id;
    sim.carriage.location_id = route->from_id;
    sim.player.coins = 10000;
    memset(sim.player.cargo, 0, sizeof(sim.player.cargo));
    sim.player.cargo[CC_GOOD_WHEAT] = 2;
    sim.player.cargo[CC_GOOD_TOOLS] = 2;
    sim.player.cargo[CC_GOOD_WOOD] = 1;
    CcCommand transfer = {.kind = CC_COMMAND_TRANSFER_ROAD_SITE,
        .target_id = site->id, .good = CC_GOOD_WHEAT, .amount = 2};
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &transfer, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = route->to_id};
    CC_CHECK(CcSimApply(&sim, &travel, error, sizeof(error)));
    sim.journey.encounter_triggered = true;
    sim.journey.ambush_pending = false;
    int32_t ticks = 0;
    while (CcSimJourneyRoadSiteStop(&sim) != site && ticks < 10000) {
        CcSimAdvanceRuntimeTicks(&sim, 1);
        ticks++;
    }
    CC_CHECK(ticks > 0 && ticks < 10000);
    CC_CHECK(sim.journey.elapsed_subticks > 0 && sim.carriage.progress_milli > 0);
    CcCommand clear = {.kind = CC_COMMAND_CLEAR_ROAD_SITE, .target_id = site->id};
    CC_CHECK(CcSimApply(&sim, &clear, error, sizeof(error)));
    CC_CHECK(CcSimApply(&sim, &transfer, error, sizeof(error)));
    CC_CHECK(site->stock[CC_GOOD_WHEAT] == 2 && sim.player.cargo[CC_GOOD_WHEAT] == 0);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckRepairs(void)
{
    const char *path = "road-repair-test.ccsave";
    CC_CHECK(CcSimPlanRoadSiteRepair(NULL, 0).gate == CC_PRODUCTION_CLOSED);
    for (int32_t slot = 0; slot < CC_MAX_ROAD_SITES; ++slot) {
        for (int32_t reverse = 0; reverse < 2; ++reverse) {
            PrepareStop(slot, reverse != 0);
            CcRoadSite *site = &sim.road_sites[slot];
            site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
            site->condition = 44;
            sim.player.cargo[CC_GOOD_TOOLS] = 2;
            sim.player.cargo[CC_GOOD_WOOD] = 1;
            sim.clock.minute_subticks = CC_WORLD_DAY_SUBTICKS - 1;
            int64_t time = (int64_t)sim.current_day * CC_WORLD_DAY_SUBTICKS + sim.clock.minute_subticks;
            int32_t progress = sim.carriage.progress_milli, elapsed = sim.journey.elapsed_subticks;
            uint64_t before = CcSimHash(&sim);
            CcProductionReceipt plan = CcSimPlanRoadSiteRepair(&sim, site->id);
            CC_CHECK(plan.gate == CC_PRODUCTION_READY && plan.inputs[0] == 1 && plan.inputs[1] == 1 && plan.work == 2);
            CC_CHECK(CcSimHash(&sim) == before);
            CcCommand repair = {.kind = CC_COMMAND_REPAIR_ROAD_SITE, .target_id = site->id};
            (void)remove(path);
            CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
            CC_CHECK(journal != NULL);
            CC_CHECK(CcJournalApply(journal, &sim, &repair, error, sizeof(error)));
            CC_CHECK(site->condition == 54 && site->accessible);
            CC_CHECK(sim.player.cargo[CC_GOOD_TOOLS] == 1 && sim.player.cargo[CC_GOOD_WOOD] == 0);
            CC_CHECK(sim.carriage.progress_milli == progress && sim.journey.elapsed_subticks == elapsed);
            CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == site);
            CC_CHECK((int64_t)sim.current_day * CC_WORLD_DAY_SUBTICKS + sim.clock.minute_subticks == time + 2 * CC_WORLD_WATCH_SUBTICKS);
            CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
            uint64_t after = CcSimHash(&sim);
            CC_CHECK(!CcJournalApply(journal, &sim, &repair, error, sizeof(error)));
            CC_CHECK(CcSimHash(&sim) == after);
            CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
            CcJournalAbandon(&journal);
            journal = CcJournalResume(path, &restored, error, sizeof(error));
            CC_CHECK(journal != NULL && CcSimHash(&restored) == after);
            CC_CHECK(restored.road_sites[slot].condition == 54);
            CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
            CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)) && CcSimHash(&restored) == after);
            (void)remove(path);
        }
    }
    for (int32_t failure = 0; failure < 6; ++failure) {
        PrepareStop(2, false);
        CcRoadSite *site = &sim.road_sites[2];
        site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE; site->condition = 97;
        sim.player.cargo[CC_GOOD_TOOLS] = 2; sim.player.cargo[CC_GOOD_WOOD] = 1;
        CcCommand repair = {.kind = CC_COMMAND_REPAIR_ROAD_SITE, .target_id = site->id};
        if (failure == 0) { site->accessible = false; site->blocker = CC_ROAD_SITE_BLOCKER_TREE; }
        if (failure == 1) repair.target_id = sim.road_sites[3].id;
        if (failure == 2) sim.player.cargo[CC_GOOD_TOOLS] = 1;
        if (failure == 3) sim.player.cargo[CC_GOOD_WOOD] = 0;
        if (failure == 4) site->condition = 100;
        if (failure == 5) sim.schema_version = 66;
        uint64_t hash = CcSimHash(&sim);
        CC_CHECK(!CcSimApply(&sim, &repair, error, sizeof(error)) && CcSimHash(&sim) == hash);
    }
    PrepareStop(2, false);
    CcRoadSite *site = &sim.road_sites[2];
    site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE; site->condition = 97;
    sim.player.cargo[CC_GOOD_TOOLS] = 2; sim.player.cargo[CC_GOOD_WOOD] = 1;
    sim.schema_version = 66;
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    sim = restored;
    CC_CHECK(sim.schema_version == CC_SIM_SCHEMA_VERSION && site->condition == 97);
    CcCommand repair = {.kind = CC_COMMAND_REPAIR_ROAD_SITE, .target_id = site->id};
    CC_CHECK(CcSimApply(&sim, &repair, error, sizeof(error)) && site->condition == 100);
    (void)remove(path);
}

static void CheckRepairRestartsMill(void)
{
    static CcMetagame game;
    static CcProductionAccounting accounting;
    PrepareStop(2, false);
    CcRoadSite *site = &sim.road_sites[2];
    CC_CHECK(site->kind == CC_ROAD_SITE_MILL);
    site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE; site->condition = 44;
    site->stock[CC_GOOD_TOOLS] = 1; site->stock[CC_GOOD_WHEAT] = 6;
    sim.player.cargo[CC_GOOD_TOOLS] = 2; sim.player.cargo[CC_GOOD_WOOD] = 1;
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_CONDITION);
    CcMetagameInit(&game, 42);
    game.sim = sim;
    char output[2048];
    CC_CHECK(CcMetagameExecute(&game, "road repair", output, sizeof(output)));
    sim = game.sim;
    CC_CHECK(site->condition == 54 && site->stock[CC_GOOD_WHEAT] == 6 && site->stock[CC_GOOD_BREAD] == 0);
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_READY);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 7, NULL, NULL, &accounting);
    CC_CHECK(accounting.sites[2].input[CC_GOOD_WHEAT] == 1);
    CC_CHECK(accounting.sites[2].output[CC_GOOD_BREAD] == 1);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

int main(void)
{
    CheckRepairs();
    CheckRepairRestartsMill();
    CheckChoices();
    CheckPersistence();
    CheckClearing();
    CheckPre62Save();
    CheckStoreTransfers();
    CheckStoreRejections();
    CheckCarriageDelivery();
    (void)puts("Roadside camps: both directions, costs, pass, replay and save upgrade passed");
    return 0;
}
