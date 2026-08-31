#include "persistence/cc_save.h"
#include "quest/cc_quest.h"
#include "sim/cc_sim.h"

#include <stdio.h>

#define CC_CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n", \
                      __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static CcSituation *FindSituation(CcSim *sim, CcSituationKind kind)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].kind == kind &&
            sim->situations[i].status == CC_SITUATION_ACTIVE) {
            return &sim->situations[i];
        }
    }
    return NULL;
}

static int TestClockIdempotency(void)
{
    CcQuestObjective objective = {0};
    CcQuestClockBegin(&objective.progress, 4, UINT64_C(11));
    CC_CHECK(CcQuestRecordEvidence(&objective, UINT64_C(21), 2));
    CC_CHECK(!CcQuestRecordEvidence(&objective, UINT64_C(21), 2));
    CC_CHECK(objective.progress.value == 2);
    CC_CHECK(CcQuestRecordEvidence(&objective, UINT64_C(22), 3));
    CC_CHECK(objective.progress.value == 4);
    CC_CHECK(objective.progress.resolved_by_event_id == UINT64_C(22));
    CC_CHECK(CcQuestClockPercent(&objective.progress) == 100);
    return 0;
}

static int TestFrontGroupingAndUrgency(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x51a7f00d));
    CC_CHECK(sim.front_count > 0);
    CC_CHECK(CcSimActiveFrontCount(&sim) > 0);
    CcSituation *relief = FindSituation(
        &sim, CC_SITUATION_RELIEF_DELIVERY);
    CcSituation *repair = FindSituation(
        &sim, CC_SITUATION_ROUTE_REPAIR);
    CC_CHECK(relief != NULL && repair != NULL);
    CC_CHECK(relief->front_id == repair->front_id);
    const CcFront *front = CcSimSituationFront(&sim, relief);
    CC_CHECK(front != NULL);
    CC_CHECK(front->kind == CC_FRONT_SUPPLY_CRISIS);
    CC_CHECK(front->situation_count >= 2);
    CC_CHECK(relief->objective.kind ==
             CC_QUEST_OBJECTIVE_DELIVER_GOODS);
    CC_CHECK(repair->objective.kind ==
             CC_QUEST_OBJECTIVE_RESTORE_ROUTE);
    int32_t danger_before = relief->objective.danger.value;
    int32_t portent_before = front->portent.value;
    CcSimAdvanceDays(&sim, 8);
    relief = (CcSituation *)CcSimSituation(&sim, relief->id);
    front = CcSimFront(&sim, repair->front_id);
    CC_CHECK(relief != NULL && front != NULL);
    CC_CHECK(relief->objective.danger.value > danger_before);
    CC_CHECK(front->portent.value > portent_before);
    CC_CHECK(CcSimFrontStage(front) != CC_FRONT_STAGE_CLOSED);
    return 0;
}

static int TestResolutionEvidenceOutcomesAndEchoQueue(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x51a7f00d));
    char error[256];
    CcSituation *repair = FindSituation(
        &sim, CC_SITUATION_ROUTE_REPAIR);
    CC_CHECK(repair != NULL);
    CcId repair_id = repair->id;
    CcId supply_front_id = repair->front_id;
    sim.player.location_id = CcSimSituationOfferSettlementId(&sim, repair);
    sim.carriage.location_id = sim.player.location_id;
    sim.player.cargo[CC_GOOD_TOOLS] = 2;
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = repair_id
    };
    CC_CHECK(CcSimApply(&sim, &accept, error, sizeof(error)));
    CcCommand repair_command = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = repair->target_id,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&sim, &repair_command, error, sizeof(error)));
    repair = (CcSituation *)CcSimSituation(&sim, repair_id);
    CC_CHECK(repair != NULL);
    CC_CHECK(repair->status == CC_SITUATION_RESOLVED);
    CC_CHECK(repair->end_reason == CC_QUEST_END_COMPLETED);
    CC_CHECK(repair->objective.progress.value ==
             repair->objective.progress.limit);
    CC_CHECK(repair->objective.evidence_count == 1);
    CC_CHECK(CcSimEvent(
        &sim, repair->objective.evidence_event_ids[0]) != NULL);
    const CcFront *supply_front = CcSimFront(&sim, supply_front_id);
    CC_CHECK(supply_front != NULL);
    CC_CHECK(supply_front->status == CC_FRONT_RESOLVED);
    CC_CHECK(supply_front->outcome ==
             CC_FRONT_OUTCOME_ROUTE_RESTORED);
    const CcQuestOutcomeRecord *repair_outcome =
        CcSimQuestOutcome(&sim, repair_id);
    CC_CHECK(repair_outcome != NULL);
    CC_CHECK(repair_outcome->end_reason == CC_QUEST_END_COMPLETED);
    CC_CHECK(sim.delayed_echo.active);

    for (int32_t i = 0; i < sim.situation_count; ++i) {
        const CcSituation *other = &sim.situations[i];
        if (other->front_id == supply_front_id && other->id != repair_id) {
            CC_CHECK(other->status == CC_SITUATION_FAILED);
            CC_CHECK(other->end_reason == CC_QUEST_END_INVALIDATED);
            CC_CHECK(CcSimQuestOutcome(&sim, other->id) != NULL);
        }
    }

    CcSituation *monster = FindSituation(
        &sim, CC_SITUATION_MONSTER_EXPEDITION);
    CC_CHECK(monster != NULL);
    CcId monster_id = monster->id;
    CcId monster_target = monster->target_id;
    sim.player.location_id = CcSimSituationOfferSettlementId(&sim, monster);
    sim.carriage.location_id = sim.player.location_id;
    sim.player.cargo[CC_GOOD_TOOLS] = 2;
    sim.player.coins = 500;
    accept.target_id = monster_id;
    CC_CHECK(CcSimApply(&sim, &accept, error, sizeof(error)));
    CcCommand settle_depths = {
        .kind = CC_COMMAND_CHANGE_DUNGEON,
        .target_id = monster_target,
        .dungeon_state = CC_DUNGEON_PUBLIC_ROUTE
    };
    CC_CHECK(CcSimApply(&sim, &settle_depths, error, sizeof(error)));
    monster = (CcSituation *)CcSimSituation(&sim, monster_id);
    CC_CHECK(monster != NULL &&
             monster->status == CC_SITUATION_RESOLVED);
    CC_CHECK(sim.pending_echo_count == 1);
    CC_CHECK(sim.pending_echoes[0].situation_id == monster_id);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));

    const char *path = "/tmp/crownless-quest-system-tests.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == CcSimHash(&sim));
    CC_CHECK(restored.pending_echo_count == 1);
    CC_CHECK(CcSimQuestOutcome(&restored, repair_id) != NULL);
    (void)remove(path);
    return 0;
}

static int TestDangerFailure(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xdab6e2));
    char error[256];
    CcSituation *monster = FindSituation(
        &sim, CC_SITUATION_MONSTER_EXPEDITION);
    CC_CHECK(monster != NULL);
    CcId monster_id = monster->id;
    sim.player.location_id = CcSimSituationOfferSettlementId(&sim, monster);
    sim.carriage.location_id = sim.player.location_id;
    monster->deadline_day = sim.current_day + 1;
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = monster_id
    };
    CC_CHECK(CcSimApply(&sim, &accept, error, sizeof(error)));
    CcSimAdvanceDays(&sim, 2);
    monster = (CcSituation *)CcSimSituation(&sim, monster_id);
    CC_CHECK(monster != NULL);
    CC_CHECK(monster->status == CC_SITUATION_FAILED);
    CC_CHECK(monster->end_reason == CC_QUEST_END_EXPIRED);
    CC_CHECK(monster->objective.danger.value ==
             monster->objective.danger.limit);
    CC_CHECK(monster->objective.danger.resolved_by_event_id != 0U);
    const CcQuestOutcomeRecord *outcome =
        CcSimQuestOutcome(&sim, monster_id);
    CC_CHECK(outcome != NULL);
    CC_CHECK(outcome->end_reason == CC_QUEST_END_EXPIRED);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    return 0;
}

int main(void)
{
    CC_CHECK(TestClockIdempotency() == 0);
    CC_CHECK(TestFrontGroupingAndUrgency() == 0);
    CC_CHECK(TestResolutionEvidenceOutcomesAndEchoQueue() == 0);
    CC_CHECK(TestDangerFailure() == 0);
    (void)printf("Quest fronts and clocks tests passed\n");
    return 0;
}
