const CcTreasure *CcSimDeepWyrmProphecy(const CcSim *sim)
{
    if (sim == NULL || sim->world_seed != CC_DEEP_WYRM_SEED) return NULL;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *book = &sim->treasures[i];
        if (book->created_day == CC_DEEP_WYRM_DAY && strcmp(book->name, CC_PROPHECY_TITLE) == 0)
            return book;
    }
    return NULL;
}

const CcSettlement *CcSimProphecyDestination(const CcSim *sim)
{
    if (CcSimDeepWyrmProphecy(sim) == NULL) return NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        if (strcmp(sim->settlements[i].name, "Gloamgate") == 0) return &sim->settlements[i];
    return NULL;
}

bool CcSimGiveDeepWyrmProphecy(CcSim *sim)
{
    if (sim == NULL || sim->world_seed != CC_DEEP_WYRM_SEED ||
        sim->current_day != CC_DEEP_WYRM_DAY || CcSimDeepWyrmProphecy(sim) != NULL ||
        CcPlayerCargoUsed(&sim->player) >= sim->player.cargo_capacity) return false;
    CcTreasure *book = AllocateTreasure(sim);
    if (book == NULL) return false;
    (void)snprintf(book->name, sizeof(book->name), "%s", CC_PROPHECY_TITLE);
    book->maker_settlement_id = sim->player.location_id;
    book->owner_id = sim->player.id;
    book->location_id = sim->player.location_id;
    /* A gilt cover and a small gem clasp make this a named, carried treasure. */
    book->gold_content = 1;
    book->gem_content = 1;
    book->craft_work = 1;
    book->appraised_value = 12;
    book->created_day = sim->current_day;
    sim->player.treasure_cargo_slots += 1;
    return true;
}

bool CcSimCanDeliverProphecy(const CcSim *sim)
{
    const CcTreasure *book = CcSimDeepWyrmProphecy(sim);
    const CcSettlement *town = CcSimProphecyDestination(sim);
    return book != NULL && !book->destroyed && town != NULL &&
        !CcSettlementIsAbandoned(town) && !sim->journey.active &&
        sim->mine.phase == CC_MINE_NONE && !sim->dungeon_expedition.active &&
        book->owner_id == sim->player.id && book->location_id == town->id &&
        sim->player.location_id == town->id;
}

static bool ApplyDeliverProphecy(CcSim *sim, const CcCommand *command,
                                 char *error, size_t capacity)
{
    const CcTreasure *held = CcSimDeepWyrmProphecy(sim);
    if (!CcSimCanDeliverProphecy(sim) || held->id != command->target_id) {
        SetError(error, capacity, "Bring the prophecy book to the town council in Gloamgate.");
        return false;
    }
    CcTreasure *book = (CcTreasure *)held;
    book->owner_id = sim->player.location_id;
    sim->player.treasure_cargo_slots -= 1;
    CcEvent *event = PushEvent(sim, CC_EVENT_PROPHECY_DELIVERED, book->id,
        book->location_id, 0U, 1,
        "The company delivers the Prophecy of the Deep Wyrm to Gloamgate. The council hears the warning of three courts and a champion.");
    event->actor_id = sim->player.id;
    SetError(error, capacity, "");
    return true;
}
