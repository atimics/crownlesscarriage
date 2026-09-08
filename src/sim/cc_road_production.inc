bool CcRoadSiteRecipe(const CcRoadSite *site, CcProductionRecipe *recipe)
{
    if (site == NULL || recipe == NULL || site->kind == CC_ROAD_SITE_ROAD_HOUSE) return false;
    *recipe = (CcProductionRecipe){.output = site->output_good, .output_units = 1,
        .input_count = 1, .inputs = {{site->input_good, 1, 1}},
        .work_per_batch = 1, .minimum_condition = 50, .tools_required = 1,
        .hunger_soft_limit = 100, .hunger_hard_limit = 100};
    switch (site->kind) {
        case CC_ROAD_SITE_FARM:
        case CC_ROAD_SITE_WOODLOT:
        case CC_ROAD_SITE_QUARRY: recipe->output_units = 4; break;
        case CC_ROAD_SITE_MINE: recipe->output_units = 2; break;
        case CC_ROAD_SITE_MILL:
        case CC_ROAD_SITE_BAKERY: recipe->inputs[0].reserve = 2; break;
        case CC_ROAD_SITE_PASTURE:
            recipe->inputs[0].units = 2; recipe->inputs[0].reserve = 2; break;
        case CC_ROAD_SITE_SMITHY:
            recipe->inputs[0].units = 2;
            recipe->input_count = 2;
            recipe->inputs[1] = (CcRecipeInput){CC_GOOD_WOOD, 1, 1};
            break;
        case CC_ROAD_SITE_ROAD_CREW:
            recipe->work_only = true;
            recipe->inputs[0].units = 2;
            recipe->work_per_batch = 3;
            break;
        case CC_ROAD_SITE_ROAD_HOUSE:
        case CC_ROAD_SITE_KIND_COUNT: return false;
    }
    return true;
}

static CcProductionContext RoadProductionContext(const CcSim *sim,
    const CcRoadSite *site, int32_t *stock, const CcProductionRecipe *recipe)
{
    int32_t limit = 0;
    if (recipe->work_only) {
        const CcRoute *route = CcSimRoute(sim, site->route_id);
        if (route != NULL) limit = (100 - route->condition + 2) / 3;
    } else {
        int32_t other_slots = 0;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (good == (int32_t)recipe->output) continue;
            int32_t units = CcGoodDefinitionFor((CcGood)good)->player_units_per_slot;
            other_slots += (stock[good] + units - 1) / units;
        }
        limit = MaximumI32(0, CC_ROAD_SITE_CAPACITY - other_slots) *
            CcGoodDefinitionFor(recipe->output)->player_units_per_slot;
    }
    return (CcProductionContext){.producer_id = site->id, .storage_id = site->id,
        .location_id = site->id, .stock = stock, .capacity = 2, .output_limit = limit,
        .work_available = recipe->work_only ? 6 : 2, .condition = site->condition,
        .enabled = sim->schema_version >= 64U && site->accessible};
}

CcProductionReceipt CcSimPlanRoadSite(const CcSim *sim, const CcRoadSite *site)
{
    CcProductionRecipe recipe;
    if (sim == NULL || !CcRoadSiteRecipe(site, &recipe))
        return (CcProductionReceipt){.gate = CC_PRODUCTION_CLOSED, .blocked_good = CC_GOOD_COUNT};
    int32_t stock[CC_GOOD_COUNT];
    memcpy(stock, site->stock, sizeof(stock));
    CcProductionContext context = RoadProductionContext(sim, site, stock, &recipe);
    return CcProductionPlan(&recipe, &context);
}

static void ProduceRoadSites(CcSim *sim, CcRoadProductionAccounting *accounting)
{
    if (sim->schema_version < 64U) return;
    int32_t active = 0, output = 0, repair = 0;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        CcRoadSite *site = &sim->road_sites[i];
        if (!site->accessible && accounting == NULL) continue;
        CcProductionRecipe recipe;
        if (!CcRoadSiteRecipe(site, &recipe)) continue;
        CcProductionContext context = RoadProductionContext(sim, site, site->stock, &recipe);
        CcProductionReceipt receipt = CcProductionRun(&recipe, &context);
        CcSiteProductionAccounting *row = accounting != NULL ? &accounting->sites[i] : NULL;
        if (row != NULL) {
            row->site_id = site->id;
            row->gates[receipt.gate]++;
            row->work += (uint64_t)receipt.work;
            for (int32_t input = 0; input < recipe.input_count; ++input)
                row->input[recipe.inputs[input].good] += (uint64_t)receipt.inputs[input];
            if (!recipe.work_only) row->output[recipe.output] += (uint64_t)receipt.output;
        }
        if (receipt.gate != CC_PRODUCTION_READY) continue;
        active++;
        output += receipt.output;
        if (recipe.work_only) {
            for (int32_t route = 0; route < sim->route_count; ++route) {
                if (sim->routes[route].id != site->route_id) continue;
                int32_t gain = MinimumI32(100 - sim->routes[route].condition, receipt.work);
                sim->routes[route].condition += gain;
                repair += gain;
                if (row != NULL) row->route_repair += (uint64_t)gain;
                break;
            }
        }
    }
    if (active > 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
            "%d roadside works store %d goods bundles locally and restore %d road condition this week.",
            active, output, repair);
        (void)PushEvent(sim, CC_EVENT_ROAD_SITE_PRODUCTION, 0, 0, 0, output + repair, text);
    }
}
