#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

/* The carriage's feed tray (schema 101), design: docs/pony-charge-travel.md.
   Wheat bought in town pours into the tray while parked, the team eats from
   the tray in town, the market is never eaten from at departure, and camps
   graze the team on the road. */

static CcSim sim;
static CcSim restored;
static char error[256];

static void TestTrayPoursFromCargoAndClamps(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed720));
    sim.player.cargo[CC_GOOD_WHEAT] = 12;
    sim.player.feed_tray_wheat = 0;
    CcSimAdvanceDays(&sim, 1);
    /* One crate fills the tray; the rest stays in cargo. */
    CC_CHECK(sim.player.feed_tray_wheat == CC_FEED_TRAY_CAPACITY);
    CC_CHECK(sim.player.cargo[CC_GOOD_WHEAT] == 2);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void TestTeamEatsFromTheTrayInTown(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed720));
    sim.player.cargo[CC_GOOD_WHEAT] = 10;
    for (int32_t i = 0; i < CcSimHorseTeamCount(&sim); ++i) {
        sim.horse_team[i].hunger = 40;
    }
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.player.feed_tray_wheat ==
             CC_FEED_TRAY_CAPACITY - CcSimHorseTeamCount(&sim));
    for (int32_t i = 0; i < CcSimHorseTeamCount(&sim); ++i) {
        CC_CHECK(sim.horse_team[i].hunger == 34);
    }
    /* An empty tray does not feed, and never goes negative. */
    sim.player.feed_tray_wheat = 0;
    for (int32_t i = 0; i < CcSimHorseTeamCount(&sim); ++i) {
        sim.horse_team[i].hunger = 20;
    }
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.player.feed_tray_wheat == 0);
    for (int32_t i = 0; i < CcSimHorseTeamCount(&sim); ++i) {
        CC_CHECK(sim.horse_team[i].hunger == 20);
    }
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void TestTraySurvivesTheSaveRoundTrip(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed720));
    sim.player.cargo[CC_GOOD_WHEAT] = 6;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.player.feed_tray_wheat == 6);
    const char *path = "feed-tray-round-trip.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.player.feed_tray_wheat == 6);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);
    (void)remove("feed-tray-round-trip.ccsave-wal");
    (void)remove("feed-tray-round-trip.ccsave-shm");
}

static void TestDepartureNeverTouchesTheMarket(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed720));
    CcTravelPreview preview = {0};
    CC_CHECK(CcSimTravelPreview(
        &sim, sim.settlements[1].id, &preview, error, sizeof(error)));
    int32_t wheat = sim.settlements[0].stock[CC_GOOD_WHEAT];
    /* A spent team with an empty tray and a stocked market: the market
       stays whole, and the team departs hungry and careful. */
    for (int32_t i = 0; i < CcSimHorseTeamCount(&sim); ++i) {
        sim.horse_team[i].hunger = 90;
        sim.horse_team[i].fatigue = 60;
    }
    CC_CHECK(CcSimHorseTeamReadiness(&sim) < 30);
    CC_CHECK(sim.player.feed_tray_wheat == 0);
    CcCommand depart = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[1].id
    };
    CC_CHECK(CcSimApply(&sim, &depart, error, sizeof(error)));
    CC_CHECK(sim.journey.active);
    CC_CHECK(sim.journey.pace == CC_JOURNEY_PACE_CAREFUL);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] == wheat);
    (void)preview;
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

int main(void)
{
    TestTrayPoursFromCargoAndClamps();
    TestTeamEatsFromTheTrayInTown();
    TestTraySurvivesTheSaveRoundTrip();
    TestDepartureNeverTouchesTheMarket();
    return 0;
}
