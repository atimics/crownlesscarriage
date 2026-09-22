#ifndef CC_OVEN_COURT_H
#define CC_OVEN_COURT_H
#include "sim/cc_production.h"

/* Public local tally, not remote stock intelligence or evidence of a past batch.
   This transient read model is never a second economic state machine. */
typedef struct CcOvenCourtObservation {
    CcId place_id;
    int32_t day, bread, wheat, capacity, next_work_day, rebuilding_days;
    CcProductionGate gate;
    bool bakery_present, abandoned;
} CcOvenCourtObservation;

const CcSettlement *CcOvenCourtPlace(const CcSim *sim);
bool CcOvenCourtRead(const CcSim *sim, CcOvenCourtObservation *out);
bool CcOvenCourtCanDiscuss(const CcSim *sim, CcId person_id);
const char *CcOvenCourtStatus(const CcOvenCourtObservation *observation);
const char *CcOvenCourtAdvice(const CcOvenCourtObservation *observation);
/* Explicit learning only. amount 0 reads the town tally; amount 1 asks a
   present local worker at their ordinary conversation target. */
bool CcOvenCourtRecord(CcSim *sim, const CcCommand *command, char *error, size_t capacity);
/* Two retained, attributed Company Book notes from the existing event tape.
   Only self-addressed, witnessed lore is visible here, never hidden history. */
const CcEvent *CcOvenCourtNote(const CcSim *sim, int32_t offset);
#endif
