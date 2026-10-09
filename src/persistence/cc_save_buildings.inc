static bool SaveBuildings(sqlite3 *database, const CcSim *sim,
                           char *error, size_t capacity)
{
    if (!Execute(database, "DELETE FROM settlement_blueprint; DELETE FROM settlement_building;",
                 error, capacity)) return false;
    if (sim->schema_version < CC_BUILDING_SCHEMA_VERSION) return true;
    sqlite3_stmt *blueprint = NULL;
    sqlite3_stmt *building = NULL;
    if (!Prepare(database, "INSERT INTO settlement_blueprint VALUES(?,?,?,?,?);",
                 &blueprint, error, capacity)) return false;
    if (!Prepare(database, "INSERT INTO settlement_building VALUES(?,?,?,?,?,?,?,?,?,?,?);",
                 &building, error, capacity)) {
        sqlite3_finalize(blueprint);
        return false;
    }
    bool ok = true;
    for (int32_t town = 0; ok && town < sim->settlement_count; ++town) {
        const CcSettlement *place = &sim->settlements[town];
        BindId(blueprint, 1, place->id);
        BindId(blueprint, 2, place->blueprint_id);
        BindId(blueprint, 3, place->blueprint_version);
        BindInt(blueprint, 4, place->building_count);
        BindInt(blueprint, 5, place->building_fire_level);
        ok = StepDone(database, blueprint, error, capacity) &&
             ResetStatement(database, blueprint, error, capacity);
        for (int32_t i = 0; ok && i < place->building_count; ++i) {
            const CcBuildingState *house = &place->buildings[i];
            BindId(building, 1, place->id);
            BindId(building, 2, house->plot_id);
            BindId(building, 3, house->style_seed);
            BindInt(building, 4, house->health);
            BindInt(building, 5, house->roof_health);
            BindInt(building, 6, house->upkeep);
            BindInt(building, 7, house->fire_damage);
            BindInt(building, 8, house->repair_progress);
            BindInt(building, 9, house->built_day);
            BindInt(building, 10, house->last_change_day);
            BindInt(building, 11, house->last_repair_day);
            ok = StepDone(database, building, error, capacity) &&
                 ResetStatement(database, building, error, capacity);
        }
    }
    sqlite3_finalize(blueprint);
    sqlite3_finalize(building);
    return ok;
}

static int32_t BuildingTownSlot(const CcSim *sim, int64_t id)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        if (sim->settlements[i].id == (CcId)id) return i;
    return -1;
}

static bool ReadBuildings(sqlite3 *database, CcSim *sim,
                           char *error, size_t capacity)
{
    if (sim->schema_version < CC_BUILDING_SCHEMA_VERSION) return true;
    sqlite3_stmt *statement = NULL;
    if (!Prepare(database, "SELECT settlement_id,blueprint_id,version,building_count,fire_level "
                 "FROM settlement_blueprint ORDER BY settlement_id;", &statement, error, capacity))
        return false;
    bool towns[CC_MAX_SETTLEMENTS] = {false};
    uint32_t plots[CC_MAX_SETTLEMENTS] = {0};
    bool ok = true;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(statement)) == SQLITE_ROW) {
        int64_t values[5];
        const int64_t maximum[5] = {INT64_MAX, UINT32_MAX, UINT32_MAX, CC_SETTLEMENT_BUILDING_CAPACITY, 100};
        for (int column = 0; ok && column < 5; ++column)
            ok = ReadIntegerColumn(statement, column, column == 4 ? 0 : 1, maximum[column], &values[column],
                                   "settlement blueprint", error, capacity);
        if (!ok) break;
        int32_t town = BuildingTownSlot(sim, values[0]);
        if (town < 0 || towns[town]) { ok = false; break; }
        towns[town] = true;
        CcSettlement *place = &sim->settlements[town];
        place->blueprint_id = (uint32_t)values[1];
        place->blueprint_version = (uint32_t)values[2];
        place->building_count = (int32_t)values[3];
        place->building_fire_level = (int32_t)values[4];
    }
    ok = ok && step == SQLITE_DONE;
    sqlite3_finalize(statement);
    for (int32_t town = 0; ok && town < sim->settlement_count; ++town) ok = towns[town];
    if (!ok) {
        SetError(error, capacity, "The saved settlement blueprints are incomplete or invalid.");
        return false;
    }
    if (!Prepare(database, "SELECT settlement_id,plot_id,style_seed,health,roof_health,upkeep,"
                 "fire_damage,repair_progress,built_day,last_change_day,last_repair_day "
                 "FROM settlement_building ORDER BY settlement_id,plot_id;", &statement, error, capacity))
        return false;
    while (ok && (step = sqlite3_step(statement)) == SQLITE_ROW) {
        int64_t values[11];
        const int64_t minimum[11] = {1, 1, 0, 0, 0, 0, 0, 0, -1000000, 0, 0};
        const int64_t maximum[11] = {INT64_MAX, CC_SETTLEMENT_BUILDING_CAPACITY, UINT32_MAX,
            100, 100, 100, 100, 100, sim->current_day, sim->current_day, sim->current_day};
        for (int column = 0; ok && column < 11; ++column)
            ok = ReadIntegerColumn(statement, column, minimum[column], maximum[column],
                                   &values[column], "settlement building", error, capacity);
        if (!ok) break;
        int32_t town = BuildingTownSlot(sim, values[0]);
        uint32_t plot = (uint32_t)values[1];
        if (town < 0 || plot > (uint32_t)sim->settlements[town].building_count) { ok = false; break; }
        uint32_t bit = UINT32_C(1) << (plot - 1U);
        if ((plots[town] & bit) != 0U) { ok = false; break; }
        plots[town] |= bit;
        sim->settlements[town].buildings[plot - 1U] = (CcBuildingState){
            .plot_id = plot, .style_seed = (uint32_t)values[2],
            .health = (int32_t)values[3], .roof_health = (int32_t)values[4],
            .upkeep = (int32_t)values[5], .fire_damage = (int32_t)values[6],
            .repair_progress = (int32_t)values[7], .built_day = (int32_t)values[8],
            .last_change_day = (int32_t)values[9], .last_repair_day = (int32_t)values[10]
        };
    }
    ok = ok && step == SQLITE_DONE;
    sqlite3_finalize(statement);
    for (int32_t town = 0; ok && town < sim->settlement_count; ++town)
        ok = plots[town] == (UINT32_C(1) << (uint32_t)sim->settlements[town].building_count) - 1U;
    if (!ok) SetError(error, capacity, "The saved settlement buildings are incomplete or invalid.");
    return ok;
}
