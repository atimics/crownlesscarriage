/* The versioned portable payload spells every field out in cc_wants_codec.c. */
static bool SaveWants(sqlite3 *database, const CcSim *sim, char *error, size_t capacity)
{
    if (!Execute(database, "DELETE FROM wants_state;", error, capacity)) return false;
    if (sim->schema_version < 117U) return true;
    uint8_t bytes[CC_WANTS_WIRE_CAPACITY];
    size_t length = CcWantsEncode(&sim->wants, bytes, sizeof(bytes));
    sqlite3_stmt *statement = NULL;
    if (length == 0 || !Prepare(database, "INSERT INTO wants_state VALUES(1,?);", &statement, error, capacity)) return false;
    bool ok = sqlite3_bind_blob(statement, 1, bytes, (int)length, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) SetError(error, capacity, "The personal requests and belongings could not be written.");
    return ok;
}
static bool ReadWants(sqlite3 *database, CcSim *sim, char *error, size_t capacity)
{
    if (sim->schema_version < 117U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT id,payload FROM wants_state ORDER BY id;", &statement, error, capacity)) return false;
    bool ok = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int64(statement, 0) == 1 &&
        sqlite3_column_type(statement, 1) == SQLITE_BLOB &&
        CcWantsDecode(&sim->wants, sqlite3_column_blob(statement, 1), (size_t)sqlite3_column_bytes(statement, 1));
    if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) SetError(error, capacity, "The personal requests and belongings are incomplete.");
    return ok;
}

