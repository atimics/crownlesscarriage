/* The versioned portable payload spells every field out in cc_scriven_codec.c. */
static bool SaveScriven(sqlite3 *database, const CcSim *sim, char *error, size_t capacity)
{
    if (!Execute(database, "DELETE FROM scriven_state;", error, capacity)) return false;
    if (sim->schema_version < 113U) return true;
    uint8_t bytes[CC_SCRIVEN_WIRE_CAPACITY];
    size_t length = CcScrivenEncode(&sim->scriven, bytes, sizeof(bytes));
    sqlite3_stmt *statement = NULL;
    if (length == 0 || !Prepare(database, "INSERT INTO scriven_state VALUES(1,?);", &statement, error, capacity)) return false;
    bool ok = sqlite3_bind_blob(statement, 1, bytes, (int)length, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) SetError(error, capacity, "The saved tomes and calendar could not be written.");
    return ok;
}
static bool ReadScriven(sqlite3 *database, CcSim *sim, char *error, size_t capacity)
{
    if (sim->schema_version < 113U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT id,payload FROM scriven_state ORDER BY id;", &statement, error, capacity)) return false;
    bool ok = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int64(statement, 0) == 1 &&
        sqlite3_column_type(statement, 1) == SQLITE_BLOB &&
        CcScrivenDecode(&sim->scriven, sqlite3_column_blob(statement, 1), (size_t)sqlite3_column_bytes(statement, 1));
    if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) SetError(error, capacity, "The saved tomes and calendar are incomplete.");
    return ok;
}

static bool SaveCrownCalendar(sqlite3 *database, const CcSim *sim, char *error, size_t capacity)
{
    if (!Execute(database, "DELETE FROM crown_calendar_state;", error, capacity)) return false;
    if (sim->schema_version < 115U) return true;
    uint8_t bytes[CC_SCRIVEN_WIRE_CAPACITY];
    size_t length = CcCrownCalendarEncode(&sim->crown_calendar, bytes, sizeof(bytes));
    sqlite3_stmt *statement = NULL;
    if (length == 0 || !Prepare(database, "INSERT INTO crown_calendar_state VALUES(1,?);", &statement, error, capacity)) return false;
    bool ok = sqlite3_bind_blob(statement, 1, bytes, (int)length, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) SetError(error, capacity, "The saved Crown Age calendar could not be written.");
    return ok;
}
static bool ReadCrownCalendar(sqlite3 *database, CcSim *sim, char *error, size_t capacity)
{
    if (sim->schema_version < 115U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT id,payload FROM crown_calendar_state ORDER BY id;", &statement, error, capacity)) return false;
    bool ok = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int64(statement, 0) == 1 &&
        sqlite3_column_type(statement, 1) == SQLITE_BLOB &&
        CcCrownCalendarDecode(&sim->crown_calendar, sqlite3_column_blob(statement, 1), (size_t)sqlite3_column_bytes(statement, 1));
    if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) SetError(error, capacity, "The saved Crown Age calendar are incomplete.");
    return ok;
}
