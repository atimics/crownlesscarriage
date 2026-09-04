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

static int32_t TotalSheep(const CcSim *sim)
{
    int32_t total = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        total += sim->settlements[i].sheep_adults;
        total += sim->settlements[i].sheep_lambs;
    }
    return total;
}

static const CcEvent *EventOfKind(const CcSim *sim, CcEventKind kind)
{
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) return event;
    }
    return NULL;
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
    CC_CHECK(TotalSheep(&travel) > 0);

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
    hungry_herd->stock[CC_GOOD_FOOD] = 10;
    hungry_herd->stock[CC_GOOD_WHEAT] = 0;
    int32_t beef_before = hungry_herd->stock[CC_GOOD_MEAT];
    CcSimAdvanceDays(&famine, 1);
    CC_CHECK(hungry_herd->cow_adults == 2);
    CC_CHECK(hungry_herd->stock[CC_GOOD_MEAT] == beef_before + 4);
    CC_CHECK(CountEvents(&famine, CC_EVENT_COW_SLAUGHTERED) == 1);
    CC_CHECK(strstr(EventOfKind(&famine, CC_EVENT_COW_SLAUGHTERED)->text,
                    "beef") != NULL);

    CcSim legacy_week;
    CcSimInit(&legacy_week, UINT32_C(0x1e9ac30));
    legacy_week.schema_version = 30U;
    legacy_week.generator_version = 23U;
    legacy_week.current_day = 272;
    legacy_week.settlement_count = 1;
    legacy_week.route_count = 0;
    legacy_week.shipment_count = 0;
    legacy_week.courier_count = 0;
    legacy_week.bandit_count = 0;
    legacy_week.monster_count = 0;
    legacy_week.dungeon_count = 0;
    legacy_week.situation_count = 0;
    legacy_week.dragon.slain = true;
    legacy_week.goblins.tribute_cooldown_days = 1000;
    legacy_week.hoard_raiders.cooldown_days = 1000;
    legacy_week.event_count = 0;
    legacy_week.event_write_index = 0;
    CcSettlement *legacy_farm = &legacy_week.settlements[0];
    legacy_farm->cow_adults = 3;
    legacy_farm->cow_calves = 0;
    legacy_farm->cow_condition = 50;
    legacy_farm->cow_hunger = 64;
    legacy_farm->sheep_adults = 24;
    legacy_farm->sheep_lambs = 6;
    legacy_farm->stock[CC_GOOD_WHEAT] = 0;
    legacy_farm->stock[CC_GOOD_WOOL] = 10;
    int32_t legacy_meat = legacy_farm->stock[CC_GOOD_MEAT];
    CcSimAdvanceDays(&legacy_week, 1);
    CC_CHECK(legacy_farm->stock[CC_GOOD_MEAT] == legacy_meat);
    CC_CHECK(legacy_farm->stock[CC_GOOD_WOOL] == 10);
    CC_CHECK(legacy_farm->sheep_adults == 24);
    CC_CHECK(legacy_farm->sheep_lambs == 6);
    const CcEvent *legacy_slaughter = EventOfKind(
        &legacy_week, CC_EVENT_COW_SLAUGHTERED);
    CC_CHECK(legacy_slaughter != NULL);
    CC_CHECK(legacy_slaughter->magnitude == 1);
    CC_CHECK(strstr(legacy_slaughter->text, "4 Food") != NULL);

    CcSim invalid_pair = legacy_week;
    invalid_pair.generator_version = CC_GENERATOR_VERSION;
    CC_CHECK(!CcSimValidate(&invalid_pair, error, sizeof(error)));

    CcSim spring;
    CcSimInit(&spring, UINT32_C(0x5ee91));
    spring.current_day = 84;
    spring.dragon.slain = true;
    spring.event_count = 0;
    spring.event_write_index = 0;
    spring.player.location_id = spring.settlements[1].id;
    spring.carriage.location_id = spring.player.location_id;
    CcSettlement *spring_flock = &spring.settlements[0];
    spring_flock->population = 1000;
    spring_flock->sheep_adults = 20;
    spring_flock->sheep_lambs = 4;
    spring_flock->sheep_condition = 90;
    spring_flock->sheep_hunger = 0;
    spring_flock->stock[CC_GOOD_WOOL] = 0;
    CcSimAdvanceDays(&spring, 7);
    CC_CHECK(spring_flock->stock[CC_GOOD_WOOL] == 5);
    CC_CHECK(spring_flock->sheep_adults == 24);
    CC_CHECK(spring_flock->sheep_lambs == 4);
    CC_CHECK(CountEvents(&spring, CC_EVENT_SHEEP_SHEARED) >= 1);
    CC_CHECK(CountEvents(&spring, CC_EVENT_SHEEP_BRED) >= 1);

    CcSim winter;
    CcSimInit(&winter, UINT32_C(0x71e7e2));
    winter.current_day = 273;
    winter.dragon.slain = true;
    winter.player.location_id = winter.settlements[1].id;
    winter.carriage.location_id = winter.player.location_id;
    CcSettlement *winter_flock = &winter.settlements[0];
    winter_flock->sheep_adults = 24;
    winter_flock->sheep_lambs = 0;
    winter_flock->sheep_condition = 70;
    winter_flock->sheep_hunger = 30;
    winter_flock->cow_adults = 0;
    winter_flock->cow_calves = 0;
    winter_flock->stock[CC_GOOD_WHEAT] = 1;
    winter_flock->production[CC_GOOD_WHEAT] = 0;
    CcSimAdvanceDays(&winter, 7);
    CC_CHECK(winter_flock->stock[CC_GOOD_WHEAT] == 0);
    CC_CHECK(winter_flock->sheep_hunger < 30);
    CC_CHECK(winter_flock->sheep_condition > 70);

    CcSim winter_hunger;
    CcSimInit(&winter_hunger, UINT32_C(0x71e7e3));
    winter_hunger.current_day = 273;
    winter_hunger.dragon.slain = true;
    winter_hunger.event_count = 0;
    winter_hunger.event_write_index = 0;
    winter_hunger.player.location_id = winter_hunger.settlements[1].id;
    winter_hunger.carriage.location_id = winter_hunger.player.location_id;
    CcSettlement *hungry_flock = &winter_hunger.settlements[0];
    hungry_flock->sheep_adults = 16;
    hungry_flock->sheep_lambs = 0;
    hungry_flock->sheep_condition = 70;
    hungry_flock->sheep_hunger = 60;
    hungry_flock->cow_adults = 0;
    hungry_flock->cow_calves = 0;
    hungry_flock->stock[CC_GOOD_WHEAT] = 0;
    hungry_flock->stock[CC_GOOD_BREAD] = 10;
    int32_t mutton_before = hungry_flock->stock[CC_GOOD_MEAT];
    CcSimAdvanceDays(&winter_hunger, 7);
    CC_CHECK(hungry_flock->sheep_adults == 15);
    CC_CHECK(hungry_flock->stock[CC_GOOD_MEAT] == mutton_before + 2);
    const CcEvent *mutton_event = EventOfKind(
        &winter_hunger, CC_EVENT_SHEEP_SLAUGHTERED);
    CC_CHECK(mutton_event != NULL);
    CC_CHECK(strstr(mutton_event->text, "mutton") != NULL);

    CcSim planned;
    CcSimInit(&planned, UINT32_C(0xc011ed));
    planned.current_day = 266;
    planned.dragon.slain = true;
    planned.event_count = 0;
    planned.event_write_index = 0;
    planned.player.location_id = planned.settlements[1].id;
    planned.carriage.location_id = planned.player.location_id;
    CcSettlement *planned_flock = &planned.settlements[0];
    planned_flock->population = 400;
    planned_flock->sheep_adults = 20;
    planned_flock->sheep_lambs = 0;
    planned_flock->sheep_condition = 90;
    planned_flock->sheep_hunger = 0;
    planned_flock->cow_adults = 0;
    planned_flock->cow_calves = 0;
    planned_flock->stock[CC_GOOD_WHEAT] = 2;
    planned_flock->stock[CC_GOOD_BREAD] = 10;
    mutton_before = planned_flock->stock[CC_GOOD_MEAT];
    CcSimAdvanceDays(&planned, 7);
    CC_CHECK(planned_flock->sheep_adults == 19);
    CC_CHECK(planned_flock->stock[CC_GOOD_MEAT] == mutton_before + 2);
    CC_CHECK(CountEvents(&planned, CC_EVENT_SHEEP_SLAUGHTERED) == 1);

    CcSim wool_use;
    CcSimInit(&wool_use, UINT32_C(0x7001));
    wool_use.current_day = 273;
    wool_use.dragon.slain = true;
    wool_use.route_count = 0;
    wool_use.shipment_count = 0;
    wool_use.player.location_id = wool_use.settlements[1].id;
    wool_use.carriage.location_id = wool_use.player.location_id;
    CcSettlement *winter_town = &wool_use.settlements[0];
    winter_town->population = 2400;
    winter_town->sheep_adults = 0;
    winter_town->sheep_lambs = 0;
    winter_town->cow_adults = 0;
    winter_town->cow_calves = 0;
    winter_town->stock[CC_GOOD_WOOL] = 10;
    winter_town->stock[CC_GOOD_BREAD] = 10;
    CcSimAdvanceDays(&wool_use, 7);
    CC_CHECK(winter_town->stock[CC_GOOD_WOOL] == 8);

    CcSim blankets;
    CcSimInit(&blankets, UINT32_C(0xb1a4ce7));
    blankets.current_day = 98;
    blankets.dragon.slain = true;
    CcSettlement *war_seat = &blankets.settlements[2];
    int32_t war_burden = CcSimWarBurdenAtSettlement(
        &blankets, war_seat->id);
    CC_CHECK(war_burden >= 20);
    war_seat->stock[CC_GOOD_WOOL] = 10;
    int32_t blanket_use = 1 + war_burden / 50;
    CcSimAdvanceDays(&blankets, 7);
    CC_CHECK(war_seat->stock[CC_GOOD_WOOL] == 10 - blanket_use);

    CcSim hunted;
    CcSimInit(&hunted, UINT32_C(0xd2a60ec0));
    int32_t cows_before_hunt = TotalCows(&hunted);
    hunted.dragon.body_condition = 20;
    hunted.dragon.hunt_cooldown_days = 0;
    CcSimAdvanceDays(&hunted, 1);
    CC_CHECK(TotalCows(&hunted) < cows_before_hunt);
    CC_CHECK(CountEvents(&hunted, CC_EVENT_DRAGON_HUNT) == 1);

    CcSim sheep_hunt;
    CcSimInit(&sheep_hunt, UINT32_C(0x5ee9d2a6));
    for (int32_t i = 0; i < sheep_hunt.settlement_count; ++i) {
        sheep_hunt.settlements[i].cow_adults = 0;
        sheep_hunt.settlements[i].cow_calves = 0;
        sheep_hunt.settlements[i].sheep_adults = 0;
        sheep_hunt.settlements[i].sheep_lambs = 0;
        sheep_hunt.settlements[i].stock[CC_GOOD_BREAD] = 0;
    }
    sheep_hunt.settlements[0].sheep_adults = 12;
    int32_t sheep_before_hunt = TotalSheep(&sheep_hunt);
    sheep_hunt.dragon.body_condition = 20;
    sheep_hunt.dragon.hunt_cooldown_days = 0;
    CcSimAdvanceDays(&sheep_hunt, 1);
    CC_CHECK(TotalSheep(&sheep_hunt) < sheep_before_hunt);
    const CcEvent *sheep_hunt_event = EventOfKind(
        &sheep_hunt, CC_EVENT_DRAGON_HUNT);
    CC_CHECK(sheep_hunt_event != NULL);
    CC_CHECK(strstr(sheep_hunt_event->text, "sheep") != NULL);

    CcSim trader;
    CcSimInit(&trader, UINT32_C(0x6ea7));
    CcSettlement *market = CcSimSettlementMutable(
        &trader, trader.player.location_id);
    market->stock[CC_GOOD_MEAT] = 8;
    market->price[CC_GOOD_MEAT] = 1;
    int32_t meat_cargo = trader.player.cargo[CC_GOOD_MEAT];
    CcCommand buy_meat = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_MEAT,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&trader, &buy_meat, error, sizeof(error)));
    CC_CHECK(trader.player.cargo[CC_GOOD_MEAT] == meat_cargo + 1);
    CC_CHECK(strcmp(CcGoodName(CC_GOOD_MEAT), "Meat") == 0);

    const char *path = "/tmp/crownless-animal-economy-tests.ccsave";
    (void)remove(path);
    with_cows.horse_team[0].fatigue = 33;
    with_cows.settlements[0].sheep_adults = 31;
    with_cows.settlements[0].sheep_lambs = 9;
    with_cows.settlements[0].sheep_condition = 77;
    with_cows.settlements[0].sheep_hunger = 22;
    uint64_t expected_hash = CcSimHash(&with_cows);
    CC_CHECK(CcSaveWrite(path, &with_cows, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    CC_CHECK(restored.horse_team[0].fatigue == 33);
    CC_CHECK(restored.settlements[0].cow_adults ==
             with_cows.settlements[0].cow_adults);
    CC_CHECK(restored.settlements[0].sheep_adults == 31);
    CC_CHECK(restored.settlements[0].sheep_lambs == 9);
    CC_CHECK(restored.settlements[0].sheep_condition == 77);
    CC_CHECK(restored.settlements[0].sheep_hunger == 22);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    restored.settlements[0].sheep_adults = -1;
    CC_CHECK(!CcSimValidate(&restored, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);

    puts("Horse, cattle, and flock economy tests passed");
    return 0;
}
