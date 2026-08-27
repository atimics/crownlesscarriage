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
    CheckPreJourneySchema3Compatibility(error, sizeof(error));
    CheckSchema4Compatibility(error, sizeof(error));
    CheckSchema5Compatibility(error, sizeof(error));
    CheckSchema6Compatibility(error, sizeof(error));
    CheckSchema8Compatibility(error, sizeof(error));
    CheckSchema10Compatibility(error, sizeof(error));
    CheckDiplomacyPersistence(error, sizeof(error));
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
    CC_CHECK(restored.iron_ledger_reserve ==
             original.iron_ledger_reserve);
    CC_CHECK(restored.kingdoms[0].iron_ledger_debt ==
             original.kingdoms[0].iron_ledger_debt);
    CC_CHECK(restored.goblins.hoard_defenses == 4);
    CC_CHECK(restored.hoard_raiders.social_raid_latched);
    CC_CHECK(restored.hoard_raiders.war_raid_latched);
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
