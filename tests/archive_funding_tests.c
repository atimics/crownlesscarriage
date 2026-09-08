#include "sim/cc_sim.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, before;
static CcArchiveFundingPlan Query(void)
{
    before = sim;
    CcArchiveFundingPlan plan = CcSimArchiveFundingPlan(&sim);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CC_CHECK(plan.total == plan.shares[0] + plan.shares[1]);
    CC_CHECK((plan.total > 0) == (plan.blocker == CC_ARCHIVE_FUNDING_READY));
    return plan;
}
static void Blocked(CcArchiveFundingBlocker blocker, const char *name)
{
    CcArchiveFundingPlan plan = Query();
    CC_CHECK(plan.total == 0 && plan.donor_count == 0 && plan.blocker == blocker);
    CC_CHECK(strcmp(CcArchiveFundingBlockerName(plan.blocker), name) == 0);
}
int main(void)
{
    CcSimInit(&sim, 42U);
    CcId seat = CcSimMaterialChainSnapshot(&sim).scriptorium_id;
    CcSettlement *town = CcSimSettlementMutable(&sim, seat);
    CC_CHECK(town != NULL);
    town->kingdom_id = sim.kingdoms[2].id;
    for (int32_t i = 0; i < sim.kingdom_count; ++i) sim.kingdoms[i].treasury = 900;
    for (int32_t i = 0; i < sim.route_count; ++i) sim.routes[i].closed = true;
    sim.iron_ledger_reserve = 0;
    CcArchiveFundingPlan plan = Query();
    CC_CHECK(plan.seat_id == seat && plan.total == 50 && plan.donor_count == 1);
    CC_CHECK(plan.donor_ids[0] == town->kingdom_id && plan.shares[0] == 50);
    sim.kingdoms[2].treasury = 799;
    Blocked(CC_ARCHIVE_FUNDING_CONNECTED_DONORS, "connected_solvent_donors");
    for (int32_t i = 0; i < sim.route_count; ++i) sim.routes[i].closed = false;
    sim.iron_ledger_reserve = 45;
    plan = Query();
    CC_CHECK(plan.donor_count == 2 && plan.total == 5);
    CC_CHECK(plan.donor_ids[0] == sim.kingdoms[0].id && plan.donor_ids[1] == sim.kingdoms[1].id);
    CC_CHECK(plan.shares[0] == 3 && plan.shares[1] == 2);
    sim.kingdoms[1].treasury = 799;
    Blocked(CC_ARCHIVE_FUNDING_CONNECTED_DONORS, "connected_solvent_donors");
    sim.kingdoms[1].treasury = 900;
    CC_CHECK(Query().total == 5);
    sim.schema_version = 57;
    sim.iron_ledger_reserve = 39;
    Blocked(CC_ARCHIVE_FUNDING_TOP_UP_LIMIT, "top_up_limit");
    sim.iron_ledger_reserve = 40;
    CC_CHECK(Query().total == 10);
    sim.iron_ledger_reserve = 50;
    Blocked(CC_ARCHIVE_FUNDING_LEDGER_FUNDED, "ledger_funded");
    sim.iron_ledger_reserve = 45;
    for (int32_t i = 0; i < sim.settlement_count; ++i) sim.settlements[i].population = 0;
    plan = Query();
    CC_CHECK(plan.total == 0 && plan.seat_id == 0);
    Blocked(CC_ARCHIVE_FUNDING_NO_SEAT, "archive_seat");
    sim.schema_version = 55;
    Blocked(CC_ARCHIVE_FUNDING_UNAVAILABLE, "unavailable");
    CC_CHECK(CcSimArchiveFundingPlan(NULL).blocker == CC_ARCHIVE_FUNDING_UNAVAILABLE);
    CC_CHECK(strcmp(CcArchiveFundingBlockerName(CC_ARCHIVE_FUNDING_READY), "ready") == 0);
    CC_CHECK(strcmp(CcArchiveFundingBlockerName((CcArchiveFundingBlocker)99), "unknown") == 0);
    puts("Verified host funding, connected donors, exact odd shares, solvency, and legacy cap");
    return 0;
}
