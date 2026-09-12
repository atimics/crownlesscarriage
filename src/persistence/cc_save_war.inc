/* War parties and sealed dispatches (#646). Schema 91 only: older saves
   carry neither, and the migration clears both arrays. */
static bool SaveWarParties(sqlite3 *database, const CcSim *sim,
                           char *error, size_t error_capacity)
{
    if (sim->schema_version < 91U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO war_party VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    bool ok = true;
    for (int32_t i = 0; i < sim->war_party_count && ok; ++i) {
        const CcWarParty *party = &sim->war_parties[i];
        sqlite3_bind_int(statement, 1, i);
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)party->id);
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)party->kingdom_id);
        sqlite3_bind_int64(statement, 4,
                           (sqlite3_int64)party->commander_character_id);
        sqlite3_bind_int64(statement, 5,
                           (sqlite3_int64)party->home_settlement_id);
        sqlite3_bind_int64(statement, 6,
                           (sqlite3_int64)party->current_settlement_id);
        sqlite3_bind_int64(statement, 7,
                           (sqlite3_int64)party->travel_route_id);
        sqlite3_bind_int64(statement, 8,
                           (sqlite3_int64)party->travel_destination_id);
        sqlite3_bind_int64(statement, 9, party->travel_arrival_day);
        sqlite3_bind_int64(statement, 10, party->members);
        sqlite3_bind_int64(statement, 11, (sqlite3_int64)party->order);
        sqlite3_bind_int64(statement, 12, (sqlite3_int64)party->order_route_id);
        sqlite3_bind_int64(statement, 13, (sqlite3_int64)party->order_target_id);
        sqlite3_bind_int(statement, 14, party->permits_player ? 1 : 0);
        sqlite3_bind_int64(statement, 15, party->battles_fought);
        sqlite3_bind_int64(statement, 16, party->casualties);
        ok = StepDone(database, statement, error, error_capacity) &&
             ResetStatement(database, statement, error, error_capacity);
    }
    sqlite3_finalize(statement);
    return ok;
}

static bool ReadWarParties(sqlite3 *database, CcSim *sim,
                           char *error, size_t error_capacity)
{
    if (sim->schema_version < 91U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM war_party ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    bool ok = true;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        if (rows >= CC_MAX_WAR_PARTIES) {
            SetError(error, error_capacity,
                     "War party rows exceed the bound.");
            sqlite3_finalize(statement);
            return false;
        }
        CcWarParty *party = &sim->war_parties[rows];
        *party = (CcWarParty){0};
        party->id = (CcId)sqlite3_column_int64(statement, 1);
        party->kingdom_id = (CcId)sqlite3_column_int64(statement, 2);
        party->commander_character_id =
            (CcId)sqlite3_column_int64(statement, 3);
        party->home_settlement_id = (CcId)sqlite3_column_int64(statement, 4);
        party->current_settlement_id =
            (CcId)sqlite3_column_int64(statement, 5);
        party->travel_route_id = (CcId)sqlite3_column_int64(statement, 6);
        party->travel_destination_id =
            (CcId)sqlite3_column_int64(statement, 7);
        party->travel_arrival_day = (int32_t)sqlite3_column_int64(statement, 8);
        party->members = (int32_t)sqlite3_column_int64(statement, 9);
        party->order = (CcWarOrderKind)sqlite3_column_int64(statement, 10);
        party->order_route_id = (CcId)sqlite3_column_int64(statement, 11);
        party->order_target_id = (CcId)sqlite3_column_int64(statement, 12);
        party->permits_player = sqlite3_column_int64(statement, 13) != 0;
        party->battles_fought = (int32_t)sqlite3_column_int64(statement, 14);
        party->casualties = (int32_t)sqlite3_column_int64(statement, 15);
        if (party->members <= 0 ||
            party->order < CC_WAR_ORDER_HOLD ||
            party->order > CC_WAR_ORDER_CEASE) {
            SetError(error, error_capacity,
                     "War party row is outside its range.");
            sqlite3_finalize(statement);
            return false;
        }
        rows += 1;
    }
    if (rows < sim->war_party_count) {
        SetError(error, error_capacity,
                 "War party rows are missing.");
        ok = false;
    }
    sim->war_party_count = rows;
    sqlite3_finalize(statement);
    return ok;
}

static bool SaveDispatches(sqlite3 *database, const CcSim *sim,
                            char *error, size_t error_capacity)
{
    if (sim->schema_version < 91U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "INSERT INTO dispatch VALUES(?,?,?,?,?,?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    bool ok = true;
    for (int32_t i = 0; i < sim->dispatch_count && ok; ++i) {
        const CcDispatch *letter = &sim->dispatches[i];
        sqlite3_bind_int(statement, 1, i);
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)letter->id);
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)letter->kind);
        sqlite3_bind_int64(statement, 4, (sqlite3_int64)letter->route_id);
        sqlite3_bind_int64(statement, 5, (sqlite3_int64)letter->war_party_id);
        sqlite3_bind_int64(statement, 6,
                           (sqlite3_int64)letter->origin_settlement_id);
        sqlite3_bind_int64(statement, 7,
                           (sqlite3_int64)letter->recipient_settlement_id);
        sqlite3_bind_int64(statement, 8, letter->issued_day);
        sqlite3_bind_int(statement, 9,
                         (letter->in_player_cargo ? 2 : 0) +
                             (letter->delivered ? 1 : 0));
        ok = StepDone(database, statement, error, error_capacity) &&
             ResetStatement(database, statement, error, error_capacity);
    }
    sqlite3_finalize(statement);
    return ok;
}

static bool ReadDispatches(sqlite3 *database, CcSim *sim,
                           char *error, size_t error_capacity)
{
    if (sim->schema_version < 91U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM dispatch ORDER BY slot;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    bool ok = true;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        if (rows >= CC_MAX_DISPATCHES) {
            SetError(error, error_capacity,
                     "Dispatch rows exceed the bound.");
            sqlite3_finalize(statement);
            return false;
        }
        CcDispatch *letter = &sim->dispatches[rows];
        *letter = (CcDispatch){0};
        letter->id = (CcId)sqlite3_column_int64(statement, 1);
        letter->kind = (CcDispatchKind)sqlite3_column_int64(statement, 2);
        letter->route_id = (CcId)sqlite3_column_int64(statement, 3);
        letter->war_party_id = (CcId)sqlite3_column_int64(statement, 4);
        letter->origin_settlement_id =
            (CcId)sqlite3_column_int64(statement, 5);
        letter->recipient_settlement_id =
            (CcId)sqlite3_column_int64(statement, 6);
        letter->issued_day = (int32_t)sqlite3_column_int64(statement, 7);
        int32_t flags = (int32_t)sqlite3_column_int64(statement, 8);
        letter->delivered = (flags & 1) != 0;
        letter->in_player_cargo = (flags & 2) != 0;
        if (letter->kind < CC_DISPATCH_ROAD_REPORT ||
            letter->kind > CC_DISPATCH_CROSSING_PERMIT ||
            letter->issued_day < 0 || flags < 0 || flags > 3) {
            SetError(error, error_capacity,
                     "Dispatch row is outside its range.");
            sqlite3_finalize(statement);
            return false;
        }
        rows += 1;
    }
    if (rows < sim->dispatch_count) {
        SetError(error, error_capacity,
                 "Dispatch rows are missing.");
        ok = false;
    }
    sim->dispatch_count = rows;
    sqlite3_finalize(statement);
    return ok;
}
