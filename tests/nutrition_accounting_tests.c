#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>

static CcSettlement *StorageFixture(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0x5EED0001));
    sim->current_day = 6;
    sim->settlement_count = 1;
    sim->route_count = 0;
    sim->shipment_count = 0;
    sim->bandit_count = 0;
    sim->monster_count = 0;
    sim->dungeon_count = 0;
    sim->situation_count = 0;
    sim->goblins.lair_settlement_id = sim->settlements[0].id;
    sim->goblins.tribute_cooldown_days = 1000;
    sim->dragon.lair_settlement_id = sim->settlements[0].id;
    sim->hoard_raiders.cooldown_days = 1000;
    CcSettlement *place = &sim->settlements[0];
    place->service_mask = UINT32_C(1) << CC_SERVICE_INN;
    place->function = CC_SETTLEMENT_FARMING;
    place->field_yield = 0;
    place->cow_adults = place->cow_calves = 0;
    place->sheep_adults = place->sheep_lambs = 0;
    place->pony_adults = place->pony_foals = 0;
    for (int good = 0; good < CC_GOOD_COUNT; ++good) {
        place->stock[good] = 0;
        place->production[good] = 0;
        place->consumption[good] = 0;
        place->reserve_target[good] = 0;
    }
    for (int i = 0; i < sim->kingdom_count; ++i) {
        for (int j = 0; j < sim->kingdom_count; ++j) {
            sim->diplomacy[i][j] = CC_DIPLOMACY_PEACE;
        }
    }
    return place;
}

int main(void)
{
    static CcSim observed, control, daily;
    CcNutritionAccounting totals = {0}, daily_totals = {0};
    CcSettlement *place = StorageFixture(&observed);
    place->stock[CC_GOOD_BREAD] = 200;
    place->stock[CC_GOOD_WHEAT] = 1000;
    place->stock[CC_GOOD_MEAT] = 100;
    control = observed;
    CcSimAdvanceDaysWithNutritionAccounting(&observed, 1, &totals);
    CcSimAdvanceDays(&control, 1);
    CC_CHECK(memcmp(&observed, &control, sizeof(observed)) == 0);
    const CcTownNutritionAccounting *town = &totals.towns[0];
    CC_CHECK(town->settlement_id == place->id);
    CC_CHECK(town->civilian_units[CC_GOOD_BREAD] == 1);
    CC_CHECK(town->aged_units[CC_GOOD_BREAD] == 1);
    CC_CHECK(town->overflow_units[CC_GOOD_BREAD] == 186);
    CC_CHECK(town->aged_units[CC_GOOD_WHEAT] == 2);
    CC_CHECK(town->overflow_units[CC_GOOD_WHEAT] == 974);
    CC_CHECK(town->aged_units[CC_GOOD_MEAT] == 5);
    CC_CHECK(town->overflow_units[CC_GOOD_MEAT] == 93);
    CC_CHECK(town->civilian_units[CC_GOOD_WHEAT] == 0);
    CC_CHECK(town->civilian_units[CC_GOOD_MEAT] == 0);
    CC_CHECK(place->stock[CC_GOOD_BREAD] == 12);
    CC_CHECK(place->stock[CC_GOOD_WHEAT] == 24);
    CC_CHECK(place->stock[CC_GOOD_MEAT] == 2);

    place = StorageFixture(&observed);
    place->stock[CC_GOOD_BREAD] = 10;
    place->stock[CC_GOOD_WHEAT] = 10;
    place->stock[CC_GOOD_MEAT] = 1;
    memset(&totals, 0, sizeof(totals));
    CcSimAdvanceDaysWithNutritionAccounting(&observed, 1, &totals);
    for (int g = 0; g < CC_GOOD_COUNT; ++g) {
        CC_CHECK(totals.towns[0].aged_units[g] == 0);
        CC_CHECK(totals.towns[0].overflow_units[g] == 0);
    }
    CC_CHECK(totals.towns[0].civilian_units[CC_GOOD_BREAD] == 1);

    /* A shipment moves bread between holders before the weekly storage pass. */
    CcSimInit(&observed, 42U);
    observed.current_day = 11;
    observed.bandit_count = 0;
    observed.monster_count = 0;
    observed.routes[0].closed = false;
    observed.routes[0].security = 100;
    observed.routes[0].condition = 100;
    observed.shipment_count = 1;
    observed.shipments[0] = (CcShipment){
        .id = CcMakeId(CC_ENTITY_SHIPMENT, observed.next_entity_serial++),
        .origin_id = observed.routes[0].from_id,
        .destination_id = observed.routes[0].to_id,
        .final_destination_id = observed.routes[0].to_id,
        .route_id = observed.routes[0].id,
        .good = CC_GOOD_BREAD,
        .quantity = 8,
        .departure_day = 11,
        .arrival_day = 12,
        .status = CC_SHIPMENT_TRAVELLING
    };
    CcSettlement *destination = CcSimSettlementMutable(&observed, observed.routes[0].to_id);
    int32_t bread_before = destination->stock[CC_GOOD_BREAD];
    memset(&totals, 0, sizeof(totals));
    CcNutritionAccounting empty = {0};
    CcSimAdvanceDaysWithNutritionAccounting(&observed, 1, &totals);
    CC_CHECK(observed.shipments[0].status == CC_SHIPMENT_ARRIVED);
    CC_CHECK(destination->stock[CC_GOOD_BREAD] == bread_before + 8);
    CC_CHECK(memcmp(&totals, &empty, sizeof(totals)) == 0);

    /* Measured and ordinary stepping share every authoritative byte and RNG. */
    char fixture_error[256];
    CC_CHECK(CcSaveRead(CC_TEST_SOURCE_DIR "/tests/fixtures/shipped/schema-53-generator-25-nutrition.ccsave",
        &observed, fixture_error, sizeof(fixture_error)));
    observed.schema_version = 53U;
    control = observed;
    daily = observed;
    memset(&totals, 0, sizeof(totals));
    CcSimAdvanceDaysWithNutritionAccounting(&observed, 365, &totals);
    CcSimAdvanceDays(&control, 365);
    for (int day = 0; day < 365; ++day) {
        CcSimAdvanceDaysWithNutritionAccounting(&daily, 1, &daily_totals);
    }
    CC_CHECK(memcmp(&observed, &control, sizeof(observed)) == 0);
    CC_CHECK(memcmp(&observed, &daily, sizeof(observed)) == 0);
    CC_CHECK(memcmp(&totals, &daily_totals, sizeof(totals)) == 0);
    CcSimAdvanceDaysWithNutritionAccounting(&observed, 14235, &totals);
    /* Frozen with the uninstrumented b105a1b simulation. */
    CC_CHECK(CcSimHash(&observed) == UINT64_C(0x50d8f654db1ed6f8));

    CcSimInit(&observed, 42U);
    observed.schema_version = 52U;
    memset(&totals, 0, sizeof(totals));
    control = observed;
    CcSimAdvanceDaysWithNutritionAccounting(&observed, 365, &totals);
    CcSimAdvanceDays(&control, 365);
    CC_CHECK(memcmp(&observed, &control, sizeof(observed)) == 0);

    CcNutritionAccounting saved = totals;
    CcSimAdvanceDaysWithNutritionAccounting(NULL, 1, &totals);
    CcSimAdvanceDaysWithNutritionAccounting(&observed, 0, &totals);
    CcSimAdvanceDaysWithNutritionAccounting(&observed, -1, &totals);
    CC_CHECK(memcmp(&totals, &saved, sizeof(totals)) == 0);
    return 0;
}
