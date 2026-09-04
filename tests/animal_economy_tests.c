#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static void AdvanceJourney(CcSim *sim)
{
    while (sim->journey.active &&
           sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    }
}

static int32_t TotalCows(const CcSim *sim)
{
    int32_t total = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        total += sim->settlements[i].cow_adults;
        total += sim->settlements[i].cow_calves;
    }
    return total;
}

static int32_t CountEvents(const CcSim *sim, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

int main(void)
{
    char error[192];
    CcSim travel;
    CcSimInit(&travel, UINT32_C(0xa11a1));
    CC_CHECK(strcmp(travel.horse_team[0].name, "Bracken") == 0);
    CC_CHECK(strcmp(travel.horse_team[1].name, "Morrow") == 0);
    CC_CHECK(CcIdKind(travel.horse_team[0].id) == CC_ENTITY_HORSE);
    CC_CHECK(CcIdKind(travel.horse_team[1].id) == CC_ENTITY_HORSE);
    CC_CHECK(travel.horse_team[0].id != travel.horse_team[1].id);
    CC_CHECK(CcSimHorseTeamReadiness(&travel) == 100);
    CC_CHECK(TotalCows(&travel) > 0);

    CcTravelPreview preview = {0};
    CC_CHECK(CcSimTravelPreview(
        &travel, travel.settlements[1].id, &preview,
        error, sizeof(error)));
    CC_CHECK(preview.horse_feed_required > 0);
    int32_t original_wheat = travel.settlements[0].stock[CC_GOOD_WHEAT];
    travel.settlements[0].stock[CC_GOOD_WHEAT] = 0;
    CcCommand depart = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = travel.settlements[1].id
    };
    CC_CHECK(!CcSimApply(&travel, &depart, error, sizeof(error)));
    CC_CHECK(strstr(error, "fodder") != NULL);
    travel.settlements[0].stock[CC_GOOD_WHEAT] = original_wheat;
    CC_CHECK(CcSimApply(&travel, &depart, error, sizeof(error)));
    CC_CHECK(travel.settlements[0].stock[CC_GOOD_WHEAT] ==
             original_wheat - preview.horse_feed_required);
    travel.journey.ambush_pending = false;
    AdvanceJourney(&travel);
    CC_CHECK(!travel.journey.active);
    CC_CHECK(travel.horse_team[0].fatigue > 0);
    CC_CHECK(CcSimHorseTeamReadiness(&travel) < 100);
    int32_t tired = travel.horse_team[0].fatigue;
    CcSimAdvanceDays(&travel, 7);
    CC_CHECK(travel.horse_team[0].fatigue < tired);

    CcSim with_cows;
    CcSim without_cows;
    CcSimInit(&with_cows, UINT32_C(0xca771e));
    CcSimInit(&without_cows, UINT32_C(0xca771e));
    with_cows.current_day = 6;
    without_cows.current_day = 6;
    with_cows.dragon.hunt_cooldown_days = 1000;
    without_cows.dragon.hunt_cooldown_days = 1000;
    with_cows.settlements[0].cow_adults = 12;
    with_cows.settlements[0].cow_calves = 0;
    with_cows.settlements[0].cow_condition = 100;
    without_cows.settlements[0].cow_adults = 0;
    without_cows.settlements[0].cow_calves = 0;
    without_cows.settlements[0].cow_condition = 0;
    int32_t cow_food_before = with_cows.settlements[0].stock[CC_GOOD_FOOD];
    int32_t plain_food_before = without_cows.settlements[0].stock[CC_GOOD_FOOD];
    CC_CHECK(cow_food_before == plain_food_before);
    CcSimAdvanceDays(&with_cows, 1);
    CcSimAdvanceDays(&without_cows, 1);
    CC_CHECK(with_cows.settlements[0].stock[CC_GOOD_FOOD] >
             without_cows.settlements[0].stock[CC_GOOD_FOOD]);

    CcSim famine;
    CcSimInit(&famine, UINT32_C(0xf4a11e));
    famine.current_day = 6;
    famine.dragon.hunt_cooldown_days = 1000;
    CcSettlement *hungry_herd = &famine.settlements[0];
    hungry_herd->cow_adults = 3;
    hungry_herd->cow_calves = 0;
    hungry_herd->cow_condition = 50;
    hungry_herd->cow_hunger = 64;
    hungry_herd->stock[CC_GOOD_FOOD] = 0;
    hungry_herd->stock[CC_GOOD_WHEAT] = 0;
    CcSimAdvanceDays(&famine, 1);
    CC_CHECK(hungry_herd->cow_adults == 2);
    CC_CHECK(CountEvents(&famine, CC_EVENT_COW_SLAUGHTERED) == 1);

    CcSim hunted;
    CcSimInit(&hunted, UINT32_C(0xd2a60ec0));
    int32_t cows_before_hunt = TotalCows(&hunted);
    hunted.dragon.body_condition = 20;
    hunted.dragon.hunt_cooldown_days = 0;
    CcSimAdvanceDays(&hunted, 1);
    CC_CHECK(TotalCows(&hunted) < cows_before_hunt);
    CC_CHECK(CountEvents(&hunted, CC_EVENT_DRAGON_HUNT) == 1);

    const char *path = "/tmp/crownless-animal-economy-tests.ccsave";
    (void)remove(path);
    with_cows.horse_team[0].fatigue = 33;
    uint64_t expected_hash = CcSimHash(&with_cows);
    CC_CHECK(CcSaveWrite(path, &with_cows, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    CC_CHECK(restored.horse_team[0].fatigue == 33);
    CC_CHECK(restored.settlements[0].cow_adults ==
             with_cows.settlements[0].cow_adults);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);

    puts("Horse and cow economy tests passed");
    return 0;
}
