#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, restored, split;
static CcRoyalCarriage *carriage;
static CcRoadSite *site;
static CcSettlement *town;
static char error[256];

static void Validate(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        fprintf(stderr, "day %d mode %d: %s\n", sim.current_day, carriage->mode, error);
        CC_CHECK(false);
    }
}

static void Prepare(bool pickup)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    site = &sim.road_sites[11];
    town = CcSimSettlementMutable(&sim, site->home_settlement_id);
    for (int32_t i = 0; i < sim.road_site_count; ++i) sim.road_sites[i].condition = 49;
    site->condition = 100; site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
    site->stock[CC_GOOD_TOOLS] = pickup ? 5 : 0;
    carriage = NULL;
    for (int32_t i = 0; i < sim.royal_carriage_count; ++i)
        if (sim.royal_carriages[i].kingdom_id == town->kingdom_id) carriage = &sim.royal_carriages[i];
    CC_CHECK(carriage != NULL);
    carriage->location_id = town->id;
    town->stock[CC_GOOD_TOOLS] = 30;
    Validate();
}

static void Day(void)
{
    CcSimAdvanceDays(&sim, 1);
    Validate();
}

static void ReachDeparture(void)
{
    for (int32_t i = 0; i < 7 && carriage->mode != CC_ROYAL_CARRIAGE_SITE_TRAVELLING; ++i) Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING);
    CC_CHECK(carriage->location_id == town->id && carriage->destination_id == site->id);
}

static void RoundTrip(void)
{
    const char *path = "site-delivery.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    split = sim;
    CcSimAdvanceDays(&split, 14);
    for (int32_t i = 0; i < 14; ++i) CcSimAdvanceDays(&restored, 1);
    CC_CHECK(CcSimHash(&split) == CcSimHash(&restored));
    (void)remove(path);
    restored = sim;
    CcJournal *journal = CcJournalStart(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &restored, 14, error, sizeof(error)));
    CC_CHECK(CcJournalFlush(journal, &restored, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&restored) == CcSimHash(&split));
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);
}

static void CheckWaiting(void)
{
    Prepare(false);
    ReachDeparture();
    CcRoute *route = &sim.routes[CcSimRoute(&sim, site->route_id) - sim.routes];
    CcId shipment_id = carriage->active_shipment_id;
    int32_t stock = town->stock[CC_GOOD_TOOLS];
    route->closed = true;
    while (carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING) Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_WAITING);
    CC_CHECK(carriage->location_id == town->id && site->stock[CC_GOOD_TOOLS] == 0);
    RoundTrip();
    route->closed = false;
    Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING);
    CC_CHECK(carriage->active_shipment_id == shipment_id && town->stock[CC_GOOD_TOOLS] == stock);
    CC_CHECK(carriage->arrival_day == sim.current_day + CcSimFreightLegDays(&sim, route->id, town->id, site->id));
    site->stock[CC_GOOD_STONE] = CC_ROAD_SITE_CAPACITY;
    while (carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING) Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_UNLOADING);
    CC_CHECK(carriage->location_id == site->id && carriage->active_shipment_id == shipment_id);
    CC_CHECK(site->stock[CC_GOOD_TOOLS] == 0);
    RoundTrip();
    site->stock[CC_GOOD_STONE] -= 1;
    Day();
    CC_CHECK(site->stock[CC_GOOD_TOOLS] == 1 && carriage->active_shipment_id == 0);
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_WAITING);
    route->closed = true;
    Day();
    CC_CHECK(carriage->location_id == site->id && carriage->mode == CC_ROYAL_CARRIAGE_SITE_WAITING);
    route->closed = false;
    sim.royal_trade_week = sim.current_day / 7;
    sim.royal_route_slots_used[route - sim.routes] = route->capacity;
    Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_WAITING);
    sim.royal_route_slots_used[route - sim.routes] = 0;
    Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING && carriage->active_shipment_id == 0);
    RoundTrip();
}

static void CheckPartialAndLoss(void)
{
    Prepare(true);
    ReachDeparture();
    while (carriage->location_id == town->id) Day();
    Day();
    while (sim.current_day + 1 < carriage->arrival_day) Day();
    static CcSim arriving;
    arriving = sim;
    town->stock[CC_GOOD_TOOLS] = CC_SIM_MAX_UNITS - 2;
    Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_UNLOADING);
    CC_CHECK(town->stock[CC_GOOD_TOOLS] == CC_SIM_MAX_UNITS);
    CcShipment *cargo = NULL;
    for (int32_t i = 0; i < sim.shipment_count; ++i)
        if (sim.shipments[i].id == carriage->active_shipment_id) cargo = &sim.shipments[i];
    CC_CHECK(cargo != NULL && cargo->quantity == 2 && cargo->status == CC_SHIPMENT_BLOCKED);
    RoundTrip();
    town->stock[CC_GOOD_TOOLS] -= 2;
    Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_IDLE && town->stock[CC_GOOD_TOOLS] == CC_SIM_MAX_UNITS);
    bool saw_loss = false;
    for (uint32_t random = 1; random <= 128 && !saw_loss; ++random) {
        sim = arriving;
        sim.random_state = random;
        int32_t before = town->stock[CC_GOOD_TOOLS];
        CcRoadProductionAccounting accounting = {0};
        CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &accounting);
        Validate();
        if (carriage->cargo_losses == 0) continue;
        saw_loss = true;
        CC_CHECK(accounting.sites[11].freight_lost[CC_GOOD_TOOLS] == 4);
        CC_CHECK(town->stock[CC_GOOD_TOOLS] == before && site->stock[CC_GOOD_TOOLS] == 1);
        CC_CHECK(carriage->active_shipment_id == 0 && carriage->location_id == town->id);
        CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_IDLE);
    }
    CC_CHECK(saw_loss);
}

int main(void)
{
    Prepare(true);
    ReachDeparture();
    CC_CHECK(carriage->active_shipment_id == 0 && site->stock[CC_GOOD_TOOLS] == 5);
    RoundTrip();
    while (carriage->location_id == town->id) Day();
    CC_CHECK(carriage->location_id == site->id && site->stock[CC_GOOD_TOOLS] == 5);
    RoundTrip();
    Day();
    CC_CHECK(carriage->mode == CC_ROYAL_CARRIAGE_SITE_TRAVELLING);
    CC_CHECK(carriage->active_shipment_id != 0 && site->stock[CC_GOOD_TOOLS] == 1);
    RoundTrip();
    int32_t before = town->stock[CC_GOOD_TOOLS];
    while (carriage->mode != CC_ROYAL_CARRIAGE_IDLE) Day();
    CC_CHECK(carriage->cargo_losses == 0 && town->stock[CC_GOOD_TOOLS] == before + 4);
    CC_CHECK(carriage->location_id == town->id && carriage->trips_completed == 1);
    Prepare(false);
    ReachDeparture();
    CC_CHECK(carriage->active_shipment_id != 0 && site->stock[CC_GOOD_TOOLS] == 0);
    RoundTrip();
    while (carriage->location_id != site->id) Day();
    CC_CHECK(carriage->cargo_losses == 0 && site->stock[CC_GOOD_TOOLS] == 1);
    CheckWaiting();
    CheckPartialAndLoss();
    puts("Site deliveries: actual pickup and supply, elapsed travel, working tool, save/resume and daily parity passed");
    return 0;
}
