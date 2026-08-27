#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static int32_t CountEvents(const CcSim *sim, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

static int32_t TotalFood(const CcSim *sim)
{
    int32_t total = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        total += sim->settlements[i].stock[CC_GOOD_FOOD];
    }
    return total;
}

int main(void)
{
    char error[256];
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xd2a60ec0));
    CC_CHECK(sim.dragon.life_stage == CC_DRAGON_STAGE_CROWNED);
    CC_CHECK(sim.dragon.activity == CC_DRAGON_ACTIVITY_DORMANT);
    CC_CHECK(sim.dragon.crown_strength >= 20);
    CC_CHECK(sim.dragon.body_condition == 82);
    CC_CHECK(sim.dragon.memory_integrity == 100);

    int32_t crown_before = sim.dragon.crown_strength;
    sim.dragon.hoard = 500;
    sim.dragon.hoard_goods[CC_GOOD_GOLD] = 4;
    sim.dragon.hoard_goods[CC_GOOD_GEMS] = 2;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon.crown_strength > crown_before);

    int32_t food_before = TotalFood(&sim);
    sim.dragon.body_condition = 20;
    sim.dragon.hunt_cooldown_days = 0;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon.hunts == 1);
    CC_CHECK(TotalFood(&sim) < food_before);
    CC_CHECK(sim.dragon.retaliations == 0);
    CC_CHECK(CountEvents(&sim, CC_EVENT_DRAGON_HUNT) == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_DRAGON_RETALIATION) == 0);

    sim.player.location_id = sim.dragon.lair_settlement_id;
    sim.carriage.location_id = sim.player.location_id;
    int32_t memory_before = sim.dragon.memory_integrity;
    CcCommand steal = {
        .kind = CC_COMMAND_STEAL_DRAGON_HOARD,
        .amount = 20
    };
    CC_CHECK(CcSimApply(&sim, &steal, error, sizeof(error)));
    CC_CHECK(sim.dragon.memory_integrity < memory_before);
    CC_CHECK(sim.dragon.crown_continuity_days == 0);
    CC_CHECK(sim.dragon.activity == CC_DRAGON_ACTIVITY_RETALIATING);
    CcCommand restore = {
        .kind = CC_COMMAND_RETURN_DRAGON_TREASURE,
        .amount = 20
    };
    CC_CHECK(CcSimApply(&sim, &restore, error, sizeof(error)));
    CC_CHECK(sim.dragon.memory_integrity >= 75);
    CC_CHECK(sim.dragon.stolen_outstanding == 0);

    CcTreasure *remembered = &sim.treasures[sim.treasure_count++];
    *remembered = (CcTreasure){
        .id = CcMakeId(CC_ENTITY_TREASURE, UINT64_C(9001)),
        .maker_settlement_id = sim.settlements[0].id,
        .owner_id = sim.dragon.id,
        .location_id = sim.dragon.lair_settlement_id,
        .gold_content = 2,
        .gem_content = 2,
        .craft_work = 3,
        .appraised_value = 240,
        .created_day = 1
    };
    (void)snprintf(remembered->name, sizeof(remembered->name),
                   "The First Crown's Seal");
    memory_before = sim.dragon.memory_integrity;
    CcCommand steal_named = {
        .kind = CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE,
        .target_id = remembered->id
    };
    CC_CHECK(CcSimApply(&sim, &steal_named, error, sizeof(error)));
    CC_CHECK(sim.dragon.stolen_treasure_id == remembered->id);
    CC_CHECK(sim.dragon.stolen_outstanding == remembered->appraised_value);
    CC_CHECK(sim.player.treasure_cargo_slots == 1);
    CC_CHECK(sim.dragon.memory_integrity < memory_before);
    CC_CHECK(!CcSimApply(&sim, &restore, error, sizeof(error)));
    CcCommand return_named = {
        .kind = CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE,
        .target_id = remembered->id
    };
    CC_CHECK(CcSimApply(&sim, &return_named, error, sizeof(error)));
    CC_CHECK(sim.dragon.stolen_treasure_id == 0U);
    CC_CHECK(sim.dragon.stolen_outstanding == 0);
    CC_CHECK(sim.player.treasure_cargo_slots == 0);
    CC_CHECK(sim.dragon.memory_integrity >= 90);
    CC_CHECK(remembered->owner_id == sim.dragon.id);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));

    CcSim brood;
    CcSimInit(&brood, UINT32_C(0xb200dc0d));
    brood.dragon.hoard = 5000;
    brood.dragon.hoard_goods[CC_GOOD_GOLD] = 10;
    brood.dragon.hoard_goods[CC_GOOD_GEMS] = 10;
    brood.dragon.body_condition = 100;
    brood.dragon.memory_integrity = 100;
    brood.dragon.territory_stability = 100;
    brood.dragon.crown_continuity_days = 250 * 365;
    brood.dragon.brood_cooldown_days = 1;
    brood.goblins.devotion = 100;
    brood.goblins.lair_stock[CC_GOOD_FOOD] = 100;
    CcSimAdvanceDays(&brood, 1);
    CC_CHECK(brood.dragon.egg_count >= 1);
    CC_CHECK(brood.dragon.egg_count <= 3);
    CC_CHECK(brood.dragon.brood_days_remaining >= 7 * 365);
    CC_CHECK(brood.dragon.activity == CC_DRAGON_ACTIVITY_BROODING);
    CC_CHECK(brood.dragon.broods_laid == 1);
    CC_CHECK(CountEvents(&brood, CC_EVENT_DRAGON_BROOD) == 1);

    const char *path = "/tmp/crownless-dragon-ecology-tests.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &brood, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&brood));
    CC_CHECK(restored.dragon.egg_count == brood.dragon.egg_count);
    CC_CHECK(restored.dragon.brood_days_remaining ==
             brood.dragon.brood_days_remaining);
    CC_CHECK(remove(path) == 0);

    restored.dragon.slain = true;
    restored.dragon.slain_day = restored.current_day;
    restored.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    restored.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    restored.dragon.body_condition = 0;
    restored.dragon.crown_strength = 0;
    restored.dragon.hoard = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        restored.dragon.hoard_goods[good] = 0;
    }
    restored.dragon.egg_count = 2;
    restored.dragon.brood_days_remaining = 1;
    CcId dead_dragon_id = restored.dragon.id;
    CcSimAdvanceDays(&restored, 1);
    CC_CHECK(!restored.dragon.slain);
    CC_CHECK(restored.dragon.id != dead_dragon_id);
    CC_CHECK(restored.dragon.life_stage == CC_DRAGON_STAGE_WHELP);
    CC_CHECK(restored.dragon.egg_count == 0);
    CC_CHECK(CountEvents(&restored, CC_EVENT_DRAGON_SUCCESSOR) == 1);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));

    restored.dragon.age_days = 15 * 365 - 1;
    CcSimAdvanceDays(&restored, 1);
    CC_CHECK(restored.dragon.life_stage == CC_DRAGON_STAGE_WANDERER);

    puts("Dragon crown, hunger, brood, aftermath, and succession tests passed");
    return 0;
}
