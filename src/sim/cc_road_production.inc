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

static CcProductionReceipt PlanSiteMaintenance(const CcSim *sim, const CcRoadSite *site);

CcProductionReceipt CcSimPlanRoadSite(const CcSim *sim, const CcRoadSite *site)
{
    CcProductionRecipe recipe;
    if (sim == NULL || !CcRoadSiteRecipe(site, &recipe))
        return (CcProductionReceipt){.gate = CC_PRODUCTION_CLOSED, .blocked_good = CC_GOOD_COUNT};
    if (PlanSiteMaintenance(sim, site).gate == CC_PRODUCTION_READY)
        return (CcProductionReceipt){.gate = CC_PRODUCTION_WORK, .blocked_good = CC_GOOD_COUNT,
            .producer_id = site->id, .storage_id = site->id, .location_id = site->id};
    int32_t stock[CC_GOOD_COUNT];
    memcpy(stock, site->stock, sizeof(stock));
    CcProductionContext context = RoadProductionContext(sim, site, stock, &recipe);
    return CcProductionPlan(&recipe, &context);
}

static CcProductionContext SiteMaintenanceContext(const CcSim *sim,
    const CcRoadSite *site, int32_t *stock)
{
    return (CcProductionContext){.producer_id = site->id, .storage_id = site->id,
        .location_id = site->id, .stock = stock, .capacity = 1,
        .output_limit = 1, .work_available = 2, .condition = site->condition,
        .enabled = SiteNeedsMaintenance(sim, site)};
}

static CcProductionReceipt PlanSiteMaintenance(const CcSim *sim, const CcRoadSite *site)
{
    CcProductionRecipe production;
    if (sim == NULL || site == NULL || !CcRoadSiteRecipe(site, &production))
        return (CcProductionReceipt){.gate = CC_PRODUCTION_CLOSED, .blocked_good = CC_GOOD_COUNT};
    int32_t stock[CC_GOOD_COUNT];
    memcpy(stock, site->stock, sizeof(stock));
    CcProductionContext context = SiteMaintenanceContext(sim, site, stock);
    CcProductionRecipe repair = RoadSiteRepairRecipe;
    repair.inputs[1].reserve = SiteMaintenanceWoodTarget(site) - 1;
    return CcProductionPlan(&repair, &context);
}

CcProductionReceipt CcSimPlanRoadSiteMaintenance(const CcSim *sim, CcId site_id)
{
    return PlanSiteMaintenance(sim, sim != NULL ? CcSimRoadSite(sim, site_id) : NULL);
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
        CcSiteProductionAccounting *row = accounting != NULL ? &accounting->sites[i] : NULL;
        if (SiteNeedsMaintenance(sim, site)) {
            CcProductionContext upkeep = SiteMaintenanceContext(sim, site, site->stock);
            CcProductionRecipe repair_recipe = RoadSiteRepairRecipe;
            repair_recipe.inputs[1].reserve = SiteMaintenanceWoodTarget(site) - 1;
            CcProductionReceipt maintenance = CcProductionRun(&repair_recipe, &upkeep);
            if (maintenance.gate == CC_PRODUCTION_READY) {
                int32_t gain = MinimumI32(10, 100 - site->condition);
                site->condition += gain;
                if (row != NULL) {
                    row->site_id = site->id;
                    row->maintenance_work += (uint64_t)maintenance.work;
                    row->site_repair += (uint64_t)gain;
                    for (int32_t input = 0; input < RoadSiteRepairRecipe.input_count; ++input)
                        row->maintenance_input[RoadSiteRepairRecipe.inputs[input].good] += (uint64_t)maintenance.inputs[input];
                    row->gates[CC_PRODUCTION_WORK]++;
                }
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                    "%.40s uses one Tool and one Wood from its store for repairs; condition rises to %d. The crew spends this week's work on maintenance.",
                    site->name, site->condition);
                (void)PushEvent(sim, CC_EVENT_ROAD_SITE_PRODUCTION, site->id, site->id, 0, gain, text);
                continue;
            }
        }
        CcProductionContext context = RoadProductionContext(sim, site, site->stock, &recipe);
        CcProductionReceipt receipt = CcProductionRun(&recipe, &context);
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
