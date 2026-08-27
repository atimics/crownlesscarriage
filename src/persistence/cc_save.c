#include "persistence/cc_save.h"

#include <sqlite3.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CC_SQLITE_APPLICATION_ID 1128481362
#define CC_JOURNAL_RECORD_VERSION 1
#define CC_JOURNAL_RUNTIME_FLUSH_TICKS 6

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
    return EnsureColumn(database, "settlement", "size",
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

static bool EnsureLegendColumns(sqlite3 *database,
                                char *error, size_t error_capacity)
{
    return EnsureColumn(database, "dragon_state", "theft_actor_id",
            "ALTER TABLE dragon_state ADD COLUMN theft_actor_id INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "hoard_raiders", "motive",
            "ALTER TABLE hoard_raiders ADD COLUMN motive INTEGER NOT NULL DEFAULT 0;",
            error, error_capacity) &&
        EnsureColumn(database, "hoard_raiders", "war_raids_completed",
            "ALTER TABLE hoard_raiders ADD COLUMN war_raids_completed INTEGER NOT NULL DEFAULT 0;",
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

static bool OpenDatabase(const char *path, sqlite3 **database,
                         char *error, size_t error_capacity)
{
    if (sqlite3_open_v2(path, database,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        SetSqlError(error, error_capacity, *database, "Could not open campaign database");
        if (*database != NULL) sqlite3_close(*database);
        *database = NULL;
        return false;
    }
    if (!Execute(*database,
        "PRAGMA foreign_keys=ON;"
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=FULL;"
        "PRAGMA wal_autocheckpoint=1000;"
        "PRAGMA application_id=1128481362;"
        "PRAGMA user_version=9;",
        error, error_capacity)) {
        sqlite3_close(*database);
        *database = NULL;
        return false;
    }
    return true;
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
    return true;
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
        " journal_generation INTEGER NOT NULL DEFAULT 0,"
        " journal_cursor INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS kingdom ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, name TEXT NOT NULL,"
        " color_r INTEGER NOT NULL, color_g INTEGER NOT NULL, color_b INTEGER NOT NULL,"
        " treasury INTEGER NOT NULL, legitimacy INTEGER NOT NULL);"
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
        " parent_id INTEGER NOT NULL, magnitude INTEGER NOT NULL, text TEXT NOT NULL);"
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
        " market_coins INTEGER NOT NULL, war_chest INTEGER NOT NULL);"
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
        " deadline_day INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS shipment_intent ("
        " slot INTEGER PRIMARY KEY, final_destination_id INTEGER NOT NULL);";
    const char *map_schema =
        "CREATE TABLE IF NOT EXISTS map_object ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, route_id INTEGER NOT NULL,"
        " maker_settlement_id INTEGER NOT NULL, owner_id INTEGER NOT NULL, name TEXT NOT NULL,"
        " surveyed_day INTEGER NOT NULL, accuracy INTEGER NOT NULL,"
        " recorded_condition INTEGER NOT NULL, recorded_danger INTEGER NOT NULL,"
        " ask_price INTEGER NOT NULL, contraband INTEGER NOT NULL);";
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
        " carriage_speed_milli_per_second INTEGER NOT NULL, carriage_condition INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS delayed_echo ("
        " id INTEGER PRIMARY KEY CHECK(id=1), active INTEGER NOT NULL,"
        " situation_id INTEGER NOT NULL, settlement_id INTEGER NOT NULL,"
        " parent_event_id INTEGER NOT NULL, outcome INTEGER NOT NULL,"
        " due_day INTEGER NOT NULL, character_name TEXT NOT NULL);";
    const char *legend_schema =
        "CREATE TABLE IF NOT EXISTS goblin_cult ("
        " slot INTEGER PRIMARY KEY CHECK(slot=1), id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, members INTEGER NOT NULL, devotion INTEGER NOT NULL,"
        " tribute_phase INTEGER NOT NULL, tribute_target_id INTEGER NOT NULL,"
        " last_tribute_origin_id INTEGER NOT NULL, tribute_event_id INTEGER NOT NULL,"
        " carried_tribute INTEGER NOT NULL, tribute_days_remaining INTEGER NOT NULL,"
        " tribute_cooldown_days INTEGER NOT NULL, tributes_delivered INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS dragon_state ("
        " slot INTEGER PRIMARY KEY CHECK(slot=1), id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, lair_settlement_id INTEGER NOT NULL, hoard INTEGER NOT NULL,"
        " stolen_outstanding INTEGER NOT NULL, theft_actor_id INTEGER NOT NULL,"
        " retaliation_target_id INTEGER NOT NULL,"
        " hoard_event_id INTEGER NOT NULL, omen_event_id INTEGER NOT NULL,"
        " omen_days_remaining INTEGER NOT NULL, retaliations INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS hoard_raiders ("
        " slot INTEGER PRIMARY KEY CHECK(slot=1), id INTEGER NOT NULL UNIQUE,"
        " name TEXT NOT NULL, phase INTEGER NOT NULL, motive INTEGER NOT NULL,"
        " origin_settlement_id INTEGER NOT NULL,"
        " cause_event_id INTEGER NOT NULL, carried_treasure INTEGER NOT NULL,"
        " days_remaining INTEGER NOT NULL, cooldown_days INTEGER NOT NULL,"
        " raids_completed INTEGER NOT NULL, war_raids_completed INTEGER NOT NULL);";
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
    return Execute(database, schema, error, error_capacity) &&
           Execute(database, realm_schema, error, error_capacity) &&
           Execute(database, situation_schema, error, error_capacity) &&
           Execute(database, map_schema, error, error_capacity) &&
           Execute(database, commitment_schema, error, error_capacity) &&
           Execute(database, legend_schema, error, error_capacity) &&
           Execute(database, material_schema, error, error_capacity) &&
           Execute(database, journal_schema, error, error_capacity) &&
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
        "event_write_index,state_hash,journal_generation,journal_cursor) "
        "VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
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
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveKingdoms(sqlite3 *database, const CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO kingdom VALUES(?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *item = &sim->kingdoms[i];
        BindInt(statement, 1, i); BindId(statement, 2, item->id);
        BindText(statement, 3, item->name); BindInt(statement, 4, item->color_r);
        BindInt(statement, 5, item->color_g); BindInt(statement, 6, item->color_b);
        BindMoney(statement, 7, item->treasury); BindInt(statement, 8, item->legitimacy);
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
    const char *sql = "INSERT INTO settlement VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
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
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
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
    return true;
}

static bool SaveLegends(sqlite3 *database, const CcSim *sim,
                        char *error, size_t error_capacity)
{
    if (sim->schema_version < 6U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO goblin_cult VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?);",
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
    bool result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result) return false;

    if (!Prepare(database,
                 "INSERT INTO dragon_state (slot,id,name,lair_settlement_id,hoard,"
                 "stolen_outstanding,theft_actor_id,retaliation_target_id,"
                 "hoard_event_id,omen_event_id,omen_days_remaining,retaliations) "
                 "VALUES(1,?,?,?,?,?,?,?,?,?,?,?);",
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
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    if (!result || sim->schema_version < 7U) return result;

    if (!Prepare(database,
                 "INSERT INTO hoard_raiders (slot,id,name,phase,motive,"
                 "origin_settlement_id,cause_event_id,carried_treasure,"
                 "days_remaining,cooldown_days,raids_completed,war_raids_completed) "
                 "VALUES(1,?,?,?,?,?,?,?,?,?,?,?);",
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
    result = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return result;
}

static bool SaveEvents(sqlite3 *database, const CcSim *sim,
                       char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO causal_event VALUES(?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        const CcEvent *e = &sim->events[i];
        if (e->id == 0U) continue;
        BindInt(statement, 1, i); BindId(statement, 2, e->id); BindInt(statement, 3, e->day);
        BindInt(statement, 4, (int32_t)e->kind); BindId(statement, 5, e->subject_id);
        BindId(statement, 6, e->location_id); BindId(statement, 7, e->parent_id);
        BindInt(statement, 8, e->magnitude); BindText(statement, 9, e->text);
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
    if (!Prepare(database, "INSERT INTO situation VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);",
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
                 "INSERT INTO runtime_state VALUES(1,?,?,?,?,?,?,?,?,?,?,"
                 "?,?,?,?,?,?,?,?,?,?,?);",
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

static bool SaveSnapshot(sqlite3 *database, const CcSim *sim,
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
    bool ok = EnsureRealmColumns(database, error, error_capacity) &&
        EnsureLegendColumns(database, error, error_capacity) &&
        Execute(database, "BEGIN IMMEDIATE;", error, error_capacity) &&
        Execute(database,
            "DELETE FROM meta; DELETE FROM kingdom; DELETE FROM settlement;"
            "DELETE FROM route; DELETE FROM map_object; DELETE FROM faction; DELETE FROM shipment;"
            "DELETE FROM shipment_intent;"
            "DELETE FROM bandit_group; DELETE FROM monster_population;"
            "DELETE FROM goblin_cult; DELETE FROM dragon_state; DELETE FROM hoard_raiders;"
            "DELETE FROM dungeon; DELETE FROM situation; DELETE FROM situation_cast;"
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
        SaveMaterialEconomy(database, sim, error, error_capacity) &&
        SaveRoutes(database, sim, error, error_capacity) &&
        SaveMaps(database, sim, error, error_capacity) &&
        SaveFactions(database, sim, error, error_capacity) &&
        SaveShipments(database, sim, error, error_capacity) &&
        SaveThreats(database, sim, error, error_capacity) &&
        SaveDungeons(database, sim, error, error_capacity) &&
        SaveLegends(database, sim, error, error_capacity) &&
        SaveSituations(database, sim, error, error_capacity) &&
        SaveSituationCasts(database, sim, error, error_capacity) &&
        SaveEvents(database, sim, error, error_capacity) &&
        SavePlayer(database, sim, error, error_capacity) &&
        SavePlayerCommitment(database, sim, error, error_capacity) &&
        SaveJourneyState(database, sim, error, error_capacity);
    if (ok) ok = Execute(database, "COMMIT;", error, error_capacity);
    else (void)Execute(database, "ROLLBACK;", NULL, 0U);
    return ok;
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
    if (!OpenDatabase(path, &database, error, error_capacity)) return false;
    bool ok = CreateSchema(database, error, error_capacity) &&
              SaveSnapshot(database, sim, 0U, 0U,
                           error, error_capacity);
    if (sqlite3_close(database) != SQLITE_OK && ok) {
        SetError(error, error_capacity, "Could not close campaign database.");
        return false;
    }
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
        "event_write_index,state_hash,journal_generation,journal_cursor "
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
    sqlite3_finalize(statement);
    return true;
}

static bool ReadKingdoms(sqlite3 *database, CcSim *sim,
                         char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT slot,id,name,color_r,color_g,color_b,treasury,legitimacy FROM kingdom ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int32_t slot = sqlite3_column_int(statement, 0);
        if (slot < 0 || slot >= CC_MAX_KINGDOMS) { sqlite3_finalize(statement); return false; }
        CcKingdom *k = &sim->kingdoms[slot];
        k->id = (CcId)sqlite3_column_int64(statement, 1);
        (void)snprintf(k->name, sizeof(k->name), "%s", sqlite3_column_text(statement, 2));
        k->color_r = (uint8_t)sqlite3_column_int(statement, 3);
        k->color_g = (uint8_t)sqlite3_column_int(statement, 4);
        k->color_b = (uint8_t)sqlite3_column_int(statement, 5);
        k->treasury = (CcMoney)sqlite3_column_int64(statement, 6);
        k->legitimacy = sqlite3_column_int(statement, 7);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->kingdom_count) { SetError(error, error_capacity, "Kingdom rows are incomplete."); return false; }
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
        (void)snprintf(s->name, sizeof(s->name), "%s", sqlite3_column_text(statement, column++));
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
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->settlement_count) { SetError(error, error_capacity, "Settlement rows are incomplete."); return false; }
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
        (void)snprintf(treasure->name, sizeof(treasure->name), "%s",
                       sqlite3_column_text(statement, 2));
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
        (void)snprintf(map->name, sizeof(map->name), "%s",
                       sqlite3_column_text(statement, 5));
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
        (void)snprintf(f->name, sizeof(f->name), "%s", sqlite3_column_text(statement, 3));
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
        (void)snprintf(b->name, sizeof(b->name), "%s", sqlite3_column_text(statement, 3));
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
        (void)snprintf(m->name, sizeof(m->name), "%s", sqlite3_column_text(statement, 3));
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
        (void)snprintf(d->name, sizeof(d->name), "%s", sqlite3_column_text(statement, 3));
        d->state = (CcDungeonState)sqlite3_column_int(statement, 4);
        d->depth = sqlite3_column_int(statement, 5); d->regional_pressure = sqlite3_column_int(statement, 6);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->dungeon_count) { SetError(error, error_capacity, "Dungeon rows are incomplete."); return false; }
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
                 "tribute_days_remaining,tribute_cooldown_days,tributes_delivered "
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
    (void)snprintf(goblins->name, sizeof(goblins->name), "%s",
                   sqlite3_column_text(statement, column++));
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
    sqlite3_finalize(statement);

    if (!Prepare(database,
                 "SELECT id,name,lair_settlement_id,hoard,stolen_outstanding,"
                 "theft_actor_id,retaliation_target_id,hoard_event_id,omen_event_id,"
                 "omen_days_remaining,retaliations FROM dragon_state WHERE slot=1;",
                 &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        SetError(error, error_capacity, "Dragon state is missing.");
        sqlite3_finalize(statement);
        return false;
    }
    CcDragon *dragon = &sim->dragon;
    column = 0;
    dragon->id = (CcId)sqlite3_column_int64(statement, column++);
    (void)snprintf(dragon->name, sizeof(dragon->name), "%s",
                   sqlite3_column_text(statement, column++));
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
    sqlite3_finalize(statement);
    if (sim->schema_version < 7U) return true;

    if (!Prepare(database,
                 "SELECT id,name,phase,motive,origin_settlement_id,cause_event_id,"
                 "carried_treasure,days_remaining,cooldown_days,raids_completed,"
                 "war_raids_completed "
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
    (void)snprintf(raiders->name, sizeof(raiders->name), "%s",
                   sqlite3_column_text(statement, column++));
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
        (void)snprintf(e->text, sizeof(e->text), "%s", sqlite3_column_text(statement, 8));
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
        (void)snprintf(situation->sponsor_name,
                       sizeof(situation->sponsor_name), "%s",
                       sqlite3_column_text(statement, 2));
        (void)snprintf(situation->affected_name,
                       sizeof(situation->affected_name), "%s",
                       sqlite3_column_text(statement, 3));
    }
    sqlite3_finalize(statement);
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
        /* Saves written before this optional schema-v3 extension had no
           commitment row and therefore load with no accepted charter. */
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
                 "carriage_speed_milli_per_second,carriage_condition "
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
        const unsigned char *name = sqlite3_column_text(statement, 6);
        (void)snprintf(sim->delayed_echo.character_name,
                       sizeof(sim->delayed_echo.character_name), "%s",
                       name != NULL ? (const char *)name : "");
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
            schema_version != CC_SIM_SCHEMA_VERSION ||
            generator_version != CC_GENERATOR_VERSION ||
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
                if (step_count <= 0) applied = false;
                else CcSimAdvanceDays(sim, step_count);
                break;
            case CC_JOURNAL_OPERATION_ADVANCE_RUNTIME_TICKS:
                if (step_count <= 0) applied = false;
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

static bool UpgradeLegacyRuntime(CcSim *sim,
                                 char *error, size_t error_capacity)
{
    uint32_t legacy_version = sim->schema_version;
    if (legacy_version != 3U && legacy_version != 4U &&
        legacy_version != 5U && legacy_version != 6U &&
        legacy_version != 7U && legacy_version != 8U) return true;
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
    sim->schema_version = CC_SIM_SCHEMA_VERSION;
    sim->generator_version = CC_GENERATOR_VERSION;
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
    if (!OpenDatabase(path, &database, error, error_capacity)) return false;
    *sim = (CcSim){0};
    uint64_t expected_hash = 0U;
    uint64_t journal_generation = 0U;
    uint64_t journal_cursor = 0U;
    bool ok = CreateSchema(database, error, error_capacity) &&
              EnsureRealmColumns(database, error, error_capacity) &&
              EnsureLegendColumns(database, error, error_capacity) &&
              ReadMeta(database, sim, &expected_hash,
                       &journal_generation, &journal_cursor,
                       error, error_capacity) &&
              ReadKingdoms(database, sim, error, error_capacity) &&
              ReadSettlements(database, sim, error, error_capacity) &&
              ReadRoutes(database, sim, error, error_capacity) &&
              ReadMaps(database, sim, error, error_capacity) &&
              ReadFactions(database, sim, error, error_capacity) &&
              ReadShipments(database, sim, error, error_capacity) &&
              ReadThreats(database, sim, error, error_capacity) &&
              ReadDungeons(database, sim, error, error_capacity) &&
              ReadSituations(database, sim, error, error_capacity) &&
              ReadSituationCasts(database, sim, error, error_capacity) &&
              ReadEvents(database, sim, error, error_capacity) &&
              ReadLegends(database, sim, error, error_capacity) &&
              ReadPlayer(database, sim, error, error_capacity) &&
              ReadMaterialEconomy(database, sim, error, error_capacity) &&
              ReadPlayerCommitment(database, sim, error, error_capacity) &&
              ReadJourneyState(database, sim, error, error_capacity);
    if (!ok) {
        sqlite3_close(database);
        return false;
    }
    char validation[160];
    if (!CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, validation);
        sqlite3_close(database);
        return false;
    }
    if (CcSimHash(sim) != expected_hash) {
        SetError(error, error_capacity, "Campaign state hash does not match stored data.");
        sqlite3_close(database);
        return false;
    }
    if (!UpgradeLegacyRuntime(sim, error, error_capacity)) {
        sqlite3_close(database);
        return false;
    }
    uint64_t replayed_through = journal_cursor;
    if (journal_generation > 0U &&
        !ReplayJournal(database, sim, journal_generation, journal_cursor,
                       &replayed_through, error, error_capacity)) {
        sqlite3_close(database);
        return false;
    }
    if (!CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, validation);
        sqlite3_close(database);
        return false;
    }
    if (sqlite3_close(database) != SQLITE_OK) {
        SetError(error, error_capacity, "Could not close campaign database.");
        return false;
    }
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

static CcJournal *AllocateJournal(const char *path,
                                  char *error, size_t error_capacity)
{
    CcJournal *journal = calloc(1U, sizeof(*journal));
    if (journal == NULL) {
        SetError(error, error_capacity,
                 "Could not allocate action journal state.");
        return NULL;
    }
    if (!OpenDatabase(path, &journal->database, error, error_capacity) ||
        !CreateSchema(journal->database, error, error_capacity)) {
        if (journal->database != NULL) sqlite3_close(journal->database);
        free(journal);
        return NULL;
    }
    return journal;
}

static bool CreateJournalEpoch(CcJournal *journal, const CcSim *sim,
                               char *error, size_t error_capacity)
{
    if (!Execute(journal->database, "BEGIN IMMEDIATE;",
                 error, error_capacity)) return false;
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
    if (ok) ok = Execute(journal->database, "COMMIT;",
                         error, error_capacity);
    else (void)Execute(journal->database, "ROLLBACK;", NULL, 0U);
    return ok;
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
    if (!Execute(journal->database, "BEGIN IMMEDIATE;",
                 error, error_capacity)) return false;
    uint64_t stored_head = 0U;
    bool ok = ReadJournalHead(journal->database, journal->generation,
                              &stored_head, error, error_capacity) &&
              stored_head == journal->last_ordinal;
    if (!ok && stored_head != journal->last_ordinal) {
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
    if (ok) ok = Execute(journal->database, "COMMIT;",
                         error, error_capacity);
    else (void)Execute(journal->database, "ROLLBACK;", NULL, 0U);
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
    CcJournal *journal = AllocateJournal(path, error, error_capacity);
    if (journal == NULL) return NULL;
    if (!CreateJournalEpoch(journal, sim, error, error_capacity) ||
        !SaveSnapshot(journal->database, sim, journal->generation, 0U,
                      error, error_capacity)) {
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
    CcSim recovered;
    if (!CcSaveRead(path, &recovered, error, error_capacity)) return NULL;
    CcJournal *journal = AllocateJournal(path, error, error_capacity);
    if (journal == NULL) return NULL;
    uint64_t checkpoint_cursor = 0U;
    if (!ReadSnapshotJournalCursor(journal->database,
                                   &journal->generation,
                                   &checkpoint_cursor,
                                   error, error_capacity)) {
        sqlite3_close(journal->database);
        free(journal);
        return NULL;
    }
    if (journal->generation == 0U) {
        sqlite3_close(journal->database);
        free(journal);
        journal = CcJournalStart(path, &recovered,
                                 error, error_capacity);
        if (journal == NULL) return NULL;
    } else if (!ReadJournalHead(journal->database, journal->generation,
                                &journal->last_ordinal,
                                error, error_capacity) ||
               journal->last_ordinal < checkpoint_cursor) {
        if (journal->last_ordinal < checkpoint_cursor) {
            SetError(error, error_capacity,
                     "Action journal is behind its snapshot checkpoint.");
        }
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
    bool ok = SaveSnapshot(journal->database, sim, journal->generation,
                           journal->last_ordinal, error, error_capacity);
    if (ok) SetError(error, error_capacity, "");
    return ok;
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
    if (journal == NULL || sim == NULL || days <= 0) {
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
    if (journal == NULL || sim == NULL || ticks < 0) {
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
