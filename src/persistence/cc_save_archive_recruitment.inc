static bool SaveArchiveRecruitment(sqlite3 *database, const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 78U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO archive_recruitment VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);", &statement, error, error_capacity)) return false;
    const CcArchiveRecruitmentOrder *order = &sim->archive_recruitment;
    sqlite3_bind_int64(statement, 1, (sqlite3_int64)order->status);
    sqlite3_bind_int64(statement, 2, (sqlite3_int64)order->person_id);
    sqlite3_bind_int64(statement, 3, (sqlite3_int64)order->trainer_id);
    sqlite3_bind_int64(statement, 4, (sqlite3_int64)order->seat_id);
    sqlite3_bind_int64(statement, 5, (sqlite3_int64)order->origin_id);
    sqlite3_bind_int64(statement, 6, (sqlite3_int64)order->first_route_id);
    sqlite3_bind_int64(statement, 7, (sqlite3_int64)order->first_hop_id);
    sqlite3_bind_int64(statement, 8, (sqlite3_int64)order->donor_ids[0]);
    sqlite3_bind_int64(statement, 9, (sqlite3_int64)order->donor_ids[1]);
    sqlite3_bind_int64(statement, 10, (sqlite3_int64)order->patron_ids[0]);
    sqlite3_bind_int64(statement, 11, (sqlite3_int64)order->patron_ids[1]);
    sqlite3_bind_int64(statement, 12, (sqlite3_int64)order->donor_shares[0]);
    sqlite3_bind_int64(statement, 13, (sqlite3_int64)order->donor_shares[1]);
    sqlite3_bind_int64(statement, 14, (sqlite3_int64)order->purse);
    sqlite3_bind_int64(statement, 15, (sqlite3_int64)order->wheat);
    sqlite3_bind_int64(statement, 16, (sqlite3_int64)order->paper);
    sqlite3_bind_int64(statement, 17, (sqlite3_int64)order->tools);
    sqlite3_bind_int64(statement, 18, (sqlite3_int64)order->travel_wheat);
    sqlite3_bind_int64(statement, 19, (sqlite3_int64)order->start_day);
    sqlite3_bind_int64(statement, 20, (sqlite3_int64)order->training_days);
    sqlite3_bind_int64(statement, 21, (sqlite3_int64)order->trainer_days);
    sqlite3_bind_int64(statement, 22, (sqlite3_int64)order->arrival_estimate);
    sqlite3_bind_int64(statement, 23, (sqlite3_int64)order->ready_estimate);
    bool ok = StepDone(database, statement, error, error_capacity);
    sqlite3_finalize(statement);
    return ok;
}

static bool ReadArchiveRecruitment(sqlite3 *database, CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 78U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT * FROM archive_recruitment ORDER BY id;", &statement, error, error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW || sqlite3_column_int64(statement, 0) != 1) {
        SetError(error, error_capacity, "Recruitment reservation row is missing.");
        sqlite3_finalize(statement); return false;
    }
    CcArchiveRecruitmentOrder *order = &sim->archive_recruitment;
    if (sqlite3_column_int64(statement, 1) < INT32_MIN || sqlite3_column_int64(statement, 1) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->status = (int32_t)sqlite3_column_int64(statement, 1);
    order->person_id = (CcId)sqlite3_column_int64(statement, 2);
    order->trainer_id = (CcId)sqlite3_column_int64(statement, 3);
    order->seat_id = (CcId)sqlite3_column_int64(statement, 4);
    order->origin_id = (CcId)sqlite3_column_int64(statement, 5);
    order->first_route_id = (CcId)sqlite3_column_int64(statement, 6);
    order->first_hop_id = (CcId)sqlite3_column_int64(statement, 7);
    order->donor_ids[0] = (CcId)sqlite3_column_int64(statement, 8);
    order->donor_ids[1] = (CcId)sqlite3_column_int64(statement, 9);
    order->patron_ids[0] = (CcId)sqlite3_column_int64(statement, 10);
    order->patron_ids[1] = (CcId)sqlite3_column_int64(statement, 11);
    order->donor_shares[0] = (CcMoney)sqlite3_column_int64(statement, 12);
    order->donor_shares[1] = (CcMoney)sqlite3_column_int64(statement, 13);
    order->purse = (CcMoney)sqlite3_column_int64(statement, 14);
    if (sqlite3_column_int64(statement, 15) < INT32_MIN || sqlite3_column_int64(statement, 15) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->wheat = (int32_t)sqlite3_column_int64(statement, 15);
    if (sqlite3_column_int64(statement, 16) < INT32_MIN || sqlite3_column_int64(statement, 16) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->paper = (int32_t)sqlite3_column_int64(statement, 16);
    if (sqlite3_column_int64(statement, 17) < INT32_MIN || sqlite3_column_int64(statement, 17) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->tools = (int32_t)sqlite3_column_int64(statement, 17);
    if (sqlite3_column_int64(statement, 18) < INT32_MIN || sqlite3_column_int64(statement, 18) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->travel_wheat = (int32_t)sqlite3_column_int64(statement, 18);
    if (sqlite3_column_int64(statement, 19) < INT32_MIN || sqlite3_column_int64(statement, 19) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->start_day = (int32_t)sqlite3_column_int64(statement, 19);
    if (sqlite3_column_int64(statement, 20) < INT32_MIN || sqlite3_column_int64(statement, 20) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->training_days = (int32_t)sqlite3_column_int64(statement, 20);
    if (sqlite3_column_int64(statement, 21) < INT32_MIN || sqlite3_column_int64(statement, 21) > INT32_MAX) {
        SetError(error, error_capacity, "Recruitment reservation integer exceeds its range.");
        sqlite3_finalize(statement); return false;
    }
    order->trainer_days = (int32_t)sqlite3_column_int64(statement, 21);
    order->arrival_estimate = (int64_t)sqlite3_column_int64(statement, 22);
    order->ready_estimate = (int64_t)sqlite3_column_int64(statement, 23);
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) SetError(error, error_capacity, "Recruitment reservation has extra rows.");
    sqlite3_finalize(statement); return ok;
}
