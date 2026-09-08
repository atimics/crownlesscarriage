static int32_t SiteFreightRoom(const CcRoadSite *site, CcGood good)
{
    int32_t other_slots = 0;
    for (int32_t i = 0; i < CC_GOOD_COUNT; ++i) {
        if (i == (int32_t)good) continue;
        int32_t units = CcGoodDefinitionFor((CcGood)i)->player_units_per_slot;
        other_slots += (site->stock[i] + units - 1) / units;
    }
    return MaximumI32(0, (CC_ROAD_SITE_CAPACITY - other_slots) *
        CcGoodDefinitionFor(good)->player_units_per_slot - site->stock[good]);
}

/* Reserve future room for loads already travelling or waiting to unload.
   Actual unloading still checks the physical store at arrival. */
static int32_t SiteSupplyRoom(const CcSim *sim, const CcRoadSite *site, CcGood good)
{
    if (sim->schema_version < 70U) return SiteFreightRoom(site, good);
    CcRoadSite projected = *site;
    for (int32_t item = 0; item < CC_GOOD_COUNT; ++item) {
        int32_t incoming = CcSimIncomingGood(sim, site->id, (CcGood)item);
        projected.stock[item] += MinimumI32(incoming, CC_SIM_MAX_UNITS - projected.stock[item]);
    }
    return SiteFreightRoom(&projected, good);
}

static bool PlanSiteSupply(const CcSim *sim, const CcRoadSite *site,
    const CcSettlement *town, CcGood good, int32_t target, int32_t slots,
    CcSiteFreightPlan *plan)
{
    int32_t incoming = sim->schema_version >= 70U ? CcSimIncomingGood(sim, site->id, good) : 0;
    int32_t need = target - site->stock[good] - incoming;
    if (need <= 0) return false;
    int32_t floor = town->reserve_target[good];
    if (good == CC_GOOD_FOOD || good == CC_GOOD_WHEAT)
        floor = MaximumI32(floor, CcEconomyWeeklyFoodUse(sim, town) * 6);
    int32_t quantity = MinimumI32(need, MinimumI32(town->stock[good] - floor,
        MinimumI32(SiteSupplyRoom(sim, site, good), slots * CcGoodsFreightUnitsPerCargoSlot(good))));
    if (quantity <= 0) return false;
    plan->kind = CC_SITE_FREIGHT_SUPPLY; plan->gate = CC_SITE_FREIGHT_READY;
    plan->good = good; plan->quantity = quantity;
    return true;
}

CcSiteFreightPlan CcSimPlanSiteFreight(const CcSim *sim, CcId carriage_id, CcId site_id)
{
    CcSiteFreightPlan plan = {.gate = CC_SITE_FREIGHT_CARRIAGE_REQUIRED, .good = CC_GOOD_COUNT};
    if (sim == NULL) return plan;
    const CcRoyalCarriage *carriage = NULL;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i)
        if (sim->royal_carriages[i].id == carriage_id) carriage = &sim->royal_carriages[i];
    if (carriage == NULL || carriage->mode != CC_ROYAL_CARRIAGE_IDLE ||
        carriage->active_shipment_id != 0 || carriage->condition < 20 ||
        sim->current_day < carriage->next_dispatch_day) return plan;
    plan.carriage_id = carriage_id;
    const CcRoadSite *site = CcSimRoadSite(sim, site_id);
    CcProductionRecipe recipe;
    plan.gate = CC_SITE_FREIGHT_SITE_REQUIRED;
    if (site == NULL || !site->accessible || (site->condition < 50 && sim->schema_version < 68U) ||
        !CcRoadSiteRecipe(site, &recipe)) return plan;
    plan.site_id = site_id; plan.route_id = site->route_id;
    const CcSettlement *town = CcSimSettlement(sim, carriage->location_id);
    plan.gate = CC_SITE_FREIGHT_LOCAL_TOWN_REQUIRED;
    if (town == NULL || CcSettlementIsAbandoned(town) ||
        town->kingdom_id != carriage->kingdom_id || site->home_settlement_id != town->id) return plan;
    plan.town_id = town->id;
    plan.gate = CC_SITE_FREIGHT_ROUTE_REQUIRED;
    const CcRoute *route = CcSimRoute(sim, site->route_id);
    if (route == NULL || route->closed) return plan;
    CcFreightLeg leg;
    if (!CcSimFreightLeg(sim, site->route_id, town->id, site_id, &leg) ||
        !CcSimRoyalCarriageCanUseRoute(sim, carriage->kingdom_id, site->route_id)) return plan;
    plan.travel_days = leg.travel_days; plan.return_days = leg.travel_days;
    int32_t route_slot = (int32_t)(route - sim->routes);
    int32_t used = sim->royal_trade_week == sim->current_day / 7 ? sim->royal_route_slots_used[route_slot] : 0;
    int32_t slots = MinimumI32(CC_ROYAL_CARRIAGE_CARGO_SLOTS,
        MinimumI32(route->capacity, CcTradeRouteCapacity(sim, route)) - used);
    plan.gate = CC_SITE_FREIGHT_CAPACITY_REQUIRED;
    if (slots < 1) return plan;
    plan.gate = CC_SITE_FREIGHT_GOODS_REQUIRED;
    bool maintenance = SiteNeedsMaintenance(sim, site);
    if (maintenance &&
        (PlanSiteSupply(sim, site, town, CC_GOOD_TOOLS, 2, slots, &plan) ||
         PlanSiteSupply(sim, site, town, CC_GOOD_WOOD, SiteMaintenanceWoodTarget(site), slots, &plan))) return plan;
    /* Collect finished stock first. Preserve the site's working Tool. */
    if (!recipe.work_only) {
        CcGood good = recipe.output;
        int32_t floor = SiteOutputFloor(sim, site, good);
        int32_t quantity = MinimumI32(site->stock[good] - floor,
            MinimumI32(CC_SIM_MAX_UNITS - town->stock[good], slots * CcGoodsFreightUnitsPerCargoSlot(good)));
        if (quantity > 0) {
            plan.kind = CC_SITE_FREIGHT_PICKUP; plan.gate = CC_SITE_FREIGHT_READY;
            plan.good = good; plan.quantity = quantity; return plan;
        }
    }
    if (maintenance) return plan;
    int32_t target[CC_GOOD_COUNT] = {0};
    target[CC_GOOD_TOOLS] = recipe.tools_required;
    for (int32_t i = 0; i < recipe.input_count; ++i)
        target[recipe.inputs[i].good] = MaximumI32(target[recipe.inputs[i].good],
            recipe.inputs[i].reserve + recipe.inputs[i].units * 2);
    /* Tools first, then declared inputs in recipe order. */
    for (int32_t i = -1; i < recipe.input_count; ++i) {
        CcGood good = i < 0 ? CC_GOOD_TOOLS : recipe.inputs[i].good;
        if (PlanSiteSupply(sim, site, town, good, target[good], slots, &plan)) return plan;
    }
    return plan;
}
