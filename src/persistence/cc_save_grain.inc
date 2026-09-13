/* Schema 73 keeps the organiser's purse and delivery receipts together. */
static bool SaveGrainSupplies(sqlite3 *database, const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 73U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO grain_supply VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);", &statement, error, error_capacity)) return false;
    for (int i = 0; i < sim->settlement_count; ++i) {
        const CcGrainSupply *g = &sim->grain_supplies[i];
        BindInt(statement, 1, i);
        BindId(statement, 2, g->organiser_id);
        BindId(statement, 3, g->supplier_id);
        BindId(statement, 4, g->route_id);
        BindId(statement, 5, g->shipment_id);
        BindMoney(statement, 6, g->purse);
        BindMoney(statement, 7, g->spent);
        BindInt(statement, 8, g->ordered);
        BindInt(statement, 9, g->delivered);
        BindInt(statement, 10, g->lost);
        BindInt(statement, 11, g->redirected);
        BindInt(statement, 12, g->last_dispatch_day);
        BindInt(statement, 13, g->last_arrival_day);
        BindInt(statement, 14, g->enabled);
        if (!StepDone(database, statement, error, error_capacity) || !ResetStatement(database, statement, error, error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadGrainSupplies(sqlite3 *database, CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 73U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM grain_supply ORDER BY slot;", &statement, error, error_capacity)) return false;
    int rows = 0, step;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        int slot = sqlite3_column_int(statement, 0);
        if (slot != rows || slot >= sim->settlement_count) {
            sqlite3_finalize(statement); SetError(error, error_capacity, "Grain fund rows are invalid."); return false;
        }
        CcGrainSupply *g = &sim->grain_supplies[slot];
        g->organiser_id = (CcId)sqlite3_column_int64(statement, 1);
        g->supplier_id = (CcId)sqlite3_column_int64(statement, 2);
        g->route_id = (CcId)sqlite3_column_int64(statement, 3);
        g->shipment_id = (CcId)sqlite3_column_int64(statement, 4);
        g->purse = sqlite3_column_int64(statement, 5);
        g->spent = sqlite3_column_int64(statement, 6);
        g->ordered = sqlite3_column_int(statement, 7);
        g->delivered = sqlite3_column_int(statement, 8);
        g->lost = sqlite3_column_int(statement, 9);
        g->redirected = sqlite3_column_int(statement, 10);
        g->last_dispatch_day = sqlite3_column_int(statement, 11);
        g->last_arrival_day = sqlite3_column_int(statement, 12);
        g->enabled = sqlite3_column_int(statement, 13) != 0;
        rows++;
    }
    sqlite3_finalize(statement);
    if (step != SQLITE_DONE || rows != sim->settlement_count) {
        SetError(error, error_capacity, "Grain fund rows are incomplete."); return false;
    }
    return true;
}
