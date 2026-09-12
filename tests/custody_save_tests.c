#include "persistence/cc_save.h"
#include "test_support.h"
#include <sqlite3.h>
#include <string.h>

static CcSim sim, loaded, before;
static char error[256];
static const char *path = "custody-contract.ccsave";

static void RoundTrip(void)
{
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &loaded, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));
    CC_CHECK(CcCustodyHash(&sim.custody) == CcCustodyHash(&loaded.custody));
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));
}

static void Corrupt(const char *sql)
{
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, sql, NULL, NULL, NULL) == SQLITE_OK);
    sqlite3_close(database);
    before = loaded;
    CC_CHECK(!CcSaveRead(path, &loaded, error, sizeof(error)));
    CC_CHECK(memcmp(&loaded, &before, sizeof(loaded)) == 0);
}

int main(void)
{
    CcSimInit(&sim, 12345);
    RoundTrip();
    CcMoney gold = CcSimTrackedGold(&sim);
    int32_t wheat = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    sim.settlements[0].stock[CC_GOOD_WHEAT] -= 3;
    sim.player.coins -= 2;
    sim.custody.entries[0] = (CcCustodyEntry){.id = 1, .revision = 2,
        .owner_id = sim.player.id, .holder = {CC_CUSTODY_STORE, sim.settlements[0].id},
        .kind = CC_CUSTODY_CONTAINER, .quantity = 1, .capacity = 8, .condition = 95, .active = true};
    sim.custody.entries[1] = (CcCustodyEntry){.id = 2, .revision = 1,
        .owner_id = sim.player.id, .holder = {CC_CUSTODY_CONTAINER_HOLDER, 1},
        .kind = CC_CUSTODY_GOODS, .quantity = 3, .good = CC_GOOD_WHEAT, .condition = 100, .active = true};
    sim.custody.entries[2] = (CcCustodyEntry){.id = 3, .revision = 1,
        .owner_id = sim.player.id, .holder = {CC_CUSTODY_CONTAINER_HOLDER, 1},
        .kind = CC_CUSTODY_PURSE, .quantity = 2, .condition = 100, .active = true};
    sim.custody.entries[95] = (CcCustodyEntry){.id = 4, .revision = UINT64_MAX,
        .owner_id = sim.player.id, .source_id = 2, .last_event_id = UINT64_MAX,
        .holder = {CC_CUSTODY_CHARACTER, UINT64_MAX}, .kind = CC_CUSTODY_GOODS};
    sim.custody.next_id = 5;
    RoundTrip();
    CC_CHECK(CcSimTrackedGood(&loaded, CC_GOOD_WHEAT) == wheat);
    CC_CHECK(CcSimTrackedGold(&loaded) == gold);
    const char *corruptions[] = {
        "DELETE FROM custody_state;", "UPDATE custody_state SET next_id=4;",
        "UPDATE custody_state SET next_id='bad';", "INSERT INTO custody_state VALUES(2,5);",
        "DELETE FROM custody_entry WHERE slot=95;",
        "UPDATE custody_entry SET slot=96 WHERE slot=95;",
        "UPDATE custody_entry SET quantity=4294967296 WHERE slot=1;",
        "UPDATE custody_entry SET good=4294967296 WHERE slot=1;",
        "UPDATE custody_entry SET active=2 WHERE slot=1;",
        "UPDATE custody_entry SET condition=1.5 WHERE slot=1;",
        "UPDATE custody_entry SET holder_id=999 WHERE slot=0;",
        "UPDATE custody_entry SET owner_id=999 WHERE slot=0;",
        "UPDATE custody_entry SET id=1 WHERE slot=1;",
        "UPDATE custody_entry SET capacity=2 WHERE slot=0;",
        "UPDATE custody_entry SET last_event_id=1 WHERE slot=94;"
    };
    for (size_t i = 0; i < sizeof(corruptions) / sizeof(corruptions[0]); ++i) Corrupt(corruptions[i]);
    CcSimInit(&sim, 12345);
    sim.schema_version = 98;
    uint64_t legacy_hash = CcSimHash(&sim);
    sim.custody.next_id = 500;
    CC_CHECK(CcSimHash(&sim) == legacy_hash);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)));
    CC_CHECK(loaded.schema_version == 99 && loaded.custody.next_id == 1);
    CC_CHECK(CcSimValidate(&loaded, error, sizeof(error)));
    (void)remove(path);
    return 0;
}
