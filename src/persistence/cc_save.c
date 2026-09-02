#include "persistence/cc_save.h"

#include <sqlite3.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CC_SQLITE_APPLICATION_ID 1128481362
#define CC_SQLITE_USER_VERSION 20
#define CC_JOURNAL_RECORD_VERSION 1
#define CC_JOURNAL_RUNTIME_FLUSH_TICKS 6
#define CC_JOURNAL_MAX_DAY_ADVANCE 3650
#define CC_JOURNAL_MAX_RUNTIME_ADVANCE 3600
#define CC_JOURNAL_COMPACT_RECORDS UINT64_C(4096)

typedef enum CcJournalOperationKind {
    CC_JOURNAL_OPERATION_COMMAND = 1,
    CC_JOURNAL_OPERATION_ADVANCE_DAYS = 2,
    CC_JOURNAL_OPERATION_ADVANCE_RUNTIME_TICKS = 3
} CcJournalOperationKind;

struct CcJournal {
    sqlite3 *database;
    uint64_t generation;
    uint64_t last_ordinal;
    int32_t pending_runtime_ticks;
    CcSim pending_runtime_base;
};

static bool Prepare(sqlite3 *database, const char *sql,
                    sqlite3_stmt **statement,
                    char *error, size_t error_capacity);

static void SetError(char *error, size_t capacity, const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message);
}
static void SetSqlError(char *error, size_t capacity, sqlite3 *database,
                        const char *context)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s: %s", context,
                   database != NULL ? sqlite3_errmsg(database) : "SQLite error");
}

static bool ReadTextColumn(sqlite3_stmt *statement, int column,
                           char *destination, size_t destination_capacity,
                           const char *field,
                           char *error, size_t error_capacity)
{
    if (statement == NULL || destination == NULL ||
        destination_capacity == 0U ||
        sqlite3_column_type(statement, column) != SQLITE_TEXT) {
        if (error != NULL && error_capacity > 0U) {
            (void)snprintf(error, error_capacity,
                           "Campaign %s text is invalid.", field);
        }
        return false;
    }
    const unsigned char *value = sqlite3_column_text(statement, column);
    int length = sqlite3_column_bytes(statement, column);
    if (value == NULL || length < 0 ||
        (size_t)length >= destination_capacity ||
        memchr(value, '\0', (size_t)length) != NULL) {
        if (error != NULL && error_capacity > 0U) {
            (void)snprintf(error, error_capacity,
                           "Campaign %s text is invalid.", field);
        }
        return false;
    }
    memcpy(destination, value, (size_t)length);
    destination[length] = '\0';
    return true;
}

static bool ParseStoredHash(const unsigned char *text, uint64_t *hash)
{
    if (text == NULL || hash == NULL) return false;
    const char *value = (const char *)text;
    int consumed = 0;
    return strlen(value) == 16U &&
           sscanf(value, "%16" SCNx64 "%n", hash, &consumed) == 1 &&
           consumed == 16;
}

static bool Execute(sqlite3 *database, const char *sql,
                    char *error, size_t error_capacity)
{
    char *sqlite_error = NULL;
    int result = sqlite3_exec(database, sql, NULL, NULL, &sqlite_error);
    if (result == SQLITE_OK) return true;
    if (error != NULL && error_capacity > 0U) {
        (void)snprintf(error, error_capacity, "SQLite: %s",
                       sqlite_error != NULL ? sqlite_error : sqlite3_errmsg(database));
    }
    sqlite3_free(sqlite_error);
    return false;
}

static bool ColumnExists(sqlite3 *database, const char *table,
                         const char *column, bool *exists,
                         char *error, size_t error_capacity)
{
    char sql[96];
    (void)snprintf(sql, sizeof(sql), "PRAGMA table_info(%s);", table);
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, sql, &statement, error, error_capacity)) return false;
    *exists = false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(statement, 1);
        if (name != NULL && strcmp((const char *)name, column) == 0) {
            *exists = true;
            break;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool EnsureColumn(sqlite3 *database, const char *table,
                         const char *column, const char *alter_sql,
                         char *error, size_t error_capacity)
{
    bool exists = false;
    return ColumnExists(database, table, column, &exists,
                        error, error_capacity) &&
        (exists || Execute(database, alter_sql, error, error_capacity));
}

static bool EnsureRealmColumns(sqlite3 *database,
                               char *error, size_t error_capacity)
{
    return EnsureColumn(database, "kingdom", "iron_ledger_debt",
            "ALTER TABLE kingdom ADD COLUMN iron_ledger_debt INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "size",
            "ALTER TABLE settlement ADD COLUMN size INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "service_mask",
            "ALTER TABLE settlement ADD COLUMN service_mask INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "service_project",
            "ALTER TABLE settlement ADD COLUMN service_project INTEGER NOT NULL DEFAULT -1;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "service_project_days",
            "ALTER TABLE settlement ADD COLUMN service_project_days INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "market_coins",
            "ALTER TABLE settlement ADD COLUMN market_coins INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "war_chest",
            "ALTER TABLE settlement ADD COLUMN war_chest INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "camp_size",
            "ALTER TABLE bandit_group ADD COLUMN camp_size INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "service_mask",
            "ALTER TABLE bandit_group ADD COLUMN service_mask INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "raid_phase",
            "ALTER TABLE bandit_group ADD COLUMN raid_phase INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "raid_target_id",
            "ALTER TABLE bandit_group ADD COLUMN raid_target_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "raid_good",
            "ALTER TABLE bandit_group ADD COLUMN raid_good INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "raid_quantity",
            "ALTER TABLE bandit_group ADD COLUMN raid_quantity INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "raid_days_remaining",
            "ALTER TABLE bandit_group ADD COLUMN raid_days_remaining INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "bandit_group", "raids_completed",
            "ALTER TABLE bandit_group ADD COLUMN raids_completed INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity);
}

static bool EnsureAnimalColumns(sqlite3 *database,
                                char *error, size_t error_capacity)
{
    return EnsureColumn(database, "settlement", "cow_adults",
            "ALTER TABLE settlement ADD COLUMN cow_adults INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "cow_calves",
            "ALTER TABLE settlement ADD COLUMN cow_calves INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "cow_condition",
            "ALTER TABLE settlement ADD COLUMN cow_condition INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "settlement", "cow_hunger",
            "ALTER TABLE settlement ADD COLUMN cow_hunger INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity);
}

static bool EnsureHorseStableColumns(sqlite3 *database,
                                     char *error, size_t error_capacity)
{
    return EnsureColumn(database, "horse_team", "sex",
            "ALTER TABLE horse_team ADD COLUMN sex INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "sire_id",
            "ALTER TABLE horse_team ADD COLUMN sire_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "dam_id",
            "ALTER TABLE horse_team ADD COLUMN dam_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "stable_settlement_id",
            "ALTER TABLE horse_team ADD COLUMN stable_settlement_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "pregnant_by_id",
            "ALTER TABLE horse_team ADD COLUMN pregnant_by_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "pregnancy_days_remaining",
            "ALTER TABLE horse_team ADD COLUMN pregnancy_days_remaining INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "breeding_cooldown_days",
            "ALTER TABLE horse_team ADD COLUMN breeding_cooldown_days INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "training",
            "ALTER TABLE horse_team ADD COLUMN training INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "strength",
            "ALTER TABLE horse_team ADD COLUMN strength INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "temperament",
            "ALTER TABLE horse_team ADD COLUMN temperament INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "horse_team", "hardiness",
            "ALTER TABLE horse_team ADD COLUMN hardiness INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity);
}

static bool EnsureJourneyColumns(sqlite3 *database,
                                 char *error, size_t error_capacity)
{
    return EnsureColumn(database, "runtime_state", "journey_pace",
            "ALTER TABLE runtime_state ADD COLUMN journey_pace INTEGER NOT NULL DEFAULT 1;",
            error, error_capacity) &&
        EnsureColumn(database, "runtime_state", "ambush_warned",
            "ALTER TABLE runtime_state ADD COLUMN ambush_warned INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity);
}

static bool EnsureLegendColumns(sqlite3 *database,
                                char *error, size_t error_capacity)
{
    return EnsureColumn(database, "goblin_cult", "hoard_defenses",
            "ALTER TABLE goblin_cult ADD COLUMN hoard_defenses INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "goblin_cult", "cohesion",
            "ALTER TABLE goblin_cult ADD COLUMN cohesion INTEGER NOT NULL DEFAULT 60;",
            error, error_capacity) &&
        EnsureColumn(database, "goblin_cult", "target_warned",
            "ALTER TABLE goblin_cult ADD COLUMN target_warned INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "goblin_cult", "expeditions_intercepted",
            "ALTER TABLE goblin_cult ADD COLUMN expeditions_intercepted INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "goblin_cult", "dragon_seed_phase",
            "ALTER TABLE goblin_cult ADD COLUMN dragon_seed_phase INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "goblin_cult", "dragon_seed_days_remaining",
            "ALTER TABLE goblin_cult ADD COLUMN dragon_seed_days_remaining INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "theft_actor_id",
            "ALTER TABLE dragon_state ADD COLUMN theft_actor_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "hoard_raiders", "motive",
            "ALTER TABLE hoard_raiders ADD COLUMN motive INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "hoard_raiders", "war_raids_completed",
            "ALTER TABLE hoard_raiders ADD COLUMN war_raids_completed INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "hoard_raiders", "social_raid_latched",
            "ALTER TABLE hoard_raiders ADD COLUMN social_raid_latched INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "hoard_raiders", "war_raid_latched",
            "ALTER TABLE hoard_raiders ADD COLUMN war_raid_latched INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "slain",
            "ALTER TABLE dragon_state ADD COLUMN slain INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "slain_day",
            "ALTER TABLE dragon_state ADD COLUMN slain_day INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "life_stage",
            "ALTER TABLE dragon_state ADD COLUMN life_stage INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "activity",
            "ALTER TABLE dragon_state ADD COLUMN activity INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "age_days",
            "ALTER TABLE dragon_state ADD COLUMN age_days INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "body_condition",
            "ALTER TABLE dragon_state ADD COLUMN body_condition INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "crown_strength",
            "ALTER TABLE dragon_state ADD COLUMN crown_strength INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "memory_integrity",
            "ALTER TABLE dragon_state ADD COLUMN memory_integrity INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "territory_stability",
            "ALTER TABLE dragon_state ADD COLUMN territory_stability INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "regional_influence",
            "ALTER TABLE dragon_state ADD COLUMN regional_influence INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "crown_continuity_days",
            "ALTER TABLE dragon_state ADD COLUMN crown_continuity_days INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "hunt_cooldown_days",
            "ALTER TABLE dragon_state ADD COLUMN hunt_cooldown_days INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "hunts",
            "ALTER TABLE dragon_state ADD COLUMN hunts INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "egg_count",
            "ALTER TABLE dragon_state ADD COLUMN egg_count INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "brood_days_remaining",
            "ALTER TABLE dragon_state ADD COLUMN brood_days_remaining INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "brood_cooldown_days",
            "ALTER TABLE dragon_state ADD COLUMN brood_cooldown_days INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "broods_laid",
            "ALTER TABLE dragon_state ADD COLUMN broods_laid INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "whelps_dispersed",
            "ALTER TABLE dragon_state ADD COLUMN whelps_dispersed INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "afterdeath_days",
            "ALTER TABLE dragon_state ADD COLUMN afterdeath_days INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "dragon_state", "lifecycle_event_id",
            "ALTER TABLE dragon_state ADD COLUMN lifecycle_event_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity);
}

static bool EnsureSocialColumns(sqlite3 *database,
                                char *error, size_t error_capacity)
{
    return EnsureColumn(database, "causal_event", "actor_id",
            "ALTER TABLE causal_event ADD COLUMN actor_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "causal_event", "target_id",
            "ALTER TABLE causal_event ADD COLUMN target_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "causal_event", "beneficiary_id",
            "ALTER TABLE causal_event ADD COLUMN beneficiary_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "causal_event", "witness_id",
            "ALTER TABLE causal_event ADD COLUMN witness_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "situation", "discovery_stage",
            "ALTER TABLE situation ADD COLUMN discovery_stage INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "situation", "lead_path",
            "ALTER TABLE situation ADD COLUMN lead_path INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "situation", "lead_event_id",
            "ALTER TABLE situation ADD COLUMN lead_event_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "npc_character", "knowledge_count",
            "ALTER TABLE npc_character ADD COLUMN knowledge_count INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "npc_character", "knowledge_write_index",
            "ALTER TABLE npc_character ADD COLUMN knowledge_write_index INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "situation_character", "witness_character_id",
            "ALTER TABLE situation_character ADD COLUMN witness_character_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity);
}

static bool Prepare(sqlite3 *database, const char *sql, sqlite3_stmt **statement,
                    char *error, size_t error_capacity)
{
    if (sqlite3_prepare_v2(database, sql, -1, statement, NULL) == SQLITE_OK) return true;
    SetSqlError(error, error_capacity, database, "Could not prepare save query");
    return false;
}

static bool StepDone(sqlite3 *database, sqlite3_stmt *statement,
                     char *error, size_t error_capacity)
{
    if (sqlite3_step(statement) == SQLITE_DONE) return true;
    SetSqlError(error, error_capacity, database, "Could not write campaign state");
    return false;
}

static bool ResetStatement(sqlite3 *database, sqlite3_stmt *statement,
                           char *error, size_t error_capacity)
{
    if (sqlite3_reset(statement) != SQLITE_OK ||
        sqlite3_clear_bindings(statement) != SQLITE_OK) {
        SetSqlError(error, error_capacity, database, "Could not reset save query");
        return false;
    }
    return true;
}

static void BindInt(sqlite3_stmt *statement, int column, int32_t value)
{
    (void)sqlite3_bind_int(statement, column, value);
}

static void BindId(sqlite3_stmt *statement, int column, CcId value)
{
    (void)sqlite3_bind_int64(statement, column, (sqlite3_int64)value);
}

static void BindMoney(sqlite3_stmt *statement, int column, CcMoney value)
{
    (void)sqlite3_bind_int64(statement, column, (sqlite3_int64)value);
}

static void BindText(sqlite3_stmt *statement, int column, const char *value)
{
    (void)sqlite3_bind_text(statement, column, value, -1, SQLITE_TRANSIENT);
}

static bool ReadPragmaInteger(sqlite3 *database, const char *sql,
                              int32_t *value,
                              char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, sql, &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetSqlError(error, error_capacity, database,
                    "Could not inspect campaign database");
        sqlite3_finalize(statement);
        return false;
    }
    *value = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return true;
}

static bool ValidateDatabaseHeader(sqlite3 *database,
                                   bool allow_unmarked,
                                   char *error, size_t error_capacity)
{
    int32_t application_id = 0;
    int32_t user_version = 0;
    if (!ReadPragmaInteger(database, "PRAGMA application_id;",
                           &application_id, error, error_capacity) ||
        !ReadPragmaInteger(database, "PRAGMA user_version;",
                           &user_version, error, error_capacity)) return false;
    if (application_id != CC_SQLITE_APPLICATION_ID &&
        !(allow_unmarked && application_id == 0)) {
        SetError(error, error_capacity,
                 "The selected database is not a Crownless campaign.");
        return false;
    }
    if (user_version > CC_SQLITE_USER_VERSION) {
        SetError(error, error_capacity,
                 "Campaign database was written by a newer version.");
        return false;
    }
    return true;
}

static bool ConfigureWritableDatabase(sqlite3 *database,
                                      char *error, size_t error_capacity)
{
#if defined(__EMSCRIPTEN__)
    return Execute(database,
        "PRAGMA foreign_keys=ON;"
        "PRAGMA journal_mode=DELETE;"
        "PRAGMA synchronous=FULL;",
        error, error_capacity);
#else
    return Execute(database,
        "PRAGMA foreign_keys=ON;"
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=FULL;"
        "PRAGMA wal_autocheckpoint=1000;",
        error, error_capacity);
#endif
}

static bool MarkDatabaseCurrent(sqlite3 *database,
                                char *error, size_t error_capacity)
{
    char sql[96];
    (void)snprintf(sql, sizeof(sql),
                   "PRAGMA application_id=%d; PRAGMA user_version=%d;",
                   CC_SQLITE_APPLICATION_ID, CC_SQLITE_USER_VERSION);
    return Execute(database, sql, error, error_capacity);
}

typedef enum WritableOpenMode {
    WRITABLE_OPEN_EXISTING,
    WRITABLE_OPEN_NEW,
    WRITABLE_OPEN_EXISTING_OR_NEW
} WritableOpenMode;

static void RemoveDatabaseArtifacts(const char *path)
{
    if (path == NULL) return;
    (void)remove(path);
    size_t length = strlen(path);
    char *sidecar = malloc(length + 5U);
    if (sidecar == NULL) return;
    (void)snprintf(sidecar, length + 5U, "%s-wal", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, length + 5U, "%s-shm", path);
    (void)remove(sidecar);
    free(sidecar);
}

static bool OpenWritableDatabase(const char *path, WritableOpenMode mode,
                                 sqlite3 **database,
                                 bool *created,
                                 char *error, size_t error_capacity)
{
    if (created != NULL) *created = false;
    bool new_database = false;
    if (mode == WRITABLE_OPEN_NEW) {
        FILE *claim = fopen(path, "wbx");
        if (claim == NULL) {
            SetError(error, error_capacity,
                     "A campaign already exists at that path.");
            return false;
        }
        if (fclose(claim) != 0) {
            RemoveDatabaseArtifacts(path);
            SetError(error, error_capacity,
                     "Could not create the campaign database.");
            return false;
        }
        new_database = true;
    }
    int result = sqlite3_open_v2(path, database, SQLITE_OPEN_READWRITE,
                                 NULL);
    if (result != SQLITE_OK && mode == WRITABLE_OPEN_EXISTING_OR_NEW) {
        if (*database != NULL) sqlite3_close(*database);
        *database = NULL;
        FILE *claim = fopen(path, "wbx");
        if (claim != NULL && fclose(claim) == 0) {
            new_database = true;
            result = sqlite3_open_v2(path, database, SQLITE_OPEN_READWRITE,
                                     NULL);
        }
    }
    if (result != SQLITE_OK) {
        SetSqlError(error, error_capacity, *database, "Could not open campaign database");
        if (*database != NULL) sqlite3_close(*database);
        *database = NULL;
        if (new_database) RemoveDatabaseArtifacts(path);
        return false;
    }
    if (!ValidateDatabaseHeader(*database, new_database,
                                error, error_capacity) ||
        !ConfigureWritableDatabase(*database, error, error_capacity)) {
        sqlite3_close(*database);
        *database = NULL;
        if (new_database) RemoveDatabaseArtifacts(path);
        return false;
    }
    if (created != NULL) *created = new_database;
    return true;
}

static bool OpenReadSnapshot(const char *path, sqlite3 **database,
                             char *error, size_t error_capacity)
{
    sqlite3 *source = NULL;
    if (sqlite3_open_v2(path, &source, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        SetSqlError(error, error_capacity, source,
                    "Could not open campaign database");
        if (source != NULL) sqlite3_close(source);
        return false;
    }
    if (!ValidateDatabaseHeader(source, false, error, error_capacity)) {
        sqlite3_close(source);
        return false;
    }
    int32_t page_size = 0;
    int32_t page_count = 0;
    if (!ReadPragmaInteger(source, "PRAGMA page_size;", &page_size,
                           error, error_capacity) ||
        !ReadPragmaInteger(source, "PRAGMA page_count;", &page_count,
                           error, error_capacity)) {
        sqlite3_close(source);
        return false;
    }
    const int64_t maximum_snapshot_bytes = INT64_C(16) * 1024 * 1024;
    if (page_size <= 0 || page_count < 0 ||
        (int64_t)page_size * (int64_t)page_count > maximum_snapshot_bytes) {
        SetError(error, error_capacity,
                 "Campaign database is too large to load safely.");
        sqlite3_close(source);
        return false;
    }
    if (sqlite3_open_v2(":memory:", database,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL) != SQLITE_OK) {
        SetSqlError(error, error_capacity, *database,
                    "Could not allocate campaign read snapshot");
        if (*database != NULL) sqlite3_close(*database);
        *database = NULL;
        sqlite3_close(source);
        return false;
    }
    sqlite3_backup *backup = sqlite3_backup_init(*database, "main",
                                                 source, "main");
    if (backup == NULL) {
        SetSqlError(error, error_capacity, *database,
                    "Could not start campaign read snapshot");
        sqlite3_close(*database);
        *database = NULL;
        sqlite3_close(source);
        return false;
    }
    int result = sqlite3_backup_step(backup, -1);
    int finish_result = sqlite3_backup_finish(backup);
    if (result != SQLITE_DONE || finish_result != SQLITE_OK) {
        SetSqlError(error, error_capacity, *database,
                    "Could not copy campaign read snapshot");
        sqlite3_close(*database);
        *database = NULL;
        sqlite3_close(source);
        return false;
    }
    if (sqlite3_close(source) != SQLITE_OK) {
        SetError(error, error_capacity,
                 "Could not close campaign source database.");
        sqlite3_close(*database);
        *database = NULL;
        return false;
    }
    return true;
}

static bool FinishTransaction(sqlite3 *database, bool ok,
                              char *error, size_t error_capacity)
{
    if (ok && Execute(database, "COMMIT;", error, error_capacity)) return true;
    (void)Execute(database, "ROLLBACK;", NULL, 0U);
    return false;
}

static bool MetaColumnExists(sqlite3 *database, const char *column,
                             bool *exists,
                             char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "PRAGMA table_info(meta);", &statement,
                 error, error_capacity)) return false;
    *exists = false;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(statement, 1);
        if (name != NULL && strcmp((const char *)name, column) == 0) {
            *exists = true;
            break;
        }
    }
    if (result != SQLITE_ROW && result != SQLITE_DONE) {
        SetSqlError(error, error_capacity, database,
                    "Could not inspect campaign metadata");
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);
    return true;
}

static bool EnsureJournalMetaColumns(sqlite3 *database,
                                     char *error, size_t error_capacity)
{
    bool generation_exists = false;
    bool cursor_exists = false;
    if (!MetaColumnExists(database, "journal_generation", &generation_exists,
                          error, error_capacity) ||
        !MetaColumnExists(database, "journal_cursor", &cursor_exists,
                          error, error_capacity)) return false;
    if (!generation_exists &&
        !Execute(database,
                 "ALTER TABLE meta ADD COLUMN journal_generation "
                 "INTEGER NOT NULL DEFAULT 0;",
                 error, error_capacity)) return false;
    if (!cursor_exists &&
        !Execute(database,
                 "ALTER TABLE meta ADD COLUMN journal_cursor "
                 "INTEGER NOT NULL DEFAULT 0;",
                 error, error_capacity)) return false;
    return EnsureColumn(database, "meta", "iron_ledger_reserve",
            "ALTER TABLE meta ADD COLUMN iron_ledger_reserve "
            "INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "meta", "archive_scribes",
            "ALTER TABLE meta ADD COLUMN archive_scribes "
            "INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "meta", "archive_lore_stored",
            "ALTER TABLE meta ADD COLUMN archive_lore_stored "
            "INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "meta", "archive_lore_lost_total",
            "ALTER TABLE meta ADD COLUMN archive_lore_lost_total "
            "INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "meta", "archive_last_recorded_day",
            "ALTER TABLE meta ADD COLUMN archive_last_recorded_day "
            "INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "meta", "archive_lore_ceiling",
            "ALTER TABLE meta ADD COLUMN archive_lore_ceiling "
            "INTEGER NOT NULL DEFAULT 40;",
            error, error_capacity);
}

static bool CreateSchema(sqlite3 *database, char *error, size_t error_capacity)
{
    const char *schema =
        "CREATE TABLE IF NOT EXISTS meta ("
        " id INTEGER PRIMARY KEY CHECK(id=1), schema_version INTEGER NOT NULL,"
        " generator_version INTEGER NOT NULL, world_seed INTEGER NOT NULL,"
        " random_state INTEGER NOT NULL, current_day INTEGER NOT NULL,"
        " next_entity_serial INTEGER NOT NULL, kingdom_count INTEGER NOT NULL,"
        " settlement_count INTEGER NOT NULL, route_count INTEGER NOT NULL,"
        " faction_count INTEGER NOT NULL, shipment_count INTEGER NOT NULL,"
        " bandit_count INTEGER NOT NULL, monster_count INTEGER NOT NULL,"
        " dungeon_count INTEGER NOT NULL, event_count INTEGER NOT NULL,"
        " event_write_index INTEGER NOT NULL, state_hash TEXT NOT NULL,"
        "journal_generation INTEGER NOT NULL DEFAULT 0,"
        " journal_cursor INTEGER NOT NULL DEFAULT 0,"
        " iron_ledger_reserve INTEGER NOT NULL DEFAULT 0,"
        " archive_scribes INTEGER NOT NULL DEFAULT 2,"
        " archive_lore_stored INTEGER NOT NULL DEFAULT 0,"
        " archive_lore_lost_total INTEGER NOT NULL DEFAULT 0,"
        " archive_last_recorded_day INTEGER NOT NULL DEFAULT 0,"
        " archive_lore_ceiling INTEGER NOT NULL DEFAULT 40);"
        "CREATE TABLE IF NOT EXISTS kingdom ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, name TEXT NOT NULL,"
        " color_r INTEGER NOT NULL, color_g INTEGER NOT NULL, color_b INTEGER NOT NULL,"
        " treasury INTEGER NOT NULL, legitimacy INTEGER NOT NULL,"
        " iron_ledger_debt INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS route ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, from_id INTEGER NOT NULL,"
        " to_id INTEGER NOT NULL, travel_days INTEGER NOT NULL, capacity INTEGER NOT NULL,"
        " security INTEGER NOT NULL, condition INTEGER NOT NULL, closed INTEGER NOT NULL,"
        " smuggler_route INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS faction ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, kingdom_id INTEGER NOT NULL,"
        " name TEXT NOT NULL, kind INTEGER NOT NULL, power INTEGER NOT NULL, support INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS shipment ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL, origin_id INTEGER NOT NULL,"
        " destination_id INTEGER NOT NULL, route_id INTEGER NOT NULL, good INTEGER NOT NULL,"
        " quantity INTEGER NOT NULL, departure_day INTEGER NOT NULL, arrival_day INTEGER NOT NULL,"
        " status INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS diplomacy ("
        " first_slot INTEGER NOT NULL, second_slot INTEGER NOT NULL,"
        " state INTEGER NOT NULL, changed_day INTEGER NOT NULL,"
        " PRIMARY KEY(first_slot,second_slot));"
        "CREATE TABLE IF NOT EXISTS courier ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE,"
        " kind INTEGER NOT NULL, status INTEGER NOT NULL,"
        " issuer_kingdom_id INTEGER NOT NULL, recipient_kingdom_id INTEGER NOT NULL,"
        " origin_settlement_id INTEGER NOT NULL, destination_settlement_id INTEGER NOT NULL,"
        " current_settlement_id INTEGER NOT NULL, route_id INTEGER NOT NULL,"
        " cause_event_id INTEGER NOT NULL, situation_id INTEGER NOT NULL,"
        " departure_day INTEGER NOT NULL, arrival_day INTEGER NOT NULL,"
        " reliability INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS monster_population ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, dungeon_id INTEGER NOT NULL,"
        " name TEXT NOT NULL, population INTEGER NOT NULL, pressure INTEGER NOT NULL,"
        " hunting_pressure INTEGER NOT NULL, last_level INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS dungeon ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, settlement_id INTEGER NOT NULL,"
        " name TEXT NOT NULL, state INTEGER NOT NULL, depth INTEGER NOT NULL,"
        " regional_pressure INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS causal_event ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, day INTEGER NOT NULL,"
        " kind INTEGER NOT NULL, subject_id INTEGER NOT NULL, location_id INTEGER NOT NULL,"
        " parent_id INTEGER NOT NULL, magnitude INTEGER NOT NULL, text TEXT NOT NULL,"
        " actor_id INTEGER NOT NULL DEFAULT 0, target_id INTEGER NOT NULL DEFAULT 0,"
        " beneficiary_id INTEGER NOT NULL DEFAULT 0, witness_id INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS player_company ("
        " id INTEGER PRIMARY KEY, location_id INTEGER NOT NULL, coins INTEGER NOT NULL,"
        " food_cargo INTEGER NOT NULL, material_cargo INTEGER NOT NULL, tools_cargo INTEGER NOT NULL,"
        " cargo_capacity INTEGER NOT NULL, passenger_capacity INTEGER NOT NULL,"
        " reputation INTEGER NOT NULL);";
    const char *realm_schema =
        "CREATE TABLE IF NOT EXISTS settlement ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, kingdom_id INTEGER NOT NULL,"
        " name TEXT NOT NULL, function INTEGER NOT NULL, map_x INTEGER NOT NULL,"
        " map_y INTEGER NOT NULL, population INTEGER NOT NULL, security INTEGER NOT NULL,"
        " prosperity INTEGER NOT NULL, hunger INTEGER NOT NULL, last_shortage INTEGER NOT NULL,"
        " food_stock INTEGER NOT NULL, material_stock INTEGER NOT NULL, tools_stock INTEGER NOT NULL,"
        " food_target INTEGER NOT NULL, material_target INTEGER NOT NULL, tools_target INTEGER NOT NULL,"
        " food_production INTEGER NOT NULL, material_production INTEGER NOT NULL, tools_production INTEGER NOT NULL,"
        " food_consumption INTEGER NOT NULL, material_consumption INTEGER NOT NULL, tools_consumption INTEGER NOT NULL,"
        " food_price INTEGER NOT NULL, material_price INTEGER NOT NULL, tools_price INTEGER NOT NULL,"
        " size INTEGER NOT NULL, service_mask INTEGER NOT NULL,"
        " service_project INTEGER NOT NULL, service_project_days INTEGER NOT NULL,"
        " market_coins INTEGER NOT NULL, war_chest INTEGER NOT NULL,"
        " cow_adults INTEGER NOT NULL DEFAULT 0,"
        " cow_calves INTEGER NOT NULL DEFAULT 0,"
        " cow_condition INTEGER NOT NULL DEFAULT 0,"
        " cow_hunger INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS horse_team ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, age_days INTEGER NOT NULL,"
        " health INTEGER NOT NULL, fatigue INTEGER NOT NULL,"
        " hunger INTEGER NOT NULL, sex INTEGER NOT NULL DEFAULT 0,"
        " sire_id INTEGER NOT NULL DEFAULT 0, dam_id INTEGER NOT NULL DEFAULT 0,"
        " stable_settlement_id INTEGER NOT NULL DEFAULT 0,"
        " pregnant_by_id INTEGER NOT NULL DEFAULT 0,"
        " pregnancy_days_remaining INTEGER NOT NULL DEFAULT 0,"
        " breeding_cooldown_days INTEGER NOT NULL DEFAULT 0,"
        " training INTEGER NOT NULL DEFAULT 0,"
        " strength INTEGER NOT NULL DEFAULT 0,"
        " temperament INTEGER NOT NULL DEFAULT 0,"
        " hardiness INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS stable_horse ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, age_days INTEGER NOT NULL,"
        " health INTEGER NOT NULL, fatigue INTEGER NOT NULL,"
        " hunger INTEGER NOT NULL, sex INTEGER NOT NULL,"
        " sire_id INTEGER NOT NULL, dam_id INTEGER NOT NULL,"
        " stable_settlement_id INTEGER NOT NULL,"
        " pregnant_by_id INTEGER NOT NULL,"
        " pregnancy_days_remaining INTEGER NOT NULL,"
        " breeding_cooldown_days INTEGER NOT NULL,"
        " training INTEGER NOT NULL, strength INTEGER NOT NULL,"
        " temperament INTEGER NOT NULL, hardiness INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS bandit_group ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, route_id INTEGER NOT NULL,"
        " name TEXT NOT NULL, members INTEGER NOT NULL, supplies INTEGER NOT NULL,"
        " influence INTEGER NOT NULL, last_level INTEGER NOT NULL,"
        " camp_size INTEGER NOT NULL, service_mask INTEGER NOT NULL,"
        " raid_phase INTEGER NOT NULL, raid_target_id INTEGER NOT NULL,"
        " raid_good INTEGER NOT NULL, raid_quantity INTEGER NOT NULL,"
        " raid_days_remaining INTEGER NOT NULL, raids_completed INTEGER NOT NULL);";
    const char *situation_schema =
        "CREATE TABLE IF NOT EXISTS situation ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, kind INTEGER NOT NULL,"
        " status INTEGER NOT NULL, issuer_faction_id INTEGER NOT NULL, target_id INTEGER NOT NULL,"
        " cause_event_id INTEGER NOT NULL, good INTEGER NOT NULL, quantity INTEGER NOT NULL,"
        " progress INTEGER NOT NULL, reward INTEGER NOT NULL, created_day INTEGER NOT NULL,"
        " deadline_day INTEGER NOT NULL, discovery_stage INTEGER NOT NULL DEFAULT 0,"
        " lead_path INTEGER NOT NULL DEFAULT 0, lead_event_id INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS shipment_intent ("
        " slot INTEGER PRIMARY KEY, final_destination_id INTEGER NOT NULL);";
    const char *quest_schema =
        "CREATE TABLE IF NOT EXISTS situation_quest ("
        " slot INTEGER PRIMARY KEY, situation_id INTEGER NOT NULL UNIQUE,"
        " front_id INTEGER NOT NULL, end_reason INTEGER NOT NULL,"
        " objective_kind INTEGER NOT NULL, objective_target_id INTEGER NOT NULL,"
        " objective_good INTEGER NOT NULL, objective_required INTEGER NOT NULL,"
        " progress_value INTEGER NOT NULL, progress_limit INTEGER NOT NULL,"
        " progress_created_event_id INTEGER NOT NULL,"
        " progress_resolved_event_id INTEGER NOT NULL,"
        " danger_value INTEGER NOT NULL, danger_limit INTEGER NOT NULL,"
        " danger_created_event_id INTEGER NOT NULL,"
        " danger_resolved_event_id INTEGER NOT NULL,"
        " evidence_count INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS situation_evidence ("
        " situation_slot INTEGER NOT NULL, evidence_slot INTEGER NOT NULL,"
        " event_id INTEGER NOT NULL,"
        " PRIMARY KEY(situation_slot,evidence_slot));"
        "CREATE TABLE IF NOT EXISTS story_front ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE,"
        " kind INTEGER NOT NULL, status INTEGER NOT NULL,"
        " outcome INTEGER NOT NULL, anchor_id INTEGER NOT NULL,"
        " cause_event_id INTEGER NOT NULL, created_event_id INTEGER NOT NULL,"
        " resolved_event_id INTEGER NOT NULL, created_day INTEGER NOT NULL,"
        " resolved_day INTEGER NOT NULL, portent_value INTEGER NOT NULL,"
        " portent_limit INTEGER NOT NULL,"
        " portent_created_event_id INTEGER NOT NULL,"
        " portent_resolved_event_id INTEGER NOT NULL,"
        " situation_count INTEGER NOT NULL, premise TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS front_situation ("
        " front_slot INTEGER NOT NULL, member_slot INTEGER NOT NULL,"
        " situation_id INTEGER NOT NULL,"
        " PRIMARY KEY(front_slot,member_slot));"
        "CREATE TABLE IF NOT EXISTS quest_outcome ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE,"
        " situation_id INTEGER NOT NULL, front_id INTEGER NOT NULL,"
        " situation_kind INTEGER NOT NULL, front_kind INTEGER NOT NULL,"
        " situation_status INTEGER NOT NULL, end_reason INTEGER NOT NULL,"
        " front_outcome INTEGER NOT NULL, target_id INTEGER NOT NULL,"
        " sponsor_character_id INTEGER NOT NULL,"
        " affected_character_id INTEGER NOT NULL,"
        " cause_event_id INTEGER NOT NULL, resolved_event_id INTEGER NOT NULL,"
        " resolved_day INTEGER NOT NULL, progress_value INTEGER NOT NULL,"
        " progress_limit INTEGER NOT NULL, danger_value INTEGER NOT NULL,"
        " danger_limit INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS delayed_echo_queue ("
        " slot INTEGER PRIMARY KEY, active INTEGER NOT NULL,"
        " situation_id INTEGER NOT NULL, settlement_id INTEGER NOT NULL,"
        " parent_event_id INTEGER NOT NULL, outcome INTEGER NOT NULL,"
        " due_day INTEGER NOT NULL, character_name TEXT NOT NULL);";
    const char *map_schema =
        "CREATE TABLE IF NOT EXISTS map_object ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, route_id INTEGER NOT NULL,"
        " maker_settlement_id INTEGER NOT NULL, owner_id INTEGER NOT NULL, name TEXT NOT NULL,"
        " surveyed_day INTEGER NOT NULL, accuracy INTEGER NOT NULL,"
        " recorded_condition INTEGER NOT NULL, recorded_danger INTEGER NOT NULL,"
        " ask_price INTEGER NOT NULL, contraband INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS map_collection ("
        " id INTEGER PRIMARY KEY CHECK(id=1),"
        " catalogue_mask INTEGER NOT NULL, archive_mask INTEGER NOT NULL);";
    const char *commitment_schema =
        "CREATE TABLE IF NOT EXISTS player_commitment ("
        " id INTEGER PRIMARY KEY CHECK(id=1), situation_id INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS situation_cast ("
        " slot INTEGER PRIMARY KEY, situation_id INTEGER NOT NULL UNIQUE,"
        " sponsor_name TEXT NOT NULL, affected_name TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS player_journey ("
        " id INTEGER PRIMARY KEY CHECK(id=1), active INTEGER NOT NULL,"
        " situation_id INTEGER NOT NULL, origin_id INTEGER NOT NULL,"
        " destination_id INTEGER NOT NULL, route_id INTEGER NOT NULL,"
        " danger INTEGER NOT NULL, bargain_cost INTEGER NOT NULL,"
        " resolved_situation_id INTEGER NOT NULL, resolved_outcome INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS runtime_state ("
        " id INTEGER PRIMARY KEY CHECK(id=1), clock_tick INTEGER NOT NULL,"
        " minute_subticks INTEGER NOT NULL, game_minutes_per_second INTEGER NOT NULL,"
        " journey_phase INTEGER NOT NULL, departure_day INTEGER NOT NULL,"
        " elapsed_subticks INTEGER NOT NULL, total_subticks INTEGER NOT NULL,"
        " encounter_subticks INTEGER NOT NULL, fare_reserved INTEGER NOT NULL,"
        " encounter_triggered INTEGER NOT NULL, ambush_pending INTEGER NOT NULL,"
        " ambush_resolved INTEGER NOT NULL, parent_event_id INTEGER NOT NULL,"
        " carriage_mode INTEGER NOT NULL, carriage_location_id INTEGER NOT NULL,"
        " carriage_route_id INTEGER NOT NULL, carriage_origin_id INTEGER NOT NULL,"
        " carriage_destination_id INTEGER NOT NULL, carriage_progress_milli INTEGER NOT NULL,"
        " carriage_speed_milli_per_second INTEGER NOT NULL, carriage_condition INTEGER NOT NULL,"
        " journey_pace INTEGER NOT NULL DEFAULT 1,"
        " ambush_warned INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS delayed_echo ("
        " id INTEGER PRIMARY KEY CHECK(id=1), active INTEGER NOT NULL,"
        " situation_id INTEGER NOT NULL, settlement_id INTEGER NOT NULL,"
        " parent_event_id INTEGER NOT NULL, outcome INTEGER NOT NULL,"
        " due_day INTEGER NOT NULL, character_name TEXT NOT NULL);";
    const char *character_schema =
        "CREATE TABLE IF NOT EXISTS npc_character ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, home_settlement_id INTEGER NOT NULL,"
        " current_settlement_id INTEGER NOT NULL, faction_id INTEGER NOT NULL,"
        " role INTEGER NOT NULL, goal INTEGER NOT NULL, activity INTEGER NOT NULL,"
        " appearance_seed INTEGER NOT NULL, player_disposition INTEGER NOT NULL,"
        " stress INTEGER NOT NULL, courage INTEGER NOT NULL,"
        " memory_count INTEGER NOT NULL, memory_write_index INTEGER NOT NULL,"
        " knowledge_count INTEGER NOT NULL DEFAULT 0,"
        " knowledge_write_index INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS character_memory ("
        " character_slot INTEGER NOT NULL, memory_slot INTEGER NOT NULL,"
        " kind INTEGER NOT NULL, subject_id INTEGER NOT NULL,"
        " event_id INTEGER NOT NULL, day INTEGER NOT NULL,"
        " PRIMARY KEY(character_slot,memory_slot));"
        "CREATE TABLE IF NOT EXISTS situation_character ("
        " slot INTEGER PRIMARY KEY, situation_id INTEGER NOT NULL UNIQUE,"
        " sponsor_character_id INTEGER NOT NULL,"
        " affected_character_id INTEGER NOT NULL,"
        " witness_character_id INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS character_knowledge ("
        " character_slot INTEGER NOT NULL, knowledge_slot INTEGER NOT NULL,"
        " kind INTEGER NOT NULL, subject_id INTEGER NOT NULL,"
        " source_character_id INTEGER NOT NULL, event_id INTEGER NOT NULL,"
        " certainty INTEGER NOT NULL, private_knowledge INTEGER NOT NULL,"
        " day INTEGER NOT NULL, PRIMARY KEY(character_slot,knowledge_slot));"
        "CREATE TABLE IF NOT EXISTS character_relationship ("
        " slot INTEGER PRIMARY KEY, from_character_id INTEGER NOT NULL,"
        " to_character_id INTEGER NOT NULL, affinity INTEGER NOT NULL,"
        " trust INTEGER NOT NULL, obligation INTEGER NOT NULL,"
        " history INTEGER NOT NULL, cause_event_id INTEGER NOT NULL);";
    const char *legend_schema =
        "CREATE TABLE IF NOT EXISTS goblin_cult ("
        " slot INTEGER PRIMARY KEY CHECK(slot=1), id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, members INTEGER NOT NULL, devotion INTEGER NOT NULL,"
        " tribute_phase INTEGER NOT NULL, tribute_target_id INTEGER NOT NULL,"
        " last_tribute_origin_id INTEGER NOT NULL, tribute_event_id INTEGER NOT NULL,"
        " carried_tribute INTEGER NOT NULL, tribute_days_remaining INTEGER NOT NULL,"
        " tribute_cooldown_days INTEGER NOT NULL, tributes_delivered INTEGER NOT NULL,"
        " hoard_defenses INTEGER NOT NULL DEFAULT 0,"
        " cohesion INTEGER NOT NULL DEFAULT 60,"
        " target_warned INTEGER NOT NULL DEFAULT 0,"
        " expeditions_intercepted INTEGER NOT NULL DEFAULT 0,"
        " dragon_seed_phase INTEGER NOT NULL DEFAULT 0,"
        " dragon_seed_days_remaining INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS dragon_state ("
        " slot INTEGER PRIMARY KEY CHECK(slot=1), id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, lair_settlement_id INTEGER NOT NULL, hoard INTEGER NOT NULL,"
        " stolen_outstanding INTEGER NOT NULL, theft_actor_id INTEGER NOT NULL,"
        " retaliation_target_id INTEGER NOT NULL,"
        " hoard_event_id INTEGER NOT NULL, omen_event_id INTEGER NOT NULL,"
        " omen_days_remaining INTEGER NOT NULL, retaliations INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS dragon_campaign ("
        " id INTEGER PRIMARY KEY CHECK(id=1), phase INTEGER NOT NULL,"
        " pledged_mask INTEGER NOT NULL, alliance_mask INTEGER NOT NULL,"
        " origin_settlement_id INTEGER NOT NULL, cause_event_id INTEGER NOT NULL,"
        " days_remaining INTEGER NOT NULL, cooldown_days INTEGER NOT NULL,"
        " food INTEGER NOT NULL, iron INTEGER NOT NULL, tools INTEGER NOT NULL,"
        " weapons INTEGER NOT NULL, gold INTEGER NOT NULL, gems INTEGER NOT NULL,"
        " recovered_coins INTEGER NOT NULL, attempts INTEGER NOT NULL,"
        " victories INTEGER NOT NULL, defeats INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS hoard_raiders ("
        " slot INTEGER PRIMARY KEY CHECK(slot=1), id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, phase INTEGER NOT NULL, motive INTEGER NOT NULL,"
        " origin_settlement_id INTEGER NOT NULL,"
        " cause_event_id INTEGER NOT NULL, carried_treasure INTEGER NOT NULL,"
        " days_remaining INTEGER NOT NULL, cooldown_days INTEGER NOT NULL,"
        " raids_completed INTEGER NOT NULL, war_raids_completed INTEGER NOT NULL,"
        " social_raid_latched INTEGER NOT NULL DEFAULT 0,"
        " war_raid_latched INTEGER NOT NULL DEFAULT 0);";
    const char *material_schema =
        "CREATE TABLE IF NOT EXISTS material_economy ("
        " slot INTEGER PRIMARY KEY, weapons_stock INTEGER NOT NULL,"
        " gold_stock INTEGER NOT NULL, gems_stock INTEGER NOT NULL,"
        " weapons_target INTEGER NOT NULL, gold_target INTEGER NOT NULL,"
        " gems_target INTEGER NOT NULL, weapons_production INTEGER NOT NULL,"
        " gold_production INTEGER NOT NULL, gems_production INTEGER NOT NULL,"
        " weapons_consumption INTEGER NOT NULL, gold_consumption INTEGER NOT NULL,"
        " gems_consumption INTEGER NOT NULL, weapons_price INTEGER NOT NULL,"
        " gold_price INTEGER NOT NULL, gems_price INTEGER NOT NULL,"
        " field_yield INTEGER NOT NULL, iron_deposit INTEGER NOT NULL,"
        " gold_seam INTEGER NOT NULL, gem_seam INTEGER NOT NULL,"
        " gold_progress INTEGER NOT NULL, gem_progress INTEGER NOT NULL,"
        " farm_tool_wear INTEGER NOT NULL, mine_tool_wear INTEGER NOT NULL,"
        " smith_tool_wear INTEGER NOT NULL, treasure_gold_committed INTEGER NOT NULL,"
        " treasure_gems_committed INTEGER NOT NULL, treasure_work INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS player_material_economy ("
        " id INTEGER PRIMARY KEY CHECK(id=1), weapons_cargo INTEGER NOT NULL,"
        " gold_cargo INTEGER NOT NULL, gems_cargo INTEGER NOT NULL,"
        " treasure_cargo_slots INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS goblin_material_economy ("
        " id INTEGER PRIMARY KEY CHECK(id=1), lair_settlement_id INTEGER NOT NULL,"
        " raid_motive INTEGER NOT NULL, lair_coins INTEGER NOT NULL,"
        " carried_treasure_id INTEGER NOT NULL,"
        " carried_food INTEGER NOT NULL, carried_iron INTEGER NOT NULL,"
        " carried_tools INTEGER NOT NULL, carried_weapons INTEGER NOT NULL,"
        " carried_gold INTEGER NOT NULL, carried_gems INTEGER NOT NULL,"
        " lair_food INTEGER NOT NULL, lair_iron INTEGER NOT NULL,"
        " lair_tools INTEGER NOT NULL, lair_weapons INTEGER NOT NULL,"
        " lair_gold INTEGER NOT NULL, lair_gems INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS dragon_material_economy ("
        " id INTEGER PRIMARY KEY CHECK(id=1), stolen_treasure_id INTEGER NOT NULL,"
        " hoard_food INTEGER NOT NULL, hoard_iron INTEGER NOT NULL,"
        " hoard_tools INTEGER NOT NULL, hoard_weapons INTEGER NOT NULL,"
        " hoard_gold INTEGER NOT NULL, hoard_gems INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS treasure ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, name TEXT NOT NULL,"
        " maker_settlement_id INTEGER NOT NULL, owner_id INTEGER NOT NULL,"
        " location_id INTEGER NOT NULL, gold_content INTEGER NOT NULL,"
        " gem_content INTEGER NOT NULL, craft_work INTEGER NOT NULL,"
        " appraised_value INTEGER NOT NULL, created_day INTEGER NOT NULL,"
        " destroyed INTEGER NOT NULL);";
    const char *journal_schema =
        "CREATE TABLE IF NOT EXISTS journal_epoch ("
        " generation INTEGER PRIMARY KEY AUTOINCREMENT,"
        " record_version INTEGER NOT NULL, world_seed INTEGER NOT NULL,"
        " initial_state_hash TEXT NOT NULL, created_tick INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS action_journal ("
        " sequence INTEGER PRIMARY KEY AUTOINCREMENT,"
        " generation INTEGER NOT NULL, ordinal INTEGER NOT NULL,"
        " record_version INTEGER NOT NULL, operation_kind INTEGER NOT NULL,"
        " command_kind INTEGER NOT NULL, target_id INTEGER NOT NULL,"
        " good INTEGER NOT NULL, amount INTEGER NOT NULL,"
        " dungeon_state INTEGER NOT NULL, step_count INTEGER NOT NULL,"
        " sim_schema_version INTEGER NOT NULL, generator_version INTEGER NOT NULL,"
        " pre_state_hash TEXT NOT NULL, post_state_hash TEXT NOT NULL,"
        " committed_tick INTEGER NOT NULL,"
        " UNIQUE(generation, ordinal),"
        " FOREIGN KEY(generation) REFERENCES journal_epoch(generation));"
        "CREATE INDEX IF NOT EXISTS action_journal_generation_ordinal "
        "ON action_journal(generation, ordinal);"
        "CREATE TRIGGER IF NOT EXISTS action_journal_no_update "
        "BEFORE UPDATE ON action_journal BEGIN "
        "SELECT RAISE(ABORT, 'action journal is append-only'); END;"
        "CREATE TRIGGER IF NOT EXISTS action_journal_no_delete "
        "BEFORE DELETE ON action_journal BEGIN "
        "SELECT RAISE(ABORT, 'action journal is append-only'); END;"
        "CREATE TRIGGER IF NOT EXISTS journal_epoch_no_update "
        "BEFORE UPDATE ON journal_epoch BEGIN "
        "SELECT RAISE(ABORT, 'journal epoch is append-only'); END;"
        "CREATE TRIGGER IF NOT EXISTS journal_epoch_no_delete "
        "BEFORE DELETE ON journal_epoch BEGIN "
        "SELECT RAISE(ABORT, 'journal epoch is append-only'); END;";
    const char *underroad_schema =
        "CREATE TABLE IF NOT EXISTS dungeon_detail ("
        " dungeon_slot INTEGER PRIMARY KEY, layout_seed INTEGER NOT NULL,"
        " encounter_random_state INTEGER NOT NULL, room_count INTEGER NOT NULL,"
        " link_count INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS dungeon_room ("
        " dungeon_slot INTEGER NOT NULL, room_slot INTEGER NOT NULL,"
        " name TEXT NOT NULL, kind INTEGER NOT NULL, depth INTEGER NOT NULL,"
        " map_x INTEGER NOT NULL, map_y INTEGER NOT NULL, flags INTEGER NOT NULL,"
        " state_flags INTEGER NOT NULL, loot_good INTEGER NOT NULL,"
        " loot_quantity INTEGER NOT NULL,"
        " PRIMARY KEY(dungeon_slot,room_slot));"
        "CREATE TABLE IF NOT EXISTS dungeon_link ("
        " dungeon_slot INTEGER NOT NULL, link_slot INTEGER NOT NULL,"
        " from_room INTEGER NOT NULL, to_room INTEGER NOT NULL,"
        " kind INTEGER NOT NULL, flags INTEGER NOT NULL,"
        " PRIMARY KEY(dungeon_slot,link_slot));"
        "CREATE TABLE IF NOT EXISTS dungeon_expedition ("
        " slot INTEGER PRIMARY KEY CHECK(slot=1), active INTEGER NOT NULL,"
        " dungeon_id INTEGER NOT NULL, current_room INTEGER NOT NULL,"
        " turns_elapsed INTEGER NOT NULL, days_elapsed INTEGER NOT NULL,"
        " light_remaining INTEGER NOT NULL, noise INTEGER NOT NULL,"
        " strain INTEGER NOT NULL, maximum_depth INTEGER NOT NULL,"
        " encounter_kind INTEGER NOT NULL, encounter_reaction INTEGER NOT NULL,"
        " encounter_room INTEGER NOT NULL);";
    return Execute(database, schema, error, error_capacity) &&
           Execute(database, realm_schema, error, error_capacity) &&
           Execute(database, situation_schema, error, error_capacity) &&
           Execute(database, quest_schema, error, error_capacity) &&
           Execute(database, map_schema, error, error_capacity) &&
           Execute(database, commitment_schema, error, error_capacity) &&
           Execute(database, character_schema, error, error_capacity) &&
           Execute(database, legend_schema, error, error_capacity) &&
           Execute(database, material_schema, error, error_capacity) &&
           Execute(database, journal_schema, error, error_capacity) &&
           Execute(database, underroad_schema, error, error_capacity) &&
           EnsureJourneyColumns(database, error, error_capacity) &&
           EnsureSocialColumns(database, error, error_capacity) &&
           EnsureJournalMetaColumns(database, error, error_capacity);
}

static bool SaveMeta(sqlite3 *database, const CcSim *sim,
                     uint64_t journal_generation, uint64_t journal_cursor,
                     char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    const char *sql =
        "INSERT INTO meta (id,schema_version,generator_version,world_seed,"
        "random_state,current_day,next_entity_serial,kingdom_count,"
        "settlement_count,route_count,faction_count,shipment_count,"
        "bandit_count,monster_count,dungeon_count,event_count,"
        "event_write_index,state_hash,journal_generation,journal_cursor,"
        "iron_ledger_reserve,archive_scribes,archive_lore_stored,"
        "archive_lore_lost_total,archive_last_recorded_day,archive_lore_ceiling) "
        "VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    if (!Prepare(database, sql, &statement, error, error_capacity)) return false;
    char hash[24];
    (void)snprintf(hash, sizeof(hash), "%016" PRIx64, CcSimHash(sim));
    BindInt(statement, 1, (int32_t)sim->schema_version);
    BindInt(statement, 2, (int32_t)sim->generator_version);
    BindInt(statement, 3, (int32_t)sim->world_seed);
    BindInt(statement, 4, (int32_t)sim->random_state);
    BindInt(statement, 5, sim->current_day);
    BindId(statement, 6, sim->next_entity_serial);
    BindInt(statement, 7, sim->kingdom_count);
    BindInt(statement, 8, sim->settlement_count);
    BindInt(statement, 9, sim->route_count);
    BindInt(statement, 10, sim->faction_count);
    BindInt(statement, 11, sim->shipment_count);
    BindInt(statement, 12, sim->bandit_count);
    BindInt(statement, 13, sim->monster_count);
    BindInt(statement, 14, sim->dungeon_count);
    BindInt(statement, 15, sim->event_count);
    BindInt(statement, 16, sim->event_write_index);
    BindText(statement, 17, hash);
    BindId(statement, 18, journal_generation);
    BindId(statement, 19, journal_cursor);
    BindMoney(statement, 20, sim->iron_ledger_reserve);
    BindInt(statement, 21, sim->archives.scribes);
    BindInt(statement, 22, sim->archives.lore_stored);
    BindInt(statement, 23, sim->archives.lore_lost_total);
    BindInt(statement, 24, sim->archives.last_recorded_day);
    BindInt(statement, 25, sim->archives.lore_ceiling);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveKingdoms(sqlite3 *database, const CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO kingdom "
                 "(slot,id,name,color_r,color_g,color_b,treasury,legitimacy,"
                 "iron_ledger_debt) VALUES(?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *item = &sim->kingdoms[i];
        BindInt(statement, 1, i); BindId(statement, 2, item->id);
        BindText(statement, 3, item->name); BindInt(statement, 4, item->color_r);
        BindInt(statement, 5, item->color_g); BindInt(statement, 6, item->color_b);
        BindMoney(statement, 7, item->treasury); BindInt(statement, 8, item->legitimacy);
        BindMoney(statement, 9, item->iron_ledger_debt);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveSettlements(sqlite3 *database, const CcSim *sim,
                            char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    const char *sql = "INSERT INTO settlement VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    if (!Prepare(database, sql, &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *s = &sim->settlements[i];
        int column = 1;
        BindInt(statement, column++, i); BindId(statement, column++, s->id);
        BindId(statement, column++, s->kingdom_id); BindText(statement, column++, s->name);
        BindInt(statement, column++, (int32_t)s->function); BindInt(statement, column++, s->map_x);
        BindInt(statement, column++, s->map_y); BindInt(statement, column++, s->population);
        BindInt(statement, column++, s->security); BindInt(statement, column++, s->prosperity);
        BindInt(statement, column++, s->hunger); BindInt(statement, column++, sim->last_shortage_level[i]);
        for (int32_t good = 0; good < 3; ++good) BindInt(statement, column++, s->stock[good]);
        for (int32_t good = 0; good < 3; ++good) BindInt(statement, column++, s->reserve_target[good]);
        for (int32_t good = 0; good < 3; ++good) BindInt(statement, column++, s->production[good]);
        for (int32_t good = 0; good < 3; ++good) BindInt(statement, column++, s->consumption[good]);
        for (int32_t good = 0; good < 3; ++good) BindInt(statement, column++, s->price[good]);
        BindInt(statement, column++, (int32_t)s->size);
        BindInt(statement, column++, (int32_t)s->service_mask);
        BindInt(statement, column++, (int32_t)s->service_project);
        BindInt(statement, column++, s->service_project_days);
        BindMoney(statement, column++, s->market_coins);
        BindMoney(statement, column++, s->war_chest);
        BindInt(statement, column++, s->cow_adults);
        BindInt(statement, column++, s->cow_calves);
        BindInt(statement, column++, s->cow_condition);
        BindInt(statement, column++, s->cow_hunger);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveHorseTeam(sqlite3 *database, const CcSim *sim,
                          char *error, size_t error_capacity)
{
    if (sim->schema_version < 14U) return true;
    sqlite3_stmt *statement = NULL;
    const char *sql = sim->schema_version >= 15U ?
        "INSERT INTO horse_team VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);" :
        "INSERT INTO horse_team (slot,id,name,age_days,health,fatigue,hunger) VALUES(?,?,?,?,?,?,?);";
    if (!Prepare(database, sql,
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        const CcHorse *horse = &sim->horse_team[i];
        int column = 1;
        BindInt(statement, column++, i);
        BindId(statement, column++, horse->id);
        BindText(statement, column++, horse->name);
        BindInt(statement, column++, horse->age_days);
        BindInt(statement, column++, horse->health);
        BindInt(statement, column++, horse->fatigue);
        BindInt(statement, column++, horse->hunger);
        if (sim->schema_version >= 15U) {
            BindInt(statement, column++, (int32_t)horse->sex);
            BindId(statement, column++, horse->sire_id);
            BindId(statement, column++, horse->dam_id);
            BindId(statement, column++, horse->stable_settlement_id);
            BindId(statement, column++, horse->pregnant_by_id);
            BindInt(statement, column++, horse->pregnancy_days_remaining);
            BindInt(statement, column++, horse->breeding_cooldown_days);
            BindInt(statement, column++, horse->training);
            BindInt(statement, column++, horse->strength);
            BindInt(statement, column++, horse->temperament);
            BindInt(statement, column++, horse->hardiness);
        }
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveStableHorses(sqlite3 *database, const CcSim *sim,
                             char *error, size_t error_capacity)
{
    if (sim->schema_version < 15U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO stable_horse VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->stable_horse_count; ++i) {
        const CcHorse *horse = &sim->stable_horses[i];
        int column = 1;
        BindInt(statement, column++, i);
        BindId(statement, column++, horse->id);
        BindText(statement, column++, horse->name);
        BindInt(statement, column++, horse->age_days);
        BindInt(statement, column++, horse->health);
        BindInt(statement, column++, horse->fatigue);
        BindInt(statement, column++, horse->hunger);
        BindInt(statement, column++, (int32_t)horse->sex);
        BindId(statement, column++, horse->sire_id);
        BindId(statement, column++, horse->dam_id);
        BindId(statement, column++, horse->stable_settlement_id);
        BindId(statement, column++, horse->pregnant_by_id);
        BindInt(statement, column++, horse->pregnancy_days_remaining);
        BindInt(statement, column++, horse->breeding_cooldown_days);
        BindInt(statement, column++, horse->training);
        BindInt(statement, column++, horse->strength);
        BindInt(statement, column++, horse->temperament);
        BindInt(statement, column++, horse->hardiness);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveMaterialEconomy(sqlite3 *database, const CcSim *sim,
                                char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
            "INSERT INTO material_economy VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
            &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *s = &sim->settlements[i];
        int column = 1;
        BindInt(statement, column++, i);
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            BindInt(statement, column++, s->stock[good]);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            BindInt(statement, column++, s->reserve_target[good]);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            BindInt(statement, column++, s->production[good]);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            BindInt(statement, column++, s->consumption[good]);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            BindInt(statement, column++, s->price[good]);
        }
        BindInt(statement, column++, s->field_yield);
        BindInt(statement, column++, s->iron_deposit);
        BindInt(statement, column++, s->gold_seam ? 1 : 0);
        BindInt(statement, column++, s->gem_seam ? 1 : 0);
        BindInt(statement, column++, s->gold_progress);
        BindInt(statement, column++, s->gem_progress);
        BindInt(statement, column++, s->farm_tool_wear);
        BindInt(statement, column++, s->mine_tool_wear);
        BindInt(statement, column++, s->smith_tool_wear);
        BindInt(statement, column++, s->treasure_gold_committed);
        BindInt(statement, column++, s->treasure_gems_committed);
        BindInt(statement, column++, s->treasure_work);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
            "INSERT INTO player_material_economy VALUES(1,?,?,?,?);",
            &statement, error, error_capacity)) return false;
    BindInt(statement, 1, sim->player.cargo[CC_GOOD_WEAPONS]);
    BindInt(statement, 2, sim->player.cargo[CC_GOOD_GOLD]);
    BindInt(statement, 3, sim->player.cargo[CC_GOOD_GEMS]);
    BindInt(statement, 4, sim->player.treasure_cargo_slots);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (!Prepare(database,
            "INSERT INTO goblin_material_economy VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
            &statement, error, error_capacity)) return false;
    int column = 1;
    BindId(statement, column++, sim->goblins.lair_settlement_id);
    BindInt(statement, column++, (int32_t)sim->goblins.raid_motive);
    BindMoney(statement, column++, sim->goblins.lair_coins);
    BindId(statement, column++, sim->goblins.carried_treasure_id);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        BindInt(statement, column++, sim->goblins.carried_goods[good]);
    }
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        BindInt(statement, column++, sim->goblins.lair_stock[good]);
    }
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (!Prepare(database,
            "INSERT INTO dragon_material_economy VALUES(1,?,?,?,?,?,?,?);",
            &statement, error, error_capacity)) return false;
    BindId(statement, 1, sim->dragon.stolen_treasure_id);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        BindInt(statement, good + 2, sim->dragon.hoard_goods[good]);
    }
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (!Prepare(database,
            "INSERT INTO treasure VALUES(?,?,?,?,?,?,?,?,?,?,?,?);",
            &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        BindInt(statement, 1, i);
        BindId(statement, 2, treasure->id);
        BindText(statement, 3, treasure->name);
        BindId(statement, 4, treasure->maker_settlement_id);
        BindId(statement, 5, treasure->owner_id);
        BindId(statement, 6, treasure->location_id);
        BindInt(statement, 7, treasure->gold_content);
        BindInt(statement, 8, treasure->gem_content);
        BindInt(statement, 9, treasure->craft_work);
        BindInt(statement, 10, treasure->appraised_value);
        BindInt(statement, 11, treasure->created_day);
        BindInt(statement, 12, treasure->destroyed ? 1 : 0);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveRoutes(sqlite3 *database, const CcSim *sim,
                       char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO route VALUES(?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *r = &sim->routes[i];
        BindInt(statement, 1, i); BindId(statement, 2, r->id);
        BindId(statement, 3, r->from_id); BindId(statement, 4, r->to_id);
        BindInt(statement, 5, r->travel_days); BindInt(statement, 6, r->capacity);
        BindInt(statement, 7, r->security); BindInt(statement, 8, r->condition);
        BindInt(statement, 9, r->closed ? 1 : 0); BindInt(statement, 10, r->smuggler_route ? 1 : 0);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveMaps(sqlite3 *database, const CcSim *sim,
                     char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO map_object VALUES(?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        BindInt(statement, 1, i); BindId(statement, 2, map->id);
        BindId(statement, 3, map->route_id);
        BindId(statement, 4, map->maker_settlement_id);
        BindId(statement, 5, map->owner_id); BindText(statement, 6, map->name);
        BindInt(statement, 7, map->surveyed_day); BindInt(statement, 8, map->accuracy);
        BindInt(statement, 9, map->recorded_condition);
        BindInt(statement, 10, map->recorded_danger);
        BindInt(statement, 11, map->ask_price);
        BindInt(statement, 12, map->contraband ? 1 : 0);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveMapCollection(sqlite3 *database, const CcSim *sim,
                              char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO map_collection VALUES(1,?,?);",
                 &statement, error, error_capacity)) return false;
    BindInt(statement, 1, (int32_t)sim->player.map_catalogue_mask);
    BindInt(statement, 2, (int32_t)sim->player.map_archive_mask);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveFactions(sqlite3 *database, const CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO faction VALUES(?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *f = &sim->factions[i];
        BindInt(statement, 1, i); BindId(statement, 2, f->id);
        BindId(statement, 3, f->kingdom_id); BindText(statement, 4, f->name);
        BindInt(statement, 5, (int32_t)f->kind); BindInt(statement, 6, f->power);
        BindInt(statement, 7, f->support);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveShipments(sqlite3 *database, const CcSim *sim,
                          char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO shipment VALUES(?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *s = &sim->shipments[i];
        BindInt(statement, 1, i); BindId(statement, 2, s->id);
        BindId(statement, 3, s->origin_id); BindId(statement, 4, s->destination_id);
        BindId(statement, 5, s->route_id); BindInt(statement, 6, (int32_t)s->good);
        BindInt(statement, 7, s->quantity); BindInt(statement, 8, s->departure_day);
        BindInt(statement, 9, s->arrival_day); BindInt(statement, 10, (int32_t)s->status);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    if (!Prepare(database, "INSERT INTO shipment_intent VALUES(?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        BindInt(statement, 1, i);
        BindId(statement, 2, sim->shipments[i].final_destination_id);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveDiplomacyAndCouriers(sqlite3 *database, const CcSim *sim,
                                     char *error, size_t error_capacity)
{
    if (sim->schema_version < 11U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO diplomacy "
                 "(first_slot,second_slot,state,changed_day) VALUES(?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = 0; second < sim->kingdom_count; ++second) {
            BindInt(statement, 1, first);
            BindInt(statement, 2, second);
            BindInt(statement, 3, (int32_t)sim->diplomacy[first][second]);
            BindInt(statement, 4,
                    sim->diplomacy_changed_day[first][second]);
            if (!StepDone(database, statement, error, error_capacity) ||
                !ResetStatement(database, statement, error, error_capacity)) {
                sqlite3_finalize(statement);
                return false;
            }
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO courier "
                 "(slot,id,kind,status,issuer_kingdom_id,recipient_kingdom_id,"
                 "origin_settlement_id,destination_settlement_id,"
                 "current_settlement_id,route_id,cause_event_id,situation_id,"
                 "departure_day,arrival_day,reliability) "
                 "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        const CcCourier *courier = &sim->couriers[i];
        int column = 1;
        BindInt(statement, column++, i);
        BindId(statement, column++, courier->id);
        BindInt(statement, column++, (int32_t)courier->kind);
        BindInt(statement, column++, (int32_t)courier->status);
        BindId(statement, column++, courier->issuer_kingdom_id);
        BindId(statement, column++, courier->recipient_kingdom_id);
        BindId(statement, column++, courier->origin_settlement_id);
        BindId(statement, column++, courier->destination_settlement_id);
        BindId(statement, column++, courier->current_settlement_id);
        BindId(statement, column++, courier->route_id);
        BindId(statement, column++, courier->cause_event_id);
        BindId(statement, column++, courier->situation_id);
        BindInt(statement, column++, courier->departure_day);
        BindInt(statement, column++, courier->arrival_day);
        BindInt(statement, column++, courier->reliability);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveThreats(sqlite3 *database, const CcSim *sim,
                        char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO bandit_group VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcBanditGroup *b = &sim->bandits[i];
        BindInt(statement, 1, i); BindId(statement, 2, b->id); BindId(statement, 3, b->route_id);
        BindText(statement, 4, b->name); BindInt(statement, 5, b->members);
        BindInt(statement, 6, b->supplies); BindInt(statement, 7, b->influence);
        BindInt(statement, 8, sim->last_bandit_level[i]);
        BindInt(statement, 9, (int32_t)b->camp_size);
        BindInt(statement, 10, (int32_t)b->service_mask);
        BindInt(statement, 11, (int32_t)b->raid_phase);
        BindId(statement, 12, b->raid_target_id);
        BindInt(statement, 13, (int32_t)b->raid_good);
        BindInt(statement, 14, b->raid_quantity);
        BindInt(statement, 15, b->raid_days_remaining);
        BindInt(statement, 16, b->raids_completed);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database, "INSERT INTO monster_population VALUES(?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        const CcMonsterPopulation *m = &sim->monsters[i];
        BindInt(statement, 1, i); BindId(statement, 2, m->id); BindId(statement, 3, m->dungeon_id);
        BindText(statement, 4, m->name); BindInt(statement, 5, m->population);
        BindInt(statement, 6, m->pressure); BindInt(statement, 7, m->hunting_pressure);
        BindInt(statement, 8, sim->last_monster_level[i]);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveDungeons(sqlite3 *database, const CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO dungeon VALUES(?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *d = &sim->dungeons[i];
        BindInt(statement, 1, i); BindId(statement, 2, d->id);
        BindId(statement, 3, d->settlement_id); BindText(statement, 4, d->name);
        BindInt(statement, 5, (int32_t)d->state); BindInt(statement, 6, d->depth);
        BindInt(statement, 7, d->regional_pressure);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    if (sim->schema_version < 19U) return true;

    if (!Prepare(database,
                 "INSERT INTO dungeon_detail VALUES(?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *d = &sim->dungeons[i];
        BindInt(statement, 1, i);
        BindInt(statement, 2, (int32_t)d->layout_seed);
        BindInt(statement, 3, (int32_t)d->encounter_random_state);
        BindInt(statement, 4, d->room_count);
        BindInt(statement, 5, d->link_count);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO dungeon_room VALUES(?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *d = &sim->dungeons[i];
        for (int32_t room = 0; room < d->room_count; ++room) {
            const CcDungeonRoom *item = &d->rooms[room];
            BindInt(statement, 1, i); BindInt(statement, 2, room);
            BindText(statement, 3, item->name);
            BindInt(statement, 4, (int32_t)item->kind);
            BindInt(statement, 5, item->depth);
            BindInt(statement, 6, item->map_x);
            BindInt(statement, 7, item->map_y);
            BindInt(statement, 8, (int32_t)item->flags);
            BindInt(statement, 9, (int32_t)item->state_flags);
            BindInt(statement, 10, (int32_t)item->loot_good);
            BindInt(statement, 11, item->loot_quantity);
            if (!StepDone(database, statement, error, error_capacity) ||
                !ResetStatement(database, statement, error, error_capacity)) {
                sqlite3_finalize(statement); return false;
            }
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO dungeon_link VALUES(?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *d = &sim->dungeons[i];
        for (int32_t link = 0; link < d->link_count; ++link) {
            const CcDungeonLink *item = &d->links[link];
            BindInt(statement, 1, i); BindInt(statement, 2, link);
            BindInt(statement, 3, item->from_room);
            BindInt(statement, 4, item->to_room);
            BindInt(statement, 5, (int32_t)item->kind);
            BindInt(statement, 6, (int32_t)item->flags);
            if (!StepDone(database, statement, error, error_capacity) ||
                !ResetStatement(database, statement, error, error_capacity)) {
                sqlite3_finalize(statement); return false;
            }
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO dungeon_expedition VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    BindInt(statement, 1, expedition->active ? 1 : 0);
    BindId(statement, 2, expedition->dungeon_id);
    BindInt(statement, 3, expedition->current_room);
    BindInt(statement, 4, expedition->turns_elapsed);
    BindInt(statement, 5, expedition->days_elapsed);
    BindInt(statement, 6, expedition->light_remaining);
    BindInt(statement, 7, expedition->noise);
    BindInt(statement, 8, expedition->strain);
    BindInt(statement, 9, expedition->maximum_depth);
    BindInt(statement, 10, (int32_t)expedition->encounter_kind);
    BindInt(statement, 11, expedition->encounter_reaction);
    BindInt(statement, 12, expedition->encounter_room);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveLegends(sqlite3 *database, const CcSim *sim,
                        char *error, size_t error_capacity)
{
    if (sim->schema_version < 6U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO goblin_cult (slot,id,name,members,devotion,"
                 "tribute_phase,tribute_target_id,last_tribute_origin_id,"
                 "tribute_event_id,carried_tribute,tribute_days_remaining,"
                 "tribute_cooldown_days,tributes_delivered,hoard_defenses,"
                 "cohesion,target_warned,expeditions_intercepted,"
                 "dragon_seed_phase,dragon_seed_days_remaining) "
                 "VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    const CcGoblinCult *goblins = &sim->goblins;
    int column = 1;
    BindId(statement, column++, goblins->id);
    BindText(statement, column++, goblins->name);
    BindInt(statement, column++, goblins->members);
    BindInt(statement, column++, goblins->devotion);
    BindInt(statement, column++, (int32_t)goblins->tribute_phase);
    BindId(statement, column++, goblins->tribute_target_id);
    BindId(statement, column++, goblins->last_tribute_origin_id);
    BindId(statement, column++, goblins->tribute_event_id);
    BindMoney(statement, column++, goblins->carried_tribute);
    BindInt(statement, column++, goblins->tribute_days_remaining);
    BindInt(statement, column++, goblins->tribute_cooldown_days);
    BindInt(statement, column++, goblins->tributes_delivered);
    BindInt(statement, column++, goblins->hoard_defenses);
    BindInt(statement, column++, goblins->cohesion);
    BindInt(statement, column++, goblins->target_warned ? 1 : 0);
    BindInt(statement, column++, goblins->expeditions_intercepted);
    BindInt(statement, column++, (int32_t)goblins->dragon_seed_phase);
    BindInt(statement, column++, goblins->dragon_seed_days_remaining);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (!Prepare(database,
                 "INSERT INTO dragon_state (slot,id,name,lair_settlement_id,hoard,"
                 "stolen_outstanding,theft_actor_id,retaliation_target_id,"
                 "hoard_event_id,omen_event_id,omen_days_remaining,retaliations,"
                 "slain,slain_day,life_stage,activity,age_days,body_condition,"
                 "crown_strength,memory_integrity,territory_stability,"
                 "regional_influence,crown_continuity_days,hunt_cooldown_days,"
                 "hunts,egg_count,brood_days_remaining,brood_cooldown_days,"
                 "broods_laid,whelps_dispersed,afterdeath_days,lifecycle_event_id) "
                 "VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    const CcDragon *dragon = &sim->dragon;
    column = 1;
    BindId(statement, column++, dragon->id);
    BindText(statement, column++, dragon->name);
    BindId(statement, column++, dragon->lair_settlement_id);
    BindMoney(statement, column++, dragon->hoard);
    BindMoney(statement, column++, dragon->stolen_outstanding);
    BindId(statement, column++, dragon->theft_actor_id);
    BindId(statement, column++, dragon->retaliation_target_id);
    BindId(statement, column++, dragon->hoard_event_id);
    BindId(statement, column++, dragon->omen_event_id);
    BindInt(statement, column++, dragon->omen_days_remaining);
    BindInt(statement, column++, dragon->retaliations);
    BindInt(statement, column++, dragon->slain ? 1 : 0);
    BindInt(statement, column++, dragon->slain_day);
    BindInt(statement, column++, (int32_t)dragon->life_stage);
    BindInt(statement, column++, (int32_t)dragon->activity);
    BindInt(statement, column++, dragon->age_days);
    BindInt(statement, column++, dragon->body_condition);
    BindInt(statement, column++, dragon->crown_strength);
    BindInt(statement, column++, dragon->memory_integrity);
    BindInt(statement, column++, dragon->territory_stability);
    BindInt(statement, column++, dragon->regional_influence);
    BindInt(statement, column++, dragon->crown_continuity_days);
    BindInt(statement, column++, dragon->hunt_cooldown_days);
    BindInt(statement, column++, dragon->hunts);
    BindInt(statement, column++, dragon->egg_count);
    BindInt(statement, column++, dragon->brood_days_remaining);
    BindInt(statement, column++, dragon->brood_cooldown_days);
    BindInt(statement, column++, dragon->broods_laid);
    BindInt(statement, column++, dragon->whelps_dispersed);
    BindInt(statement, column++, dragon->afterdeath_days);
    BindId(statement, column++, dragon->lifecycle_event_id);
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (sim->schema_version >= 11U) {
        if (!Prepare(database,
                     "INSERT INTO dragon_campaign "
                     "(id,phase,pledged_mask,alliance_mask,origin_settlement_id,"
                     "cause_event_id,days_remaining,cooldown_days,food,iron,tools,"
                     "weapons,gold,gems,recovered_coins,attempts,victories,defeats) "
                     "VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                     &statement, error, error_capacity)) return false;
        const CcDragonCampaign *campaign = &sim->dragon_campaign;
        column = 1;
        BindInt(statement, column++, (int32_t)campaign->phase);
        BindInt(statement, column++,
                (int32_t)campaign->pledged_kingdom_mask);
        BindInt(statement, column++,
                (int32_t)campaign->alliance_kingdom_mask);
        BindId(statement, column++, campaign->origin_settlement_id);
        BindId(statement, column++, campaign->cause_event_id);
        BindInt(statement, column++, campaign->days_remaining);
        BindInt(statement, column++, campaign->cooldown_days);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            BindInt(statement, column++, campaign->supplies[good]);
        }
        BindMoney(statement, column++, campaign->recovered_coins);
        BindInt(statement, column++, campaign->attempts);
        BindInt(statement, column++, campaign->victories);
        BindInt(statement, column++, campaign->defeats);
        result = StepDone(database, statement, error, error_capacity);
        sqlite3_finalize(statement);
        if (!result) return false;
    }
    if (sim->schema_version < 7U) return true;

    if (!Prepare(database,
                 "INSERT INTO hoard_raiders (slot,id,name,phase,motive,"
                 "origin_settlement_id,cause_event_id,carried_treasure,"
                 "days_remaining,cooldown_days,raids_completed,war_raids_completed,"
                 "social_raid_latched,war_raid_latched) "
                 "VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    const CcHoardRaiders *raiders = &sim->hoard_raiders;
    column = 1;
    BindId(statement, column++, raiders->id);
    BindText(statement, column++, raiders->name);
    BindInt(statement, column++, (int32_t)raiders->phase);
    BindInt(statement, column++, (int32_t)raiders->motive);
    BindId(statement, column++, raiders->origin_settlement_id);
    BindId(statement, column++, raiders->cause_event_id);
    BindMoney(statement, column++, raiders->carried_treasure);
    BindInt(statement, column++, raiders->days_remaining);
    BindInt(statement, column++, raiders->cooldown_days);
    BindInt(statement, column++, raiders->raids_completed);
    BindInt(statement, column++, raiders->war_raids_completed);
    BindInt(statement, column++, raiders->social_raid_latched ? 1 : 0);
    BindInt(statement, column++, raiders->war_raid_latched ? 1 : 0);
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveEvents(sqlite3 *database, const CcSim *sim,
                       char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO causal_event "
                 "(slot,id,day,kind,subject_id,location_id,parent_id,magnitude,text,"
                 "actor_id,target_id,beneficiary_id,witness_id) "
                 "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        const CcEvent *e = &sim->events[i];
        if (e->id == 0U) continue;
        BindInt(statement, 1, i); BindId(statement, 2, e->id); BindInt(statement, 3, e->day);
        BindInt(statement, 4, (int32_t)e->kind); BindId(statement, 5, e->subject_id);
        BindId(statement, 6, e->location_id); BindId(statement, 7, e->parent_id);
        BindInt(statement, 8, e->magnitude); BindText(statement, 9, e->text);
        BindId(statement, 10, e->actor_id);
        BindId(statement, 11, e->target_id);
        BindId(statement, 12, e->beneficiary_id);
        BindId(statement, 13, e->witness_id);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveSituations(sqlite3 *database, const CcSim *sim,
                           char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO situation "
                 "(slot,id,kind,status,issuer_faction_id,target_id,cause_event_id,"
                 "good,quantity,progress,reward,created_day,deadline_day,"
                 "discovery_stage,lead_path,lead_event_id) "
                 "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *s = &sim->situations[i];
        int column = 1;
        BindInt(statement, column++, i); BindId(statement, column++, s->id);
        BindInt(statement, column++, (int32_t)s->kind);
        BindInt(statement, column++, (int32_t)s->status);
        BindId(statement, column++, s->issuer_faction_id);
        BindId(statement, column++, s->target_id);
        BindId(statement, column++, s->cause_event_id);
        BindInt(statement, column++, (int32_t)s->good);
        BindInt(statement, column++, s->quantity);
        BindInt(statement, column++, s->progress);
        BindMoney(statement, column++, s->reward);
        BindInt(statement, column++, s->created_day);
        BindInt(statement, column++, s->deadline_day);
        BindInt(statement, column++, (int32_t)s->discovery_stage);
        BindInt(statement, column++, (int32_t)s->lead_path);
        BindId(statement, column++, s->lead_event_id);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveSituationCasts(sqlite3 *database, const CcSim *sim,
                               char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO situation_cast VALUES(?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        BindInt(statement, 1, i);
        BindId(statement, 2, situation->id);
        BindText(statement, 3, situation->sponsor_name);
        BindText(statement, 4, situation->affected_name);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveQuestArchitecture(sqlite3 *database, const CcSim *sim,
                                  char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO situation_quest VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        const CcQuestObjective *objective = &situation->objective;
        int column = 1;
        BindInt(statement, column++, i);
        BindId(statement, column++, situation->id);
        BindId(statement, column++, situation->front_id);
        BindInt(statement, column++, (int32_t)situation->end_reason);
        BindInt(statement, column++, (int32_t)objective->kind);
        BindId(statement, column++, objective->target_id);
        BindInt(statement, column++, (int32_t)objective->good);
        BindInt(statement, column++, objective->required);
        BindInt(statement, column++, objective->progress.value);
        BindInt(statement, column++, objective->progress.limit);
        BindId(statement, column++, objective->progress.created_by_event_id);
        BindId(statement, column++, objective->progress.resolved_by_event_id);
        BindInt(statement, column++, objective->danger.value);
        BindInt(statement, column++, objective->danger.limit);
        BindId(statement, column++, objective->danger.created_by_event_id);
        BindId(statement, column++, objective->danger.resolved_by_event_id);
        BindInt(statement, column++, objective->evidence_count);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO situation_evidence VALUES(?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcQuestObjective *objective = &sim->situations[i].objective;
        for (int32_t evidence = 0;
             evidence < objective->evidence_count; ++evidence) {
            BindInt(statement, 1, i);
            BindInt(statement, 2, evidence);
            BindId(statement, 3, objective->evidence_event_ids[evidence]);
            if (!StepDone(database, statement, error, error_capacity) ||
                !ResetStatement(database, statement,
                                error, error_capacity)) {
                sqlite3_finalize(statement);
                return false;
            }
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO story_front VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->front_count; ++i) {
        const CcFront *front = &sim->fronts[i];
        int column = 1;
        BindInt(statement, column++, i);
        BindId(statement, column++, front->id);
        BindInt(statement, column++, (int32_t)front->kind);
        BindInt(statement, column++, (int32_t)front->status);
        BindInt(statement, column++, (int32_t)front->outcome);
        BindId(statement, column++, front->anchor_id);
        BindId(statement, column++, front->cause_event_id);
        BindId(statement, column++, front->created_event_id);
        BindId(statement, column++, front->resolved_event_id);
        BindInt(statement, column++, front->created_day);
        BindInt(statement, column++, front->resolved_day);
        BindInt(statement, column++, front->portent.value);
        BindInt(statement, column++, front->portent.limit);
        BindId(statement, column++, front->portent.created_by_event_id);
        BindId(statement, column++, front->portent.resolved_by_event_id);
        BindInt(statement, column++, front->situation_count);
        BindText(statement, column++, front->premise);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO front_situation VALUES(?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->front_count; ++i) {
        const CcFront *front = &sim->fronts[i];
        for (int32_t member = 0;
             member < front->situation_count; ++member) {
            BindInt(statement, 1, i);
            BindInt(statement, 2, member);
            BindId(statement, 3, front->situation_ids[member]);
            if (!StepDone(database, statement, error, error_capacity) ||
                !ResetStatement(database, statement,
                                error, error_capacity)) {
                sqlite3_finalize(statement);
                return false;
            }
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO quest_outcome VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
        const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
        int column = 1;
        BindInt(statement, column++, i);
        BindId(statement, column++, outcome->id);
        BindId(statement, column++, outcome->situation_id);
        BindId(statement, column++, outcome->front_id);
        BindInt(statement, column++, (int32_t)outcome->situation_kind);
        BindInt(statement, column++, (int32_t)outcome->front_kind);
        BindInt(statement, column++, (int32_t)outcome->situation_status);
        BindInt(statement, column++, (int32_t)outcome->end_reason);
        BindInt(statement, column++, (int32_t)outcome->front_outcome);
        BindId(statement, column++, outcome->target_id);
        BindId(statement, column++, outcome->sponsor_character_id);
        BindId(statement, column++, outcome->affected_character_id);
        BindId(statement, column++, outcome->cause_event_id);
        BindId(statement, column++, outcome->resolved_event_id);
        BindInt(statement, column++, outcome->resolved_day);
        BindInt(statement, column++, outcome->progress_value);
        BindInt(statement, column++, outcome->progress_limit);
        BindInt(statement, column++, outcome->danger_value);
        BindInt(statement, column++, outcome->danger_limit);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "INSERT INTO delayed_echo_queue VALUES(?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->pending_echo_count; ++i) {
        const CcDelayedEcho *echo = &sim->pending_echoes[i];
        BindInt(statement, 1, i);
        BindInt(statement, 2, echo->active ? 1 : 0);
        BindId(statement, 3, echo->situation_id);
        BindId(statement, 4, echo->settlement_id);
        BindId(statement, 5, echo->parent_event_id);
        BindInt(statement, 6, (int32_t)echo->outcome);
        BindInt(statement, 7, echo->due_day);
        BindText(statement, 8, echo->character_name);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool SaveCharacters(sqlite3 *database, const CcSim *sim,
                           char *error, size_t error_capacity)
{
    sqlite3_stmt *character_statement = NULL;
    sqlite3_stmt *memory_statement = NULL;
    sqlite3_stmt *knowledge_statement = NULL;
    sqlite3_stmt *situation_statement = NULL;
    sqlite3_stmt *relationship_statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO npc_character "
                 "(slot,id,name,home_settlement_id,current_settlement_id,faction_id,"
                 "role,goal,activity,appearance_seed,player_disposition,stress,courage,"
                 "memory_count,memory_write_index,knowledge_count,"
                 "knowledge_write_index) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &character_statement, error, error_capacity) ||
        !Prepare(database,
                 "INSERT INTO character_memory VALUES(?,?,?,?,?,?);",
                 &memory_statement, error, error_capacity) ||
        !Prepare(database,
                 "INSERT INTO character_knowledge VALUES(?,?,?,?,?,?,?,?,?);",
                 &knowledge_statement, error, error_capacity) ||
        !Prepare(database,
                 "INSERT INTO situation_character "
                 "(slot,situation_id,sponsor_character_id,affected_character_id,"
                 "witness_character_id) VALUES(?,?,?,?,?);",
                 &situation_statement, error, error_capacity) ||
        !Prepare(database,
                 "INSERT INTO character_relationship VALUES(?,?,?,?,?,?,?,?);",
                 &relationship_statement, error, error_capacity)) {
        sqlite3_finalize(character_statement);
        sqlite3_finalize(memory_statement);
        sqlite3_finalize(knowledge_statement);
        sqlite3_finalize(situation_statement);
        sqlite3_finalize(relationship_statement);
        return false;
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *character = &sim->characters[i];
        int column = 1;
        BindInt(character_statement, column++, i);
        BindId(character_statement, column++, character->id);
        BindText(character_statement, column++, character->name);
        BindId(character_statement, column++, character->home_settlement_id);
        BindId(character_statement, column++, character->current_settlement_id);
        BindId(character_statement, column++, character->faction_id);
        BindInt(character_statement, column++, (int32_t)character->role);
        BindInt(character_statement, column++, (int32_t)character->goal);
        BindInt(character_statement, column++, (int32_t)character->activity);
        BindInt(character_statement, column++,
                (int32_t)character->appearance_seed);
        BindInt(character_statement, column++, character->player_disposition);
        BindInt(character_statement, column++, character->stress);
        BindInt(character_statement, column++, character->courage);
        BindInt(character_statement, column++, character->memory_count);
        BindInt(character_statement, column++, character->memory_write_index);
        BindInt(character_statement, column++, character->knowledge_count);
        BindInt(character_statement, column++, character->knowledge_write_index);
        if (!StepDone(database, character_statement, error, error_capacity) ||
            !ResetStatement(database, character_statement,
                            error, error_capacity)) goto failed;
        for (int32_t memory = 0;
             memory < CC_CHARACTER_MEMORY_CAPACITY; ++memory) {
            const CcCharacterMemory *item = &character->memories[memory];
            BindInt(memory_statement, 1, i);
            BindInt(memory_statement, 2, memory);
            BindInt(memory_statement, 3, (int32_t)item->kind);
            BindId(memory_statement, 4, item->subject_id);
            BindId(memory_statement, 5, item->event_id);
            BindInt(memory_statement, 6, item->day);
            if (!StepDone(database, memory_statement,
                          error, error_capacity) ||
                !ResetStatement(database, memory_statement,
                                error, error_capacity)) goto failed;
        }
        for (int32_t knowledge = 0;
             knowledge < CC_CHARACTER_KNOWLEDGE_CAPACITY; ++knowledge) {
            const CcCharacterKnowledge *item =
                &character->knowledge[knowledge];
            BindInt(knowledge_statement, 1, i);
            BindInt(knowledge_statement, 2, knowledge);
            BindInt(knowledge_statement, 3, (int32_t)item->kind);
            BindId(knowledge_statement, 4, item->subject_id);
            BindId(knowledge_statement, 5, item->source_character_id);
            BindId(knowledge_statement, 6, item->event_id);
            BindInt(knowledge_statement, 7, (int32_t)item->certainty);
            BindInt(knowledge_statement, 8,
                    item->private_knowledge ? 1 : 0);
            BindInt(knowledge_statement, 9, item->day);
            if (!StepDone(database, knowledge_statement,
                          error, error_capacity) ||
                !ResetStatement(database, knowledge_statement,
                                error, error_capacity)) goto failed;
        }
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        BindInt(situation_statement, 1, i);
        BindId(situation_statement, 2, situation->id);
        BindId(situation_statement, 3, situation->sponsor_character_id);
        BindId(situation_statement, 4, situation->affected_character_id);
        BindId(situation_statement, 5, situation->witness_character_id);
        if (!StepDone(database, situation_statement,
                      error, error_capacity) ||
            !ResetStatement(database, situation_statement,
                            error, error_capacity)) goto failed;
    }
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        const CcRelationship *relationship = &sim->relationships[i];
        BindInt(relationship_statement, 1, i);
        BindId(relationship_statement, 2, relationship->from_character_id);
        BindId(relationship_statement, 3, relationship->to_character_id);
        BindInt(relationship_statement, 4, relationship->affinity);
        BindInt(relationship_statement, 5, relationship->trust);
        BindInt(relationship_statement, 6, relationship->obligation);
        BindInt(relationship_statement, 7, (int32_t)relationship->history);
        BindId(relationship_statement, 8, relationship->cause_event_id);
        if (!StepDone(database, relationship_statement,
                      error, error_capacity) ||
            !ResetStatement(database, relationship_statement,
                            error, error_capacity)) goto failed;
    }
    sqlite3_finalize(character_statement);
    sqlite3_finalize(memory_statement);
    sqlite3_finalize(knowledge_statement);
    sqlite3_finalize(situation_statement);
    sqlite3_finalize(relationship_statement);
    return true;

failed:
    sqlite3_finalize(character_statement);
    sqlite3_finalize(memory_statement);
    sqlite3_finalize(knowledge_statement);
    sqlite3_finalize(situation_statement);
    sqlite3_finalize(relationship_statement);
    return false;
}

static bool SavePlayer(sqlite3 *database, const CcSim *sim,
                       char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO player_company VALUES(?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    const CcPlayerCompany *p = &sim->player;
    BindId(statement, 1, p->id); BindId(statement, 2, p->location_id);
    BindMoney(statement, 3, p->coins); BindInt(statement, 4, p->cargo[CC_GOOD_FOOD]);
    BindInt(statement, 5, p->cargo[CC_GOOD_MATERIAL]); BindInt(statement, 6, p->cargo[CC_GOOD_TOOLS]);
    BindInt(statement, 7, p->cargo_capacity); BindInt(statement, 8, p->passenger_capacity);
    BindInt(statement, 9, p->reputation);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SavePlayerCommitment(sqlite3 *database, const CcSim *sim,
                                 char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO player_commitment VALUES(1,?);",
                 &statement, error, error_capacity)) return false;
    BindId(statement, 1, sim->player.accepted_situation_id);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveJourneyState(sqlite3 *database, const CcSim *sim,
                             char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO player_journey VALUES(1,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    BindInt(statement, 1, sim->journey.active ? 1 : 0);
    BindId(statement, 2, sim->journey.situation_id);
    BindId(statement, 3, sim->journey.origin_id);
    BindId(statement, 4, sim->journey.destination_id);
    BindId(statement, 5, sim->journey.route_id);
    BindInt(statement, 6, sim->journey.danger);
    BindInt(statement, 7, sim->journey.bargain_cost);
    BindId(statement, 8, sim->resolved_journey_situation_id);
    BindInt(statement, 9, (int32_t)sim->resolved_journey_outcome);
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (!Prepare(database,
                 "INSERT INTO runtime_state (id,clock_tick,minute_subticks,"
                 "game_minutes_per_second,journey_phase,departure_day,"
                 "elapsed_subticks,total_subticks,encounter_subticks,"
                 "fare_reserved,encounter_triggered,ambush_pending,"
                 "ambush_resolved,parent_event_id,carriage_mode,"
                 "carriage_location_id,carriage_route_id,carriage_origin_id,"
                 "carriage_destination_id,carriage_progress_milli,"
                 "carriage_speed_milli_per_second,carriage_condition,"
                 "journey_pace,ambush_warned) "
                 "VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    int column = 1;
    BindId(statement, column++, sim->clock.tick);
    BindInt(statement, column++, sim->clock.minute_subticks);
    BindInt(statement, column++, sim->clock.game_minutes_per_second);
    BindInt(statement, column++, (int32_t)sim->journey.phase);
    BindInt(statement, column++, sim->journey.departure_day);
    BindInt(statement, column++, sim->journey.elapsed_subticks);
    BindInt(statement, column++, sim->journey.total_subticks);
    BindInt(statement, column++, sim->journey.encounter_subticks);
    BindInt(statement, column++, sim->journey.fare_reserved);
    BindInt(statement, column++, sim->journey.encounter_triggered ? 1 : 0);
    BindInt(statement, column++, sim->journey.ambush_pending ? 1 : 0);
    BindInt(statement, column++, sim->journey.ambush_resolved ? 1 : 0);
    BindId(statement, column++, sim->journey.parent_event_id);
    BindInt(statement, column++, (int32_t)sim->carriage.mode);
    BindId(statement, column++, sim->carriage.location_id);
    BindId(statement, column++, sim->carriage.route_id);
    BindId(statement, column++, sim->carriage.origin_id);
    BindId(statement, column++, sim->carriage.destination_id);
    BindInt(statement, column++, sim->carriage.progress_milli);
    BindInt(statement, column++, sim->carriage.speed_milli_per_second);
    BindInt(statement, column++, sim->carriage.condition);
    BindInt(statement, column++, (int32_t)sim->journey.pace);
    BindInt(statement, column++, sim->journey.ambush_warned ? 1 : 0);
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (!Prepare(database,
                 "INSERT INTO delayed_echo VALUES(1,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    BindInt(statement, 1, sim->delayed_echo.active ? 1 : 0);
    BindId(statement, 2, sim->delayed_echo.situation_id);
    BindId(statement, 3, sim->delayed_echo.settlement_id);
    BindId(statement, 4, sim->delayed_echo.parent_event_id);
    BindInt(statement, 5, (int32_t)sim->delayed_echo.outcome);
    BindInt(statement, 6, sim->delayed_echo.due_day);
    BindText(statement, 7, sim->delayed_echo.character_name);
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveSnapshotContents(sqlite3 *database, const CcSim *sim,
                                 uint64_t journal_generation,
                                 uint64_t journal_cursor,
                                 char *error, size_t error_capacity)
{
    char validation[160];
    if (database == NULL || sim == NULL ||
        !CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, sim == NULL || database == NULL ?
                 "Save database or simulation is missing." : validation);
        return false;
    }
    return Execute(database,
            "DELETE FROM meta; DELETE FROM kingdom; DELETE FROM settlement;"
            "DELETE FROM horse_team; DELETE FROM stable_horse;"
            "DELETE FROM route; DELETE FROM map_object; DELETE FROM map_collection;"
            "DELETE FROM faction; DELETE FROM shipment;"
            "DELETE FROM shipment_intent; DELETE FROM diplomacy; DELETE FROM courier;"
            "DELETE FROM bandit_group; DELETE FROM monster_population;"
            "DELETE FROM goblin_cult; DELETE FROM dragon_state;"
            "DELETE FROM dragon_campaign; DELETE FROM hoard_raiders;"
            "DELETE FROM dungeon; DELETE FROM dungeon_detail;"
            "DELETE FROM dungeon_room; DELETE FROM dungeon_link;"
            "DELETE FROM dungeon_expedition;"
            "DELETE FROM situation; DELETE FROM situation_cast;"
            "DELETE FROM situation_quest; DELETE FROM situation_evidence;"
            "DELETE FROM story_front; DELETE FROM front_situation;"
            "DELETE FROM quest_outcome; DELETE FROM delayed_echo_queue;"
            "DELETE FROM situation_character; DELETE FROM character_memory;"
            "DELETE FROM character_knowledge; DELETE FROM character_relationship;"
            "DELETE FROM npc_character;"
            "DELETE FROM causal_event;"
            "DELETE FROM player_company; DELETE FROM player_commitment;"
            "DELETE FROM player_journey; DELETE FROM runtime_state;"
            "DELETE FROM delayed_echo; DELETE FROM material_economy;"
            "DELETE FROM player_material_economy;"
            "DELETE FROM goblin_material_economy;"
            "DELETE FROM dragon_material_economy; DELETE FROM treasure;",
            error, error_capacity) &&
        SaveMeta(database, sim, journal_generation, journal_cursor,
                 error, error_capacity) &&
        SaveKingdoms(database, sim, error, error_capacity) &&
        SaveSettlements(database, sim, error, error_capacity) &&
        SaveHorseTeam(database, sim, error, error_capacity) &&
        SaveStableHorses(database, sim, error, error_capacity) &&
        SaveMaterialEconomy(database, sim, error, error_capacity) &&
        SaveRoutes(database, sim, error, error_capacity) &&
        SaveMaps(database, sim, error, error_capacity) &&
        SaveMapCollection(database, sim, error, error_capacity) &&
        SaveFactions(database, sim, error, error_capacity) &&
        SaveShipments(database, sim, error, error_capacity) &&
        SaveDiplomacyAndCouriers(database, sim, error, error_capacity) &&
        SaveThreats(database, sim, error, error_capacity) &&
        SaveDungeons(database, sim, error, error_capacity) &&
        SaveLegends(database, sim, error, error_capacity) &&
        SaveSituations(database, sim, error, error_capacity) &&
        SaveSituationCasts(database, sim, error, error_capacity) &&
        SaveQuestArchitecture(database, sim, error, error_capacity) &&
        SaveCharacters(database, sim, error, error_capacity) &&
        SaveEvents(database, sim, error, error_capacity) &&
        SavePlayer(database, sim, error, error_capacity) &&
        SavePlayerCommitment(database, sim, error, error_capacity) &&
        SaveJourneyState(database, sim, error, error_capacity);
}

static bool SaveSnapshot(sqlite3 *database, const CcSim *sim,
                         uint64_t journal_generation,
                         uint64_t journal_cursor,
                         char *error, size_t error_capacity)
{
    bool ok = EnsureRealmColumns(database, error, error_capacity) &&
        EnsureAnimalColumns(database, error, error_capacity) &&
        EnsureHorseStableColumns(database, error, error_capacity) &&
        EnsureJourneyColumns(database, error, error_capacity) &&
        EnsureLegendColumns(database, error, error_capacity) &&
        EnsureSocialColumns(database, error, error_capacity) &&
        Execute(database, "BEGIN IMMEDIATE;", error, error_capacity);
    if (ok) {
        ok = SaveSnapshotContents(database, sim, journal_generation,
                                  journal_cursor, error, error_capacity);
    }
    return FinishTransaction(database, ok, error, error_capacity);
}

bool CcSaveWrite(const char *path, const CcSim *sim,
                 char *error, size_t error_capacity)
{
    if (path == NULL || sim == NULL) {
        SetError(error, error_capacity,
                 "Save path or simulation is missing.");
        return false;
    }
    sqlite3 *database = NULL;
    bool created = false;
    if (!OpenWritableDatabase(path, WRITABLE_OPEN_EXISTING_OR_NEW, &database,
                              &created,
                              error, error_capacity)) return false;
    bool ok = CreateSchema(database, error, error_capacity) &&
              MarkDatabaseCurrent(database, error, error_capacity) &&
              SaveSnapshot(database, sim, 0U, 0U,
                           error, error_capacity);
    if (sqlite3_close(database) != SQLITE_OK && ok) {
        SetError(error, error_capacity, "Could not close campaign database.");
        return false;
    }
    if (!ok && created) RemoveDatabaseArtifacts(path);
    return ok;
}

static bool ReadMeta(sqlite3 *database, CcSim *sim, uint64_t *expected_hash,
                     uint64_t *journal_generation,
                     uint64_t *journal_cursor,
                     char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT schema_version,generator_version,world_seed,random_state,"
        "current_day,next_entity_serial,kingdom_count,settlement_count,route_count,faction_count,"
        "shipment_count,bandit_count,monster_count,dungeon_count,event_count,"
        "event_write_index,state_hash,journal_generation,journal_cursor,"
        "iron_ledger_reserve,archive_scribes,archive_lore_stored,"
        "archive_lore_lost_total,archive_last_recorded_day,archive_lore_ceiling "
        "FROM meta WHERE id=1;", &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity, "Campaign metadata is missing.");
        sqlite3_finalize(statement); return false;
    }
    sim->schema_version = (uint32_t)sqlite3_column_int(statement, 0);
    sim->generator_version = (uint32_t)sqlite3_column_int(statement, 1);
    sim->world_seed = (uint32_t)sqlite3_column_int(statement, 2);
    sim->random_state = (uint32_t)sqlite3_column_int(statement, 3);
    sim->current_day = sqlite3_column_int(statement, 4);
    sim->next_entity_serial = (uint64_t)sqlite3_column_int64(statement, 5);
    sim->kingdom_count = sqlite3_column_int(statement, 6);
    sim->settlement_count = sqlite3_column_int(statement, 7);
    sim->route_count = sqlite3_column_int(statement, 8);
    sim->faction_count = sqlite3_column_int(statement, 9);
    sim->shipment_count = sqlite3_column_int(statement, 10);
    sim->bandit_count = sqlite3_column_int(statement, 11);
    sim->monster_count = sqlite3_column_int(statement, 12);
    sim->dungeon_count = sqlite3_column_int(statement, 13);
    sim->event_count = sqlite3_column_int(statement, 14);
    sim->event_write_index = sqlite3_column_int(statement, 15);
    const unsigned char *hash_text = sqlite3_column_text(statement, 16);
    if (!ParseStoredHash(hash_text, expected_hash)) {
        SetError(error, error_capacity, "Campaign hash is invalid.");
        sqlite3_finalize(statement); return false;
    }
    *journal_generation = (uint64_t)sqlite3_column_int64(statement, 17);
    *journal_cursor = (uint64_t)sqlite3_column_int64(statement, 18);
    sim->iron_ledger_reserve =
        (CcMoney)sqlite3_column_int64(statement, 19);
    sim->archives.scribes = sqlite3_column_int(statement, 20);
    sim->archives.lore_stored = sqlite3_column_int(statement, 21);
    sim->archives.lore_lost_total = sqlite3_column_int(statement, 22);
    sim->archives.last_recorded_day = sqlite3_column_int(statement, 23);
    sim->archives.lore_ceiling = sqlite3_column_int(statement, 24);
    sqlite3_finalize(statement);
    return true;
}

static bool ReadKingdoms(sqlite3 *database, CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT slot,id,name,color_r,color_g,color_b,treasury,"
                 "legitimacy,iron_ledger_debt FROM kingdom ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_KINGDOMS) { sqlite3_finalize(statement); return false; }
        CcKingdom *k = &sim->kingdoms[slot];
        k->id = (CcId)sqlite3_column_int64(statement, 1);
        if (!ReadTextColumn(statement, 2, k->name, sizeof(k->name),
                            "kingdom name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        k->color_r = (uint8_t)sqlite3_column_int(statement, 3);
        k->color_g = (uint8_t)sqlite3_column_int(statement, 4);
        k->color_b = (uint8_t)sqlite3_column_int(statement, 5);
        k->treasury = (CcMoney)sqlite3_column_int64(statement, 6);
        k->legitimacy = sqlite3_column_int(statement, 7);
        k->iron_ledger_debt =
            (CcMoney)sqlite3_column_int64(statement, 8);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->kingdom_count) { SetError(error, error_capacity, "Kingdom rows are incomplete."); return false; }
    return true;
}

static bool ReadDiplomacyAndCouriers(sqlite3 *database, CcSim *sim,
                                     char *error, size_t error_capacity)
{
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = 0; second < sim->kingdom_count; ++second) {
            sim->diplomacy[first][second] = first == second ?
                CC_DIPLOMACY_PEACE : CC_DIPLOMACY_WAR;
            sim->diplomacy_changed_day[first][second] = sim->current_day;
        }
    }
    sim->courier_count = 0;
    if (sim->schema_version < 11U) return true;

    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT first_slot,second_slot,state,changed_day "
                 "FROM diplomacy ORDER BY first_slot,second_slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t first = sqlite3_column_int(statement, 0);
        int32_t second = sqlite3_column_int(statement, 1);
        if (first < 0 || first >= sim->kingdom_count ||
            second < 0 || second >= sim->kingdom_count) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Diplomacy rows are invalid.");
            return false;
        }
        sim->diplomacy[first][second] =
            (CcDiplomaticState)sqlite3_column_int(statement, 2);
        sim->diplomacy_changed_day[first][second] =
            sqlite3_column_int(statement, 3);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->kingdom_count * sim->kingdom_count) {
        SetError(error, error_capacity, "Diplomacy rows are incomplete.");
        return false;
    }

    if (!Prepare(database, "SELECT * FROM courier ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_COURIERS ||
            slot != sim->courier_count) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Courier rows are invalid.");
            return false;
        }
        CcCourier *courier = &sim->couriers[slot];
        int column = 1;
        courier->id = (CcId)sqlite3_column_int64(statement, column++);
        courier->kind = (CcCourierKind)sqlite3_column_int(statement, column++);
        courier->status =
            (CcCourierStatus)sqlite3_column_int(statement, column++);
        courier->issuer_kingdom_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->recipient_kingdom_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->origin_settlement_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->destination_settlement_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->current_settlement_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->route_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->cause_event_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->situation_id =
            (CcId)sqlite3_column_int64(statement, column++);
        courier->departure_day = sqlite3_column_int(statement, column++);
        courier->arrival_day = sqlite3_column_int(statement, column++);
        courier->reliability = sqlite3_column_int(statement, column++);
        sim->courier_count += 1;
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadSettlements(sqlite3 *database, CcSim *sim,
                            char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM settlement ORDER BY slot;", &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_SETTLEMENTS) { sqlite3_finalize(statement); return false; }
        CcSettlement *s = &sim->settlements[slot];
        int column = 1;
        s->id = (CcId)sqlite3_column_int64(statement, column++);
        s->kingdom_id = (CcId)sqlite3_column_int64(statement, column++);
        if (!ReadTextColumn(statement, column++, s->name, sizeof(s->name),
                            "settlement name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        s->function = (CcSettlementFunction)sqlite3_column_int(statement, column++);
        s->map_x = sqlite3_column_int(statement, column++); s->map_y = sqlite3_column_int(statement, column++);
        s->population = sqlite3_column_int(statement, column++); s->security = sqlite3_column_int(statement, column++);
        s->prosperity = sqlite3_column_int(statement, column++); s->hunger = sqlite3_column_int(statement, column++);
        sim->last_shortage_level[slot] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < 3; ++good) s->stock[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < 3; ++good) s->reserve_target[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < 3; ++good) s->production[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < 3; ++good) s->consumption[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < 3; ++good) s->price[good] = sqlite3_column_int(statement, column++);
        s->size = (CcSettlementSize)sqlite3_column_int(statement, column++);
        s->service_mask = (uint32_t)sqlite3_column_int64(statement, column++);
        s->service_project = (CcServiceKind)sqlite3_column_int(statement, column++);
        s->service_project_days = sqlite3_column_int(statement, column++);
        s->market_coins = (CcMoney)sqlite3_column_int64(statement, column++);
        s->war_chest = (CcMoney)sqlite3_column_int64(statement, column++);
        if (sim->schema_version >= 14U) {
            s->cow_adults = sqlite3_column_int(statement, column++);
            s->cow_calves = sqlite3_column_int(statement, column++);
            s->cow_condition = sqlite3_column_int(statement, column++);
            s->cow_hunger = sqlite3_column_int(statement, column++);
        }
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->settlement_count) { SetError(error, error_capacity, "Settlement rows are incomplete."); return false; }
    return true;
}

static bool ReadHorseTeam(sqlite3 *database, CcSim *sim,
                          char *error, size_t error_capacity)
{
    if (sim->schema_version < 14U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM horse_team ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot != rows || slot < 0 ||
            slot >= CC_CARRIAGE_HORSE_COUNT) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Horse team rows are invalid.");
            return false;
        }
        CcHorse *horse = &sim->horse_team[slot];
        horse->id = (CcId)sqlite3_column_int64(statement, 1);
        if (!ReadTextColumn(statement, 2, horse->name, sizeof(horse->name),
                            "horse name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        horse->age_days = sqlite3_column_int(statement, 3);
        horse->health = sqlite3_column_int(statement, 4);
        horse->fatigue = sqlite3_column_int(statement, 5);
        horse->hunger = sqlite3_column_int(statement, 6);
        if (sim->schema_version >= 15U) {
            horse->sex = (CcHorseSex)sqlite3_column_int(statement, 7);
            horse->sire_id = (CcId)sqlite3_column_int64(statement, 8);
            horse->dam_id = (CcId)sqlite3_column_int64(statement, 9);
            horse->stable_settlement_id =
                (CcId)sqlite3_column_int64(statement, 10);
            horse->pregnant_by_id =
                (CcId)sqlite3_column_int64(statement, 11);
            horse->pregnancy_days_remaining =
                sqlite3_column_int(statement, 12);
            horse->breeding_cooldown_days =
                sqlite3_column_int(statement, 13);
            horse->training = sqlite3_column_int(statement, 14);
            horse->strength = sqlite3_column_int(statement, 15);
            horse->temperament = sqlite3_column_int(statement, 16);
            horse->hardiness = sqlite3_column_int(statement, 17);
        }
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != CC_CARRIAGE_HORSE_COUNT) {
        SetError(error, error_capacity, "Horse team rows are incomplete.");
        return false;
    }
    return true;
}

static bool ReadStableHorses(sqlite3 *database, CcSim *sim,
                             char *error, size_t error_capacity)
{
    if (sim->schema_version < 15U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM stable_horse ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot != rows || slot < 0 || slot >= CC_MAX_STABLE_HORSES) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Stable horse rows are invalid.");
            return false;
        }
        CcHorse *horse = &sim->stable_horses[slot];
        horse->id = (CcId)sqlite3_column_int64(statement, 1);
        if (!ReadTextColumn(statement, 2, horse->name, sizeof(horse->name),
                            "horse name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        horse->age_days = sqlite3_column_int(statement, 3);
        horse->health = sqlite3_column_int(statement, 4);
        horse->fatigue = sqlite3_column_int(statement, 5);
        horse->hunger = sqlite3_column_int(statement, 6);
        horse->sex = (CcHorseSex)sqlite3_column_int(statement, 7);
        horse->sire_id = (CcId)sqlite3_column_int64(statement, 8);
        horse->dam_id = (CcId)sqlite3_column_int64(statement, 9);
        horse->stable_settlement_id =
            (CcId)sqlite3_column_int64(statement, 10);
        horse->pregnant_by_id =
            (CcId)sqlite3_column_int64(statement, 11);
        horse->pregnancy_days_remaining = sqlite3_column_int(statement, 12);
        horse->breeding_cooldown_days = sqlite3_column_int(statement, 13);
        horse->training = sqlite3_column_int(statement, 14);
        horse->strength = sqlite3_column_int(statement, 15);
        horse->temperament = sqlite3_column_int(statement, 16);
        horse->hardiness = sqlite3_column_int(statement, 17);
        rows += 1;
    }
    sqlite3_finalize(statement);
    sim->stable_horse_count = rows;
    return true;
}

static bool ReadMaterialEconomy(sqlite3 *database, CcSim *sim,
                                char *error, size_t error_capacity)
{
    if (sim->schema_version < 9U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
            "SELECT * FROM material_economy ORDER BY slot;",
            &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= sim->settlement_count) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Material economy rows are invalid.");
            return false;
        }
        CcSettlement *s = &sim->settlements[slot];
        int column = 1;
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            s->stock[good] = sqlite3_column_int(statement, column++);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            s->reserve_target[good] = sqlite3_column_int(statement, column++);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            s->production[good] = sqlite3_column_int(statement, column++);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            s->consumption[good] = sqlite3_column_int(statement, column++);
        }
        for (int32_t good = CC_GOOD_WEAPONS; good < CC_GOOD_COUNT; ++good) {
            s->price[good] = sqlite3_column_int(statement, column++);
        }
        s->field_yield = sqlite3_column_int(statement, column++);
        s->iron_deposit = sqlite3_column_int(statement, column++);
        s->gold_seam = sqlite3_column_int(statement, column++) != 0;
        s->gem_seam = sqlite3_column_int(statement, column++) != 0;
        s->gold_progress = sqlite3_column_int(statement, column++);
        s->gem_progress = sqlite3_column_int(statement, column++);
        s->farm_tool_wear = sqlite3_column_int(statement, column++);
        s->mine_tool_wear = sqlite3_column_int(statement, column++);
        s->smith_tool_wear = sqlite3_column_int(statement, column++);
        s->treasure_gold_committed = sqlite3_column_int(statement, column++);
        s->treasure_gems_committed = sqlite3_column_int(statement, column++);
        s->treasure_work = sqlite3_column_int(statement, column++);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->settlement_count) {
        SetError(error, error_capacity, "Material economy rows are incomplete.");
        return false;
    }

    if (!Prepare(database,
            "SELECT weapons_cargo,gold_cargo,gems_cargo,treasure_cargo_slots "
            "FROM player_material_economy WHERE id=1;",
            &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        SetError(error, error_capacity, "Player material economy is missing.");
        return false;
    }
    sim->player.cargo[CC_GOOD_WEAPONS] = sqlite3_column_int(statement, 0);
    sim->player.cargo[CC_GOOD_GOLD] = sqlite3_column_int(statement, 1);
    sim->player.cargo[CC_GOOD_GEMS] = sqlite3_column_int(statement, 2);
    sim->player.treasure_cargo_slots = sqlite3_column_int(statement, 3);
    sqlite3_finalize(statement);

    if (!Prepare(database,
            "SELECT * FROM goblin_material_economy WHERE id=1;",
            &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        SetError(error, error_capacity, "Goblin material economy is missing.");
        return false;
    }
    int column = 1;
    sim->goblins.lair_settlement_id =
        (CcId)sqlite3_column_int64(statement, column++);
    sim->goblins.raid_motive =
        (CcGoblinRaidMotive)sqlite3_column_int(statement, column++);
    sim->goblins.lair_coins =
        (CcMoney)sqlite3_column_int64(statement, column++);
    sim->goblins.carried_treasure_id =
        (CcId)sqlite3_column_int64(statement, column++);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        sim->goblins.carried_goods[good] =
            sqlite3_column_int(statement, column++);
    }
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        sim->goblins.lair_stock[good] =
            sqlite3_column_int(statement, column++);
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
            "SELECT * FROM dragon_material_economy WHERE id=1;",
            &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        SetError(error, error_capacity, "Dragon material economy is missing.");
        return false;
    }
    sim->dragon.stolen_treasure_id =
        (CcId)sqlite3_column_int64(statement, 1);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        sim->dragon.hoard_goods[good] =
            sqlite3_column_int(statement, good + 2);
    }
    sqlite3_finalize(statement);

    if (!Prepare(database, "SELECT * FROM treasure ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    sim->treasure_count = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_TREASURES ||
            slot != sim->treasure_count) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Treasure rows are invalid.");
            return false;
        }
        CcTreasure *treasure = &sim->treasures[slot];
        treasure->id = (CcId)sqlite3_column_int64(statement, 1);
        if (!ReadTextColumn(statement, 2, treasure->name,
                            sizeof(treasure->name), "treasure name",
                            error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        treasure->maker_settlement_id =
            (CcId)sqlite3_column_int64(statement, 3);
        treasure->owner_id = (CcId)sqlite3_column_int64(statement, 4);
        treasure->location_id = (CcId)sqlite3_column_int64(statement, 5);
        treasure->gold_content = sqlite3_column_int(statement, 6);
        treasure->gem_content = sqlite3_column_int(statement, 7);
        treasure->craft_work = sqlite3_column_int(statement, 8);
        treasure->appraised_value = sqlite3_column_int(statement, 9);
        treasure->created_day = sqlite3_column_int(statement, 10);
        treasure->destroyed = sqlite3_column_int(statement, 11) != 0;
        sim->treasure_count += 1;
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadRoutes(sqlite3 *database, CcSim *sim,
                       char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM route ORDER BY slot;", &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_ROUTES) { sqlite3_finalize(statement); return false; }
        CcRoute *r = &sim->routes[slot];
        r->id = (CcId)sqlite3_column_int64(statement, 1);
        r->from_id = (CcId)sqlite3_column_int64(statement, 2);
        r->to_id = (CcId)sqlite3_column_int64(statement, 3);
        r->travel_days = sqlite3_column_int(statement, 4); r->capacity = sqlite3_column_int(statement, 5);
        r->security = sqlite3_column_int(statement, 6); r->condition = sqlite3_column_int(statement, 7);
        r->closed = sqlite3_column_int(statement, 8) != 0;
        r->smuggler_route = sqlite3_column_int(statement, 9) != 0;
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->route_count) { SetError(error, error_capacity, "Route rows are incomplete."); return false; }
    return true;
}

static bool ReadMaps(sqlite3 *database, CcSim *sim,
                     char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM map_object ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_MAPS) {
            SetError(error, error_capacity, "Map rows exceed save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        CcMap *map = &sim->maps[slot];
        map->id = (CcId)sqlite3_column_int64(statement, 1);
        map->route_id = (CcId)sqlite3_column_int64(statement, 2);
        map->maker_settlement_id = (CcId)sqlite3_column_int64(statement, 3);
        map->owner_id = (CcId)sqlite3_column_int64(statement, 4);
        if (!ReadTextColumn(statement, 5, map->name, sizeof(map->name),
                            "map name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        map->surveyed_day = sqlite3_column_int(statement, 6);
        map->accuracy = sqlite3_column_int(statement, 7);
        map->recorded_condition = sqlite3_column_int(statement, 8);
        map->recorded_danger = sqlite3_column_int(statement, 9);
        map->ask_price = sqlite3_column_int(statement, 10);
        map->contraband = sqlite3_column_int(statement, 11) != 0;
        rows += 1;
    }
    sqlite3_finalize(statement);
    sim->map_count = rows;
    return true;
}

static bool ReadMapCollection(sqlite3 *database, CcSim *sim,
                              char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT catalogue_mask,archive_mask "
                 "FROM map_collection WHERE id=1;",
                 &statement, error, error_capacity)) return false;
    int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        sim->player.map_catalogue_mask =
            (uint32_t)sqlite3_column_int(statement, 0);
        sim->player.map_archive_mask =
            (uint32_t)sqlite3_column_int(statement, 1);
    } else if (result != SQLITE_DONE) {
        SetSqlError(error, error_capacity, database,
                    "Could not read the map collection");
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadFactions(sqlite3 *database, CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM faction ORDER BY slot;", &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_FACTIONS) { sqlite3_finalize(statement); return false; }
        CcFaction *f = &sim->factions[slot];
        f->id = (CcId)sqlite3_column_int64(statement, 1);
        f->kingdom_id = (CcId)sqlite3_column_int64(statement, 2);
        if (!ReadTextColumn(statement, 3, f->name, sizeof(f->name),
                            "faction name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        f->kind = (CcFactionKind)sqlite3_column_int(statement, 4);
        f->power = sqlite3_column_int(statement, 5); f->support = sqlite3_column_int(statement, 6);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->faction_count) { SetError(error, error_capacity, "Faction rows are incomplete."); return false; }
    return true;
}

static bool ReadShipments(sqlite3 *database, CcSim *sim,
                          char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM shipment ORDER BY slot;", &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_SHIPMENTS) { sqlite3_finalize(statement); return false; }
        CcShipment *s = &sim->shipments[slot];
        s->id = (CcId)sqlite3_column_int64(statement, 1);
        s->origin_id = (CcId)sqlite3_column_int64(statement, 2);
        s->destination_id = (CcId)sqlite3_column_int64(statement, 3);
        s->final_destination_id = s->destination_id;
        s->route_id = (CcId)sqlite3_column_int64(statement, 4);
        s->good = (CcGood)sqlite3_column_int(statement, 5);
        s->quantity = sqlite3_column_int(statement, 6); s->departure_day = sqlite3_column_int(statement, 7);
        s->arrival_day = sqlite3_column_int(statement, 8);
        s->status = (CcShipmentStatus)sqlite3_column_int(statement, 9);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->shipment_count) { SetError(error, error_capacity, "Shipment rows are incomplete."); return false; }
    if (!Prepare(database, "SELECT slot,final_destination_id FROM shipment_intent ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t intents = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= sim->shipment_count) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Shipment intent rows are invalid.");
            return false;
        }
        sim->shipments[slot].final_destination_id =
            (CcId)sqlite3_column_int64(statement, 1);
        intents += 1;
    }
    sqlite3_finalize(statement);
    if (intents != sim->shipment_count) {
        SetError(error, error_capacity, "Shipment intent rows are incomplete.");
        return false;
    }
    return true;
}

static bool ReadThreats(sqlite3 *database, CcSim *sim,
                        char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM bandit_group ORDER BY slot;", &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_BANDITS) { sqlite3_finalize(statement); return false; }
        CcBanditGroup *b = &sim->bandits[slot];
        b->id = (CcId)sqlite3_column_int64(statement, 1);
        b->route_id = (CcId)sqlite3_column_int64(statement, 2);
        if (!ReadTextColumn(statement, 3, b->name, sizeof(b->name),
                            "bandit name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        b->members = sqlite3_column_int(statement, 4); b->supplies = sqlite3_column_int(statement, 5);
        b->influence = sqlite3_column_int(statement, 6); sim->last_bandit_level[slot] = sqlite3_column_int(statement, 7);
        b->camp_size = (CcBanditCampSize)sqlite3_column_int(statement, 8);
        b->service_mask = (uint32_t)sqlite3_column_int64(statement, 9);
        b->raid_phase = (CcBanditRaidPhase)sqlite3_column_int(statement, 10);
        b->raid_target_id = (CcId)sqlite3_column_int64(statement, 11);
        b->raid_good = (CcGood)sqlite3_column_int(statement, 12);
        b->raid_quantity = sqlite3_column_int(statement, 13);
        b->raid_days_remaining = sqlite3_column_int(statement, 14);
        b->raids_completed = sqlite3_column_int(statement, 15);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->bandit_count) { SetError(error, error_capacity, "Bandit rows are incomplete."); return false; }

    if (!Prepare(database, "SELECT * FROM monster_population ORDER BY slot;", &statement, error, error_capacity)) return false;
    rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_MONSTERS) { sqlite3_finalize(statement); return false; }
        CcMonsterPopulation *m = &sim->monsters[slot];
        m->id = (CcId)sqlite3_column_int64(statement, 1);
        m->dungeon_id = (CcId)sqlite3_column_int64(statement, 2);
        if (!ReadTextColumn(statement, 3, m->name, sizeof(m->name),
                            "monster name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        m->population = sqlite3_column_int(statement, 4); m->pressure = sqlite3_column_int(statement, 5);
        m->hunting_pressure = sqlite3_column_int(statement, 6); sim->last_monster_level[slot] = sqlite3_column_int(statement, 7);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->monster_count) { SetError(error, error_capacity, "Monster rows are incomplete."); return false; }
    return true;
}

static bool ReadDungeons(sqlite3 *database, CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM dungeon ORDER BY slot;", &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_DUNGEONS) { sqlite3_finalize(statement); return false; }
        CcDungeon *d = &sim->dungeons[slot];
        d->id = (CcId)sqlite3_column_int64(statement, 1);
        d->settlement_id = (CcId)sqlite3_column_int64(statement, 2);
        if (!ReadTextColumn(statement, 3, d->name, sizeof(d->name),
                            "dungeon name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        d->state = (CcDungeonState)sqlite3_column_int(statement, 4);
        d->depth = sqlite3_column_int(statement, 5); d->regional_pressure = sqlite3_column_int(statement, 6);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->dungeon_count) { SetError(error, error_capacity, "Dungeon rows are incomplete."); return false; }
    return true;
}

static bool ReadUnderroad(sqlite3 *database, CcSim *sim,
                          char *error, size_t error_capacity)
{
    if (sim->schema_version < 19U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT dungeon_slot,layout_seed,encounter_random_state,"
                 "room_count,link_count FROM dungeon_detail "
                 "ORDER BY dungeon_slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= sim->dungeon_count) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Dungeon detail rows are invalid.");
            return false;
        }
        CcDungeon *dungeon = &sim->dungeons[slot];
        dungeon->layout_seed = (uint32_t)sqlite3_column_int(statement, 1);
        dungeon->encounter_random_state =
            (uint32_t)sqlite3_column_int(statement, 2);
        dungeon->room_count = sqlite3_column_int(statement, 3);
        dungeon->link_count = sqlite3_column_int(statement, 4);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows == 0 && sim->schema_version == 19U) {
        sim->dungeon_expedition.current_room = -1;
        sim->dungeon_expedition.encounter_room = -1;
        return true;
    }
    if (rows != sim->dungeon_count) {
        SetError(error, error_capacity, "Dungeon detail rows are incomplete.");
        return false;
    }

    if (!Prepare(database,
                 "SELECT dungeon_slot,room_slot,name,kind,depth,map_x,map_y,"
                 "flags,state_flags,loot_good,loot_quantity FROM dungeon_room "
                 "ORDER BY dungeon_slot,room_slot;",
                 &statement, error, error_capacity)) return false;
    int32_t expected_rooms = 0;
    int32_t room_rows = 0;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        expected_rooms += sim->dungeons[i].room_count;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t dungeon_slot = sqlite3_column_int(statement, 0);
        int32_t room_slot = sqlite3_column_int(statement, 1);
        if (dungeon_slot < 0 || dungeon_slot >= sim->dungeon_count ||
            room_slot < 0 ||
            room_slot >= sim->dungeons[dungeon_slot].room_count ||
            room_slot >= CC_MAX_DUNGEON_ROOMS) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Dungeon room rows are invalid.");
            return false;
        }
        CcDungeonRoom *room =
            &sim->dungeons[dungeon_slot].rooms[room_slot];
        if (!ReadTextColumn(statement, 2, room->name, sizeof(room->name),
                            "dungeon room name", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        room->kind = (CcDungeonRoomKind)sqlite3_column_int(statement, 3);
        room->depth = sqlite3_column_int(statement, 4);
        room->map_x = sqlite3_column_int(statement, 5);
        room->map_y = sqlite3_column_int(statement, 6);
        room->flags = (uint32_t)sqlite3_column_int(statement, 7);
        room->state_flags = (uint32_t)sqlite3_column_int(statement, 8);
        room->loot_good = (CcGood)sqlite3_column_int(statement, 9);
        room->loot_quantity = sqlite3_column_int(statement, 10);
        room_rows += 1;
    }
    sqlite3_finalize(statement);
    if (room_rows != expected_rooms) {
        SetError(error, error_capacity, "Dungeon room rows are incomplete.");
        return false;
    }

    if (!Prepare(database,
                 "SELECT dungeon_slot,link_slot,from_room,to_room,kind,flags "
                 "FROM dungeon_link ORDER BY dungeon_slot,link_slot;",
                 &statement, error, error_capacity)) return false;
    int32_t expected_links = 0;
    int32_t link_rows = 0;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        expected_links += sim->dungeons[i].link_count;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t dungeon_slot = sqlite3_column_int(statement, 0);
        int32_t link_slot = sqlite3_column_int(statement, 1);
        if (dungeon_slot < 0 || dungeon_slot >= sim->dungeon_count ||
            link_slot < 0 ||
            link_slot >= sim->dungeons[dungeon_slot].link_count ||
            link_slot >= CC_MAX_DUNGEON_LINKS) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Dungeon passage rows are invalid.");
            return false;
        }
        CcDungeonLink *link =
            &sim->dungeons[dungeon_slot].links[link_slot];
        link->from_room = sqlite3_column_int(statement, 2);
        link->to_room = sqlite3_column_int(statement, 3);
        link->kind = (CcDungeonLinkKind)sqlite3_column_int(statement, 4);
        link->flags = (uint32_t)sqlite3_column_int(statement, 5);
        link_rows += 1;
    }
    sqlite3_finalize(statement);
    if (link_rows != expected_links) {
        SetError(error, error_capacity, "Dungeon passage rows are incomplete.");
        return false;
    }

    if (!Prepare(database,
                 "SELECT active,dungeon_id,current_room,turns_elapsed,"
                 "days_elapsed,light_remaining,noise,strain,maximum_depth,"
                 "encounter_kind,encounter_reaction,encounter_room "
                 "FROM dungeon_expedition WHERE slot=1;",
                 &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        SetError(error, error_capacity, "Dungeon expedition state is missing.");
        return false;
    }
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    expedition->active = sqlite3_column_int(statement, 0) != 0;
    expedition->dungeon_id = (CcId)sqlite3_column_int64(statement, 1);
    expedition->current_room = sqlite3_column_int(statement, 2);
    expedition->turns_elapsed = sqlite3_column_int(statement, 3);
    expedition->days_elapsed = sqlite3_column_int(statement, 4);
    expedition->light_remaining = sqlite3_column_int(statement, 5);
    expedition->noise = sqlite3_column_int(statement, 6);
    expedition->strain = sqlite3_column_int(statement, 7);
    expedition->maximum_depth = sqlite3_column_int(statement, 8);
    expedition->encounter_kind =
        (CcDungeonEncounterKind)sqlite3_column_int(statement, 9);
    expedition->encounter_reaction = sqlite3_column_int(statement, 10);
    expedition->encounter_room = sqlite3_column_int(statement, 11);
    sqlite3_finalize(statement);
    return true;
}

static bool ReadLegends(sqlite3 *database, CcSim *sim,
                        char *error, size_t error_capacity)
{
    if (sim->schema_version < 6U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT id,name,members,devotion,tribute_phase,tribute_target_id,"
                 "last_tribute_origin_id,tribute_event_id,carried_tribute,"
                 "tribute_days_remaining,tribute_cooldown_days,tributes_delivered,"
                 "hoard_defenses,cohesion,target_warned,expeditions_intercepted,"
                 "dragon_seed_phase,dragon_seed_days_remaining "
                 "FROM goblin_cult WHERE slot=1;",
                 &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity, "Goblin cult state is missing.");
        sqlite3_finalize(statement);
        return false;
    }
    CcGoblinCult *goblins = &sim->goblins;
    int column = 0;
    goblins->id = (CcId)sqlite3_column_int64(statement, column++);
    if (!ReadTextColumn(statement, column++, goblins->name,
                        sizeof(goblins->name), "goblin name",
                        error, error_capacity)) {
        sqlite3_finalize(statement);
        return false;
    }
    goblins->members = sqlite3_column_int(statement, column++);
    goblins->devotion = sqlite3_column_int(statement, column++);
    goblins->tribute_phase =
        (CcGoblinTributePhase)sqlite3_column_int(statement, column++);
    goblins->tribute_target_id =
        (CcId)sqlite3_column_int64(statement, column++);
    goblins->last_tribute_origin_id =
        (CcId)sqlite3_column_int64(statement, column++);
    goblins->tribute_event_id =
        (CcId)sqlite3_column_int64(statement, column++);
    goblins->carried_tribute =
        (CcMoney)sqlite3_column_int64(statement, column++);
    goblins->tribute_days_remaining =
        sqlite3_column_int(statement, column++);
    goblins->tribute_cooldown_days =
        sqlite3_column_int(statement, column++);
    goblins->tributes_delivered = sqlite3_column_int(statement, column++);
    goblins->hoard_defenses = sqlite3_column_int(statement, column++);
    goblins->cohesion = sqlite3_column_int(statement, column++);
    goblins->target_warned = sqlite3_column_int(statement, column++) != 0;
    goblins->expeditions_intercepted =
        sqlite3_column_int(statement, column++);
    goblins->dragon_seed_phase =
        (CcGoblinDragonSeedPhase)sqlite3_column_int(statement, column++);
    goblins->dragon_seed_days_remaining =
        sqlite3_column_int(statement, column++);
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "SELECT id,name,lair_settlement_id,hoard,stolen_outstanding,"
                 "theft_actor_id,retaliation_target_id,hoard_event_id,omen_event_id,"
                 "omen_days_remaining,retaliations,slain,slain_day,life_stage,"
                 "activity,age_days,body_condition,crown_strength,memory_integrity,"
                 "territory_stability,regional_influence,crown_continuity_days,"
                 "hunt_cooldown_days,hunts,egg_count,brood_days_remaining,"
                 "brood_cooldown_days,broods_laid,whelps_dispersed,afterdeath_days,"
                 "lifecycle_event_id "
                 "FROM dragon_state WHERE slot=1;",
                 &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity, "Dragon state is missing.");
        sqlite3_finalize(statement);
        return false;
    }
    CcDragon *dragon = &sim->dragon;
    column = 0;
    dragon->id = (CcId)sqlite3_column_int64(statement, column++);
    if (!ReadTextColumn(statement, column++, dragon->name,
                        sizeof(dragon->name), "dragon name",
                        error, error_capacity)) {
        sqlite3_finalize(statement);
        return false;
    }
    dragon->lair_settlement_id =
        (CcId)sqlite3_column_int64(statement, column++);
    dragon->hoard = (CcMoney)sqlite3_column_int64(statement, column++);
    dragon->stolen_outstanding =
        (CcMoney)sqlite3_column_int64(statement, column++);
    dragon->theft_actor_id =
        (CcId)sqlite3_column_int64(statement, column++);
    dragon->retaliation_target_id =
        (CcId)sqlite3_column_int64(statement, column++);
    dragon->hoard_event_id =
        (CcId)sqlite3_column_int64(statement, column++);
    dragon->omen_event_id =
        (CcId)sqlite3_column_int64(statement, column++);
    dragon->omen_days_remaining =
        sqlite3_column_int(statement, column++);
    dragon->retaliations = sqlite3_column_int(statement, column++);
    dragon->slain = sqlite3_column_int(statement, column++) != 0;
    dragon->slain_day = sqlite3_column_int(statement, column++);
    dragon->life_stage =
        (CcDragonLifeStage)sqlite3_column_int(statement, column++);
    dragon->activity =
        (CcDragonActivity)sqlite3_column_int(statement, column++);
    dragon->age_days = sqlite3_column_int(statement, column++);
    dragon->body_condition = sqlite3_column_int(statement, column++);
    dragon->crown_strength = sqlite3_column_int(statement, column++);
    dragon->memory_integrity = sqlite3_column_int(statement, column++);
    dragon->territory_stability = sqlite3_column_int(statement, column++);
    dragon->regional_influence = sqlite3_column_int(statement, column++);
    dragon->crown_continuity_days =
        sqlite3_column_int(statement, column++);
    dragon->hunt_cooldown_days = sqlite3_column_int(statement, column++);
    dragon->hunts = sqlite3_column_int(statement, column++);
    dragon->egg_count = sqlite3_column_int(statement, column++);
    dragon->brood_days_remaining = sqlite3_column_int(statement, column++);
    dragon->brood_cooldown_days = sqlite3_column_int(statement, column++);
    dragon->broods_laid = sqlite3_column_int(statement, column++);
    dragon->whelps_dispersed = sqlite3_column_int(statement, column++);
    dragon->afterdeath_days = sqlite3_column_int(statement, column++);
    dragon->lifecycle_event_id =
        (CcId)sqlite3_column_int64(statement, column++);
    sqlite3_finalize(statement);

    if (sim->schema_version >= 11U) {
        if (!Prepare(database,
                     "SELECT phase,pledged_mask,alliance_mask,"
                     "origin_settlement_id,cause_event_id,days_remaining,"
                     "cooldown_days,food,iron,tools,weapons,gold,gems,"
                     "recovered_coins,attempts,victories,defeats "
                     "FROM dragon_campaign WHERE id=1;",
                     &statement, error, error_capacity)) return false;
        if (sqlite3_step(statement) != SQLITE_ROW) {
            SetError(error, error_capacity,
                     "Dragon campaign state is missing.");
            sqlite3_finalize(statement);
            return false;
        }
        CcDragonCampaign *campaign = &sim->dragon_campaign;
        column = 0;
        campaign->phase =
            (CcDragonCampaignPhase)sqlite3_column_int(statement, column++);
        campaign->pledged_kingdom_mask =
            (uint32_t)sqlite3_column_int64(statement, column++);
        campaign->alliance_kingdom_mask =
            (uint32_t)sqlite3_column_int64(statement, column++);
        campaign->origin_settlement_id =
            (CcId)sqlite3_column_int64(statement, column++);
        campaign->cause_event_id =
            (CcId)sqlite3_column_int64(statement, column++);
        campaign->days_remaining = sqlite3_column_int(statement, column++);
        campaign->cooldown_days = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            campaign->supplies[good] =
                sqlite3_column_int(statement, column++);
        }
        campaign->recovered_coins =
            (CcMoney)sqlite3_column_int64(statement, column++);
        campaign->attempts = sqlite3_column_int(statement, column++);
        campaign->victories = sqlite3_column_int(statement, column++);
        campaign->defeats = sqlite3_column_int(statement, column++);
        sqlite3_finalize(statement);
    }
    if (sim->schema_version < 7U) return true;

    if (!Prepare(database,
                 "SELECT id,name,phase,motive,origin_settlement_id,cause_event_id,"
                 "carried_treasure,days_remaining,cooldown_days,raids_completed,"
                 "war_raids_completed,social_raid_latched,war_raid_latched "
                 "FROM hoard_raiders WHERE slot=1;",
                 &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity, "Social hoard-raider state is missing.");
        sqlite3_finalize(statement);
        return false;
    }
    CcHoardRaiders *raiders = &sim->hoard_raiders;
    column = 0;
    raiders->id = (CcId)sqlite3_column_int64(statement, column++);
    if (!ReadTextColumn(statement, column++, raiders->name,
                        sizeof(raiders->name), "hoard-raider name",
                        error, error_capacity)) {
        sqlite3_finalize(statement);
        return false;
    }
    raiders->phase =
        (CcHoardRaiderPhase)sqlite3_column_int(statement, column++);
    raiders->motive =
        (CcHoardRaidMotive)sqlite3_column_int(statement, column++);
    raiders->origin_settlement_id =
        (CcId)sqlite3_column_int64(statement, column++);
    raiders->cause_event_id =
        (CcId)sqlite3_column_int64(statement, column++);
    raiders->carried_treasure =
        (CcMoney)sqlite3_column_int64(statement, column++);
    raiders->days_remaining = sqlite3_column_int(statement, column++);
    raiders->cooldown_days = sqlite3_column_int(statement, column++);
    raiders->raids_completed = sqlite3_column_int(statement, column++);
    raiders->war_raids_completed = sqlite3_column_int(statement, column++);
    raiders->social_raid_latched =
        sqlite3_column_int(statement, column++) != 0;
    raiders->war_raid_latched =
        sqlite3_column_int(statement, column++) != 0;
    sqlite3_finalize(statement);
    if (raiders->phase != CC_HOARD_RAIDERS_IDLE &&
        raiders->motive == CC_HOARD_RAID_NO_MOTIVE) {
        raiders->motive = CC_HOARD_RAID_SOCIAL_RELIEF;
    }
    return true;
}

static bool ReadEvents(sqlite3 *database, CcSim *sim,
                       char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM causal_event ORDER BY slot;", &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_EVENTS) { sqlite3_finalize(statement); return false; }
        CcEvent *e = &sim->events[slot];
        e->id = (CcId)sqlite3_column_int64(statement, 1); e->day = sqlite3_column_int(statement, 2);
        e->kind = (CcEventKind)sqlite3_column_int(statement, 3);
        e->subject_id = (CcId)sqlite3_column_int64(statement, 4);
        e->location_id = (CcId)sqlite3_column_int64(statement, 5);
        e->parent_id = (CcId)sqlite3_column_int64(statement, 6);
        e->magnitude = sqlite3_column_int(statement, 7);
        if (!ReadTextColumn(statement, 8, e->text, sizeof(e->text),
                            "event text", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        if (sim->schema_version >= 19U) {
            e->actor_id = (CcId)sqlite3_column_int64(statement, 9);
            e->target_id = (CcId)sqlite3_column_int64(statement, 10);
            e->beneficiary_id = (CcId)sqlite3_column_int64(statement, 11);
            e->witness_id = (CcId)sqlite3_column_int64(statement, 12);
        }
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->event_count) { SetError(error, error_capacity, "Causal-event rows are incomplete."); return false; }
    return true;
}

static bool ReadSituations(sqlite3 *database, CcSim *sim,
                           char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM situation ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_SITUATIONS) {
            SetError(error, error_capacity, "Situation rows exceed save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        CcSituation *s = &sim->situations[slot];
        int column = 1;
        s->id = (CcId)sqlite3_column_int64(statement, column++);
        s->kind = (CcSituationKind)sqlite3_column_int(statement, column++);
        s->status = (CcSituationStatus)sqlite3_column_int(statement, column++);
        s->issuer_faction_id = (CcId)sqlite3_column_int64(statement, column++);
        s->target_id = (CcId)sqlite3_column_int64(statement, column++);
        s->cause_event_id = (CcId)sqlite3_column_int64(statement, column++);
        s->good = (CcGood)sqlite3_column_int(statement, column++);
        s->quantity = sqlite3_column_int(statement, column++);
        s->progress = sqlite3_column_int(statement, column++);
        s->reward = (CcMoney)sqlite3_column_int64(statement, column++);
        s->created_day = sqlite3_column_int(statement, column++);
        s->deadline_day = sqlite3_column_int(statement, column++);
        if (sim->schema_version >= 19U) {
            s->discovery_stage =
                (CcSituationDiscoveryStage)sqlite3_column_int(statement,
                                                               column++);
            s->lead_path =
                (CcSituationLeadPath)sqlite3_column_int(statement, column++);
            s->lead_event_id =
                (CcId)sqlite3_column_int64(statement, column++);
        }
        rows += 1;
    }
    sqlite3_finalize(statement);
    sim->situation_count = rows;
    return true;
}

static bool ReadSituationCasts(sqlite3 *database, CcSim *sim,
                               char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT slot,situation_id,sponsor_name,affected_name "
                 "FROM situation_cast ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        CcId situation_id = (CcId)sqlite3_column_int64(statement, 1);
        if (slot < 0 || slot >= sim->situation_count ||
            sim->situations[slot].id != situation_id) {
            SetError(error, error_capacity,
                     "Situation cast does not match its saved charter.");
            sqlite3_finalize(statement);
            return false;
        }
        CcSituation *situation = &sim->situations[slot];
        if (!ReadTextColumn(statement, 2, situation->sponsor_name,
                            sizeof(situation->sponsor_name),
                            "situation sponsor", error, error_capacity) ||
            !ReadTextColumn(statement, 3, situation->affected_name,
                            sizeof(situation->affected_name),
                            "situation participant", error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadQuestArchitecture(sqlite3 *database, CcSim *sim,
                                  char *error, size_t error_capacity)
{
    if (sim->schema_version < 19U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT * FROM situation_quest ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        CcId situation_id = (CcId)sqlite3_column_int64(statement, 1);
        if (slot != rows || slot < 0 || slot >= sim->situation_count ||
            sim->situations[slot].id != situation_id) {
            SetError(error, error_capacity,
                     "Quest objective does not match its situation.");
            sqlite3_finalize(statement);
            return false;
        }
        CcSituation *situation = &sim->situations[slot];
        CcQuestObjective *objective = &situation->objective;
        situation->front_id = (CcId)sqlite3_column_int64(statement, 2);
        situation->end_reason =
            (CcQuestEndReason)sqlite3_column_int(statement, 3);
        objective->kind =
            (CcQuestObjectiveKind)sqlite3_column_int(statement, 4);
        objective->target_id = (CcId)sqlite3_column_int64(statement, 5);
        objective->good = (CcGood)sqlite3_column_int(statement, 6);
        objective->required = sqlite3_column_int(statement, 7);
        objective->progress.value = sqlite3_column_int(statement, 8);
        objective->progress.limit = sqlite3_column_int(statement, 9);
        objective->progress.created_by_event_id =
            (CcId)sqlite3_column_int64(statement, 10);
        objective->progress.resolved_by_event_id =
            (CcId)sqlite3_column_int64(statement, 11);
        objective->danger.value = sqlite3_column_int(statement, 12);
        objective->danger.limit = sqlite3_column_int(statement, 13);
        objective->danger.created_by_event_id =
            (CcId)sqlite3_column_int64(statement, 14);
        objective->danger.resolved_by_event_id =
            (CcId)sqlite3_column_int64(statement, 15);
        objective->evidence_count = sqlite3_column_int(statement, 16);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows == 0 && sim->schema_version < CC_SIM_SCHEMA_VERSION) {
        return true;
    }
    if (rows != sim->situation_count) {
        SetError(error, error_capacity,
                 "Quest objectives are missing from the save.");
        return false;
    }

    int32_t evidence_rows[CC_MAX_SITUATIONS] = {0};
    if (!Prepare(database,
                 "SELECT situation_slot,evidence_slot,event_id "
                 "FROM situation_evidence ORDER BY situation_slot,evidence_slot;",
                 &statement, error, error_capacity)) return false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        int32_t evidence = sqlite3_column_int(statement, 1);
        if (slot < 0 || slot >= sim->situation_count || evidence < 0 ||
            evidence >= CC_MAX_QUEST_EVIDENCE ||
            evidence != evidence_rows[slot] ||
            evidence >= sim->situations[slot].objective.evidence_count) {
            SetError(error, error_capacity,
                     "Quest evidence rows exceed save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        sim->situations[slot].objective.evidence_event_ids[evidence] =
            (CcId)sqlite3_column_int64(statement, 2);
        evidence_rows[slot] += 1;
    }
    sqlite3_finalize(statement);
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (evidence_rows[i] !=
            sim->situations[i].objective.evidence_count) {
            SetError(error, error_capacity,
                     "Quest evidence is missing from the save.");
            return false;
        }
    }

    if (!Prepare(database,
                 "SELECT * FROM story_front ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot != rows || slot < 0 || slot >= CC_MAX_FRONTS) {
            SetError(error, error_capacity,
                     "Story front rows exceed save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        CcFront *front = &sim->fronts[slot];
        front->id = (CcId)sqlite3_column_int64(statement, 1);
        front->kind = (CcFrontKind)sqlite3_column_int(statement, 2);
        front->status = (CcFrontStatus)sqlite3_column_int(statement, 3);
        front->outcome = (CcFrontOutcome)sqlite3_column_int(statement, 4);
        front->anchor_id = (CcId)sqlite3_column_int64(statement, 5);
        front->cause_event_id = (CcId)sqlite3_column_int64(statement, 6);
        front->created_event_id = (CcId)sqlite3_column_int64(statement, 7);
        front->resolved_event_id = (CcId)sqlite3_column_int64(statement, 8);
        front->created_day = sqlite3_column_int(statement, 9);
        front->resolved_day = sqlite3_column_int(statement, 10);
        front->portent.value = sqlite3_column_int(statement, 11);
        front->portent.limit = sqlite3_column_int(statement, 12);
        front->portent.created_by_event_id =
            (CcId)sqlite3_column_int64(statement, 13);
        front->portent.resolved_by_event_id =
            (CcId)sqlite3_column_int64(statement, 14);
        front->situation_count = sqlite3_column_int(statement, 15);
        if (!ReadTextColumn(statement, 16, front->premise,
                            sizeof(front->premise), "front premise",
                            error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        rows += 1;
    }
    sqlite3_finalize(statement);
    sim->front_count = rows;

    int32_t member_rows[CC_MAX_FRONTS] = {0};
    if (!Prepare(database,
                 "SELECT front_slot,member_slot,situation_id "
                 "FROM front_situation ORDER BY front_slot,member_slot;",
                 &statement, error, error_capacity)) return false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        int32_t member = sqlite3_column_int(statement, 1);
        if (slot < 0 || slot >= sim->front_count || member < 0 ||
            member >= CC_MAX_FRONT_SITUATIONS ||
            member != member_rows[slot] ||
            member >= sim->fronts[slot].situation_count) {
            SetError(error, error_capacity,
                     "Story front membership exceeds save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        sim->fronts[slot].situation_ids[member] =
            (CcId)sqlite3_column_int64(statement, 2);
        member_rows[slot] += 1;
    }
    sqlite3_finalize(statement);
    for (int32_t i = 0; i < sim->front_count; ++i) {
        if (member_rows[i] != sim->fronts[i].situation_count) {
            SetError(error, error_capacity,
                     "Story front membership is missing from the save.");
            return false;
        }
    }

    if (!Prepare(database,
                 "SELECT * FROM quest_outcome ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot != rows || slot < 0 || slot >= CC_MAX_QUEST_OUTCOMES) {
            SetError(error, error_capacity,
                     "Quest outcome rows exceed save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[slot];
        outcome->id = (CcId)sqlite3_column_int64(statement, 1);
        outcome->situation_id = (CcId)sqlite3_column_int64(statement, 2);
        outcome->front_id = (CcId)sqlite3_column_int64(statement, 3);
        outcome->situation_kind =
            (CcSituationKind)sqlite3_column_int(statement, 4);
        outcome->front_kind =
            (CcFrontKind)sqlite3_column_int(statement, 5);
        outcome->situation_status =
            (CcSituationStatus)sqlite3_column_int(statement, 6);
        outcome->end_reason =
            (CcQuestEndReason)sqlite3_column_int(statement, 7);
        outcome->front_outcome =
            (CcFrontOutcome)sqlite3_column_int(statement, 8);
        outcome->target_id = (CcId)sqlite3_column_int64(statement, 9);
        outcome->sponsor_character_id =
            (CcId)sqlite3_column_int64(statement, 10);
        outcome->affected_character_id =
            (CcId)sqlite3_column_int64(statement, 11);
        outcome->cause_event_id =
            (CcId)sqlite3_column_int64(statement, 12);
        outcome->resolved_event_id =
            (CcId)sqlite3_column_int64(statement, 13);
        outcome->resolved_day = sqlite3_column_int(statement, 14);
        outcome->progress_value = sqlite3_column_int(statement, 15);
        outcome->progress_limit = sqlite3_column_int(statement, 16);
        outcome->danger_value = sqlite3_column_int(statement, 17);
        outcome->danger_limit = sqlite3_column_int(statement, 18);
        rows += 1;
    }
    sqlite3_finalize(statement);
    sim->quest_outcome_count = rows;

    if (!Prepare(database,
                 "SELECT * FROM delayed_echo_queue ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot != rows || slot < 0 || slot >= CC_MAX_PENDING_ECHOES) {
            SetError(error, error_capacity,
                     "Delayed echo queue exceeds save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        CcDelayedEcho *echo = &sim->pending_echoes[slot];
        echo->active = sqlite3_column_int(statement, 1) != 0;
        echo->situation_id = (CcId)sqlite3_column_int64(statement, 2);
        echo->settlement_id = (CcId)sqlite3_column_int64(statement, 3);
        echo->parent_event_id = (CcId)sqlite3_column_int64(statement, 4);
        echo->outcome =
            (CcJourneyOutcome)sqlite3_column_int(statement, 5);
        echo->due_day = sqlite3_column_int(statement, 6);
        if (!ReadTextColumn(statement, 7, echo->character_name,
                            sizeof(echo->character_name),
                            "queued echo character", error,
                            error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        rows += 1;
    }
    sqlite3_finalize(statement);
    sim->pending_echo_count = rows;
    return true;
}

static bool ReadCharacters(sqlite3 *database, CcSim *sim,
                           char *error, size_t error_capacity)
{
    if (sim->schema_version < 17U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT * FROM npc_character ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot != rows || slot < 0 || slot >= CC_MAX_CHARACTERS) {
            SetError(error, error_capacity,
                     "Character rows exceed save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        CcCharacter *character = &sim->characters[slot];
        character->id = (CcId)sqlite3_column_int64(statement, 1);
        if (!ReadTextColumn(statement, 2, character->name,
                            sizeof(character->name), "character name",
                            error, error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
        character->home_settlement_id =
            (CcId)sqlite3_column_int64(statement, 3);
        character->current_settlement_id =
            (CcId)sqlite3_column_int64(statement, 4);
        character->faction_id = (CcId)sqlite3_column_int64(statement, 5);
        character->role =
            (CcCharacterRole)sqlite3_column_int(statement, 6);
        character->goal =
            (CcCharacterGoal)sqlite3_column_int(statement, 7);
        character->activity =
            (CcCharacterActivity)sqlite3_column_int(statement, 8);
        character->appearance_seed =
            (uint32_t)sqlite3_column_int(statement, 9);
        character->player_disposition = sqlite3_column_int(statement, 10);
        character->stress = sqlite3_column_int(statement, 11);
        character->courage = sqlite3_column_int(statement, 12);
        character->memory_count = sqlite3_column_int(statement, 13);
        character->memory_write_index = sqlite3_column_int(statement, 14);
        if (sim->schema_version >= 19U) {
            character->knowledge_count = sqlite3_column_int(statement, 15);
            character->knowledge_write_index = sqlite3_column_int(statement, 16);
        }
        rows += 1;
    }
    sqlite3_finalize(statement);
    sim->character_count = rows;

    if (!Prepare(database,
                 "SELECT character_slot,memory_slot,kind,subject_id,event_id,day "
                 "FROM character_memory ORDER BY character_slot,memory_slot;",
                 &statement, error, error_capacity)) return false;
    int32_t memory_rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t character_slot = sqlite3_column_int(statement, 0);
        int32_t memory_slot = sqlite3_column_int(statement, 1);
        if (character_slot < 0 || character_slot >= sim->character_count ||
            memory_slot < 0 ||
            memory_slot >= CC_CHARACTER_MEMORY_CAPACITY) {
            SetError(error, error_capacity,
                     "Character memory rows exceed save limits.");
            sqlite3_finalize(statement);
            return false;
        }
        CcCharacterMemory *memory =
            &sim->characters[character_slot].memories[memory_slot];
        memory->kind =
            (CcCharacterMemoryKind)sqlite3_column_int(statement, 2);
        memory->subject_id = (CcId)sqlite3_column_int64(statement, 3);
        memory->event_id = (CcId)sqlite3_column_int64(statement, 4);
        memory->day = sqlite3_column_int(statement, 5);
        memory_rows += 1;
    }
    sqlite3_finalize(statement);
    if (memory_rows != sim->character_count *
                       CC_CHARACTER_MEMORY_CAPACITY) {
        SetError(error, error_capacity,
                 "Character memory rows are incomplete.");
        return false;
    }

    if (sim->schema_version >= 19U) {
        if (!Prepare(database,
                     "SELECT character_slot,knowledge_slot,kind,subject_id,"
                     "source_character_id,event_id,certainty,private_knowledge,day "
                     "FROM character_knowledge "
                     "ORDER BY character_slot,knowledge_slot;",
                     &statement, error, error_capacity)) return false;
        int32_t knowledge_rows = 0;
        while (sqlite3_step(statement) == SQLITE_ROW) {
            int32_t character_slot = sqlite3_column_int(statement, 0);
            int32_t knowledge_slot = sqlite3_column_int(statement, 1);
            if (character_slot < 0 ||
                character_slot >= sim->character_count ||
                knowledge_slot < 0 ||
                knowledge_slot >= CC_CHARACTER_KNOWLEDGE_CAPACITY) {
                SetError(error, error_capacity,
                         "Character knowledge rows exceed save limits.");
                sqlite3_finalize(statement);
                return false;
            }
            CcCharacterKnowledge *knowledge =
                &sim->characters[character_slot].knowledge[knowledge_slot];
            knowledge->kind =
                (CcKnowledgeKind)sqlite3_column_int(statement, 2);
            knowledge->subject_id =
                (CcId)sqlite3_column_int64(statement, 3);
            knowledge->source_character_id =
                (CcId)sqlite3_column_int64(statement, 4);
            knowledge->event_id =
                (CcId)sqlite3_column_int64(statement, 5);
            knowledge->certainty =
                (CcKnowledgeCertainty)sqlite3_column_int(statement, 6);
            knowledge->private_knowledge =
                sqlite3_column_int(statement, 7) != 0;
            knowledge->day = sqlite3_column_int(statement, 8);
            knowledge_rows += 1;
        }
        sqlite3_finalize(statement);
        int32_t expected_knowledge_rows = sim->character_count *
            CC_CHARACTER_KNOWLEDGE_CAPACITY;
        bool empty_legacy_social_data = sim->schema_version == 19U &&
            knowledge_rows == 0;
        if (knowledge_rows != expected_knowledge_rows &&
            !empty_legacy_social_data) {
            SetError(error, error_capacity,
                     "Character knowledge rows are incomplete.");
            return false;
        }
    }

    if (!Prepare(database,
                 "SELECT slot,situation_id,sponsor_character_id,"
                 "affected_character_id,witness_character_id "
                 "FROM situation_character "
                 "ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t situation_rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        CcId situation_id = (CcId)sqlite3_column_int64(statement, 1);
        if (slot < 0 || slot >= sim->situation_count ||
            sim->situations[slot].id != situation_id) {
            SetError(error, error_capacity,
                     "Situation character rows do not match their charters.");
            sqlite3_finalize(statement);
            return false;
        }
        sim->situations[slot].sponsor_character_id =
            (CcId)sqlite3_column_int64(statement, 2);
        sim->situations[slot].affected_character_id =
            (CcId)sqlite3_column_int64(statement, 3);
        if (sim->schema_version >= 19U) {
            sim->situations[slot].witness_character_id =
                (CcId)sqlite3_column_int64(statement, 4);
        }
        situation_rows += 1;
    }
    sqlite3_finalize(statement);
    if (situation_rows != sim->situation_count) {
        SetError(error, error_capacity,
                 "Situation character rows are incomplete.");
        return false;
    }
    if (sim->schema_version >= 19U) {
        if (!Prepare(database,
                     "SELECT slot,from_character_id,to_character_id,affinity,"
                     "trust,obligation,history,cause_event_id "
                     "FROM character_relationship ORDER BY slot;",
                     &statement, error, error_capacity)) return false;
        int32_t relationship_rows = 0;
        while (sqlite3_step(statement) == SQLITE_ROW) {
            int32_t slot = sqlite3_column_int(statement, 0);
            if (slot != relationship_rows || slot < 0 ||
                slot >= CC_MAX_RELATIONSHIPS) {
                SetError(error, error_capacity,
                         "Relationship rows exceed save limits.");
                sqlite3_finalize(statement);
                return false;
            }
            CcRelationship *relationship = &sim->relationships[slot];
            relationship->from_character_id =
                (CcId)sqlite3_column_int64(statement, 1);
            relationship->to_character_id =
                (CcId)sqlite3_column_int64(statement, 2);
            relationship->affinity = sqlite3_column_int(statement, 3);
            relationship->trust = sqlite3_column_int(statement, 4);
            relationship->obligation = sqlite3_column_int(statement, 5);
            relationship->history =
                (CcRelationshipHistory)sqlite3_column_int(statement, 6);
            relationship->cause_event_id =
                (CcId)sqlite3_column_int64(statement, 7);
            relationship_rows += 1;
        }
        sqlite3_finalize(statement);
        sim->relationship_count = relationship_rows;
    }
    return true;
}

static bool ReadPlayer(sqlite3 *database, CcSim *sim,
                       char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM player_company LIMIT 1;", &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity, "Player company row is missing."); sqlite3_finalize(statement); return false;
    }
    CcPlayerCompany *p = &sim->player;
    p->id = (CcId)sqlite3_column_int64(statement, 0);
    p->location_id = (CcId)sqlite3_column_int64(statement, 1);
    p->coins = (CcMoney)sqlite3_column_int64(statement, 2);
    p->cargo[CC_GOOD_FOOD] = sqlite3_column_int(statement, 3);
    p->cargo[CC_GOOD_MATERIAL] = sqlite3_column_int(statement, 4);
    p->cargo[CC_GOOD_TOOLS] = sqlite3_column_int(statement, 5);
    p->cargo_capacity = sqlite3_column_int(statement, 6);
    p->passenger_capacity = sqlite3_column_int(statement, 7);
    p->map_capacity = CC_MAP_CAPACITY;
    p->reputation = sqlite3_column_int(statement, 8);
    sqlite3_finalize(statement);
    return true;
}

static bool ReadPlayerCommitment(sqlite3 *database, CcSim *sim,
                                 char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT situation_id FROM player_commitment WHERE id=1;",
                 &statement, error, error_capacity)) return false;
    int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        sim->player.accepted_situation_id =
            (CcId)sqlite3_column_int64(statement, 0);
    } else if (result == SQLITE_DONE) {

        sim->player.accepted_situation_id = 0U;
    } else {
        SetSqlError(error, error_capacity, database,
                    "Could not read player commitment");
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadJourneyState(sqlite3 *database, CcSim *sim,
                             char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT active,situation_id,origin_id,destination_id,route_id,"
                 "danger,bargain_cost,resolved_situation_id,resolved_outcome "
                 "FROM player_journey WHERE id=1;",
                 &statement, error, error_capacity)) return false;
    int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        sim->journey.active = sqlite3_column_int(statement, 0) != 0;
        sim->journey.situation_id = (CcId)sqlite3_column_int64(statement, 1);
        sim->journey.origin_id = (CcId)sqlite3_column_int64(statement, 2);
        sim->journey.destination_id = (CcId)sqlite3_column_int64(statement, 3);
        sim->journey.route_id = (CcId)sqlite3_column_int64(statement, 4);
        sim->journey.danger = sqlite3_column_int(statement, 5);
        sim->journey.bargain_cost = sqlite3_column_int(statement, 6);
        sim->resolved_journey_situation_id =
            (CcId)sqlite3_column_int64(statement, 7);
        sim->resolved_journey_outcome =
            (CcJourneyOutcome)sqlite3_column_int(statement, 8);
    } else if (result != SQLITE_DONE) {
        SetSqlError(error, error_capacity, database,
                    "Could not read prepared journey");
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "SELECT clock_tick,minute_subticks,game_minutes_per_second,"
                 "journey_phase,departure_day,elapsed_subticks,total_subticks,"
                 "encounter_subticks,fare_reserved,encounter_triggered,"
                 "ambush_pending,ambush_resolved,parent_event_id,carriage_mode,"
                 "carriage_location_id,carriage_route_id,carriage_origin_id,"
                 "carriage_destination_id,carriage_progress_milli,"
                 "carriage_speed_milli_per_second,carriage_condition,"
                 "journey_pace,ambush_warned "
                 "FROM runtime_state WHERE id=1;",
                 &statement, error, error_capacity)) return false;
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        int column = 0;
        sim->clock.tick = (uint64_t)sqlite3_column_int64(statement, column++);
        sim->clock.minute_subticks = sqlite3_column_int(statement, column++);
        sim->clock.game_minutes_per_second =
            sqlite3_column_int(statement, column++);
        sim->journey.phase =
            (CcJourneyPhase)sqlite3_column_int(statement, column++);
        sim->journey.departure_day = sqlite3_column_int(statement, column++);
        sim->journey.elapsed_subticks = sqlite3_column_int(statement, column++);
        sim->journey.total_subticks = sqlite3_column_int(statement, column++);
        sim->journey.encounter_subticks =
            sqlite3_column_int(statement, column++);
        sim->journey.fare_reserved = sqlite3_column_int(statement, column++);
        sim->journey.encounter_triggered =
            sqlite3_column_int(statement, column++) != 0;
        sim->journey.ambush_pending =
            sqlite3_column_int(statement, column++) != 0;
        sim->journey.ambush_resolved =
            sqlite3_column_int(statement, column++) != 0;
        sim->journey.parent_event_id =
            (CcId)sqlite3_column_int64(statement, column++);
        sim->carriage.mode =
            (CcCarriageMode)sqlite3_column_int(statement, column++);
        sim->carriage.location_id =
            (CcId)sqlite3_column_int64(statement, column++);
        sim->carriage.route_id =
            (CcId)sqlite3_column_int64(statement, column++);
        sim->carriage.origin_id =
            (CcId)sqlite3_column_int64(statement, column++);
        sim->carriage.destination_id =
            (CcId)sqlite3_column_int64(statement, column++);
        sim->carriage.progress_milli =
            sqlite3_column_int(statement, column++);
        sim->carriage.speed_milli_per_second =
            sqlite3_column_int(statement, column++);
        sim->carriage.condition = sqlite3_column_int(statement, column++);
        sim->journey.pace =
            (CcJourneyPace)sqlite3_column_int(statement, column++);
        sim->journey.ambush_warned =
            sqlite3_column_int(statement, column++) != 0;
    } else if (result != SQLITE_DONE) {
        SetSqlError(error, error_capacity, database,
                    "Could not read world runtime state");
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "SELECT active,situation_id,settlement_id,parent_event_id,"
                 "outcome,due_day,character_name FROM delayed_echo WHERE id=1;",
                 &statement, error, error_capacity)) return false;
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        sim->delayed_echo.active = sqlite3_column_int(statement, 0) != 0;
        sim->delayed_echo.situation_id =
            (CcId)sqlite3_column_int64(statement, 1);
        sim->delayed_echo.settlement_id =
            (CcId)sqlite3_column_int64(statement, 2);
        sim->delayed_echo.parent_event_id =
            (CcId)sqlite3_column_int64(statement, 3);
        sim->delayed_echo.outcome =
            (CcJourneyOutcome)sqlite3_column_int(statement, 4);
        sim->delayed_echo.due_day = sqlite3_column_int(statement, 5);
        if (!ReadTextColumn(statement, 6,
                            sim->delayed_echo.character_name,
                            sizeof(sim->delayed_echo.character_name),
                            "delayed-echo character", error,
                            error_capacity)) {
            sqlite3_finalize(statement);
            return false;
        }
    } else if (result != SQLITE_DONE) {
        SetSqlError(error, error_capacity, database,
                    "Could not read delayed echo");
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ValidateJournalCheckpoint(sqlite3 *database, const CcSim *sim,
                                      uint64_t generation, uint64_t cursor,
                                      char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT record_version,world_seed,initial_state_hash "
                 "FROM journal_epoch WHERE generation=?;",
                 &statement, error, error_capacity)) return false;
    (void)sqlite3_bind_int64(statement, 1, (sqlite3_int64)generation);
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity,
                 "Journal checkpoint references a missing epoch.");
        sqlite3_finalize(statement);
        return false;
    }
    int32_t record_version = sqlite3_column_int(statement, 0);
    uint32_t world_seed = (uint32_t)sqlite3_column_int(statement, 1);
    uint64_t checkpoint_hash = 0U;
    bool parsed = ParseStoredHash(sqlite3_column_text(statement, 2),
                                  &checkpoint_hash);
    sqlite3_finalize(statement);
    if (record_version != CC_JOURNAL_RECORD_VERSION ||
        world_seed != sim->world_seed || !parsed) {
        SetError(error, error_capacity,
                 "Journal epoch does not match the campaign checkpoint.");
        return false;
    }
    if (cursor > 0U) {
        if (!Prepare(database,
                     "SELECT post_state_hash FROM action_journal "
                     "WHERE generation=? AND ordinal=?;",
                     &statement, error, error_capacity)) return false;
        (void)sqlite3_bind_int64(statement, 1, (sqlite3_int64)generation);
        (void)sqlite3_bind_int64(statement, 2, (sqlite3_int64)cursor);
        if (sqlite3_step(statement) != SQLITE_ROW ||
            !ParseStoredHash(sqlite3_column_text(statement, 0),
                             &checkpoint_hash)) {
            SetError(error, error_capacity,
                     "Journal checkpoint cursor is missing or corrupt.");
            sqlite3_finalize(statement);
            return false;
        }
        sqlite3_finalize(statement);
    }
    if (CcSimHash(sim) != checkpoint_hash) {
        SetError(error, error_capacity,
                 "Journal checkpoint hash does not match the snapshot.");
        return false;
    }
    return true;
}

static bool ReplayJournal(sqlite3 *database, CcSim *sim,
                          uint64_t generation, uint64_t cursor,
                          uint64_t *replayed_through,
                          char *error, size_t error_capacity)
{
    if (!ValidateJournalCheckpoint(database, sim, generation, cursor,
                                   error, error_capacity)) return false;
    sqlite3_stmt *statement = NULL;
    const char *sql =
        "SELECT ordinal,record_version,operation_kind,command_kind,target_id,"
        "good,amount,dungeon_state,step_count,sim_schema_version,"
        "generator_version,pre_state_hash,post_state_hash "
        "FROM action_journal WHERE generation=? AND ordinal>? "
        "ORDER BY ordinal ASC;";
    if (!Prepare(database, sql, &statement, error, error_capacity)) return false;
    (void)sqlite3_bind_int64(statement, 1, (sqlite3_int64)generation);
    (void)sqlite3_bind_int64(statement, 2, (sqlite3_int64)cursor);
    uint64_t expected_ordinal = cursor + 1U;
    uint32_t expected_schema_version = sim->schema_version;
    uint32_t expected_generator_version = sim->generator_version;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        uint64_t ordinal = (uint64_t)sqlite3_column_int64(statement, 0);
        int32_t version = sqlite3_column_int(statement, 1);
        CcJournalOperationKind operation =
            (CcJournalOperationKind)sqlite3_column_int(statement, 2);
        CcCommand command = {
            .kind = (CcCommandKind)sqlite3_column_int(statement, 3),
            .target_id = (CcId)sqlite3_column_int64(statement, 4),
            .good = (CcGood)sqlite3_column_int(statement, 5),
            .amount = sqlite3_column_int(statement, 6),
            .dungeon_state =
                (CcDungeonState)sqlite3_column_int(statement, 7)
        };
        int32_t step_count = sqlite3_column_int(statement, 8);
        uint32_t schema_version =
            (uint32_t)sqlite3_column_int(statement, 9);
        uint32_t generator_version =
            (uint32_t)sqlite3_column_int(statement, 10);
        uint64_t pre_hash = 0U;
        uint64_t post_hash = 0U;
        bool hashes_valid =
            ParseStoredHash(sqlite3_column_text(statement, 11), &pre_hash) &&
            ParseStoredHash(sqlite3_column_text(statement, 12), &post_hash);
        if (ordinal != expected_ordinal ||
            version != CC_JOURNAL_RECORD_VERSION ||
            schema_version != expected_schema_version ||
            generator_version != expected_generator_version ||
            !hashes_valid || CcSimHash(sim) != pre_hash) {
            SetError(error, error_capacity,
                     "Action journal continuity check failed.");
            sqlite3_finalize(statement);
            return false;
        }
        char replay_error[192];
        bool applied = true;
        switch (operation) {
            case CC_JOURNAL_OPERATION_COMMAND:
                applied = CcSimApply(sim, &command, replay_error,
                                     sizeof(replay_error));
                break;
            case CC_JOURNAL_OPERATION_ADVANCE_DAYS:
                if (step_count <= 0 ||
                    step_count > CC_JOURNAL_MAX_DAY_ADVANCE ||
                    sim->current_day > CC_SIM_MAX_DAY - step_count) {
                    applied = false;
                }
                else CcSimAdvanceDays(sim, step_count);
                break;
            case CC_JOURNAL_OPERATION_ADVANCE_RUNTIME_TICKS:
                if (step_count <= 0 ||
                    step_count > CC_JOURNAL_MAX_RUNTIME_ADVANCE ||
                    sim->clock.tick > UINT64_MAX - (uint64_t)step_count) {
                    applied = false;
                }
                else CcSimAdvanceRuntimeTicks(sim, step_count);
                break;
            default:
                applied = false;
                break;
        }
        if (!applied || CcSimHash(sim) != post_hash) {
            SetError(error, error_capacity,
                     "Action journal replay diverged from its committed hash.");
            sqlite3_finalize(statement);
            return false;
        }
        expected_ordinal += 1U;
    }
    if (result != SQLITE_DONE) {
        SetSqlError(error, error_capacity, database,
                    "Could not replay action journal");
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);
    *replayed_through = expected_ordinal - 1U;
    return true;
}

static void TunePhysicalReserveTargets(CcSim *sim)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        switch (place->function) {
            case CC_SETTLEMENT_FARMING:
                place->reserve_target[CC_GOOD_IRON] = 2;
                place->reserve_target[CC_GOOD_TOOLS] = 6;
                break;
            case CC_SETTLEMENT_MARKET:
                place->reserve_target[CC_GOOD_IRON] = 16;
                place->reserve_target[CC_GOOD_TOOLS] = 12;
                break;
            case CC_SETTLEMENT_FORTRESS:
                place->reserve_target[CC_GOOD_IRON] = 8;
                place->reserve_target[CC_GOOD_TOOLS] = 8;
                break;
            case CC_SETTLEMENT_MINING:
                place->reserve_target[CC_GOOD_IRON] = 12;
                place->reserve_target[CC_GOOD_TOOLS] = 8;
                break;
            case CC_SETTLEMENT_CAPITAL:
                place->reserve_target[CC_GOOD_IRON] = 16;
                place->reserve_target[CC_GOOD_TOOLS] = 14;
                break;
            case CC_SETTLEMENT_DUNGEON_TOWN:
                place->reserve_target[CC_GOOD_IRON] = 10;
                place->reserve_target[CC_GOOD_TOOLS] = 6;
                break;
        }
    }
}

static bool HasQuestArchitecture(const CcSim *sim)
{
    if (sim->front_count > 0 || sim->quest_outcome_count > 0 ||
        sim->pending_echo_count > 0) return true;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->front_id != 0U ||
            situation->objective.progress.limit > 0) return true;
    }
    return false;
}

static void FinishLegacyRuntimeUpgrade(CcSim *sim)
{
    if (!HasQuestArchitecture(sim)) CcSimUpgradeQuestArchitecture(sim);
    sim->schema_version = CC_SIM_SCHEMA_VERSION;
    sim->generator_version = CC_GENERATOR_VERSION;
}

static bool UpgradeLegacyRuntime(CcSim *sim,
                                 char *error, size_t error_capacity)
{
    uint32_t legacy_version = sim->schema_version;
    if (legacy_version != 3U && legacy_version != 4U &&
        legacy_version != 5U && legacy_version != 6U &&
        legacy_version != 7U && legacy_version != 8U &&
        legacy_version != 9U && legacy_version != 10U &&
        legacy_version != 11U && legacy_version != 12U &&
        legacy_version != 13U && legacy_version != 14U &&
        legacy_version != 15U && legacy_version != 16U &&
        legacy_version != 17U && legacy_version != 18U &&
        legacy_version != 19U && legacy_version != 20U) return true;
    bool social_schema_19 = legacy_version == 19U &&
        sim->relationship_count > 0;
    bool quest_schema_19 = legacy_version == 19U &&
        HasQuestArchitecture(sim);
    if (social_schema_19) {
        int32_t old_first = (int32_t)CC_EVENT_GOBLIN_TUNNEL_TRAVERSED + 1;
        int32_t old_last = old_first + 3;
        for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
            int32_t kind = (int32_t)sim->events[i].kind;
            if (kind >= old_first && kind <= old_last) {
                sim->events[i].kind = (CcEventKind)(
                    (int32_t)CC_EVENT_RELATIONSHIP_HISTORY +
                    kind - old_first);
            }
        }
    }
    if (quest_schema_19) {
        int32_t old_first = (int32_t)CC_EVENT_GOBLIN_TUNNEL_TRAVERSED + 1;
        int32_t old_last = old_first + 3;
        for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
            int32_t kind = (int32_t)sim->events[i].kind;
            if (kind >= old_first && kind <= old_last) {
                sim->events[i].kind = (CcEventKind)(
                    (int32_t)CC_EVENT_FRONT_CREATED + kind - old_first);
            }
        }
    }
    CcSimInitializeUnderroad(sim);
    if (legacy_version == 20U) {
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 19U) {
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 18U) {
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 17U) {
        CcSimUpgradeMapCollection(sim);
        sim->journey.pace = CC_JOURNEY_PACE_STEADY;
        sim->journey.ambush_warned = false;
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    sim->goblins.cohesion = 60;
    sim->goblins.target_warned = false;
    sim->goblins.expeditions_intercepted = 0;
    sim->goblins.dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_NONE;
    sim->goblins.dragon_seed_days_remaining = 0;
    CcSimUpgradeMapCollection(sim);
    if (legacy_version == 16U) {
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 15U) {
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 14U) {
        CcSimInitializeHorseStableSystem(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 13U) {
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 3U) {
        sim->clock = (CcWorldClock){
            .game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND
        };
        sim->carriage = (CcCarriageState){
            .mode = CC_CARRIAGE_PARKED,
            .location_id = sim->player.location_id,
            .condition = 100
        };
        if (sim->journey.active) {
            const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
            if (route == NULL) {
                SetError(error, error_capacity,
                         "Legacy journey route is no longer valid.");
                return false;
            }
            int32_t fare = route->travel_days +
                (route->smuggler_route ? 3 : 0);
            fare += route->closed ? 4 : 0;
            fare += route->smuggler_route ? 2 :
                CcSimRouteCrossesWarBorder(sim, route->id) ? 4 : 0;
            if (sim->player.coins >= fare) sim->player.coins -= fare;
            int32_t total_subticks = route->travel_days * CC_WORLD_DAY_SUBTICKS;
            sim->journey.phase = CC_JOURNEY_PHASE_BLOCKED;
            sim->journey.departure_day = sim->current_day;
            sim->journey.total_subticks = total_subticks;
            sim->journey.encounter_subticks = 0;
            sim->journey.elapsed_subticks = 0;
            sim->journey.fare_reserved = fare;
            sim->journey.encounter_triggered = true;
            const CcEvent *recent = CcSimRecentEvent(sim, 0);
            sim->journey.parent_event_id = recent != NULL ? recent->id : 0U;
            sim->clock.game_minutes_per_second = 0;
            sim->carriage = (CcCarriageState){
                .mode = CC_CARRIAGE_STOPPED,
                .route_id = sim->journey.route_id,
                .origin_id = sim->journey.origin_id,
                .destination_id = sim->journey.destination_id,
                .condition = 100
            };
        }
    }

    if (legacy_version <= 4U) {
#define LEGACY_SERVICE(service) (UINT32_C(1) << (uint32_t)(service))
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *settlement = &sim->settlements[i];
        if (settlement->service_mask != 0U) continue;
        settlement->service_project = CC_SERVICE_NONE;
        settlement->service_project_days = 0;
        settlement->size = settlement->function == CC_SETTLEMENT_CAPITAL ?
            CC_SETTLEMENT_CAPITAL_SIZE :
            (settlement->function == CC_SETTLEMENT_MARKET ||
             settlement->function == CC_SETTLEMENT_FORTRESS ||
             settlement->function == CC_SETTLEMENT_MINING) ?
                CC_SETTLEMENT_TOWN : CC_SETTLEMENT_VILLAGE;
        settlement->service_mask = LEGACY_SERVICE(CC_SERVICE_INN);
        switch (settlement->function) {
            case CC_SETTLEMENT_FARMING:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_FARM) |
                    LEGACY_SERVICE(CC_SERVICE_GRANARY) |
                    LEGACY_SERVICE(CC_SERVICE_STABLE);
                break;
            case CC_SETTLEMENT_MINING:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_MINE) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_SHRINE);
                break;
            case CC_SETTLEMENT_MARKET:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_STABLE) |
                    LEGACY_SERVICE(CC_SERVICE_CARTOGRAPHER);
                break;
            case CC_SETTLEMENT_FORTRESS:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_BARRACKS) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_HEALER) |
                    LEGACY_SERVICE(CC_SERVICE_GRANARY);
                break;
            case CC_SETTLEMENT_CAPITAL:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_HEALER) |
                    LEGACY_SERVICE(CC_SERVICE_STABLE) |
                    LEGACY_SERVICE(CC_SERVICE_SHRINE) |
                    LEGACY_SERVICE(CC_SERVICE_BARRACKS) |
                    LEGACY_SERVICE(CC_SERVICE_CARTOGRAPHER) |
                    LEGACY_SERVICE(CC_SERVICE_GUILDHALL);
                break;
            case CC_SETTLEMENT_DUNGEON_TOWN:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_HEALER) |
                    LEGACY_SERVICE(CC_SERVICE_BLACK_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_DUNGEON_WARD);
                break;
        }
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        CcBanditGroup *bandits = &sim->bandits[i];
        if (bandits->service_mask == 0U) {
            bandits->camp_size = bandits->influence >= 60 ?
                CC_BANDIT_WAR_CAMP : bandits->influence >= 35 ?
                CC_BANDIT_CAMP : CC_BANDIT_HIDEOUT;
            bandits->service_mask = LEGACY_SERVICE(CC_SERVICE_BLACK_MARKET) |
                                    LEGACY_SERVICE(CC_SERVICE_STABLE);
        }
        bandits->raid_phase = CC_BANDIT_RAID_IDLE;
        bandits->raid_target_id = 0U;
        bandits->raid_good = CC_GOOD_FOOD;
        bandits->raid_quantity = 0;
        bandits->raid_days_remaining = 0;
    }
#undef LEGACY_SERVICE
    }
    if (legacy_version >= 11U) {
        CcSimInitializeDragonEcology(sim);
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 10U) {
        CcSimInitializeDragonEcology(sim);
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 9U) {
        TunePhysicalReserveTargets(sim);
        if (sim->iron_ledger_reserve == 0) {
            for (int32_t i = 0; i < sim->kingdom_count; ++i) {
                CcMoney deposit = sim->kingdoms[i].treasury < 160 ?
                                  sim->kingdoms[i].treasury : 160;
                sim->kingdoms[i].treasury -= deposit;
                sim->iron_ledger_reserve += deposit;
                sim->kingdoms[i].iron_ledger_debt = 0;
            }
        }
        CcSimInitializeDragonEcology(sim);
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    CcSimInitializeDragonCycle(sim);
    CcSimInitializeHoardRaiders(sim);
    if (legacy_version == 6U && sim->dragon.stolen_outstanding > 0 &&
        sim->dragon.theft_actor_id == 0U) {
        const CcEvent *theft = CcSimEvent(sim, sim->dragon.hoard_event_id);
        sim->dragon.theft_actor_id = theft != NULL ? theft->subject_id :
                                      sim->player.id;
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (legacy_version < 8U) {
            place->market_coins = 60 + place->population / 20 +
                                  place->prosperity * 2;
            place->war_chest = 0;
        }
        place->price[CC_GOOD_WEAPONS] = 24;
        place->price[CC_GOOD_GOLD] = 40;
        place->price[CC_GOOD_GEMS] = 70;
        place->reserve_target[CC_GOOD_WEAPONS] =
            place->function == CC_SETTLEMENT_FORTRESS ? 14 : 4;
        place->reserve_target[CC_GOOD_GOLD] = 1;
        place->reserve_target[CC_GOOD_GEMS] = 1;
        bool needs_field = place->function == CC_SETTLEMENT_FARMING ||
                           place->function == CC_SETTLEMENT_MINING ||
                           place->function == CC_SETTLEMENT_FORTRESS ||
                           place->function == CC_SETTLEMENT_CAPITAL;
        if (needs_field &&
            !CcSettlementHasService(place, CC_SERVICE_FARM) &&
            CcSettlementServiceCount(place) <
                CcSettlementServiceCapacity(place->size)) {
            place->service_mask |=
                UINT32_C(1) << (uint32_t)CC_SERVICE_FARM;
        }
        if (CcSettlementHasService(place, CC_SERVICE_FARM)) {
            place->field_yield = place->function == CC_SETTLEMENT_FARMING ?
                                 100 : place->function == CC_SETTLEMENT_CAPITAL ?
                                 90 : place->function == CC_SETTLEMENT_FORTRESS ?
                                 85 : 70;
            if (place->production[CC_GOOD_FOOD] == 0) {
                place->production[CC_GOOD_FOOD] = 16;
            }
        }
        if (CcSettlementHasService(place, CC_SERVICE_MINE)) {
            place->iron_deposit = 8000 + i * 800;
            place->gold_seam = true;
            place->gem_seam = place->function == CC_SETTLEMENT_MINING;
        }
        if (CcSettlementHasService(place, CC_SERVICE_SMITHY)) {
            place->production[CC_GOOD_WEAPONS] =
                place->function == CC_SETTLEMENT_FORTRESS ? 2 : 1;
        }
        if (CcSettlementHasService(place, CC_SERVICE_BARRACKS)) {
            place->stock[CC_GOOD_WEAPONS] = 6;
        }
        place->consumption[CC_GOOD_IRON] = 0;
        place->consumption[CC_GOOD_TOOLS] = 0;
    }
    sim->goblins.lair_settlement_id = sim->settlements[
        sim->settlement_count > 3 ? 3 : 0].id;
    sim->goblins.raid_motive = sim->goblins.tribute_phase ==
        CC_GOBLIN_TRIBUTE_IDLE ? CC_GOBLIN_RAID_NONE :
        CC_GOBLIN_RAID_DRAGON_TRIBUTE;
    sim->goblins.lair_stock[CC_GOOD_FOOD] = 12;
    sim->goblins.lair_stock[CC_GOOD_TOOLS] = 2;
    sim->goblins.lair_stock[CC_GOOD_WEAPONS] = 3;
    if (sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_RETURNING) {
        sim->goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
        sim->goblins.tribute_target_id = sim->dragon.lair_settlement_id;
    }
    if (sim->iron_ledger_reserve == 0) {
        for (int32_t i = 0; i < sim->kingdom_count; ++i) {
            CcMoney deposit = sim->kingdoms[i].treasury < 160 ?
                              sim->kingdoms[i].treasury : 160;
            sim->kingdoms[i].treasury -= deposit;
            sim->iron_ledger_reserve += deposit;
            sim->kingdoms[i].iron_ledger_debt = 0;
        }
    }
    TunePhysicalReserveTargets(sim);
    CcSimInitializeDragonEcology(sim);
    CcSimInitializeAnimalEconomy(sim);
    CcSimInitializeCharacters(sim);
    FinishLegacyRuntimeUpgrade(sim);
    return true;
}

static bool LoadDatabase(sqlite3 *database, CcSim *sim, bool *upgraded,
                         char *error, size_t error_capacity)
{
    *sim = (CcSim){0};
    if (upgraded != NULL) *upgraded = false;
    uint64_t expected_hash = 0U;
    uint64_t journal_generation = 0U;
    uint64_t journal_cursor = 0U;
    bool ok = CreateSchema(database, error, error_capacity) &&
              EnsureRealmColumns(database, error, error_capacity) &&
              EnsureAnimalColumns(database, error, error_capacity) &&
              EnsureHorseStableColumns(database, error, error_capacity) &&
              EnsureJourneyColumns(database, error, error_capacity) &&
              EnsureLegendColumns(database, error, error_capacity) &&
              EnsureSocialColumns(database, error, error_capacity) &&
              ReadMeta(database, sim, &expected_hash,
                       &journal_generation, &journal_cursor,
                       error, error_capacity) &&
              ReadKingdoms(database, sim, error, error_capacity) &&
              ReadDiplomacyAndCouriers(database, sim,
                                       error, error_capacity) &&
              ReadSettlements(database, sim, error, error_capacity) &&
              ReadHorseTeam(database, sim, error, error_capacity) &&
              ReadStableHorses(database, sim, error, error_capacity) &&
              ReadRoutes(database, sim, error, error_capacity) &&
              ReadMaps(database, sim, error, error_capacity) &&
              ReadFactions(database, sim, error, error_capacity) &&
              ReadShipments(database, sim, error, error_capacity) &&
              ReadThreats(database, sim, error, error_capacity) &&
              ReadDungeons(database, sim, error, error_capacity) &&
              ReadUnderroad(database, sim, error, error_capacity) &&
              ReadSituations(database, sim, error, error_capacity) &&
              ReadSituationCasts(database, sim, error, error_capacity) &&
              ReadCharacters(database, sim, error, error_capacity) &&
              ReadQuestArchitecture(database, sim, error, error_capacity) &&
              ReadEvents(database, sim, error, error_capacity) &&
              ReadLegends(database, sim, error, error_capacity) &&
              ReadPlayer(database, sim, error, error_capacity) &&
              ReadMapCollection(database, sim, error, error_capacity) &&
              ReadMaterialEconomy(database, sim, error, error_capacity) &&
              ReadPlayerCommitment(database, sim, error, error_capacity) &&
              ReadJourneyState(database, sim, error, error_capacity);
    if (!ok) {
        return false;
    }
    char validation[160];
    if (!CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, validation);
        return false;
    }
    if (CcSimHash(sim) != expected_hash) {
        SetError(error, error_capacity, "Campaign state hash does not match stored data.");
        return false;
    }
    uint64_t replayed_through = journal_cursor;
    if (journal_generation > 0U &&
        !ReplayJournal(database, sim, journal_generation, journal_cursor,
                       &replayed_through, error, error_capacity)) {
        return false;
    }
    uint32_t stored_schema_version = sim->schema_version;
    uint32_t stored_generator_version = sim->generator_version;
    if (!UpgradeLegacyRuntime(sim, error, error_capacity)) return false;
    if (upgraded != NULL) {
        *upgraded = sim->schema_version != stored_schema_version ||
                    sim->generator_version != stored_generator_version;
    }
    if (!CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, validation);
        return false;
    }
    return true;
}

bool CcSaveRead(const char *path, CcSim *sim,
                char *error, size_t error_capacity)
{
    if (path == NULL || sim == NULL) {
        SetError(error, error_capacity, "Load path or simulation is missing.");
        return false;
    }
    sqlite3 *database = NULL;
    if (!OpenReadSnapshot(path, &database, error, error_capacity)) return false;
    CcSim recovered;
    bool ok = LoadDatabase(database, &recovered, NULL,
                           error, error_capacity);
    if (sqlite3_close(database) != SQLITE_OK) {
        SetError(error, error_capacity, "Could not close campaign database.");
        return false;
    }
    if (!ok) return false;
    *sim = recovered;
    SetError(error, error_capacity, "");
    return true;
}

static bool ReadSnapshotJournalCursor(sqlite3 *database,
                                      uint64_t *generation,
                                      uint64_t *cursor,
                                      char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT journal_generation,journal_cursor "
                 "FROM meta WHERE id=1;",
                 &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity,
                 "Campaign journal checkpoint is missing.");
        sqlite3_finalize(statement);
        return false;
    }
    *generation = (uint64_t)sqlite3_column_int64(statement, 0);
    *cursor = (uint64_t)sqlite3_column_int64(statement, 1);
    sqlite3_finalize(statement);
    return true;
}

static bool ReadJournalHead(sqlite3 *database, uint64_t generation,
                            uint64_t *head,
                            char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT COALESCE(MAX(ordinal),0) FROM action_journal "
                 "WHERE generation=?;",
                 &statement, error, error_capacity)) return false;
    (void)sqlite3_bind_int64(statement, 1, (sqlite3_int64)generation);
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetSqlError(error, error_capacity, database,
                    "Could not read action journal head");
        sqlite3_finalize(statement);
        return false;
    }
    *head = (uint64_t)sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    return true;
}

static CcJournal *AllocateJournal(const char *path, WritableOpenMode mode,
                                  bool *created,
                                  char *error, size_t error_capacity)
{
    if (created != NULL) *created = false;
    CcJournal *journal = calloc(1U, sizeof(*journal));
    if (journal == NULL) {
        SetError(error, error_capacity,
                 "Could not allocate action journal state.");
        return NULL;
    }
    bool database_created = false;
    if (!OpenWritableDatabase(path, mode, &journal->database,
                              &database_created,
                              error, error_capacity) ||
        !CreateSchema(journal->database, error, error_capacity) ||
        !MarkDatabaseCurrent(journal->database, error, error_capacity)) {
        if (journal->database != NULL) sqlite3_close(journal->database);
        if (database_created) RemoveDatabaseArtifacts(path);
        free(journal);
        return NULL;
    }
    if (created != NULL) *created = database_created;
    return journal;
}

static bool InsertJournalEpoch(CcJournal *journal, const CcSim *sim,
                               char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    const char *sql =
        "INSERT INTO journal_epoch "
        "(record_version,world_seed,initial_state_hash,created_tick) "
        "VALUES(?,?,?,?);";
    bool ok = Prepare(journal->database, sql, &statement,
                      error, error_capacity);
    if (ok) {
        char hash[24];
        (void)snprintf(hash, sizeof(hash), "%016" PRIx64, CcSimHash(sim));
        BindInt(statement, 1, CC_JOURNAL_RECORD_VERSION);
        BindInt(statement, 2, (int32_t)sim->world_seed);
        BindText(statement, 3, hash);
        BindId(statement, 4, sim->clock.tick);
        ok = StepDone(journal->database, statement,
                      error, error_capacity);
    }
    sqlite3_finalize(statement);
    if (ok) {
        sqlite3_int64 generation = sqlite3_last_insert_rowid(journal->database);
        if (generation <= 0) {
            SetError(error, error_capacity,
                     "Action journal epoch did not receive an identity.");
            ok = false;
        } else {
            journal->generation = (uint64_t)generation;
            journal->last_ordinal = 0U;
        }
    }
    return ok;
}

static bool CreateJournalEpoch(CcJournal *journal, const CcSim *sim,
                               char *error, size_t error_capacity)
{
    bool ok = Execute(journal->database, "BEGIN IMMEDIATE;",
                      error, error_capacity);
    if (ok) ok = InsertJournalEpoch(journal, sim, error, error_capacity);
    return FinishTransaction(journal->database, ok, error, error_capacity);
}

static bool PruneJournalHistory(sqlite3 *database, uint64_t keep_generation,
                                char *error, size_t error_capacity)
{
    char sql[1024];
    int written = snprintf(
        sql, sizeof(sql),
        "DROP TRIGGER IF EXISTS action_journal_no_delete;"
        "DROP TRIGGER IF EXISTS journal_epoch_no_delete;"
        "DELETE FROM action_journal WHERE generation!=%" PRIu64 ";"
        "DELETE FROM journal_epoch WHERE generation!=%" PRIu64 ";"
        "CREATE TRIGGER action_journal_no_delete "
        "BEFORE DELETE ON action_journal BEGIN "
        "SELECT RAISE(ABORT, 'action journal is append-only'); END;"
        "CREATE TRIGGER journal_epoch_no_delete "
        "BEFORE DELETE ON journal_epoch BEGIN "
        "SELECT RAISE(ABORT, 'journal epoch is append-only'); END;",
        keep_generation, keep_generation);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        SetError(error, error_capacity,
                 "Could not prepare action journal compaction.");
        return false;
    }
    return Execute(database, sql, error, error_capacity);
}

static bool CompactJournal(CcJournal *journal, const CcSim *sim,
                           bool verify_active,
                           char *error, size_t error_capacity)
{
    uint64_t previous_generation = journal->generation;
    uint64_t previous_ordinal = journal->last_ordinal;
    bool ok = Execute(journal->database, "BEGIN IMMEDIATE;",
                      error, error_capacity);
    if (ok && verify_active) {
        uint64_t active_generation = 0U;
        uint64_t checkpoint_cursor = 0U;
        uint64_t stored_head = 0U;
        ok = ReadSnapshotJournalCursor(
                 journal->database, &active_generation, &checkpoint_cursor,
                 error, error_capacity) &&
             active_generation == previous_generation &&
             ReadJournalHead(journal->database, previous_generation,
                             &stored_head, error, error_capacity) &&
             stored_head == previous_ordinal;
        if (!ok && active_generation != previous_generation) {
            SetError(error, error_capacity,
                     "Another writer started a new campaign epoch.");
        } else if (!ok && stored_head != previous_ordinal) {
            SetError(error, error_capacity,
                     "Action journal advanced from another writer.");
        }
    }
    if (ok) ok = InsertJournalEpoch(journal, sim, error, error_capacity);
    if (ok) {
        ok = SaveSnapshotContents(journal->database, sim,
                                  journal->generation, 0U,
                                  error, error_capacity) &&
             PruneJournalHistory(journal->database, journal->generation,
                                 error, error_capacity);
    }
    ok = FinishTransaction(journal->database, ok, error, error_capacity);
    if (!ok) {
        journal->generation = previous_generation;
        journal->last_ordinal = previous_ordinal;
        return false;
    }
    journal->last_ordinal = 0U;
    (void)Execute(journal->database,
                  "PRAGMA wal_checkpoint(TRUNCATE); VACUUM;", NULL, 0U);
    SetError(error, error_capacity, "");
    return true;
}

static bool AppendJournalOperation(CcJournal *journal,
                                   CcJournalOperationKind operation,
                                   const CcCommand *command,
                                   int32_t step_count,
                                   const CcSim *before,
                                   const CcSim *after,
                                   char *error, size_t error_capacity)
{
    if (journal == NULL || before == NULL || after == NULL) {
        SetError(error, error_capacity,
                 "Action journal mutation is missing state.");
        return false;
    }
    char validation[160];
    if (before->schema_version != CC_SIM_SCHEMA_VERSION ||
        after->schema_version != CC_SIM_SCHEMA_VERSION ||
        !CcSimValidate(after, validation, sizeof(validation))) {
        SetError(error, error_capacity,
                 before->schema_version != CC_SIM_SCHEMA_VERSION ||
                 after->schema_version != CC_SIM_SCHEMA_VERSION ?
                     "Action journal requires the current simulation schema." :
                     validation);
        return false;
    }
    if (journal->last_ordinal >= CC_JOURNAL_COMPACT_RECORDS &&
        !CompactJournal(journal, before, true,
                        error, error_capacity)) return false;
    if (!Execute(journal->database, "BEGIN IMMEDIATE;",
                 error, error_capacity)) return false;
    uint64_t active_generation = 0U;
    uint64_t checkpoint_cursor = 0U;
    uint64_t stored_head = 0U;
    bool ok = ReadSnapshotJournalCursor(
                  journal->database, &active_generation, &checkpoint_cursor,
                  error, error_capacity) &&
              active_generation == journal->generation &&
              ReadJournalHead(journal->database, journal->generation,
                              &stored_head, error, error_capacity) &&
              stored_head == journal->last_ordinal;
    if (!ok && active_generation != journal->generation) {
        SetError(error, error_capacity,
                 "Another writer started a new campaign epoch.");
    } else if (!ok && stored_head != journal->last_ordinal) {
        SetError(error, error_capacity,
                 "Action journal advanced from another writer.");
    }
    sqlite3_stmt *statement = NULL;
    if (ok) {
        const char *sql =
            "INSERT INTO action_journal "
            "(generation,ordinal,record_version,operation_kind,command_kind,"
            "target_id,good,amount,dungeon_state,step_count,"
            "sim_schema_version,generator_version,pre_state_hash,"
            "post_state_hash,committed_tick) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
        ok = Prepare(journal->database, sql, &statement,
                     error, error_capacity);
    }
    uint64_t next_ordinal = journal->last_ordinal + 1U;
    if (ok) {
        CcCommand empty = {0};
        const CcCommand *input = command != NULL ? command : &empty;
        char pre_hash[24];
        char post_hash[24];
        (void)snprintf(pre_hash, sizeof(pre_hash), "%016" PRIx64,
                       CcSimHash(before));
        (void)snprintf(post_hash, sizeof(post_hash), "%016" PRIx64,
                       CcSimHash(after));
        BindId(statement, 1, journal->generation);
        BindId(statement, 2, next_ordinal);
        BindInt(statement, 3, CC_JOURNAL_RECORD_VERSION);
        BindInt(statement, 4, (int32_t)operation);
        BindInt(statement, 5, (int32_t)input->kind);
        BindId(statement, 6, input->target_id);
        BindInt(statement, 7, (int32_t)input->good);
        BindInt(statement, 8, input->amount);
        BindInt(statement, 9, (int32_t)input->dungeon_state);
        BindInt(statement, 10, step_count);
        BindInt(statement, 11, (int32_t)after->schema_version);
        BindInt(statement, 12, (int32_t)after->generator_version);
        BindText(statement, 13, pre_hash);
        BindText(statement, 14, post_hash);
        BindId(statement, 15, after->clock.tick);
        ok = StepDone(journal->database, statement,
                      error, error_capacity);
    }
    sqlite3_finalize(statement);
    ok = FinishTransaction(journal->database, ok, error, error_capacity);
    if (ok) journal->last_ordinal = next_ordinal;
    return ok;
}

CcJournal *CcJournalStart(const char *path, const CcSim *sim,
                          char *error, size_t error_capacity)
{
    if (path == NULL || sim == NULL ||
        sim->schema_version != CC_SIM_SCHEMA_VERSION) {
        SetError(error, error_capacity,
                 "Journal path or current-schema simulation is missing.");
        return NULL;
    }
    char validation[160];
    if (!CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, validation);
        return NULL;
    }
    bool created = false;
    CcJournal *journal = AllocateJournal(path, WRITABLE_OPEN_NEW, &created,
                                         error, error_capacity);
    if (journal == NULL) return NULL;
    if (!CreateJournalEpoch(journal, sim, error, error_capacity) ||
        !SaveSnapshot(journal->database, sim, journal->generation, 0U,
                      error, error_capacity)) {
        sqlite3_close(journal->database);
        free(journal);
        if (created) RemoveDatabaseArtifacts(path);
        return NULL;
    }
    SetError(error, error_capacity, "");
    return journal;
}

CcJournal *CcJournalRestart(const char *path, const CcSim *sim,
                            char *error, size_t error_capacity)
{
    if (path == NULL || sim == NULL ||
        sim->schema_version != CC_SIM_SCHEMA_VERSION) {
        SetError(error, error_capacity,
                 "Journal path or current-schema simulation is missing.");
        return NULL;
    }
    char validation[160];
    if (!CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, validation);
        return NULL;
    }
    CcJournal *journal = AllocateJournal(path, WRITABLE_OPEN_EXISTING, NULL,
                                         error, error_capacity);
    if (journal == NULL) return NULL;
    if (!CompactJournal(journal, sim, false, error, error_capacity)) {
        sqlite3_close(journal->database);
        free(journal);
        return NULL;
    }
    SetError(error, error_capacity, "");
    return journal;
}

CcJournal *CcJournalResume(const char *path, CcSim *sim,
                           char *error, size_t error_capacity)
{
    if (path == NULL || sim == NULL) {
        SetError(error, error_capacity,
                 "Journal path or simulation is missing.");
        return NULL;
    }
    CcSim preflight;
    if (!CcSaveRead(path, &preflight, error, error_capacity)) return NULL;
    CcJournal *journal = AllocateJournal(path, WRITABLE_OPEN_EXISTING, NULL,
                                         error, error_capacity);
    if (journal == NULL) return NULL;
    bool ok = Execute(journal->database, "BEGIN IMMEDIATE;",
                      error, error_capacity);
    CcSim recovered;
    bool upgraded = false;
    if (ok) {
        ok = LoadDatabase(journal->database, &recovered, &upgraded,
                          error, error_capacity);
    }
    uint64_t checkpoint_cursor = 0U;
    bool compacted = false;
    if (ok) {
        ok = ReadSnapshotJournalCursor(journal->database,
                                       &journal->generation,
                                       &checkpoint_cursor,
                                       error, error_capacity);
    }
    if (ok && (upgraded || journal->generation == 0U)) {
        ok = InsertJournalEpoch(journal, &recovered,
                                error, error_capacity) &&
             SaveSnapshotContents(journal->database, &recovered,
                                  journal->generation, 0U,
                                  error, error_capacity) &&
             PruneJournalHistory(journal->database, journal->generation,
                                 error, error_capacity);
        compacted = ok;
    } else if (ok) {
        ok = ReadJournalHead(journal->database, journal->generation,
                             &journal->last_ordinal,
                             error, error_capacity);
        if (ok && journal->last_ordinal < checkpoint_cursor) {
            SetError(error, error_capacity,
                     "Action journal is behind its snapshot checkpoint.");
            ok = false;
        }
    }
    ok = FinishTransaction(journal->database, ok,
                           error, error_capacity);
    if (!ok) {
        sqlite3_close(journal->database);
        free(journal);
        return NULL;
    }
    if (compacted) {
        (void)Execute(journal->database,
                      "PRAGMA wal_checkpoint(TRUNCATE); VACUUM;", NULL, 0U);
    } else if (journal->last_ordinal >= CC_JOURNAL_COMPACT_RECORDS &&
               !CompactJournal(journal, &recovered, true,
                               error, error_capacity)) {
        sqlite3_close(journal->database);
        free(journal);
        return NULL;
    }
    *sim = recovered;
    SetError(error, error_capacity, "");
    return journal;
}

bool CcJournalFlush(CcJournal *journal, CcSim *sim,
                    char *error, size_t error_capacity)
{
    if (journal == NULL || sim == NULL) {
        SetError(error, error_capacity,
                 "Action journal or simulation is missing.");
        return false;
    }
    if (journal->pending_runtime_ticks <= 0) {
        SetError(error, error_capacity, "");
        return true;
    }
    CcSim durable_base = journal->pending_runtime_base;
    int32_t ticks = journal->pending_runtime_ticks;
    journal->pending_runtime_ticks = 0;
    if (!AppendJournalOperation(journal,
                                CC_JOURNAL_OPERATION_ADVANCE_RUNTIME_TICKS,
                                NULL, ticks, &durable_base, sim,
                                error, error_capacity)) {
        *sim = durable_base;
        return false;
    }
    SetError(error, error_capacity, "");
    return true;
}

bool CcJournalCheckpoint(CcJournal *journal, CcSim *sim,
                         char *error, size_t error_capacity)
{
    if (!CcJournalFlush(journal, sim, error, error_capacity)) return false;
    return CompactJournal(journal, sim, true, error, error_capacity);
}

bool CcJournalApply(CcJournal *journal, CcSim *sim,
                    const CcCommand *command,
                    char *error, size_t error_capacity)
{
    if (journal == NULL || sim == NULL || command == NULL) {
        SetError(error, error_capacity,
                 "Journaled command is missing input state.");
        return false;
    }
    if (!CcJournalFlush(journal, sim, error, error_capacity)) return false;
    CcSim candidate = *sim;
    if (!CcSimApply(&candidate, command, error, error_capacity)) return false;
    if (!AppendJournalOperation(journal, CC_JOURNAL_OPERATION_COMMAND,
                                command, 0, sim, &candidate,
                                error, error_capacity)) return false;
    *sim = candidate;
    SetError(error, error_capacity, "");
    return true;
}

bool CcJournalAdvanceDays(CcJournal *journal, CcSim *sim, int32_t days,
                          char *error, size_t error_capacity)
{
    if (journal == NULL || sim == NULL || days <= 0 ||
        days > CC_JOURNAL_MAX_DAY_ADVANCE ||
        sim->current_day < 1 ||
        sim->current_day > CC_SIM_MAX_DAY - days) {
        SetError(error, error_capacity,
                 "Journaled day advance requires a positive duration.");
        return false;
    }
    if (!CcJournalFlush(journal, sim, error, error_capacity)) return false;
    CcSim candidate = *sim;
    CcSimAdvanceDays(&candidate, days);
    if (!AppendJournalOperation(journal, CC_JOURNAL_OPERATION_ADVANCE_DAYS,
                                NULL, days, sim, &candidate,
                                error, error_capacity)) return false;
    *sim = candidate;
    SetError(error, error_capacity, "");
    return true;
}

bool CcJournalAdvanceRuntimeTicks(CcJournal *journal, CcSim *sim,
                                  int32_t ticks,
                                  char *error, size_t error_capacity)
{
    if (journal == NULL || sim == NULL || ticks < 0 ||
        ticks > CC_JOURNAL_MAX_RUNTIME_ADVANCE ||
        (sim != NULL &&
         sim->clock.tick > UINT64_MAX - (uint64_t)ticks)) {
        SetError(error, error_capacity,
                 "Journaled runtime advance has invalid input.");
        return false;
    }
    if (ticks == 0) {
        SetError(error, error_capacity, "");
        return true;
    }
    if (journal->pending_runtime_ticks == 0) {
        journal->pending_runtime_base = *sim;
    }
    if (journal->pending_runtime_ticks > INT32_MAX - ticks) {
        if (!CcJournalFlush(journal, sim, error, error_capacity)) return false;
        journal->pending_runtime_base = *sim;
    }
    CcSimAdvanceRuntimeTicks(sim, ticks);
    journal->pending_runtime_ticks += ticks;
    bool transition_finished = !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING;
    if (journal->pending_runtime_ticks >= CC_JOURNAL_RUNTIME_FLUSH_TICKS ||
        transition_finished) {
        return CcJournalFlush(journal, sim, error, error_capacity);
    }
    SetError(error, error_capacity, "");
    return true;
}

bool CcJournalClose(CcJournal **journal, CcSim *sim,
                    char *error, size_t error_capacity)
{
    if (journal == NULL || *journal == NULL) {
        SetError(error, error_capacity, "");
        return true;
    }
    if (!CcJournalFlush(*journal, sim, error, error_capacity)) return false;
    if (sqlite3_close((*journal)->database) != SQLITE_OK) {
        SetError(error, error_capacity,
                 "Could not close the action journal.");
        return false;
    }
    free(*journal);
    *journal = NULL;
    SetError(error, error_capacity, "");
    return true;
}

void CcJournalAbandon(CcJournal **journal)
{
    if (journal == NULL || *journal == NULL) return;
    (void)sqlite3_close((*journal)->database);
    free(*journal);
    *journal = NULL;
}
