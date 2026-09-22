#include "sim/cc_oven_court.h"
#include "sim/cc_production_internal.h"
#include <stdio.h>
#include <string.h>

const CcSettlement *CcOvenCourtPlace(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    for (int32_t i=0; i<sim->settlement_count; ++i)
        if (sim->settlements[i].function == CC_SETTLEMENT_MINING) return &sim->settlements[i];
    return NULL;
}

bool CcOvenCourtRead(const CcSim *sim, CcOvenCourtObservation *out)
{
    const CcSettlement *place = CcOvenCourtPlace(sim);
    if (out == NULL || place == NULL || sim->schema_version < 107U ||
        sim->player.location_id != place->id || sim->journey.active ||
        sim->mine.phase != CC_MINE_NONE || sim->dungeon_expedition.active) return false;
    CcProductionReceipt plan = CcSimPlanBakery(sim, place);
    *out = (CcOvenCourtObservation){.place_id=place->id, .day=sim->current_day,
        .bread=place->stock[CC_GOOD_BREAD], .wheat=place->stock[CC_GOOD_WHEAT],
        .capacity=CcEconomyBakeryCapacity(place), .gate=plan.gate,
        .next_work_day=sim->current_day+7-sim->current_day%7,
        .rebuilding_days=place->service_project == CC_SERVICE_BAKERY ? place->service_project_days : 0,
        .bakery_present=CcSettlementHasService(place, CC_SERVICE_BAKERY),
        .abandoned=CcSettlementIsAbandoned(place)};
    return true;
}

bool CcOvenCourtCanDiscuss(const CcSim *sim, CcId person_id)
{
    CcOvenCourtObservation observation;
    if (!CcOvenCourtRead(sim, &observation) || observation.abandoned) return false;
    const CcCharacter *person = CcSimCharacter(sim, person_id);
    if (person == NULL || person->current_settlement_id != observation.place_id ||
        person->travel_destination_id != 0U || person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
        (person->death_day > 0 && person->death_day <= sim->current_day)) return false;
    /* The local bakery receiver can consult the same public tally. Being a
       generic town resident alone does not grant knowledge of the ovens. */
    CcBakerySupportPlan support = CcSimBakerySupportPlan(sim, observation.place_id);
    return person->id == support.contact_id || person->occupation == CC_OCCUPATION_BAKER;
}

const char *CcOvenCourtStatus(const CcOvenCourtObservation *o)
{
    if (o == NULL) return "No local inspection.";
    if (o->abandoned) return "The public tally is unattended.";
    if (o->rebuilding_days > 0 && !o->bakery_present) return "Bakery rebuilding is unfinished.";
    if (o->gate == CC_PRODUCTION_CAPACITY) return "Baking capacity is absent.";
    if (o->gate == CC_PRODUCTION_INPUT) return o->wheat > 0 ?
        "The Wheat is reserved; none is available for baking." : "Baking needs Wheat.";
    if (o->gate == CC_PRODUCTION_OUTPUT_FULL) return "Bread storage is full.";
    if (o->gate == CC_PRODUCTION_READY) return "Another batch is possible, not yet baked.";
    return "Baking is unavailable; ask locally.";
}

const char *CcOvenCourtAdvice(const CcOvenCourtObservation *o)
{
    if (o == NULL || o->abandoned) return "No one maintains a current tally here. Do not treat old notes as today's stock.";
    if (o->gate == CC_PRODUCTION_CAPACITY) return o->rebuilding_days > 0 ?
        "The building work needs time, not another grain delivery. Bread can be eaten meanwhile; it does not finish the repairs." :
        "More Wheat alone cannot restore baking. Ask at the Company store about bakery support. Bread can be eaten meanwhile.";
    if (o->gate == CC_PRODUCTION_INPUT) return o->wheat > 0 ?
        "The listed Wheat is held in reserve. Check the store's terms before offering more; reserved grain is not a ready batch." :
        "Wheat enables later baking; Bread can be eaten now. Both compete with your road supplies, and Wheat also feeds the team.";
    if (o->gate == CC_PRODUCTION_READY) return o->bread > 0 ?
        "There is Bread here and the next batch has supplies. An old shortage is not today's offer. Check the current price before selling." :
        "The next scheduled batch has supplies. It has not happened yet, and people may eat the output. A delivery is not complete recovery.";
    if (o->gate == CC_PRODUCTION_OUTPUT_FULL) return "The Bread shelves are full. Another sale needs available space and a funded buyer.";
    return "Ask at the Company store before committing supplies. Nothing here guarantees a future sale.";
}

const CcEvent *CcOvenCourtNote(const CcSim *sim, int32_t offset)
{
    if (offset < 0 || offset >= 2) return NULL;
    const CcSettlement *place = CcOvenCourtPlace(sim);
    if (place == NULL) return NULL;
    for (int32_t i=0; i<sim->event_count; ++i) {
        const CcEvent *e = CcSimRecentEvent(sim, i);
        /* Self-addressed Company lore at this site is an acquired field note.
           Other people's accounts/production events never pass this filter. */
        if (e != NULL && e->kind == CC_EVENT_LORE_RECORDED && e->location_id == place->id &&
            e->subject_id == sim->player.id && e->target_id == sim->player.id &&
            e->witness_id == sim->player.id && e->actor_id != 0U && --offset < 0) return e;
    }
    return NULL;
}

bool CcOvenCourtRecord(CcSim *sim, const CcCommand *command, char *error, size_t capacity)
{
    CcOvenCourtObservation o;
    bool read = command != NULL && command->kind == CC_COMMAND_OBSERVE_OVEN_COURT &&
        CcOvenCourtRead(sim, &o) && !o.abandoned;
    bool asked = read && command->amount == 1 && CcOvenCourtCanDiscuss(sim, command->target_id);
    if (!read || (!asked && (command->amount != 0 || command->target_id != o.place_id))) {
        if (error != NULL && capacity > 0) (void)snprintf(error, capacity,
            "Inspect Silverwick's public tally or speak with a present bakery worker.");
        return false;
    }
    const CcCharacter *speaker = asked ? CcSimCharacter(sim, command->target_id) : NULL;
    CcId source = speaker != NULL ? speaker->id : sim->player.id;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%s, day %d: %d Bread, %d Wheat. %s",
        speaker != NULL ? speaker->name : "Oven tally", o.day, o.bread, o.wheat, CcOvenCourtStatus(&o));
    /* Consecutive identical inspections are idempotent. Switching between
       a copied tally and a named explanation is a new acquisition: retain
       its source as the latest note rather than borrowing another speaker's
       event for the response. Reading never changes world time or stocks. */
    {
        const CcEvent *old = CcOvenCourtNote(sim, 0);
        if (old != NULL && old->day == o.day && old->actor_id == source &&
            strcmp(old->text, text) == 0) {
            if (error != NULL && capacity > 0) error[0]='\0';
            return true;
        }
    }
    CcEvent *note = CcSimPushEvent(sim, CC_EVENT_LORE_RECORDED, sim->player.id, o.place_id,
        0U, (int32_t)o.gate, text);
    if (note == NULL) {
        if (error != NULL && capacity > 0) (void)snprintf(error, capacity, "The field note could not be recorded.");
        return false;
    }
    note->actor_id=source;
    note->target_id=sim->player.id;
    note->witness_id=sim->player.id;
    if (error != NULL && capacity > 0) error[0]='\0';
    return true;
}
