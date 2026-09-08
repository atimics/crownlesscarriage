#include "persistence/cc_journal_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum {
    CC_SCHEMA12_COMMAND_STEAL_DRAGON_NAMED_TREASURE = 16,
    CC_SCHEMA12_COMMAND_RETURN_DRAGON_NAMED_TREASURE = 17,
};

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

static bool Prepare(sqlite3 *database, const char *sql, sqlite3_stmt **statement,
                    char *error, size_t error_capacity)
{
    if (sqlite3_prepare_v2(database, sql, -1, statement, NULL) == SQLITE_OK) return true;
    SetSqlError(error, error_capacity, database, "Could not prepare save query");
    return false;
}

bool CcSaveParseStoredHash(const unsigned char *text, uint64_t *hash)
{
    if (text == NULL || hash == NULL) return false;
    const char *value = (const char *)text;
    int consumed = 0;
    return strlen(value) == 16U &&
           sscanf(value, "%16" SCNx64 "%n", hash, &consumed) == 1 &&
           consumed == 16;
}

static CcCommandKind ReadCommandKind(uint32_t schema_version,
                                     int32_t stored_kind)
{
    if (schema_version == 12U) {
        if (stored_kind ==
            CC_SCHEMA12_COMMAND_STEAL_DRAGON_NAMED_TREASURE) {
            return CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE;
        }
        if (stored_kind ==
            CC_SCHEMA12_COMMAND_RETURN_DRAGON_NAMED_TREASURE) {
            return CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE;
        }
    }
    return (CcCommandKind)stored_kind;
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
    bool parsed = CcSaveParseStoredHash(sqlite3_column_text(statement, 2),
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
            !CcSaveParseStoredHash(sqlite3_column_text(statement, 0),
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

bool CcJournalReplay(sqlite3 *database, CcSim *sim,
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
        int32_t stored_command_kind = sqlite3_column_int(statement, 3);
        int32_t step_count = sqlite3_column_int(statement, 8);
        uint32_t schema_version =
            (uint32_t)sqlite3_column_int(statement, 9);
        uint32_t generator_version =
            (uint32_t)sqlite3_column_int(statement, 10);
        CcCommand command = {
            .kind = ReadCommandKind(schema_version, stored_command_kind),
            .target_id = (CcId)sqlite3_column_int64(statement, 4),
            .good = (CcGood)sqlite3_column_int(statement, 5),
            .amount = sqlite3_column_int(statement, 6),
            .dungeon_state =
                (CcDungeonState)sqlite3_column_int(statement, 7)
        };
        uint64_t pre_hash = 0U;
        uint64_t post_hash = 0U;
        bool hashes_valid =
            CcSaveParseStoredHash(sqlite3_column_text(statement, 11), &pre_hash) &&
            CcSaveParseStoredHash(sqlite3_column_text(statement, 12), &post_hash);
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

