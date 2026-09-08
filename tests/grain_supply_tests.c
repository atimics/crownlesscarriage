#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <string.h>

static CcSim sim, restored, before;
static char error[256];
static void CheckValid(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

int main(void)
{
    CcSimInit(&sim, 42U);
    sim.player.location_id = sim.settlements[1].id;
    sim.carriage.location_id = sim.player.location_id;
    sim.player.coins = 400;
    CcCommand fund = {.kind = CC_COMMAND_FUND_GRAIN_SUPPLY, .target_id = sim.player.location_id};
    CcMoney gold = CcSimTrackedGold(&sim);
    CC_CHECK(CcSimApply(&sim, &fund, error, sizeof(error)));
    CcGrainSupply *supply = &sim.grain_supplies[1];
    CC_CHECK(supply->purse == 192 && supply->enabled && CcSimTrackedGold(&sim) == gold);
    CC_CHECK(CcSimCharacter(&sim, supply->organiser_id) != NULL);
    CheckValid();
    before = sim;
    (void)CcSimGrainDeliveryPlan(&sim, fund.target_id);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    bool dispatched = false, blocked = false;
    int closed_days = 0;
    for (int day = 0; day < 240; ++day) {
        if (dispatched && closed_days < 14) {
            for (int r = 0; r < sim.route_count; ++r) { sim.routes[r].closed = true; sim.routes[r].condition = 0; }
            closed_days++;
        } else if (closed_days == 14) {
            for (int r = 0; r < sim.route_count; ++r) { sim.routes[r].closed = false; sim.routes[r].condition = 80; }
            closed_days++;
        }
        CcSimAdvanceDays(&sim, 1);
        if (supply->ordered > 0) dispatched = true;
        if (CcSimGrainDeliveryPlan(&sim, fund.target_id).status == CC_GRAIN_BLOCKED) blocked = true;
        CheckValid();
    }
    printf("ordered=%d delivered=%d lost=%d redirected=%d blocked=%d\n", supply->ordered, supply->delivered, supply->lost, supply->redirected, blocked);
    CC_CHECK(dispatched && blocked && supply->delivered > 0);
    CC_CHECK(supply->spent > 0 && supply->purse + supply->spent == 192);
    const char *path = "grain-supply-test.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CcMoney refund = supply->purse;
    gold = CcSimTrackedGold(&sim);
    CcMoney player = sim.player.coins;
    fund.amount = -1;
    CC_CHECK(CcSimApply(&sim, &fund, error, sizeof(error)));
    CC_CHECK(!supply->enabled && supply->purse == 0 && sim.player.coins == player + refund);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    (void)remove(path);
    (void)remove("grain-supply-test.ccsave-wal");
    (void)remove("grain-supply-test.ccsave-shm");
    puts("Grain orders use a real purse and carriage, survive disruption, and retain receipts.");
    return 0;
}
