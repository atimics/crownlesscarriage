#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, legacy, loaded;
static CcProductionAccounting total;
static char error[256];

static void Prepare(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    sim.current_day = 6;
    CcRoadSite *site = &sim.road_sites[2];
    site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE; site->condition = 100;
    CcSettlement *town = CcSimSettlementMutable(&sim, site->home_settlement_id);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) town->stock[good] = 1000;
    CcRoute *route = &sim.routes[CcSimRoute(&sim, site->route_id) - sim.routes];
    route->travel_days = 40;
    for (int32_t i = 0; i < sim.faction_count; ++i) sim.factions[i].support = 100;
    sim.diplomacy[0][1] = sim.diplomacy[1][0] = CC_DIPLOMACY_ALLIANCE;
    for (int32_t i = 0; i < sim.royal_carriage_count; ++i) {
        CcRoyalCarriage *cart = &sim.royal_carriages[i];
        cart->condition = i == 0 ? 100 : 0;
        if (i == 0) { cart->location_id = town->id; cart->next_dispatch_day = sim.current_day; }
    }
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
}

int main(void)
{
    Prepare();
    CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &total);
    CcRoadSite *site = &sim.road_sites[2];
    CC_CHECK(CcSimIncomingGood(&sim, site->id, CC_GOOD_TOOLS) == 1);
    /* A new crown inherits the town while its previous supplier is on the road. */
    CC_CHECK(sim.settlements[0].kingdom_id == sim.kingdoms[0].id);
    for (int32_t i = 0; i < sim.character_count; ++i)
        if (sim.characters[i].id == sim.kingdoms[0].ruler_character_id ||
            sim.characters[i].id == sim.kingdoms[0].monastery_patron_id)
            sim.characters[i].home_settlement_id = sim.settlements[0].id;
    CcSimSettlementMutable(&sim, site->home_settlement_id)->kingdom_id = sim.kingdoms[1].id;
    sim.royal_carriages[1].location_id = site->home_settlement_id;
    sim.royal_carriages[1].condition = 100;
    sim.royal_carriages[1].next_dispatch_day = sim.current_day;
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
    CcSiteFreightPlan planned = CcSimPlanSiteFreight(&sim, sim.royal_carriages[1].id, site->id);
    CC_CHECK(planned.gate == CC_SITE_FREIGHT_READY && planned.good == CC_GOOD_WHEAT && planned.quantity == 4);
    legacy = sim; legacy.schema_version = 69;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 7, NULL, NULL, &total);
    CcSimAdvanceDays(&legacy, 7);
    CC_CHECK(total.sites[2].freight_sent[CC_GOOD_TOOLS] == 1);
    CC_CHECK(total.sites[2].freight_sent[CC_GOOD_WHEAT] == 4);
    CC_CHECK(CcSimIncomingGood(&sim, site->id, CC_GOOD_TOOLS) == 1);
    CC_CHECK(CcSimIncomingGood(&legacy, site->id, CC_GOOD_TOOLS) == 2);
    CC_CHECK(CcSimIncomingGood(&legacy, site->id, CC_GOOD_WHEAT) == 0);
    CC_CHECK(site->stock[CC_GOOD_TOOLS] == 0 && site->stock[CC_GOOD_WHEAT] == 0);
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
    const char *path = "site-freight-reservations.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)) && CcSimHash(&sim) == CcSimHash(&loaded));
    CC_CHECK(CcSimIncomingGood(&loaded, site->id, CC_GOOD_TOOLS) == 1);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 8, NULL, NULL, &total);
    CcSimAdvanceDays(&loaded, 8);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));
    CC_CHECK(site->stock[CC_GOOD_TOOLS] == 1 && site->stock[CC_GOOD_WHEAT] == 4);
    CC_CHECK(total.sites[2].freight_received[CC_GOOD_TOOLS] == 1 && total.sites[2].freight_received[CC_GOOD_WHEAT] == 4);
    /* An old save retains both real tool loads through upgrade. */
    CC_CHECK(CcSaveWrite(path, &legacy, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)));
    CC_CHECK(loaded.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcSimIncomingGood(&loaded, site->id, CC_GOOD_TOOLS) == 2);
    CcSimAdvanceDays(&loaded, 8);
    CC_CHECK(loaded.road_sites[2].stock[CC_GOOD_TOOLS] == 2);
    CC_CHECK(CcSimValidate(&loaded, error, sizeof(error)));
    (void)remove(path);
    puts("Site freight: inherited incoming cargo reserves the tool order and the next crown supplies grain");
    return 0;
}
