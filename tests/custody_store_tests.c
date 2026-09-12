#include "sim/cc_sim_custody.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>

static CcSim sim, before, loaded;
static CcId town, event;
static char error[256];

static void Prepare(void)
{
    CcSimInit(&sim, 42);
    town = sim.settlements[0].id;
    event = sim.events[0].id;
    CC_CHECK(event != 0);
    sim.settlements[0].stock[CC_GOOD_WHEAT] = 1000;
    sim.custody.entries[0] = (CcCustodyEntry){.id = 1, .revision = 1,
        .owner_id = town, .holder = {CC_CUSTODY_STORE, town},
        .kind = CC_CUSTODY_CONTAINER, .quantity = 1, .condition = 100,
        .capacity = 10, .active = true};
    sim.custody.next_id = 2;
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void RejectPack(int32_t quantity, CcCustodyResult expected)
{
    before = sim;
    uint64_t id = 999;
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, quantity, 1,
        sim.custody.entries[0].revision, sim.custody.next_id, event, &id) == expected);
    CC_CHECK(id == 999 && memcmp(&sim, &before, sizeof(sim)) == 0);
}

static void Journey(void)
{
    Prepare();
    int32_t total = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    CcMoney coins = CcSimTrackedGold(&sim);
    uint64_t id = 0;
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, 7, 1, 1, 2, event, &id) == CC_CUSTODY_READY);
    CC_CHECK(id == 2 && sim.custody.next_id == 3);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] == 993);
    CC_CHECK(sim.custody.entries[0].revision == 2);
    const CcCustodyEntry *entry = CcCustodyFind(&sim.custody, id);
    CC_CHECK(entry != NULL && entry->quantity == 7 && entry->holder.id == 1);
    CC_CHECK(entry->last_event_id == event && entry->condition == 100);
    before = sim;
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, 7, 1, 1, 2, event, NULL) == CC_CUSTODY_STALE);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &loaded, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&loaded) == CcSimHash(&sim));
    sim = loaded;
    uint64_t revision = CcCustodyFind(&sim.custody, id)->revision;
    CC_CHECK(CcSimUnpackStoreGoods(&sim, town, id, revision, 3, event) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyFind(&sim.custody, id)->quantity == 4);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] == 996);
    before = sim;
    CC_CHECK(CcSimUnpackStoreGoods(&sim, town, id, revision, 3, event) == CC_CUSTODY_STALE);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    revision = CcCustodyFind(&sim.custody, id)->revision;
    CC_CHECK(CcSimUnpackStoreGoods(&sim, town, id, revision, 4, event) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyFind(&sim.custody, id) == NULL);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == total && CcSimTrackedGold(&sim) == coins);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] == 1000);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, 2, 1,
        sim.custody.entries[0].revision, sim.custody.next_id, event, &id) == CC_CUSTODY_READY);
    CC_CHECK(id == 4 && CcCustodyFind(&sim.custody, 2) == NULL);
}

static void FreightSlots(void)
{
    Prepare();
    int32_t total = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    uint64_t id = 0;
    /* Royal freight fits ten wheat per slot, so the ten-slot crate fits 100. */
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, 100, 1, 1, 2, event, &id) == CC_CUSTODY_READY);
    RejectPack(1, CC_CUSTODY_FULL);
    CC_CHECK(CcSimUnpackStoreGoods(&sim, town, id, 2, 50, event) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyFind(&sim.custody, id)->quantity == 50);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == total);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CarrierJourney(void)
{
    Prepare();
    CcRoyalCarriage *carrier = &sim.royal_carriages[0];
    CC_CHECK(carrier->location_id == town && carrier->mode == CC_ROYAL_CARRIAGE_IDLE);
    CcId destination = sim.settlements[1].id;
    uint64_t id = 0;
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, 7, 1, 1, 2, event, &id) == CC_CUSTODY_READY);
    CcCustodyTransfer load = {.entry_id = 1, .revision = 2, .actor_id = town,
        .event_id = event, .destination = {CC_CUSTODY_CARRIER, carrier->id}, .quantity = 1};
    CC_CHECK(CcSimTransferCustody(&sim, &load, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcSimCustodyCarrierLoad(&sim, carrier->id) == 2);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    before = sim;
    CC_CHECK(CcSimTransferCustody(&sim, &load, NULL) == CC_CUSTODY_STALE);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    loaded = sim;
    sim.royal_trade_week = sim.current_day / 7;
    for (int i = 0; i < sim.route_count; ++i) sim.royal_route_slots_used[i] = CC_SIM_MAX_UNITS;
    before = sim;
    CC_CHECK(!CcSimDispatchCustodyCarrier(&sim, carrier->id, destination));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    sim = loaded;
    CC_CHECK(CcSimDispatchCustodyCarrier(&sim, carrier->id, destination));
    CC_CHECK(carrier->mode == CC_ROYAL_CARRIAGE_REPOSITIONING);
    before = sim;
    CC_CHECK(!CcSimDispatchCustodyCarrier(&sim, carrier->id, destination));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CC_CHECK(carrier->active_shipment_id == 0);
    CcCustodyTransfer unload = {.entry_id = 1, .revision = 3, .actor_id = town,
        .event_id = event, .destination = {CC_CUSTODY_STORE, destination}, .quantity = 1};
    before = sim;
    CC_CHECK(CcSimTransferCustody(&sim, &unload, NULL) == CC_CUSTODY_REMOTE);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    loaded = sim;
    carrier->mode = CC_ROYAL_CARRIAGE_BLOCKED;
    carrier->arrival_day = 0;
    carrier->blocked_since_day = sim.current_day;
    CcCustodyTransfer return_to_origin = unload;
    return_to_origin.destination.id = town;
    before = sim;
    CC_CHECK(CcSimTransferCustody(&sim, &return_to_origin, NULL) == CC_CUSTODY_REMOTE);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    sim = loaded;
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &loaded, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));
    sim = loaded;
    for (int day = 0; day < 20 && carrier->location_id != destination; ++day)
        CcSimAdvanceDays(&sim, 1);
    CC_CHECK(carrier->location_id == destination && carrier->mode == CC_ROYAL_CARRIAGE_IDLE);
    CC_CHECK(carrier->active_shipment_id == 0);
    CC_CHECK(CcSimCustodyCarrierLoad(&sim, carrier->id) == 2);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    int32_t total = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    CC_CHECK(CcSimTransferCustody(&sim, &unload, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyFind(&sim.custody, 1)->holder.id == destination);
    CC_CHECK(CcCustodyFind(&sim.custody, id)->holder.id == 1);
    CC_CHECK(CcCustodyFind(&sim.custody, id)->quantity == 7);
    CC_CHECK(CcCustodyFind(&sim.custody, id)->owner_id == town);
    CC_CHECK(CcSimCustodyCarrierLoad(&sim, carrier->id) == 0);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == total);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void BookingGates(void)
{
    Prepare();
    CcRoyalCarriage *carrier = &sim.royal_carriages[0];
    CcId destination = sim.settlements[1].id;
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, 14, 1, 1, 2, event, NULL) == CC_CUSTODY_READY);
    sim.custody.entries[2] = sim.custody.entries[0];
    sim.custody.entries[2].id = 3;
    sim.custody.entries[2].revision = 1;
    sim.custody.next_id = 4;
    CcCustodyTransfer load = {.entry_id = 1, .revision = 2, .actor_id = town,
        .event_id = event, .destination = {CC_CUSTODY_CARRIER, carrier->id}, .quantity = 1};
    CC_CHECK(CcSimTransferCustody(&sim, &load, NULL) == CC_CUSTODY_READY);
    load.entry_id = 3; load.revision = 1;
    CC_CHECK(CcSimTransferCustody(&sim, &load, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcSimCustodyCarrierLoad(&sim, carrier->id) == 4);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    carrier->next_dispatch_day = sim.current_day + 2;
    before = sim;
    CC_CHECK(!CcSimDispatchCustodyCarrier(&sim, carrier->id, destination));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    carrier->next_dispatch_day = sim.current_day;
    CC_CHECK(CcSimDispatchCustodyCarrier(&sim, carrier->id, destination));
    CcCustodyTransfer repack = {.entry_id = 2, .revision = 2, .actor_id = town,
        .event_id = event, .destination = {CC_CUSTODY_CONTAINER_HOLDER, 3}, .quantity = 1};
    before = sim;
    CC_CHECK(CcSimTransferCustody(&sim, &repack, NULL) == CC_CUSTODY_FORBIDDEN);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    for (int day = 0; day < 20 && carrier->location_id != destination; ++day)
        CcSimAdvanceDays(&sim, 1);
    CC_CHECK(carrier->mode == CC_ROYAL_CARRIAGE_IDLE && carrier->location_id == destination);
    CC_CHECK(CcSimTransferCustody(&sim, &repack, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcSimCustodyCarrierLoad(&sim, carrier->id) == 5);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void Gates(void)
{
    Prepare(); RejectPack(0, CC_CUSTODY_INVALID); RejectPack(1001, CC_CUSTODY_INVALID);
    RejectPack(101, CC_CUSTODY_FULL);
    sim.custody.entries[0].holder.id = sim.settlements[1].id;
    RejectPack(1, CC_CUSTODY_REMOTE);
    sim.custody.entries[0].holder.id = town;
    sim.custody.entries[0].owner_id = sim.player.id;
    RejectPack(1, CC_CUSTODY_FORBIDDEN);
    Prepare();
    for (int i = 1; i < CC_CUSTODY_CAPACITY; ++i) {
        sim.custody.entries[i] = (CcCustodyEntry){.id = (uint64_t)i + 1, .revision = 1,
            .owner_id = town, .holder = {CC_CUSTODY_STORE, town},
            .kind = CC_CUSTODY_GOODS, .quantity = 1, .good = CC_GOOD_WHEAT,
            .condition = 100, .active = true};
        sim.settlements[0].stock[CC_GOOD_WHEAT]--;
    }
    sim.custody.next_id = CC_CUSTODY_CAPACITY + 1;
    RejectPack(1, CC_CUSTODY_FULL);
    Prepare();
    uint64_t id = 0;
    CC_CHECK(CcSimPackStoreGoods(&sim, town, CC_GOOD_WHEAT, 2, 1, 1, 2, event, &id) == CC_CUSTODY_READY);
    sim.settlements[0].stock[CC_GOOD_WHEAT] = CC_SIM_MAX_UNITS;
    before = sim;
    CC_CHECK(CcSimUnpackStoreGoods(&sim, town, id, 2, 2, event) == CC_CUSTODY_FULL);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
}

int main(void)
{
    Journey(); Gates(); FreightSlots(); CarrierJourney(); BookingGates(); return 0;
}
