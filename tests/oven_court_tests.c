#include "sim/cc_oven_court.h"
#include "sim/cc_production_internal.h"
#include "sim/cc_archive_internal.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include "multiplayer/cc_coop.h"
#include "multiplayer/cc_coop_commands.h"
#include <string.h>

static CcSim sim, before, restored;
static char error[256];
static CcSettlement *Town(void)
{
    return CcSimSettlementMutable(&sim, CcOvenCourtPlace(&sim)->id);
}

static void Prepare(void)
{
    CcSimInit(&sim, 42U);
    CcSettlement *town = Town();
    sim.player.location_id = town->id;
    sim.carriage.location_id = town->id;
    sim.player.accepted_situation_id = 0U;
    town->service_mask |= UINT32_C(1) << CC_SERVICE_BAKERY;
    town->production[CC_GOOD_BREAD] = 12;
    town->stock[CC_GOOD_BREAD] = 0;
    town->stock[CC_GOOD_WHEAT] = 0;
    sim.current_day = 6;
}

static void CheckReadAndAdvice(void)
{
    Prepare();
    CcOvenCourtObservation o;
    before = sim;
    CC_CHECK(CcOvenCourtRead(&sim, &o));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CC_CHECK(o.gate == CC_PRODUCTION_INPUT && o.next_work_day == 7);
    CC_CHECK(strstr(CcOvenCourtAdvice(&o), "later baking") != NULL);
    Town()->stock[CC_GOOD_WHEAT] = 20;
    CC_CHECK(CcOvenCourtRead(&sim, &o) && o.gate == CC_PRODUCTION_READY);
    CC_CHECK(strstr(CcOvenCourtStatus(&o), "not yet baked") != NULL);
    Town()->service_mask &= ~(UINT32_C(1) << CC_SERVICE_BAKERY);
    CC_CHECK(CcOvenCourtRead(&sim, &o) && o.gate == CC_PRODUCTION_CAPACITY);
    CC_CHECK(strstr(CcOvenCourtAdvice(&o), "cannot restore") != NULL);
    Town()->service_project = CC_SERVICE_BAKERY;
    Town()->service_project_days = 5;
    CC_CHECK(CcOvenCourtRead(&sim, &o) && o.rebuilding_days == 5);
    CC_CHECK(strstr(CcOvenCourtAdvice(&o), "needs time") != NULL);
    Town()->service_project_days = 0;
    Town()->service_mask |= UINT32_C(1) << CC_SERVICE_BAKERY;
    Town()->stock[CC_GOOD_BREAD] = CC_SIM_MAX_UNITS;
    CC_CHECK(CcOvenCourtRead(&sim, &o) && o.gate == CC_PRODUCTION_OUTPUT_FULL);
    Town()->stock[CC_GOOD_BREAD] = 4;
    CC_CHECK(CcOvenCourtRead(&sim, &o));
    CC_CHECK(strstr(CcOvenCourtAdvice(&o), "old shortage") != NULL);
    sim.player.location_id = sim.settlements[0].id;
    CC_CHECK(!CcOvenCourtRead(&sim, &o));
    CC_CHECK(!CcOvenCourtRead(NULL, &o) && !CcOvenCourtRead(&sim, NULL));
}

static void CheckPlannerMatchesWork(void)
{
    for (int32_t wheat = 0; wheat < 35; wheat += 7) {
        for (int32_t hunger = 0; hunger <= 100; hunger += 20) {
            Prepare();
            Town()->stock[CC_GOOD_WHEAT] = wheat;
            Town()->hunger = hunger;
            before = sim;
            CcProductionReceipt plan = CcSimPlanBakery(&sim, Town());
            CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
            CcRecipeAccounting accounting = {0};
            const CcSettlement *seat = CcArchiveSeat(&sim);
            int32_t output = CcEconomyRunBakery(&sim, Town(), seat != NULL ? seat->id : 0U,
                &accounting, NULL); /* Day 6: no monthly event callback. */
            CC_CHECK(output == plan.output);
            CC_CHECK(Town()->stock[CC_GOOD_BREAD] == output);
            CC_CHECK(Town()->stock[CC_GOOD_WHEAT] == wheat - output);
            CC_CHECK(accounting.gates[plan.gate] == 1U);
            CC_CHECK(accounting.input[CC_GOOD_WHEAT] == (uint64_t)output);
        }
    }
    /* Reserves apply at the actual archive seat, not invented global stock. */
    Prepare();
    CcSettlement *seat = CcSimSettlementMutable(&sim, CcArchiveSeat(&sim)->id);
    seat->service_mask |= (UINT32_C(1) << CC_SERVICE_BAKERY) | (UINT32_C(1) << CC_SERVICE_MILL);
    seat->production[CC_GOOD_BREAD] = 12;
    seat->stock[CC_GOOD_WHEAT] = 1;
    seat->stock[CC_GOOD_TOOLS] = 1;
    seat->stock[CC_GOOD_BREAD] = 0;
    seat->reserve_target[CC_GOOD_WHEAT] = 20;
    seat->hunger = 0;
    CC_CHECK(CcSimPlanBakery(&sim, seat).gate == CC_PRODUCTION_INPUT);
}

static void CheckSourcesAndNotes(void)
{
    Prepare();
    CcCommand inspect = {.kind = CC_COMMAND_OBSERVE_OVEN_COURT, .target_id = Town()->id};
    CcId town_id = Town()->id;
    CcMoney gold = CcSimTrackedGold(&sim);
    int32_t wheat = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    CC_CHECK(CcSimApply(&sim, &inspect, error, sizeof(error)));
    const CcEvent *note = CcOvenCourtNote(&sim, 0);
    CC_CHECK(note != NULL && note->day == 6 && note->actor_id == sim.player.id);
    CC_CHECK(strstr(note->text, "0 Wheat") != NULL);
    CcId first = note->id;
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(CcSimApply(&sim, &inspect, error, sizeof(error)) && CcSimHash(&sim) == hash);
    CC_CHECK(CcSimTrackedGold(&sim) == gold && CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == wheat);
    CC_CHECK(sim.current_day == 6 && Town()->stock[CC_GOOD_BREAD] == 0);
    Town()->stock[CC_GOOD_WHEAT] = 18;
    CC_CHECK(CcOvenCourtNote(&sim, 0)->id == first);
    CC_CHECK(strstr(CcOvenCourtNote(&sim, 0)->text, "0 Wheat") != NULL);
    sim.player.location_id = sim.settlements[0].id;
    before = sim;
    CC_CHECK(!CcSimApply(&sim, &inspect, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&before));
    CC_CHECK(CcOvenCourtNote(&sim, 0)->id == first); /* Book is still readable remotely. */
    sim.player.location_id = town_id;
    CcId source = CcSimBakerySupportPlan(&sim, town_id).contact_id;
    CC_CHECK(source != 0U && CcOvenCourtCanDiscuss(&sim, source));
    CcCommand ask = {.kind = CC_COMMAND_OBSERVE_OVEN_COURT, .target_id = source, .amount = 1};
    CC_CHECK(CcSimApply(&sim, &ask, error, sizeof(error)));
    CC_CHECK(CcOvenCourtNote(&sim, 0)->actor_id == source);
    CC_CHECK(CcOvenCourtNote(&sim, 1)->id == first);
    CcCharacter *person = (CcCharacter *)CcSimCharacter(&sim, source);
    person->travel_destination_id = sim.settlements[0].id;
    CC_CHECK(!CcOvenCourtCanDiscuss(&sim, source));
    CC_CHECK(!CcSimApply(&sim, &ask, error, sizeof(error)));
    person->travel_destination_id = 0U;
    person->death_day = sim.current_day;
    CC_CHECK(!CcOvenCourtCanDiscuss(&sim, source));
    CC_CHECK(!CcOvenCourtCanDiscuss(&sim, sim.player.id));
    CC_CHECK(!CcOvenCourtCanDiscuss(&sim, UINT64_MAX));
    sim.journey.active = true;
    CC_CHECK(!CcSimApply(&sim, &inspect, error, sizeof(error)));
    sim.journey.active = false;
    sim.mine.phase = CC_MINE_YARD;
    CC_CHECK(!CcSimApply(&sim, &inspect, error, sizeof(error)));
    CC_CHECK(CcOvenCourtNote(&sim, -1) == NULL && CcOvenCourtNote(NULL, 0) == NULL);
}

static void CheckPersistenceAndFiniteTrade(void)
{
    Prepare();
    CcId town = Town()->id;
    sim.player.cargo[CC_GOOD_WHEAT] = 4;
    sim.player.coins = 100;
    Town()->market_coins = 400;
    CcCommand inspect = {.kind = CC_COMMAND_OBSERVE_OVEN_COURT, .target_id = town};
    const char *path = "oven-court-test.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalApply(journal, &sim, &inspect, error, sizeof(error)));
    CcId first = CcOvenCourtNote(&sim, 0)->id;
    CcCommand sell = {.kind = CC_COMMAND_TRADE, .target_id = town, .good = CC_GOOD_WHEAT, .amount = -4};
    CcMoney coins = CcSimTrackedGold(&sim);
    int32_t wheat = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    CC_CHECK(CcJournalApply(journal, &sim, &sell, error, sizeof(error)));
    CC_CHECK(CcSimTrackedGold(&sim) == coins && CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == wheat);
    CC_CHECK(sim.player.cargo[CC_GOOD_WHEAT] == 0 && Town()->stock[CC_GOOD_WHEAT] == 4);
    CC_CHECK(Town()->stock[CC_GOOD_BREAD] == 0); /* Sale is not baking. */
    CC_CHECK(CcOvenCourtNote(&sim, 0)->id == first); /* Sale is not re-inspection. */
    CC_CHECK(!CcJournalApply(journal, &sim, &sell, error, sizeof(error)));
    CC_CHECK(CcJournalApply(journal, &sim, &inspect, error, sizeof(error)));
    CC_CHECK(CcOvenCourtNote(&sim, 1)->id == first);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1, error, sizeof(error)));
    /* Don't infer net Bread stock or exclusive credit: production and consumption both run. */
    CC_CHECK(CcJournalFlush(journal, &sim, error, sizeof(error)));
    uint64_t expected = CcSimHash(&sim);
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    CC_CHECK(journal != NULL && CcSimHash(&restored) == expected);
    CC_CHECK(CcOvenCourtNote(&restored, 1)->id == first);
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path);
    /* Ordinary event-tape compaction retains two acquired notes, but does not
       expose an unrelated hidden production or lore event as company knowledge. */
    for (int32_t i = 0; i < CC_MAX_EVENTS * 2; ++i)
        (void)CcSimPushEvent(&sim, CC_EVENT_LORE_RECORDED, Town()->id, town, 0U, 0, "Unacquired account");
    CC_CHECK(CcOvenCourtNote(&sim, 1)->id == first);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckSharedCommand(void)
{
    Prepare();
    CcCommand inspect = {.kind = CC_COMMAND_OBSERVE_OVEN_COURT, .target_id = Town()->id};
    before = sim;
    CC_CHECK(CcSimApply(&before, &inspect, error, sizeof(error)));
    CC_CHECK(strcmp(CcCoopActionName(inspect.kind), "observe_oven_court") == 0);
    CC_CHECK(CcCoopApply(&sim, "observe_oven_court", inspect.target_id, 0, 0, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&before));
    CC_CHECK(CcCoopApply(&sim, "observe_oven_court", inspect.target_id, 0, 0, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&before));
    CC_CHECK(!CcCoopApply(&sim, "food_relief_execute", inspect.target_id, 0, 0, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&before));
}

int main(void)
{
    CheckSharedCommand(); CheckReadAndAdvice(); CheckPlannerMatchesWork(); CheckSourcesAndNotes(); CheckPersistenceAndFiniteTrade();
    puts("Oven Court: real bottlenecks, local knowledge, finite trade, dated notes and replay.");
    return 0;
}
