#ifndef CROWNLESS_QUEST_H
#define CROWNLESS_QUEST_H

#include "sim/cc_sim.h"

CcQuestObjectiveKind CcQuestObjectiveForSituationKind(
    CcSituationKind kind);
CcFrontKind CcQuestFrontForSituationKind(CcSituationKind kind);
CcFrontOutcome CcQuestOutcomeForSituationKind(CcSituationKind kind);

void CcQuestClockBegin(CcQuestClock *clock, int32_t limit,
                       CcId created_by_event_id);
bool CcQuestClockContainsEvidence(const CcQuestObjective *objective,
                                  CcId event_id);
bool CcQuestRecordEvidence(CcQuestObjective *objective, CcId event_id,
                           int32_t amount);
void CcQuestAdvanceDanger(CcQuestObjective *objective, int32_t amount);
int32_t CcQuestClockPercent(const CcQuestClock *clock);

#endif
