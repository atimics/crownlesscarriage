static bool SaveRoadStores(sqlite3 *database, const CcSim *sim,
                           char *error, size_t error_capacity)
{
    if (sim->schema_version < 64U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO road_site_stock VALUES(?,?,?);",
                 &statement, error, error_capacity)) return false;
    for (int32_t site = 0; site < sim->road_site_count; ++site) {
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            BindInt(statement, 1, site); BindInt(statement, 2, good);
            BindInt(statement, 3, sim->road_sites[site].stock[good]);
            if (!StepDone(database, statement, error, error_capacity) ||
                !ResetStatement(database, statement, error, error_capacity)) {
                sqlite3_finalize(statement); return false;
            }
        }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadRoadStores(sqlite3 *database, CcSim *sim,
                           char *error, size_t error_capacity)
{
    if (sim->schema_version < 64U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT site_slot,good,quantity FROM road_site_stock ORDER BY site_slot,good;",
                 &statement, error, error_capacity)) return false;
    int32_t rows = 0;
    int status;
    while ((status = sqlite3_step(statement)) == SQLITE_ROW) {
        int64_t site = sqlite3_column_int64(statement, 0);
        int64_t good = sqlite3_column_int64(statement, 1);
        int64_t quantity = sqlite3_column_int64(statement, 2);
        if (site != rows / CC_GOOD_COUNT || good != rows % CC_GOOD_COUNT ||
            site >= sim->road_site_count || quantity < 0 || quantity > CC_SIM_MAX_UNITS) {
            sqlite3_finalize(statement);
            SetError(error, error_capacity, "Road store rows are invalid.");
            return false;
        }
        sim->road_sites[site].stock[good] = (int32_t)quantity;
        rows++;
    }
    sqlite3_finalize(statement);
    if (status != SQLITE_DONE || rows != sim->road_site_count * CC_GOOD_COUNT) {
        SetError(error, error_capacity, "Road store rows are incomplete.");
        return false;
    }
    return true;
}
