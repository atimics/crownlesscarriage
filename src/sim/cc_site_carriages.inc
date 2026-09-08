/* Site service uses the kingdom's ordinary carriage and shipment pool. */
static CcShipment *SiteCarriageShipment(CcSim *sim, const CcRoyalCarriage *carriage)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i)
        if (sim->shipments[i].id == carriage->active_shipment_id) return &sim->shipments[i];
    return NULL;
}

static void SiteFreightEvent(CcSim *sim, CcEventKind type, CcRoyalCarriage *carriage,
                             CcShipment *shipment, CcId place, int32_t quantity,
                             const char *verb)
{
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "A royal carriage %s %d %s at the site road stop.",
        verb, quantity, shipment != NULL ? CcGoodName(shipment->good) : "cargo");
    (void)PushEvent(sim, type, shipment != NULL ? shipment->id : carriage->id,
        place, 0U, quantity, text);
}

static int32_t SiteRouteSlots(const CcSim *sim, const CcRoyalCarriage *carriage)
{
    const CcRoute *route = CcSimRoute(sim, carriage->route_id);
    if (route == NULL || route->closed ||
        !CcSimRoyalCarriageCanUseRoute(sim, carriage->kingdom_id, route->id)) return 0;
    int32_t slot = (int32_t)(route - sim->routes);
    return MaximumI32(0, MinimumI32(CC_ROYAL_CARRIAGE_CARGO_SLOTS,
        MinimumI32(route->capacity, TradeRouteCapacity(sim, route)) - sim->royal_route_slots_used[slot]));
}

static void StartSiteLeg(CcSim *sim, CcRoyalCarriage *carriage, CcShipment *shipment)
{
    carriage->mode = CC_ROYAL_CARRIAGE_SITE_TRAVELLING;
    carriage->departure_day = sim->current_day;
    carriage->arrival_day = sim->current_day + CcSimFreightLegDays(sim,
        carriage->route_id, carriage->location_id, carriage->destination_id);
    carriage->blocked_since_day = 0;
    int32_t slots = 1;
    if (shipment != NULL) {
        shipment->origin_id = carriage->location_id;
        shipment->destination_id = carriage->destination_id;
        shipment->final_destination_id = carriage->destination_id;
        shipment->route_id = carriage->route_id;
        shipment->departure_day = carriage->departure_day;
        shipment->arrival_day = carriage->arrival_day;
        shipment->status = CC_SHIPMENT_TRAVELLING;
        slots = CcGoodsFreightCargoSlots(shipment->good, shipment->quantity);
    }
    const CcRoute *route = CcSimRoute(sim, carriage->route_id);
    UseRoyalRoute(sim, (int32_t)(route - sim->routes), slots);
    SiteFreightEvent(sim, CC_EVENT_SHIPMENT_DEPARTED, carriage, shipment,
        carriage->location_id, shipment != NULL ? shipment->quantity : 0, "loads");
}

static bool DispatchSiteCarriage(CcSim *sim, CcRoyalCarriage *carriage, CcRoadProductionAccounting *accounting)
{
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        CcSiteFreightPlan plan = CcSimPlanSiteFreight(sim, carriage->id, sim->road_sites[i].id);
        if (plan.gate != CC_SITE_FREIGHT_READY) continue;
        CcShipment *shipment = NULL;
        if (plan.kind == CC_SITE_FREIGHT_SUPPLY) {
            shipment = AllocateShipment(sim);
            if (shipment == NULL) return false;
            shipment->id = NextId(sim, CC_ENTITY_SHIPMENT);
            shipment->good = plan.good; shipment->quantity = plan.quantity;
            CcSimSettlementMutable(sim, plan.town_id)->stock[plan.good] -= plan.quantity;
            if (accounting != NULL) accounting->sites[i].freight_sent[plan.good] += (uint64_t)plan.quantity;
        }
        carriage->route_id = plan.route_id;
        carriage->destination_id = plan.site_id;
        carriage->target_id = plan.town_id;
        carriage->active_shipment_id = shipment != NULL ? shipment->id : 0U;
        carriage->next_dispatch_day = sim->current_day + 7;
        StartSiteLeg(sim, carriage, shipment);
        return true;
    }
    return false;
}

static void FinishSiteStop(CcSim *sim, CcRoyalCarriage *carriage)
{
    carriage->active_shipment_id = 0;
    if (CcSimRoadSite(sim, carriage->location_id) != NULL) {
        carriage->mode = CC_ROYAL_CARRIAGE_SITE_WAITING;
        carriage->destination_id = carriage->target_id;
        carriage->arrival_day = 0;
        carriage->blocked_since_day = sim->current_day;
    } else {
        carriage->trips_completed = MinimumI32(CC_SIM_MAX_UNITS, carriage->trips_completed + 1);
        ParkRoyalCarriage(carriage, carriage->location_id);
        carriage->departure_day = sim->current_day;
        carriage->next_dispatch_day = sim->current_day + 7;
    }
}

static void UnloadSiteCarriage(CcSim *sim, CcRoyalCarriage *carriage, CcRoadProductionAccounting *accounting)
{
    CcShipment *shipment = SiteCarriageShipment(sim, carriage);
    if (shipment == NULL) { FinishSiteStop(sim, carriage); return; }
    CcRoadSite *site = NULL;
    for (int32_t i = 0; i < sim->road_site_count; ++i)
        if (sim->road_sites[i].id == carriage->location_id) site = &sim->road_sites[i];
    CcSettlement *town = CcSimSettlementMutable(sim, carriage->location_id);
    if (site == NULL && town == NULL) return;
    int32_t *stock = site != NULL ? site->stock : town->stock;
    int32_t room = site != NULL ? (site->accessible ? SiteFreightRoom(site, shipment->good) : 0) :
        CC_SIM_MAX_UNITS - stock[shipment->good];
    int32_t quantity = MinimumI32(room, shipment->quantity);
    if (quantity <= 0) return;
    stock[shipment->good] += quantity;
    if (site != NULL && accounting != NULL)
        accounting->sites[site - sim->road_sites].freight_received[shipment->good] += (uint64_t)quantity;
    if (site == NULL && accounting != NULL) {
        const CcRoadSite *source = CcSimRoadSite(sim, shipment->origin_id);
        if (source != NULL)
            accounting->sites[source - sim->road_sites].freight_delivered[shipment->good] += (uint64_t)quantity;
    }
    SiteFreightEvent(sim, CC_EVENT_SHIPMENT_ARRIVED, carriage, shipment,
        carriage->location_id, quantity, "unloads");
    if (quantity == shipment->quantity) {
        shipment->status = CC_SHIPMENT_ARRIVED;
        FinishSiteStop(sim, carriage);
    } else shipment->quantity -= quantity;
}

static void AdvanceSiteCarriages(CcSim *sim, CcRoadProductionAccounting *accounting)
{
    if (sim->schema_version < 66U) return;
    PrepareRoyalRouteUsage(sim);
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        CcRoyalCarriage *carriage = &sim->royal_carriages[i];
        if (!IsSiteCarriage(carriage)) continue;
        CcShipment *shipment = SiteCarriageShipment(sim, carriage);
        if (carriage->mode == CC_ROYAL_CARRIAGE_SITE_UNLOADING) {
            UnloadSiteCarriage(sim, carriage, accounting);
            continue;
        }
        if (carriage->mode == CC_ROYAL_CARRIAGE_SITE_WAITING) {
            int32_t slots = SiteRouteSlots(sim, carriage);
            if (slots < (shipment != NULL ? CcGoodsFreightCargoSlots(shipment->good, shipment->quantity) : 1)) continue;
            const CcRoadSite *site = CcSimRoadSite(sim, carriage->location_id);
            CcProductionRecipe recipe;
            if (shipment == NULL && site != NULL && site->accessible && CcRoadSiteRecipe(site, &recipe) &&
                !recipe.work_only) {
                CcGood good = recipe.output;
                int32_t floor = SiteOutputFloor(sim, site, good);
                const CcSettlement *town = CcSimSettlement(sim, carriage->destination_id);
                int32_t quantity = MinimumI32(site->stock[good] - floor,
                    MinimumI32(CC_SIM_MAX_UNITS - town->stock[good], slots * CcGoodsFreightUnitsPerCargoSlot(good)));
                if (quantity > 0) {
                    shipment = AllocateShipment(sim);
                    if (shipment == NULL) continue;
                    shipment->id = NextId(sim, CC_ENTITY_SHIPMENT);
                    shipment->good = good; shipment->quantity = quantity;
                    sim->road_sites[site - sim->road_sites].stock[good] -= quantity;
                    if (accounting != NULL)
                        accounting->sites[site - sim->road_sites].freight_shipped[good] += (uint64_t)quantity;
                    carriage->active_shipment_id = shipment->id;
                }
            }
            StartSiteLeg(sim, carriage, shipment);
            continue;
        }
        if (carriage->arrival_day > sim->current_day) continue;
        const CcRoute *route = CcSimRoute(sim, carriage->route_id);
        if (route->closed || !CcSimRoyalCarriageCanUseRoute(sim, carriage->kingdom_id, route->id)) {
            carriage->mode = CC_ROYAL_CARRIAGE_SITE_WAITING;
            carriage->arrival_day = 0; carriage->blocked_since_day = sim->current_day;
            if (shipment != NULL) { shipment->status = CC_SHIPMENT_BLOCKED; shipment->arrival_day = 0; }
            SiteFreightEvent(sim, CC_EVENT_ROYAL_CARRIAGE_BLOCKED, carriage, shipment,
                carriage->location_id, shipment != NULL ? shipment->quantity : 0, "holds");
            continue;
        }
        carriage->location_id = carriage->destination_id;
        carriage->condition = MaximumI32(0, carriage->condition - 1);
        carriage->mode = CC_ROYAL_CARRIAGE_SITE_UNLOADING;
        carriage->arrival_day = 0;
        if (shipment != NULL) {
            bool lost = (int32_t)(NextRandom(sim) % 100U) < CcSimRouteDanger(sim, route->id);
            shipment->status = lost ? CC_SHIPMENT_LOST : CC_SHIPMENT_BLOCKED;
            shipment->arrival_day = 0;
            if (lost) {
                const CcRoadSite *site = CcSimRoadSite(sim, shipment->origin_id);
                if (site == NULL) site = CcSimRoadSite(sim, shipment->destination_id);
                if (site != NULL && accounting != NULL)
                    accounting->sites[site - sim->road_sites].freight_lost[shipment->good] += (uint64_t)shipment->quantity;
                SiteFreightEvent(sim, CC_EVENT_SHIPMENT_LOST, carriage, shipment,
                    carriage->location_id, shipment->quantity, "loses");
                carriage->cargo_losses = MinimumI32(CC_SIM_MAX_UNITS, carriage->cargo_losses + 1);
                carriage->condition = MaximumI32(0, carriage->condition - 11);
                carriage->active_shipment_id = 0;
            }
        }
        UnloadSiteCarriage(sim, carriage, accounting);
    }
}

static bool ValidSiteCarriage(const CcSim *sim, const CcRoyalCarriage *carriage, int32_t index)
{
    if (sim->schema_version < 66U || CcIdKind(carriage->id) != CC_ENTITY_ROYAL_CARRIAGE ||
        carriage->kingdom_id != sim->kingdoms[index].id || carriage->condition < 0 ||
        carriage->condition > 100 || carriage->trips_completed < 0 ||
        carriage->trips_completed > CC_SIM_MAX_UNITS || carriage->cargo_losses < 0 ||
        carriage->cargo_losses > CC_SIM_MAX_UNITS || carriage->departure_day < 1 ||
        carriage->departure_day > sim->current_day || carriage->next_dispatch_day < 0 ||
        carriage->next_dispatch_day > sim->current_day + 7 ||
        CcSimSettlement(sim, carriage->target_id) == NULL) return false;
    const CcShipment *shipment = NULL;
    for (int32_t i = 0; i < sim->shipment_count; ++i)
        if (sim->shipments[i].id == carriage->active_shipment_id) shipment = &sim->shipments[i];
    if ((carriage->active_shipment_id != 0) != (shipment != NULL)) return false;
    if (shipment != NULL && (CcGoodsFreightCargoSlots(shipment->good, shipment->quantity) >
        CC_ROYAL_CARRIAGE_CARGO_SLOTS || shipment->route_id != carriage->route_id ||
        shipment->destination_id != carriage->destination_id ||
        shipment->final_destination_id != carriage->destination_id)) return false;
    bool unloading = carriage->mode == CC_ROYAL_CARRIAGE_SITE_UNLOADING;
    CcId origin = unloading && shipment != NULL ? shipment->origin_id : carriage->location_id;
    CcFreightLeg leg;
    if (!CcSimFreightLeg(sim, carriage->route_id, origin, carriage->destination_id, &leg)) return false;
    const CcRoadSite *site = CcSimRoadSite(sim, origin);
    if (site == NULL) site = CcSimRoadSite(sim, carriage->destination_id);
    if (site == NULL || site->home_settlement_id != carriage->target_id ||
        (origin != site->id && origin != carriage->target_id) ||
        (carriage->destination_id != site->id && carriage->destination_id != carriage->target_id)) return false;
    if (unloading) return shipment != NULL && shipment->status == CC_SHIPMENT_BLOCKED &&
        carriage->location_id == carriage->destination_id && carriage->arrival_day == 0 &&
        carriage->blocked_since_day == 0;
    if (shipment != NULL && shipment->origin_id != carriage->location_id) return false;
    if (carriage->mode == CC_ROYAL_CARRIAGE_SITE_WAITING) return carriage->arrival_day == 0 &&
        carriage->blocked_since_day >= 1 && carriage->blocked_since_day <= sim->current_day &&
        (shipment == NULL || shipment->status == CC_SHIPMENT_BLOCKED);
    return carriage->arrival_day > sim->current_day && carriage->blocked_since_day == 0 &&
        (int64_t)carriage->arrival_day == (int64_t)carriage->departure_day + leg.travel_days &&
        (shipment == NULL || (shipment->status == CC_SHIPMENT_TRAVELLING &&
        shipment->departure_day == carriage->departure_day && shipment->arrival_day == carriage->arrival_day));
}
