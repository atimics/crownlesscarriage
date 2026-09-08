#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <string.h>

static CcSim sim, before, restored;
static char error[256] = {0};

static void Prepare(bool rebuild)
{
    CcSimInit(&sim, 42U);
    sim.player.location_id = sim.settlements[1].id;
    sim.carriage.location_id = sim.player.location_id;
    if (rebuild) sim.settlements[1].service_mask &= ~(UINT32_C(1) << CC_SERVICE_BAKERY);
    CcBakerySupportPlan plan = CcSimBakerySupportPlan(&sim, sim.player.location_id);
    for (int i = 0; i < CC_GOOD_COUNT; ++i) sim.player.cargo[i] = plan.cargo[i];
    for (int i = 0; i < CC_GOOD_COUNT; ++i) sim.settlements[1].stock[i] += plan.town_materials[i];
    sim.player.coins = 400;
}

int main(void)
{
    Prepare(false);
    before = sim;
    CcBakerySupportPlan plan = CcSimBakerySupportPlan(&sim, sim.player.location_id);
    CC_CHECK(plan.ready && plan.contact_id != 0 && plan.building_days == 0);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CcMoney gold = CcSimTrackedGold(&sim);
    int32_t wheat = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    int32_t hunger = sim.settlements[1].hunger;
    CcCommand support = {.kind = CC_COMMAND_SUPPORT_BAKERY, .target_id = sim.player.location_id};
    CC_CHECK(CcSimApply(&sim, &support, error, sizeof(error)));
    CC_CHECK(CcSimTrackedGold(&sim) == gold && CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == wheat);
    CC_CHECK(sim.settlements[1].hunger == hunger);
    const CcCharacter *contact = CcSimCharacter(&sim, plan.contact_id);
    CC_CHECK(CcCharacterRemembers(contact, CC_CHARACTER_MEMORY_PLAYER_HELPED, support.target_id));
    int32_t disposition = contact->player_disposition;
    sim.player.cargo[CC_GOOD_WHEAT] = 12;
    CC_CHECK(CcSimApply(&sim, &support, error, sizeof(error)));
    CC_CHECK(contact->player_disposition == disposition);
    before = sim;
    CC_CHECK(!CcSimApply(&sim, &support, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }

    Prepare(true);
    before = sim;
    sim.settlements[1].stock[CC_GOOD_TOOLS] = 0;
    CC_CHECK(!CcSimBakerySupportPlan(&sim, sim.player.location_id).ready);
    sim = before;
    plan = CcSimBakerySupportPlan(&sim, sim.player.location_id);
    CC_CHECK(plan.ready && plan.building_days == 7);
    support.target_id = sim.player.location_id;
    before = sim;
    CC_CHECK(!CcSimApply(&sim, &support, error, sizeof(error))); /* stale supply quote */
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    support.amount = 7;
    gold = CcSimTrackedGold(&sim);
    int32_t wood = CcSimTrackedGood(&sim, CC_GOOD_WOOD);
    const char *path = "bakery-support-test.sqlite3";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) { fprintf(stderr, "%s\n", error); return 1; }
    CC_CHECK(CcJournalApply(journal, &sim, &support, error, sizeof(error)));
    CC_CHECK(sim.settlements[1].service_project_days == 7);
    CC_CHECK(CcSimTrackedGold(&sim) == gold && CcSimTrackedGood(&sim, CC_GOOD_WOOD) == wood - 8);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 6, error, sizeof(error)));
    CC_CHECK(!CcSettlementHasService(&sim.settlements[1], CC_SERVICE_BAKERY));
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1, error, sizeof(error)));
    CC_CHECK(CcSettlementHasService(&sim.settlements[1], CC_SERVICE_BAKERY));
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcSimBakerySupportPlan(&restored, restored.player.location_id).remembered);
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);

    Prepare(false);
    for (int i = 0; i < sim.situation_count; ++i) {
        CcSituation *promise = &sim.situations[i];
        if (promise->kind == CC_SITUATION_RELIEF_DELIVERY && promise->status == CC_SITUATION_ACTIVE) {
            promise->good = CC_GOOD_WHEAT;
            promise->quantity = 12;
            promise->progress = 0;
            sim.player.accepted_situation_id = promise->id;
            CC_CHECK(!CcSimBakerySupportPlan(&sim, sim.player.location_id).ready);
            break;
        }
    }
    Prepare(false);
    sim.schema_version = 71U;
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(restored.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(CcSimBakerySupportPlan(&restored, restored.player.location_id).ready);
    (void)remove(path);
    puts("Bakery support conserves goods and crowns, remembers help, and survives replay.");
    return 0;
}
