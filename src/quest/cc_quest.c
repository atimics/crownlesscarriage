#include "quest/cc_quest.h"

static int32_t ClampClockValue(int64_t value, int32_t limit)
{
    if (value < 0) return 0;
    if (value > limit) return limit;
    return (int32_t)value;
}

CcQuestObjectiveKind CcQuestObjectiveForSituationKind(
    CcSituationKind kind)
{
    switch (kind) {
        case CC_SITUATION_RELIEF_DELIVERY:
        case CC_SITUATION_BLACK_MARKET_DELIVERY:
            return CC_QUEST_OBJECTIVE_DELIVER_GOODS;
        case CC_SITUATION_ROUTE_REPAIR:
            return CC_QUEST_OBJECTIVE_RESTORE_ROUTE;
        case CC_SITUATION_MONSTER_EXPEDITION:
            return CC_QUEST_OBJECTIVE_SETTLE_DUNGEON;
        case CC_SITUATION_COURIER_DELIVERY:
            return CC_QUEST_OBJECTIVE_ESCORT_COURIER;
    }
    return CC_QUEST_OBJECTIVE_DELIVER_GOODS;
}

CcFrontKind CcQuestFrontForSituationKind(CcSituationKind kind)
{
    switch (kind) {
        case CC_SITUATION_MONSTER_EXPEDITION:
            return CC_FRONT_MONSTER_PRESSURE;
        case CC_SITUATION_COURIER_DELIVERY:
            return CC_FRONT_COURIER_DISPATCH;
        case CC_SITUATION_RELIEF_DELIVERY:
        case CC_SITUATION_ROUTE_REPAIR:
        case CC_SITUATION_BLACK_MARKET_DELIVERY:
            return CC_FRONT_SUPPLY_CRISIS;
    }
    return CC_FRONT_SUPPLY_CRISIS;
}

CcFrontOutcome CcQuestOutcomeForSituationKind(CcSituationKind kind)
{
    switch (kind) {
        case CC_SITUATION_RELIEF_DELIVERY:
            return CC_FRONT_OUTCOME_RELIEF_DELIVERED;
        case CC_SITUATION_ROUTE_REPAIR:
            return CC_FRONT_OUTCOME_ROUTE_RESTORED;
        case CC_SITUATION_BLACK_MARKET_DELIVERY:
            return CC_FRONT_OUTCOME_NIGHT_ROAD;
        case CC_SITUATION_MONSTER_EXPEDITION:
            return CC_FRONT_OUTCOME_MONSTER_SETTLED;
        case CC_SITUATION_COURIER_DELIVERY:
            return CC_FRONT_OUTCOME_DISPATCH_DELIVERED;
    }
    return CC_FRONT_OUTCOME_NONE;
}

void CcQuestClockBegin(CcQuestClock *clock, int32_t limit,
                       CcId created_by_event_id)
{
    if (clock == NULL) return;
    *clock = (CcQuestClock){
        .limit = limit > 0 ? limit : 1,
        .created_by_event_id = created_by_event_id
    };
}

bool CcQuestClockContainsEvidence(const CcQuestObjective *objective,
                                  CcId event_id)
{
    if (objective == NULL || event_id == 0U) return false;
    for (int32_t i = 0; i < objective->evidence_count; ++i) {
        if (objective->evidence_event_ids[i] == event_id) return true;
    }
    return false;
}

bool CcQuestRecordEvidence(CcQuestObjective *objective, CcId event_id,
                           int32_t amount)
{
    if (objective == NULL || event_id == 0U || amount <= 0 ||
        CcQuestClockContainsEvidence(objective, event_id) ||
        objective->evidence_count >= CC_MAX_QUEST_EVIDENCE) return false;
    objective->evidence_event_ids[objective->evidence_count++] = event_id;
    objective->progress.value = ClampClockValue(
        (int64_t)objective->progress.value + amount,
        objective->progress.limit);
    if (objective->progress.value >= objective->progress.limit &&
        objective->progress.resolved_by_event_id == 0U) {
        objective->progress.resolved_by_event_id = event_id;
    }
    return true;
}

void CcQuestAdvanceDanger(CcQuestObjective *objective, int32_t amount)
{
    if (objective == NULL || amount <= 0) return;
    objective->danger.value = ClampClockValue(
        (int64_t)objective->danger.value + amount,
        objective->danger.limit);
}

int32_t CcQuestClockPercent(const CcQuestClock *clock)
{
    if (clock == NULL || clock->limit <= 0) return 0;
    int64_t percent = (int64_t)clock->value * 100 / clock->limit;
    if (percent < 0) return 0;
    if (percent > 100) return 100;
    return (int32_t)percent;
}
