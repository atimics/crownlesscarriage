#include "persistence/cc_save.h"
#include "sim/cc_food_relief.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    CcSim sim, restored;
    CcFoodReliefObservation observation;
    CcFoodReliefOutcome outcome;
    char error[256];
    CcSimInit(&sim, UINT32_C(1202));
    CcId payer = sim.characters[0].id;
    CcId beneficiary = sim.characters[6].id;
    assert(sim.characters[0].current_settlement_id == sim.characters[6].current_settlement_id);
    sim.characters[0].travel_coins = 100;
    sim.characters[6].hungry_days = 4;
    assert(CcFoodReliefObserve(&sim, payer, beneficiary, &observation, error, sizeof(error)));
    CcFoodReliefProposal proposal = {payer, beneficiary, observation.place_id, 1, observation.unit_price};
    assert(CcFoodReliefPropose(&sim, &proposal, &outcome, error, sizeof(error)));
    CcId agreement = outcome.agreement_id;
    assert(CcFoodReliefAccept(&sim, agreement, beneficiary, &outcome, error, sizeof(error)));
    CcMoney before = sim.characters[0].travel_coins;
    int32_t stock = CcSimSettlement(&sim, observation.place_id)->stock[CC_GOOD_FOOD];
    assert(CcFoodReliefExecute(&sim, agreement, payer, &outcome, error, sizeof(error)));
    assert(outcome.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED);
    assert(sim.characters[0].travel_coins == before - observation.unit_price);
    assert(CcSimSettlement(&sim, observation.place_id)->stock[CC_GOOD_FOOD] == stock - 1);
    assert(sim.characters[6].hungry_days == 0);
    CcFoodReliefOutcome replay;
    assert(CcFoodReliefExecute(&sim, agreement, payer, &replay, error, sizeof(error)));
    assert(replay.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED && sim.characters[0].travel_coins == before - observation.unit_price);
    unsigned char *bytes = NULL;
    size_t length = 0U;
    assert(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    assert(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    assert(CcFoodReliefRead(&restored, agreement, &replay));
    assert(replay.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED);
    puts("food relief typed agreement tests passed");
    return 0;
}
