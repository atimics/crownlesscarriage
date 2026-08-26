#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
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

static void CheckSchema4Compatibility(char *error, size_t error_capacity)
{
    const char *path = "persistence-legacy-v4-test.ccsave";
    RemoveDatabase(path);
    CcSim legacy;
    CcSimInit(&legacy, UINT32_C(0x1e9ac5));
    CcSimAdvanceDays(&legacy, 5);
    legacy.schema_version = 4U;
    CcSim migrated = legacy;
    migrated.schema_version = CC_SIM_SCHEMA_VERSION;

    CC_CHECK(CcSaveWrite(path, &legacy, error, error_capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, error_capacity));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&migrated));
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
                 path, "SELECT journal_cursor FROM meta WHERE id=1;") == 1);
    CC_CHECK(ReadSqliteInteger(
                 path, "SELECT MAX(ordinal) FROM action_journal;") == 2);
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
    for (int32_t i = 0; i < restored.situation_count; ++i) {
        CC_CHECK(restored.situations[i].sponsor_name[0] == '\0');
        CC_CHECK(restored.situations[i].affected_name[0] == '\0');
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
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = situation->id
    };
    CC_CHECK(CcSimApply(&legacy_journey, &accept,
                        error, error_capacity));
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

int main(void)
{
    const char *path = "persistence-test.ccsave";
    RemoveDatabase(path);

    CcSim original;
    CcSimInit(&original, UINT32_C(0xa11ce5ed));
    CcSimAdvanceDays(&original, 23);
    char error[256];
    CcSettlement *capital = &original.settlements[4];
    capital->stock[CC_GOOD_MATERIAL] += 20;
    capital->stock[CC_GOOD_TOOLS] += 10;
    original.kingdoms[2].treasury += 100;
    CC_CHECK(CcSimStartServiceProject(&original, capital->id,
                                      CC_SERVICE_GRANARY,
                                      error, sizeof(error)));
    CheckPreJourneySchema3Compatibility(error, sizeof(error));
    CheckSchema4Compatibility(error, sizeof(error));
    CheckJournalRecovery(error, sizeof(error));
    CheckJournalCheckpointAndTamper(error, sizeof(error));
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
        if (original.situations[i].status == CC_SITUATION_ACTIVE) {
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
    uint64_t expected = CcSimHash(&original);
    CC_CHECK(CcSaveWrite(path, &original, error, sizeof(error)));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == expected);
    CC_CHECK(restored.current_day == original.current_day);
    CC_CHECK(restored.player.location_id == original.player.location_id);
    CC_CHECK(restored.player.map_capacity == original.player.map_capacity);
    CC_CHECK(restored.player.accepted_situation_id ==
             original.player.accepted_situation_id);
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
    CC_CHECK(restored.maps[0].owner_id == original.maps[0].owner_id);
    CC_CHECK(restored.maps[0].recorded_danger ==
             original.maps[0].recorded_danger);
    CC_CHECK(restored.event_count == original.event_count);

    RemoveDatabase(path);
    puts("SQLite persistence tests passed");
    return 0;
}
