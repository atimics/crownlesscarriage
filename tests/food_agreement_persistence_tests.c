#include "persistence/cc_save.h"
#include "sim/cc_food_relief.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>

static void Corrupt(const char *path, const char *sql)
{
    sqlite3 *db = NULL;
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    assert(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_close(db) == SQLITE_OK);
}

int main(void)
{
    CcSim sim, loaded;
    CcFoodReliefObservation observation;
    CcFoodReliefOutcome outcome;
    char error[256];
    const char *path = "/tmp/crownless-food-agreement-corrupt.ccsave";
    CcSimInit(&sim, UINT32_C(1202));
    CcId payer = sim.characters[0].id;
    CcId beneficiary = sim.characters[6].id;
    assert(CcFoodReliefObserve(&sim, payer, beneficiary, &observation, error, sizeof(error)));
    assert(CcFoodReliefPropose(&sim, &(CcFoodReliefProposal){payer, beneficiary,
        observation.place_id, 1, observation.unit_price}, &outcome, error, sizeof(error)));
    assert(CcSaveWrite(path, &sim, error, sizeof(error)));
    assert(CcSaveRead(path, &loaded, error, sizeof(error)));
    Corrupt(path, "UPDATE food_agreement SET quantity='bad' WHERE slot=0;");
    assert(!CcSaveRead(path, &loaded, error, sizeof(error)));
    remove(path);
    puts("food agreement persistence validation tests passed");
    return 0;
}
