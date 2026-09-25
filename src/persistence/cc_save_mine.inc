static bool SaveMine(sqlite3 *database, const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 59U) return true;
    sqlite3_stmt *statement=NULL;
    if (!Prepare(database,"INSERT INTO mine_visit VALUES(1,?,?,?,?,?,?,?,?,?,?,?);",
        &statement,error,error_capacity)) return false;
    const CcMineVisit *m=&sim->mine;
    BindInt(statement,1,(int32_t)m->phase); BindId(statement,2,m->site_id);
    BindInt(statement,3,m->x); BindInt(statement,4,m->y); BindInt(statement,5,m->revision);
    BindInt(statement,6,m->return_speed); BindInt(statement,7,m->light);
    BindInt(statement,8,m->steps); BindInt(statement,9,(int32_t)m->seen);
    BindInt(statement,10,m->bar_open); BindInt(statement,11,m->surveyed);
    bool ok=StepDone(database,statement,error,error_capacity);
    sqlite3_finalize(statement);
    if (!ok || !Prepare(database,"INSERT INTO mine_pack VALUES(?,?);",&statement,error,error_capacity)) return false;
    for (int32_t good=0;good<CcGoodCountForSchema(sim->schema_version);++good) {
        BindInt(statement,1,good); BindInt(statement,2,m->pack[good]);
        if (!StepDone(database,statement,error,error_capacity) || !ResetStatement(database,statement,error,error_capacity)) {
            sqlite3_finalize(statement); return false;
        }
    }
    sqlite3_finalize(statement);
    if (sim->schema_version < 103U) return true;
    if (!Prepare(database,"INSERT INTO mine_load_state VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?);",
        &statement,error,error_capacity)) return false;
    BindId(statement,1,m->source_id); BindId(statement,2,m->source_owner_id);
    BindId(statement,3,m->cache_id); BindId(statement,4,m->cache_owner_id);
    BindInt(statement,5,m->source_x); BindInt(statement,6,m->source_y);
    BindInt(statement,7,m->cache_x); BindInt(statement,8,m->cache_y);
    BindInt(statement,9,m->source_released ? 1 : 0);
    BindInt(statement,10,m->bypass_route_seen); BindInt(statement,11,m->contest_active);
    BindInt(statement,12,(int32_t)m->encounter_outcome);
    BindInt(statement,13,(int32_t)m->player_injury);
    ok=StepDone(database,statement,error,error_capacity);
    sqlite3_finalize(statement);
    if (!ok || sim->schema_version < 106U) return ok;
    if (!Prepare(database,"INSERT INTO mine_return_state VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
        &statement,error,error_capacity)) return false;
    BindId(statement,1,m->bread_source_entry_id); BindId(statement,2,m->iron_source_entry_id);
    BindId(statement,3,m->gold_source_entry_id); BindId(statement,4,m->gems_source_entry_id);
    BindInt(statement,5,m->return_revision);
    BindId(statement,6,m->lead_source_id); BindId(statement,7,m->lead_event_id);
    BindInt(statement,8,m->lead_day); BindInt(statement,9,m->lead_document ? 1 : 0);
    BindId(statement,10,m->survey_source_id); BindId(statement,11,m->survey_event_id);
    BindInt(statement,12,m->survey_read_day); BindInt(statement,13,m->survey_observed_day);
    BindId(statement,14,m->bypass_event_id); BindInt(statement,15,m->bypass_day);
    BindId(statement,16,m->haul_receipt_event_id);
    BindInt(statement,17,m->haul_receipt_quantity); BindInt(statement,18,m->haul_receipt_good);
    BindId(statement,19,m->report_recipient_id); BindId(statement,20,m->report_event_id);
    BindInt(statement,21,m->report_day); BindInt(statement,22,m->report_quantity);
    BindInt(statement,23,m->report_good); BindInt(statement,24,(int32_t)m->report_kind);
    BindInt(statement,25,(int32_t)m->reported_encounter_outcome);
    ok=StepDone(database,statement,error,error_capacity);
    sqlite3_finalize(statement); return ok;
}
static bool ReadMine(sqlite3 *database, CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 59U) return true;
    sqlite3_stmt *statement=NULL;
    if (!Prepare(database,"SELECT phase,site_id,x,y,revision,return_speed,light,steps,seen,bar_open,surveyed FROM mine_visit WHERE slot=1;",
        &statement,error,error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement); SetError(error,error_capacity,"Mine visit is missing."); return false;
    }
    CcMineVisit *m=&sim->mine;
    m->phase=(CcMinePhase)sqlite3_column_int(statement,0); m->site_id=(CcId)sqlite3_column_int64(statement,1);
    m->x=sqlite3_column_int(statement,2); m->y=sqlite3_column_int(statement,3);
    m->revision=sqlite3_column_int(statement,4); m->return_speed=sqlite3_column_int(statement,5);
    m->light=sqlite3_column_int(statement,6); m->steps=sqlite3_column_int(statement,7);
    m->seen=(uint32_t)sqlite3_column_int(statement,8);
    m->bar_open=sqlite3_column_int(statement,9)!=0; m->surveyed=sqlite3_column_int(statement,10)!=0;
    sqlite3_finalize(statement);
    if (!Prepare(database,"SELECT good,quantity FROM mine_pack ORDER BY good;",&statement,error,error_capacity)) return false;
    int32_t count=0;
    while (sqlite3_step(statement)==SQLITE_ROW) {
        if (count>=CcGoodCountForSchema(sim->schema_version) || sqlite3_column_int(statement,0)!=count) break;
        m->pack[count++]=sqlite3_column_int(statement,1);
    }
    sqlite3_finalize(statement);
    if (count != CcGoodCountForSchema(sim->schema_version)) {SetError(error,error_capacity,"Mine pack is incomplete."); return false;}
    if (sim->schema_version < 103U) return true;
    if (!Prepare(database,"SELECT source_id,source_owner_id,cache_id,cache_owner_id,source_x,source_y,cache_x,cache_y,source_released,bypass_route_seen,contest_active,encounter_outcome,player_injury FROM mine_load_state WHERE slot=1;",
        &statement,error,error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement); SetError(error,error_capacity,"Mine load is missing."); return false;
    }
    m->source_id=(CcId)sqlite3_column_int64(statement,0);
    m->source_owner_id=(CcId)sqlite3_column_int64(statement,1);
    m->cache_id=(CcId)sqlite3_column_int64(statement,2);
    m->cache_owner_id=(CcId)sqlite3_column_int64(statement,3);
    m->source_x=sqlite3_column_int(statement,4); m->source_y=sqlite3_column_int(statement,5);
    m->cache_x=sqlite3_column_int(statement,6); m->cache_y=sqlite3_column_int(statement,7);
    m->source_released=sqlite3_column_int(statement,8)!=0;
    m->bypass_route_seen=sqlite3_column_int(statement,9)!=0;
    m->contest_active=sqlite3_column_int(statement,10)!=0;
    m->encounter_outcome=(uint8_t)sqlite3_column_int(statement,11);
    m->player_injury=(uint8_t)sqlite3_column_int(statement,12);
    bool done=sqlite3_step(statement)==SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!done) {SetError(error,error_capacity,"Mine load rows are invalid."); return false;}
    if (sim->schema_version < 106U) return true;
    if (!Prepare(database,"SELECT bread_source_entry_id,iron_source_entry_id,gold_source_entry_id,gems_source_entry_id,"
        "return_revision,lead_source_id,lead_event_id,lead_day,lead_document,"
        "survey_source_id,survey_event_id,survey_read_day,survey_observed_day,bypass_event_id,bypass_day,"
        "haul_receipt_event_id,haul_receipt_quantity,haul_receipt_good,"
        "report_recipient_id,report_event_id,report_day,report_quantity,report_good,"
        "report_kind,reported_encounter_outcome "
        "FROM mine_return_state WHERE slot=1;",&statement,error,error_capacity)) return false;
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement); SetError(error,error_capacity,"Mine return record is missing."); return false;
    }
    m->bread_source_entry_id=(CcId)sqlite3_column_int64(statement,0);
    m->iron_source_entry_id=(CcId)sqlite3_column_int64(statement,1);
    m->gold_source_entry_id=(CcId)sqlite3_column_int64(statement,2);
    m->gems_source_entry_id=(CcId)sqlite3_column_int64(statement,3);
    m->return_revision=sqlite3_column_int(statement,4);
    m->lead_source_id=(CcId)sqlite3_column_int64(statement,5);
    m->lead_event_id=(CcId)sqlite3_column_int64(statement,6);
    m->lead_day=sqlite3_column_int(statement,7);
    m->lead_document=sqlite3_column_int(statement,8)!=0;
    m->survey_source_id=(CcId)sqlite3_column_int64(statement,9);
    m->survey_event_id=(CcId)sqlite3_column_int64(statement,10);
    m->survey_read_day=sqlite3_column_int(statement,11);
    m->survey_observed_day=sqlite3_column_int(statement,12);
    m->bypass_event_id=(CcId)sqlite3_column_int64(statement,13);
    m->bypass_day=sqlite3_column_int(statement,14);
    m->haul_receipt_event_id=(CcId)sqlite3_column_int64(statement,15);
    m->haul_receipt_quantity=sqlite3_column_int(statement,16);
    m->haul_receipt_good=sqlite3_column_int(statement,17);
    m->report_recipient_id=(CcId)sqlite3_column_int64(statement,18);
    m->report_event_id=(CcId)sqlite3_column_int64(statement,19);
    m->report_day=sqlite3_column_int(statement,20);
    m->report_quantity=sqlite3_column_int(statement,21);
    m->report_good=sqlite3_column_int(statement,22);
    m->report_kind=(uint8_t)sqlite3_column_int(statement,23);
    m->reported_encounter_outcome=(uint8_t)sqlite3_column_int(statement,24);
    done=sqlite3_step(statement)==SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!done) {SetError(error,error_capacity,"Mine return rows are invalid."); return false;}
    return true;
}
