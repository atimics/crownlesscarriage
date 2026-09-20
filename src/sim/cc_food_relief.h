#ifndef CROWNLESS_FOOD_RELIEF_H
#define CROWNLESS_FOOD_RELIEF_H

#include "sim/cc_sim.h"

typedef struct CcFoodReliefObservation {
    CcId payer_id;
    CcId beneficiary_id;
    CcId place_id;
    char place_name[CC_NAME_CAPACITY];
    int32_t stock;
    int32_t reserve_target;
    int32_t unit_price;
    int32_t day;
    CcMoney payer_coins;
    int32_t beneficiary_hungry_days;
} CcFoodReliefObservation;

typedef struct CcFoodReliefProposal {
    CcId payer_id;
    CcId beneficiary_id;
    CcId place_id;
    int32_t quantity;
    int32_t unit_price;
} CcFoodReliefProposal;

typedef enum CcFoodReliefOutcomeKind {
    CC_FOOD_RELIEF_OUTCOME_NONE,
    CC_FOOD_RELIEF_OUTCOME_PROPOSED,
    CC_FOOD_RELIEF_OUTCOME_ACCEPTED,
    CC_FOOD_RELIEF_OUTCOME_FULFILLED,
    CC_FOOD_RELIEF_OUTCOME_FAILED
} CcFoodReliefOutcomeKind;

typedef struct CcFoodReliefOutcome {
    CcFoodReliefOutcomeKind kind;
    CcId agreement_id;
    CcId payer_id;
    CcId beneficiary_id;
    CcId place_id;
    int32_t quantity;
    int32_t unit_price;
    int32_t beneficiary_hungry_days;
    CcMoney total_cost;
    CcId event_id;
} CcFoodReliefOutcome;

bool CcFoodReliefObserve(const CcSim *sim, CcId payer_id,
                         CcId beneficiary_id, CcFoodReliefObservation *out,
                         char *error, size_t error_capacity);
bool CcFoodReliefPropose(CcSim *sim, const CcFoodReliefProposal *proposal,
                         CcFoodReliefOutcome *out, char *error,
                         size_t error_capacity);
bool CcFoodReliefAccept(CcSim *sim, CcId agreement_id, CcId beneficiary_id,
                        CcFoodReliefOutcome *out, char *error,
                        size_t error_capacity);
bool CcFoodReliefExecute(CcSim *sim, CcId agreement_id, CcId payer_id,
                         CcFoodReliefOutcome *out, char *error,
                         size_t error_capacity);
bool CcFoodReliefRead(const CcSim *sim, CcId agreement_id,
                      CcFoodReliefOutcome *out);

#endif
