#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <inttypes.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

enum {
    CC_TEST_SCHEMA12_COMMAND_STEAL_DRAGON_NAMED_TREASURE = 16,
    CC_TEST_SCHEMA12_COMMAND_RETURN_DRAGON_NAMED_TREASURE = 17,
    CC_TEST_SCHEMA17_EVENT_ENCOUNTER_LOOT = 90
};

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

static void CompleteJourney(CcSim *sim, char *error, size_t error_capacity)
{
    while (sim->journey.active) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
            CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
        } else if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {
                .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                    CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
            };
            CC_CHECK(CcSimApply(sim, &rest, error, error_capacity));
        } else {
            CC_CHECK(false);
        }
    }
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
        "UPDATE settlement_good SET stock=%d "
        "WHERE settlement_slot=0 AND good=%d;"
        "UPDATE meta SET state_hash=%Q WHERE id=1;",
        INT32_MAX, INT32_MAX, CC_GOOD_BREAD, forged_hash);
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

static void AddSchema23RuntimeJournalSuffix(const char *path,
                                            const CcSim *before,
                                            const CcSim *after,
                                            int32_t ticks)
{
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open schema 23 journal fixture");
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
        "VALUES(last_insert_rowid(),1,1,3,0,0,0,0,0,%d,23,20,%Q,%Q,%llu);"
        "UPDATE meta SET journal_generation="
        "(SELECT MAX(generation) FROM journal_epoch),"
        "journal_cursor=0 WHERE id=1;"
        "COMMIT;",
        before->world_seed, pre_hash,
        (unsigned long long)before->clock.tick,
        ticks, pre_hash, post_hash,
        (unsigned long long)after->clock.tick);
    CC_CHECK(sql != NULL);
    ExecuteFixtureSql(database, sql,
                      "could not create schema 23 journal suffix");
    sqlite3_free(sql);
    sqlite3_close(database);
}

static void ClearLegacyCharacterLifecycles(CcSim *sim)
{
    sim->character_births = 0;
    sim->character_deaths = 0;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        sim->characters[i].ancestor_id = 0U;
        sim->characters[i].birth_day = 0;
        sim->characters[i].death_day = 0;
        sim->characters[i].generation = 0;
    }
}

static void AddLegacyDayJournalSuffix(const char *path,
                                      const CcSim *before,
                                      const CcSim *after,
                                      uint32_t schema_version,
                                      uint32_t generator_version)
{
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open lifecycle journal fixture");
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
        "VALUES(last_insert_rowid(),1,1,2,0,0,0,0,0,1,%u,%u,%Q,%Q,%llu);"
        "UPDATE meta SET journal_generation="
        "(SELECT MAX(generation) FROM journal_epoch),"
        "journal_cursor=0 WHERE id=1;"
        "COMMIT;",
        before->world_seed, pre_hash,
        (unsigned long long)before->clock.tick,
        schema_version, generator_version, pre_hash, post_hash,
        (unsigned long long)after->clock.tick);
    CC_CHECK(sql != NULL);
    ExecuteFixtureSql(database, sql,
                      "could not create lifecycle journal suffix");
    sqlite3_free(sql);
    sqlite3_close(database);
}

static void AddSchema12NamedTreasureJournalSuffix(
    const char *path, const CcSim *before, const CcSim *after_steal,
    const CcSim *after_return, CcId treasure_id)
{
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open schema 12 command fixture");
    char pre_hash[24];
    char steal_hash[24];
    char return_hash[24];
    (void)snprintf(pre_hash, sizeof(pre_hash), "%016" PRIx64,
                   CcSimHash(before));
    (void)snprintf(steal_hash, sizeof(steal_hash), "%016" PRIx64,
                   CcSimHash(after_steal));
    (void)snprintf(return_hash, sizeof(return_hash), "%016" PRIx64,
                   CcSimHash(after_return));
    char *sql = sqlite3_mprintf(
        "BEGIN IMMEDIATE;"
        "INSERT INTO journal_epoch "
        "(record_version,world_seed,initial_state_hash,created_tick) "
        "VALUES(1,%u,%Q,%llu);"
        "INSERT INTO action_journal "
        "(generation,ordinal,record_version,operation_kind,command_kind,"
        "target_id,good,amount,dungeon_state,step_count,sim_schema_version,"
        "generator_version,pre_state_hash,post_state_hash,committed_tick) "
        "VALUES((SELECT MAX(generation) FROM journal_epoch),1,1,1,%d,%llu,"
        "0,0,0,0,12,12,%Q,%Q,%llu);"
        "INSERT INTO action_journal "
        "(generation,ordinal,record_version,operation_kind,command_kind,"
        "target_id,good,amount,dungeon_state,step_count,sim_schema_version,"
        "generator_version,pre_state_hash,post_state_hash,committed_tick) "
        "VALUES((SELECT MAX(generation) FROM journal_epoch),2,1,1,%d,%llu,"
        "0,0,0,0,12,12,%Q,%Q,%llu);"
        "UPDATE meta SET journal_generation="
        "(SELECT MAX(generation) FROM journal_epoch),"
        "journal_cursor=0 WHERE id=1;"
        "PRAGMA user_version=12;"
        "COMMIT;",
        before->world_seed, pre_hash,
        (unsigned long long)before->clock.tick,
        CC_TEST_SCHEMA12_COMMAND_STEAL_DRAGON_NAMED_TREASURE,
        (unsigned long long)treasure_id, pre_hash, steal_hash,
        (unsigned long long)after_steal->clock.tick,
        CC_TEST_SCHEMA12_COMMAND_RETURN_DRAGON_NAMED_TREASURE,
        (unsigned long long)treasure_id, steal_hash, return_hash,
        (unsigned long long)after_return->clock.tick);
    CC_CHECK(sql != NULL);
    ExecuteFixtureSql(database, sql,
                      "could not create schema 12 command fixture");
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
    CC_CHECK(ReadSqliteInteger(path, "PRAGMA user_version;") == 27);
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
    sim.dragon_campaign.patron_character_id =
        sim.kingdoms[0].monastery_patron_id;
    sim.dragon_campaign.hero_character_id = sim.characters[1].id;
    sim.dragon.age_days = 500 * 365;
    CcSimAdvanceDays(&sim, 27);
    sim.dragon.territoryless_days = 17;
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
    CC_CHECK(restored.archives.abbot_character_id ==
             sim.archives.abbot_character_id);
    CC_CHECK(restored.archives.stewardship_rank ==
             sim.archives.stewardship_rank);
    CC_CHECK(restored.kingdoms[0].ruler_character_id ==
             sim.kingdoms[0].ruler_character_id);
    CC_CHECK(restored.kingdoms[0].monastery_patron_id ==
             sim.kingdoms[0].monastery_patron_id);
    CC_CHECK(restored.kingdoms[0].sanction ==
             sim.kingdoms[0].sanction);
    CC_CHECK(restored.dragon.territoryless_days == 17);
    CC_CHECK(restored.dragon_campaign.patron_character_id ==
             sim.dragon_campaign.patron_character_id);
    CC_CHECK(restored.dragon_campaign.hero_character_id ==
             sim.dragon_campaign.hero_character_id);
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

static void CheckSchema12CommandKindCompatibility(char *error,
                                                   size_t error_capacity)
{
    const char *path = "persistence-legacy-v12-command-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac120));
    legacy.map_count = CC_MAX_ROUTES;
    legacy.player.map_catalogue_mask = 0U;
    legacy.player.map_archive_mask = 0U;
    legacy.player.location_id = legacy.dragon.lair_settlement_id;
    legacy.carriage.location_id = legacy.player.location_id;
    CcTreasure *treasure = &legacy.treasures[legacy.treasure_count++];
    *treasure = (CcTreasure){
        .id = CcMakeId(CC_ENTITY_TREASURE, legacy.next_entity_serial++),
        .maker_settlement_id = legacy.settlements[0].id,
        .owner_id = legacy.dragon.id,
        .location_id = legacy.dragon.lair_settlement_id,
        .gold_content = 2,
        .gem_content = 2,
        .craft_work = 3,
        .appraised_value = 240,
        .created_day = 1
    };
    (void)snprintf(treasure->name, sizeof(treasure->name),
                   "The First Crown's Seal");
    CcId treasure_id = treasure->id;
    legacy.schema_version = 12U;
    legacy.generator_version = 12U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim after_steal = legacy;
    CcCommand steal = {
        .kind = CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE,
        .target_id = treasure_id
    };
    CC_CHECK(CcSimApply(&after_steal, &steal, error, error_capacity));
    CcSim after_return = after_steal;
    CcCommand return_treasure = {
        .kind = CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE,
        .target_id = treasure_id
    };
    CC_CHECK(CcSimApply(&after_return, &return_treasure,
                        error, error_capacity));
    AddSchema12NamedTreasureJournalSuffix(
        path, &legacy, &after_steal, &after_return, treasure_id);
    CC_CHECK(ReadSqliteInteger(
                 path,
                 "SELECT command_kind FROM action_journal "
                 "WHERE ordinal=1;") ==
             CC_TEST_SCHEMA12_COMMAND_STEAL_DRAGON_NAMED_TREASURE);
    CC_CHECK(ReadSqliteInteger(
                 path,
                 "SELECT command_kind FROM action_journal "
                 "WHERE ordinal=2;") ==
             CC_TEST_SCHEMA12_COMMAND_RETURN_DRAGON_NAMED_TREASURE);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    const CcTreasure *restored_treasure = CcSimTreasure(
        &restored, treasure_id);
    CC_CHECK(restored_treasure != NULL);
    CC_CHECK(restored_treasure->owner_id == restored.dragon.id);
    CC_CHECK(restored.dragon.stolen_treasure_id == 0U);
    bool found_theft = false;
    bool found_return = false;
    for (int32_t i = 0; i < restored.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&restored, i);
        if (event != NULL && event->kind == CC_EVENT_DRAGON_HOARD_STOLEN) {
            found_theft = true;
        }
        if (event != NULL &&
            event->kind == CC_EVENT_DRAGON_TREASURE_RETURNED) {
            found_return = true;
        }
    }
    CC_CHECK(found_theft);
    CC_CHECK(found_return);
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
    const CcEvent *recent = CcSimRecentEvent(&legacy, 0);
    CC_CHECK(recent != NULL);
    CcId legacy_loot_event_id = recent->id;
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        if (legacy.events[i].id == legacy_loot_event_id) {
            legacy.events[i].kind =
                (CcEventKind)CC_TEST_SCHEMA17_EVENT_ENCOUNTER_LOOT;
        }
    }
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
    const CcEvent *restored_loot = CcSimEvent(
        &restored, legacy_loot_event_id);
    CC_CHECK(restored_loot != NULL);
    CC_CHECK(restored_loot->kind == CC_EVENT_ENCOUNTER_LOOT);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema18QuestCompatibility(char *error,
                                            size_t error_capacity)
{
    const char *path = "persistence-legacy-v18-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac18));
    legacy.schema_version = 18U;
    legacy.generator_version = 17U;
    legacy.front_count = 0;
    legacy.quest_outcome_count = 0;
    legacy.pending_echo_count = 0;
    (void)memset(legacy.fronts, 0, sizeof(legacy.fronts));
    (void)memset(legacy.quest_outcomes, 0,
                 sizeof(legacy.quest_outcomes));
    (void)memset(legacy.pending_echoes, 0,
                 sizeof(legacy.pending_echoes));
    for (int32_t i = 0; i < legacy.situation_count; ++i) {
        legacy.situations[i].front_id = 0U;
        legacy.situations[i].end_reason = CC_QUEST_END_NONE;
        legacy.situations[i].objective = (CcQuestObjective){0};
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.front_count > 0);
    CC_CHECK(restored.situation_count == legacy.situation_count);
    for (int32_t i = 0; i < restored.situation_count; ++i) {
        CC_CHECK(restored.situations[i].front_id != 0U);
        CC_CHECK(restored.situations[i].objective.target_id != 0U);
        CC_CHECK(restored.situations[i].objective.progress.limit > 0);
    }
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
    legacy_journey.schema_version = 40U;
    const CcSituation *situation = NULL;
    for (int32_t i = 0; i < legacy_journey.situation_count; ++i) {
        if (legacy_journey.situations[i].status == CC_SITUATION_ACTIVE) {
            situation = &legacy_journey.situations[i];
            break;
        }
    }
    CC_CHECK(situation != NULL);
    legacy_journey.player.cargo[CC_GOOD_FOOD] = 0;
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
    int32_t successor_slot = original.character_count - 1;
    CcId ancestor_id = original.characters[successor_slot].id;
    original.characters[successor_slot].death_day =
        original.current_day + 1;
    CcSimAdvanceDays(&original, 1);
    CcId successor_id = original.characters[successor_slot].id;
    CC_CHECK(successor_id != ancestor_id);
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
    const CcCharacter *successor = CcSimCharacter(&restored, successor_id);
    CC_CHECK(successor != NULL);
    CC_CHECK(successor->ancestor_id == ancestor_id);
    CC_CHECK(successor->generation == 1);
    CC_CHECK(restored.character_births == 1);
    CC_CHECK(restored.character_deaths == 1);
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

static void CheckSchema21Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v21-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac21));
    legacy.schema_version = 21U;
    legacy.archives = (CcArchives){0};
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.current_day == legacy.current_day);
    CC_CHECK(restored.archives.scribes == 0);
    CC_CHECK(restored.archives.lore_stored == 0);
    CC_CHECK(restored.archives.lore_lost_total == 0);
    CC_CHECK(restored.archives.lore_ceiling == 40);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void PrepareLegacyJourneyTiming(CcSim *sim, int32_t travel_days,
                                       char *error, size_t error_capacity)
{
    int32_t new_total = sim->journey.total_subticks;
    int32_t old_total = travel_days * CC_WORLD_DAY_SUBTICKS;
    CC_CHECK(new_total > 0);
    CC_CHECK(old_total > 0);
    sim->journey.encounter_subticks = (int32_t)(
        (int64_t)sim->journey.encounter_subticks * old_total / new_total);
    sim->journey.total_subticks = old_total;
    sim->journey.elapsed_subticks = 0;
    sim->carriage.progress_milli = 0;
    CcCommand refresh_pace = {
        .kind = CC_COMMAND_SET_JOURNEY_PACE,
        .amount = (int32_t)sim->journey.pace
    };
    CC_CHECK(CcSimApply(sim, &refresh_pace, error, error_capacity));
}

static void CheckSchema22Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v22-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac22));
    CcTravelPreview preview = {0};
    CC_CHECK(CcSimTravelPreview(
        &legacy, legacy.settlements[1].id, &preview,
        error, error_capacity));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = legacy.settlements[1].id
    };
    CC_CHECK(CcSimApply(&legacy, &travel, error, error_capacity));
    PrepareLegacyJourneyTiming(
        &legacy, preview.travel_days, error, error_capacity);
    legacy.journey.elapsed_subticks =
        legacy.journey.total_subticks * 3 / 10;
    legacy.carriage.progress_milli = 300;
    legacy.clock.minute_subticks =
        legacy.journey.elapsed_subticks % CC_WORLD_DAY_SUBTICKS;
    CC_CHECK(legacy.journey.active);
    CC_CHECK(legacy.carriage.progress_milli > 0);
    legacy.schema_version = 22U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open schema 22 fixture");
    ExecuteFixtureSql(database,
                      "DROP TABLE player_route_knowledge;"
                      "PRAGMA user_version=20;",
                      "could not create schema 22 fixture");
    sqlite3_close(database);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.journey.active);
    const CcRouteKnowledge *knowledge = CcSimPlayerRouteKnowledge(
        &restored, restored.journey.route_id);
    CC_CHECK(knowledge != NULL);
    CC_CHECK(knowledge->from_reveal_milli > 280);
    CC_CHECK(knowledge->to_reveal_milli == 0);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema23Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v23-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x12823));
    legacy.generator_version = 20U;
    CcRoute *route = &legacy.routes[0];
    for (int32_t i = 0; i < legacy.map_count; ++i) {
        CcMap *map = &legacy.maps[i];
        if (map->route_id != route->id ||
            map->owner_id != legacy.player.id) continue;
        map->owner_id = legacy.player.location_id;
        legacy.player.map_catalogue_mask &=
            ~(UINT32_C(1) << (uint32_t)i);
        legacy.player.map_archive_mask &=
            ~(UINT32_C(1) << (uint32_t)i);
    }
    CcSimInitializePlayerRouteKnowledge(&legacy);
    legacy.schema_version = 23U;
    legacy.generator_version = 20U;
    route->closed = false;
    route->security = 100;
    route->condition = 100;
    CcTravelPreview preview = {0};
    CC_CHECK(CcSimTravelPreview(
        &legacy, route->to_id, &preview, error, error_capacity));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = route->to_id
    };
    CC_CHECK(CcSimApply(&legacy, &travel, error, error_capacity));
    PrepareLegacyJourneyTiming(
        &legacy, preview.travel_days, error, error_capacity);
    legacy.journey.ambush_pending = false;
    CcSim suffix = legacy;
    CcSimAdvanceRuntimeTicks(&suffix, 480);
    const CcRouteKnowledge *knowledge = CcSimPlayerRouteKnowledge(
        &suffix, route->id);
    CC_CHECK(knowledge != NULL);
    int32_t expected_reveal = suffix.carriage.progress_milli + 140;
    if (expected_reveal < 280) expected_reveal = 280;
    if (expected_reveal > 1000) expected_reveal = 1000;
    CC_CHECK(knowledge->from_reveal_milli == expected_reveal);
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    AddSchema23RuntimeJournalSuffix(path, &legacy, &suffix, 480);
    int64_t legacy_generation = ReadSqliteInteger(
        path, "SELECT journal_generation FROM meta WHERE id=1;");

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    knowledge = CcSimPlayerRouteKnowledge(&restored, route->id);
    CC_CHECK(knowledge != NULL);
    CC_CHECK(knowledge->from_reveal_milli == expected_reveal);
    CC_CHECK(restored.carriage.progress_milli ==
             suffix.carriage.progress_milli);
    CC_CHECK(restored.journey.active);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    uint64_t migrated_hash = CcSimHash(&restored);

    CcSim resumed;
    CcJournal *journal = CcJournalResume(
        path, &resumed, error, error_capacity);
    CC_CHECK(journal != NULL);
    CC_CHECK(CcSimHash(&resumed) == migrated_hash);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT journal_generation FROM meta WHERE id=1;") !=
             legacy_generation);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM action_journal;") == 0);
    CC_CHECK(CcJournalClose(&journal, &resumed, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema24Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v24-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac24));
    CcId first_character_id = legacy.characters[0].id;
    char first_character_name[CC_NAME_CAPACITY];
    (void)snprintf(first_character_name, sizeof(first_character_name), "%s",
                   legacy.characters[0].name);
    CcTravelPreview preview = {0};
    CC_CHECK(CcSimTravelPreview(
        &legacy, legacy.settlements[1].id, &preview,
        error, error_capacity));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = legacy.settlements[1].id
    };
    CC_CHECK(CcSimApply(&legacy, &travel, error, error_capacity));
    PrepareLegacyJourneyTiming(
        &legacy, preview.travel_days, error, error_capacity);
    legacy.journey.ambush_pending = false;
    CcSimAdvanceRuntimeTicks(&legacy, 480);
    int32_t legacy_progress = legacy.carriage.progress_milli;
    legacy.schema_version = 24U;
    legacy.generator_version = 20U;
    ClearLegacyCharacterLifecycles(&legacy);
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.journey.active);
    CC_CHECK(restored.journey.total_subticks ==
             (preview.travel_days * 2 < 3 ? 3 : preview.travel_days * 2) *
                 CC_WORLD_WATCH_SUBTICKS);
    CC_CHECK(restored.carriage.progress_milli == legacy_progress);
    CC_CHECK(restored.character_count == CC_MAX_CHARACTERS);
    CC_CHECK(restored.characters[0].id == first_character_id);
    CC_CHECK(strcmp(restored.characters[0].name,
                    first_character_name) == 0);
    for (int32_t i = 0; i < restored.character_count; ++i) {
        CC_CHECK(restored.characters[i].birth_day <= restored.current_day);
        CC_CHECK(restored.characters[i].death_day > restored.current_day);
        CC_CHECK(restored.characters[i].generation == 0);
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema25Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v25-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac25));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = legacy.settlements[1].id
    };
    CC_CHECK(CcSimApply(&legacy, &travel, error, error_capacity));
    legacy.journey.ambush_pending = false;
    CcSimAdvanceRuntimeTicks(&legacy, 480);
    int32_t journey_total = legacy.journey.total_subticks;
    int32_t journey_progress = legacy.carriage.progress_milli;
    legacy.schema_version = 25U;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.journey.total_subticks == journey_total);
    CC_CHECK(restored.carriage.progress_milli == journey_progress);
    for (int32_t settlement = 0;
         settlement < restored.settlement_count; ++settlement) {
        for (int32_t good = CC_LEGACY_GOOD_COUNT;
             good < CC_GOOD_COUNT; ++good) {
            if (good == CC_GOOD_WOOD || good == CC_GOOD_WHEAT ||
                good == CC_GOOD_STONE) {
                CC_CHECK(restored.settlements[settlement].stock[good] > 0);
                CC_CHECK(restored.settlements[settlement]
                             .reserve_target[good] > 0);
                if (good == CC_GOOD_WOOD) {
                    CC_CHECK(restored.settlements[settlement]
                                 .production[good] > 0);
                } else if (good == CC_GOOD_STONE) {
                    CC_CHECK(restored.settlements[settlement]
                                 .production[good] ==
                             (restored.settlements[settlement].function ==
                                  CC_SETTLEMENT_MINING ? 12 : 0));
                }
            } else if (good == CC_GOOD_MEAT ||
                       good == CC_GOOD_WOOL) {
                CC_CHECK(restored.settlements[settlement].reserve_target[good] > 0);
                if (CcSettlementHasService(
                        &restored.settlements[settlement],
                        CC_SERVICE_FARM)) {
                    CC_CHECK(restored.settlements[settlement].stock[good] > 0);
                } else {
                    CC_CHECK(restored.settlements[settlement].stock[good] == 0);
                }
            } else if (good == CC_GOOD_PAPER) {
                CC_CHECK(restored.settlements[settlement].stock[good] > 0);
                CC_CHECK(restored.settlements[settlement]
                             .reserve_target[good] > 0);
            } else {
                CC_CHECK(restored.settlements[settlement].stock[good] == 0);
                CC_CHECK(restored.settlements[settlement]
                             .reserve_target[good] == 0);
                CC_CHECK(restored.settlements[settlement]
                             .production[good] == 0);
            }
            if (good != CC_GOOD_PAPER) {
                CC_CHECK(restored.settlements[settlement]
                             .consumption[good] == 0);
            }
            CC_CHECK(restored.settlements[settlement].price[good] ==
                     CcGoodDefinitionFor((CcGood)good)->base_price);
        }
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema26Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v26-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac26));
    legacy.schema_version = 26U;
    legacy.generator_version = 21U;
    CcId first_character_id = legacy.characters[0].id;
    int32_t first_birth_day = legacy.characters[0].birth_day;
    int32_t first_death_day = legacy.characters[0].death_day;
    for (int32_t settlement = 0;
         settlement < legacy.settlement_count; ++settlement) {
        for (int32_t good = CC_LEGACY_GOOD_COUNT;
             good < CC_GOOD_COUNT; ++good) {
            legacy.settlements[settlement].stock[good] = 0;
            legacy.settlements[settlement].reserve_target[good] = 0;
            legacy.settlements[settlement].production[good] = 0;
            legacy.settlements[settlement].consumption[good] = 0;
            legacy.settlements[settlement].price[good] = 0;
        }
        legacy.settlements[settlement].sheep_adults = 0;
        legacy.settlements[settlement].sheep_lambs = 0;
        legacy.settlements[settlement].sheep_condition = 0;
        legacy.settlements[settlement].sheep_hunger = 0;
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(path, &database,
                                  SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open schema 26 fixture");
    ExecuteFixtureSql(database,
                      "DELETE FROM settlement_good;"
                      "DELETE FROM player_good;"
                      "DELETE FROM goblin_good;"
                      "DELETE FROM dragon_good;"
                      "DELETE FROM dragon_campaign_good;",
                      "could not remove future goods rows");
    sqlite3_close(database);
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.characters[0].id == first_character_id);
    CC_CHECK(restored.characters[0].birth_day == first_birth_day);
    CC_CHECK(restored.characters[0].death_day == first_death_day);
    for (int32_t settlement = 0;
         settlement < restored.settlement_count; ++settlement) {
        for (int32_t good = CC_LEGACY_GOOD_COUNT;
             good < CC_GOOD_COUNT; ++good) {
            if (good == CC_GOOD_WOOD || good == CC_GOOD_WHEAT ||
                good == CC_GOOD_STONE) {
                CC_CHECK(restored.settlements[settlement].stock[good] > 0);
            } else if (good == CC_GOOD_MEAT ||
                       good == CC_GOOD_WOOL) {
                if (CcSettlementHasService(
                        &restored.settlements[settlement],
                        CC_SERVICE_FARM)) {
                    CC_CHECK(restored.settlements[settlement].stock[good] > 0);
                } else {
                    CC_CHECK(restored.settlements[settlement].stock[good] == 0);
                }
            } else if (good == CC_GOOD_PAPER) {
                CC_CHECK(restored.settlements[settlement].stock[good] > 0);
                CC_CHECK(restored.settlements[settlement]
                             .reserve_target[good] > 0);
            } else {
                CC_CHECK(restored.settlements[settlement].stock[good] == 0);
            }
            CC_CHECK(restored.settlements[settlement].price[good] ==
                     CcGoodDefinitionFor((CcGood)good)->base_price);
        }
    }
    CC_CHECK(restored.settlements[0].sheep_adults > 0);
    CC_CHECK(restored.settlements[0].sheep_lambs > 0);
    CC_CHECK(restored.settlements[0].sheep_condition == 88);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_MEAT] > 0);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_WOOL] > 0);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema28GrainMigration(char *error,
                                        size_t error_capacity)
{
    const char *path = "persistence-schema28-grain-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0xc0a7118e));
    legacy.schema_version = 28U;
    legacy.generator_version = 22U;
    legacy.player.cargo[CC_GOOD_BREAD] = 7;
    CcSituation *quest = &legacy.situations[0];
    quest->good = CC_GOOD_FOOD;
    int32_t quest_quantity = quest->quantity;
    int32_t quest_progress = quest->progress;
    CcSettlement *farm = &legacy.settlements[0];
    CcSettlement *market = &legacy.settlements[1];
    farm->production[CC_GOOD_BREAD] = 23;
    farm->production[CC_GOOD_WHEAT] = 0;
    market->service_mask &=
        ~(UINT32_C(1) << (uint32_t)CC_SERVICE_BAKERY);
    market->production[CC_GOOD_BREAD] = 0;
    int32_t map_x[CC_MAX_SETTLEMENTS];
    int32_t map_y[CC_MAX_SETTLEMENTS];
    int32_t wood_stock[CC_MAX_SETTLEMENTS];
    int32_t wood_reserve[CC_MAX_SETTLEMENTS];
    int32_t wood_production[CC_MAX_SETTLEMENTS];
    for (int32_t index = 0; index < legacy.settlement_count; ++index) {
        map_x[index] = legacy.settlements[index].map_x;
        map_y[index] = legacy.settlements[index].map_y;
        wood_stock[index] = legacy.settlements[index].stock[CC_GOOD_WOOD];
        wood_reserve[index] =
            legacy.settlements[index].reserve_target[CC_GOOD_WOOD];
        wood_production[index] =
            legacy.settlements[index].production[CC_GOOD_WOOD];
        legacy.settlements[index].sheep_adults = 0;
        legacy.settlements[index].sheep_lambs = 0;
        legacy.settlements[index].sheep_condition = 0;
        legacy.settlements[index].sheep_hunger = 0;
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.player.cargo[CC_GOOD_BREAD] == 7);
    CC_CHECK(restored.situations[0].good == CC_GOOD_BREAD);
    CC_CHECK(restored.situations[0].quantity == quest_quantity);
    CC_CHECK(restored.situations[0].progress == quest_progress);
    CC_CHECK(restored.settlements[0].production[CC_GOOD_BREAD] == 0);
    CC_CHECK(restored.settlements[0].production[CC_GOOD_WHEAT] == 60);
    CC_CHECK(CcSettlementHasService(
        &restored.settlements[1], CC_SERVICE_BAKERY));
    CC_CHECK(restored.settlements[1].production[CC_GOOD_BREAD] == 35);
    for (int32_t index = 0; index < restored.settlement_count; ++index) {
        CC_CHECK(restored.settlements[index].map_x == map_x[index]);
        CC_CHECK(restored.settlements[index].map_y == map_y[index]);
        CC_CHECK(restored.settlements[index].stock[CC_GOOD_WOOD] ==
                 wood_stock[index]);
        CC_CHECK(restored.settlements[index].reserve_target[CC_GOOD_WOOD] ==
                 wood_reserve[index]);
        CC_CHECK(restored.settlements[index].production[CC_GOOD_WOOD] ==
                 wood_production[index]);
    }
    CC_CHECK(restored.settlements[0].sheep_adults > 0);
    CC_CHECK(restored.settlements[0].sheep_lambs > 0);
    CC_CHECK(restored.settlements[0].sheep_condition == 88);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_MEAT] > 0);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_WOOL] > 0);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckFlockMigration(uint32_t schema_version,
                                uint32_t generator_version,
                                const char *path,
                                char *error, size_t error_capacity)
{
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0xf10cc29));
    legacy.schema_version = schema_version;
    legacy.generator_version = generator_version;
    int32_t wheat_stock = legacy.settlements[0].stock[CC_GOOD_WHEAT];
    int32_t wood_stock = legacy.settlements[0].stock[CC_GOOD_WOOD];
    int32_t bread_production =
        legacy.settlements[0].production[CC_GOOD_BREAD];
    for (int32_t index = 0; index < legacy.settlement_count; ++index) {
        legacy.settlements[index].sheep_adults = 0;
        legacy.settlements[index].sheep_lambs = 0;
        legacy.settlements[index].sheep_condition = 0;
        legacy.settlements[index].sheep_hunger = 0;
        legacy.settlements[index].stock[CC_GOOD_MEAT] = 0;
        legacy.settlements[index].stock[CC_GOOD_WOOL] = 0;
        legacy.settlements[index].reserve_target[CC_GOOD_MEAT] = 0;
        legacy.settlements[index].reserve_target[CC_GOOD_WOOL] = 0;
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.settlements[0].stock[CC_GOOD_WHEAT] == wheat_stock);
    CC_CHECK(restored.settlements[0].stock[CC_GOOD_WOOD] == wood_stock);
    CC_CHECK(restored.settlements[0].production[CC_GOOD_BREAD] ==
             bread_production);
    CC_CHECK(restored.settlements[0].sheep_adults > 0);
    CC_CHECK(restored.settlements[0].sheep_lambs > 0);
    CC_CHECK(restored.settlements[0].sheep_condition == 88);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_MEAT] > 0);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_WOOL] > 0);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema30JournalMigration(char *error,
                                          size_t error_capacity)
{
    const char *path = "persistence-schema30-journal-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x30a0c0de));
    legacy.schema_version = 30U;
    legacy.generator_version = 23U;
    legacy.current_day = 272;
    legacy.shipment_count = 0;
    legacy.dragon.hunt_cooldown_days = 1000;
    legacy.goblins.tribute_cooldown_days = 1000;
    legacy.hoard_raiders.cooldown_days = 1000;
    for (int32_t i = 0; i < legacy.settlement_count; ++i) {
        legacy.settlements[i].stock[CC_GOOD_MEAT] = 0;
        legacy.settlements[i].stock[CC_GOOD_WOOL] = 0;
        legacy.settlements[i].reserve_target[CC_GOOD_MEAT] = 0;
        legacy.settlements[i].reserve_target[CC_GOOD_WOOL] = 0;
        legacy.settlements[i].consumption[CC_GOOD_WOOL] = 0;
    }
    CcSettlement *farm = &legacy.settlements[0];
    farm->cow_adults = 3;
    farm->cow_calves = 0;
    farm->cow_condition = 50;
    farm->cow_hunger = 64;
    farm->stock[CC_GOOD_WHEAT] = 0;
    farm->stock[CC_GOOD_WOOL] = 10;
    int32_t meat_before = farm->stock[CC_GOOD_MEAT];
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim suffix = legacy;
    CcSimAdvanceDays(&suffix, 1);
    CC_CHECK(suffix.settlements[0].stock[CC_GOOD_MEAT] == meat_before);
    CC_CHECK(suffix.settlements[0].stock[CC_GOOD_WOOL] == 10);
    const CcEvent *slaughter = NULL;
    for (int32_t i = 0; i < suffix.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&suffix, i);
        if (event != NULL && event->kind == CC_EVENT_COW_SLAUGHTERED) {
            slaughter = event;
            break;
        }
    }
    CC_CHECK(slaughter != NULL);
    CC_CHECK(slaughter->kind == CC_EVENT_COW_SLAUGHTERED);
    CC_CHECK(slaughter->magnitude == 1);
    CC_CHECK(strstr(slaughter->text, "4 Food") != NULL);
    AddLegacyDayJournalSuffix(path, &legacy, &suffix, 30U, 23U);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.current_day == suffix.current_day);
    CC_CHECK(restored.settlements[0].sheep_adults > 0);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema30RoadDistrictMigration(char *error,
                                                size_t error_capacity)
{
    const char *path = "persistence-schema30-road-district-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0xc0a7119e));
    legacy.schema_version = 30U;
    legacy.generator_version = 23U;
    legacy.road_site_count = 0;
    memset(legacy.road_sites, 0, sizeof(legacy.road_sites));
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.road_site_count == CC_MAX_ROAD_SITES);
    for (int32_t route_slot = 0;
         route_slot < restored.route_count; ++route_slot) {
        const CcRoadSite *road_house = CcSimRoadHouseSite(
            &restored, restored.routes[route_slot].id);
        CC_CHECK(road_house != NULL);
        CC_CHECK(!road_house->accessible);
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema27WoodCompatibility(char *error,
                                           size_t error_capacity)
{
    const char *path = "persistence-legacy-v27-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac27));
    legacy.schema_version = 27U;
    legacy.generator_version = 21U;
    int32_t wheat_stock[CC_MAX_SETTLEMENTS];
    for (int32_t settlement = 0;
         settlement < legacy.settlement_count; ++settlement) {
        CcSettlement *place = &legacy.settlements[settlement];
        place->stock[CC_GOOD_WOOD] = 0;
        place->reserve_target[CC_GOOD_WOOD] = 0;
        place->production[CC_GOOD_WOOD] = 0;
        place->price[CC_GOOD_WOOD] =
            CcGoodDefinitionFor(CC_GOOD_WOOD)->base_price;
        place->stock[CC_GOOD_WHEAT] = 100 + settlement;
        place->price[CC_GOOD_WHEAT] = 20 + settlement;
        wheat_stock[settlement] = place->stock[CC_GOOD_WHEAT];
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    for (int32_t settlement = 0;
         settlement < restored.settlement_count; ++settlement) {
        const CcSettlement *place = &restored.settlements[settlement];
        CC_CHECK(place->stock[CC_GOOD_WOOD] > 0);
        CC_CHECK(place->reserve_target[CC_GOOD_WOOD] > 0);
        CC_CHECK(place->production[CC_GOOD_WOOD] > 0);
        CC_CHECK(place->price[CC_GOOD_WOOD] ==
                 CcGoodDefinitionFor(CC_GOOD_WOOD)->base_price);
        CC_CHECK(place->stock[CC_GOOD_WHEAT] == wheat_stock[settlement]);
        CC_CHECK(place->price[CC_GOOD_WHEAT] ==
                 CcGoodDefinitionFor(CC_GOOD_WHEAT)->base_price);
    }
    CC_CHECK(restored.settlements[0].sheep_adults > 0);
    CC_CHECK(restored.settlements[0].sheep_lambs > 0);
    CC_CHECK(restored.settlements[0].sheep_condition == 88);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_MEAT] > 0);
    CC_CHECK(restored.settlements[0].reserve_target[CC_GOOD_WOOL] > 0);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckSchema29StoneMigration(char *error,
                                        size_t error_capacity)
{
    const char *path = "persistence-schema29-stone-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac29));
    legacy.schema_version = 29U;
    legacy.generator_version = 23U;
    int32_t wood_stock[CC_MAX_SETTLEMENTS];
    int32_t wood_reserve[CC_MAX_SETTLEMENTS];
    int32_t wheat_stock[CC_MAX_SETTLEMENTS];
    int32_t wheat_price[CC_MAX_SETTLEMENTS];
    int32_t map_x[CC_MAX_SETTLEMENTS];
    int32_t map_y[CC_MAX_SETTLEMENTS];
    for (int32_t settlement = 0;
         settlement < legacy.settlement_count; ++settlement) {
        CcSettlement *place = &legacy.settlements[settlement];
        wood_stock[settlement] = place->stock[CC_GOOD_WOOD];
        wood_reserve[settlement] = place->reserve_target[CC_GOOD_WOOD];
        wheat_stock[settlement] = place->stock[CC_GOOD_WHEAT];
        wheat_price[settlement] = place->price[CC_GOOD_WHEAT];
        map_x[settlement] = place->map_x;
        map_y[settlement] = place->map_y;
        place->stock[CC_GOOD_STONE] = 0;
        place->reserve_target[CC_GOOD_STONE] = 0;
        place->production[CC_GOOD_STONE] = 0;
        place->consumption[CC_GOOD_STONE] = 0;
        place->price[CC_GOOD_STONE] =
            CcGoodDefinitionFor(CC_GOOD_STONE)->base_price;
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    for (int32_t settlement = 0;
         settlement < restored.settlement_count; ++settlement) {
        const CcSettlement *place = &restored.settlements[settlement];
        CC_CHECK(place->stock[CC_GOOD_STONE] > 0);
        CC_CHECK(place->reserve_target[CC_GOOD_STONE] > 0);
        CC_CHECK(place->production[CC_GOOD_STONE] ==
                 (place->function == CC_SETTLEMENT_MINING ? 12 : 0));
        CC_CHECK(place->stock[CC_GOOD_WOOD] == wood_stock[settlement]);
        CC_CHECK(place->reserve_target[CC_GOOD_WOOD] ==
                 wood_reserve[settlement]);
        CC_CHECK(place->stock[CC_GOOD_WHEAT] == wheat_stock[settlement]);
        CC_CHECK(place->price[CC_GOOD_WHEAT] == wheat_price[settlement]);
        CC_CHECK(place->map_x == map_x[settlement]);
        CC_CHECK(place->map_y == map_y[settlement]);
        CC_CHECK(place->stock[CC_GOOD_PAPER] > 0);
        CC_CHECK(place->reserve_target[CC_GOOD_PAPER] > 0);
        CC_CHECK(place->price[CC_GOOD_PAPER] ==
                 CcGoodDefinitionFor(CC_GOOD_PAPER)->base_price);
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckPaperCompatibility(uint32_t schema_version,
                                    uint32_t generator_version,
                                    const char *path,
                                    char *error, size_t error_capacity)
{
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac30));
    legacy.schema_version = schema_version;
    legacy.generator_version = generator_version;
    for (int32_t settlement = 0;
         settlement < legacy.settlement_count; ++settlement) {
        CcSettlement *place = &legacy.settlements[settlement];
        place->stock[CC_GOOD_PAPER] = 0;
        place->reserve_target[CC_GOOD_PAPER] = 0;
        place->production[CC_GOOD_PAPER] = 0;
        place->consumption[CC_GOOD_PAPER] = 0;
        place->price[CC_GOOD_PAPER] = 0;
    }
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM player_good;") == 11);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    for (int32_t settlement = 0;
         settlement < restored.settlement_count; ++settlement) {
        const CcSettlement *place = &restored.settlements[settlement];
        CC_CHECK(place->stock[CC_GOOD_PAPER] > 0);
        CC_CHECK(place->reserve_target[CC_GOOD_PAPER] > 0);
        CC_CHECK(place->price[CC_GOOD_PAPER] ==
                 CcGoodDefinitionFor(CC_GOOD_PAPER)->base_price);
        CC_CHECK(place->stock[CC_GOOD_ROTTEN_MEAT] == 0);
        CC_CHECK(place->stock[CC_GOOD_ROTTEN_GRAIN] == 0);
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckMaterialChainMigration(char *error, size_t error_capacity)
{
    const char *path = "persistence-schema33-material-chain-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x33a7e211));
    legacy.schema_version = 33U;
    legacy.generator_version = 25U;
    legacy.current_day = 6;
    CcSimInitializePaperEconomy(&legacy);
    legacy.archives.kit_tool_wear = 0;
    for (int32_t kingdom = 0; kingdom < legacy.kingdom_count; ++kingdom) {
        legacy.kingdoms[kingdom].sanction = 0;
        legacy.kingdoms[kingdom].unsanctioned_weeks = 0;
        legacy.kingdoms[kingdom].pretender_crises = 0;
        legacy.kingdoms[kingdom].anointed = false;
    }
    for (int32_t settlement = 0;
         settlement < legacy.settlement_count; ++settlement) {
        CcSettlement *place = &legacy.settlements[settlement];
        place->service_mask &=
            ~(UINT32_C(1) << (uint32_t)CC_SERVICE_MILL);
        place->paper_tool_wear = 0;
        if (place->function == CC_SETTLEMENT_FARMING) {
            place->production[CC_GOOD_WHEAT] = 28;
        }
    }
    CcSettlement *legacy_mill = &legacy.settlements[1];
    legacy_mill->stock[CC_GOOD_PAPER] = 0;
    legacy_mill->stock[CC_GOOD_WOOD] =
        legacy_mill->reserve_target[CC_GOOD_WOOD] + 2;
    legacy_mill->stock[CC_GOOD_GOLD] = 100;
    legacy_mill->stock[CC_GOOD_GEMS] = 100;
    legacy_mill->consumption[CC_GOOD_GOLD] = 0;
    legacy_mill->consumption[CC_GOOD_GEMS] = 0;
    int32_t event_slot = legacy.event_write_index;
    legacy.events[event_slot] = (CcEvent){
        .id = CcMakeId(CC_ENTITY_EVENT, legacy.next_entity_serial++),
        .day = legacy.current_day,
        .kind = CC_EVENT_KINGDOM_ACTION,
        .subject_id = legacy.kingdoms[0].id,
        .location_id = legacy_mill->id,
        .magnitude = 40
    };
    (void)snprintf(legacy.events[event_slot].text,
                   sizeof(legacy.events[event_slot].text),
                   "A ruler makes a lasting public vow.");
    legacy.event_write_index = (event_slot + 1) % CC_MAX_EVENTS;
    if (legacy.event_count < CC_MAX_EVENTS) legacy.event_count += 1;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim suffix = legacy;
    CcSimAdvanceDays(&suffix, 1);
    CC_CHECK(suffix.current_day == 7);
    CC_CHECK(suffix.settlements[1].stock[CC_GOOD_PAPER] > 0);
    CC_CHECK(suffix.archives.lore_stored > legacy.archives.lore_stored);
    CC_CHECK(suffix.treasure_count > legacy.treasure_count);
    AddLegacyDayJournalSuffix(path, &legacy, &suffix, 33U, 25U);

    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open_v2(
                      path, &database, SQLITE_OPEN_READWRITE, NULL),
                  database, "could not open material chain fixture");
    ExecuteFixtureSql(
        database,
        "ALTER TABLE meta DROP COLUMN archive_kit_tool_wear;"
        "ALTER TABLE kingdom DROP COLUMN sanction;"
        "ALTER TABLE kingdom DROP COLUMN unsanctioned_weeks;"
        "ALTER TABLE kingdom DROP COLUMN pretender_crises;"
        "ALTER TABLE kingdom DROP COLUMN anointed;"
        "ALTER TABLE material_economy DROP COLUMN paper_tool_wear;"
        "PRAGMA user_version=23;",
        "could not strip material chain columns");
    sqlite3_close(database);

    CcSim restored;
    bool loaded = CcSaveRead(path, &restored, error, error_capacity);
    if (!loaded) {
        (void)fprintf(stderr, "material chain migration: %s\n", error);
    }
    CC_CHECK(loaded);
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.current_day == suffix.current_day);
    CC_CHECK(ReadSqliteInteger(path, "PRAGMA user_version;") == 23);
    CC_CHECK(restored.archives.kit_tool_wear == 0);
    for (int32_t kingdom = 0;
         kingdom < restored.kingdom_count; ++kingdom) {
        CC_CHECK(restored.kingdoms[kingdom].sanction ==
                 restored.kingdoms[kingdom].legitimacy);
        CC_CHECK(restored.kingdoms[kingdom].unsanctioned_weeks == 0);
        CC_CHECK(restored.kingdoms[kingdom].pretender_crises == 0);
        CC_CHECK(!restored.kingdoms[kingdom].anointed);
    }
    for (int32_t settlement = 0;
         settlement < restored.settlement_count; ++settlement) {
        const CcSettlement *place = &restored.settlements[settlement];
        bool paper_town = place->function == CC_SETTLEMENT_MARKET ||
                          place->function == CC_SETTLEMENT_CAPITAL;
        CC_CHECK(CcSettlementHasService(place, CC_SERVICE_MILL) ==
                 paper_town);
        CC_CHECK(place->production[CC_GOOD_PAPER] ==
                 (place->function == CC_SETTLEMENT_MARKET ? 3 :
                  place->function == CC_SETTLEMENT_CAPITAL ? 4 : 0));
        CC_CHECK(place->paper_tool_wear == 0);
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckArchivePhysicalLoreMigration(char *error,
                                              size_t error_capacity)
{
    const char *path = "persistence-schema35-archive-lore-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0xa4c417e));
    legacy.current_day = 6;
    legacy.iron_ledger_reserve = 50;
    legacy.archives.scribes = 1;
    CcSettlement *scriptorium = &legacy.settlements[1];
    scriptorium->stock[CC_GOOD_WHEAT] = 100;
    scriptorium->stock[CC_GOOD_PAPER] = 1;
    scriptorium->stock[CC_GOOD_TOOLS] = 1;
    for (int32_t i = 0; i < legacy.event_count; ++i) {
        legacy.events[i].magnitude = 0;
    }
    legacy.events[0].day = 6;
    legacy.events[0].kind = CC_EVENT_KINGDOM_ACTION;
    legacy.events[0].magnitude = 40;
    CcSimAdvanceDays(&legacy, 1);
    CC_CHECK(CcSimArchivePhysicalLore(&legacy) == 1);

    legacy.schema_version = 35U;
    legacy.generator_version = 25U;
    legacy.archives.lore_stored = 0;
    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.archives.lore_stored == 1);
    CC_CHECK(CcSimArchivePhysicalLore(&restored) == 1);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckWoodPaperJournalMigration(char *error,
                                           size_t error_capacity)
{
    char fixture[512];
    (void)snprintf(
        fixture, sizeof(fixture),
        "%s/tests/fixtures/shipped/schema-36-generator-25-paper-journal.ccsave",
        CC_TEST_SOURCE_DIR);
    CC_CHECK(ReadSqliteInteger(
        fixture, "SELECT schema_version FROM meta WHERE id=1;") == 36);
    CC_CHECK(ReadSqliteInteger(
        fixture, "SELECT COUNT(*) FROM action_journal;") == 1);

    CcSim restored;
    CC_CHECK(CcSaveRead(fixture, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.current_day == 7);
    CC_CHECK(restored.settlements[1].stock[CC_GOOD_WHEAT] == 98);
    /* The replay produces 22 wood in transit. Migration completes this
       extra load because the Crown carriage already holds its food load. */
    CC_CHECK(restored.settlements[1].stock[CC_GOOD_WOOD] == 22);
    CC_CHECK(restored.shipments[1].good == CC_GOOD_WOOD);
    CC_CHECK(restored.shipments[1].quantity == 22);
    CC_CHECK(restored.shipments[1].status == CC_SHIPMENT_ARRIVED);
    CC_CHECK(restored.settlements[1].stock[CC_GOOD_PAPER] == 8);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));

    /* Captured with main 9ab3a56 before changing the paper recipe. */
    CcSim legacy_view = restored;
    legacy_view.schema_version = 36U;
    legacy_view.next_entity_serial -= (uint64_t)restored.kingdom_count;
    legacy_view.settlements[1].stock[CC_GOOD_WOOD] -= 22;
    legacy_view.shipments[1].status = CC_SHIPMENT_TRAVELLING;
    CC_CHECK(CcSimHash(&legacy_view) == UINT64_C(0x8e390ecaf46cc546));
    CC_CHECK(ReadSqliteInteger(
        fixture, "SELECT schema_version FROM meta WHERE id=1;") == 36);

    const char *path = "persistence-wood-paper-upgrade-test.ccsave";
    RemoveDatabase(path);
    CC_CHECK(CcSaveWrite(path, &restored, error, error_capacity));
    CcSim round_trip;
    CC_CHECK(CcSaveRead(path, &round_trip, error, error_capacity));
    CC_CHECK(CcSimHash(&round_trip) == CcSimHash(&restored));
    RemoveDatabase(path);
}

static void CheckJourneyStopPersistence(char *error, size_t error_capacity)
{
    const char *path = "persistence-journey-stop-test.ccsave";
    RemoveDatabase(path);
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x570900));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[1].id
    };
    CC_CHECK(CcSimApply(&sim, &travel, error, error_capacity));
    sim.journey.ambush_pending = false;
    while (sim.journey.active) {
        CcSimAdvanceRuntimeTicks(&sim, CC_WORLD_TICKS_PER_SECOND);
    }
    travel.target_id = sim.settlements[0].id;
    CC_CHECK(CcSimApply(&sim, &travel, error, error_capacity));
    sim.journey.ambush_pending = false;
    while (sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(&sim, CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(CcSimJourneyStop(&sim) == CC_JOURNEY_STOP_MIDDAY);
    CC_CHECK(CcSaveWrite(path, &sim, error, error_capacity));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.journey.active);
    CC_CHECK(restored.journey.phase == CC_JOURNEY_PHASE_RESTING);
    CC_CHECK(CcSimJourneyStop(&restored) == CC_JOURNEY_STOP_MIDDAY);
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

static void CheckLegacyLifecycleJournalCompatibility(
    uint32_t schema_version, char *error, size_t error_capacity)
{
    char path[64];
    (void)snprintf(path, sizeof(path),
                   "persistence-legacy-lifecycle-v%u-test.ccsave",
                   schema_version);
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x11fec000) ^ schema_version);
    legacy.schema_version = schema_version;
    legacy.generator_version = 20U;
    ClearLegacyCharacterLifecycles(&legacy);
    CcId first_character_id = legacy.characters[0].id;
    uint64_t first_unused_serial = legacy.next_entity_serial;

    CcSim suffix = legacy;
    for (int32_t i = 0; i < suffix.character_count; ++i) {
        suffix.characters[i].death_day = CC_SIM_MAX_DAY;
    }
    CcSimAdvanceDays(&suffix, 1);
    ClearLegacyCharacterLifecycles(&suffix);
    CC_CHECK(suffix.characters[0].id == first_character_id);
    CC_CHECK(suffix.next_entity_serial == first_unused_serial);

    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    AddLegacyDayJournalSuffix(path, &legacy, &suffix,
                              schema_version, 20U);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(restored.current_day == suffix.current_day);
    CC_CHECK(restored.characters[0].id == first_character_id);
    CC_CHECK(restored.next_entity_serial ==
             first_unused_serial + (uint64_t)restored.kingdom_count);
    CC_CHECK(restored.character_births == 0);
    CC_CHECK(restored.character_deaths == 0);
    for (int32_t i = 0; i < restored.character_count; ++i) {
        CC_CHECK(restored.characters[i].death_day > restored.current_day);
    }
    CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    RemoveDatabase(path);
}

typedef struct ShippedSaveFixture {
    uint32_t schema_version;
    uint32_t generator_version;
} ShippedSaveFixture;

static void CheckShippedSaveCompatibility(char *error,
                                          size_t error_capacity)
{
    static const ShippedSaveFixture fixtures[] = {
        {2U, 2U}, {3U, 3U}, {4U, 3U}, {5U, 4U}, {9U, 9U},
        {11U, 11U}, {12U, 12U}, {13U, 13U}, {14U, 14U},
        {15U, 15U}, {16U, 15U}, {17U, 16U}, {18U, 16U},
        {18U, 17U}, {19U, 18U}, {20U, 19U}, {21U, 20U},
        {22U, 20U}, {23U, 20U}, {24U, 20U}, {25U, 20U},
        {26U, 21U}, {27U, 21U}, {27U, 22U}, {28U, 22U},
        {29U, 23U}, {30U, 23U}, {31U, 24U}, {32U, 25U},
        {33U, 25U}, {34U, 25U}, {35U, 25U}
    };
    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        const ShippedSaveFixture *fixture = &fixtures[i];
        char file[512];
        (void)snprintf(
            file, sizeof(file),
            "%s/tests/fixtures/shipped/schema-%u-generator-%u.ccsave",
            CC_TEST_SOURCE_DIR, fixture->schema_version,
            fixture->generator_version);
        CC_CHECK(ReadSqliteInteger(
                     file, "SELECT schema_version FROM meta WHERE id=1;") ==
                 (int64_t)fixture->schema_version);
        CC_CHECK(ReadSqliteInteger(
                     file, "SELECT generator_version FROM meta WHERE id=1;") ==
                 (int64_t)fixture->generator_version);
        int64_t kingdom_count = ReadSqliteInteger(
            file, "SELECT kingdom_count FROM meta WHERE id=1;");
        int64_t settlement_count = ReadSqliteInteger(
            file, "SELECT settlement_count FROM meta WHERE id=1;");
        int64_t route_count = ReadSqliteInteger(
            file, "SELECT route_count FROM meta WHERE id=1;");
        int64_t player_location = ReadSqliteInteger(
            file, "SELECT location_id FROM player_company LIMIT 1;");

        CcSim restored;
        if (!CcSaveRead(file, &restored, error, error_capacity)) {
            (void)fprintf(stderr,
                          "could not load shipped schema %u / generator %u: %s\n",
                          fixture->schema_version, fixture->generator_version,
                          error);
            CC_CHECK(false);
        }
        CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
        CC_CHECK(restored.generator_version == CC_GENERATOR_VERSION);
        CC_CHECK(restored.world_seed == 42U);
        CC_CHECK(restored.current_day == 366);
        CC_CHECK(restored.kingdom_count == kingdom_count);
        CC_CHECK(restored.settlement_count == settlement_count);
        CC_CHECK(restored.route_count == route_count);
        CC_CHECK(restored.player.location_id == (CcId)player_location);
        CC_CHECK(CcSimValidate(&restored, error, error_capacity));
    }

    char decay_file[512];
    (void)snprintf(decay_file, sizeof(decay_file),
                   "%s/tests/fixtures/shipped/schema-35-generator-25-decay-journal.ccsave",
                   CC_TEST_SOURCE_DIR);
    CcSim decayed;
    CC_CHECK(CcSaveRead(decay_file, &decayed, error, error_capacity));
    CC_CHECK(decayed.current_day == 373);
    CC_CHECK(decayed.archives.lore_stored == CcSimArchivePhysicalLore(&decayed));
    CC_CHECK(CcSimValidate(&decayed, error, error_capacity));

    char journal_file[512];
    (void)snprintf(
        journal_file, sizeof(journal_file),
        "%s/tests/fixtures/shipped/"
        "schema-21-generator-20-weekly-journal.ccsave",
        CC_TEST_SOURCE_DIR);
    CcSim replayed;
    if (!CcSaveRead(journal_file, &replayed, error, error_capacity)) {
        (void)fprintf(stderr, "could not replay shipped weekly journal: %s\n",
                      error);
        CC_CHECK(false);
    }
    CC_CHECK(replayed.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(replayed.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(replayed.current_day == 7);
    CC_CHECK(CcSimValidate(&replayed, error, error_capacity));

    (void)snprintf(
        journal_file, sizeof(journal_file),
        "%s/tests/fixtures/shipped/"
        "schema-21-generator-20-weekly-runtime-journal.ccsave",
        CC_TEST_SOURCE_DIR);
    if (!CcSaveRead(journal_file, &replayed, error, error_capacity)) {
        (void)fprintf(stderr,
                      "could not replay shipped weekly runtime journal: %s\n",
                      error);
        CC_CHECK(false);
    }
    CC_CHECK(replayed.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(replayed.generator_version == CC_GENERATOR_VERSION);
    CC_CHECK(replayed.current_day == 7);
    CC_CHECK(replayed.clock.tick == 6U);
    CC_CHECK(CcSimValidate(&replayed, error, error_capacity));
}

static void CheckDragonHairPersistence(void)
{
    const char *path = "dragon-hair-colours.ccsave";
    CcSim court, restored;
    char error[256];
    CcSimInit(&court, 42U);
    CC_CHECK(court.dragon.hair_color == CC_DRAGON_HAIR_PURPLE);
    uint64_t previous = CcSimHash(&court);
    for (int32_t hair = CC_DRAGON_HAIR_RED; hair <= CC_DRAGON_HAIR_BLUE; ++hair) {
        court.dragon.hair_color = (CcDragonHairColor)hair;
        CC_CHECK(CcSimHash(&court) != previous);
        previous = CcSimHash(&court);
        CC_CHECK(CcSaveWrite(path, &court, error, sizeof(error)));
        CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
        CC_CHECK(restored.dragon.hair_color == court.dragon.hair_color);
        CC_CHECK(CcSimHash(&restored) == previous);
    }
    court.dragon.hair_color = CC_DRAGON_HAIR_COLOR_COUNT;
    CC_CHECK(!CcSimValidate(&court, error, sizeof(error)));
    court.dragon.hair_color = CC_DRAGON_HAIR_PURPLE;
    court.schema_version = 42U;
    CC_CHECK(CcSaveWrite(path, &court, error, sizeof(error)));
    sqlite3 *database = NULL;
    RequireSqlite(sqlite3_open(path, &database), database, "open old court save");
    RequireSqlite(sqlite3_exec(database,
        "ALTER TABLE dragon_state DROP COLUMN hair_color; PRAGMA user_version=26;",
        NULL, NULL, NULL), database, "restore old dragon table");
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.dragon.hair_color == CC_DRAGON_HAIR_PURPLE);
    court.schema_version = CC_SIM_SCHEMA_VERSION;
    CC_CHECK(CcSimHash(&court) == CcSimHash(&restored));
    RemoveDatabase(path);
}

static void CheckSchema41Upgrade(void)
{
    const char *path = "schema41-upgrade.ccsave";
    CcSim legacy, restored;
    char error[256];
    CcSimInit(&legacy, 42U);
    legacy.schema_version = 41U;
    CcSimAdvanceDays(&legacy, 7);
    CC_CHECK(CcSaveWrite(path, &legacy, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    legacy.schema_version = CC_SIM_SCHEMA_VERSION;
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&legacy));
    RemoveDatabase(path);
}

int main(void)
{
    CheckDragonHairPersistence();
    CheckSchema41Upgrade();
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
    CheckShippedSaveCompatibility(error, sizeof(error));
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
    CheckSchema12CommandKindCompatibility(error, sizeof(error));
    CheckSchema13Compatibility(error, sizeof(error));
    CheckSchema14Compatibility(error, sizeof(error));
    CheckSchema15Compatibility(error, sizeof(error));
    CheckSchema16Compatibility(error, sizeof(error));
    CheckSchema17Compatibility(error, sizeof(error));
    CheckSchema18QuestCompatibility(error, sizeof(error));
    CheckSchema18Compatibility(error, sizeof(error));
    CheckSchema21Compatibility(error, sizeof(error));
    CheckSchema22Compatibility(error, sizeof(error));
    CheckSchema23Compatibility(error, sizeof(error));
    CheckSchema24Compatibility(error, sizeof(error));
    CheckSchema25Compatibility(error, sizeof(error));
    CheckSchema26Compatibility(error, sizeof(error));
    CheckSchema27WoodCompatibility(error, sizeof(error));
    CheckSchema28GrainMigration(error, sizeof(error));
    CheckSchema29StoneMigration(error, sizeof(error));
    CheckSchema30RoadDistrictMigration(error, sizeof(error));
    CheckFlockMigration(30U, 23U,
                        "persistence-schema30-flock-test.ccsave",
                        error, sizeof(error));
    CheckFlockMigration(31U, 24U,
                        "persistence-schema31-flock-test.ccsave",
                        error, sizeof(error));
    CheckSchema30JournalMigration(error, sizeof(error));
    CheckPaperCompatibility(30U, 23U,
                            "persistence-schema30-paper-test.ccsave",
                            error, sizeof(error));
    CheckPaperCompatibility(32U, 25U,
                            "persistence-schema32-paper-test.ccsave",
                            error, sizeof(error));
    CheckMaterialChainMigration(error, sizeof(error));
    CheckArchivePhysicalLoreMigration(error, sizeof(error));
    CheckWoodPaperJournalMigration(error, sizeof(error));
    CheckJourneyStopPersistence(error, sizeof(error));
    CheckLegacyLifecycleJournalCompatibility(24U, error, sizeof(error));
    CheckLegacyLifecycleJournalCompatibility(25U, error, sizeof(error));
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
    original.journey.ambush_pending = false;
    original.journey.situation_id = 0U;
    CompleteJourney(&original, error, sizeof(error));
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
    original.settlements[1].stock[CC_GOOD_WHEAT] += 100;
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
    if (original.bandits[0].raid_phase == CC_BANDIT_RAID_IDLE) {
        CC_CHECK(CcSimLaunchBanditRaid(&original, original.bandits[0].id,
                                       error, sizeof(error)));
    }
    CC_CHECK(original.bandits[0].raid_phase != CC_BANDIT_RAID_IDLE);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        original.player.cargo[good] = 0;
    }
    for (int32_t good = CC_LEGACY_GOOD_COUNT;
         good < CC_GOOD_COUNT; ++good) {
        original.player.cargo[good] = 1;
        original.goblins.carried_goods[good] = 0;
        original.goblins.lair_stock[good] = good + 2;
        original.dragon.hoard_goods[good] = good + 3;
        original.dragon_campaign.supplies[good] = good + 4;
        for (int32_t settlement = 0;
             settlement < original.settlement_count; ++settlement) {
            CcSettlement *place = &original.settlements[settlement];
            place->stock[good] = 10 + settlement + good;
            place->reserve_target[good] = 20 + settlement + good;
            place->production[good] = settlement + 1;
            place->consumption[good] = settlement;
            place->price[good] =
                CcGoodDefinitionFor((CcGood)good)->base_price + settlement;
        }
    }
    original.archives.kit_tool_wear = 3;
    original.kingdoms[0].sanction = 67;
    original.kingdoms[0].unsanctioned_weeks = 9;
    original.kingdoms[0].pretender_crises = 2;
    original.kingdoms[0].anointed = true;
    original.settlements[0].paper_tool_wear = 4;
    uint64_t expected = CcSimHash(&original);
    CC_CHECK(CcSaveWrite(path, &original, error, sizeof(error)));
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM settlement_good;") ==
             original.settlement_count * CC_GOOD_COUNT);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM player_good;") ==
             CC_GOOD_COUNT);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM goblin_good;") ==
             CC_GOOD_COUNT);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM dragon_good;") ==
             CC_GOOD_COUNT);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT COUNT(*) FROM dragon_campaign_good;") ==
             CC_GOOD_COUNT);

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == expected);
    CC_CHECK(restored.current_day == original.current_day);
    CC_CHECK(restored.iron_ledger_reserve ==
             original.iron_ledger_reserve);
    CC_CHECK(restored.kingdoms[0].iron_ledger_debt ==
             original.kingdoms[0].iron_ledger_debt);
    CC_CHECK(restored.archives.kit_tool_wear == 3);
    CC_CHECK(restored.kingdoms[0].sanction == 67);
    CC_CHECK(restored.kingdoms[0].unsanctioned_weeks == 9);
    CC_CHECK(restored.kingdoms[0].pretender_crises == 2);
    CC_CHECK(restored.kingdoms[0].anointed);
    CC_CHECK(restored.settlements[0].paper_tool_wear == 4);
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
    CC_CHECK(restored.bandits[0].service_mask ==
             original.bandits[0].service_mask);
    CC_CHECK(restored.bandits[0].raid_phase == original.bandits[0].raid_phase);
    CC_CHECK(restored.bandits[0].raid_target_id ==
             original.bandits[0].raid_target_id);
    CC_CHECK(restored.bandits[0].raid_good == original.bandits[0].raid_good);
    CC_CHECK(restored.bandits[0].raid_quantity ==
             original.bandits[0].raid_quantity);
    CC_CHECK(restored.bandits[0].raid_days_remaining ==
             original.bandits[0].raid_days_remaining);
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
    CC_CHECK(restored.road_site_count == original.road_site_count);
    for (int32_t site = 0; site < original.road_site_count; ++site) {
        CC_CHECK(restored.road_sites[site].id ==
                 original.road_sites[site].id);
        CC_CHECK(restored.road_sites[site].route_id ==
                 original.road_sites[site].route_id);
        CC_CHECK(strcmp(restored.road_sites[site].name,
                        original.road_sites[site].name) == 0);
        CC_CHECK(restored.road_sites[site].blocker ==
                 original.road_sites[site].blocker);
        CC_CHECK(!restored.road_sites[site].accessible);
    }
    for (int32_t good = CC_LEGACY_GOOD_COUNT;
         good < CC_GOOD_COUNT; ++good) {
        CC_CHECK(restored.player.cargo[good] ==
                 original.player.cargo[good]);
        CC_CHECK(restored.goblins.carried_goods[good] ==
                 original.goblins.carried_goods[good]);
        CC_CHECK(restored.goblins.lair_stock[good] ==
                 original.goblins.lair_stock[good]);
        CC_CHECK(restored.dragon.hoard_goods[good] ==
                 original.dragon.hoard_goods[good]);
        CC_CHECK(restored.dragon_campaign.supplies[good] ==
                 original.dragon_campaign.supplies[good]);
        for (int32_t settlement = 0;
             settlement < original.settlement_count; ++settlement) {
            CC_CHECK(restored.settlements[settlement].stock[good] ==
                     original.settlements[settlement].stock[good]);
            CC_CHECK(restored.settlements[settlement].reserve_target[good] ==
                     original.settlements[settlement].reserve_target[good]);
            CC_CHECK(restored.settlements[settlement].production[good] ==
                     original.settlements[settlement].production[good]);
            CC_CHECK(restored.settlements[settlement].consumption[good] ==
                     original.settlements[settlement].consumption[good]);
            CC_CHECK(restored.settlements[settlement].price[good] ==
                     original.settlements[settlement].price[good]);
        }
    }

    RemoveDatabase(path);
    puts("SQLite persistence tests passed");
    return 0;
}
