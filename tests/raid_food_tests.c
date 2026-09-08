#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "test_support.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static CcSettlement *ReturningRaid(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0x460f00d));
    CcSettlement *town = CcSimSettlementMutable(sim, sim->player.location_id);
    CC_CHECK(town != NULL);
    town->hunger = 80;
    town->stock[CC_GOOD_BREAD] = 0;
    town->stock[CC_GOOD_WHEAT] = 0;
    town->stock[CC_GOOD_MEAT] = 0;
    sim->iron_ledger_reserve += town->market_coins;
    town->market_coins = 0;
    for (int32_t i = 0; i < sim->route_count; ++i) sim->routes[i].closed = true;
    sim->hoard_raiders.phase = CC_HOARD_RAIDERS_OUTBOUND;
    sim->hoard_raiders.motive = CC_HOARD_RAID_SOCIAL_RELIEF;
    sim->hoard_raiders.origin_settlement_id = town->id;
    sim->hoard_raiders.days_remaining = 1;
    CcSimAdvanceDays(sim, 1);
    CC_CHECK(sim->hoard_raiders.phase == CC_HOARD_RAIDERS_RETURNING);
    sim->hoard_raiders.days_remaining = 1;
    return town;
}

static int32_t ReturnEvents(const CcSim *sim)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event->kind == CC_EVENT_HOARD_HEIST_RETURNED) ++count;
    }
    return count;
}

int main(void)
{
    char error[256];
    static CcSim legacy, current, resumed, batched, supplied;
    ReturningRaid(&legacy);
    legacy.schema_version = 52U;
    CcMoney legacy_coins = legacy.hoard_raiders.carried_treasure;
    CcSimAdvanceDays(&legacy, 1);
    CC_CHECK(legacy.settlements[0].hunger == 80 - (int32_t)legacy_coins / 2);
    CC_CHECK(CcSimHash(&legacy) == UINT64_C(0x26bed631bd4afed2));

    CcSettlement *town = ReturningRaid(&current);
    CcId town_id = town->id;
    CC_CHECK(CcSimValidate(&current, error, sizeof(error)));
    CcMoney gold = CcSimTrackedGold(&current);
    CcMoney returned = current.hoard_raiders.carried_treasure;
    CcMoney market = town->market_coins;
    batched = current;
    const char *path = "raid-food-journal.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &current, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &current, 1, error, sizeof(error)));
    CC_CHECK(town->hunger == 80);
    CC_CHECK(town->stock[CC_GOOD_BREAD] == 0);
    CC_CHECK(town->stock[CC_GOOD_WHEAT] == 0);
    CC_CHECK(town->stock[CC_GOOD_MEAT] == 0);
    CC_CHECK(town->market_coins == market + returned);
    CC_CHECK(CcSimTrackedGold(&current) == gold);
    CC_CHECK(ReturnEvents(&current) == 1);
    supplied = current;
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &resumed, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcSimHash(&resumed) == CcSimHash(&current));
    CC_CHECK(CcJournalAdvanceDays(journal, &resumed, 1, error, sizeof(error)));
    CcSimAdvanceDays(&current, 1);
    CcSimAdvanceDays(&batched, 2);
    CC_CHECK(CcSimHash(&resumed) == CcSimHash(&current));
    CC_CHECK(CcSimHash(&batched) == CcSimHash(&current));
    CC_CHECK(ReturnEvents(&resumed) == 1);
    CC_CHECK(CcSimTrackedGold(&resumed) == gold);
    CC_CHECK(CcSimSettlement(&resumed, town_id)->market_coins == market + returned);
    CC_CHECK(CcJournalClose(&journal, &resumed, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);

    /* Returned money pays for real carried bread through the existing trade. */
    town = CcSimSettlementMutable(&supplied, town_id);
    CcSettlement *source = &supplied.settlements[1];
    CC_CHECK(source->stock[CC_GOOD_BREAD] >= 2);
    source->stock[CC_GOOD_BREAD] -= 2;
    supplied.player.cargo[CC_GOOD_BREAD] += 2;
    town->price[CC_GOOD_BREAD] = 4;
    int32_t bread = CcSimTrackedGood(&supplied, CC_GOOD_BREAD);
    CcMoney purse = supplied.player.coins;
    CcMoney buyer = town->market_coins;
    CcCommand delivery = {.kind = CC_COMMAND_TRADE, .good = CC_GOOD_BREAD, .amount = -2};
    CC_CHECK(CcSimApply(&supplied, &delivery, error, sizeof(error)));
    CC_CHECK(town->stock[CC_GOOD_BREAD] == 2);
    CC_CHECK(town->market_coins == buyer - 6);
    CC_CHECK(supplied.player.coins == purse + 6);
    CC_CHECK(CcSimTrackedGold(&supplied) == gold);
    CC_CHECK(CcSimTrackedGood(&supplied, CC_GOOD_BREAD) == bread);
    CC_CHECK(town->hunger == 80 - 2 * CcGoodNutritionValue(CC_GOOD_BREAD, CC_NUTRITION_CIVILIAN));
    CcSimAdvanceDays(&supplied, 4);
    CC_CHECK(town->stock[CC_GOOD_BREAD] < 2);
    CC_CHECK(CcSimTrackedGold(&supplied) == gold);
    CC_CHECK(CcSimValidate(&supplied, error, sizeof(error)));

    /* An old returning save upgrades before adopting the new return rule. */
    ReturningRaid(&legacy);
    legacy.schema_version = 52U;
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&legacy, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &resumed, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(resumed.schema_version == CC_SIM_SCHEMA_VERSION);
    CcSimAdvanceDays(&resumed, 1);
    CC_CHECK(CcSimSettlement(&resumed, town_id)->hunger == 80);
    CC_CHECK(ReturnEvents(&resumed) == 1);
    return 0;
}
