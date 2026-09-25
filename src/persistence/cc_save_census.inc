static bool SaveCensus(sqlite3 *database, const CcSim *sim,
                       char *error, size_t capacity)
{
    if (!Execute(database, "DELETE FROM resident_census;", error, capacity))
        return false;
    if (sim->schema_version < 117U) return true;
    size_t length = CcCensusEncodedSize(&sim->census);
    uint8_t *bytes = length > 0U ? malloc(length) : NULL;
    if (bytes == NULL ||
        CcCensusEncode(&sim->census, bytes, length) != length) {
        free(bytes);
        SetError(error, capacity, "The resident census could not be encoded.");
        return false;
    }
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO resident_census VALUES(1,?);",
                 &statement, error, capacity)) {
        free(bytes);
        return false;
    }
    bool ok = sqlite3_bind_blob(statement, 1, bytes, (int)length,
                                SQLITE_TRANSIENT) == SQLITE_OK &&
              sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    free(bytes);
    if (!ok) SetError(error, capacity, "The resident census could not be saved.");
    return ok;
}

static bool ReadCensus(sqlite3 *database, CcSim *sim,
                       char *error, size_t capacity)
{
    if (sim->schema_version < 117U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database,
                 "SELECT id,payload FROM resident_census ORDER BY id;",
                 &statement, error, capacity)) return false;
    bool ok = sqlite3_step(statement) == SQLITE_ROW &&
              sqlite3_column_int64(statement, 0) == 1 &&
              sqlite3_column_type(statement, 1) == SQLITE_BLOB &&
              CcCensusDecode(&sim->census,
                  sqlite3_column_blob(statement, 1),
                  (size_t)sqlite3_column_bytes(statement, 1));
    if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) SetError(error, capacity, "The saved resident census is incomplete.");
    return ok;
}
