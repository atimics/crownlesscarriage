#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static uint32_t Service(CcServiceKind service)
{
    return UINT32_C(1) << (uint32_t)service;
}

static CcSettlement *IsolatedSettlement(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0x9a41e001));
    sim->settlement_count = 1;
    sim->route_count = 0;
    sim->shipment_count = 0;
    sim->bandit_count = 0;
    sim->monster_count = 0;
    sim->dungeon_count = 0;
    sim->situation_count = 0;
    sim->goblins.tribute_cooldown_days = 1000;
    sim->hoard_raiders.cooldown_days = 1000;
    CcSettlement *place = &sim->settlements[0];
    place->service_mask = Service(CC_SERVICE_INN);
    place->cow_adults = 0;
    place->cow_calves = 0;
    place->field_yield = 100;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        place->stock[good] = 0;
        place->reserve_target[good] = 0;
        place->production[good] = 0;
        place->consumption[good] = 0;
        place->price[good] = CcGoodDefinitionFor((CcGood)good)->base_price;
    }
    return place;
}

int main(void)
{
    CC_CHECK(strcmp(CcGoodName(CC_GOOD_BREAD), "Bread") == 0);
    CC_CHECK(CcGoodNutritionValue(
        CC_GOOD_BREAD, CC_NUTRITION_CIVILIAN) == 2);
    CC_CHECK(CcGoodNutritionValue(
        CC_GOOD_MEAT, CC_NUTRITION_TRAVEL) == 2);
    CC_CHECK(CcGoodNutritionValue(
        CC_GOOD_WHEAT, CC_NUTRITION_CIVILIAN) == 1);
    CC_CHECK(CcGoodNutritionValue(
        CC_GOOD_WHEAT, CC_NUTRITION_TRAVEL) == 0);
    CC_CHECK(CcGoodNutritionValue(
        CC_GOOD_WHEAT, CC_NUTRITION_ANIMAL) == 2);

    int32_t pantry[CC_GOOD_COUNT] = {0};
    pantry[CC_GOOD_BREAD] = 1;
    pantry[CC_GOOD_MEAT] = 1;
    pantry[CC_GOOD_WHEAT] = 4;
    CC_CHECK(CcNutritionAvailable(
        pantry, CC_NUTRITION_CIVILIAN) == 8);
    CC_CHECK(CcNutritionConsume(
        pantry, CC_NUTRITION_CIVILIAN, 5) == 5);
    CC_CHECK(pantry[CC_GOOD_BREAD] == 0);
    CC_CHECK(pantry[CC_GOOD_MEAT] == 0);
    CC_CHECK(pantry[CC_GOOD_WHEAT] == 3);

    int32_t provisions[CC_GOOD_COUNT] = {0};
    provisions[CC_GOOD_WHEAT] = 8;
    CC_CHECK(CcNutritionAvailable(
        provisions, CC_NUTRITION_TRAVEL) == 0);
    CC_CHECK(CcNutritionAvailable(
        provisions, CC_NUTRITION_ANIMAL) == 16);

    CcSim farm;
    CcSettlement *place = IsolatedSettlement(&farm);
    place->service_mask |= Service(CC_SERVICE_FARM);
    place->production[CC_GOOD_WHEAT] = 8;
    place->stock[CC_GOOD_TOOLS] = 1;
    place->stock[CC_GOOD_BREAD] = 4;
    CcSimAdvanceDays(&farm, 7);
    CC_CHECK(place->stock[CC_GOOD_WHEAT] > 0);
    CC_CHECK(place->stock[CC_GOOD_BREAD] < 4);

    CcSim bakery;
    place = IsolatedSettlement(&bakery);
    place->service_mask |= Service(CC_SERVICE_BAKERY);
    place->production[CC_GOOD_BREAD] = 4;
    place->stock[CC_GOOD_WHEAT] = 10;
    CcSimAdvanceDays(&bakery, 7);
    CC_CHECK(place->stock[CC_GOOD_WHEAT] == 6);
    CC_CHECK(place->stock[CC_GOOD_BREAD] == 3);

    CcSim migration;
    CcSimInit(&migration, UINT32_C(0x9a41e002));
    CcSettlement *farmstead = &migration.settlements[0];
    CcSettlement *market = &migration.settlements[1];
    int32_t old_bread = market->stock[CC_GOOD_BREAD];
    farmstead->production[CC_GOOD_BREAD] = 23;
    farmstead->production[CC_GOOD_WHEAT] = 0;
    market->service_mask &= ~Service(CC_SERVICE_BAKERY);
    CcSimUpgradeGrainEconomy(&migration);
    CC_CHECK(farmstead->production[CC_GOOD_BREAD] == 0);
    CC_CHECK(farmstead->production[CC_GOOD_WHEAT] == 23);
    CC_CHECK(market->stock[CC_GOOD_BREAD] == old_bread);
    CC_CHECK(CcSettlementHasService(market, CC_SERVICE_BAKERY));
    CC_CHECK(market->production[CC_GOOD_BREAD] == 18);

    CC_CHECK(CcSimHash(&migration) != CcSimHash(&farm));
    printf("grain economy tests passed\n");
    return 0;
}
