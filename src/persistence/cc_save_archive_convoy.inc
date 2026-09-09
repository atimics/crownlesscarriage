static bool SaveArchiveConvoy(sqlite3 *database, const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 92U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO archive_convoy VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);", &statement, error, error_capacity)) return false;
    sqlite3_bind_int64(statement, 1, (sqlite3_int64)sim->archive_convoy.origin_id);
    sqlite3_bind_int64(statement, 2, (sqlite3_int64)sim->archive_convoy.destination_id);
    sqlite3_bind_int64(statement, 3, (sqlite3_int64)sim->archive_convoy.sponsor_id);
    sqlite3_bind_int64(statement, 4, (sqlite3_int64)sim->archive_convoy.funding_kingdom_id);
    sqlite3_bind_int64(statement, 5, (sqlite3_int64)sim->archive_convoy.carriage_id);
    sqlite3_bind_int64(statement, 6, (sqlite3_int64)sim->archive_convoy.first_route_id);
    sqlite3_bind_int64(statement, 7, (sqlite3_int64)sim->archive_convoy.first_hop_id);
    sqlite3_bind_int64(statement, 8, (sqlite3_int64)sim->archive_convoy.book_ids[0]);
    sqlite3_bind_int64(statement, 9, (sqlite3_int64)sim->archive_convoy.book_ids[1]);
    sqlite3_bind_int64(statement, 10, (sqlite3_int64)sim->archive_convoy.book_ids[2]);
    sqlite3_bind_int64(statement, 11, (sqlite3_int64)sim->archive_convoy.book_ids[3]);
    sqlite3_bind_int64(statement, 12, (sqlite3_int64)sim->archive_convoy.purse);
    sqlite3_bind_int64(statement, 13, (sqlite3_int64)sim->archive_convoy.wheat);
    sqlite3_bind_int64(statement, 14, (sqlite3_int64)sim->archive_convoy.book_count);
    sqlite3_bind_int64(statement, 15, (sqlite3_int64)sim->archive_convoy.reserved_day);
    sqlite3_bind_int64(statement, 16, (sqlite3_int64)sim->archive_convoy.status);
    bool ok = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement); return ok;
}
static bool ReadArchiveConvoy(sqlite3 *database, CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 92U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT id,origin_id,destination_id,sponsor_id,funding_kingdom_id,carriage_id,first_route_id,first_hop_id,book_ids_0,book_ids_1,book_ids_2,book_ids_3,purse,wheat,book_count,reserved_day,status FROM archive_convoy ORDER BY id;", &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW || sqlite3_column_int64(statement, 0) != 1) {
        SetError(error, error_capacity, "Archive convoy row is missing.");
        sqlite3_finalize(statement); return false;
    }
    if (sqlite3_column_type(statement, 1) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.origin_id = (CcId)sqlite3_column_int64(statement, 1);
    if (sqlite3_column_type(statement, 2) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.destination_id = (CcId)sqlite3_column_int64(statement, 2);
    if (sqlite3_column_type(statement, 3) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.sponsor_id = (CcId)sqlite3_column_int64(statement, 3);
    if (sqlite3_column_type(statement, 4) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.funding_kingdom_id = (CcId)sqlite3_column_int64(statement, 4);
    if (sqlite3_column_type(statement, 5) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.carriage_id = (CcId)sqlite3_column_int64(statement, 5);
    if (sqlite3_column_type(statement, 6) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.first_route_id = (CcId)sqlite3_column_int64(statement, 6);
    if (sqlite3_column_type(statement, 7) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.first_hop_id = (CcId)sqlite3_column_int64(statement, 7);
    if (sqlite3_column_type(statement, 8) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.book_ids[0] = (CcId)sqlite3_column_int64(statement, 8);
    if (sqlite3_column_type(statement, 9) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.book_ids[1] = (CcId)sqlite3_column_int64(statement, 9);
    if (sqlite3_column_type(statement, 10) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.book_ids[2] = (CcId)sqlite3_column_int64(statement, 10);
    if (sqlite3_column_type(statement, 11) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.book_ids[3] = (CcId)sqlite3_column_int64(statement, 11);
    if (sqlite3_column_type(statement, 12) != SQLITE_INTEGER) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.purse = (CcMoney)sqlite3_column_int64(statement, 12);
    if (sqlite3_column_type(statement, 13) != SQLITE_INTEGER || sqlite3_column_int64(statement, 13) < 0 || sqlite3_column_int64(statement, 13) > INT32_MAX) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.wheat = (int32_t)sqlite3_column_int64(statement, 13);
    if (sqlite3_column_type(statement, 14) != SQLITE_INTEGER || sqlite3_column_int64(statement, 14) < 0 || sqlite3_column_int64(statement, 14) > INT32_MAX) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.book_count = (int32_t)sqlite3_column_int64(statement, 14);
    if (sqlite3_column_type(statement, 15) != SQLITE_INTEGER || sqlite3_column_int64(statement, 15) < 0 || sqlite3_column_int64(statement, 15) > INT32_MAX) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.reserved_day = (int32_t)sqlite3_column_int64(statement, 15);
    if (sqlite3_column_type(statement, 16) != SQLITE_INTEGER || sqlite3_column_int64(statement, 16) < 0 || sqlite3_column_int64(statement, 16) > INT32_MAX) {
        SetError(error, error_capacity, "Archive convoy value is invalid.");
        sqlite3_finalize(statement); return false;
    }
    sim->archive_convoy.status = (int32_t)sqlite3_column_int64(statement, 16);
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) SetError(error, error_capacity, "Archive convoy has extra rows.");
    sqlite3_finalize(statement); return ok;
}
