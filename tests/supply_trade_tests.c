#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <limits.h>
#include <string.h>

static CcSim sim, restored, before;
static char error[256];

static void Prepare(int town)
{
    CcSimInit(&sim, 42U);
    sim.player.location_id = sim.settlements[town].id;
    sim.carriage.location_id = sim.player.location_id;
    memset(sim.player.cargo, 0, sizeof(sim.player.cargo));
    sim.player.coins = 100;
}

static bool Trade(CcGood good, int amount)
{
    CcCommand command = {.kind=CC_COMMAND_TRADE_SUPPLY,
        .target_id=sim.player.location_id, .good=good, .amount=amount};
    return CcSimApply(&sim, &command, error, sizeof(error));
}

static void Reject(CcGood good, int amount)
{
    before = sim;
    CC_CHECK(!Trade(good, amount));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
}

static void RoundTrip(void)
{
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    sim = restored;
}

static void CheckWheat(void)
{
    Prepare(0);
    sim.settlements[0].reserve_target[CC_GOOD_BREAD] = 20;
    sim.settlements[0].consumption[CC_GOOD_BREAD] = 0;
    sim.settlements[0].stock[CC_GOOD_BREAD] = 15;
    CcSupplyOffer offer = CcSimSupplyOffer(&sim, CC_GOOD_WHEAT);
    CC_CHECK(offer.ready && offer.remaining == 6);
    CC_CHECK(offer.source_unit_price == 3 && offer.delivery_unit_price == 5);
    int bread = sim.settlements[0].stock[CC_GOOD_BREAD];
    int wheat = sim.settlements[0].stock[CC_GOOD_WHEAT];
    CcMoney total = CcSimTrackedGold(&sim);
    CC_CHECK(Trade(CC_GOOD_WHEAT, 4));
    CC_CHECK(sim.player.coins == 88 && sim.player.cargo[CC_GOOD_WHEAT] == 4);
    CC_CHECK(Trade(CC_GOOD_WHEAT, -4));
    CC_CHECK(sim.player.coins == 108 && sim.player.cargo[CC_GOOD_WHEAT] == 0);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_BREAD] == bread + 4);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] == wheat - 4);
    CC_CHECK(CcSimTrackedGold(&sim) == total);
    RoundTrip();
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining == 2);
    CC_CHECK(Trade(CC_GOOD_WHEAT, 3));
    Reject(CC_GOOD_WHEAT, -3);
    CC_CHECK(Trade(CC_GOOD_WHEAT, -2));
    Reject(CC_GOOD_WHEAT, -1);
    RoundTrip();
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining == 0);
    ++sim.current_day;
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining == 0);
    CcCommand retail = {.kind=CC_COMMAND_TRADE, .target_id=sim.player.location_id,
        .good=CC_GOOD_BREAD, .amount=2};
    CC_CHECK(CcSimApply(&sim, &retail, error, sizeof(error)));
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining == 2);
    CC_CHECK(Trade(CC_GOOD_WHEAT, -1));
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining == 1);
}

static void CheckStone(void)
{
    Prepare(3);
    CcSupplyOffer offer = CcSimSupplyOffer(&sim, CC_GOOD_RAW_STONE);
    CC_CHECK(offer.source_available == 24);
    CC_CHECK(offer.source_unit_price == 3 && offer.delivery_unit_price == 6);
    CcMoney total = CcSimTrackedGold(&sim);
    CC_CHECK(Trade(CC_GOOD_RAW_STONE, 4));
    RoundTrip();
    sim.settlements[4].stock[CC_GOOD_STONE] = 0;
    CC_CHECK(CcSimSupplyOfferAt(&sim, sim.settlements[4].id, CC_GOOD_RAW_STONE).remaining > 0);
    sim.player.location_id = sim.settlements[4].id;
    sim.carriage.location_id = sim.player.location_id;
    sim.settlements[4].stock[CC_GOOD_STONE] = 0;
    sim.settlements[4].reserve_target[CC_GOOD_STONE] = 10;
    sim.settlements[4].consumption[CC_GOOD_STONE] = 0;
    int stone = sim.settlements[4].stock[CC_GOOD_STONE];
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_RAW_STONE).source_available == 0);
    Reject(CC_GOOD_RAW_STONE, 1);
    CC_CHECK(Trade(CC_GOOD_RAW_STONE, -4));
    CC_CHECK(sim.player.coins == 112 && sim.player.cargo[CC_GOOD_RAW_STONE] == 0);
    CC_CHECK(sim.settlements[4].stock[CC_GOOD_STONE] == stone + 4);
    CC_CHECK(sim.settlements[4].stock[CC_GOOD_RAW_STONE] == 0);
    CC_CHECK(CcSimTrackedGold(&sim) == total);
    RoundTrip();
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_RAW_STONE).remaining == 6);
}

static void CheckRejections(void)
{
    Prepare(0);
    Reject(CC_GOOD_BREAD, 1);
    Reject(CC_GOOD_WHEAT, 0);
    Reject(CC_GOOD_WHEAT, INT_MIN);
    Reject(CC_GOOD_WHEAT, INT_MAX);
    Reject(CC_GOOD_WHEAT, -1);
    sim.player.coins = 0;
    Reject(CC_GOOD_WHEAT, 1);
    sim.player.coins = 100;
    sim.player.cargo_capacity = 0;
    Reject(CC_GOOD_WHEAT, 1);
    sim.player.cargo_capacity = CC_CARGO_CAPACITY;
    sim.settlements[0].stock[CC_GOOD_WHEAT] = 0;
    Reject(CC_GOOD_WHEAT, 1);
    sim.player.cargo[CC_GOOD_WHEAT] = 2;
    sim.settlements[0].market_coins = 4;
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining == 0);
    Reject(CC_GOOD_WHEAT, -1);
    sim.settlements[0].market_coins = 100;
    sim.settlements[0].fire_damage = 80;
    CC_CHECK(!CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).ready);
    Reject(CC_GOOD_WHEAT, -1);
    Prepare(0);
    before = sim;
    CcCommand far = {.kind=CC_COMMAND_TRADE_SUPPLY,
        .target_id=sim.settlements[3].id, .good=CC_GOOD_WHEAT, .amount=1};
    CC_CHECK(!CcSimApply(&sim, &far, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    sim.journey.active = true;
    Reject(CC_GOOD_WHEAT, 1);
}

static void CheckBuyback(void)
{
    Prepare(0);
    sim.settlements[0].price[CC_GOOD_WHEAT] = 99;
    CC_CHECK(Trade(CC_GOOD_WHEAT, 2));
    CcCommand buyback = {.kind=CC_COMMAND_TRADE, .target_id=sim.player.location_id,
        .good=CC_GOOD_WHEAT, .amount=-2};
    CC_CHECK(CcSimTradeResalePrice(&sim, &sim.settlements[0], CC_GOOD_WHEAT) == 2);
    CC_CHECK(CcSimApply(&sim, &buyback, error, sizeof(error)));
    CC_CHECK(sim.player.coins == 98);
    sim.schema_version = 116U;
    sim.settlements[0].price[CC_GOOD_WHEAT] = 12;
    CC_CHECK(CcSimTradeResalePrice(&sim, &sim.settlements[0], CC_GOOD_WHEAT) == 9);
}

static void CheckJournal(void)
{
    const char *path = "supply-journal-test.ccsave";
    (void)remove(path);
    Prepare(0);
    sim.settlements[0].stock[CC_GOOD_BREAD] = 0;
    CC_CHECK(CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining >= 2);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand command = {.kind=CC_COMMAND_TRADE_SUPPLY,
        .target_id=sim.player.location_id, .good=CC_GOOD_WHEAT, .amount=2};
    CC_CHECK(CcJournalApply(journal, &sim, &command, error, sizeof(error)));
    command.amount = -2;
    CC_CHECK(CcJournalApply(journal, &sim, &command, error, sizeof(error)));
    int wanted = CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining;
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcSimSupplyOffer(&restored, CC_GOOD_WHEAT).remaining == wanted);
    (void)remove(path);
}

static void CheckLegacy(void)
{
    Prepare(0);
    sim.schema_version = 116U;
    /* A legacy save omits raw stone and orders from every store and hash. */
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.player.coins == sim.player.coins);
    CC_CHECK(restored.settlements[3].stock[CC_GOOD_RAW_STONE] == 24);
    CC_CHECK(CcSimSupplyOffer(&restored, CC_GOOD_WHEAT).ready);
    sim = restored;
    RoundTrip();
}

int main(void)
{
    CheckWheat(); CheckStone(); CheckRejections(); CheckBuyback(); CheckJournal(); CheckLegacy();
    puts("PASS paid local deliveries: quotes, profit, stocks, stock demand, atomic rejection, save and journal replay");
    return 0;
}
