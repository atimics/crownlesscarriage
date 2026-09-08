#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, old, loaded;
static CcProductionAccounting total;
static char error[256];

static CcRoadSite *Prepare(int32_t slot, int32_t condition)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    memset(&total, 0, sizeof(total));
    sim.current_day = 6;
    CcRoadSite *site = &sim.road_sites[slot];
    site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE; site->condition = condition;
    for (int32_t town = 0; town < sim.settlement_count; ++town)
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
            sim.settlements[town].reserve_target[good] = CC_SIM_MAX_UNITS;
    return site;
}

static void CheckTaper(void)
{
    CcRoadSite *site = Prepare(2, 60);
    site->stock[CC_GOOD_WHEAT] = 20; site->stock[CC_GOOD_TOOLS] = 1;
    old = sim; old.schema_version = 68;
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
    CcSimAdvanceDaysWithProductionAccounting(&sim, 364, NULL, NULL, &total);
    CcSimAdvanceDays(&old, 364);
    CC_CHECK(site->condition == 49 && total.sites[2].wear == 11);
    CC_CHECK(total.sites[2].maintenance_work == 0 && total.sites[2].output[CC_GOOD_BREAD] == 11);
    CC_CHECK(site->stock[CC_GOOD_WHEAT] == 9 && site->stock[CC_GOOD_TOOLS] == 1);
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_CONDITION);
    CC_CHECK(old.road_sites[2].condition == 60);
    const char *path = "site-wear.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)) && CcSimHash(&sim) == CcSimHash(&loaded));
    CcSimAdvanceDaysWithProductionAccounting(&sim, 28, NULL, NULL, &total);
    CC_CHECK(site->condition == 49 && total.sites[2].wear == 11 && total.sites[2].output[CC_GOOD_BREAD] == 11);
    /* Prepare local repair supplies for this focused recovery fixture. */
    CcSettlement *town = CcSimSettlementMutable(&sim, site->home_settlement_id);
    CC_CHECK(town->stock[CC_GOOD_TOOLS] >= 1 && town->stock[CC_GOOD_WOOD] >= 1);
    town->stock[CC_GOOD_TOOLS]--; town->stock[CC_GOOD_WOOD]--;
    site->stock[CC_GOOD_TOOLS]++; site->stock[CC_GOOD_WOOD]++;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 7, NULL, NULL, &total);
    CC_CHECK(site->condition == 59 && total.sites[2].site_repair == 10);
    CC_CHECK(total.sites[2].maintenance_input[CC_GOOD_TOOLS] == 1 && total.sites[2].maintenance_input[CC_GOOD_WOOD] == 1);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 7, NULL, NULL, &total);
    CC_CHECK(site->condition == 58 && total.sites[2].output[CC_GOOD_BREAD] == 12);
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
    (void)remove(path);
}

static void CheckRecipeWork(void)
{
    CcRoadSite *site = Prepare(11, 60);
    site->stock[CC_GOOD_TOOLS] = 1; site->stock[CC_GOOD_IRON] = 5; site->stock[CC_GOOD_WOOD] = 3;
    CcProductionReceipt plan = CcSimPlanRoadSite(&sim, site);
    CC_CHECK(plan.gate == CC_PRODUCTION_READY && plan.batches == 1);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &total);
    CC_CHECK(total.sites[11].output[CC_GOOD_TOOLS] == 1 && total.sites[11].wear == 1 && site->condition == 59);

    site = Prepare(5, 90);
    site->stock[CC_GOOD_TOOLS] = 1; site->stock[CC_GOOD_STONE] = 5;
    sim.routes[CcSimRoute(&sim, site->route_id) - sim.routes].condition = 50;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &total);
    CC_CHECK(total.sites[5].work == 6 && total.sites[5].wear == 3 && site->condition == 87);

    site = Prepare(2, 90);
    site->stock[CC_GOOD_TOOLS] = 0; site->stock[CC_GOOD_WHEAT] = 6;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 7, NULL, NULL, &total);
    CC_CHECK(total.sites[2].wear == 0 && site->condition == 90);
}

int main(void)
{
    CheckTaper(); CheckRecipeWork();
    puts("Site wear: completed work, condition taper, stop, saved condition and funded recovery passed");
    return 0;
}
