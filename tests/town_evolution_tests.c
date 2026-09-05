#include "client/cc_local_place.h"
#include "persistence/cc_save.h"
#include "test_support.h"

#include <sqlite3.h>
#include <string.h>

static void ConditionsCanCoexist(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0x710a));
    CcSettlement *town = &sim->settlements[0];
    town->prosperity = 85;
    town->hunger = 10;
    town->security = 15;
    uint32_t state = CcSimTownConditions(sim, town->id);
    CC_CHECK((state & CC_TOWN_THRIVING) != 0U);
    CC_CHECK((state & CC_TOWN_LAWLESS) != 0U);
    CC_CHECK((state & CC_TOWN_PEACEFUL) == 0U);
    town->security = 75;
    town->prosperity = 25;
    state = CcSimTownConditions(sim, town->id);
    CC_CHECK((state & CC_TOWN_PEACEFUL) != 0U);
    CC_CHECK((state & CC_TOWN_THRIVING) == 0U);
    town->fire_damage = 60;
    town->last_fire_day = sim->current_day;
    town->stock[CC_GOOD_WOOD] = 12;
    town->stock[CC_GOOD_STONE] = 6;
    town->stock[CC_GOOD_TOOLS] = 6;
    state = CcSimTownConditions(sim, town->id);
    CC_CHECK((state & CC_TOWN_BURNT) != 0U);
    CC_CHECK((state & CC_TOWN_REBUILDING) != 0U);
    CC_CHECK((state & CC_TOWN_PEACEFUL) != 0U);
    char description[96];
    CcLocalTownConditionText(state, description, sizeof(description));
    CC_CHECK(strcmp(description, "BURNT / REBUILDING / PEACEFUL") == 0);
    town->hunger = 80;
    state = CcSimTownConditions(sim, town->id);
    CC_CHECK((state & CC_TOWN_HUNGRY) != 0U);
    CC_CHECK((state & CC_TOWN_REBUILDING) == 0U);
    town->population = 0;
    CC_CHECK(CcSimTownConditions(sim, town->id) ==
             (CC_TOWN_BURNT | CC_TOWN_ABANDONED));
}

static void RepairsUseGoodsAndSurviveSave(CcSim *sim, CcSim *restored)
{
    CcSimInit(sim, UINT32_C(0x710b));
    sim->current_day = 13;
    CcSettlement *town = &sim->settlements[0];
    town->fire_damage = 60;
    town->last_fire_day = 1;
    town->hunger = 0;
    town->security = 75;
    town->stock[CC_GOOD_BREAD] = 500;
    town->stock[CC_GOOD_WOOD] = 12;
    town->stock[CC_GOOD_STONE] = 12;
    town->stock[CC_GOOD_TOOLS] = 12;
    *restored = *sim;
    restored->settlements[0].fire_damage = 0;
    CcSimAdvanceDays(sim, 1);
    CcSimAdvanceDays(restored, 1);
    CC_CHECK(town->fire_damage == 50);
    CC_CHECK(town->stock[CC_GOOD_WOOD] == restored->settlements[0].stock[CC_GOOD_WOOD] - 2);
    CC_CHECK(town->stock[CC_GOOD_STONE] == restored->settlements[0].stock[CC_GOOD_STONE] - 1);
    CC_CHECK(town->stock[CC_GOOD_TOOLS] == restored->settlements[0].stock[CC_GOOD_TOOLS] - 1);
    CC_CHECK(town->last_fire_day == 1);
    char error[256];
    const char *path = "town-evolution-test.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(sim) == CcSimHash(restored));
    CcSimAdvanceDays(sim, 7);
    CcSimAdvanceDays(restored, 7);
    CC_CHECK(CcSimHash(sim) == CcSimHash(restored));
    CC_CHECK(town->fire_damage == 40);
    for (int32_t week = 0; week < 4; ++week) {
        town->hunger = 0;
        town->security = 75;
        town->stock[CC_GOOD_BREAD] = 500;
        town->stock[CC_GOOD_WOOD] = 30;
        town->stock[CC_GOOD_STONE] = 30;
        town->stock[CC_GOOD_TOOLS] = 30;
        CcSimAdvanceDays(sim, 7);
    }
    CC_CHECK(town->fire_damage == 0);
    CC_CHECK(town->last_fire_day == 1);
    CC_CHECK((CcSimTownConditions(sim, town->id) & CC_TOWN_BURNT) == 0U);

    /* A damaged snapshot needs every town row to restore the same history. */
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "DELETE FROM town_recovery WHERE slot=0;",
                          NULL, NULL, NULL) == SQLITE_OK);
    sqlite3_close(database);
    uint64_t untouched_hash = CcSimHash(restored);
    CC_CHECK(!CcSaveRead(path, restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(restored) == untouched_hash);
    CC_CHECK(remove(path) == 0);
}

static void SuppliesAndSafetyGateRepairs(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0x710c));
    sim->current_day = 13;
    CcSettlement *town = &sim->settlements[0];
    town->fire_damage = 60;
    town->last_fire_day = 1;
    town->stock[CC_GOOD_BREAD] = 500;
    town->stock[CC_GOOD_WOOD] = 12;
    town->stock[CC_GOOD_STONE] = 12;
    town->stock[CC_GOOD_TOOLS] = 12;
    town->hunger = 0;
    town->security = 0;
    CC_CHECK(!CcSettlementCanRepairFire(town));
    CcSimAdvanceDays(sim, 1);
    CC_CHECK(town->fire_damage == 60);
    town->security = 75;
    town->stock[CC_GOOD_WOOD] = 0;
    CC_CHECK(!CcSettlementCanRepairFire(town));
    town->stock[CC_GOOD_WOOD] = 12;
    CC_CHECK(CcSettlementCanRepairFire(town));
    town->hunger = 40;
    CC_CHECK(!CcSettlementCanRepairFire(town));
    town->hunger = 0;
    town->last_fire_day = sim->current_day + 1;
    char error[256];
    CC_CHECK(!CcSimValidate(sim, error, sizeof(error)));

    CcSimInit(sim, UINT32_C(0x710c));
    sim->current_day = 13;
    town = &sim->settlements[0];
    town->fire_damage = 60;
    town->last_fire_day = 13;
    town->security = 75;
    town->hunger = 0;
    town->stock[CC_GOOD_BREAD] = 500;
    town->stock[CC_GOOD_WOOD] = 30;
    town->stock[CC_GOOD_STONE] = 30;
    town->stock[CC_GOOD_TOOLS] = 30;
    CcSimAdvanceDays(sim, 1);
    CC_CHECK(town->fire_damage == 60);
    CcSimAdvanceDays(sim, 7);
    CC_CHECK(town->fire_damage == 50);
}

static void LegacyHistoryStartsClean(CcSim *sim, CcSim *restored)
{
    CcSimInit(sim, UINT32_C(0x710d));
    sim->schema_version = 44U;
    char error[256];
    const char *path = "town-evolution-legacy-test.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, restored, error, sizeof(error)));
    CC_CHECK(restored->settlements[0].fire_damage == 0);
    CC_CHECK(restored->settlements[0].last_fire_day == 0);
    CC_CHECK(CcSimValidate(restored, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);
}

int main(void)
{
    CcSim *sim = calloc(1, sizeof(*sim));
    CcSim *restored = calloc(1, sizeof(*restored));
    CC_CHECK(sim != NULL && restored != NULL);
    ConditionsCanCoexist(sim);
    RepairsUseGoodsAndSurviveSave(sim, restored);
    SuppliesAndSafetyGateRepairs(sim);
    LegacyHistoryStartsClean(sim, restored);
    free(restored);
    free(sim);
    puts("Town conditions, repair costs, fire history, and save restoration passed");
    return 0;
}
