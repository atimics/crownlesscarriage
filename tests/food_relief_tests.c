#include "persistence/cc_save.h"
#include "sim/cc_food_relief.h"
#include "test_support.h"

#include <stdio.h>

static CcSim sim, restored;
static CcId payer, beneficiary, place_id;
static char error[256];

static void Setup(void)
{
    CcSimInit(&sim, UINT32_C(1202));
    payer = sim.characters[0].id;
    beneficiary = sim.characters[6].id;
    place_id = sim.characters[0].current_settlement_id;
    CC_CHECK(place_id == sim.characters[6].current_settlement_id);
    sim.characters[0].travel_coins = 100;
    sim.characters[6].hungry_days = 4;
    CcSettlement *place = CcSimSettlementMutable(&sim, place_id);
    place->stock[CC_GOOD_FOOD] = 20;
    place->price[CC_GOOD_FOOD] = 2;
}

static void Valid(const CcSim *world)
{
    if (!CcSimValidate(world, error, sizeof(error))) {
        (void)fprintf(stderr, "world validation: %s\n", error);
        CC_CHECK(false);
    }
}

static CcId Propose(void)
{
    CcFoodReliefOutcome outcome;
    CC_CHECK(CcFoodReliefPropose(&sim,
        &(CcFoodReliefProposal){payer, beneficiary, place_id, 1, 2},
        &outcome, error, sizeof(error)));
    CC_CHECK(outcome.kind == CC_FOOD_RELIEF_OUTCOME_PROPOSED);
    return outcome.agreement_id;
}

static void RoundTrip(void)
{
    unsigned char *bytes = NULL;
    size_t length = 0U;
    Valid(&sim);
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}

static void Success(void)
{
    Setup();
    CcFoodReliefObservation observation;
    CC_CHECK(CcFoodReliefObserve(&sim, payer, beneficiary, &observation, error, sizeof(error)));
    CC_CHECK(observation.stock == 20 && observation.unit_price == 2);
    CcId id = Propose();
    CcFoodReliefOutcome outcome;
    uint64_t before = CcSimHash(&sim);
    CC_CHECK(!CcFoodReliefExecute(&sim, id, payer, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);
    CC_CHECK(!CcFoodReliefAccept(&sim, id, payer, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);
    RoundTrip();
    sim = restored;
    CC_CHECK(CcFoodReliefAccept(&sim, id, beneficiary, &outcome, error, sizeof(error)));
    before = CcSimHash(&sim);
    CC_CHECK(CcFoodReliefAccept(&sim, id, beneficiary, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);
    CC_CHECK(!CcFoodReliefExecute(&sim, id, beneficiary, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);
    RoundTrip();
    sim = restored;
    CcMoney market = CcSimSettlement(&sim, place_id)->market_coins;
    CC_CHECK(CcFoodReliefExecute(&sim, id, payer, &outcome, error, sizeof(error)));
    CC_CHECK(outcome.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED && outcome.total_cost == 2);
    CC_CHECK(sim.characters[0].travel_coins == 98);
    CC_CHECK(CcSimSettlement(&sim, place_id)->market_coins == market + 2);
    CC_CHECK(CcSimSettlement(&sim, place_id)->stock[CC_GOOD_FOOD] == 19);
    CC_CHECK(sim.characters[6].hungry_days == 0);
    CC_CHECK(CcCharacterRemembers(&sim.characters[0], CC_CHARACTER_MEMORY_PROMISE_FULFILLED, id));
    CC_CHECK(CcCharacterRemembers(&sim.characters[6], CC_CHARACTER_MEMORY_PROMISE_FULFILLED, id));
    RoundTrip();
    sim = restored;
    before = CcSimHash(&sim);
    CC_CHECK(CcFoodReliefExecute(&sim, id, payer, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);
}

static void FailedTerms(void)
{
    for (int change = 0; change < 6; ++change) {
        Setup();
        CcId id = Propose();
        CcFoodReliefOutcome outcome;
        CC_CHECK(CcFoodReliefAccept(&sim, id, beneficiary, &outcome, error, sizeof(error)));
        CcSettlement *place = CcSimSettlementMutable(&sim, place_id);
        if (change == 0) place->stock[CC_GOOD_FOOD] = 0;
        if (change == 1) place->price[CC_GOOD_FOOD] = 3;
        if (change == 2) sim.characters[0].travel_coins = 0;
        if (change == 3) sim.characters[6].travel_destination_id = sim.settlements[1].id;
        if (change == 4) sim.characters[6].death_day = sim.current_day;
        if (change == 5) place->market_coins = CC_SIM_MAX_MONEY;
        CcMoney purse = sim.characters[0].travel_coins, market = place->market_coins;
        int32_t stock = place->stock[CC_GOOD_FOOD];
        CC_CHECK(CcFoodReliefExecute(&sim, id, payer, &outcome, error, sizeof(error)));
        CC_CHECK(outcome.kind == CC_FOOD_RELIEF_OUTCOME_FAILED);
        CC_CHECK(purse == sim.characters[0].travel_coins && market == place->market_coins);
        CC_CHECK(stock == place->stock[CC_GOOD_FOOD] && sim.characters[6].hungry_days == 4);
        CC_CHECK(CcCharacterRemembers(&sim.characters[0], CC_CHARACTER_MEMORY_PROMISE_FAILED, id));
        CC_CHECK(CcCharacterRemembers(&sim.characters[6], CC_CHARACTER_MEMORY_PROMISE_FAILED, id));
        uint64_t hash = CcSimHash(&sim);
        CC_CHECK(CcFoodReliefExecute(&sim, id, payer, &outcome, error, sizeof(error)));
        CC_CHECK(hash == CcSimHash(&sim));
        if (change < 3 || change == 5) RoundTrip();
    }
}

static void EvictedOffer(void)
{
    Setup();
    CcId id = Propose();
    for (int i = 0; i < CC_MAX_EVENTS * 3; ++i)
        (void)CcSimPushEvent(&sim, CC_EVENT_RELIEF, place_id, place_id, 0U, 0, "Store observed");
    CC_CHECK(CcSimEvent(&sim, id) == NULL);
    CcFoodReliefOutcome outcome;
    CC_CHECK(CcFoodReliefRead(&sim, id, &outcome));
    CC_CHECK(CcFoodReliefAccept(&sim, id, beneficiary, &outcome, error, sizeof(error)));
    CC_CHECK(CcFoodReliefExecute(&sim, id, payer, &outcome, error, sizeof(error)));
    RoundTrip();
}

static void RejectedTerms(void)
{
    Setup();
    CcFoodReliefOutcome outcome;
    CcFoodReliefProposal proposal = {payer, beneficiary, place_id, 1, 3};
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(!CcFoodReliefPropose(&sim, &proposal, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    proposal.quantity = CC_SIM_MAX_UNITS; proposal.unit_price = INT32_MAX;
    CC_CHECK(!CcFoodReliefPropose(&sim, &proposal, NULL, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    proposal.quantity = 1; proposal.unit_price = 2;
    CC_CHECK(CcFoodReliefPropose(&sim, &proposal, NULL, NULL, 0));
    CcId id = sim.food_agreements[0].id;
    CcSimSettlementMutable(&sim, place_id)->price[CC_GOOD_FOOD] = 3;
    hash = CcSimHash(&sim);
    CC_CHECK(!CcFoodReliefAccept(&sim, id, beneficiary, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    Setup();
    for (int i = 0; i < CC_MAX_FOOD_AGREEMENTS; ++i) (void)Propose();
    hash = CcSimHash(&sim);
    CC_CHECK(!CcFoodReliefPropose(&sim, &proposal, &outcome, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == hash);
    RoundTrip();
}

int main(void)
{
    Success();
    FailedTerms();
    EvictedOffer();
    RejectedTerms();
    puts("food relief consent, outcomes, conservation, eviction and persistence passed");
    return 0;
}
