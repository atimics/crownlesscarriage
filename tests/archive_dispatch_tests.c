#include "sim/cc_archive_internal.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>

static CcSim sim, before, restored;
static char error[256];
static CcId carriage_id, seat_id, source_id;
static void Valid(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}
static CcRoyalCarriage *Carriage(void)
{
    for (int i = 0; i < sim.royal_carriage_count; ++i)
        if (sim.royal_carriages[i].id == carriage_id) return &sim.royal_carriages[i];
    return NULL;
}
static void Fixture(void)
{
    CcSimInit(&sim, 42U);
    seat_id = CcArchiveSeat(&sim)->id;
    CcSettlement *seat = CcSimSettlementMutable(&sim, seat_id);
    seat->stock[CC_GOOD_FOOD] = 10000; seat->stock[CC_GOOD_WHEAT] = 10000;
    seat->stock[CC_GOOD_TOOLS] = 10; seat->stock[CC_GOOD_PAPER] = 0;
    source_id = 0;
    for (int i = 0; i < sim.route_count; ++i) {
        CcRoute *route = &sim.routes[i];
        route->closed = false; route->condition = 100; route->security = 100;
        if (source_id == 0 && !route->smuggler_route &&
            (route->from_id == seat_id || route->to_id == seat_id)) {
            source_id = route->from_id == seat_id ? route->to_id : route->from_id;
            route->travel_days = 2;
        }
    }
    CC_CHECK(source_id != 0);
    for (int i = 0; i < sim.kingdom_count; ++i)
        for (int j = 0; j < sim.kingdom_count; ++j) if (i != j) sim.diplomacy[i][j] = CC_DIPLOMACY_ALLIANCE;
    for (int i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_PAPER] = sim.settlements[i].id == source_id ? 1 : 0;
        sim.settlements[i].reserve_target[CC_GOOD_PAPER] = 0;
        sim.settlements[i].production[CC_GOOD_PAPER] = 0;
        sim.settlements[i].price[CC_GOOD_PAPER] = 3;
    }
    carriage_id = 0;
    for (int i = 0; i < sim.royal_carriage_count; ++i) {
        CcRoyalCarriage *carriage = &sim.royal_carriages[i];
        if (carriage->kingdom_id == seat->kingdom_id) {
            carriage_id = carriage->id; carriage->location_id = source_id;
            carriage->condition = 100; break;
        }
    }
    CC_CHECK(carriage_id != 0);
    sim.iron_ledger_reserve = 1000;
    Valid();
}
static void RoundTrip(void)
{
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(memcmp(sim.royal_carriages, restored.royal_carriages, sizeof(sim.royal_carriages)) == 0);
}
static const CcShipment *Load(void)
{
    for (int i = 0; i < sim.shipment_count; ++i)
        if (sim.shipments[i].id == Carriage()->active_shipment_id) return &sim.shipments[i];
    return NULL;
}
static void CheckDisruption(void)
{
    Fixture();
    CC_CHECK(CcArchiveDispatchSupply(&sim, carriage_id));
    CcId id = Load()->id, route_id = Load()->route_id;
    int32_t arrival = Load()->arrival_day;
    for (int i = 0; i < sim.route_count; ++i)
        if (sim.routes[i].id == route_id) sim.routes[i].condition = 0;
    CcSimAdvanceDays(&sim, arrival - sim.current_day);
    CC_CHECK(Load() != NULL && Load()->id == id && Load()->status == CC_SHIPMENT_BLOCKED);
    CC_CHECK(CcSimSettlement(&sim, seat_id)->stock[CC_GOOD_PAPER] == 0);
    CC_CHECK(CcSimSettlement(&sim, source_id)->stock[CC_GOOD_PAPER] == 0);
    Valid(); RoundTrip();
    CcSimAdvanceDays(&sim, 1); CcSimAdvanceDays(&restored, 1);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    bool saw_loss = false;
    for (uint32_t random = 1; random <= 64 && !saw_loss; ++random) {
        Fixture(); sim.random_state = random;
        for (int i = 0; i < sim.route_count; ++i) {
            sim.routes[i].security = 0;
            sim.routes[i].closed = true;
        }
        CC_CHECK(CcArchiveDispatchSupply(&sim, carriage_id));
        id = Load()->id; arrival = Load()->arrival_day;
        CcSimAdvanceDays(&sim, arrival - sim.current_day);
        for (int i = 0; i < sim.shipment_count; ++i) {
            const CcShipment *shipment = &sim.shipments[i];
            if (shipment->id != id || shipment->status != CC_SHIPMENT_LOST) continue;
            CC_CHECK(shipment->quantity == 1);
            CC_CHECK(CcSimSettlement(&sim, source_id)->stock[CC_GOOD_PAPER] == 0);
            CC_CHECK(CcSimSettlement(&sim, seat_id)->stock[CC_GOOD_PAPER] == 0);
            bool linked = false;
            for (int event = 0; event < sim.event_count; ++event) {
                const CcEvent *record = CcSimRecentEvent(&sim, event);
                if (record->kind == CC_EVENT_SHIPMENT_LOST && record->subject_id == id)
                    linked = record->parent_id != 0;
            }
            CC_CHECK(linked);
            saw_loss = true;
            Valid(); RoundTrip();
        }
    }
    CC_CHECK(saw_loss);
}

static void CheckHostileContract(void)
{
    Fixture();
    CcId home = CcSimSettlement(&sim, seat_id)->kingdom_id;
    CcSimSettlementMutable(&sim, source_id)->stock[CC_GOOD_PAPER] = 0;
    for (int i = 0; i < sim.settlement_count; ++i) {
        if (sim.settlements[i].kingdom_id == home) continue;
        source_id = sim.settlements[i].id;
        sim.settlements[i].stock[CC_GOOD_PAPER] = 1;
        break;
    }
    Carriage()->location_id = source_id;
    for (int i = 0; i < sim.kingdom_count; ++i)
        for (int j = 0; j < sim.kingdom_count; ++j)
            if (i != j) sim.diplomacy[i][j] = CC_DIPLOMACY_WAR;
    CcArchiveSupplyPlan plan = CcSimArchiveSupplyPlan(&sim, carriage_id);
    CC_CHECK(plan.gate == CC_ARCHIVE_SUPPLY_READY);
    CC_CHECK(!CcSimRoyalCarriageCanUseRoute(&sim, home, plan.first_route_id));
    CC_CHECK(plan.path_cost > 0);
    CC_CHECK(CcArchiveDispatchSupply(&sim, carriage_id));
    CC_CHECK(Carriage()->archive_contract && Load() != NULL);
    uint64_t contract_hash = CcSimHash(&sim);
    Carriage()->archive_contract = false;
    CC_CHECK(CcSimHash(&sim) != contract_hash);
    Carriage()->archive_contract = true;
    CcId id = Load()->id;
    Valid(); RoundTrip();
    for (int day = 0; day < 60 && Carriage()->active_shipment_id == id; ++day) {
        CcSimAdvanceDays(&sim, 1);
        CcSimAdvanceDays(&restored, 1);
    }
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    bool completed = false;
    for (int i = 0; i < sim.shipment_count; ++i) {
        const CcShipment *shipment = &sim.shipments[i];
        if (shipment->id == id) completed = shipment->status == CC_SHIPMENT_ARRIVED || shipment->status == CC_SHIPMENT_LOST;
    }
    CC_CHECK(completed && !Carriage()->archive_contract);
    Valid(); RoundTrip();
    /* Resting and site-service carriages have their ordinary permission. */
    Fixture(); Carriage()->archive_contract = true;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckBindingDelivery(void)
{
    const CcGood goods[] = {CC_GOOD_GOLD, CC_GOOD_GEMS};
    for (int i = 0; i < 2; ++i) {
        Fixture();
        CcGood good = goods[i];
        CcSettlement *seat = CcSimSettlementMutable(&sim, seat_id);
        seat->stock[CC_GOOD_PAPER] = 10;
        seat->stock[CC_GOOD_GOLD] = 1; seat->stock[CC_GOOD_GEMS] = 1;
        seat->stock[good] = 0;
        sim.archive_staff.active = true; sim.archive_staff.seat_id = seat_id;
        sim.archive_staff.legacy_scribes = sim.archives.scribes;
        CcSettlement *source = CcSimSettlementMutable(&sim, source_id);
        source->stock[good] = source->reserve_target[good] + 1;
        source->price[good] = 1;
        CcArchiveSupplyPlan plan = CcSimArchiveSupplyPlan(&sim, carriage_id);
        CC_CHECK(plan.gate == CC_ARCHIVE_SUPPLY_READY && plan.good == good && plan.source_id == source_id);
        CcMoney money = CcSimTrackedGold(&sim), reserve = sim.iron_ledger_reserve;
        int32_t units = CcSimTrackedGood(&sim, good), stock = source->stock[good];
        CC_CHECK(CcArchiveDispatchSupply(&sim, carriage_id));
        CC_CHECK(CcSimTrackedGold(&sim) == money && CcSimTrackedGood(&sim, good) == units);
        CC_CHECK(source->stock[good] == stock - 1 && seat->stock[good] == 0);
        CC_CHECK(sim.iron_ledger_reserve == reserve - plan.total_charge);
        CcId id = Load()->id; int32_t arrival = Load()->arrival_day;
        Valid(); RoundTrip();
        CcSimAdvanceDays(&sim, arrival - sim.current_day);
        CcSimAdvanceDays(&restored, arrival - restored.current_day);
        CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
        bool arrived = false;
        for (int j = 0; j < sim.shipment_count; ++j)
            if (sim.shipments[j].id == id) arrived = sim.shipments[j].status == CC_SHIPMENT_ARRIVED;
        CC_CHECK(arrived && CcSimSettlement(&sim, seat_id)->stock[good] == 1);
        Valid(); RoundTrip();
    }
}

int main(void)
{
    CheckBindingDelivery();
    CheckHostileContract();
    CheckDisruption();
    Fixture();
    CcArchiveSupplyPlan plan = CcSimArchiveSupplyPlan(&sim, carriage_id);
    CC_CHECK(plan.gate == CC_ARCHIVE_SUPPLY_READY && plan.quantity == 1);
    CcMoney gold = CcSimTrackedGold(&sim), reserve = sim.iron_ledger_reserve;
    CcMoney seller = CcSimSettlement(&sim, source_id)->market_coins;
    CC_CHECK(CcArchiveDispatchSupply(&sim, carriage_id));
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    CC_CHECK(sim.iron_ledger_reserve == reserve - plan.total_charge);
    CC_CHECK(CcSimSettlement(&sim, source_id)->market_coins == seller + plan.goods_cost);
    CC_CHECK(CcSimSettlement(&sim, source_id)->stock[CC_GOOD_PAPER] == 0);
    CC_CHECK(CcSimSettlement(&sim, seat_id)->stock[CC_GOOD_PAPER] == 0);
    const CcShipment *load = Load();
    CC_CHECK(load != NULL && load->quantity == 1 && load->status == CC_SHIPMENT_TRAVELLING);
    CC_CHECK(load->arrival_day > sim.current_day && load->final_destination_id == seat_id);
    CcId shipment_id = load->id;
    int32_t arrival = load->arrival_day;
    before = sim;
    CC_CHECK(!CcArchiveDispatchSupply(&sim, carriage_id));
    CC_CHECK(memcmp(&before, &sim, sizeof(sim)) == 0);
    Valid(); RoundTrip();
    CcSimAdvanceDays(&sim, arrival - sim.current_day);
    CcSimAdvanceDays(&restored, arrival - restored.current_day);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    bool arrived = false;
    for (int i = 0; i < sim.shipment_count; ++i) {
        if (sim.shipments[i].id == shipment_id) arrived = sim.shipments[i].status == CC_SHIPMENT_ARRIVED;
    }
    CC_CHECK(arrived && CcSimSettlement(&sim, seat_id)->stock[CC_GOOD_PAPER] == 1);
    Valid(); RoundTrip();
    Fixture(); sim.iron_ledger_reserve = 0; before = sim;
    CC_CHECK(!CcArchiveDispatchSupply(&sim, carriage_id)); CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    Fixture(); sim.schema_version = 74U; before = sim;
    CC_CHECK(!CcArchiveDispatchSupply(&sim, carriage_id)); CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    Fixture();
    CcSimAdvanceDays(&sim, 27);
    /* Reset the booking stock after the calendar advances to pickup day. */
    CcId host = CcSimSettlement(&sim, seat_id)->kingdom_id;
    for (int i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_PAPER] = 0;
        if (sim.settlements[i].kingdom_id != host) source_id = sim.settlements[i].id;
    }
    for (int i = 0; i < sim.kingdom_count; ++i)
        for (int j = 0; j < sim.kingdom_count; ++j)
            if (i != j) sim.diplomacy[i][j] = CC_DIPLOMACY_WAR;
    CcSimSettlementMutable(&sim, seat_id)->stock[CC_GOOD_PAPER] = 0;
    CcSimSettlementMutable(&sim, source_id)->stock[CC_GOOD_PAPER] = 1;
    Carriage()->location_id = seat_id; Carriage()->mode = CC_ROYAL_CARRIAGE_IDLE;
    Carriage()->route_id = 0; Carriage()->destination_id = 0; Carriage()->target_id = 0;
    Carriage()->active_shipment_id = 0; Carriage()->departure_day = 0; Carriage()->arrival_day = 0;
    Carriage()->next_dispatch_day = 0; Carriage()->blocked_since_day = 0; Carriage()->archive_contract = false;
    reserve = sim.iron_ledger_reserve;
    CC_CHECK(CcArchiveDispatchSupply(&sim, carriage_id));
    CC_CHECK(Carriage()->mode == CC_ROYAL_CARRIAGE_REPOSITIONING && Carriage()->active_shipment_id == 0);
    CC_CHECK(Carriage()->archive_contract);
    CC_CHECK(sim.iron_ledger_reserve == reserve && CcSimSettlement(&sim, source_id)->stock[CC_GOOD_PAPER] == 1);
    Valid(); RoundTrip();
    CcJournal *journal = CcJournalStart("archive-dispatch-test.ccsave", &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 28, error, sizeof(error)));
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead("archive-dispatch-test.ccsave", &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    bool booked = false;
    for (int i = 0; i < sim.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&sim, i);
        if (strstr(event->text, "The archive at") != NULL) booked = true;
    }
    CC_CHECK(booked);
    Valid();
    (void)remove("archive-dispatch-test.ccsave");
    (void)remove("archive-dispatch-test.ccsave-wal");
    (void)remove("archive-dispatch-test.ccsave-shm");
    puts("Archive payment, arrival, repositioning, and journal replay passed.");
    return 0;
}
