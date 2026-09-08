#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, restored, split;
static CcSettlement *town;
static char error[256];

static void Prepare(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    town = NULL;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        CcSettlement *place = &sim.settlements[i];
        place->stock[CC_GOOD_GOLD] = 0; place->stock[CC_GOOD_GEMS] = 0;
        place->gold_seam = false; place->gem_seam = false;
        if (town == NULL && place->function == CC_SETTLEMENT_MARKET &&
            CcSettlementHasService(place, CC_SERVICE_SMITHY)) town = place;
    }
    CC_CHECK(town != NULL);
    town->stock[CC_GOOD_GOLD] = 1; town->stock[CC_GOOD_GEMS] = 1;
    sim.hoard_raiders.cooldown_days = 1000;
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static CcProductionReceipt Plan(void)
{
    uint64_t hash = CcSimHash(&sim);
    CcProductionReceipt receipt = CcSimPlanTreasureWork(&sim, town);
    CC_CHECK(CcSimHash(&sim) == hash);
    CC_CHECK(receipt.location_id == town->id && receipt.storage_id == town->id);
    return receipt;
}

static void CheckSave(void)
{
    const char *path = "treasure-production.ccsave";
    (void)remove(path);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    split = sim;
    CcSimAdvanceDays(&split, 28);
    for (int32_t day = 0; day < 28; ++day) CcSimAdvanceDays(&restored, 1);
    CC_CHECK(CcSimHash(&split) == CcSimHash(&restored));
    (void)remove(path);
    restored = sim;
    CcJournal *journal = CcJournalStart(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &restored, 28, error, sizeof(error)));
    CC_CHECK(CcJournalFlush(journal, &restored, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&restored) == CcSimHash(&split));
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);
}

int main(void)
{
    Prepare();
    CcProductionReceipt receipt = Plan();
    CC_CHECK(receipt.gate == CC_PRODUCTION_READY && receipt.work == 1 && receipt.output == 0);
    CC_CHECK(receipt.inputs[0] == 1 && receipt.inputs[1] == 1);
    town->stock[CC_GOOD_GEMS] = 0;
    receipt = Plan();
    CC_CHECK(receipt.gate == CC_PRODUCTION_INPUT && receipt.blocked_good == CC_GOOD_GEMS);
    CC_CHECK(receipt.work == 0 && town->stock[CC_GOOD_GOLD] == 1);
    town->stock[CC_GOOD_GEMS] = 1;
    town->service_mask &= ~(UINT32_C(1) << CC_SERVICE_SMITHY);
    CC_CHECK(Plan().gate == CC_PRODUCTION_CLOSED);
    town->service_mask |= UINT32_C(1) << CC_SERVICE_SMITHY;
    CcSettlementFunction function = town->function;
    town->function = CC_SETTLEMENT_FARMING;
    CC_CHECK(Plan().gate == CC_PRODUCTION_CLOSED);
    town->function = function;
    CheckSave();
    CcSimAdvanceDays(&sim, 6);
    CC_CHECK(town->treasure_work == 1 && town->treasure_gold_committed == 1 && town->treasure_gems_committed == 1);
    CC_CHECK(town->stock[CC_GOOD_GOLD] == 0 && town->stock[CC_GOOD_GEMS] == 0);
    receipt = Plan();
    CC_CHECK(receipt.gate == CC_PRODUCTION_READY && receipt.work == 1 && receipt.inputs[0] == 0 && receipt.inputs[1] == 0);
    CheckSave();
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(town->treasure_work == 2);
    CheckSave();
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(town->treasure_work == 0 && town->treasure_gold_committed == 0 && town->treasure_gems_committed == 0);
    const CcTreasure *made = NULL;
    for (int32_t i = 0; i < sim.treasure_count; ++i)
        if (sim.treasures[i].maker_settlement_id == town->id && sim.treasures[i].created_day == 21) made = &sim.treasures[i];
    CC_CHECK(made != NULL && made->gold_content == 1 && made->gem_content == 1 && made->craft_work == 3);
    CC_CHECK(made->appraised_value == 140 && made->location_id == town->id && made->owner_id == town->id);
    CcTreasure sample = *made;
    while (sim.treasure_count < CC_MAX_TREASURES) {
        sample.id = CcMakeId(CC_ENTITY_TREASURE, sim.next_entity_serial++);
        sim.treasures[sim.treasure_count++] = sample;
    }
    town->treasure_work = 3; town->treasure_gold_committed = 1; town->treasure_gems_committed = 1;
    CC_CHECK(Plan().gate == CC_PRODUCTION_WORK);
    CheckSave();
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(town->treasure_work == 3 && town->treasure_gold_committed == 1 && town->treasure_gems_committed == 1);
    sim.treasure_count -= 1;
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(town->treasure_work == 0 && sim.treasure_count == CC_MAX_TREASURES);
    CC_CHECK(CcSimPlanTreasureWork(NULL, town).gate == CC_PRODUCTION_INVALID);
    CC_CHECK(CcSimPlanTreasureWork(&sim, NULL).gate == CC_PRODUCTION_INVALID);
    puts("Treasure recipe: atomic inputs, three work steps, local artifact, full pool, saves and replay passed");
    return 0;
}
