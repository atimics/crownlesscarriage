#include "persistence/cc_save.h"
#include "sim/cc_goods_internal.h"
#include "metagame/cc_metagame.h"
#include "test_support.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static CcSim sim, legacy, loaded;
static char error[256];

static void ClearRot(CcSim *world)
{
    world->goblins.lair_stock[CC_GOOD_ROTTEN_MEAT] = 0;
    world->goblins.lair_stock[CC_GOOD_ROTTEN_GRAIN] = 0;
    world->dragon.hoard_goods[CC_GOOD_ROTTEN_MEAT] = 0;
    world->dragon.hoard_goods[CC_GOOD_ROTTEN_GRAIN] = 0;
    for (int32_t i = 0; i < world->settlement_count; ++i) {
        world->settlements[i].stock[CC_GOOD_ROTTEN_MEAT] = 0;
        world->settlements[i].stock[CC_GOOD_ROTTEN_GRAIN] = 0;
    }
}

static void CheckDiet(void)
{
    int32_t stock[CC_GOOD_COUNT] = {0};
    stock[CC_GOOD_ROTTEN_MEAT] = 3;
    stock[CC_GOOD_ROTTEN_GRAIN] = 5;
    CC_CHECK(CcNutritionAvailable(stock, CC_NUTRITION_SCAVENGER) == 11);
    for (int32_t purpose = CC_NUTRITION_CIVILIAN; purpose <= CC_NUTRITION_ANIMAL; ++purpose) {
        int32_t copy[CC_GOOD_COUNT];
        memcpy(copy, stock, sizeof(copy));
        CC_CHECK(CcNutritionAvailable(stock, (CcNutritionPurpose)purpose) == 0);
        CC_CHECK(CcNutritionConsume(copy, (CcNutritionPurpose)purpose, 20) == 0);
        CC_CHECK(memcmp(copy, stock, sizeof(copy)) == 0);
    }
    stock[CC_GOOD_BREAD] = 100;
    CC_CHECK(CcGoodsPreferredNutritionGood(stock, CC_NUTRITION_SCAVENGER) == CC_GOOD_ROTTEN_MEAT);
    CC_CHECK(CcNutritionConsume(stock, CC_NUTRITION_SCAVENGER, 7) == 7);
    CC_CHECK(stock[CC_GOOD_ROTTEN_MEAT] == 0 && stock[CC_GOOD_ROTTEN_GRAIN] == 4);
    CC_CHECK(stock[CC_GOOD_BREAD] == 100);
    CC_CHECK(CcNutritionConsume(stock, CC_NUTRITION_SCAVENGER, 6) == 6);
    CC_CHECK(stock[CC_GOOD_ROTTEN_GRAIN] == 0 && stock[CC_GOOD_BREAD] == 99);
    stock[CC_GOOD_ROTTEN_MEAT] = INT32_MAX;
    stock[CC_GOOD_ROTTEN_GRAIN] = INT32_MAX;
    CC_CHECK(CcGoodsRotNutrition(stock) == INT32_MAX);
    CC_CHECK(CcGoodsConsumeRot(NULL, 5) == 0);
    CC_CHECK(CcGoodsConsumeRot(stock, 0) == 0);
}

static void CheckGoblinMealsAndTrade(void)
{
    CcSimInit(&sim, 42U);
    sim.current_day = 6;
    sim.goblins.tribute_cooldown_days = 1000;
    sim.dragon.hunt_cooldown_days = 1000;
    memset(sim.goblins.lair_stock, 0, sizeof(sim.goblins.lair_stock));
    sim.goblins.lair_stock[CC_GOOD_ROTTEN_GRAIN] = 20;
    legacy = sim;
    legacy.schema_version = 75U;
    int32_t members = sim.goblins.members;
    CcSimAdvanceDays(&sim, 1);
    CcSimAdvanceDays(&legacy, 1);
    CC_CHECK(sim.goblins.lair_stock[CC_GOOD_ROTTEN_GRAIN] == 18);
    CC_CHECK(sim.goblins.members == members);
    CC_CHECK(legacy.goblins.lair_stock[CC_GOOD_ROTTEN_GRAIN] == 20);
    CC_CHECK(legacy.goblins.members < members);
    sim.player.location_id = sim.goblins.lair_settlement_id;
    sim.player.cargo[CC_GOOD_ROTTEN_MEAT] = 4;
    CcCommand trade = {.kind = CC_COMMAND_GOBLIN_TRADE, .good = CC_GOOD_ROTTEN_MEAT, .amount = 4};
    CC_CHECK(CcSimApply(&sim, &trade, error, sizeof(error)));
    CC_CHECK(sim.goblins.lair_stock[CC_GOOD_ROTTEN_MEAT] == 4);
    CC_CHECK(sim.player.cargo[CC_GOOD_ROTTEN_MEAT] == 0);
    static CcMetagame text;
    CcMetagameInit(&text, 42U);
    text.sim.player.location_id = text.sim.goblins.lair_settlement_id;
    text.sim.carriage.location_id = text.sim.player.location_id;
    text.sim.player.cargo[CC_GOOD_ROTTEN_GRAIN] = 2;
    char output[2048];
    bool traded = CcMetagameExecute(&text, "goblins trade rotten-wheat 2", output, sizeof(output));
    if (!traded) fprintf(stderr, "%s\n", output);
    CC_CHECK(traded);
    CC_CHECK(text.sim.goblins.lair_stock[CC_GOOD_ROTTEN_GRAIN] == 2);
}

static void CheckRotRaid(void)
{
    CcSimInit(&sim, 42U);
    sim.current_day = 6;
    sim.dragon.hunt_cooldown_days = 1000;
    sim.goblins.tribute_cooldown_days = 0;
    memset(sim.goblins.lair_stock, 0, sizeof(sim.goblins.lair_stock));
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_BREAD] = 0;
        sim.settlements[i].stock[CC_GOOD_MEAT] = 0;
        sim.settlements[i].stock[CC_GOOD_WHEAT] = 0;
    }
    sim.settlements[0].stock[CC_GOOD_ROTTEN_GRAIN] = 1000;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblins.raid_motive == CC_GOBLIN_RAID_HUNGER);
    CC_CHECK(sim.goblins.tribute_target_id == sim.settlements[0].id);
    CcSimInit(&sim, 42U);
    sim.dragon.hunt_cooldown_days = 1000;
    CcSettlement *target = &sim.settlements[0];
    target->stock[CC_GOOD_BREAD] = 100;
    target->stock[CC_GOOD_ROTTEN_GRAIN] = 30;
    sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_OUTBOUND;
    sim.goblins.tribute_target_id = target->id;
    sim.goblins.tribute_days_remaining = 1;
    sim.goblins.raid_motive = CC_GOBLIN_RAID_HUNGER;
    int32_t hunger = target->hunger;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblins.carried_goods[CC_GOOD_ROTTEN_GRAIN] > 0);
    CC_CHECK(target->stock[CC_GOOD_ROTTEN_GRAIN] + sim.goblins.carried_goods[CC_GOOD_ROTTEN_GRAIN] == 30);
    CC_CHECK(target->stock[CC_GOOD_BREAD] == 100);
    CC_CHECK(target->hunger == hunger);
}

static void CheckDragonMeals(void)
{
    for (int32_t source = 0; source < 3; ++source) {
        CcSimInit(&sim, 42U);
        ClearRot(&sim);
        sim.dragon.body_condition = 20;
        sim.dragon.hunt_cooldown_days = 0;
        sim.goblins.tribute_cooldown_days = 1000;
        int32_t *stock = source == 0 ? sim.settlements[0].stock :
                         source == 1 ? sim.goblins.lair_stock : sim.dragon.hoard_goods;
        stock[CC_GOOD_ROTTEN_MEAT] = 3;
        stock[CC_GOOD_ROTTEN_GRAIN] = 10;
        int32_t cows = sim.settlements[0].cow_adults;
        int32_t sheep = sim.settlements[0].sheep_adults;
        int32_t fresh = sim.settlements[0].stock[CC_GOOD_BREAD];
        int32_t body = sim.dragon.body_condition;
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(stock[CC_GOOD_ROTTEN_MEAT] == 0 && stock[CC_GOOD_ROTTEN_GRAIN] == 0);
        CC_CHECK(sim.dragon.body_condition > body);
        CC_CHECK(sim.settlements[0].cow_adults == cows && sim.settlements[0].sheep_adults == sheep);
        CC_CHECK(sim.settlements[0].stock[CC_GOOD_BREAD] == fresh);
        CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_ROTTEN_MEAT) == 0);
        CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_ROTTEN_GRAIN) == 0);
        CC_CHECK(sim.dragon.hunt_cooldown_days == 42);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    }
    /* One grain gives half a ration and leads to an earlier return. */
    CcSimInit(&sim, 42U);
    ClearRot(&sim);
    sim.settlements[0].stock[CC_GOOD_ROTTEN_GRAIN] = 1;
    sim.dragon.body_condition = 20;
    sim.dragon.hunt_cooldown_days = 0;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_ROTTEN_GRAIN] == 0);
    CC_CHECK(sim.dragon.hunt_cooldown_days == 14);
}

static void CheckCrownHunts(void)
{
    CcSimInit(&sim, 42U);
    sim.goblin_politics.crown_faction = CC_GOBLIN_RED;
    sim.dragon.body_condition = 20;
    sim.dragon.hunt_cooldown_days = 0;
    sim.settlements[0].stock[CC_GOOD_ROTTEN_MEAT] = 100;
    int32_t red = sim.goblin_politics.factions[CC_GOBLIN_RED].members;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblin_politics.factions[CC_GOBLIN_RED].members == red);
    CC_CHECK(sim.goblin_politics.factions[CC_GOBLIN_PURPLE].hunted > 0);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_ROTTEN_MEAT] == 100);
}

static void CheckLegacyAndPersistence(void)
{
    CcSimInit(&legacy, 42U);
    legacy.schema_version = 75U;
    legacy.dragon.body_condition = 20;
    legacy.dragon.hunt_cooldown_days = 0;
    legacy.settlements[0].stock[CC_GOOD_ROTTEN_MEAT] = 20;
    CcSimAdvanceDays(&legacy, 1);
    CC_CHECK(legacy.settlements[0].stock[CC_GOOD_ROTTEN_MEAT] == 20);
    uint64_t hash = CcSimHash(&legacy);
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&legacy, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &loaded, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(loaded.schema_version == CC_SIM_SCHEMA_VERSION);
    loaded.schema_version = 75U;
    CC_CHECK(CcSimHash(&loaded) == hash);
    loaded.schema_version = CC_SIM_SCHEMA_VERSION;
    sim = loaded;
    CcSimAdvanceDays(&sim, 100);
    CcSimAdvanceDays(&loaded, 100);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));
}

int main(void)
{
    CheckDiet();
    CheckGoblinMealsAndTrade();
    CheckRotRaid();
    CheckDragonMeals();
    CheckCrownHunts();
    CheckLegacyAndPersistence();
    puts("Rot diets, trade, raids, three dragon food sources, stock removal and schema-75 replay passed.");
    return 0;
}
