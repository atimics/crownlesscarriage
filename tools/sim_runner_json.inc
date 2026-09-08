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

#include "sim_runner_network.inc"

static void JsonArchiveWork(const CcSim *sim)
{
    CcArchiveWorkPlan plan = CcSimArchiveWorkPlan(sim);
    printf("{\"semantics\":\"held_supply_plan_snapshot\",\"seat_id\":\"%" PRIu64
           "\",\"nominal_scribes\":%d,\"eligible_scribes\":%d,\"wheat_required\":%d,\"recording_ready\":%s}",
        plan.seat_id, sim->archives.scribes, plan.eligible_scribes,
        plan.wheat_required, plan.recording_ready ? "true" : "false");
}

static void JsonArchiveFunding(const CcSim *sim)
{
    CcArchiveFundingPlan plan = CcSimArchiveFundingPlan(sim);
    printf("{\"semantics\":\"treasury_top_up_plan_snapshot\",\"seat_id\":\"%" PRIu64
           "\",\"total\":%" PRId64 ",\"donors\":[", plan.seat_id, plan.total);
    for (int32_t i = 0; i < plan.donor_count; ++i)
        printf("%s{\"kingdom_id\":\"%" PRIu64 "\",\"amount\":%" PRId64 "}",
            i ? "," : "", plan.donor_ids[i], plan.shares[i]);
    printf("],\"blocker\":"); JsonString(CcArchiveFundingBlockerName(plan.blocker));
    putchar('}');
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

static void JsonThreatSnapshot(const CcSim *sim)
{
    printf("{\"semantics\":\"snapshot\",\"bandit_groups\":[");
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcBanditGroup *group = &sim->bandits[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"route_id\":\"%" PRIu64
               "\",\"camp_settlement_id\":\"%" PRIu64 "\",\"name\":",
               i ? "," : "", group->id, group->route_id, group->camp_settlement_id);
        JsonString(group->name);
        printf(",\"members\":%d,\"supplies\":%d,\"influence\":%d,\"raid_phase\":%d}",
               group->members, group->supplies, group->influence, (int)group->raid_phase);
    }
    printf("],\"monster_groups\":[");
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        const CcMonsterPopulation *group = &sim->monsters[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"dungeon_id\":\"%" PRIu64 "\",\"name\":",
               i ? "," : "", group->id, group->dungeon_id);
        JsonString(group->name);
        printf(",\"population\":%d,\"pressure\":%d,\"hunting_pressure\":%d}",
               group->population, group->pressure, group->hunting_pressure);
    }
    printf("]}");
}

static const char *const ROAD_RECOVERY_BLOCK_NAMES[] = {
    "invalid", "open", "war_border", "calendar", "abandoned_endpoint",
    "people", "food", "wood", "stone", "tools"};

static void JsonRoadEndpoint(const CcSim *sim, CcId id)
{
    const CcSettlement *town = CcSimSettlement(sim, id);
    if (town == NULL) { printf("null"); return; }
    printf("{\"id\":\"%" PRIu64 "\",\"kingdom_id\":\"%" PRIu64
           "\",\"population\":%d,\"inhabited\":%s}",
        town->id, town->kingdom_id, town->population, town->population > 0 ? "true" : "false");
}

static void JsonRoadContext(const CcSim *sim, const CcRoute *route)
{
    printf("{\"semantics\":\"snapshot\",\"from\":"); JsonRoadEndpoint(sim, route->from_id);
    printf(",\"to\":"); JsonRoadEndpoint(sim, route->to_id);
    printf(",\"crosses_kingdom_border\":%s,\"crosses_war_border\":%s,\"smuggler_route\":%s,\"royal_carriage_rules\":[",
        CcSimRouteCrossesKingdomBorder(sim, route->id) ? "true" : "false",
        CcSimRouteCrossesWarBorder(sim, route->id) ? "true" : "false",
        route->smuggler_route ? "true" : "false");
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        printf("%s{\"kingdom_id\":\"%" PRIu64 "\",\"official_route_eligible\":%s}",
            i ? "," : "", sim->kingdoms[i].id,
            CcSimRoyalCarriageCanUseRoute(sim, sim->kingdoms[i].id, route->id) ? "true" : "false");
    }
    printf("]}");
}

static void JsonRoadRecovery(const CcSim *sim, CcId route_id)
{
    CcRoadRecoveryPlan plan = CcSimRoadRecoveryPlan(sim, route_id);
    printf("{\"semantics\":\"evaluated_plan_snapshot\",\"blocked_mask\":%" PRIu32 ",\"blocked_reasons\":[", plan.blocked);
    bool separator = false;
    for (unsigned bit = 0; bit < sizeof(ROAD_RECOVERY_BLOCK_NAMES) / sizeof(ROAD_RECOVERY_BLOCK_NAMES[0]); ++bit) {
        if ((plan.blocked & (UINT32_C(1) << bit)) == 0U) continue;
        if (separator) putchar(',');
        JsonString(ROAD_RECOVERY_BLOCK_NAMES[bit]); separator = true;
    }
    printf("],\"labor_base_id\":\"%" PRIu64 "\",\"supplier_id\":\"%" PRIu64 "\",\"next_work_day\":",
        plan.labor_base_id, plan.supplier_id);
    if (plan.next_work_day < 0) printf("null"); else printf("%" PRId64, plan.next_work_day);
    const char *names[] = {"population", "food_rations", "wood", "stone", "tools", "effort", "people_used"};
    const int32_t values[] = {plan.population, plan.food_rations, plan.wood, plan.stone, plan.tools, plan.effort, plan.people_used};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        printf(",\"%s\":", names[i]);
        if (values[i] < 0) printf("null"); else printf("%d", values[i]);
    }
    putchar('}');
}

static const char *const CAMPAIGN_BLOCK_NAMES[] = {
    "invalid", "active", "cooldown", "dragon_slain", "pledges", "dragon_age",
    "food", "tools", "weapons", "patron", "hero", "seat"};
static const char *const RITUAL_BLOCK_NAMES[] = {
    "invalid", "members", "devotion", "cohesion", "coins", "relics", "food", "tools", "weapons"};

static void JsonPlanBlocks(uint32_t blocked, const char *const *names, size_t count)
{
    printf("\"blocked_mask\":%" PRIu32 ",\"blocked_reasons\":[", blocked);
    bool separator = false;
    for (size_t bit = 0; bit < count; ++bit) {
        if ((blocked & (UINT32_C(1) << bit)) == 0U) continue;
        if (separator) putchar(',');
        JsonString(names[bit]); separator = true;
    }
    putchar(']');
}

static void JsonCampaignPlan(const CcSim *sim)
{
    CcCampaignLaunchPlan plan = CcSimCampaignLaunchPlan(sim);
    printf("{\"semantics\":\"held_supply_plan_snapshot\",\"policy_intent\":null,");
    JsonPlanBlocks(plan.blocked, CAMPAIGN_BLOCK_NAMES, sizeof(CAMPAIGN_BLOCK_NAMES) / sizeof(CAMPAIGN_BLOCK_NAMES[0]));
    printf(",\"phase\":%d,\"attempts_lifetime\":%d,\"cooldown_days\":%d,\"pledged_mask\":%" PRIu32
           ",\"pledged_count\":%d,\"prepare_eligible\":%s,\"food_rations\":%d,\"tools\":%d,\"weapons\":%d",
        (int)sim->dragon_campaign.phase, sim->dragon_campaign.attempts,
        sim->dragon_campaign.cooldown_days, plan.pledged_mask, plan.pledged_count,
        (plan.blocked & CC_CAMPAIGN_PREPARATION_BLOCKS) == 0U ? "true" : "false",
        plan.food_rations, plan.tools, plan.weapons);
    printf(",\"patron_id\":\"%" PRIu64 "\",\"hero_id\":\"%" PRIu64 "\",\"origin_id\":\"%" PRIu64 "\"}",
        plan.patron_id, plan.hero_id, plan.origin_id);
}

static void JsonRitualPlan(const CcSim *sim)
{
    CcRitualOfferingPlan plan = CcSimRitualOfferingPlan(sim);
    printf("{\"semantics\":\"held_offering_plan_snapshot\",");
    JsonPlanBlocks(plan.blocked, RITUAL_BLOCK_NAMES, sizeof(RITUAL_BLOCK_NAMES) / sizeof(RITUAL_BLOCK_NAMES[0]));
    printf(",\"cult_id\":\"%" PRIu64 "\",\"lair_id\":\"%" PRIu64 "\",\"phase\":%d,\"days_remaining\":%d,\"afterdeath_days\":%d,\"tribute_phase\":%d",
        sim->goblins.id, sim->goblins.lair_settlement_id, (int)sim->goblins.dragon_seed_phase,
        sim->goblins.dragon_seed_days_remaining, sim->dragon.afterdeath_days, (int)sim->goblins.tribute_phase);
    printf(",\"members\":%d,\"devotion\":%d,\"cohesion\":%d,\"coins\":%" PRId64
           ",\"relics\":%d,\"food_rations\":%d,\"tools\":%d,\"weapons\":%d,\"planned_eggs\":%d,\"existing_eggs\":%d}",
        sim->goblins.members, sim->goblins.devotion, sim->goblins.cohesion, sim->goblins.lair_coins,
        plan.relics, plan.food_rations, sim->goblins.lair_stock[CC_GOOD_TOOLS],
        sim->goblins.lair_stock[CC_GOOD_WEAPONS], plan.eggs, sim->dragon.egg_count);
}

static void JsonRetainedHistory(const CcSim *sim)
{
    int32_t earliest = -1, latest = -1;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) continue;
        if (earliest < 0 || event->day < earliest) earliest = event->day;
        if (latest < 0 || event->day > latest) latest = event->day;
    }
    printf("{\"semantics\":\"retained_records_snapshot\",\"retained_count\":%d,\"capacity\":%d,\"lifetime_event_count\":null,\"earliest_retained_day\":",
        sim->event_count, CC_MAX_EVENTS);
    if (earliest < 0) printf("null"); else printf("%d", earliest);
    printf(",\"latest_retained_day\":");
    if (latest < 0) printf("null"); else printf("%d", latest);
    putchar('}');
}

static void PrintProductionJson(const CcSim *sim, int32_t start_day, int32_t year,
    const char *fixture, const char *dragon_policy, const CcNutritionAccounting *nutrition,
    const CcSmithyAccounting *smithy, const CcRoadProductionAccounting *sites,
    const RoadObservation *roads)
{
    CcHungerSnapshot hunger = CcSimHungerSnapshot(sim);
    printf("{\"protocol\":6,\"fixture\":"); JsonString(fixture);
    printf(",\"dragon_policy\":"); JsonString(dragon_policy);
    printf(",\"comparison_scope\":\"whole-policy\",\"dragon\":{\"id\":\"%" PRIu64 "\",\"slain\":%s,\"slain_day\":%d,\"life_stage\":%d,\"activity\":%d,\"body_condition\":%d,\"crown_strength\":%d,\"eggs\":%d}",
        sim->dragon.id, sim->dragon.slain ? "true" : "false", sim->dragon.slain_day,
        (int)sim->dragon.life_stage, (int)sim->dragon.activity, sim->dragon.body_condition,
        sim->dragon.crown_strength, sim->dragon.egg_count);
    printf(",\"archive_work\":"); JsonArchiveWork(sim);
    printf(",\"archive_funding\":"); JsonArchiveFunding(sim);
    printf(",\"road_network\":"); JsonRoadNetwork(sim);
    printf(",\"threats\":"); JsonThreatSnapshot(sim);
    printf(",\"retained_history\":"); JsonRetainedHistory(sim);
    printf(",\"campaign_launch\":"); JsonCampaignPlan(sim);
    printf(",\"ritual_offering\":"); JsonRitualPlan(sim);
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
        printf(",\"maintenance_work\":%" PRIu64 ",\"site_repair\":%" PRIu64 ",\"wear\":%" PRIu64 "}", row->maintenance_work, row->site_repair, row->wear);
    }
    printf("],\"routes\":[");
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        printf("%s{\"id\":\"%" PRIu64 "\",\"condition\":%d,\"closed\":%s,\"open_days\":%d,\"closed_days\":%d,\"closure_reason\":null",
            i ? "," : "", route->id, route->condition, route->closed ? "true" : "false",
            roads[i].open_days, sim->current_day - start_day - roads[i].open_days);
        printf(",\"from_id\":\"%" PRIu64 "\",\"to_id\":\"%" PRIu64 "\",\"travel_days\":%d,\"capacity\":%d",
            route->from_id, route->to_id, route->travel_days, route->capacity);
        printf(",\"context\":"); JsonRoadContext(sim, route);
        printf(",\"observation\":{\"semantics\":\"run_interval\",\"start_day_exclusive\":%d,\"end_day_inclusive\":%d,\"sampling\":\"after_daily_advance\"}",
            start_day, sim->current_day);
        printf(",\"outage\":{\"semantics\":\"run_interval_daily_samples\",\"current_closed_days\":%d,\"longest_closed_days\":%d,\"closed_inhabited_days\":%d}",
            roads[i].current_closed_days, roads[i].longest_closed_days, roads[i].closed_inhabited_days);
        printf(",\"recovery\":"); JsonRoadRecovery(sim, route->id); putchar('}');
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
