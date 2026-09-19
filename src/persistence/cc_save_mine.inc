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
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) {
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
        if (count>=CC_GOOD_COUNT || sqlite3_column_int(statement,0)!=count) break;
        m->pack[count++]=sqlite3_column_int(statement,1);
    }
    sqlite3_finalize(statement);
    if (count != CC_GOOD_COUNT) {SetError(error,error_capacity,"Mine pack is incomplete."); return false;}
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
    return true;
}
