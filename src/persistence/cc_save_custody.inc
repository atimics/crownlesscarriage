/* Explicit columns preserve IDs, retired slots, and the portable custody hash. */
static bool SaveCustody(sqlite3 *database, const CcSim *sim, char *error, size_t capacity)
{
    if (sim->schema_version < 99U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO custody_state VALUES(1,?);", &statement, error, capacity)) return false;
    BindId(statement, 1, sim->custody.next_id);
    bool ok = StepDone(database, statement, error, capacity);
    sqlite3_finalize(statement);
    if (!ok || !Prepare(database, "INSERT INTO custody_entry VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);", &statement, error, capacity)) return false;
    for (int i = 0; i < CC_CUSTODY_CAPACITY && ok; ++i) {
        const CcCustodyEntry *entry = &sim->custody.entries[i];
        BindInt(statement, 1, i);
        BindId(statement, 2, entry->id); BindId(statement, 3, entry->revision);
        BindId(statement, 4, entry->owner_id); BindId(statement, 5, entry->source_id);
        BindId(statement, 6, entry->last_event_id); BindInt(statement, 7, (int)entry->holder.kind);
        BindId(statement, 8, entry->holder.id); BindInt(statement, 9, (int)entry->kind);
        BindId(statement, 10, entry->reference_id);
        sqlite3_bind_int64(statement, 11, entry->quantity);
        BindInt(statement, 12, entry->good); BindInt(statement, 13, entry->condition);
        BindInt(statement, 14, entry->capacity); BindInt(statement, 15, entry->active ? 1 : 0);
        ok = StepDone(database, statement, error, capacity);
        sqlite3_reset(statement); sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
    return ok;
}

static bool ReadCustody(sqlite3 *database, CcSim *sim, char *error, size_t capacity)
{
    if (sim->schema_version < 99U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT slot,next_id FROM custody_state ORDER BY slot;", &statement, error, capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW ||
        sqlite3_column_type(statement, 0) != SQLITE_INTEGER ||
        sqlite3_column_int64(statement, 0) != 1 ||
        sqlite3_column_type(statement, 1) != SQLITE_INTEGER) goto invalid;
    sim->custody.next_id = (uint64_t)sqlite3_column_int64(statement, 1);
    if (sqlite3_step(statement) != SQLITE_DONE) goto invalid;
    sqlite3_finalize(statement); statement = NULL;
    if (!Prepare(database, "SELECT slot,id,revision,owner_id,source_id,last_event_id,holder_kind,holder_id,kind,reference_id,quantity,good,condition,capacity,active FROM custody_entry ORDER BY slot;", &statement, error, capacity)) return false;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        if (sqlite3_step(statement) != SQLITE_ROW) goto invalid;
        for (int column = 0; column < 15; ++column)
            if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) goto invalid;
        if (sqlite3_column_int64(statement, 0) != i) goto invalid;
        const int narrow[] = {6, 8, 11, 12, 13};
        for (size_t j = 0; j < sizeof(narrow) / sizeof(narrow[0]); ++j) {
            sqlite3_int64 value = sqlite3_column_int64(statement, narrow[j]);
            if (value < 0 || value > INT32_MAX) goto invalid;
        }
        if (sqlite3_column_int64(statement, 14) < 0 ||
            sqlite3_column_int64(statement, 14) > 1) goto invalid;
        CcCustodyEntry *entry = &sim->custody.entries[i];
        entry->id = (uint64_t)sqlite3_column_int64(statement, 1);
        entry->revision = (uint64_t)sqlite3_column_int64(statement, 2);
        entry->owner_id = (uint64_t)sqlite3_column_int64(statement, 3);
        entry->source_id = (uint64_t)sqlite3_column_int64(statement, 4);
        entry->last_event_id = (uint64_t)sqlite3_column_int64(statement, 5);
        entry->holder.kind = (CcCustodyHolderKind)sqlite3_column_int(statement, 6);
        entry->holder.id = (uint64_t)sqlite3_column_int64(statement, 7);
        entry->kind = (CcCustodyKind)sqlite3_column_int(statement, 8);
        entry->reference_id = (uint64_t)sqlite3_column_int64(statement, 9);
        entry->quantity = sqlite3_column_int64(statement, 10);
        entry->good = sqlite3_column_int(statement, 11);
        entry->condition = sqlite3_column_int(statement, 12);
        entry->capacity = sqlite3_column_int(statement, 13);
        entry->active = sqlite3_column_int(statement, 14) != 0;
    }
    if (sqlite3_step(statement) != SQLITE_DONE) goto invalid;
    sqlite3_finalize(statement);
    return true;
invalid:
    sqlite3_finalize(statement);
    SetError(error, capacity, "Custody rows are invalid or incomplete.");
    return false;
}
