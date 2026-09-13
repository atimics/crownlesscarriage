#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <inttypes.h>
#include <string.h>
static CcSim sim, split, restored;
static CcRoadProductionAccounting totals, daily;
static char error[256];

static void Prepare(uint32_t seed)
{
    CcSimInit(&sim, seed);
    memset(&totals, 0, sizeof(totals));
    memset(&daily, 0, sizeof(daily));
    for (int32_t i = 0; i < sim.road_site_count; ++i) {
        CcRoadSite *site = &sim.road_sites[i];
        CcProductionRecipe recipe;
        if (!CcRoadSiteRecipe(site, &recipe)) continue;
        site->accessible = true; site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
        site->stock[CC_GOOD_TOOLS] = 1;
        for (int32_t j = 0; j < recipe.input_count; ++j)
            site->stock[recipe.inputs[j].good] = recipe.inputs[j].reserve + recipe.inputs[j].units * 4;
    }
    sim.routes[1].condition = 50;
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckYear(uint32_t seed)
{
    Prepare(seed);
    int32_t initial[CC_MAX_ROAD_SITES][CC_GOOD_COUNT];
    for (int32_t i = 0; i < sim.road_site_count; ++i)
        memcpy(initial[i], sim.road_sites[i].stock, sizeof(initial[i]));
    split = sim;
    restored = sim;
    CcSimAdvanceDays(&restored, 365);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 365, NULL, NULL, &totals);
    for (int32_t day = 0; day < 365; ++day)
        CcSimAdvanceDaysWithProductionAccounting(&split, 1, NULL, NULL, &daily);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&split));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(memcmp(&totals, &daily, sizeof(totals)) == 0);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    CC_CHECK(totals.sites[2].output[CC_GOOD_BREAD] >= 4);
    CC_CHECK(totals.sites[11].output[CC_GOOD_TOOLS] >= 4);
    CC_CHECK(totals.sites[15].output[CC_GOOD_WHEAT] >= 16);
    CC_CHECK(totals.sites[5].output[CC_GOOD_STONE] == 0);
    CC_CHECK(totals.sites[5].route_repair > 0 && totals.sites[5].input[CC_GOOD_STONE] > 0);
    for (int32_t i = 0; i < sim.road_site_count; ++i) {
        CcProductionRecipe recipe;
        if (CcRoadSiteRecipe(&sim.road_sites[i], &recipe)) {
            CC_CHECK(totals.sites[i].gates[CC_PRODUCTION_INVALID] == 0);
            for (int32_t j = 0; j < recipe.input_count; ++j)
                CC_CHECK(sim.road_sites[i].stock[recipe.inputs[j].good] >= recipe.inputs[j].reserve);
        }
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            uint64_t aboard = 0;
            for (int32_t j = 0; j < sim.shipment_count; ++j) {
                const CcShipment *cargo = &sim.shipments[j];
                if ((cargo->origin_id == sim.road_sites[i].id || cargo->destination_id == sim.road_sites[i].id) &&
                    (cargo->status == CC_SHIPMENT_TRAVELLING || cargo->status == CC_SHIPMENT_BLOCKED) &&
                    cargo->good == (CcGood)good) aboard += (uint64_t)cargo->quantity;
            }
            CC_CHECK(totals.sites[i].freight_sent[good] + totals.sites[i].freight_shipped[good] ==
                totals.sites[i].freight_received[good] + totals.sites[i].freight_delivered[good] +
                totals.sites[i].freight_lost[good] + aboard);
            CC_CHECK((int64_t)sim.road_sites[i].stock[good] == initial[i][good] +
                (int64_t)totals.sites[i].output[good] - (int64_t)totals.sites[i].input[good] -
                (int64_t)totals.sites[i].maintenance_input[good] +
                (int64_t)totals.sites[i].freight_received[good] - (int64_t)totals.sites[i].freight_shipped[good]);
        }
    }
    const char *path = "road-production.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CcSimAdvanceDays(&restored, 365);
    CcSimAdvanceDays(&sim, 365);
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    (void)remove(path);
    Prepare(seed);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 35, error, sizeof(error)));
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&restored) == CcSimHash(&sim));
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);
}

static void CheckGates(void)
{
    Prepare(42);
    CcRoadSite *site = &sim.road_sites[2];
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(CcSimPlanRoadSite(&sim, site).output == 2);
    CC_CHECK(CcSimHash(&sim) == hash);
    site->accessible = false;
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_CLOSED);
    site->accessible = true; site->condition = 49;
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_CONDITION);
    site->condition = 80; site->stock[CC_GOOD_TOOLS] = 0;
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_TOOLS);
    site->stock[CC_GOOD_TOOLS] = 1; site->stock[CC_GOOD_WHEAT] = 2;
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_INPUT);
    site->stock[CC_GOOD_WHEAT] = 6; site->stock[CC_GOOD_BREAD] = 17;
    CC_CHECK(CcSimPlanRoadSite(&sim, site).gate == CC_PRODUCTION_OUTPUT_FULL);
    site->stock[CC_GOOD_BREAD] = 0;
    sim.schema_version = 64;
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(site->stock[CC_GOOD_WHEAT] == 6 && site->stock[CC_GOOD_BREAD] == 0);
}

static void CheckControls(void)
{
    for (int32_t unfunded = 0; unfunded < 2; ++unfunded) {
        Prepare(42);
        /* Keep the unfunded control's source goods in town reserves. */
        if (unfunded != 0)
            for (int32_t i = 0; i < sim.settlement_count; ++i)
                for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
                    sim.settlements[i].reserve_target[good] = CC_SIM_MAX_UNITS;
        for (int32_t i = 0; i < sim.road_site_count; ++i) {
            if (unfunded != 0) memset(sim.road_sites[i].stock, 0, sizeof(sim.road_sites[i].stock));
            else { sim.road_sites[i].accessible = false; sim.road_sites[i].blocker = CC_ROAD_SITE_BLOCKER_TREE; }
        }
        CcSimAdvanceDaysWithProductionAccounting(&sim, 365, NULL, NULL, &totals);
        for (int32_t i = 0; i < sim.road_site_count; ++i) {
            CC_CHECK(totals.sites[i].work == 0);
            for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
                CC_CHECK(totals.sites[i].input[good] == 0 && totals.sites[i].output[good] == 0);
        }
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    }
}

int main(void)
{
    CheckGates();
    CheckControls();
    CheckYear(UINT32_C(0x5eed0001));
    CheckYear(UINT32_C(0xc0a71a9e));
    puts("Road production: local receipts, pilot outputs, reserves, gates, time batching and saves passed");
    return 0;
}
