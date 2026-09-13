/* Fixed rows and explicit columns keep saves portable and inspectable. */
static bool SaveGoblinPolitics(sqlite3 *database, const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 75U) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "INSERT INTO goblin_faction VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);", &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < 3; ++i) {
        const CcGoblinFaction *f = &sim->goblin_politics.factions[i];
        BindInt(statement, 1, i);
        BindInt(statement, 2, f->members);
        BindId(statement, 3, f->dungeon_id);
        BindInt(statement, 4, f->lair_room);
        BindInt(statement, 5, f->porter_room);
        BindInt(statement, 6, f->target_room);
        BindMoney(statement, 7, f->coins);
        BindInt(statement, 8, f->gold);
        BindInt(statement, 9, f->gems);
        BindMoney(statement, 10, f->carried_coins);
        BindInt(statement, 11, f->carried_gold);
        BindInt(statement, 12, f->carried_gems);
        BindMoney(statement, 13, f->tribute);
        BindInt(statement, 14, f->deliveries);
        BindInt(statement, 15, f->hunted);
        BindId(statement, 16, f->journey_event_id);
        if (!StepDone(database, statement, error, error_capacity) || !ResetStatement(database, statement, error, error_capacity)) { sqlite3_finalize(statement); return false; }
    }
    sqlite3_finalize(statement);
    if (!Prepare(database, "INSERT INTO goblin_politics VALUES(?,?,?,?,?,?);", &statement, error, error_capacity)) return false;
    for (int32_t i = 0; i < 1; ++i) {
        const CcGoblinPolitics *f = &sim->goblin_politics;
        BindInt(statement, 1, i);
        BindId(statement, 2, f->dragon_id);
        BindInt(statement, 3, f->contest_started_day);
        BindInt(statement, 4, f->crown_faction);
        BindInt(statement, 5, f->raid_faction);
        BindInt(statement, 6, f->next_hunt_faction);
        if (!StepDone(database, statement, error, error_capacity) || !ResetStatement(database, statement, error, error_capacity)) { sqlite3_finalize(statement); return false; }
    }
    sqlite3_finalize(statement);
    if (!Prepare(database, "INSERT INTO dragon_cult VALUES(?,?,?,?,?,?);", &statement, error, error_capacity)) return false;
    for (int32_t species = 0; species < CC_CULT_SPECIES_COUNT; ++species) {
        BindInt(statement, 1, species);
        for (int32_t rank = 0; rank < CC_CULT_RANK_COUNT; ++rank)
            BindInt(statement, rank + 2, sim->dragon_cult.ranks[species][rank]);
        BindMoney(statement, 6, sim->dragon_cult.service[species]);
        if (!StepDone(database, statement, error, error_capacity) || !ResetStatement(database, statement, error, error_capacity)) { sqlite3_finalize(statement); return false; }
    }
    sqlite3_finalize(statement);
    if (!Prepare(database, "INSERT INTO dragon_cult_store VALUES(?,?);", &statement, error, error_capacity)) return false;
    for (int32_t good = -1; good < CC_GOOD_COUNT; ++good) {
        BindInt(statement, 1, good);
        BindMoney(statement, 2, good < 0 ? sim->dragon_cult.offering_coins : sim->dragon_cult.offering_stock[good]);
        if (!StepDone(database, statement, error, error_capacity) || !ResetStatement(database, statement, error, error_capacity)) { sqlite3_finalize(statement); return false; }
    }
    sqlite3_finalize(statement);
    return true;
}

static bool ReadGoblinPolitics(sqlite3 *database, CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->schema_version < 75U) return true;
    sqlite3_stmt *statement = NULL;
    int32_t rows = 0;
    if (!Prepare(database, "SELECT slot,members,dungeon_id,lair_room,porter_room,target_room,coins,gold,gems,carried_coins,carried_gold,carried_gems,tribute,deliveries,hunted,journey_event_id FROM goblin_faction ORDER BY slot;", &statement, error, error_capacity)) return false;
    rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        if (rows >= 3 || sqlite3_column_int(statement, 0) != rows) { sqlite3_finalize(statement); SetError(error, error_capacity, "Goblin rows are invalid."); return false; }
        CcGoblinFaction *f = &sim->goblin_politics.factions[rows];
        f->members = sqlite3_column_int(statement, 1);
        f->dungeon_id = (CcId)sqlite3_column_int64(statement, 2);
        f->lair_room = sqlite3_column_int(statement, 3);
        f->porter_room = sqlite3_column_int(statement, 4);
        f->target_room = sqlite3_column_int(statement, 5);
        f->coins = sqlite3_column_int64(statement, 6);
        f->gold = sqlite3_column_int(statement, 7);
        f->gems = sqlite3_column_int(statement, 8);
        f->carried_coins = sqlite3_column_int64(statement, 9);
        f->carried_gold = sqlite3_column_int(statement, 10);
        f->carried_gems = sqlite3_column_int(statement, 11);
        f->tribute = sqlite3_column_int64(statement, 12);
        f->deliveries = sqlite3_column_int(statement, 13);
        f->hunted = sqlite3_column_int(statement, 14);
        f->journey_event_id = (CcId)sqlite3_column_int64(statement, 15);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != 3) { SetError(error, error_capacity, "Goblin rows are incomplete."); return false; }
    if (!Prepare(database, "SELECT slot,dragon_id,contest_started_day,crown_faction,raid_faction,next_hunt_faction FROM goblin_politics ORDER BY slot;", &statement, error, error_capacity)) return false;
    rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        if (rows >= 1 || sqlite3_column_int(statement, 0) != rows) { sqlite3_finalize(statement); SetError(error, error_capacity, "Goblin rows are invalid."); return false; }
        CcGoblinPolitics *f = &sim->goblin_politics;
        f->dragon_id = (CcId)sqlite3_column_int64(statement, 1);
        f->contest_started_day = sqlite3_column_int(statement, 2);
        f->crown_faction = sqlite3_column_int(statement, 3);
        f->raid_faction = sqlite3_column_int(statement, 4);
        f->next_hunt_faction = sqlite3_column_int(statement, 5);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != 1) { SetError(error, error_capacity, "Goblin rows are incomplete."); return false; }
    if (!Prepare(database, "SELECT species,initiate,bearer,keeper,voice,service FROM dragon_cult ORDER BY species;", &statement, error, error_capacity)) return false;
    rows = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        if (rows >= CC_CULT_SPECIES_COUNT || sqlite3_column_int(statement, 0) != rows) { sqlite3_finalize(statement); SetError(error, error_capacity, "Cult rows are invalid."); return false; }
        for (int32_t rank = 0; rank < CC_CULT_RANK_COUNT; ++rank)
            sim->dragon_cult.ranks[rows][rank] = sqlite3_column_int(statement, rank + 1);
        sim->dragon_cult.service[rows] = sqlite3_column_int64(statement, 5);
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != CC_CULT_SPECIES_COUNT) { SetError(error, error_capacity, "Cult rows are incomplete."); return false; }
    if (!Prepare(database, "SELECT good,quantity FROM dragon_cult_store ORDER BY good;", &statement, error, error_capacity)) return false;
    rows = -1;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        if (rows >= CC_GOOD_COUNT || sqlite3_column_int(statement, 0) != rows) { sqlite3_finalize(statement); SetError(error, error_capacity, "Cult stores are invalid."); return false; }
        sqlite3_int64 value = sqlite3_column_int64(statement, 1);
        if (value < 0 || value > (rows < 0 ? CC_SIM_MAX_MONEY : CC_SIM_MAX_UNITS)) { sqlite3_finalize(statement); SetError(error, error_capacity, "Cult quantity is invalid."); return false; }
        if (rows < 0) sim->dragon_cult.offering_coins = value;
        else sim->dragon_cult.offering_stock[rows] = (int32_t)value;
        rows += 1;
    }
    sqlite3_finalize(statement);
    if (rows != CC_GOOD_COUNT) { SetError(error, error_capacity, "Cult stores are incomplete."); return false; }
    return true;
}
