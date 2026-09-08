/* A named organiser books ordinary royal freight from a separate grain purse. */
const CcGrainSupply *CcSimGrainSupply(const CcSim *sim, CcId settlement_id)
{
    if (sim == NULL) return NULL;
    for (int i = 0; i < sim->settlement_count; ++i)
        if (sim->settlements[i].id == settlement_id) return &sim->grain_supplies[i];
    return NULL;
}

static CcGrainDeliveryPlan PlanGrainForCarriage(const CcSim *sim, const CcSettlement *town,
                                               const CcRoyalCarriage *carriage)
{
    CcGrainDeliveryPlan plan = {0};
    const CcGrainSupply *supply = CcSimGrainSupply(sim, town->id);
    plan.organiser_id = supply->organiser_id;
    const CcCharacter *organiser = CcSimCharacter(sim, supply->organiser_id);
#define GRAIN_WAIT(state, text) do { plan.status = state; SetError(plan.reason, sizeof(plan.reason), text); return plan; } while (0)
    if (supply->shipment_id != 0U) {
        for (int i = 0; i < sim->shipment_count; ++i) {
            const CcShipment *shipment = &sim->shipments[i];
            if (shipment->id != supply->shipment_id) continue;
            plan.route_id = shipment->route_id;
            plan.supplier_id = supply->supplier_id;
            if (shipment->status == CC_SHIPMENT_BLOCKED ||
                !CcSimRoyalCarriageCanUseRoute(sim, town->kingdom_id, shipment->route_id))
                GRAIN_WAIT(CC_GRAIN_BLOCKED, "The grain carriage is waiting for a usable road.");
            GRAIN_WAIT(CC_GRAIN_TRANSIT, "A paid grain load is on its way.");
        }
    }
    if (!supply->enabled) GRAIN_WAIT(CC_GRAIN_INACTIVE, "Fund an organiser to arrange continuing grain deliveries.");
    if (organiser == NULL || organiser->death_day <= sim->current_day ||
        organiser->current_settlement_id != town->id)
        GRAIN_WAIT(CC_GRAIN_CONTACT, "The town needs its organiser here before buying more grain.");
    if (supply->ordered > INT32_MAX - 12) GRAIN_WAIT(CC_GRAIN_FUNDS, "This delivery record is complete. End its orders and collect the remaining fund.");
    if (CcSettlementIsAbandoned(town) || !CcSettlementHasService(town, CC_SERVICE_BAKERY))
        GRAIN_WAIT(CC_GRAIN_BAKERY, "Reopen the bakery before ordering its next load.");
    if (town->stock[CC_GOOD_WHEAT] + CcSimIncomingGood(sim, town->id, CC_GOOD_WHEAT) >= 12)
        GRAIN_WAIT(CC_GRAIN_STOCKED, "The bakery has grain for its next batch.");
    if (carriage == NULL || carriage->mode != CC_ROYAL_CARRIAGE_IDLE ||
        carriage->active_shipment_id != 0U || carriage->condition < 20 ||
        sim->current_day < carriage->next_dispatch_day)
        GRAIN_WAIT(CC_GRAIN_CARRIAGE, "The organiser is waiting for the kingdom's carriage.");
    plan.carriage_id = carriage->id;
    bool surplus_found = false, path_found = false;
    int64_t best_score = INT64_MAX;
    for (int i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *from = &sim->settlements[i];
        if (from->id == town->id || CcSettlementIsAbandoned(from) ||
            TradeSurplus(sim, from, town, CC_GOOD_WHEAT) < 4) continue;
        surplus_found = true;
        int route_slot = -1, path_cost = 0, capacity = 0, reposition_cost = 0;
        CcId hop = 0;
        if (!CcTradeFindPath(sim, from->id, town->id, CC_GOOD_WHEAT,
            &route_slot, &hop, &path_cost, &capacity, sim->royal_route_slots_used,
            true, carriage->kingdom_id, false, 1)) continue;
        if (carriage->location_id != from->id && !CcTradeFindPath(sim, carriage->location_id,
            from->id, CC_GOOD_WHEAT, NULL, NULL, &reposition_cost, NULL, NULL,
            true, carriage->kingdom_id, false, 1)) continue;
        path_found = true;
        CcMoney toll = RoyalTradeRouteToll(sim, &sim->routes[route_slot], carriage->kingdom_id);
        CcMoney price = MaximumI32(1, from->price[CC_GOOD_WHEAT]);
        if (supply->purse < price * 4 + toll) continue;
        int64_t score = price * 12 + toll + path_cost + reposition_cost;
        if (score < best_score) {
            best_score = score;
            plan.supplier_id = from->id;
            plan.route_id = sim->routes[route_slot].id;
            plan.next_hop_id = hop;
            plan.path_capacity = capacity;
        }
    }
    if (!surplus_found) GRAIN_WAIT(CC_GRAIN_SUPPLIER, "The organiser is seeking a town with spare grain.");
    if (!path_found) GRAIN_WAIT(CC_GRAIN_ROUTE, "Grain is available. The organiser needs an open supply road.");
    if (plan.supplier_id == 0U) GRAIN_WAIT(CC_GRAIN_FUNDS, "The grain fund needs more crowns for the next load.");
    plan.status = CC_GRAIN_READY;
    SetError(plan.reason, sizeof(plan.reason), "The organiser has found a supplier and a usable route.");
#undef GRAIN_WAIT
    return plan;
}

CcGrainDeliveryPlan CcSimGrainDeliveryPlan(const CcSim *sim, CcId settlement_id)
{
    const CcSettlement *town = CcSimSettlement(sim, settlement_id);
    if (town == NULL || sim->schema_version < 73U) {
        CcGrainDeliveryPlan plan = {.status = CC_GRAIN_INACTIVE};
        SetError(plan.reason, sizeof(plan.reason), "Choose a town in a current company save.");
        return plan;
    }
    const CcRoyalCarriage *carriage = NULL;
    for (int i = 0; i < sim->royal_carriage_count; ++i) {
        const CcRoyalCarriage *candidate = &sim->royal_carriages[i];
        if (candidate->kingdom_id == town->kingdom_id && !IsSiteCarriage(candidate)) {
            carriage = candidate;
            break;
        }
    }
    return PlanGrainForCarriage(sim, town, carriage);
}

static bool DispatchGrainSupply(CcSim *sim, CcRoyalCarriage *carriage,
                                int32_t route_used[CC_MAX_ROUTES])
{
    if (IsSiteCarriage(carriage)) return false;
    for (int i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *town = &sim->settlements[i];
        if (town->kingdom_id != carriage->kingdom_id) continue;
        CcGrainSupply *supply = &sim->grain_supplies[i];
        CcGrainDeliveryPlan plan = PlanGrainForCarriage(sim, town, carriage);
        if (plan.status != CC_GRAIN_READY) continue;
        if (carriage->location_id != plan.supplier_id) {
            supply->supplier_id = plan.supplier_id;
            supply->route_id = plan.route_id;
            return StartRoyalRepositioningLeg(sim, carriage, plan.supplier_id);
        }
        for (int route = 0; route < sim->route_count; ++route) {
            if (sim->routes[route].id != plan.route_id) continue;
            return CreateTradeShipment(sim, carriage, route, plan.next_hop_id, CC_GOOD_WHEAT,
                CcSimSettlementMutable(sim, plan.supplier_id), town, plan.path_capacity, 1,
                route_used, supply);
        }
    }
    return false;
}
