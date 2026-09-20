#include "sim/cc_food_relief.h"
#include "sim/cc_food_economy_internal.h"

#include <stdio.h>

static void Error(char *error, size_t capacity, const char *text)
{
    if (error != NULL && capacity > 0U) (void)snprintf(error, capacity, "%s", text);
}

static CcFoodAgreement *Agreement(CcSim *sim, CcId id)
{
    if (sim == NULL || sim->schema_version < 107U || sim->food_agreement_count < 0 ||
        sim->food_agreement_count > CC_MAX_FOOD_AGREEMENTS || id == 0U) return NULL;
    for (int32_t i = 0; i < sim->food_agreement_count; ++i) {
        if (sim->food_agreements[i].id == id) return &sim->food_agreements[i];
    }
    return NULL;
}

static bool Present(const CcSim *sim, const CcCharacter *person, CcId place)
{
    return person != NULL && place != 0U &&
        !(person->death_day > 0 && person->death_day <= sim->current_day) &&
        person->travel_destination_id == 0U &&
        person->activity != CC_CHARACTER_ACTIVITY_TRAVELLING &&
        person->current_settlement_id == place;
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

static void ImproveRelation(CcSim *sim, CcId from, CcId to, CcId event_id)
{
    CcRelationship *relation = NULL;
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        if (sim->relationships[i].from_character_id == from &&
            sim->relationships[i].to_character_id == to) {
            relation = &sim->relationships[i];
            break;
        }
    }
    if (relation == NULL && sim->relationship_count < CC_MAX_RELATIONSHIPS) {
        relation = &sim->relationships[sim->relationship_count++];
        *relation = (CcRelationship){from, to, 0, 0, 0,
            CC_RELATIONSHIP_HISTORY_COWORKERS, event_id};
    }
    if (relation != NULL) {
        if (relation->trust < 3) relation->trust += 1;
        if (relation->affinity < 3) relation->affinity += 1;
        relation->cause_event_id = event_id;
    }
}

bool CcFoodReliefObserve(const CcSim *sim, CcId payer_id, CcId beneficiary_id,
                         CcFoodReliefObservation *out, char *error, size_t capacity)
{
    if (sim == NULL) { Error(error, capacity, "A world is required."); return false; }
    const CcCharacter *payer = CcSimCharacter(sim, payer_id);
    const CcCharacter *beneficiary = CcSimCharacter(sim, beneficiary_id);
    if (out == NULL || payer == NULL || payer_id == beneficiary_id ||
        !Present(sim, payer, payer->current_settlement_id) ||
        !Present(sim, beneficiary, payer->current_settlement_id)) {
        Error(error, capacity, "Both people must be alive and present in one place."); return false;
    }
    const CcSettlement *place = CcSimSettlement(sim, payer->current_settlement_id);
    if (place == NULL) { Error(error, capacity, "The local food store is unavailable."); return false; }
    *out = (CcFoodReliefObservation){.payer_id = payer_id, .beneficiary_id = beneficiary_id,
        .place_id = place->id, .stock = place->stock[CC_GOOD_FOOD],
        .reserve_target = CcEconomyEffectiveReserveTarget(sim, place, CC_GOOD_FOOD),
        .unit_price = place->price[CC_GOOD_FOOD], .day = sim->current_day,
        .payer_coins = payer->travel_coins, .beneficiary_hungry_days = beneficiary->hungry_days};
    (void)snprintf(out->place_name, sizeof(out->place_name), "%s", place->name);
    Error(error, capacity, ""); return true;
}

bool CcFoodReliefRead(const CcSim *sim, CcId agreement_id, CcFoodReliefOutcome *out)
{
    if (sim == NULL || sim->schema_version < 107U || sim->food_agreement_count < 0 ||
        sim->food_agreement_count > CC_MAX_FOOD_AGREEMENTS || out == NULL) return false;
    for (int32_t i = 0; i < sim->food_agreement_count; ++i) {
        const CcFoodAgreement *record = &sim->food_agreements[i];
        if (record->id != agreement_id) continue;
        *out = (CcFoodReliefOutcome){
            .kind = record->status == CC_FOOD_AGREEMENT_ACCEPTED ? CC_FOOD_RELIEF_OUTCOME_ACCEPTED :
                record->status == CC_FOOD_AGREEMENT_FULFILLED ? CC_FOOD_RELIEF_OUTCOME_FULFILLED :
                record->status == CC_FOOD_AGREEMENT_FAILED ? CC_FOOD_RELIEF_OUTCOME_FAILED : CC_FOOD_RELIEF_OUTCOME_PROPOSED,
            .agreement_id = record->id, .payer_id = record->payer_id,
            .beneficiary_id = record->beneficiary_id, .place_id = record->place_id,
            .quantity = record->quantity, .unit_price = record->unit_price,
            .total_cost = record->total_cost,
            .event_id = record->outcome_event_id != 0U ? record->outcome_event_id :
                record->accepted_event_id != 0U ? record->accepted_event_id : record->id};
        const CcCharacter *beneficiary = CcSimCharacter(sim, record->beneficiary_id);
        out->beneficiary_hungry_days = beneficiary != NULL ? beneficiary->hungry_days : 0;
        return true;
    }
    return false;
}

static CcEvent *RecordEvent(CcSim *sim, const CcFoodAgreement *record,
                            CcId actor, CcId other, int32_t magnitude, const char *text)
{
    CcEvent *event = CcSimPushEvent(sim, CC_EVENT_RELIEF, record->id,
        record->place_id, CcSimEvent(sim, record->id) != NULL ? record->id : 0U,
        magnitude, text);
    if (event != NULL) {
        event->actor_id = actor;
        event->target_id = other;
        event->beneficiary_id = record->beneficiary_id;
    }
    return event;
}

bool CcFoodReliefPropose(CcSim *sim, const CcFoodReliefProposal *proposal,
                         CcFoodReliefOutcome *out, char *error, size_t capacity)
{
    CcFoodReliefObservation observation;
    if (sim == NULL || sim->schema_version < 107U) {
        Error(error, capacity, "Food agreements require world schema 107."); return false;
    }
    if (sim->food_agreement_count >= CC_MAX_FOOD_AGREEMENTS) {
        Error(error, capacity, "The food agreement ledger is full."); return false;
    }
    if (proposal == NULL || proposal->quantity <= 0 || proposal->quantity > CC_SIM_MAX_UNITS ||
        proposal->unit_price <= 0 ||
        (CcMoney)proposal->quantity * proposal->unit_price > CC_SIM_MAX_MONEY) {
        Error(error, capacity, "Food quantity and price must be within world limits."); return false;
    }
    if (!CcFoodReliefObserve(sim, proposal->payer_id, proposal->beneficiary_id,
                             &observation, error, capacity)) return false;
    CcMoney cost = (CcMoney)proposal->quantity * proposal->unit_price;
    if (proposal->place_id != observation.place_id || proposal->unit_price != observation.unit_price ||
        proposal->quantity > observation.stock || cost > observation.payer_coins) {
        Error(error, capacity, "The proposed terms must match the local store and payer purse."); return false;
    }
    CcEvent *event = CcSimPushEvent(sim, CC_EVENT_RELIEF, proposal->payer_id,
        proposal->place_id, 0U, proposal->quantity, "Food offered");
    if (event == NULL) { Error(error, capacity, "The agreement could not be recorded."); return false; }
    event->subject_id = event->id;
    event->actor_id = proposal->payer_id;
    event->target_id = proposal->beneficiary_id;
    event->beneficiary_id = proposal->beneficiary_id;
    CcId id = event->id;
    sim->food_agreements[sim->food_agreement_count++] = (CcFoodAgreement){
        .id = id, .payer_id = proposal->payer_id,
        .beneficiary_id = proposal->beneficiary_id, .place_id = proposal->place_id,
        .total_cost = cost, .quantity = proposal->quantity, .unit_price = proposal->unit_price,
        .created_day = sim->current_day, .status = CC_FOOD_AGREEMENT_PROPOSED};
    if (out != NULL) (void)CcFoodReliefRead(sim, id, out);
    Error(error, capacity, ""); return true;
}

bool CcFoodReliefAccept(CcSim *sim, CcId agreement_id, CcId beneficiary_id,
                        CcFoodReliefOutcome *out, char *error, size_t capacity)
{
    CcFoodAgreement *record = Agreement(sim, agreement_id);
    if (record == NULL || record->beneficiary_id != beneficiary_id ||
        record->status > CC_FOOD_AGREEMENT_ACCEPTED) {
        Error(error, capacity, "Only the beneficiary can accept an open agreement."); return false;
    }
    if (record->status == CC_FOOD_AGREEMENT_ACCEPTED) {
        if (out != NULL) (void)CcFoodReliefRead(sim, agreement_id, out);
        Error(error, capacity, ""); return true;
    }
    CcCharacter *payer = (CcCharacter *)CcSimCharacter(sim, record->payer_id);
    CcCharacter *beneficiary = (CcCharacter *)CcSimCharacter(sim, record->beneficiary_id);
    const CcSettlement *place = CcSimSettlement(sim, record->place_id);
    if (!Present(sim, payer, record->place_id) || !Present(sim, beneficiary, record->place_id) ||
        place == NULL || place->price[CC_GOOD_FOOD] != record->unit_price ||
        place->stock[CC_GOOD_FOOD] < record->quantity || payer->travel_coins < record->total_cost) {
        Error(error, capacity, "Both people must observe the agreed terms at acceptance."); return false;
    }
    CcEvent *event = RecordEvent(sim, record, beneficiary_id, record->payer_id,
                                 record->quantity, "Food accepted");
    if (event == NULL) { Error(error, capacity, "The acceptance could not be recorded."); return false; }
    record->status = CC_FOOD_AGREEMENT_ACCEPTED;
    record->accepted_event_id = event->id;
    record->accepted_day = sim->current_day;
    Remember(payer, CC_CHARACTER_MEMORY_NPC_PROMISED, agreement_id, event->id, sim->current_day);
    Remember(beneficiary, CC_CHARACTER_MEMORY_NPC_PROMISED, agreement_id, event->id, sim->current_day);
    if (out != NULL) (void)CcFoodReliefRead(sim, agreement_id, out);
    Error(error, capacity, ""); return true;
}

bool CcFoodReliefExecute(CcSim *sim, CcId agreement_id, CcId payer_id,
                         CcFoodReliefOutcome *out, char *error, size_t capacity)
{
    CcFoodAgreement *record = Agreement(sim, agreement_id);
    if (record == NULL || record->payer_id != payer_id) {
        Error(error, capacity, "Only the payer can execute this agreement."); return false;
    }
    if (record->status == CC_FOOD_AGREEMENT_PROPOSED) {
        Error(error, capacity, "The beneficiary must accept first."); return false;
    }
    if (record->status == CC_FOOD_AGREEMENT_FULFILLED || record->status == CC_FOOD_AGREEMENT_FAILED) {
        if (out != NULL) (void)CcFoodReliefRead(sim, agreement_id, out);
        Error(error, capacity, ""); return true;
    }
    CcCharacter *payer = (CcCharacter *)CcSimCharacter(sim, record->payer_id);
    CcCharacter *beneficiary = (CcCharacter *)CcSimCharacter(sim, record->beneficiary_id);
    CcSettlement *place = CcSimSettlementMutable(sim, record->place_id);
    bool available = Present(sim, payer, record->place_id) &&
        Present(sim, beneficiary, record->place_id) && place != NULL &&
        place->price[CC_GOOD_FOOD] == record->unit_price &&
        place->stock[CC_GOOD_FOOD] >= record->quantity &&
        payer->travel_coins >= record->total_cost &&
        place->market_coins <= CC_SIM_MAX_MONEY - record->total_cost;
    CcEvent *event = RecordEvent(sim, record, payer_id, record->beneficiary_id,
        available ? record->quantity : -record->quantity,
        available ? "Food fulfilled" : "Food failed");
    if (event == NULL) { Error(error, capacity, "The outcome could not be recorded."); return false; }
    CcId event_id = event->id;
    record->status = available ? CC_FOOD_AGREEMENT_FULFILLED : CC_FOOD_AGREEMENT_FAILED;
    record->outcome_event_id = event_id;
    if (available) {
        place->stock[CC_GOOD_FOOD] -= record->quantity;
        payer->travel_coins -= record->total_cost;
        place->market_coins += record->total_cost;
        /* One bread unit supplies one civilian ration. */
        beneficiary->hungry_days = 0;
        ImproveRelation(sim, payer_id, record->beneficiary_id, event_id);
        ImproveRelation(sim, record->beneficiary_id, payer_id, event_id);
    }
    CcCharacterMemoryKind memory = available ? CC_CHARACTER_MEMORY_PROMISE_FULFILLED : CC_CHARACTER_MEMORY_PROMISE_FAILED;
    Remember(payer, memory, agreement_id, event_id, sim->current_day);
    Remember(beneficiary, memory, agreement_id, event_id, sim->current_day);
    if (out != NULL) (void)CcFoodReliefRead(sim, agreement_id, out);
    Error(error, capacity, ""); return true;
}
