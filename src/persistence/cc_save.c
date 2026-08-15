#include "persistence/cc_save.h"

#include <sqlite3.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CC_SQLITE_APPLICATION_ID 1128481362

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
        "PRAGMA journal_mode=DELETE;"
        "PRAGMA synchronous=FULL;"
        "PRAGMA application_id=1128481362;"
        "PRAGMA user_version=3;",
        error, error_capacity)) {
        sqlite3_close(*database);
        *database = NULL;
        return false;
    }
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
        " event_write_index INTEGER NOT NULL, state_hash TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS kingdom ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, name TEXT NOT NULL,"
        " color_r INTEGER NOT NULL, color_g INTEGER NOT NULL, color_b INTEGER NOT NULL,"
        " treasury INTEGER NOT NULL, legitimacy INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS settlement ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, kingdom_id INTEGER NOT NULL,"
        " name TEXT NOT NULL, function INTEGER NOT NULL, map_x INTEGER NOT NULL,"
        " map_y INTEGER NOT NULL, population INTEGER NOT NULL, security INTEGER NOT NULL,"
        " prosperity INTEGER NOT NULL, hunger INTEGER NOT NULL, last_shortage INTEGER NOT NULL,"
        " food_stock INTEGER NOT NULL, material_stock INTEGER NOT NULL, tools_stock INTEGER NOT NULL,"
        " food_target INTEGER NOT NULL, material_target INTEGER NOT NULL, tools_target INTEGER NOT NULL,"
        " food_production INTEGER NOT NULL, material_production INTEGER NOT NULL, tools_production INTEGER NOT NULL,"
        " food_consumption INTEGER NOT NULL, material_consumption INTEGER NOT NULL, tools_consumption INTEGER NOT NULL,"
        " food_price INTEGER NOT NULL, material_price INTEGER NOT NULL, tools_price INTEGER NOT NULL);"
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
        "CREATE TABLE IF NOT EXISTS bandit_group ("
        " slot INTEGER PRIMARY KEY, id INTEGER NOT NULL UNIQUE, route_id INTEGER NOT NULL,"
        " name TEXT NOT NULL, members INTEGER NOT NULL, supplies INTEGER NOT NULL,"
        " influence INTEGER NOT NULL, last_level INTEGER NOT NULL);"
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
        "CREATE TABLE IF NOT EXISTS delayed_echo ("
        " id INTEGER PRIMARY KEY CHECK(id=1), active INTEGER NOT NULL,"
        " situation_id INTEGER NOT NULL, settlement_id INTEGER NOT NULL,"
        " parent_event_id INTEGER NOT NULL, outcome INTEGER NOT NULL,"
        " due_day INTEGER NOT NULL, character_name TEXT NOT NULL);";
    return Execute(database, schema, error, error_capacity) &&
           Execute(database, situation_schema, error, error_capacity) &&
           Execute(database, map_schema, error, error_capacity) &&
           Execute(database, commitment_schema, error, error_capacity);
}

static bool SaveMeta(sqlite3 *database, const CcSim *sim,
                     char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    const char *sql =
        "INSERT INTO meta VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
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
    const char *sql = "INSERT INTO settlement VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
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
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) BindInt(statement, column++, s->stock[good]);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) BindInt(statement, column++, s->reserve_target[good]);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) BindInt(statement, column++, s->production[good]);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) BindInt(statement, column++, s->consumption[good]);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) BindInt(statement, column++, s->price[good]);
        if (!StepDone(database, statement, error, error_capacity) ||
            !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
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
    if (!Prepare(database, "INSERT INTO bandit_group VALUES(?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcBanditGroup *b = &sim->bandits[i];
        BindInt(statement, 1, i); BindId(statement, 2, b->id); BindId(statement, 3, b->route_id);
        BindText(statement, 4, b->name); BindInt(statement, 5, b->members);
        BindInt(statement, 6, b->supplies); BindInt(statement, 7, b->influence);
        BindInt(statement, 8, sim->last_bandit_level[i]);
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

bool CcSaveWrite(const char *path, const CcSim *sim,
                 char *error, size_t error_capacity)
{
    char validation[160];
    if (path == NULL || sim == NULL ||
        !CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, sim == NULL || path == NULL ?
                 "Save path or simulation is missing." : validation);
        return false;
    }
    sqlite3 *database = NULL;
    if (!OpenDatabase(path, &database, error, error_capacity)) return false;
    bool ok = CreateSchema(database, error, error_capacity) &&
        Execute(database, "BEGIN IMMEDIATE;", error, error_capacity) &&
        Execute(database,
            "DELETE FROM meta; DELETE FROM kingdom; DELETE FROM settlement;"
            "DELETE FROM route; DELETE FROM map_object; DELETE FROM faction; DELETE FROM shipment;"
            "DELETE FROM shipment_intent;"
            "DELETE FROM bandit_group; DELETE FROM monster_population;"
            "DELETE FROM dungeon; DELETE FROM situation; DELETE FROM situation_cast;"
            "DELETE FROM causal_event;"
            "DELETE FROM player_company; DELETE FROM player_commitment;"
            "DELETE FROM player_journey; DELETE FROM delayed_echo;",
            error, error_capacity) &&
        SaveMeta(database, sim, error, error_capacity) &&
        SaveKingdoms(database, sim, error, error_capacity) &&
        SaveSettlements(database, sim, error, error_capacity) &&
        SaveRoutes(database, sim, error, error_capacity) &&
        SaveMaps(database, sim, error, error_capacity) &&
        SaveFactions(database, sim, error, error_capacity) &&
        SaveShipments(database, sim, error, error_capacity) &&
        SaveThreats(database, sim, error, error_capacity) &&
        SaveDungeons(database, sim, error, error_capacity) &&
        SaveSituations(database, sim, error, error_capacity) &&
        SaveSituationCasts(database, sim, error, error_capacity) &&
        SaveEvents(database, sim, error, error_capacity) &&
        SavePlayer(database, sim, error, error_capacity) &&
        SavePlayerCommitment(database, sim, error, error_capacity) &&
        SaveJourneyState(database, sim, error, error_capacity);
    if (ok) ok = Execute(database, "COMMIT;", error, error_capacity);
    else (void)Execute(database, "ROLLBACK;", NULL, 0U);
    if (sqlite3_close(database) != SQLITE_OK && ok) {
        SetError(error, error_capacity, "Could not close campaign database.");
        return false;
    }
    return ok;
}

static bool ReadMeta(sqlite3 *database, CcSim *sim, uint64_t *expected_hash,
                     char *error, size_t error_capacity)
{
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT schema_version,generator_version,world_seed,random_state,"
        "current_day,next_entity_serial,kingdom_count,settlement_count,route_count,faction_count,"
        "shipment_count,bandit_count,monster_count,dungeon_count,event_count,event_write_index,state_hash "
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
    if (hash_text == NULL || sscanf((const char *)hash_text, "%" SCNx64, expected_hash) != 1) {
        SetError(error, error_capacity, "Campaign hash is invalid.");
        sqlite3_finalize(statement); return false;
    }
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
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) s->stock[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) s->reserve_target[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) s->production[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) s->consumption[good] = sqlite3_column_int(statement, column++);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) s->price[good] = sqlite3_column_int(statement, column++);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != sim->settlement_count) { SetError(error, error_capacity, "Settlement rows are incomplete."); return false; }
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
    bool ok = CreateSchema(database, error, error_capacity) &&
              ReadMeta(database, sim, &expected_hash, error, error_capacity) &&
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
              ReadPlayer(database, sim, error, error_capacity) &&
              ReadPlayerCommitment(database, sim, error, error_capacity) &&
              ReadJourneyState(database, sim, error, error_capacity);
    sqlite3_close(database);
    if (!ok) return false;
    char validation[160];
    if (!CcSimValidate(sim, validation, sizeof(validation))) {
        SetError(error, error_capacity, validation);
        return false;
    }
    if (CcSimHash(sim) != expected_hash) {
        SetError(error, error_capacity, "Campaign state hash does not match stored data.");
        return false;
    }
    SetError(error, error_capacity, "");
    return true;
}
