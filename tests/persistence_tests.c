#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <inttypes.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static void RequireSqlite(int result, sqlite3 *database, const char *context)
{
    if (result == SQLITE_OK) return;
    (void)fprintf(stderr, "%s: %s\n", context,
                  database != NULL ? sqlite3_errmsg(database) : "SQLite error");
    if (database != NULL) sqlite3_close(database);
    exit(EXIT_FAILURE);
}

static void RemoveDatabase(const char *path)
{
    char sidecar[384];
    (void)remove(path);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-wal", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-shm", path);
    (void)remove(sidecar);
}

static int64_t ReadSqliteInteger(const char *path, const char *sql)
{
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database, SQLITE_OPEN_READONLY, NULL),
                  database, "could not open journal fixture");
    sqlite3_stmt *statement = NULL;
    RequireSqlite(sqlite3_prepare_v2(database, sql, -1, &statement, NULL),
                  database, "could not prepare journal query");
    CC_CHECK(sqlite3_step(statement) == SQLITE_ROW);
    int64_t value = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return value;
}

static void ExecuteFixtureSql(sqlite3 *database, const char *sql,
                              const char *context)
{
    char *sqlite_error = NULL;
    int result = sqlite3_exec(database, sql, NULL, NULL, &sqlite_error);
    if (result != SQLITE_OK) {
        (void)fprintf(stderr, "%s: %s\n", context,
                      sqlite_error != NULL ? sqlite_error :
                      sqlite3_errmsg(database));
        sqlite3_free(sqlite_error);
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    }
}

static void CheckReadDoesNotCreateOrRelabel(char *error,
                                             size_t error_capacity)
{
    const char *missing = "persistence-missing-test.ccsave";
    RemoveDatabase(missing);
    CcSim untouched;
    CcSimInit(&untouched, UINT32_C(0x51a7e));
    uint64_t untouched_hash = CcSimHash(&untouched);
    CC_CHECK(!CcSaveRead(missing, &untouched, error, error_capacity));
    CC_CHECK(CcSimHash(&untouched) == untouched_hash);
    FILE *created = fopen(missing, "rb");
    CC_CHECK(created == NULL);

    const char *newer = "persistence-newer-test.ccsave";
    RemoveDatabase(newer);
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(newer, &database,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                  NULL),
                  database, "could not create newer fixture");
    ExecuteFixtureSql(database,
                      "PRAGMA application_id=1128481362;"
                      "PRAGMA user_version=99;",
                      "could not label newer fixture");
    sqlite3_close(database);
    CC_CHECK(!CcSaveRead(newer, &untouched, error, error_capacity));
    CC_CHECK(strstr(error, "newer") != NULL);
    CC_CHECK(CcSimHash(&untouched) == untouched_hash);
    CC_CHECK(ReadSqliteInteger(newer, "PRAGMA application_id;") ==
             INT64_C(1128481362));
    CC_CHECK(ReadSqliteInteger(newer, "PRAGMA user_version;") == 99);
    CC_CHECK(ReadSqliteInteger(
                 newer,
                 "SELECT COUNT(*) FROM sqlite_master WHERE name='meta';") == 0);
    RemoveDatabase(newer);

    const char *malformed = "persistence-malformed-test.ccsave";
    RemoveDatabase(malformed);
    database = NULL;
    RequireSqlite(sqlite3_open_v2(malformed, &database,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                  NULL),
                  database, "could not create malformed fixture");
    ExecuteFixtureSql(database,
                      "PRAGMA user_version=7;"
                      "CREATE TABLE unrelated(value INTEGER);"
                      "INSERT INTO unrelated VALUES(41);",
                      "could not create malformed fixture contents");
    sqlite3_close(database);
    CcJournal *journal = CcJournalResume(malformed, &untouched,
                                         error, error_capacity);
    CC_CHECK(journal == NULL);
    CC_CHECK(!CcSaveWrite(malformed, &untouched, error, error_capacity));
    CC_CHECK(strstr(error, "not a Crownless campaign") != NULL);
    CC_CHECK(CcSimHash(&untouched) == untouched_hash);
    CC_CHECK(ReadSqliteInteger(malformed, "PRAGMA application_id;") == 0);
    CC_CHECK(ReadSqliteInteger(malformed, "PRAGMA user_version;") == 7);
    CC_CHECK(ReadSqliteInteger(
                 malformed,
                 "SELECT COUNT(*) FROM sqlite_master WHERE name='meta';") == 0);
    CC_CHECK(ReadSqliteInteger(
                 malformed,
                 "SELECT value FROM unrelated;") == 41);
    RemoveDatabase(malformed);

    const char *oversized = "persistence-oversized-test.ccsave";
    RemoveDatabase(oversized);
    database = NULL;
    RequireSqlite(sqlite3_open_v2(oversized, &database,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                  NULL),
                  database, "could not create oversized fixture");
    ExecuteFixtureSql(database,
                      "PRAGMA application_id=1128481362;"
                      "CREATE TABLE payload(value BLOB);"
                      "INSERT INTO payload VALUES(zeroblob(16777216));",
                      "could not create oversized fixture contents");
    sqlite3_close(database);
    CC_CHECK(!CcSaveRead(oversized, &untouched, error, error_capacity));
    CC_CHECK(strstr(error, "too large") != NULL);
    CC_CHECK(CcSimHash(&untouched) == untouched_hash);
    RemoveDatabase(oversized);
}

static void CheckJournalOwnership(char *error, size_t error_capacity)
{
    const char *path = "persistence-journal-ownership-test.ccsave";
    RemoveDatabase(path);
    CcSim original;
    CcSimInit(&original, UINT32_C(0xa11ce001));
    CcJournal *first = CcJournalStart(path, &original,
                                      error, error_capacity);
    CC_CHECK(first != NULL);
    uint64_t original_hash = CcSimHash(&original);

    CcJournal *accidental = CcJournalStart(path, &original,
                                           error, error_capacity);
    CC_CHECK(accidental == NULL);
    CcSim preserved;
    CC_CHECK(CcSaveRead(path, &preserved, error, error_capacity));
    CC_CHECK(CcSimHash(&preserved) == original_hash);

    CcSim replacement;
    CcSimInit(&replacement, UINT32_C(0xa11ce002));
    uint64_t replacement_hash = CcSimHash(&replacement);
    CcJournal *second = CcJournalRestart(path, &replacement,
                                         error, error_capacity);
    CC_CHECK(second != NULL);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM journal_epoch;") == 1);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM action_journal;") == 0);

    CC_CHECK(!CcJournalAdvanceDays(first, &original, 1,
                                   error, error_capacity));
    CC_CHECK(strstr(error, "new campaign epoch") != NULL);
    CC_CHECK(CcSimHash(&original) == original_hash);
    CcJournalAbandon(&first);
    CC_CHECK(first == NULL);
    CC_CHECK(CcJournalClose(&second, &replacement,
                            error, error_capacity));
    CC_CHECK(CcSaveRead(path, &preserved, error, error_capacity));
    CC_CHECK(CcSimHash(&preserved) == replacement_hash);
    RemoveDatabase(path);
}

static void CheckForgedExtremeStateRejected(char *error,
                                            size_t error_capacity)
{
    const char *path = "persistence-extreme-state-test.ccsave";
    RemoveDatabase(path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xe87e0e));
    CC_CHECK(CcSaveWrite(path, &sim, error, error_capacity));

    CcSim forged = sim;
    forged.settlements[0].stock[CC_GOOD_FOOD] = INT32_MAX;
    char forged_hash[24];
    (void)snprintf(forged_hash, sizeof(forged_hash), "%016" PRIx64,
                   CcSimHash(&forged));
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database, SQLITE_OPEN_READWRITE,
                                  NULL),
                  database, "could not open extreme-state fixture");
    char *sql = sqlite3_mprintf(
        "UPDATE settlement SET food_stock=%d WHERE slot=0;"
        "UPDATE meta SET state_hash=%Q WHERE id=1;",
        INT32_MAX, forged_hash);
    CC_CHECK(sql != NULL);
    ExecuteFixtureSql(database, sql,
                      "could not forge extreme-state fixture");
    sqlite3_free(sql);
    sqlite3_close(database);

    CcSim restored;
    CC_CHECK(!CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(strstr(error, "Market accounting") != NULL);
    RemoveDatabase(path);
}

static void CheckMalformedTextRejected(char *error, size_t error_capacity)
{
    const char *path = "persistence-malformed-text-test.ccsave";
    RemoveDatabase(path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x7e870bad));
    CC_CHECK(CcSaveWrite(path, &sim, error, error_capacity));

    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database, SQLITE_OPEN_READWRITE,
                                  NULL),
                  database, "could not open malformed-text fixture");
    ExecuteFixtureSql(
        database,
        "CREATE TABLE kingdom_copy AS SELECT * FROM kingdom;"
        "DROP TABLE kingdom;"
        "ALTER TABLE kingdom_copy RENAME TO kingdom;"
        "UPDATE kingdom SET name=NULL WHERE slot=0;",
        "could not forge malformed-text fixture");
    sqlite3_close(database);

    CcSim restored;
    CC_CHECK(!CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(strstr(error, "kingdom name text is invalid") != NULL);
    RemoveDatabase(path);
}

static void CheckForgedIdentityStateRejected(char *error,
                                             size_t error_capacity)
{
    const char *path = "persistence-forged-identity-test.ccsave";
    RemoveDatabase(path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x1de1717e));
    CC_CHECK(CcSaveWrite(path, &sim, error, error_capacity));

    CcSim forged = sim;
    const CcEvent *event = CcSimRecentEvent(&forged, 0);
    CC_CHECK(event != NULL);
    forged.next_entity_serial =
        event->id & UINT64_C(0x00ffffffffffffff);
    char forged_hash[24];
    (void)snprintf(forged_hash, sizeof(forged_hash), "%016" PRIx64,
                   CcSimHash(&forged));

    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database, SQLITE_OPEN_READWRITE,
                                  NULL),
                  database, "could not open forged-identity fixture");
    char *sql = sqlite3_mprintf(
        "UPDATE meta SET next_entity_serial=%llu,state_hash=%Q WHERE id=1;",
        (unsigned long long)forged.next_entity_serial, forged_hash);
    CC_CHECK(sql != NULL);
    ExecuteFixtureSql(database, sql,
                      "could not forge identity fixture");
    sqlite3_free(sql);
    sqlite3_close(database);

    CcSim restored;
    CC_CHECK(!CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(strstr(error, "identity counter") != NULL);
    RemoveDatabase(path);
}

static void AddLegacyJournalSuffix(const char *path,
                                   const CcSim *before,
                                   const CcSim *after)
{
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open legacy journal fixture");
    char pre_hash[24];
    char post_hash[24];
    (void)snprintf(pre_hash, sizeof(pre_hash), "%016" PRIx64,
                   CcSimHash(before));
    (void)snprintf(post_hash, sizeof(post_hash), "%016" PRIx64,
                   CcSimHash(after));
    char *sql = sqlite3_mprintf(
        "BEGIN IMMEDIATE;"
        "INSERT INTO journal_epoch "
        "(record_version,world_seed,initial_state_hash,created_tick) "
        "VALUES(1,%u,%Q,%llu);"
        "INSERT INTO action_journal "
        "(generation,ordinal,record_version,operation_kind,command_kind,"
        "target_id,good,amount,dungeon_state,step_count,sim_schema_version,"
        "generator_version,pre_state_hash,post_state_hash,committed_tick) "
        "VALUES(last_insert_rowid(),1,1,2,0,0,0,0,0,1,10,10,%Q,%Q,%llu);"
        "UPDATE meta SET journal_generation="
        "(SELECT MAX(generation) FROM journal_epoch),"
        "journal_cursor=0 WHERE id=1;"
        "PRAGMA user_version=10;"
        "COMMIT;",
        before->world_seed, pre_hash,
        (unsigned long long)before->clock.tick,
        pre_hash, post_hash,
        (unsigned long long)after->clock.tick);
    CC_CHECK(sql != NULL);
    ExecuteFixtureSql(database, sql,
                      "could not create legacy journal suffix");
    sqlite3_free(sql);
    sqlite3_close(database);
}

static void CheckLegacyJournalMigration(char *error,
                                        size_t error_capacity)
{
    const char *path = "persistence-legacy-journal-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac11));
    legacy.schema_version = 10U;
    legacy.generator_version = 10U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim suffix = legacy;
    CcSimAdvanceDays(&suffix, 1);
    AddLegacyJournalSuffix(path, &legacy, &suffix);
    int64_t legacy_generation = ReadSqliteInteger(
        path, "SELECT journal_generation FROM meta WHERE id=1;");

    CcSim read_only_result;
    CC_CHECK(CcSaveRead(path, &read_only_result, error, error_capacity));
    CC_CHECK(read_only_result.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(read_only_result.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(read_only_result.current_day == suffix.current_day);
    uint64_t migrated_hash = CcSimHash(&read_only_result);
    CC_CHECK(ReadSqliteInteger(path, "PRAGMA user_version;") == 10);

    CcSim resumed;
    CcJournal *journal = CcJournalResume(path, &resumed,
                                         error, error_capacity);
    CC_CHECK(journal != NULL);
    CC_CHECK(CcSimHash(&resumed) == migrated_hash);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT journal_generation FROM meta WHERE id=1;") !=
             legacy_generation);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT journal_cursor FROM meta WHERE id=1;") == 0);
    CC_CHECK(ReadSqliteInteger(path, "PRAGMA user_version;") == 18);
    CC_CHECK(CcJournalAdvanceDays(journal, &resumed, 2,
                                  error, error_capacity));
    uint64_t expected_hash = CcSimHash(&resumed);
    CC_CHECK(CcJournalClose(&journal, &resumed,
                            error, error_capacity));
    CC_CHECK(CcSaveRead(path, &resumed, error, error_capacity));
    CC_CHECK(CcSimHash(&resumed) == expected_hash);
    RemoveDatabase(path);
}

static void CheckSchema4Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v4-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac5));
    CcSimAdvanceDays(&legacy, 5);
    legacy.schema_version = 4U;

    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.current_day == legacy.current_day);
    CC_CHECK(CcIdKind(restored.goblins.id) == CC_ENTITY_GOBLIN_CULT);
    CC_CHECK(CcIdKind(restored.dragon.id) == CC_ENTITY_DRAGON);
    CC_CHECK(restored.dragon.hoard == 30);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    uint64_t migrated_hash = CcSimHash(&restored);
    CC_CHECK(CcSaveWrite(path, &restored, error, error_capacity));
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(CcSimHash(&restored) == migrated_hash);
    RemoveDatabase(path);
}

static void CheckSchema5Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v5-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac6));
    CcSimAdvanceDays(&legacy, 19);
    legacy.schema_version = 5U;
    legacy.generator_version = 5U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.current_day == legacy.current_day);
    CC_CHECK(CcIdKind(restored.goblins.id) == CC_ENTITY_GOBLIN_CULT);
    CC_CHECK(CcIdKind(restored.dragon.id) == CC_ENTITY_DRAGON);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema6Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v6-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac7));
    CcSimAdvanceDays(&legacy, 17);
    legacy.schema_version = 6U;
    legacy.generator_version = 6U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.current_day == legacy.current_day);
    CC_CHECK(CcIdKind(restored.hoard_raiders.id) ==
             CC_ENTITY_HOARD_RAIDERS);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema8Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v8-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac8));
    legacy.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    legacy.goblins.tribute_target_id = 0U;
    legacy.goblins.tribute_event_id = 0U;
    legacy.goblins.carried_tribute = 0;
    legacy.goblins.tribute_days_remaining = 0;
    legacy.schema_version = 8U;
    legacy.generator_version = 8U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.settlements[0].field_yield > 0);
    CC_CHECK(restored.settlements[3].iron_deposit > 0);
    CC_CHECK(restored.settlements[2].stock[CC_GOOD_WEAPONS] > 0);
    CC_CHECK(restored.goblins.lair_stock[CC_GOOD_FOOD] > 0);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckDiplomacyPersistence(char *error, size_t error_capacity)
{
    const char *path = "persistence-diplomacy-test.ccsave";
    RemoveDatabase(path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71e12));
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        sim.dragon.hoard += sim.kingdoms[i].treasury;
        sim.kingdoms[i].treasury = 0;
    }
    sim.dragon_campaign.attempts = 3;
    sim.dragon_campaign.victories = 1;
    sim.dragon_campaign.defeats = 2;
    sim.dragon_campaign.cooldown_days = 123;
    CcSimAdvanceDays(&sim, 27);
    CC_CHECK(sim.courier_count > 0);
    CC_CHECK(sim.couriers[0].status == CC_COURIER_WAITING);
    CC_CHECK(CcSaveWrite(path, &sim, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    CC_CHECK(restored.courier_count == sim.courier_count);
    CC_CHECK(restored.couriers[0].id == sim.couriers[0].id);
    CC_CHECK(restored.couriers[0].reliability ==
             sim.couriers[0].reliability);
    CC_CHECK(restored.diplomacy[0][1] == sim.diplomacy[0][1]);
    CC_CHECK(restored.dragon_campaign.attempts == 3);
    CC_CHECK(restored.dragon_campaign.victories == 1);
    CC_CHECK(restored.dragon_campaign.defeats == 2);
    RemoveDatabase(path);
}

static void CheckSchema10Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v10-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac10));
    legacy.schema_version = 10U;
    legacy.generator_version = 10U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    for (int32_t first = 0; first < restored.kingdom_count; ++first) {
        for (int32_t second = first + 1;
             second < restored.kingdom_count; ++second) {
            CC_CHECK(CcSimKingdomsAtWar(
                &restored, restored.kingdoms[first].id,
                restored.kingdoms[second].id));
        }
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema11Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v11-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac11));
    legacy.map_count = CC_MAX_ROUTES;
    legacy.player.map_catalogue_mask = 0U;
    legacy.player.map_archive_mask = 0U;
    legacy.dragon.life_stage = CC_DRAGON_STAGE_EGG;
    legacy.dragon.activity = CC_DRAGON_ACTIVITY_DORMANT;
    legacy.dragon.age_days = 0;
    legacy.dragon.body_condition = 0;
    legacy.dragon.crown_strength = 0;
    legacy.dragon.memory_integrity = 0;
    legacy.dragon.territory_stability = 0;
    legacy.dragon.regional_influence = 0;
    legacy.dragon.crown_continuity_days = 0;
    legacy.dragon.hunt_cooldown_days = 0;
    legacy.dragon.brood_cooldown_days = 0;
    legacy.schema_version = 11U;
    legacy.generator_version = 11U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.map_count == CC_MAP_COLLECTION_COUNT);
    CC_CHECK(CcPlayerMapCollectionCount(&restored) == 1);
    CC_CHECK(strcmp(restored.maps[CC_MAP_CROWNLESS_ATLAS].name,
                    CC_CROWNLESS_ATLAS_MAP_NAME) == 0);
    CC_CHECK(restored.dragon.life_stage == CC_DRAGON_STAGE_CROWNED);
    CC_CHECK(restored.dragon.age_days > 0);
    CC_CHECK(restored.dragon.crown_strength > 0);
    CC_CHECK(restored.dragon.memory_integrity == 100);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema12Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v12-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac12));
    legacy.map_count = CC_MAX_ROUTES;
    legacy.player.map_catalogue_mask = 0U;
    legacy.player.map_archive_mask = 0U;
    int32_t dragon_age = legacy.dragon.age_days;
    legacy.schema_version = 12U;
    legacy.generator_version = 12U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.map_count == CC_MAP_COLLECTION_COUNT);
    CC_CHECK(CcPlayerMapCollectionCount(&restored) == 1);
    CC_CHECK(restored.dragon.age_days == dragon_age);
    CC_CHECK(restored.dragon.memory_integrity == 100);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema13Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v13-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac13));
    legacy.schema_version = 13U;
    legacy.generator_version = 13U;
    legacy.goblins.cohesion = 0;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(CcIdKind(restored.horse_team[0].id) == CC_ENTITY_HORSE);
    CC_CHECK(CcIdKind(restored.horse_team[1].id) == CC_ENTITY_HORSE);
    CC_CHECK(restored.settlements[0].cow_adults > 0);
    CC_CHECK(restored.goblins.cohesion == 60);
    CC_CHECK(restored.goblins.dragon_seed_phase ==
             CC_GOBLIN_DRAGON_SEED_NONE);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema14Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v14-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac14));
    legacy.schema_version = 14U;
    legacy.generator_version = 14U;
    legacy.goblins.cohesion = 0;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.horse_team[0].sex == CC_HORSE_STALLION);
    CC_CHECK(restored.horse_team[1].sex == CC_HORSE_MARE);
    CC_CHECK(restored.horse_team[0].training == 100);
    CC_CHECK(restored.horse_team[0].strength > 0);
    CC_CHECK(restored.goblins.cohesion == 60);
    CC_CHECK(restored.goblins.dragon_seed_phase ==
             CC_GOBLIN_DRAGON_SEED_NONE);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema15Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v15-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac15));
    legacy.schema_version = 15U;
    legacy.generator_version = 15U;
    legacy.goblins.cohesion = 0;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.goblins.cohesion == 60);
    CC_CHECK(restored.goblins.dragon_seed_phase ==
             CC_GOBLIN_DRAGON_SEED_NONE);
    CC_CHECK(restored.stable_horse_count == legacy.stable_horse_count);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema16Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v16-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac16));
    legacy.schema_version = 16U;
    legacy.generator_version = 15U;
    legacy.character_count = 0;
    (void)memset(legacy.characters, 0, sizeof(legacy.characters));
    for (int32_t i = 0; i < legacy.situation_count; ++i) {
        legacy.situations[i].sponsor_character_id = 0U;
        legacy.situations[i].affected_character_id = 0U;
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.character_count > 0);
    for (int32_t i = 0; i < restored.situation_count; ++i) {
        CC_CHECK(CcSimSituationAffectedCharacter(
            &restored, &restored.situations[i]) != NULL);
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema17Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v17-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac17));
    legacy.map_count = 12;
    legacy.maps[CC_MAP_DRAGON_HOARD] = (CcMap){0};
    legacy.goblins.cohesion = 47;
    legacy.goblins.expeditions_intercepted = 3;
    legacy.schema_version = 17U;
    legacy.generator_version = 16U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open schema 17 fixture");
    char *sqlite_error = NULL;
    int result = sqlite3_exec(
        database,
        "ALTER TABLE runtime_state DROP COLUMN journey_pace;"
        "ALTER TABLE runtime_state DROP COLUMN ambush_warned;",
        NULL, NULL, &sqlite_error);
    if (result != SQLITE_OK) {
        (void)fprintf(stderr, "could not create schema 17 fixture: %s\n",
                      sqlite_error != NULL ? sqlite_error :
                      sqlite3_errmsg(database));
        sqlite3_free(sqlite_error);
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    }
    sqlite3_close(database);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.map_count == CC_MAP_COLLECTION_COUNT);
    CC_CHECK(strcmp(restored.maps[CC_MAP_DRAGON_HOARD].name,
                    CC_DRAGON_HOARD_MAP_NAME) == 0);
    CC_CHECK(restored.maps[CC_MAP_DRAGON_HOARD].owner_id ==
             restored.settlements[1].id);
    CC_CHECK(restored.goblins.cohesion == 47);
    CC_CHECK(restored.goblins.expeditions_intercepted == 3);
    CC_CHECK(restored.journey.pace == CC_JOURNEY_PACE_STEADY);
    CC_CHECK(!restored.journey.ambush_warned);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckJournalRecovery(char *error, size_t error_capacity)
{
    const char *path = "persistence-journal-recovery-test.ccsave";
    RemoveDatabase(path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x10a7b00c));
    CcJournal *journal = CcJournalStart(path, &sim, error, error_capacity);
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 2,
                                  error, error_capacity));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[1].id
    };
    CC_CHECK(CcJournalApply(journal, &sim, &travel,
                            error, error_capacity));
    CC_CHECK(CcJournalAdvanceRuntimeTicks(journal, &sim, 4,
                                          error, error_capacity));
    CC_CHECK(CcJournalAdvanceRuntimeTicks(journal, &sim, 4,
                                          error, error_capacity));
    CC_CHECK(CcJournalAdvanceRuntimeTicks(journal, &sim, 2,
                                          error, error_capacity));
    uint64_t expected_hash = CcSimHash(&sim);
    CC_CHECK(CcJournalClose(&journal, &sim, error, error_capacity));
    CC_CHECK(journal == NULL);

    /* The snapshot is still the epoch base; recovery must replay the suffix. */
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT journal_cursor FROM meta WHERE id=1;") == 0);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM action_journal;") == 4);
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    CC_CHECK(restored.journey.active);
    CC_CHECK(restored.clock.tick == sim.clock.tick);

    CcJournal *resumed = CcJournalResume(path, &restored,
                                         error, error_capacity);
    CC_CHECK(resumed != NULL);
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    CC_CHECK(CcJournalAdvanceRuntimeTicks(resumed, &restored, 3,
                                          error, error_capacity));
    expected_hash = CcSimHash(&restored);
    CC_CHECK(CcJournalClose(&resumed, &restored,
                            error, error_capacity));
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM action_journal;") == 5);

    /* SQL clients cannot revise or remove committed input records. */
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open immutable journal");
    char *sqlite_error = NULL;
    int result = sqlite3_exec(database,
                              "UPDATE action_journal SET step_count=99;",
                              NULL, NULL, &sqlite_error);
    CC_CHECK(result == SQLITE_CONSTRAINT);
    CC_CHECK(sqlite_error != NULL &&
             strstr(sqlite_error, "append-only") != NULL);
    sqlite3_free(sqlite_error);
    sqlite_error = NULL;
    result = sqlite3_exec(database, "DELETE FROM action_journal;",
                          NULL, NULL, &sqlite_error);
    CC_CHECK(result == SQLITE_CONSTRAINT);
    CC_CHECK(sqlite_error != NULL &&
             strstr(sqlite_error, "append-only") != NULL);
    sqlite3_free(sqlite_error);
    sqlite3_close(database);
    RemoveDatabase(path);
}

static void CheckJournalCheckpointAndTamper(char *error,
                                            size_t error_capacity)
{
    const char *path = "persistence-journal-checkpoint-test.ccsave";
    RemoveDatabase(path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x10a7c0de));
    CcJournal *journal = CcJournalStart(path, &sim, error, error_capacity);
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1,
                                  error, error_capacity));
    CC_CHECK(CcJournalCheckpoint(journal, &sim, error, error_capacity));
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 3,
                                  error, error_capacity));
    uint64_t expected_hash = CcSimHash(&sim);
    CC_CHECK(CcJournalClose(&journal, &sim, error, error_capacity));
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT journal_cursor FROM meta WHERE id=1;") == 0);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM journal_epoch;") == 1);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM action_journal;") == 1);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT MAX(ordinal) FROM action_journal;") == 1);
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(CcSimHash(&restored) == expected_hash);

    /* Simulate privileged corruption: replay must reject the broken hash chain. */
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open tamper fixture");
    char *sqlite_error = NULL;
    int result = sqlite3_exec(
        database,
        "DROP TRIGGER action_journal_no_update;"
        "UPDATE action_journal SET post_state_hash='0000000000000000' "
        "WHERE ordinal=(SELECT MAX(ordinal) FROM action_journal);",
        NULL, NULL, &sqlite_error);
    if (result != SQLITE_OK) {
        (void)fprintf(stderr, "could not tamper with journal fixture: %s\n",
                      sqlite_error != NULL ? sqlite_error :
                      sqlite3_errmsg(database));
        sqlite3_free(sqlite_error);
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    }
    sqlite3_close(database);
    CC_CHECK(!CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(strstr(error, "diverged") != NULL);
    RemoveDatabase(path);
}

static void ConvertToPreJourneySchema3(const char *path)
{
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open legacy fixture");
    char *sqlite_error = NULL;
    int result = sqlite3_exec(
        database,
        "DROP TABLE situation_cast;"
        "DROP TABLE player_commitment;"
        "DROP TABLE player_journey;"
        "DROP TABLE runtime_state;"
        "DROP TABLE delayed_echo;"
        "PRAGMA user_version=3;",
        NULL, NULL, &sqlite_error);
    if (result != SQLITE_OK) {
        (void)fprintf(stderr, "could not create legacy fixture: %s\n",
                      sqlite_error != NULL ? sqlite_error :
                      sqlite3_errmsg(database));
        sqlite3_free(sqlite_error);
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    }
    sqlite3_close(database);
}

static void RemoveRuntimeStateFromSchema3(const char *path)
{
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open journey migration fixture");
    char *sqlite_error = NULL;
    int result = sqlite3_exec(
        database,
        "DROP TABLE runtime_state; PRAGMA user_version=3;",
        NULL, NULL, &sqlite_error);
    if (result != SQLITE_OK) {
        (void)fprintf(stderr, "could not remove runtime state: %s\n",
                      sqlite_error != NULL ? sqlite_error :
                      sqlite3_errmsg(database));
        sqlite3_free(sqlite_error);
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    }
    sqlite3_close(database);
}

static void CheckPreJourneySchema3Compatibility(char *error,
                                                 size_t error_capacity)
{
    const char *path = "persistence-legacy-v3-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac3));
    CcSimAdvanceDays(&legacy, 11);
    for (int32_t i = 0; i < legacy.situation_count; ++i) {
        legacy.situations[i].sponsor_name[0] = '\0';
        legacy.situations[i].affected_name[0] = '\0';
    }
    legacy.player.accepted_situation_id = 0U;
    legacy.journey = (CcJourneyEncounter){0};
    legacy.resolved_journey_situation_id = 0U;
    legacy.resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    legacy.delayed_echo = (CcDelayedEcho){0};
    legacy.schema_version = 3U;
    legacy.clock = (CcWorldClock){0};
    legacy.carriage = (CcCarriageState){0};
    uint64_t expected = CcSimHash(&legacy);

    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    ConvertToPreJourneySchema3(path);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcSimHash(&restored) != expected);
    CC_CHECK(restored.current_day == legacy.current_day);
    CC_CHECK(restored.player.accepted_situation_id == 0U);
    CC_CHECK(!restored.journey.active);
    CC_CHECK(restored.carriage.mode == CC_CARRIAGE_PARKED);
    CC_CHECK(restored.carriage.location_id == restored.player.location_id);
    CC_CHECK(!restored.delayed_echo.active);
    CC_CHECK(restored.character_count > 0);
    for (int32_t i = 0; i < restored.situation_count; ++i) {
        CC_CHECK(restored.situations[i].sponsor_name[0] != '\0');
        CC_CHECK(restored.situations[i].affected_name[0] != '\0');
        CC_CHECK(CcSimCharacter(
            &restored,
            restored.situations[i].sponsor_character_id) != NULL);
        CC_CHECK(CcSimSituationAffectedCharacter(
            &restored, &restored.situations[i]) != NULL);
    }

    /* The migrated file must remain stable after the new writer adopts it. */
    uint64_t migrated_hash = CcSimHash(&restored);
    CC_CHECK(CcSaveWrite(path, &restored, error, error_capacity));
    CcSim rewritten;
    CC_CHECK(CcSaveRead(path, &rewritten, error, error_capacity));
    CC_CHECK(CcSimHash(&rewritten) == migrated_hash);
    RemoveDatabase(path);

    const char *journey_path = "persistence-legacy-journey-v3-test.ccsave";
    RemoveDatabase(journey_path);
    CcSim legacy_journey;
    CcSimInit(&legacy_journey, UINT32_C(0x1e9ac4));
    const CcSituation *situation = NULL;
    for (int32_t i = 0; i < legacy_journey.situation_count; ++i) {
        if (legacy_journey.situations[i].status == CC_SITUATION_ACTIVE) {
            situation = &legacy_journey.situations[i];
            break;
        }
    }
    CC_CHECK(situation != NULL);
    legacy_journey.player.cargo[CC_GOOD_FOOD] = situation->quantity;
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = situation->id
    };
    CC_CHECK(CcSimApply(&legacy_journey, &accept,
                        error, error_capacity));
    legacy_journey.routes[0].closed = true;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = legacy_journey.settlements[1].id
    };
    CC_CHECK(CcSimApply(&legacy_journey, &travel,
                        error, error_capacity));
    int32_t reserved_fare = legacy_journey.journey.fare_reserved;
    legacy_journey.player.coins += reserved_fare;
    CcMoney legacy_coins = legacy_journey.player.coins;
    legacy_journey.schema_version = 3U;
    legacy_journey.clock = (CcWorldClock){0};
    legacy_journey.carriage = (CcCarriageState){0};
    CC_CHECK(CcSaveWrite(journey_path, &legacy_journey,
                         error, error_capacity));
    RemoveRuntimeStateFromSchema3(journey_path);

    CcSim resumed;
    CC_CHECK(CcSaveRead(journey_path, &resumed, error, error_capacity));
    CC_CHECK(resumed.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(resumed.journey.active);
    CC_CHECK(resumed.journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    CC_CHECK(resumed.journey.elapsed_subticks == 0);
    CC_CHECK(resumed.player.coins == legacy_coins - reserved_fare);
    CC_CHECK(resumed.carriage.mode == CC_CARRIAGE_STOPPED);
    CC_CHECK(CcSimValidate(&resumed, error, error_capacity));
    RemoveDatabase(journey_path);
}

static void CheckCharacterPersistence(char *error, size_t error_capacity)
{
    const char *path = "persistence-character-test.ccsave";
    RemoveDatabase(path);
    CcSim original;
    CcSimInit(&original, UINT32_C(0xc4a4ac7e));
    CcSituation *situation = NULL;
    for (int32_t i = 0; i < original.situation_count; ++i) {
        if (original.situations[i].status == CC_SITUATION_ACTIVE) {
            situation = &original.situations[i];
            break;
        }
    }
    CC_CHECK(situation != NULL);
    const CcCharacter *character = CcSimSituationAffectedCharacter(
        &original, situation);
    CC_CHECK(character != NULL);
    CcId character_id = character->id;
    CcId situation_id = situation->id;
    original.player.location_id = character->current_settlement_id;
    original.carriage.location_id = original.player.location_id;
    CcCommand listen = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = situation_id,
        .amount = CC_CHARACTER_RESPONSE_LISTEN
    };
    CC_CHECK(CcSimApply(&original, &listen, error, error_capacity));
    uint64_t expected_hash = CcSimHash(&original);
    CC_CHECK(CcSaveWrite(path, &original, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    const CcCharacter *remembering = CcSimCharacter(
        &restored, character_id);
    CC_CHECK(remembering != NULL);
    CC_CHECK(remembering->appearance_seed == character->appearance_seed);
    CC_CHECK(remembering->player_disposition == 2);
    CC_CHECK(CcCharacterRemembers(
        remembering, CC_CHARACTER_MEMORY_MET_PLAYER, situation_id));
    RemoveDatabase(path);
}

static void CheckSocialThreadPersistence(char *error, size_t error_capacity)
{
    const char *path = "persistence-social-thread-test.ccsave";
    RemoveDatabase(path);
    CcSim original;
    CcSimInit(&original, UINT32_C(0x50c1a1));
    CcSituation *mine = NULL;
    for (int32_t i = 0; i < original.situation_count; ++i) {
        if (original.situations[i].kind ==
            CC_SITUATION_MONSTER_EXPEDITION) {
            mine = &original.situations[i];
            break;
        }
    }
    CC_CHECK(mine != NULL);
    const CcCharacter *jory = CcSimSituationAffectedCharacter(
        &original, mine);
    const CcCharacter *mara = CcSimSituationSponsorCharacter(
        &original, mine);
    CC_CHECK(jory != NULL && mara != NULL);
    original.player.location_id = jory->current_settlement_id;
    original.carriage.location_id = original.player.location_id;
    CcCommand response = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = mine->id,
        .amount = CC_CHARACTER_RESPONSE_LISTEN
    };
    CC_CHECK(CcSimApply(&original, &response, error, error_capacity));
    CC_CHECK(CcSimApply(&original, &response, error, error_capacity));
    response.amount = CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE;
    CC_CHECK(CcSimApply(&original, &response, error, error_capacity));
    CC_CHECK(mine->lead_path == CC_LEAD_PATH_CONFIDENCE);
    uint64_t expected_hash = CcSimHash(&original);
    CC_CHECK(CcSaveWrite(path, &original, error, error_capacity));
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM character_relationship;") ==
             original.relationship_count);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    const CcSituation *restored_mine = CcSimSituation(&restored, mine->id);
    CC_CHECK(restored_mine != NULL);
    CC_CHECK(restored_mine->discovery_stage == CC_DISCOVERY_OFFER);
    CC_CHECK(restored_mine->lead_path == CC_LEAD_PATH_CONFIDENCE);
    CC_CHECK(restored_mine->witness_character_id ==
             mine->witness_character_id);
    const CcCharacter *restored_jory = CcSimCharacter(&restored, jory->id);
    CC_CHECK(CcCharacterKnows(
        restored_jory, CC_KNOWLEDGE_OFFER, restored_mine->id));
    const CcRelationship *restored_relationship = CcSimRelationship(
        &restored, jory->id, mara->id);
    CC_CHECK(restored_relationship != NULL);
    CC_CHECK(restored_relationship->history ==
             CcSimRelationship(&original, jory->id, mara->id)->history);
    const CcEvent *lead_event = CcSimEvent(
        &restored, restored_mine->lead_event_id);
    CC_CHECK(lead_event != NULL);
    CC_CHECK(lead_event->actor_id == jory->id);
    CC_CHECK(lead_event->target_id == restored.player.id);
    RemoveDatabase(path);
}

static void CheckSchema18Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v18-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac18));
    CcSituation *mine = NULL;
    for (int32_t i = 0; i < legacy.situation_count; ++i) {
        if (legacy.situations[i].kind ==
            CC_SITUATION_MONSTER_EXPEDITION) {
            mine = &legacy.situations[i];
            break;
        }
    }
    CC_CHECK(mine != NULL);
    legacy.player.accepted_situation_id = mine->id;
    legacy.schema_version = 18U;
    legacy.generator_version = 17U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    const CcSituation *restored_mine = CcSimSituation(&restored, mine->id);
    CC_CHECK(restored_mine != NULL);
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored_mine->discovery_stage == CC_DISCOVERY_OFFER);
    CC_CHECK(restored.player.accepted_situation_id == restored_mine->id);
    CC_CHECK(restored.relationship_count >= 4);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

int main(void)
{
    const char *path = "persistence-test.ccsave";
    RemoveDatabase(path);

    CcSim original;
    CcSimInit(&original, UINT32_C(0xa11ce5ed));
    original.kingdoms[0].iron_ledger_debt = 37;
    original.iron_ledger_reserve -= 37;
    original.settlements[0].market_coins += 37;
    original.goblins.hoard_defenses = 4;
    original.hoard_raiders.social_raid_latched = true;
    original.hoard_raiders.war_raid_latched = true;
    CcSimAdvanceDays(&original, 23);
    char error[256];
    CcSettlement *capital = &original.settlements[4];
    original.goblins.tribute_cooldown_days = 1000;
    original.hoard_raiders.cooldown_days = 1000;
    capital->stock[CC_GOOD_GOLD] = 1;
    capital->stock[CC_GOOD_GEMS] = 1;
    CcSimAdvanceDays(&original, 21);
    CC_CHECK(original.treasure_count >= 1);
    capital->stock[CC_GOOD_MATERIAL] += 20;
    capital->stock[CC_GOOD_TOOLS] += 10;
    original.kingdoms[2].treasury += 100;
    CC_CHECK(CcSimStartServiceProject(&original, capital->id,
                                      CC_SERVICE_GRANARY,
                                      error, sizeof(error)));
    CheckReadDoesNotCreateOrRelabel(error, sizeof(error));
    CheckJournalOwnership(error, sizeof(error));
    CheckForgedExtremeStateRejected(error, sizeof(error));
    CheckMalformedTextRejected(error, sizeof(error));
    CheckForgedIdentityStateRejected(error, sizeof(error));
    CheckPreJourneySchema3Compatibility(error, sizeof(error));
    CheckSchema4Compatibility(error, sizeof(error));
    CheckSchema5Compatibility(error, sizeof(error));
    CheckSchema6Compatibility(error, sizeof(error));
    CheckSchema8Compatibility(error, sizeof(error));
    CheckSchema10Compatibility(error, sizeof(error));
    CheckSchema11Compatibility(error, sizeof(error));
    CheckSchema12Compatibility(error, sizeof(error));
    CheckSchema13Compatibility(error, sizeof(error));
    CheckSchema14Compatibility(error, sizeof(error));
    CheckSchema15Compatibility(error, sizeof(error));
    CheckSchema16Compatibility(error, sizeof(error));
    CheckSchema17Compatibility(error, sizeof(error));
    CheckSchema18Compatibility(error, sizeof(error));
    CheckDiplomacyPersistence(error, sizeof(error));
    CheckJournalRecovery(error, sizeof(error));
    CheckJournalCheckpointAndTamper(error, sizeof(error));
    CheckLegacyJournalMigration(error, sizeof(error));
    CheckCharacterPersistence(error, sizeof(error));
    CheckSocialThreadPersistence(error, sizeof(error));
    CcCommand command = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = original.settlements[1].id
    };
    CC_CHECK(CcSimApply(&original, &command, error, sizeof(error)));
    while (original.journey.active) {
        CcSimAdvanceRuntimeTicks(&original, CC_WORLD_TICKS_PER_SECOND);
    }
    const CcSituation *charter = NULL;
    for (int32_t i = 0; i < original.situation_count; ++i) {
        if (original.situations[i].status == CC_SITUATION_ACTIVE &&
            CcSimSituationOfferSettlementId(
                &original, &original.situations[i]) ==
                original.player.location_id) {
            charter = &original.situations[i];
            break;
        }
    }
    CC_CHECK(charter != NULL);
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = charter->id
    };
    CC_CHECK(CcSimApply(&original, &accept, error, sizeof(error)));
    CcCommand prepare_journey = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = original.settlements[0].id
    };
    CC_CHECK(CcSimApply(&original, &prepare_journey, error, sizeof(error)));
    CC_CHECK(original.journey.active);
    CC_CHECK(original.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CcCommand push_pace = {
        .kind = CC_COMMAND_SET_JOURNEY_PACE,
        .amount = CC_JOURNEY_PACE_PUSH
    };
    CC_CHECK(CcSimApply(&original, &push_pace, error, sizeof(error)));
    CcSimAdvanceRuntimeTicks(&original, 480);
    CC_CHECK(original.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(original.carriage.progress_milli > 0);
    original.delayed_echo = (CcDelayedEcho){
        .active = true,
        .situation_id = charter->id,
        .settlement_id = original.settlements[1].id,
        .parent_event_id = charter->cause_event_id,
        .outcome = CC_JOURNEY_OUTCOME_COMBAT,
        .due_day = original.current_day + 9
    };
    (void)snprintf(original.delayed_echo.character_name,
                   sizeof(original.delayed_echo.character_name), "%s",
                   charter->affected_name);
    original.goblins.cohesion = 77;
    original.goblins.expeditions_intercepted = 3;
    uint64_t expected = CcSimHash(&original);
    CC_CHECK(CcSaveWrite(path, &original, error, sizeof(error)));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == expected);
    CC_CHECK(restored.current_day == original.current_day);
    CC_CHECK(restored.iron_ledger_reserve ==
             original.iron_ledger_reserve);
    CC_CHECK(restored.kingdoms[0].iron_ledger_debt ==
             original.kingdoms[0].iron_ledger_debt);
    CC_CHECK(restored.goblins.hoard_defenses == 4);
    CC_CHECK(restored.goblins.cohesion == 77);
    CC_CHECK(restored.goblins.expeditions_intercepted == 3);
    CC_CHECK(restored.hoard_raiders.social_raid_latched);
    CC_CHECK(restored.hoard_raiders.war_raid_latched);
    CC_CHECK(restored.player.location_id == original.player.location_id);
    CC_CHECK(restored.player.map_capacity == original.player.map_capacity);
    CC_CHECK(restored.player.accepted_situation_id ==
             original.player.accepted_situation_id);
    CC_CHECK(restored.player.map_catalogue_mask ==
             original.player.map_catalogue_mask);
    CC_CHECK(restored.player.map_archive_mask ==
             original.player.map_archive_mask);
    CC_CHECK(restored.settlements[4].service_mask ==
             original.settlements[4].service_mask);
    CC_CHECK(restored.settlements[4].service_project ==
             original.settlements[4].service_project);
    CC_CHECK(restored.settlements[4].service_project_days ==
             original.settlements[4].service_project_days);
    CC_CHECK(restored.bandits[0].camp_size == original.bandits[0].camp_size);
    CC_CHECK(restored.bandits[0].raid_phase == original.bandits[0].raid_phase);
    CC_CHECK(restored.bandits[0].raid_target_id ==
             original.bandits[0].raid_target_id);
    CC_CHECK(restored.bandits[0].raids_completed ==
             original.bandits[0].raids_completed);
    CC_CHECK(CcSimAcceptedSituation(&restored) != NULL);
    CC_CHECK(restored.journey.active);
    CC_CHECK(restored.journey.phase == original.journey.phase);
    CC_CHECK(restored.journey.route_id == original.journey.route_id);
    CC_CHECK(restored.journey.bargain_cost == original.journey.bargain_cost);
    CC_CHECK(restored.journey.elapsed_subticks ==
             original.journey.elapsed_subticks);
    CC_CHECK(restored.journey.pace == CC_JOURNEY_PACE_PUSH);
    CC_CHECK(restored.clock.tick == original.clock.tick);
    CC_CHECK(restored.clock.minute_subticks ==
             original.clock.minute_subticks);
    CC_CHECK(restored.carriage.progress_milli ==
             original.carriage.progress_milli);
    CC_CHECK(restored.delayed_echo.active);
    CC_CHECK(restored.delayed_echo.due_day == original.delayed_echo.due_day);
    CC_CHECK(strcmp(restored.delayed_echo.character_name,
                    original.delayed_echo.character_name) == 0);
    CC_CHECK(restored.map_count == original.map_count);
    CC_CHECK(restored.treasure_count == original.treasure_count);
    CC_CHECK(strcmp(restored.treasures[0].name,
                    original.treasures[0].name) == 0);
    CC_CHECK(restored.treasures[0].owner_id ==
             original.treasures[0].owner_id);
    CC_CHECK(restored.maps[0].owner_id == original.maps[0].owner_id);
    CC_CHECK(restored.maps[0].recorded_danger ==
             original.maps[0].recorded_danger);
    CC_CHECK(restored.event_count == original.event_count);

    RemoveDatabase(path);
    puts("SQLite persistence tests passed");
    return 0;
}
