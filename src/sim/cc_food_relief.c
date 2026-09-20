#include "sim/cc_food_relief.h"
#include "sim/cc_food_economy_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void Error(char *error, size_t capacity, const char *text)
{
    if (error != NULL && capacity > 0U) (void)snprintf(error, capacity, "%s", text);
}

static CcEvent *Find(const CcSim *sim, CcId id)
{
    if (sim == NULL || id == 0U) return NULL;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        CcEvent *event = (CcEvent *)&sim->events[i];
        if (event->id == id && event->kind == CC_EVENT_RELIEF &&
            event->subject_id == id) return event;
    }
    return NULL;
}

static CcFoodAgreement *Agreement(CcSim *sim, CcId id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->food_agreement_count; ++i) {
        if (sim->food_agreements[i].id == id) return &sim->food_agreements[i];
    }
    return NULL;
}

static CcEvent *Child(const CcSim *sim, CcId agreement, CcId actor,
                      const char *prefix)
{
    if (sim == NULL) return NULL;
    for (int32_t i = sim->event_count - 1; i >= 0; --i) {
        CcEvent *event = (CcEvent *)&sim->events[i];
        if (event->kind == CC_EVENT_RELIEF && event->parent_id == agreement &&
            event->actor_id == actor && strncmp(event->text, prefix, strlen(prefix)) == 0) {
            return event;
        }
    }
    return NULL;
}

static bool Parse(const CcEvent *event, CcFoodReliefOutcome *out)
{
    int price;
    if (event == NULL || out == NULL || event->actor_id == 0U ||
        event->target_id == 0U || event->location_id == 0U ||
        event->witness_id == 0U || event->witness_id > INT32_MAX ||
        event->magnitude <= 0) return false;
    price = (int)event->witness_id;
    *out = (CcFoodReliefOutcome){
        .kind = CC_FOOD_RELIEF_OUTCOME_PROPOSED, .agreement_id = event->id,
        .payer_id = event->actor_id, .beneficiary_id = event->target_id,
        .place_id = event->location_id, .quantity = event->magnitude,
        .unit_price = price, .total_cost = (CcMoney)event->magnitude * price,
        .event_id = event->id};
    return true;
}

static void Remember(CcCharacter *person, CcCharacterMemoryKind kind,
                     CcId agreement, CcId event_id, int32_t day)
{
    if (person == NULL || CcCharacterRemembers(person, kind, agreement)) return;
    int32_t slot = person->memory_write_index;
    if (slot < 0 || slot >= CC_CHARACTER_MEMORY_CAPACITY) slot = 0;
    person->memories[slot] = (CcCharacterMemory){kind, agreement, event_id, day};
    person->memory_write_index = (slot + 1) % CC_CHARACTER_MEMORY_CAPACITY;
    if (person->memory_count < CC_CHARACTER_MEMORY_CAPACITY) person->memory_count += 1;
}

static CcRelationship *Relation(CcSim *sim, CcId from, CcId to, CcId event_id)
{
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        if (sim->relationships[i].from_character_id == from &&
            sim->relationships[i].to_character_id == to) return &sim->relationships[i];
    }
    if (sim->relationship_count >= CC_MAX_RELATIONSHIPS) return NULL;
    CcRelationship *relation = &sim->relationships[sim->relationship_count++];
    *relation = (CcRelationship){from, to, 0, 0, 0,
        CC_RELATIONSHIP_HISTORY_COWORKERS, event_id};
    return relation;
}

bool CcFoodReliefObserve(const CcSim *sim, CcId payer_id, CcId beneficiary_id,
                         CcFoodReliefObservation *out, char *error, size_t capacity)
{
    const CcCharacter *payer = CcSimCharacter(sim, payer_id);
    const CcCharacter *beneficiary = CcSimCharacter(sim, beneficiary_id);
    const CcSettlement *place;
    if (out == NULL || payer == NULL || beneficiary == NULL || payer_id == beneficiary_id ||
        payer->current_settlement_id == 0U || payer->current_settlement_id != beneficiary->current_settlement_id) {
        Error(error, capacity, "Payer and beneficiary must be distinct people in one place."); return false;
    }
    place = CcSimSettlement(sim, payer->current_settlement_id);
    if (place == NULL) { Error(error, capacity, "The local food store is unavailable."); return false; }
    *out = (CcFoodReliefObservation){.payer_id = payer_id, .beneficiary_id = beneficiary_id,
        .place_id = place->id, .stock = place->stock[CC_GOOD_FOOD],
        .reserve_target = CcEconomyEffectiveReserveTarget(sim, place, CC_GOOD_FOOD),
        .unit_price = place->price[CC_GOOD_FOOD], .day = sim->current_day,
        .payer_coins = payer->travel_coins, .beneficiary_hungry_days = beneficiary->hungry_days};
    (void)snprintf(out->place_name, sizeof(out->place_name), "%s", place->name);
    Error(error, capacity, ""); return true;
}

bool CcFoodReliefPropose(CcSim *sim, const CcFoodReliefProposal *proposal,
                         CcFoodReliefOutcome *out, char *error, size_t capacity)
{
    CcFoodReliefObservation observation;
    CcEvent *event;
    if (sim == NULL || sim->food_agreement_count >= CC_MAX_FOOD_AGREEMENTS) {
        Error(error, capacity, "The food agreement ledger is full."); return false;
    }
    if (proposal == NULL || proposal->quantity <= 0 || proposal->quantity > CC_SIM_MAX_UNITS ||
        proposal->unit_price <= 0 || !CcFoodReliefObserve(sim, proposal->payer_id,
        proposal->beneficiary_id, &observation, error, capacity) ||
        proposal->place_id != observation.place_id || proposal->unit_price != observation.unit_price ||
        proposal->quantity > observation.stock) {
        if (error != NULL && error[0] == '\0') Error(error, capacity, "The proposed food terms are not currently observed.");
        return false;
    }
    event = CcSimPushEvent(sim, CC_EVENT_RELIEF, proposal->payer_id,
        proposal->place_id, 0U, proposal->quantity, "Food agreement");
    if (event == NULL) { Error(error, capacity, "The agreement could not be recorded."); return false; }
    event->subject_id = event->id; event->actor_id = proposal->payer_id;
    event->target_id = proposal->beneficiary_id; event->beneficiary_id = proposal->beneficiary_id;
    event->witness_id = (CcId)proposal->unit_price;
    (void)snprintf(event->text, sizeof(event->text),
        "Food agreement: payer=%" PRIu64 " beneficiary=%" PRIu64 " place=%" PRIu64
        " qty=%d", proposal->payer_id, proposal->beneficiary_id,
        proposal->place_id, proposal->quantity);
    if (!Parse(event, out)) { Error(error, capacity, "The agreement record is invalid."); return false; }
    sim->food_agreements[sim->food_agreement_count++] = (CcFoodAgreement){
        .id = event->id, .payer_id = proposal->payer_id,
        .beneficiary_id = proposal->beneficiary_id, .place_id = proposal->place_id,
        .total_cost = (CcMoney)proposal->quantity * proposal->unit_price,
        .quantity = proposal->quantity, .unit_price = proposal->unit_price,
        .created_day = sim->current_day, .status = CC_FOOD_AGREEMENT_PROPOSED};
    Error(error, capacity, ""); return true;
}

bool CcFoodReliefRead(const CcSim *sim, CcId agreement_id, CcFoodReliefOutcome *out)
{
    for (int32_t i = 0; sim != NULL && i < sim->food_agreement_count; ++i) {
        const CcFoodAgreement *record = &sim->food_agreements[i];
        if (record->id == agreement_id) {
            if (out == NULL) return false;
            *out = (CcFoodReliefOutcome){
                .kind = record->status == CC_FOOD_AGREEMENT_ACCEPTED ? CC_FOOD_RELIEF_OUTCOME_ACCEPTED :
                    record->status == CC_FOOD_AGREEMENT_FULFILLED ? CC_FOOD_RELIEF_OUTCOME_FULFILLED :
                    record->status == CC_FOOD_AGREEMENT_FAILED ? CC_FOOD_RELIEF_OUTCOME_FAILED : CC_FOOD_RELIEF_OUTCOME_PROPOSED,
                .agreement_id = record->id, .payer_id = record->payer_id,
                .beneficiary_id = record->beneficiary_id, .place_id = record->place_id,
                .quantity = record->quantity, .unit_price = record->unit_price,
                .total_cost = record->total_cost, .event_id = record->outcome_event_id != 0U ? record->outcome_event_id : record->id};
            const CcCharacter *beneficiary = CcSimCharacter(sim, record->beneficiary_id);
            out->beneficiary_hungry_days = beneficiary != NULL ? beneficiary->hungry_days : 0;
            return true;
        }
    }
    CcEvent *agreement = Find(sim, agreement_id);
    CcEvent *fulfilled = Child(sim, agreement_id, agreement != NULL ? agreement->actor_id : 0U, "Food fulfilled");
    CcEvent *failed = Child(sim, agreement_id, agreement != NULL ? agreement->actor_id : 0U, "Food failed");
    if (!Parse(agreement, out)) return false;
    if (fulfilled != NULL) { out->kind = CC_FOOD_RELIEF_OUTCOME_FULFILLED; out->event_id = fulfilled->id; }
    else if (failed != NULL) { out->kind = CC_FOOD_RELIEF_OUTCOME_FAILED; out->event_id = failed->id; }
    else if (Child(sim, agreement_id, agreement->target_id, "Food accepted") != NULL) out->kind = CC_FOOD_RELIEF_OUTCOME_ACCEPTED;
    return true;
}

bool CcFoodReliefAccept(CcSim *sim, CcId agreement_id, CcId beneficiary_id,
                        CcFoodReliefOutcome *out, char *error, size_t capacity)
{
    CcFoodReliefOutcome current;
    CcEvent *agreement = Find(sim, agreement_id);
    const CcCharacter *payer;
    const CcCharacter *beneficiary;
    const CcSettlement *place;
    if (!CcFoodReliefRead(sim, agreement_id, &current) || agreement == NULL ||
        agreement->target_id != beneficiary_id || current.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED ||
        current.kind == CC_FOOD_RELIEF_OUTCOME_FAILED) { Error(error, capacity, "The agreement cannot be accepted."); return false; }
    payer = CcSimCharacter(sim, current.payer_id);
    beneficiary = CcSimCharacter(sim, current.beneficiary_id);
    place = CcSimSettlement(sim, current.place_id);
    if (payer == NULL || beneficiary == NULL || place == NULL ||
        payer->current_settlement_id != current.place_id ||
        beneficiary->current_settlement_id != current.place_id ||
        place->price[CC_GOOD_FOOD] != current.unit_price ||
        place->stock[CC_GOOD_FOOD] < current.quantity) {
        Error(error, capacity, "The agreement terms are stale in the current place."); return false;
    }
    if (current.kind == CC_FOOD_RELIEF_OUTCOME_ACCEPTED) { if (out != NULL) *out = current; Error(error, capacity, ""); return true; }
    CcEvent *event = CcSimPushEvent(sim, CC_EVENT_RELIEF, agreement_id,
        agreement->location_id, agreement_id, agreement->magnitude, "Food accepted");
    event->actor_id = beneficiary_id; event->target_id = agreement->actor_id;
    CcCharacter *person = (CcCharacter *)CcSimCharacter(sim, beneficiary_id);
    CcFoodAgreement *record = Agreement(sim, agreement_id);
    if (record != NULL) { record->status = CC_FOOD_AGREEMENT_ACCEPTED; record->accepted_event_id = event->id; record->accepted_day = sim->current_day; }
    Remember(person, CC_CHARACTER_MEMORY_NPC_PROMISED, agreement_id, event->id, sim->current_day);
    if (out != NULL) { current.kind = CC_FOOD_RELIEF_OUTCOME_ACCEPTED; current.event_id = event->id; *out = current; }
    Error(error, capacity, ""); return true;
}

bool CcFoodReliefExecute(CcSim *sim, CcId agreement_id, CcId payer_id,
                         CcFoodReliefOutcome *out, char *error, size_t capacity)
{
    CcFoodReliefOutcome current;
    CcEvent *agreement = Find(sim, agreement_id);
    CcSettlement *place;
    CcCharacter *payer, *beneficiary;
    if (!CcFoodReliefRead(sim, agreement_id, &current) ||
        current.payer_id != payer_id || (agreement == NULL && Agreement(sim, agreement_id) == NULL)) {
        Error(error, capacity, "The payer does not match the agreement."); return false;
    }
    if (current.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED) { if (out != NULL) *out = current; Error(error, capacity, ""); return true; }
    if (current.kind == CC_FOOD_RELIEF_OUTCOME_FAILED) { if (out != NULL) *out = current; Error(error, capacity, "The agreement already failed."); return false; }
    payer = (CcCharacter *)CcSimCharacter(sim, current.payer_id);
    beneficiary = (CcCharacter *)CcSimCharacter(sim, current.beneficiary_id);
    place = CcSimSettlementMutable(sim, current.place_id);
    if (current.kind != CC_FOOD_RELIEF_OUTCOME_ACCEPTED || payer == NULL || beneficiary == NULL || place == NULL ||
        payer->death_day <= sim->current_day || beneficiary->death_day <= sim->current_day ||
        payer->travel_destination_id != 0U || beneficiary->travel_destination_id != 0U ||
        payer->current_settlement_id != current.place_id || beneficiary->current_settlement_id != current.place_id ||
        place->price[CC_GOOD_FOOD] != current.unit_price ||
        place->stock[CC_GOOD_FOOD] < current.quantity || payer->travel_coins < current.total_cost) {
        if (current.kind != CC_FOOD_RELIEF_OUTCOME_ACCEPTED) {
            Error(error, capacity, "The agreement must be accepted before execution."); return false;
        }
        CcEvent *failed = CcSimPushEvent(sim, CC_EVENT_RELIEF, agreement_id,
            current.place_id, agreement_id, -current.quantity, "Food failed");
        failed->actor_id = payer_id; failed->target_id = current.beneficiary_id;
        CcFoodAgreement *record = Agreement(sim, agreement_id);
        if (record != NULL) { record->status = CC_FOOD_AGREEMENT_FAILED; record->outcome_event_id = failed->id; }
        Remember(beneficiary, CC_CHARACTER_MEMORY_PROMISE_FAILED, agreement_id, failed->id, sim->current_day);
        if (out != NULL) { current.kind = CC_FOOD_RELIEF_OUTCOME_FAILED; current.event_id = failed->id; *out = current; }
        Error(error, capacity, "The stored food, price, or payer purse changed."); return false;
    }
    place->stock[CC_GOOD_FOOD] -= current.quantity;
    payer->travel_coins -= current.total_cost;
    place->market_coins += current.total_cost;
    beneficiary->hungry_days = 0;
    CcEvent *fulfilled = CcSimPushEvent(sim, CC_EVENT_RELIEF, agreement_id,
        current.place_id, agreement_id, current.quantity, "Food fulfilled");
    fulfilled->actor_id = payer_id; fulfilled->target_id = current.beneficiary_id;
    CcFoodAgreement *record = Agreement(sim, agreement_id);
    if (record != NULL) { record->status = CC_FOOD_AGREEMENT_FULFILLED; record->outcome_event_id = fulfilled->id; }
    Remember(beneficiary, CC_CHARACTER_MEMORY_PROMISE_FULFILLED, agreement_id, fulfilled->id, sim->current_day);
    CcRelationship *relation = Relation(sim, payer_id, current.beneficiary_id, fulfilled->id);
    if (relation != NULL) { relation->trust = relation->trust < 3 ? relation->trust + 1 : 3; relation->affinity = relation->affinity < 3 ? relation->affinity + 1 : 3; relation->cause_event_id = fulfilled->id; }
    relation = Relation(sim, current.beneficiary_id, payer_id, fulfilled->id);
    if (relation != NULL) { relation->trust = relation->trust < 3 ? relation->trust + 1 : 3; relation->affinity = relation->affinity < 3 ? relation->affinity + 1 : 3; relation->cause_event_id = fulfilled->id; }
    current.kind = CC_FOOD_RELIEF_OUTCOME_FULFILLED; current.event_id = fulfilled->id;
    current.beneficiary_hungry_days = beneficiary->hungry_days;
    if (out != NULL) *out = current;
    Error(error, capacity, ""); return true;
}
