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
    sqlite3_finalize(statement); return true;
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
    return true;
}
