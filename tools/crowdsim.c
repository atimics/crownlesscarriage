/* In-process access to the food-relief simulation for policy search.
 *
 * The probes cost a process launch per call (about 0.3 s per snapshot). This
 * shared library keeps a world in memory and exposes the same operations, so
 * evolution and counterfactual replay can run thousands of episodes a minute.
 * Views are the compact JSON that tools/dialogue/meaning.py reads. A world is a
 * plain struct, so cs_clone is a memory copy. Every call is read-only except
 * cs_advance, cs_promise, cs_accept and cs_execute.
 */
#include "story/cc_core_participant.h"
#include "persistence/cc_save.h"
#include "sim/cc_food_relief.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CrowdSim { CcSim sim; } CrowdSim;

typedef struct Out { char *buf; size_t cap, len; int overflow; } Out;

static void Put(Out *o, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    size_t room = o->cap > o->len ? o->cap - o->len : 0;
    int n = vsnprintf(o->buf + o->len, room, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= room) o->overflow = 1; else o->len += (size_t)n;
}

static void PutString(Out *o, const char *text)
{
    Put(o, "\"");
    for (const unsigned char *p = (const unsigned char *)text; *p != 0U; ++p) {
        if (*p == '"' || *p == '\\') Put(o, "\\%c", *p);
        else if (*p < 32U) Put(o, "\\u%04x", (unsigned int)*p);
        else Put(o, "%c", *p);
    }
    Put(o, "\"");
}

static int Finish(Out *o) { return o->overflow || o->len >= o->cap ? -1 : (int)o->len; }

CrowdSim *cs_load(const char *path, char *error, size_t error_capacity)
{
    CrowdSim *w = calloc(1U, sizeof(*w));
    if (w == NULL) return NULL;
    if (!CcSaveRead(path, &w->sim, error, error_capacity)) { free(w); return NULL; }
    return w;
}

CrowdSim *cs_clone(const CrowdSim *w)
{
    CrowdSim *copy = malloc(sizeof(*copy));
    if (copy == NULL) return NULL;
    memcpy(copy, w, sizeof(*copy));
    return copy;
}

void cs_free(CrowdSim *w) { free(w); }

uint64_t cs_hash(const CrowdSim *w) { return CcSimHash(&w->sim); }

void cs_advance(CrowdSim *w, int days) { if (days > 0) CcSimAdvanceDays(&w->sim, days); }

int cs_list(const CrowdSim *w, char *buf, size_t cap)
{
    Out o = {buf, cap, 0, 0};
    const CcSim *sim = &w->sim;
    Put(&o, "{\"day\":%d,\"people\":[", sim->current_day);
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *p = &sim->characters[i];
        Put(&o, "%s{\"id\":\"%" PRIu64 "\",\"name\":", i ? "," : "", p->id);
        PutString(&o, p->name);
        Put(&o, ",\"place_id\":\"%" PRIu64 "\",\"hungry_days\":%d,\"coins\":%" PRId64
                ",\"in_transit\":%s,\"alive\":%s,\"stress\":%d,\"bandit\":%s}",
            p->current_settlement_id, p->hungry_days, p->travel_coins,
            p->travel_destination_id != 0U || p->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ? "true" : "false",
            p->death_day > 0 && p->death_day <= sim->current_day ? "false" : "true",
            p->stress, p->bandit_group_id != 0U ? "true" : "false");
    }
    Put(&o, "]}");
    return Finish(&o);
}

int cs_observe(const CrowdSim *w, uint64_t payer, uint64_t beneficiary, char *buf, size_t cap)
{
    CcFoodReliefObservation ob;
    char error[128];
    if (!CcFoodReliefObserve(&w->sim, payer, beneficiary, &ob, error, sizeof(error))) return -2;
    Out o = {buf, cap, 0, 0};
    Put(&o, "{\"kind\":\"food_store\",\"place_id\":\"%" PRIu64 "\",\"place_name\":", ob.place_id);
    PutString(&o, ob.place_name);
    Put(&o, ",\"stock\":%d,\"reserve_target\":%d,\"unit_price\":%d,\"payer_coins\":%" PRId64
            ",\"beneficiary_hungry_days\":%d,\"day\":%d}",
        ob.stock, ob.reserve_target, ob.unit_price, ob.payer_coins, ob.beneficiary_hungry_days, ob.day);
    return Finish(&o);
}

static void Outcomes(const CcSim *sim, CcId a, CcId b, Out *o)
{
    int first = 1;
    Put(o, "[");
    for (int32_t i = 0; i < sim->food_agreement_count; ++i) {
        const CcFoodAgreement *r = &sim->food_agreements[i];
        CcFoodReliefOutcome out;
        if (!((r->payer_id == a && r->beneficiary_id == b) || (r->payer_id == b && r->beneficiary_id == a)) ||
            !CcFoodReliefRead(sim, r->id, &out) ||
            (out.kind != CC_FOOD_RELIEF_OUTCOME_FULFILLED && out.kind != CC_FOOD_RELIEF_OUTCOME_FAILED)) continue;
        Put(o, "%s{\"outcome\":\"%s\",\"quantity\":%d,\"total_cost\":%" PRId64 ",\"event_id\":\"%" PRIu64
               "\",\"actor_id\":\"%" PRIu64 "\",\"beneficiary_id\":\"%" PRIu64 "\",\"reason\":\"outcome\"}",
            first ? "" : ",", out.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED ? "fulfilled" : "failed",
            out.quantity, out.total_cost, out.event_id, out.payer_id, out.beneficiary_id);
        first = 0;
    }
    Put(o, "]");
}

static int Participant(const CrowdSim *w, CcId own, CcId other, Out *o)
{
    CcCoreParticipant p;
    CcFoodReliefObservation ob;
    char error[128];
    if (!CcCoreParticipantBuild(&w->sim, own, other, &p)) return 0;
    if (!CcFoodReliefObserve(&w->sim, own, other, &ob, error, sizeof(error))) return 0;
    Put(o, "{\"self\":{\"id\":\"%" PRIu64 "\",\"name\":", p.id);
    PutString(o, p.name);
    Put(o, ",\"coins\":%" PRId64 ",\"hungry_days\":%d,\"stress\":%d},\"listener\":{\"id\":\"%" PRIu64 "\",\"name\":",
        p.coins, p.hungry_days, p.stress, p.listener_id);
    PutString(o, p.listener_name);
    Put(o, "},\"place\":{\"id\":\"%" PRIu64 "\",\"name\":", p.place_id);
    PutString(o, p.place);
    Put(o, "},\"day\":%d,\"relationship\":", p.day);
    if (p.has_relationship) Put(o, "{\"affinity\":%d,\"trust\":%d,\"obligation\":%d}",
                                p.relationship.affinity, p.relationship.trust, p.relationship.obligation);
    else Put(o, "null");
    Put(o, ",\"facts\":[{\"kind\":\"food_store\",\"owner\":\"%" PRIu64 "\",\"place_id\":\"%" PRIu64 "\",\"place_name\":",
        own, ob.place_id);
    PutString(o, ob.place_name);
    Put(o, ",\"stock\":%d,\"target\":%d,\"unit_price\":%d,\"day\":%d,\"source\":\"observed\",\"private\":false}],\"outcomes\":",
        ob.stock, ob.reserve_target, ob.unit_price, ob.day);
    Outcomes(&w->sim, own, other, o);
    Put(o, "}");
    return 1;
}

/* Both participants' views for two people in the same place, as {"participants":[a,b]}. */
int cs_view(const CrowdSim *w, uint64_t first, uint64_t second, char *buf, size_t cap)
{
    Out o = {buf, cap, 0, 0};
    const CcCharacter *a = CcSimCharacter(&w->sim, first), *b = CcSimCharacter(&w->sim, second);
    if (a == NULL || b == NULL || first == second || a->current_settlement_id == 0U ||
        a->current_settlement_id != b->current_settlement_id ||
        a->travel_destination_id != 0U || b->travel_destination_id != 0U) return -2;
    Put(&o, "{\"participants\":[");
    if (!Participant(w, first, second, &o)) return -2;
    Put(&o, ",");
    if (!Participant(w, second, first, &o)) return -2;
    Put(&o, "]}");
    return Finish(&o);
}

static int PrintOutcome(const CcFoodReliefOutcome *out, char *buf, size_t cap)
{
    Out o = {buf, cap, 0, 0};
    Put(&o, "{\"agreement_id\":\"%" PRIu64 "\",\"kind\":%d,\"quantity\":%d,\"unit_price\":%d,\"total_cost\":%" PRId64
            ",\"beneficiary_hungry_days\":%d}", out->agreement_id, (int)out->kind, out->quantity,
        out->unit_price, out->total_cost, out->beneficiary_hungry_days);
    return Finish(&o);
}

int cs_promise(CrowdSim *w, uint64_t payer, uint64_t beneficiary, int quantity, int price, char *buf, size_t cap)
{
    const CcCharacter *person = CcSimCharacter(&w->sim, payer);
    char error[128];
    if (person == NULL || quantity < 1 || price < 0) return -2;
    CcCommand command = {.kind = CC_COMMAND_FOOD_RELIEF_PROPOSE, .actor_id = payer, .target_id = beneficiary,
        .secondary_id = person->current_settlement_id, .amount = quantity, .good = (CcGood)price};
    if (!CcSimApply(&w->sim, &command, error, sizeof(error))) return -2;
    CcFoodReliefOutcome out;
    CcId id = w->sim.food_agreements[w->sim.food_agreement_count - 1].id;
    if (!CcFoodReliefRead(&w->sim, id, &out)) return -2;
    return PrintOutcome(&out, buf, cap);
}

static int Step(CrowdSim *w, int accept, uint64_t agreement, uint64_t actor, char *buf, size_t cap)
{
    char error[128];
    CcCommand command = {.kind = accept ? CC_COMMAND_FOOD_RELIEF_ACCEPT : CC_COMMAND_FOOD_RELIEF_EXECUTE,
        .actor_id = actor, .target_id = agreement};
    if (!CcSimApply(&w->sim, &command, error, sizeof(error))) return -2;
    CcFoodReliefOutcome out;
    if (!CcFoodReliefRead(&w->sim, agreement, &out)) return -2;
    return PrintOutcome(&out, buf, cap);
}

int cs_accept(CrowdSim *w, uint64_t agreement, uint64_t beneficiary, char *buf, size_t cap)
{
    return Step(w, 1, agreement, beneficiary, buf, cap);
}

int cs_execute(CrowdSim *w, uint64_t agreement, uint64_t payer, char *buf, size_t cap)
{
    return Step(w, 0, agreement, payer, buf, cap);
}
