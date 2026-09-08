#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, plain, split, loaded;
static CcProductionAccounting total, daily;
static char error[256];

static CcRoadSite *Prepare(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    memset(&total, 0, sizeof(total)); memset(&daily, 0, sizeof(daily));
    sim.current_day = 6;
    CcRoadSite *site = &sim.road_sites[2];
    CC_CHECK(site->kind == CC_ROAD_SITE_MILL);
    site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE; site->condition = 44;
    for (int32_t i = 0; i < sim.royal_carriage_count; ++i) sim.royal_carriages[i].condition = 0;
    return site;
}

static void CheckLocal(void)
{
    CcRoadSite *site = Prepare();
    site->stock[CC_GOOD_TOOLS] = 5; site->stock[CC_GOOD_WOOD] = 4; site->stock[CC_GOOD_WHEAT] = 6;
    uint64_t hash = CcSimHash(&sim);
    CcProductionReceipt preview = CcSimPlanRoadSiteMaintenance(&sim, site->id);
    CC_CHECK(preview.gate == CC_PRODUCTION_READY && preview.inputs[0] == 1 && preview.inputs[1] == 1);
    CC_CHECK(hash == CcSimHash(&sim));
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_WORK);
    plain = sim; split = sim;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 28, NULL, NULL, &total);
    CcSimAdvanceDays(&plain, 28);
    for (int day = 0; day < 28; ++day) CcSimAdvanceDaysWithProductionAccounting(&split, 1, NULL, NULL, &daily);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&plain) && CcSimHash(&sim) == CcSimHash(&split));
    CC_CHECK(memcmp(&total, &daily, sizeof(total)) == 0);
    CC_CHECK(site->condition == 84 && site->stock[CC_GOOD_TOOLS] == 1 && site->stock[CC_GOOD_WOOD] == 0);
    CC_CHECK(site->stock[CC_GOOD_WHEAT] == 6 && site->stock[CC_GOOD_BREAD] == 0);
    CC_CHECK(total.sites[2].maintenance_input[CC_GOOD_TOOLS] == 4 && total.sites[2].maintenance_input[CC_GOOD_WOOD] == 4);
    CC_CHECK(total.sites[2].site_repair == 40 && total.sites[2].maintenance_work == 8);
    CC_CHECK(CcSimPlanRoadSiteMaintenance(&sim, site->id).gate == CC_PRODUCTION_CLOSED);
    const char *path = "site-maintenance.ccsave";
    if (!CcSaveWrite(path, &sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)) && CcSimHash(&sim) == CcSimHash(&loaded));
    CcSimAdvanceDaysWithProductionAccounting(&sim, 7, NULL, NULL, &total);
    CcSimAdvanceDays(&loaded, 7);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));
    CC_CHECK(total.sites[2].output[CC_GOOD_BREAD] == 2 && total.sites[2].input[CC_GOOD_WHEAT] == 2);
    CC_CHECK(total.sites[2].site_repair == 40);
    (void)remove(path);
}

static void CheckGates(void)
{
    for (int mode = 0; mode < 4; ++mode) {
        CcRoadSite *site = Prepare();
        site->stock[CC_GOOD_TOOLS] = mode == 0 ? 1 : 2;
        site->stock[CC_GOOD_WOOD] = mode == 1 ? 0 : 1;
        if (mode == 2) { site->accessible = false; site->blocker = CC_ROAD_SITE_BLOCKER_TREE; }
        if (mode == 3) sim.schema_version = 67;
        CC_CHECK(CcSimPlanRoadSiteMaintenance(&sim, site->id).gate != CC_PRODUCTION_READY);
        CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &total);
        CC_CHECK(site->condition == 44 && total.sites[2].site_repair == 0);
    }
    CC_CHECK(CcSimPlanRoadSiteMaintenance(NULL, 0).gate == CC_PRODUCTION_CLOSED);
}

static void CheckCarriageFunding(void)
{
    CcRoadSite *site = Prepare();
    CcSettlement *town = CcSimSettlementMutable(&sim, site->home_settlement_id);
    town->stock[CC_GOOD_TOOLS] = 100; town->stock[CC_GOOD_WOOD] = 100;
    for (int32_t i = 0; i < sim.royal_carriage_count; ++i) {
        CcRoyalCarriage *cart = &sim.royal_carriages[i];
        if (cart->kingdom_id == town->kingdom_id) {
            cart->location_id = town->id;
            cart->condition = 100;
            cart->next_dispatch_day = 6;
            break;
        }
    }
    CC_CHECK(site->stock[CC_GOOD_TOOLS] == 0 && site->stock[CC_GOOD_WOOD] == 0);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &total);
    CC_CHECK(site->condition == 44 && total.sites[2].site_repair == 0);
    CC_CHECK(total.sites[2].freight_sent[CC_GOOD_TOOLS] == 2 && total.sites[2].freight_received[CC_GOOD_TOOLS] == 0);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 364, NULL, NULL, &total);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    CC_CHECK(total.sites[2].site_repair >= 40);
    CC_CHECK(site->condition == 44 + (int32_t)total.sites[2].site_repair - (int32_t)total.sites[2].wear);
    CC_CHECK(total.sites[2].freight_received[CC_GOOD_TOOLS] >= 5 && total.sites[2].freight_received[CC_GOOD_WOOD] >= 4);
    for (int good = 0; good < CC_GOOD_COUNT; ++good) {
        const CcSiteProductionAccounting *row = &total.sites[2];
        CC_CHECK((int64_t)site->stock[good] == (int64_t)row->freight_received[good] - (int64_t)row->freight_shipped[good] +
            (int64_t)row->output[good] - (int64_t)row->input[good] - (int64_t)row->maintenance_input[good]);
    }
}

int main(void)
{
    CheckLocal(); CheckGates(); CheckCarriageFunding();
    puts("Site maintenance: local costs, weekly work, physical funding, observer parity and saved condition passed");
    return 0;
}
