static bool SaveNotices(sqlite3 *database, const CcSim *sim, char *error, size_t capacity)
{
    if (sim->schema_version < 97U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO notice_state VALUES(1,?);", &statement, error, capacity)) return false;
    BindInt(statement, 1, sim->notice_board.ready ? 1 : 0);
    bool ok = StepDone(database, statement, error, capacity);
    sqlite3_finalize(statement);
    if (!ok || !Prepare(database, "INSERT INTO notice_board VALUES(?,?,?,?,?,?,?);", &statement, error, capacity)) return false;
    for (int i = 0; i < CC_MAX_SITUATIONS && ok; ++i) {
        const CcNotice *notice = &sim->notice_board.notices[i];
        BindInt(statement, 1, i); BindId(statement, 2, notice->situation_id);
        BindId(statement, 3, notice->event_id); BindId(statement, 4, notice->settlement_id);
        BindId(statement, 5, notice->sponsor_id); BindInt(statement, 6, notice->day);
        BindText(statement, 7, notice->text);
        ok = StepDone(database, statement, error, capacity);
        sqlite3_reset(statement); sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement); return ok;
}

static bool ReadNotices(sqlite3 *database, CcSim *sim, char *error, size_t capacity)
{
    if (sim->schema_version < 97U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT id,ready FROM notice_state ORDER BY id;", &statement, error, capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW || sqlite3_column_int64(statement, 0) != 1 ||
        sqlite3_column_type(statement, 1) != SQLITE_INTEGER ||
        sqlite3_column_int64(statement, 1) < 0 || sqlite3_column_int64(statement, 1) > 1) goto invalid;
    sim->notice_board.ready = sqlite3_column_int(statement, 1) != 0;
    if (sqlite3_step(statement) != SQLITE_DONE) goto invalid;
    sqlite3_finalize(statement); statement = NULL;
    if (!Prepare(database, "SELECT slot,situation_id,event_id,settlement_id,sponsor_id,day,text FROM notice_board ORDER BY slot;", &statement, error, capacity)) return false;
    for (int i = 0; i < CC_MAX_SITUATIONS; ++i) {
        if (sqlite3_step(statement) != SQLITE_ROW) goto invalid;
        for (int col = 0; col < 6; ++col)
            if (sqlite3_column_type(statement, col) != SQLITE_INTEGER) goto invalid;
        if (sqlite3_column_int64(statement, 0) != i || sqlite3_column_int64(statement, 5) < 0 ||
            sqlite3_column_int64(statement, 5) > CC_SIM_MAX_DAY) goto invalid;
        CcNotice *notice = &sim->notice_board.notices[i];
        notice->situation_id = (CcId)sqlite3_column_int64(statement, 1);
        notice->event_id = (CcId)sqlite3_column_int64(statement, 2);
        notice->settlement_id = (CcId)sqlite3_column_int64(statement, 3);
        notice->sponsor_id = (CcId)sqlite3_column_int64(statement, 4);
        notice->day = sqlite3_column_int(statement, 5);
        if (!ReadTextColumn(statement, 6, notice->text, sizeof(notice->text), "notice", error, capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    if (sqlite3_step(statement) != SQLITE_DONE) goto invalid;
    sqlite3_finalize(statement); return true;
invalid:
    sqlite3_finalize(statement); SetError(error, capacity, "Notice board rows are invalid or incomplete."); return false;
}
