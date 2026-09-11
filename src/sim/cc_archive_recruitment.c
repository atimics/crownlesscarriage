#include "sim/cc_archive_recruitment.h"
#include "sim/cc_archive_internal.h"
#include "sim/cc_trade_path_internal.h"

static bool Available(const CcSim *sim, const CcCharacter *person)
{
    if (person == NULL || person->death_day <= sim->current_day ||
        CcCharacterAgeYears(sim, person) < 16 || person->bandit_group_id != 0 ||
        person->activity != CC_CHARACTER_ACTIVITY_WORKING) return false;
    const CcSettlement *place = CcSimSettlement(sim, person->current_settlement_id);
    if (place == NULL || CcSettlementIsAbandoned(place)) return false;
    for (int32_t i = 0; i < sim->kingdom_count; ++i)
        if (sim->kingdoms[i].ruler_character_id == person->id ||
            sim->kingdoms[i].monastery_patron_id == person->id) return false;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *s = &sim->situations[i];
        if (s->status == CC_SITUATION_ACTIVE &&
            (s->sponsor_character_id == person->id || s->affected_character_id == person->id ||
             s->witness_character_id == person->id)) return false;
    }
    return true;
}

/* Until work orders store staff IDs, assign the occupied places to available
   local scribes in stable ID order. The quote reports its named trainer. */
static bool Incumbent(const CcSim *sim, CcId seat, const CcCharacter *person)
{
    if (person->occupation != CC_OCCUPATION_SCRIBE || person->current_settlement_id != seat ||
        !Available(sim, person)) return false;
    int32_t earlier = 0;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *other = &sim->characters[i];
        if (other->id < person->id && other->occupation == CC_OCCUPATION_SCRIBE &&
            other->current_settlement_id == seat && Available(sim, other)) ++earlier;
    }
    return earlier < sim->archives.scribes;
}

static CcId Trainer(const CcSim *sim, CcId seat, CcId trainee)
{
    if (sim->archives.scribes <= 0) return 0;
    CcId result = 0;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (person->id != trainee && person->current_settlement_id == seat &&
            Incumbent(sim, seat, person) &&
            (result == 0 || person->id < result)) result = person->id;
    }
    return result;
}

static bool Journey(const CcSim *sim, CcArchiveRecruitmentPlan *plan)
{
    CcId place = plan->origin_id;
    CcId visited[CC_MAX_SETTLEMENTS] = {0};
    for (int32_t leg = 0; leg < sim->settlement_count; ++leg) {
        if (place == plan->seat_id) return true;
        for (int32_t i = 0; i < leg; ++i) if (visited[i] == place) return false;
        visited[leg] = place;
        int32_t route_slot = -1;
        CcId next = 0;
        /* The same path policy as StartCourierLeg. */
        if (!CcTradeFindPath(sim, place, plan->seat_id, CC_GOOD_FOOD,
            &route_slot, &next, NULL, NULL, NULL, true, 0, false, 1)) return false;
        const CcRoute *route = &sim->routes[route_slot];
        if (leg == 0) { plan->first_route_id = route->id; plan->first_hop_id = next; }
        plan->travel_days += route->travel_days;
        plan->dangerous |= route->closed || CcSimRouteCrossesWarBorder(sim, route->id) ||
            CcSimRouteDanger(sim, route->id) >= 50;
        place = next;
    }
    return place == plan->seat_id;
}

static CcId Patron(const CcSim *sim, CcId kingdom_id)
{
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *kingdom = &sim->kingdoms[i];
        if (kingdom->id != kingdom_id) continue;
        CcId ids[2] = {kingdom->monastery_patron_id, kingdom->ruler_character_id};
        for (int32_t j = 0; j < 2; ++j) {
            const CcCharacter *person = CcSimCharacter(sim, ids[j]);
            const CcSettlement *home = person != NULL ?
                CcSimSettlement(sim, person->home_settlement_id) : NULL;
            if (person != NULL && person->death_day > sim->current_day &&
                CcCharacterAgeYears(sim, person) >= 16 && home != NULL &&
                home->kingdom_id == kingdom_id) return person->id;
        }
    }
    return 0;
}

static CcArchiveRecruitmentPlan Candidate(const CcSim *sim, CcId seat,
                                         const CcCharacter *person)
{
    CcArchiveRecruitmentPlan plan = {.seat_id = seat, .person_id = person->id,
        .origin_id = person->current_settlement_id, .wages = 50, .tools = 1};
    plan.training = person->occupation != CC_OCCUPATION_SCRIBE;
    plan.training_days = plan.training ? 28 : 7;
    plan.trainer_days = plan.training ? 28 : 0;
    plan.first_task_wheat = 2;
    plan.first_task_paper = 1;
    plan.paper = (plan.training ? 4 : 1) + plan.first_task_paper;
    plan.wheat = (plan.training ? 16 : 2) + plan.first_task_wheat;
    if (plan.training) {
        plan.trainer_id = Trainer(sim, seat, person->id);
        if (plan.origin_id != seat || plan.trainer_id == 0) {
            plan.gate = CC_ARCHIVE_RECRUIT_TRAINER;
            return plan;
        }
    }
    if (!Journey(sim, &plan)) { plan.gate = CC_ARCHIVE_RECRUIT_ROUTE; return plan; }
    plan.travel_wheat = 2 * ((plan.travel_days + 6) / 7);
    plan.arrival_day = (int64_t)sim->current_day + plan.travel_days;
    plan.ready_day = plan.arrival_day + plan.training_days;
    if (plan.ready_day > CC_SIM_MAX_DAY) { plan.gate = CC_ARCHIVE_RECRUIT_CALENDAR; return plan; }
    const CcSettlement *origin = CcSimSettlement(sim, plan.origin_id);
    if (CcArchiveSpareGrain(sim, origin) < plan.travel_wheat) {
        plan.gate = CC_ARCHIVE_RECRUIT_TRAVEL_FOOD; return plan;
    }
    const CcSettlement *archive = CcSimSettlement(sim, seat);
    if (CcArchiveSpareGrain(sim, archive) < plan.wheat ||
        archive->stock[CC_GOOD_PAPER] < plan.paper || archive->stock[CC_GOOD_TOOLS] < plan.tools) {
        plan.gate = CC_ARCHIVE_RECRUIT_MATERIALS; return plan;
    }
    if (sim->iron_ledger_reserve >= plan.wages) return plan;
    plan.recovery = true;
    if (CcSimArchiveRecoveryWindow(sim).gate != CC_ARCHIVE_RECOVERY_DUE) {
        plan.gate = CC_ARCHIVE_RECRUIT_SILENCE; return plan;
    }
    plan.funding = CcSimArchiveFundingPlan(sim);
    if (plan.funding.blocker != CC_ARCHIVE_FUNDING_READY ||
        plan.funding.total + sim->iron_ledger_reserve < plan.wages) {
        plan.gate = CC_ARCHIVE_RECRUIT_FUNDS; return plan;
    }
    for (int32_t i = 0; i < plan.funding.donor_count; ++i) {
        plan.patron_ids[i] = Patron(sim, plan.funding.donor_ids[i]);
        if (plan.patron_ids[i] == 0) { plan.gate = CC_ARCHIVE_RECRUIT_PATRON; return plan; }
    }
    return plan;
}

CcArchiveRecruitmentPlan CcSimArchiveRecruitmentPlan(const CcSim *sim)
{
    CcArchiveRecruitmentPlan result = {.gate = CC_ARCHIVE_RECRUIT_UNAVAILABLE};
    if (sim == NULL || sim->schema_version < 77U) return result;
    if (sim->archives.scribes >= CC_MAX_SCRIBES) { result.gate = CC_ARCHIVE_RECRUIT_FULL; return result; }
    const CcSettlement *seat = CcArchiveSeat(sim);
    result.gate = CC_ARCHIVE_RECRUIT_SEAT;
    if (seat == NULL || !CcSimArchiveSeatCandidate(sim, seat->id).viable) return result;
    result.seat_id = seat->id;
    result.gate = CC_ARCHIVE_RECRUIT_CANDIDATE;
    bool found = false;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (!Available(sim, person) || Incumbent(sim, seat->id, person)) continue;
        CcArchiveRecruitmentPlan plan = Candidate(sim, seat->id, person);
        /* Ready plans lead, then trained hires, then the earliest completion.
           A stable ID resolves equal quotes and keeps blocked reasons stable. */
        bool better = !found ||
            ((plan.gate == CC_ARCHIVE_RECRUIT_READY) != (result.gate == CC_ARCHIVE_RECRUIT_READY) ?
                plan.gate == CC_ARCHIVE_RECRUIT_READY :
             plan.training != result.training ? !plan.training :
             plan.ready_day != result.ready_day ? plan.ready_day < result.ready_day :
                plan.person_id < result.person_id);
        if (better) { result = plan; found = true; }
    }
    return result;
}

const char *CcArchiveRecruitmentGateName(CcArchiveRecruitmentGate gate)
{
    static const char *const names[] = {"ready", "unavailable", "full", "seat", "candidate",
        "trainer", "route", "travel_food", "materials", "silence", "funds", "patron", "calendar"};
    return gate >= CC_ARCHIVE_RECRUIT_READY && gate <= CC_ARCHIVE_RECRUIT_CALENDAR ?
        names[gate] : "unknown";
}
