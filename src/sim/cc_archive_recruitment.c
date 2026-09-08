#include "sim/cc_archive_recruitment.h"
#include "sim/cc_archive_internal.h"
#include "sim/cc_food_economy_internal.h"
#include "sim/cc_identity_internal.h"
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
    if (sim->schema_version >= 78U && sim->archive_recruitment.status != 0) {
        result.gate = CC_ARCHIVE_RECRUIT_BUSY; return result;
    }
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
        "trainer", "route", "travel_food", "materials", "silence", "funds", "patron", "busy", "calendar"};
    return gate >= CC_ARCHIVE_RECRUIT_READY && gate <= CC_ARCHIVE_RECRUIT_CALENDAR ?
        names[gate] : "unknown";
}

bool CcSimBeginArchiveRecruitment(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 78U) return false;
    CcArchiveRecruitmentPlan plan = CcSimArchiveRecruitmentPlan(sim);
    if (plan.gate != CC_ARCHIVE_RECRUIT_READY) return false;
    CcArchiveRecruitmentOrder order = {.status = 1, .person_id = plan.person_id,
        .trainer_id = plan.trainer_id, .seat_id = plan.seat_id, .origin_id = plan.origin_id,
        .first_route_id = plan.first_route_id, .first_hop_id = plan.first_hop_id,
        .purse = plan.wages, .wheat = plan.wheat, .paper = plan.paper, .tools = plan.tools,
        .travel_wheat = plan.travel_wheat, .start_day = sim->current_day,
        .training_days = plan.training_days, .trainer_days = plan.trainer_days,
        .arrival_estimate = plan.arrival_day, .ready_estimate = plan.ready_day};
    for (int32_t i = 0; i < plan.funding.donor_count; ++i) {
        order.donor_ids[i] = plan.funding.donor_ids[i];
        order.patron_ids[i] = plan.patron_ids[i];
        order.donor_shares[i] = plan.funding.shares[i];
        for (int32_t k = 0; k < sim->kingdom_count; ++k)
            if (sim->kingdoms[k].id == order.donor_ids[i])
                sim->kingdoms[k].treasury -= order.donor_shares[i];
    }
    sim->iron_ledger_reserve += plan.funding.total - plan.wages;
    CcSettlement *seat = CcSimSettlementMutable(sim, plan.seat_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, plan.origin_id);
    seat->stock[CC_GOOD_WHEAT] -= plan.wheat;
    seat->stock[CC_GOOD_PAPER] -= plan.paper;
    seat->stock[CC_GOOD_TOOLS] -= plan.tools;
    origin->stock[CC_GOOD_WHEAT] -= plan.travel_wheat;
    CcEconomyRefreshSettlementGoodPrice(sim, seat, CC_GOOD_WHEAT);
    CcEconomyRefreshSettlementGoodPrice(sim, seat, CC_GOOD_PAPER);
    CcEconomyRefreshSettlementGoodPrice(sim, seat, CC_GOOD_TOOLS);
    CcEconomyRefreshSettlementGoodPrice(sim, origin, CC_GOOD_WHEAT);
    sim->archive_recruitment = order;
    return true;
}

static bool JourneyValid(const CcSim *sim)
{
    if (sim->schema_version < 79U) return true;
    const CcArchiveRecruitmentOrder *o = &sim->archive_recruitment;
    if (o->status == 0) return o->current_id == 0 && o->leg_route_id == 0 &&
        o->leg_hop_id == 0 && o->leg_arrival_day == 0 && o->provisioned_days == 0 && o->arrived_day == 0;
    if ((o->current_id != 0 && CcSimSettlement(sim, o->current_id) == NULL) ||
        o->provisioned_days < 0 || o->provisioned_days > CC_SIM_MAX_DAY ||
        o->arrived_day < 0 || o->arrived_day > sim->current_day) return false;
    if (o->status == 2) {
        const CcRoute *route = CcSimRoute(sim, o->leg_route_id);
        if (route == NULL || o->current_id == 0 || o->leg_hop_id == o->current_id ||
            !((route->from_id == o->current_id && route->to_id == o->leg_hop_id) ||
              (route->to_id == o->current_id && route->from_id == o->leg_hop_id)) ||
            o->leg_arrival_day < sim->current_day || o->leg_arrival_day > CC_SIM_MAX_DAY ||
            o->provisioned_days == 0 || o->arrived_day != 0) return false;
    } else if (o->leg_route_id != 0 || o->leg_hop_id != 0 || o->leg_arrival_day != 0) return false;
    if (o->status == 3) return o->current_id == o->seat_id && o->arrived_day >= o->start_day;
    return o->arrived_day == 0;
}

bool CcSimArchiveRecruitmentOrderValid(const CcSim *sim)
{
    if (sim == NULL) return false;
    if (sim->schema_version < 78U) return true;
    const CcArchiveRecruitmentOrder *o = &sim->archive_recruitment;
    if (!JourneyValid(sim)) return false;
    if (o->status == 0) {
        return o->person_id == 0 &&
            o->trainer_id == 0 &&
            o->seat_id == 0 &&
            o->origin_id == 0 &&
            o->first_route_id == 0 &&
            o->first_hop_id == 0 &&
            o->donor_ids[0] == 0 &&
            o->donor_ids[1] == 0 &&
            o->patron_ids[0] == 0 &&
            o->patron_ids[1] == 0 &&
            o->donor_shares[0] == 0 &&
            o->donor_shares[1] == 0 &&
            o->purse == 0 &&
            o->wheat == 0 &&
            o->paper == 0 &&
            o->tools == 0 &&
            o->travel_wheat == 0 &&
            o->start_day == 0 &&
            o->training_days == 0 &&
            o->trainer_days == 0 &&
            o->arrival_estimate == 0 &&
            o->ready_estimate == 0;
    }
    if ((o->status < 1 || o->status > (sim->schema_version >= 79U ? 4 : 1)) || CcIdKind(o->person_id) != CC_ENTITY_CHARACTER ||
        (o->person_id & CC_ID_SERIAL_MASK) == 0 ||
        (o->person_id & CC_ID_SERIAL_MASK) >= sim->next_entity_serial ||
        (o->trainer_id != 0 && (CcIdKind(o->trainer_id) != CC_ENTITY_CHARACTER ||
         (o->trainer_id & CC_ID_SERIAL_MASK) == 0 ||
         (o->trainer_id & CC_ID_SERIAL_MASK) >= sim->next_entity_serial || o->trainer_id == o->person_id)) ||
        CcSimSettlement(sim, o->seat_id) == NULL || CcSimSettlement(sim, o->origin_id) == NULL ||
        (o->first_route_id != 0 && CcSimRoute(sim, o->first_route_id) == NULL) ||
        (o->first_hop_id != 0 && CcSimSettlement(sim, o->first_hop_id) == NULL) ||
        o->purse < 0 || o->purse > 50 || o->wheat < 0 || o->wheat > 18 ||
        o->paper < 0 || o->paper > 5 || o->tools < 0 || o->tools > 1 ||
        o->travel_wheat < 0 || o->travel_wheat > CC_SIM_MAX_UNITS ||
        o->start_day < 1 || o->start_day > sim->current_day ||
        (o->training_days != 7 && o->training_days != 28) ||
        o->trainer_days < 0 || o->trainer_days > 28 ||
        o->arrival_estimate < o->start_day || o->arrival_estimate > CC_SIM_MAX_DAY ||
        o->ready_estimate < o->arrival_estimate || o->ready_estimate > CC_SIM_MAX_DAY) return false;
    if (o->donor_shares[0] < 0 || o->donor_shares[0] > 50 ||
        o->donor_shares[1] < 0 || o->donor_shares[1] > 50 ||
        o->donor_shares[0] + o->donor_shares[1] > o->purse ||
        (o->donor_ids[0] != 0 && o->donor_ids[0] == o->donor_ids[1])) return false;
    for (int32_t i = 0; i < 2; ++i) {
        if (o->donor_ids[i] == 0) {
            if (o->patron_ids[i] != 0 || o->donor_shares[i] != 0) return false;
            continue;
        }
        bool donor_exists = false;
        for (int32_t k = 0; k < sim->kingdom_count; ++k)
            if (sim->kingdoms[k].id == o->donor_ids[i]) donor_exists = true;
        if (!donor_exists ||
            o->patron_ids[i] == 0 || CcIdKind(o->patron_ids[i]) != CC_ENTITY_CHARACTER ||
            (o->patron_ids[i] & CC_ID_SERIAL_MASK) == 0 ||
            (o->patron_ids[i] & CC_ID_SERIAL_MASK) >= sim->next_entity_serial ||
            o->donor_shares[i] <= 0 || o->donor_shares[i] > 50) return false;
    }
    return true;
}

bool CcSimCancelArchiveRecruitment(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 78U || (sim->archive_recruitment.status != 1 &&
         !(sim->schema_version >= 79U && (sim->archive_recruitment.status == 3 ||
           sim->archive_recruitment.status == 4))) ||
        !CcSimArchiveRecruitmentOrderValid(sim)) return false;
    const CcArchiveRecruitmentOrder *o = &sim->archive_recruitment;
    CcSettlement *seat = CcSimSettlementMutable(sim, o->seat_id);
    CcSettlement *origin = CcSimSettlementMutable(sim,
        sim->schema_version >= 79U && o->current_id != 0 ? o->current_id : o->origin_id);
    CcMoney donors = o->donor_shares[0] + o->donor_shares[1];
    if (donors > o->purse || sim->iron_ledger_reserve > CC_SIM_MAX_MONEY - (o->purse - donors)) return false;
    int32_t seat_wheat = o->wheat + (origin == seat ? o->travel_wheat : 0);
    if (seat->stock[CC_GOOD_WHEAT] > CC_SIM_MAX_UNITS - seat_wheat ||
        seat->stock[CC_GOOD_PAPER] > CC_SIM_MAX_UNITS - o->paper ||
        seat->stock[CC_GOOD_TOOLS] > CC_SIM_MAX_UNITS - o->tools ||
        (origin != seat && origin->stock[CC_GOOD_WHEAT] > CC_SIM_MAX_UNITS - o->travel_wheat)) return false;
    for (int32_t i = 0; i < 2; ++i)
        for (int32_t k = 0; k < sim->kingdom_count; ++k)
            if (sim->kingdoms[k].id == o->donor_ids[i] &&
                sim->kingdoms[k].treasury > CC_SIM_MAX_MONEY - o->donor_shares[i]) return false;
    for (int32_t i = 0; i < 2; ++i)
        for (int32_t k = 0; k < sim->kingdom_count; ++k)
            if (sim->kingdoms[k].id == o->donor_ids[i]) sim->kingdoms[k].treasury += o->donor_shares[i];
    sim->iron_ledger_reserve += o->purse - donors;
    seat->stock[CC_GOOD_WHEAT] += o->wheat;
    seat->stock[CC_GOOD_PAPER] += o->paper;
    seat->stock[CC_GOOD_TOOLS] += o->tools;
    origin->stock[CC_GOOD_WHEAT] += o->travel_wheat;
    CcEconomyRefreshSettlementGoodPrice(sim, seat, CC_GOOD_WHEAT);
    CcEconomyRefreshSettlementGoodPrice(sim, seat, CC_GOOD_PAPER);
    CcEconomyRefreshSettlementGoodPrice(sim, seat, CC_GOOD_TOOLS);
    CcEconomyRefreshSettlementGoodPrice(sim, origin, CC_GOOD_WHEAT);
    sim->archive_recruitment = (CcArchiveRecruitmentOrder){0};
    return true;
}

static CcCharacter *Recruit(CcSim *sim)
{
    for (int32_t i = 0; i < sim->character_count; ++i)
        if (sim->characters[i].id == sim->archive_recruitment.person_id) return &sim->characters[i];
    return NULL;
}

CcArchiveRecruitmentGate CcSimArchiveRecruitmentJourneyGate(const CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 79U || sim->archive_recruitment.status == 0)
        return CC_ARCHIVE_RECRUIT_UNAVAILABLE;
    const CcArchiveRecruitmentOrder *o = &sim->archive_recruitment;
    const CcCharacter *person = CcSimCharacter(sim, o->person_id);
    if (person == NULL || person->death_day <= sim->current_day)
        return CC_ARCHIVE_RECRUIT_CANDIDATE;
    if (o->status == 4) return CC_ARCHIVE_RECRUIT_TRAVEL_FOOD;
    if (o->status == 2 || o->status == 3) return CC_ARCHIVE_RECRUIT_BUSY;
    CcId current = o->current_id != 0 ? o->current_id : o->origin_id;
    if (!Available(sim, person) || person->current_settlement_id != current)
        return CC_ARCHIVE_RECRUIT_CANDIDATE;
    const CcSettlement *seat = CcSimSettlement(sim, o->seat_id);
    if (seat == NULL || CcSettlementIsAbandoned(seat)) return CC_ARCHIVE_RECRUIT_SEAT;
    if (current == o->seat_id) return CC_ARCHIVE_RECRUIT_READY;
    int32_t slot = -1;
    if (!CcTradeFindPath(sim, current, o->seat_id, CC_GOOD_FOOD, &slot, NULL,
        NULL, NULL, NULL, true, 0, false, 1)) return CC_ARCHIVE_RECRUIT_ROUTE;
    int64_t days = (int64_t)o->provisioned_days + sim->routes[slot].travel_days;
    if (days > CC_SIM_MAX_DAY || (int64_t)sim->current_day + sim->routes[slot].travel_days > CC_SIM_MAX_DAY)
        return CC_ARCHIVE_RECRUIT_CALENDAR;
    int64_t food = 2 * ((days + 6) / 7 - ((int64_t)o->provisioned_days + 6) / 7);
    if (o->travel_wheat < food) return CC_ARCHIVE_RECRUIT_TRAVEL_FOOD;
    return CC_ARCHIVE_RECRUIT_READY;
}

CcArchiveJourneyStep CcSimAdvanceArchiveRecruitmentJourney(CcSim *sim, uint32_t road_roll)
{
    if (sim == NULL || sim->schema_version < 79U) return CC_ARCHIVE_JOURNEY_WAIT;
    CcArchiveRecruitmentOrder *o = &sim->archive_recruitment;
    if (o->status != 1 && o->status != 2) return CC_ARCHIVE_JOURNEY_WAIT;
    CcCharacter *person = Recruit(sim);
    if (person == NULL || person->death_day <= sim->current_day) {
        if (o->status == 2) o->travel_wheat = 0;
        o->status = 4;
        o->leg_route_id = o->leg_hop_id = 0;
        o->leg_arrival_day = 0;
        return CC_ARCHIVE_JOURNEY_FAILED;
    }
    if (o->status == 2) {
        if (sim->current_day < o->leg_arrival_day) return CC_ARCHIVE_JOURNEY_WAIT;
        int32_t danger = CcSimRouteDanger(sim, o->leg_route_id);
        o->current_id = o->leg_hop_id;
        person->current_settlement_id = o->current_id;
        person->activity = CC_CHARACTER_ACTIVITY_WORKING;
        o->leg_route_id = o->leg_hop_id = 0;
        o->leg_arrival_day = 0;
        o->status = 1;
        /* Match the courier's bounded road-risk threshold. The recruit reaches
           shelter after an attack; the remaining provisions are lost. */
        if (road_roll % 100U < (uint32_t)(danger / 5)) {
            o->status = 4;
            o->travel_wheat = 0;
            person->activity = CC_CHARACTER_ACTIVITY_RECOVERING;
            return CC_ARCHIVE_JOURNEY_FAILED;
        }
        if (o->current_id != o->seat_id) return CC_ARCHIVE_JOURNEY_STOP;
    }
    if (CcSimArchiveRecruitmentJourneyGate(sim) != CC_ARCHIVE_RECRUIT_READY)
        return CC_ARCHIVE_JOURNEY_WAIT;
    if (o->current_id == 0) o->current_id = o->origin_id;
    if (o->current_id == o->seat_id) {
        o->status = 3;
        o->arrived_day = sim->current_day;
        return CC_ARCHIVE_JOURNEY_ARRIVED;
    }
    int32_t slot = -1; CcId hop = 0;
    if (!CcTradeFindPath(sim, o->current_id, o->seat_id, CC_GOOD_FOOD, &slot, &hop,
        NULL, NULL, NULL, true, 0, false, 1)) return CC_ARCHIVE_JOURNEY_WAIT;
    const CcRoute *route = &sim->routes[slot];
    int32_t days = o->provisioned_days + route->travel_days;
    o->travel_wheat -= 2 * ((days + 6) / 7 - (o->provisioned_days + 6) / 7);
    o->provisioned_days = days;
    o->leg_route_id = route->id;
    o->leg_hop_id = hop;
    o->leg_arrival_day = sim->current_day + route->travel_days;
    o->status = 2;
    person->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    return CC_ARCHIVE_JOURNEY_DEPARTED;
}
