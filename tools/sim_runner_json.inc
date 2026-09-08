/* JSON Lines: one complete, read-only checkpoint per line. IDs are strings. */
static void JsonString(const char *value)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; *p != 0; ++p) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p < 32) printf("\\u%04x", (unsigned)*p);
        else putchar(*p);
    }
    putchar('"');
}

static void JsonStock(const int32_t *values)
{
    putchar('[');
    for (int32_t i = 0; i < CC_GOOD_COUNT; ++i) printf("%s%d", i ? "," : "", values[i]);
    putchar(']');
}

static void JsonTotals(const uint64_t *values, int32_t count)
{
    putchar('[');
    for (int32_t i = 0; i < count; ++i) printf("%s%" PRIu64, i ? "," : "", values[i]);
    putchar(']');
}

static void JsonRecipe(const CcRecipeAccounting *recipe)
{
    printf("{\"input\":"); JsonTotals(recipe->input, CC_GOOD_COUNT);
    printf(",\"output\":"); JsonTotals(recipe->output, CC_GOOD_COUNT);
    printf(",\"work\":%" PRIu64 ",\"tools_worn\":%" PRIu64 ",\"gates\":", recipe->work, recipe->tools_worn);
    JsonTotals(recipe->gates, CC_PRODUCTION_GATE_COUNT); putchar('}');
}

static void JsonHerd(const CcHerdAccounting *herd, int32_t adults, int32_t young,
                     int32_t hunger, int32_t condition)
{
    printf("{\"adults\":%d,\"young\":%d,\"hunger\":%d,\"condition\":%d,\"feed\":",
        adults, young, hunger, condition);
    JsonTotals(herd->feed, CC_GOOD_COUNT);
    printf(",\"output\":"); JsonTotals(herd->output, CC_GOOD_COUNT);
    printf(",\"cap_loss\":"); JsonTotals(herd->cap_loss, CC_GOOD_COUNT);
    putchar('}');
}

static void PrintProductionJson(const CcSim *sim, int32_t start_day, int32_t year,
    const char *fixture, const char *dragon_policy, const CcNutritionAccounting *nutrition,
    const CcSmithyAccounting *smithy, const CcRoadProductionAccounting *sites,
    const int32_t *open_days)
{
    CcHungerSnapshot hunger = CcSimHungerSnapshot(sim);
    printf("{\"protocol\":5,\"fixture\":"); JsonString(fixture);
    printf(",\"dragon_policy\":"); JsonString(dragon_policy);
    printf(",\"comparison_scope\":\"whole-policy\",\"dragon\":{\"id\":\"%" PRIu64 "\",\"slain\":%s,\"slain_day\":%d,\"life_stage\":%d,\"activity\":%d,\"body_condition\":%d,\"crown_strength\":%d,\"eggs\":%d}",
        sim->dragon.id, sim->dragon.slain ? "true" : "false", sim->dragon.slain_day,
        (int)sim->dragon.life_stage, (int)sim->dragon.activity, sim->dragon.body_condition,
        sim->dragon.crown_strength, sim->dragon.egg_count);
    printf(",\"world_seed\":%" PRIu32 ",\"schema\":%" PRIu32 ",\"generator\":%" PRIu32
           ",\"accounting_start_day\":%d,\"elapsed_years\":%d,\"day\":%d,\"state_hash\":\"%016" PRIx64 "\",\"goods\":[",
        sim->world_seed, sim->schema_version, sim->generator_version, start_day, year, sim->current_day, CcSimHash(sim));
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (good) putchar(',');
        JsonString(CcGoodName((CcGood)good));
    }
    printf("],\"production_gates\":[\"ready\",\"invalid\",\"closed\",\"condition\",\"capacity\",\"output_full\",\"work\",\"tools\",\"input\"],\"hunger\":{\"inhabited\":%d,\"abandoned\":%d,\"population\":%" PRId64
           ",\"average\":%d,\"maximum\":%d,\"population_weighted\":%d},\"towns\":[",
        hunger.inhabited_settlements, hunger.abandoned_settlements, hunger.population,
        hunger.average, hunger.maximum, hunger.population_weighted);
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *town = &sim->settlements[i];
        const CcTownSmithyAccounting *craft = &smithy->towns[i];
        const CcTownProductionAccounting *production = &sites->towns[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"name\":", i ? "," : "", town->id); JsonString(town->name);
        printf(",\"population\":%d,\"hunger\":%d,\"market_coins\":%" PRId64
               ",\"war_chest\":%" PRId64 ",\"fire_damage\":%d,\"stock\":",
            town->population, town->hunger, town->market_coins, town->war_chest, town->fire_damage);
        JsonStock(town->stock); printf(",\"reserves\":"); JsonStock(town->reserve_target);
        printf(",\"civilian_use\":"); JsonTotals(nutrition->towns[i].civilian_units, CC_GOOD_COUNT);
        printf(",\"aged_loss\":"); JsonTotals(nutrition->towns[i].aged_units, CC_GOOD_COUNT);
        printf(",\"overflow_loss\":"); JsonTotals(nutrition->towns[i].overflow_units, CC_GOOD_COUNT);
        printf(",\"smithy\":{\"tools\":%" PRIu64 ",\"weapons\":%" PRIu64 ",\"iron_used\":%" PRIu64
               ",\"wood_used\":%" PRIu64 ",\"tools_worn\":%" PRIu64 "}",
            craft->tools_made, craft->weapons_made, craft->iron_used, craft->wood_used, craft->tools_worn);
        printf(",\"production\":{\"active_weeks\":%" PRIu64 ",\"inactive_weeks\":%" PRIu64 ",\"primary_output\":",
            production->active_weeks, production->inactive_weeks);
        JsonTotals(production->primary_output, CC_GOOD_COUNT);
        printf(",\"primary_tools_worn\":%" PRIu64, production->primary_tools_worn);
        printf(",\"primary_cap_loss\":"); JsonTotals(production->primary_cap_loss, CC_GOOD_COUNT);
        printf(",\"rare_mine_output\":"); JsonTotals(production->rare_mine_output, CC_GOOD_COUNT);
        printf(",\"bakery\":"); JsonRecipe(&production->bakery);
        printf(",\"paper\":"); JsonRecipe(&production->paper);
        printf(",\"treasure\":"); JsonRecipe(&production->treasure);
        printf(",\"treasures_completed\":%" PRIu64 "},\"herds\":{\"cows\":", production->treasures_completed);
        JsonHerd(&production->cows, town->cow_adults, town->cow_calves, town->cow_hunger, town->cow_condition);
        printf(",\"sheep\":");
        JsonHerd(&production->sheep, town->sheep_adults, town->sheep_lambs, town->sheep_hunger, town->sheep_condition);
        printf(",\"ponies\":");
        JsonHerd(&production->ponies, town->pony_adults, town->pony_foals, town->pony_hunger, town->pony_condition);
        printf(",\"dairy_nutrition\":%" PRIu64 ",\"dairy_used\":%" PRIu64 ",\"dairy_unused\":%" PRIu64 "}}",
            production->dairy_nutrition, production->dairy_used, production->dairy_unused);
    }
    printf("],\"sites\":[");
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        const CcRoadSite *site = &sim->road_sites[i];
        const CcSiteProductionAccounting *row = &sites->sites[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"name\":", i ? "," : "", site->id); JsonString(site->name);
        printf(",\"home_id\":\"%" PRIu64 "\",\"route_id\":\"%" PRIu64 "\",\"progress_milli\":%d,\"accessible\":%s,\"condition\":%d,\"stock\":",
            site->home_settlement_id, site->route_id, site->progress_milli, site->accessible ? "true" : "false", site->condition);
        JsonStock(site->stock);
#define TOTAL(name, field) printf(",\"" name "\":"); JsonTotals(row->field, CC_GOOD_COUNT)
        TOTAL("input", input); TOTAL("output", output);
        TOTAL("maintenance_input", maintenance_input);
        TOTAL("sent", freight_sent); TOTAL("received", freight_received);
        TOTAL("shipped", freight_shipped); TOTAL("delivered", freight_delivered); TOTAL("lost", freight_lost);
#undef TOTAL
        printf(",\"work\":%" PRIu64 ",\"route_repair\":%" PRIu64 ",\"gates\":", row->work, row->route_repair);
        JsonTotals(row->gates, CC_PRODUCTION_GATE_COUNT);
        printf(",\"maintenance_work\":%" PRIu64 ",\"site_repair\":%" PRIu64 ",\"wear\":null}", row->maintenance_work, row->site_repair);
    }
    printf("],\"routes\":[");
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"condition\":%d,\"closed\":%s,\"open_days\":%d,\"closed_days\":%d,\"closure_reason\":null",
            i ? "," : "", route->id, route->condition, route->closed ? "true" : "false",
            open_days[i], sim->current_day - start_day - open_days[i]);
        printf(",\"from_id\":\"%" PRIu64 "\",\"to_id\":\"%" PRIu64 "\",\"travel_days\":%d,\"capacity\":%d}",
            route->from_id, route->to_id, route->travel_days, route->capacity);
    }
    printf("],\"carriages\":[");
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        const CcRoyalCarriage *cart = &sim->royal_carriages[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"location_id\":\"%" PRIu64 "\",\"destination_id\":\"%" PRIu64
               "\",\"shipment_id\":\"%" PRIu64 "\",\"mode\":%d,\"condition\":%d,\"trips\":%d,\"losses\":%d",
            i ? "," : "", cart->id, cart->location_id, cart->destination_id, cart->active_shipment_id,
            cart->mode, cart->condition, cart->trips_completed, cart->cargo_losses);
        printf(",\"route_id\":\"%" PRIu64 "\",\"departure_day\":%d,\"arrival_day\":%d}",
            cart->route_id, cart->departure_day, cart->arrival_day);
    }
    printf("],\"shipments\":[");
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *cargo = &sim->shipments[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"origin_id\":\"%" PRIu64 "\",\"destination_id\":\"%" PRIu64
               "\",\"good\":%d,\"quantity\":%d,\"status\":%d,\"departure_day\":%d,\"arrival_day\":%d",
            i ? "," : "", cargo->id, cargo->origin_id, cargo->destination_id,
            (int)cargo->good, cargo->quantity, (int)cargo->status, cargo->departure_day, cargo->arrival_day);
        printf(",\"route_id\":\"%" PRIu64 "\",\"final_destination_id\":\"%" PRIu64 "\"}",
            cargo->route_id, cargo->final_destination_id);
    }
    printf("],\"treasuries\":[");
    for (int32_t i = 0; i < sim->kingdom_count; ++i)
        printf("%s{\"id\":\"%" PRIu64 "\",\"coins\":%" PRId64 "}", i ? "," : "", sim->kingdoms[i].id, sim->kingdoms[i].treasury);
    printf("],\"dragon_hoard\":%" PRId64 ",\"goblin_coins\":%" PRId64 ",\"player_coins\":%" PRId64
           ",\"monastery_reserve\":%" PRId64 "}\n", sim->dragon.hoard, sim->goblins.lair_coins,
        sim->player.coins, sim->iron_ledger_reserve);
}
