#include "sim/cc_buildings.h"
#include "persistence/cc_save.h"
#include "test_support.h"

#include <sqlite3.h>
#include <string.h>

static CcSim before, after, restored;

static int32_t FireSum(const CcSettlement *place)
{
    int32_t result = 0;
    for (int32_t i = 0; i < place->building_count; ++i) result += place->buildings[i].fire_damage;
    return result;
}

static void RoundTrip(const CcSim *sim, CcSim *copy)
{
    char error[256] = {0};
    unsigned char *bytes = NULL;
    size_t length = 0;
    if (!CcSaveEncode(sim, &bytes, &length, error, sizeof(error))) {
        fprintf(stderr, "building encode: %s\n", error);
        CC_CHECK(false);
    }
    if (!CcSaveDecode(bytes, length, copy, error, sizeof(error))) {
        fprintf(stderr, "building round trip: %s\n", error);
        CC_CHECK(false);
    }
    CcSaveFreeBuffer(bytes);
}

static void SeededIdentityAndReorder(void)
{
    CcSimInit(&before, 73U);
    CcSimInit(&after, 73U);
    CC_CHECK(CcSimHash(&before) == CcSimHash(&after));
    const CcSettlement *place = &before.settlements[0];
    CC_CHECK(place->blueprint_id == CC_BLUEPRINT_THORNFORD);
    CC_CHECK(place->building_count == 12);
    CC_CHECK(CcSettlementBuilding(place, 2)->built_day == before.current_day);
    CC_CHECK(CcSettlementBuilding(place, 4)->upkeep < 50);
    CC_CHECK(CcSettlementBuilding(place, 1)->upkeep >= 60);
    CC_CHECK(CcSettlementBuilding(place, 0) == NULL);
    CC_CHECK(CcSettlementBuilding(place, 13) == NULL);
    CcBuildingState first = after.settlements[0].buildings[0];
    after.settlements[0].buildings[0] = after.settlements[0].buildings[11];
    after.settlements[0].buildings[11] = first;
    CC_CHECK(CcSimHash(&before) == CcSimHash(&after));
    CcSettlementBuildingsSetFire(&before, &before.settlements[0], 60);
    CcSettlementBuildingsSetFire(&after, &after.settlements[0], 60);
    before.settlements[0].last_fire_day = before.current_day;
    after.settlements[0].last_fire_day = after.current_day;
    CC_CHECK(CcSimHash(&before) == CcSimHash(&after));
    CC_CHECK(FireSum(&before.settlements[0]) == 720);
    for (uint32_t plot = 1; plot <= 12U; ++plot)
        CC_CHECK(CcSettlementBuildingBurn(&before.settlements[0], plot) ==
                 CcSettlementBuildingBurn(&after.settlements[0], plot));
    RoundTrip(&after, &restored);
    CC_CHECK(CcSimHash(&after) == CcSimHash(&restored));
    for (uint32_t plot = 1; plot <= 12U; ++plot)
        CC_CHECK(memcmp(CcSettlementBuilding(&after.settlements[0], plot),
                        CcSettlementBuilding(&restored.settlements[0], plot), sizeof(CcBuildingState)) == 0);
}

static void EveryFieldHasHashAndSave(void)
{
    CcSimInit(&before, 74U);
    CcSimAdvanceDays(&before, 19);
    uint64_t baseline = CcSimHash(&before);
    for (int field = 0; field < 9; ++field) {
        after = before;
        CcBuildingState *house = &after.settlements[0].buildings[0];
        switch (field) {
            case 0: house->style_seed ^= UINT32_C(0x80000000); break;
            case 1: house->health = 47; break;
            case 2: house->roof_health = 49; break;
            case 3: house->upkeep = 17; break;
            case 4: house->fire_damage = 31; break;
            case 5: house->repair_progress = 19; house->last_repair_day = 1; break;
            case 6: house->built_day -= 1; break;
            case 7: house->last_change_day = 20; break;
            case 8: house->last_repair_day = 1; break;
        }
        CC_CHECK(CcSimHash(&after) != baseline);
        RoundTrip(&after, &restored);
        CC_CHECK(CcSimHash(&after) == CcSimHash(&restored));
        CC_CHECK(memcmp(house, &restored.settlements[0].buildings[0], sizeof(*house)) == 0);
    }
}

static void FundedRepairSurvivesReturn(void)
{
    CcSimInit(&before, 75U);
    before.current_day = 13;
    CcSettlement *town = &before.settlements[0];
    town->last_fire_day = 1;
    town->fire_damage = 60; /* Also exercise existing town-level capture fixtures. */
    uint64_t unprojected = CcSimHash(&before);
    float visible[12];
    for (uint32_t plot = 1; plot <= 12U; ++plot)
        visible[plot - 1U] = CcSettlementBuildingBurn(town, plot);
    CC_CHECK(CcSimHash(&before) == unprojected);
    CcSettlementBuildingsSync(&before, town);
    for (uint32_t plot = 1; plot <= 12U; ++plot)
        CC_CHECK(visible[plot - 1U] == CcSettlementBuildingBurn(town, plot));
    town->hunger = 0;
    town->security = 75;
    town->stock[CC_GOOD_BREAD] = 500;
    town->stock[CC_GOOD_WOOD] = 12;
    town->stock[CC_GOOD_STONE] = 12;
    town->stock[CC_GOOD_TOOLS] = 12;
    after = before;
    after.settlements[0].fire_damage = 0;
    CcSettlementBuildingsSync(&after, &after.settlements[0]);
    CcSimAdvanceDays(&before, 1);
    CcSimAdvanceDays(&after, 1);
    CC_CHECK(town->fire_damage == 50);
    CC_CHECK(FireSum(town) == 600);
    CC_CHECK(town->stock[CC_GOOD_WOOD] == after.settlements[0].stock[CC_GOOD_WOOD] - 2);
    CC_CHECK(town->stock[CC_GOOD_STONE] == after.settlements[0].stock[CC_GOOD_STONE] - 1);
    CC_CHECK(town->stock[CC_GOOD_TOOLS] == after.settlements[0].stock[CC_GOOD_TOOLS] - 1);
    bool funded = false;
    for (int32_t i = 0; i < town->building_count; ++i)
        funded = funded || town->buildings[i].last_repair_day == before.current_day;
    CC_CHECK(funded);
    RoundTrip(&before, &restored);
    CC_CHECK(CcSimHash(&before) == CcSimHash(&restored));
    CcSimAdvanceDays(&before, 7);
    CcSimAdvanceDays(&restored, 7);
    CC_CHECK(CcSimHash(&before) == CcSimHash(&restored));
}

static void LegacyMigrationKeepsTownDamage(void)
{
    CcSimInit(&before, 76U);
    before.schema_version = 125U;
    before.settlements[0].fire_damage = 63;
    before.settlements[0].last_fire_day = 1;
    RoundTrip(&before, &restored);
    CC_CHECK(restored.schema_version == CC_BUILDING_SCHEMA_VERSION);
    CC_CHECK(restored.settlements[0].blueprint_id == CC_BLUEPRINT_THORNFORD);
    CC_CHECK(restored.settlements[0].fire_damage == 63);
    CC_CHECK(FireSum(&restored.settlements[0]) == 63 * 12);
    before = restored;
    RoundTrip(&before, &restored);
    CC_CHECK(CcSimHash(&before) == CcSimHash(&restored));
}

static void CorruptRowsAreRejected(void)
{
    static const char *changes[] = {
        "DELETE FROM settlement_building WHERE plot_id=1;",
        "DELETE FROM settlement_blueprint WHERE blueprint_id=1;",
        "UPDATE settlement_building SET health=101 WHERE plot_id=1;",
        "UPDATE settlement_building SET health=2.5 WHERE plot_id=1;",
        "UPDATE settlement_building SET plot_id=99 WHERE plot_id=1;",
        "UPDATE settlement_building SET settlement_id=1 WHERE plot_id=1 AND settlement_id=(SELECT min(settlement_id) FROM settlement_building);",
        "UPDATE settlement_building SET style_seed=4294967296 WHERE plot_id=1;",
        "UPDATE settlement_blueprint SET version=2 WHERE blueprint_id=1;",
        "UPDATE settlement_blueprint SET building_count=13 WHERE blueprint_id=1;",
        "UPDATE settlement_building SET repair_progress=100,fire_damage=10,last_repair_day=1 WHERE plot_id=1;",
        "UPDATE settlement_building SET last_change_day=99999999 WHERE plot_id=1;"
    };
    const char *path = "building-state-corruption.ccsave";
    char error[256];
    CcSimInit(&before, 77U);
    for (size_t i = 0; i < sizeof(changes) / sizeof(changes[0]); ++i) {
        (void)remove(path);
        CC_CHECK(CcSaveWrite(path, &before, error, sizeof(error)));
        sqlite3 *database = NULL;
        CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
        CC_CHECK(sqlite3_exec(database, changes[i], NULL, NULL, NULL) == SQLITE_OK);
        sqlite3_close(database);
        CC_CHECK(!CcSaveRead(path, &restored, error, sizeof(error)));
    }
    (void)remove(path);
}

int main(void)
{
    SeededIdentityAndReorder();
    EveryFieldHasHashAndSave();
    FundedRepairSurvivesReturn();
    LegacyMigrationKeepsTownDamage();
    CorruptRowsAreRejected();
    return 0;
}
