static bool SaveSupplyOrders(sqlite3 *database, const CcSim *sim,
                              char *error, size_t error_capacity)
{
    if (sim->schema_version < 117U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO supply_order VALUES(?,?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t town = 0; town < sim->settlement_count; ++town) {
        for (int order = 0; order < 2; ++order) {
            BindInt(statement, 1, town); BindInt(statement, 2, order);
            BindInt(statement, 3, sim->settlements[town].supply_order_day[order]);
            BindInt(statement, 4, sim->settlements[town].supply_order_filled[order]);
            if (!StepDone(database, statement, error, error_capacity) ||
                !ResetStatement(database, statement, error, error_capacity)) {
                sqlite3_finalize(statement); return false;
            }
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadSupplyOrders(sqlite3 *database, CcSim *sim,
                              char *error, size_t error_capacity)
{
    if (sim->schema_version < 117U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT town,kind,day,filled FROM supply_order ORDER BY town,kind;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    int status;
    while ((status = sqlite3_step(statement)) == SQLITE_ROW) {
        int64_t town = sqlite3_column_int64(statement, 0);
        int64_t order = sqlite3_column_int64(statement, 1);
        int64_t day = sqlite3_column_int64(statement, 2);
        int64_t filled = sqlite3_column_int64(statement, 3);
        if (town != rows / 2 || order != rows % 2 || town >= sim->settlement_count ||
            day < 0 || day > sim->current_day || filled < 0 || filled > 8) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Workshop order rows are invalid.");
            return false;
        }
        sim->settlements[town].supply_order_day[order] = (int32_t)day;
        sim->settlements[town].supply_order_filled[order] = (int32_t)filled;
        ++rows;
    }
    sqlite3_finalize(statement);
    if (status != SQLITE_DONE || rows != sim->settlement_count * 2) {
        SetError(error, error_capacity, "Workshop order rows are incomplete.");
        return false;
    }
    return true;
}
