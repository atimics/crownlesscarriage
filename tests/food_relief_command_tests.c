#include "persistence/cc_save.h"
#include "multiplayer/cc_coop.h"
#include "sim/cc_food_relief.h"
#include "test_support.h"

#include <stdio.h>

static void Setup(CcSim *sim, CcCharacter **payer, CcCharacter **beneficiary)
{
    CcSimInit(sim, 107U);
    *payer = NULL; *beneficiary = NULL;
    CcSettlement *place = &sim->settlements[0];
    place->stock[CC_GOOD_FOOD] = 40;
    place->price[CC_GOOD_FOOD] = 2;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *person = &sim->characters[i];
        if (person->id == sim->player.id) continue;
        person->current_settlement_id = place->id;
        person->travel_destination_id = 0U;
        person->travel_coins = 40;
        if (*payer == NULL) *payer = person;
        else if (*beneficiary == NULL) { *beneficiary = person; break; }
    }
    CC_CHECK(*payer != NULL && *beneficiary != NULL);
}

static CcCommand Propose(const CcCharacter *payer, const CcCharacter *beneficiary,
                         const CcSettlement *place)
{
    return (CcCommand){.kind = CC_COMMAND_FOOD_RELIEF_PROPOSE,
        .actor_id = payer->id, .target_id = beneficiary->id,
        .secondary_id = place->id, .good = (CcGood)2, .amount = 3};
}

int main(void)
{
    CcSim sim, restored;
    CcCharacter *payer, *beneficiary;
    char error[256];
    Setup(&sim, &payer, &beneficiary);
    CcCommand propose = Propose(payer, beneficiary, &sim.settlements[0]);
    CC_CHECK(CcSimApply(&sim, &propose, error, sizeof(error)));
    const CcEvent *agreement = &sim.events[sim.event_count - 1];
    CC_CHECK(agreement->kind == CC_EVENT_RELIEF);
    CcCommand accept = {.kind = CC_COMMAND_FOOD_RELIEF_ACCEPT,
        .actor_id = beneficiary->id, .target_id = agreement->id};
    CC_CHECK(CcSimApply(&sim, &accept, error, sizeof(error)));
    CcCommand execute = {.kind = CC_COMMAND_FOOD_RELIEF_EXECUTE,
        .actor_id = payer->id, .target_id = agreement->id};
    CC_CHECK(CcSimApply(&sim, &execute, error, sizeof(error)));
    CcFoodReliefOutcome outcome;
    CC_CHECK(CcFoodReliefRead(&sim, agreement->id, &outcome));
    CC_CHECK(outcome.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_FOOD] == 37);

    CcCommand forged = propose;
    forged.actor_id = sim.player.id;
    CC_CHECK(!CcSimApply(&sim, &forged, error, sizeof(error)));
    CcSim public_sim;
    Setup(&public_sim, &payer, &beneficiary);
    CC_CHECK(!CcCoopApply(&public_sim, "food_relief_propose", beneficiary->id,
                          2, 3, error, sizeof(error)));

    const char *path = "food-relief-command.ccsave";
    Setup(&sim, &payer, &beneficiary);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    propose = Propose(payer, beneficiary, &sim.settlements[0]);
    CC_CHECK(CcJournalApply(journal, &sim, &propose, error, sizeof(error)));
    agreement = &sim.events[sim.event_count - 1];
    accept = (CcCommand){.kind = CC_COMMAND_FOOD_RELIEF_ACCEPT,
        .actor_id = beneficiary->id, .target_id = agreement->id};
    CC_CHECK(CcJournalApply(journal, &sim, &accept, error, sizeof(error)));
    execute = (CcCommand){.kind = CC_COMMAND_FOOD_RELIEF_EXECUTE,
        .actor_id = payer->id, .target_id = agreement->id};
    CC_CHECK(CcJournalApply(journal, &sim, &execute, error, sizeof(error)));
    CcJournalAbandon(&journal);
    journal = CcJournalResume(path, &restored, error, sizeof(error));
    if (journal == NULL) (void)fprintf(stderr, "journal resume: %s\n", error);
    CC_CHECK(journal != NULL);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(CcJournalClose(&journal, &restored, error, sizeof(error)));
    (void)remove(path); (void)remove("food-relief-command.ccsave-wal");
    (void)remove("food-relief-command.ccsave-shm");
    return 0;
}
