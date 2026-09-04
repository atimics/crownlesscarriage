#include "persistence/cc_save.h"
#include "test_support.h"

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
    sim.carriage.progress_milli -= 1;
    CC_CHECK(CcSimJourneyRoadSiteStop(&sim) == NULL);
    sim.carriage.progress_milli += 12;
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
    PrepareStop(0, false);
    sim.schema_version = 37U;
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
    (void)remove(path);
}

int main(void)
{
    CheckChoices();
    CheckPersistence();
    (void)puts("Roadside camps: both directions, costs, pass, replay and save upgrade passed");
    return 0;
}
