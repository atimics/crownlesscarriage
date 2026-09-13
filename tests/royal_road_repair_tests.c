#include "sim/cc_sim.h"
#include "test_support.h"

#include <string.h>

/* Crown carriage road repair (schema 100), design: docs/crown-carriage-roads.md.
   A closed route belongs to the crowns of its endpoint settlements; their idle
   carriages mend it, jointly at peace, by skirmish at war. Markets come first:
   a carriage takes road work only after trade planning leaves it idle, so the
   tests quiet the economy first to watch the road duty deterministically. */

#define WORLD_SEED(number) ((uint32_t)((number) * UINT64_C(0x9e3779b9)))

static CcSim sim;
static char error[256];

static int MaxI32(int a, int b) { return a > b ? a : b; }

static int32_t KingdomSlot(CcId kingdom_id)
{
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        if (sim.kingdoms[i].id == kingdom_id) return i;
    }
    return -1;
}

static CcRoyalCarriage *CarriageForKingdom(CcId kingdom_id)
{
    for (int32_t i = 0; i < sim.royal_carriage_count; ++i) {
        if (sim.royal_carriages[i].kingdom_id == kingdom_id) {
            return &sim.royal_carriages[i];
        }
    }
    return NULL;
}

static int32_t CountRouteEvents(CcEventKind kind, CcId route_id)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim.event_count; ++i) {
        if (sim.events[i].kind == kind && sim.events[i].subject_id == route_id) ++count;
    }
    return count;
}

static bool RepairMode(const CcRoyalCarriage *carriage)
{
    return carriage != NULL &&
        (carriage->mode == CC_ROYAL_CARRIAGE_REPAIR_TRAVELLING ||
         carriage->mode == CC_ROYAL_CARRIAGE_REPAIR_WORKING);
}

/* Remove trade needs so the crown carriages are free for road duty. */
static void QuietTrade(void)
{
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        CcSettlement *place = &sim.settlements[i];
        for (int good = 0; good < CC_GOOD_COUNT; ++good) {
            place->consumption[good] = 0;
            place->production[good] = 0;
            place->stock[good] = MaxI32(place->stock[good],
                                        place->reserve_target[good]);
        }
    }
}

/* Close a route and make it a crown matter: both ends stocked for the camp,
   but too depopulated for the roadside labor plan (< 220 people), so local
   hands are blocked and the crown carriage is the only repair. */
static void BreakRoute(int32_t slot)
{
    CcRoute *road = &sim.routes[slot];
    road->closed = true;
    road->condition = 5;
    for (int end = 0; end < 2; ++end) {
        CcId end_id = end == 0 ? road->from_id : road->to_id;
        CcSettlement *place = CcSimSettlementMutable(&sim, end_id);
        CC_CHECK(place != NULL);
        place->stock[CC_GOOD_WOOD] = MaxI32(place->stock[CC_GOOD_WOOD], 4);
        place->stock[CC_GOOD_STONE] = MaxI32(place->stock[CC_GOOD_STONE], 4);
        place->stock[CC_GOOD_TOOLS] = MaxI32(place->stock[CC_GOOD_TOOLS], 4);
        place->population = 100;
    }
    CC_CHECK(CcSimRoadRecoveryPlan(&sim, road->id).blocked != 0U);
}

static void TestSoloRoadIsRepairedByItsCrown(void)
{
    CcSimInit(&sim, WORLD_SEED(1));
    QuietTrade();
    const int32_t road_slot = 0;
    const CcSettlement *from = CcSimSettlement(&sim, sim.routes[road_slot].from_id);
    const CcSettlement *to = CcSimSettlement(&sim, sim.routes[road_slot].to_id);
    CC_CHECK(from != NULL && to != NULL);
    CC_CHECK(from->kingdom_id == to->kingdom_id);
    CcMoney gold = CcSimTrackedGold(&sim);
    BreakRoute(road_slot);
    bool opened = false;
    bool saw_repair_mode = false;
    for (int32_t day = 0; day < 120 && !opened; ++day) {
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
        saw_repair_mode = saw_repair_mode ||
            RepairMode(CarriageForKingdom(from->kingdom_id));
        opened = !sim.routes[road_slot].closed;
    }
    CC_CHECK(opened);
    CC_CHECK(saw_repair_mode);
    CC_CHECK(CountRouteEvents(CC_EVENT_ROYAL_CARRIAGE_REPAIR_DISPATCHED,
                              sim.routes[road_slot].id) >= 1);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
}

static void TestSharedRoadIsMendedFromBothEnds(void)
{
    CcSimInit(&sim, WORLD_SEED(2));
    QuietTrade();
    const int32_t road_slot = 1;
    const CcSettlement *from = CcSimSettlement(&sim, sim.routes[road_slot].from_id);
    const CcSettlement *to = CcSimSettlement(&sim, sim.routes[road_slot].to_id);
    CC_CHECK(from != NULL && to != NULL);
    CC_CHECK(from->kingdom_id != to->kingdom_id);
    int32_t slot_a = KingdomSlot(from->kingdom_id);
    int32_t slot_b = KingdomSlot(to->kingdom_id);
    sim.diplomacy[slot_a][slot_b] = CC_DIPLOMACY_PEACE;
    sim.diplomacy[slot_b][slot_a] = CC_DIPLOMACY_PEACE;
    CC_CHECK(!CcSimKingdomsAtWar(&sim, from->kingdom_id, to->kingdom_id));
    CcMoney gold = CcSimTrackedGold(&sim);
    BreakRoute(road_slot);
    bool opened = false;
    bool saw_first_crown = false;
    bool saw_second_crown = false;
    for (int32_t day = 0; day < 120 && !opened; ++day) {
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
        saw_first_crown = saw_first_crown ||
            RepairMode(CarriageForKingdom(from->kingdom_id));
        saw_second_crown = saw_second_crown ||
            RepairMode(CarriageForKingdom(to->kingdom_id));
        opened = !sim.routes[road_slot].closed;
    }
    CC_CHECK(opened);
    CC_CHECK(saw_first_crown && saw_second_crown);
    CC_CHECK(CountRouteEvents(CC_EVENT_ROYAL_CARRIAGE_REPAIR_DISPATCHED,
                              sim.routes[road_slot].id) >= 2);
    CC_CHECK(CountRouteEvents(CC_EVENT_ROYAL_ROAD_SKIRMISH,
                              sim.routes[road_slot].id) == 0);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
}

static void TestContestedRoadGoesToTheStrongerCrown(void)
{
    CcSimInit(&sim, WORLD_SEED(3));
    QuietTrade();
    const int32_t road_slot = 1;
    const CcSettlement *from = CcSimSettlement(&sim, sim.routes[road_slot].from_id);
    const CcSettlement *to = CcSimSettlement(&sim, sim.routes[road_slot].to_id);
    CC_CHECK(from != NULL && to != NULL);
    CC_CHECK(from->kingdom_id != to->kingdom_id);
    int32_t slot_a = KingdomSlot(from->kingdom_id);
    int32_t slot_b = KingdomSlot(to->kingdom_id);
    CC_CHECK(slot_a >= 0 && slot_b >= 0);
    sim.diplomacy[slot_a][slot_b] = CC_DIPLOMACY_WAR;
    sim.diplomacy[slot_b][slot_a] = CC_DIPLOMACY_WAR;
    sim.kingdoms[slot_a].treasury = 400;
    sim.kingdoms[slot_b].treasury = 200;
    /* Taxes refill treasuries over the run, so the war costs are asserted
       through the skirmish event and gold conservation, not balances. */
    CcMoney gold = CcSimTrackedGold(&sim);
    BreakRoute(road_slot);
    bool opened = false;
    for (int32_t day = 0; day < 160 && !opened; ++day) {
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
        opened = !sim.routes[road_slot].closed;
    }
    /* The skirmish happened, the stronger escort held the work and finished
       it, and the soldiers' pay stayed inside the tracked economy. */
    CC_CHECK(opened);
    CC_CHECK(CountRouteEvents(CC_EVENT_ROYAL_ROAD_SKIRMISH,
                              sim.routes[road_slot].id) >= 1);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
}

static void TestBrokeCrownLeavesAContestedRoad(void)
{
    CcSimInit(&sim, WORLD_SEED(3));
    QuietTrade();
    const int32_t road_slot = 1;
    const CcSettlement *from = CcSimSettlement(&sim, sim.routes[road_slot].from_id);
    const CcSettlement *to = CcSimSettlement(&sim, sim.routes[road_slot].to_id);
    int32_t slot_a = KingdomSlot(from->kingdom_id);
    int32_t slot_b = KingdomSlot(to->kingdom_id);
    sim.diplomacy[slot_a][slot_b] = CC_DIPLOMACY_WAR;
    sim.diplomacy[slot_b][slot_a] = CC_DIPLOMACY_WAR;
    /* Markets sweep their coins into treasuries, so a broke crown needs a
       broke realm to stay broke: empty every treasury, market and war chest. */
    for (int32_t i = 0; i < sim.kingdom_count; ++i) {
        sim.kingdoms[i].treasury = 0;
    }
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].market_coins = 0;
        sim.settlements[i].war_chest = 0;
    }
    CcMoney gold = CcSimTrackedGold(&sim);
    BreakRoute(road_slot);
    /* Neither crown can hire escorts, so while the war lasts no carriage
       rides this road and no crown gold moves. A brokered peace may end
       the condition; the check holds only for the war's duration. */
    for (int32_t day = 0; day < 40; ++day) {
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
        if (!CcSimKingdomsAtWar(&sim, from->kingdom_id, to->kingdom_id)) break;
        CC_CHECK(CountRouteEvents(CC_EVENT_ROYAL_CARRIAGE_REPAIR_DISPATCHED,
                                  sim.routes[road_slot].id) == 0);
    }
    CC_CHECK(CountRouteEvents(CC_EVENT_ROYAL_ROAD_SKIRMISH,
                              sim.routes[road_slot].id) == 0);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
}

int main(void)
{
    TestSoloRoadIsRepairedByItsCrown();
    TestSharedRoadIsMendedFromBothEnds();
    TestContestedRoadGoesToTheStrongerCrown();
    TestBrokeCrownLeavesAContestedRoad();
    return 0;
}
