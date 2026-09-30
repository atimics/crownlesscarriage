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
#include "sim/cc_policy.h"

#include <inttypes.h>
#include <math.h>
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

/* ---- daily-life policy: a small network scores each option, in C ---- */

#define CC_MAX_CHARACTERS_HINT 512
#define POLICY_KINDS 10
#define ACTOR_FEATURES 19
#define OPTION_FEATURES 21
#define POLICY_INPUTS (POLICY_KINDS + ACTOR_FEATURES + OPTION_FEATURES)
#define POLICY_HIDDEN 16
#define POLICY_WEIGHTS (POLICY_INPUTS * POLICY_HIDDEN + POLICY_HIDDEN + POLICY_HIDDEN + 1)

static double policy_weights[POLICY_WEIGHTS];
static long policy_offered[POLICY_KINDS], policy_changed[POLICY_KINDS];
static int policy_active;
static int policy_mask;
static double trade_bias[3];   /* probe: rule score + a*destination hunger + b*need + c*path cost */
static int trade_bias_on;
static int dragon_force = -1;   /* probe: >= 0 always burns the k-th listed town */
static int gossip_floor = -1;   /* >= 0: withhold any story carried below this confidence (a hand-set probe) */   /* bit k set: the network decides kind k; others follow the rule */

int cs_policy_size(void) { return POLICY_WEIGHTS; }

/* Decisions offered and decisions changed from the rule's, per kind (offered first). */
void cs_policy_stats(long *out, int reset)
{
    for (int k = 0; k < POLICY_KINDS; ++k) { out[k] = policy_offered[k]; out[POLICY_KINDS + k] = policy_changed[k]; }
    if (reset) { memset(policy_offered, 0, sizeof(policy_offered)); memset(policy_changed, 0, sizeof(policy_changed)); }
}

static double Clip(double v) { return v < 0.0 ? 0.0 : v > 3.0 ? 3.0 : v; }

/* Cost of the cheapest meal at a settlement, or 0 when it has none in stock. */
static double MealCost(const CcSettlement *place)
{
    double cheapest = 0.0;
    for (int good = 0; place != NULL && good < CC_GOOD_COUNT; ++good) {
        int32_t nutrition = CcGoodNutritionValue((CcGood)good, CC_NUTRITION_CIVILIAN);
        if (nutrition <= 0) continue;
        int32_t needed = (CC_NUTRITION_PER_RATION + nutrition - 1) / nutrition;
        double cost = (double)(place->price[good] > 1 ? place->price[good] : 1) * needed;
        if (place->stock[good] >= needed && (cheapest == 0.0 || cost < cheapest)) cheapest = cost;
    }
    return cheapest;
}

static void ActorFeatures(const CcSim *sim, const CcCharacter *p, double *f)
{
    const CcSettlement *here = CcSimSettlement(sim, p->current_settlement_id);
    memset(f, 0, ACTOR_FEATURES * sizeof(double));
    f[0] = Clip((double)p->travel_coins / 30.0);
    f[1] = (double)p->hungry_days / 7.0;
    f[2] = (double)p->unsheltered_nights / 7.0;
    f[3] = (double)p->stress / 100.0;
    f[4] = p->current_settlement_id != p->home_settlement_id;
    f[5] = p->bandit_group_id != 0U;
    if ((int)p->role >= 1 && (int)p->role <= 6) f[5 + (int)p->role] = 1.0;   /* six roles */
    if ((int)p->goal >= 0 && (int)p->goal < 4) f[11 + (int)p->goal] = 1.0;
    f[15] = Clip(MealCost(here) / 20.0);
    f[16] = here != NULL && CcSettlementHasService(here, CC_SERVICE_INN);
    f[17] = here != NULL ? (double)here->prosperity / 100.0 : 0.0;
    f[18] = (double)CcCharacterAgeYears(sim, p) / 60.0;
}

static void OptionFeatures(const CcSim *sim, CcPolicyKind kind, CcId actor, const CcPolicyOption *o, int is_last,
                           double *f)
{
    memset(f, 0, OPTION_FEATURES * sizeof(double));
    const CcCharacter *p = CcSimCharacter(sim, actor);
    if (kind == CC_POLICY_DRAGON_TARGET) {
        const CcSettlement *town = CcSimSettlement(sim, o->target_id);
        if (town != NULL) {
            int32_t services = CcSettlementServiceCount(town);
            f[13] = (double)town->prosperity / 100.0;
            f[14] = (double)town->hunger / 100.0;
            f[15] = (double)town->security / 100.0;
            f[16] = Clip((double)town->population / 1000.0);
            f[17] = Clip((double)o->value / 500.0);       /* the rule's richness score */
            f[18] = (double)services / 10.0;
            for (int32_t k = 0; k < sim->kingdom_count; ++k)
                if (sim->kingdoms[k].id == town->kingdom_id) {
                    f[19] = Clip((double)sim->kingdoms[k].treasury / 1000.0);
                    f[20] = (double)sim->kingdoms[k].legitimacy / 100.0;
                }
        }
    } else if (kind == CC_POLICY_KINGDOM_RELIEF) {
        const CcSettlement *town = CcSimSettlement(sim, o->target_id);
        f[0] = is_last;                                   /* hold the treasury */
        if (town != NULL && !is_last) {
            f[13] = (double)town->hunger / 100.0;
            f[14] = (double)town->prosperity / 100.0;
            f[15] = (double)town->security / 100.0;
            f[16] = Clip((double)town->population / 1000.0);
            f[17] = Clip((double)town->market_coins / 200.0);
        }
    } else if (kind == CC_POLICY_RAID_TARGET || kind == CC_POLICY_RAID_LAUNCH) {
        f[0] = kind == CC_POLICY_RAID_LAUNCH ? is_last : 0.0;   /* hold */
        const CcSettlement *town = CcSimSettlement(sim, o->target_id);
        if (town != NULL) {
            double stock = 0.0;
            for (int g = 0; g < CC_GOOD_COUNT; ++g) stock += town->stock[g];
            f[13] = (double)town->security / 100.0;
            f[14] = Clip(stock / 200.0);
            f[15] = (double)town->hunger / 100.0;
            f[16] = (double)town->prosperity / 100.0;
            f[17] = Clip((double)o->value / 500.0);       /* the rule's own score */
        }
    } else if (kind == CC_POLICY_TRADE_ROUTE) {
        const CcSettlement *to = CcSimSettlement(sim, o->target_id), *from = CcSimSettlement(sim, o->source_id);
        f[13] = Clip((double)o->need / 100.0);
        f[14] = Clip((double)o->surplus / 100.0);
        f[15] = Clip((double)o->path_cost / 200.0);
        f[16] = (double)o->urgent;
        f[17] = Clip((double)o->value / 2000.0);          /* the rule's own score */
        f[18] = (double)o->good / (double)CC_GOOD_COUNT;
        f[19] = to != NULL ? (double)to->hunger / 100.0 : 0.0;
        f[20] = to != NULL && from != NULL ? ((double)to->prosperity - (double)from->prosperity) / 100.0 : 0.0;
    } else if (kind == CC_POLICY_GOSSIP_SHARE) {
        f[0] = is_last;                                   /* withhold */
        for (int i = 0; i < CC_MAX_GOSSIP; ++i) {
            const CcGossip *story = &sim->gossip[i];
            if (story->event_id != o->target_id) continue;
            uint32_t mask = story->settlement_mask;
            int known = 0;
            for (; mask != 0U; mask &= mask - 1U) ++known;
            f[8] = Clip((double)(sim->current_day - story->day) / 90.0);
            f[9] = (double)o->value / 100.0;              /* confidence of the carried version */
            f[10] = (double)story->heard.alarm / 100.0;
            f[11] = sim->settlement_count > 0 ? (double)known / (double)sim->settlement_count : 0.0;
            f[12] = Clip((double)story->heard.retellings / 10.0);
            break;
        }
    } else if (kind == CC_POLICY_TRAVEL_DESTINATION) {
        const CcSettlement *far = CcSimSettlement(sim, o->target_id);
        f[0] = is_last;                                   /* stay */
        f[1] = (double)o->value / 7.0;                    /* travel days */
        if (!is_last && far != NULL) {
            f[3] = Clip(MealCost(far) / 20.0);
            f[4] = CcSettlementHasService(far, CC_SERVICE_INN);
            f[5] = (double)far->prosperity / 100.0;
            f[6] = p != NULL && far->id == p->home_settlement_id;
        }
    } else if (kind == CC_POLICY_MEAL || kind == CC_POLICY_LODGING) {
        f[0] = is_last;                                   /* go without / sleep rough */
        f[2] = Clip((double)o->value / 20.0);
    } else {
        f[0] = is_last;                                   /* hold out */
        f[7] = Clip((double)o->value / 50.0);
    }
}

static int32_t Choose(void *user, const CcSim *sim, CcPolicyKind kind, CcId actor,
                      const CcPolicyOption *options, int32_t count, int32_t fallback)
{
    (void)user;
    if (kind == CC_POLICY_DRAGON_TARGET && dragon_force >= 0 && ((policy_mask >> (int)kind) & 1))
        return dragon_force < count ? dragon_force : fallback;
    if (kind == CC_POLICY_TRADE_ROUTE && trade_bias_on && ((policy_mask >> (int)kind) & 1)) {
        int32_t best = fallback; double top = -1e300;
        for (int32_t i = 0; i < count; ++i) {
            const CcSettlement *to = CcSimSettlement(sim, options[i].target_id);
            double score = (double)options[i].value + trade_bias[0] * (to != NULL ? to->hunger : 0) +
                trade_bias[1] * options[i].need + trade_bias[2] * options[i].path_cost;
            if (score > top + 1e-9) { top = score; best = i; }
        }
        return best;
    }
    if (kind == CC_POLICY_GOSSIP_SHARE && gossip_floor >= 0 && ((policy_mask >> (int)kind) & 1))
        return options[0].value < gossip_floor ? 1 : 0;
    const CcCharacter *p = CcSimCharacter(sim, actor);
    if ((p == NULL && kind != CC_POLICY_GOSSIP_SHARE && kind != CC_POLICY_TRADE_ROUTE && kind != CC_POLICY_RAID_TARGET && kind != CC_POLICY_RAID_LAUNCH && kind != CC_POLICY_KINGDOM_RELIEF && kind != CC_POLICY_DRAGON_TARGET) || count > 64 || !((policy_mask >> (int)kind) & 1)) return fallback;
    /* Whether a traveller leaves is the role's business: keep the rule's decision to stay,
       and when it decides to go, choose only where (never "stay"). */
    if (kind == CC_POLICY_TRAVEL_DESTINATION && fallback == count - 1) return fallback;
    int32_t choices = kind == CC_POLICY_TRAVEL_DESTINATION ? count - 1 : count;
    double x[POLICY_INPUTS], a[ACTOR_FEATURES];
    if (p != NULL) ActorFeatures(sim, p, a); else memset(a, 0, sizeof(a));   /* a courier or carriage carries stories too */
    if (kind == CC_POLICY_RAID_LAUNCH) {   /* the actor is a bandit band */
        for (int32_t i = 0; i < sim->bandit_count; ++i) {
            const CcBanditGroup *band = &sim->bandits[i];
            if (band->id != actor) continue;
            a[0] = (double)band->members / 120.0; a[1] = (double)band->supplies / 100.0;
            a[2] = (double)band->influence / 100.0; a[3] = Clip((double)band->raids_completed / 10.0);
            a[4] = (double)band->camp_size / 3.0;
        }
    }
    double score[64], best = -1e300;
    int32_t argmax = 0;
    for (int32_t i = 0; i < choices; ++i) {
        memset(x, 0, sizeof(x));
        x[(int)kind] = 1.0;
        memcpy(x + POLICY_KINDS, a, sizeof(a));
        OptionFeatures(sim, kind, actor, &options[i], i == count - 1, x + POLICY_KINDS + ACTOR_FEATURES);
        const double *w1 = policy_weights, *b1 = w1 + POLICY_INPUTS * POLICY_HIDDEN,
                     *w2 = b1 + POLICY_HIDDEN, b2 = w2[POLICY_HIDDEN];
        double total = b2;
        for (int h = 0; h < POLICY_HIDDEN; ++h) {
            double z = b1[h];
            for (int j = 0; j < POLICY_INPUTS; ++j) z += x[j] * w1[j * POLICY_HIDDEN + h];
            total += tanh(z) * w2[h];
        }
        score[i] = total;
        if (total > best) { best = total; argmax = i; }
    }
    /* The rule's option wins ties, so zero weights reproduce the rule exactly. */
    int32_t pick = score[fallback] >= best - 1e-9 ? fallback : argmax;
    policy_offered[(int)kind] += 1;
    policy_changed[(int)kind] += pick != fallback;
    return pick;
}

/* Probe: with biases, trade follows the rule's score plus a*hunger + b*need + c*path cost. */
void cs_set_trade_bias(double a, double b, double c, int on)
{
    trade_bias[0] = a; trade_bias[1] = b; trade_bias[2] = c; trade_bias_on = on;
    if (on) { policy_mask |= 32; CcSimSetPolicy(Choose, NULL); } else if (!policy_active && gossip_floor < 0) CcSimSetPolicy(NULL, NULL);
}

/* Probe: with a floor, gossip follows "withhold below this confidence" and ignores the network. */
void cs_set_gossip_floor(int floor) { gossip_floor = floor; if (floor >= 0) { policy_mask |= 16; CcSimSetPolicy(Choose, NULL); } else if (!policy_active) CcSimSetPolicy(NULL, NULL); }

/* Probe: the dragon always burns the k-th eligible town (-1 clears). */
void cs_set_dragon_force(int k) { dragon_force = k; if (k >= 0) { policy_mask |= 256; CcSimSetPolicy(Choose, NULL); } else if (!policy_active && gossip_floor < 0 && !trade_bias_on) CcSimSetPolicy(NULL, NULL); }

/* Give the dragon a theft to collect on: it retaliates after a short omen. */
void cs_inject_theft(CrowdSim *w, int amount)
{
    w->sim.dragon.slain = false;
    w->sim.dragon.stolen_outstanding = amount;
    w->sim.dragon.retaliation_target_id = 0U;
    w->sim.dragon.omen_days_remaining = 2;
    w->sim.dragon.theft_actor_id = w->sim.hoard_raiders.id;
}

void cs_set_policy(const double *weights, int count, int mask)
{
    policy_mask = mask;
    if (weights == NULL || count != POLICY_WEIGHTS) { policy_active = 0; CcSimSetPolicy(NULL, NULL); return; }
    memcpy(policy_weights, weights, sizeof(policy_weights));
    policy_active = 1;
    CcSimSetPolicy(Choose, NULL);
}

CrowdSim *cs_new(uint32_t seed, int days)
{
    CrowdSim *w = calloc(1U, sizeof(*w));
    if (w == NULL) return NULL;
    CcSimInit(&w->sim, seed);
    if (days > 0) CcSimAdvanceDays(&w->sim, days);
    return w;
}

/* Advance day by day and total who is hungry, unsheltered or outlawed. Road-going roles
 * (scout, traveller, refugee, courier) are the people whose choices the hook changes.
 * metrics: 0 road person-days, 1 road hungry days, 2 road unsheltered, 3 road bandit,
 * 4 road stress, 5 road coins at the end, 6 all person-days, 7 all hungry days, 8 all bandit,
 * 9 town changes by road-going people (a tracked invariant: a policy must not stop travelling),
 * 10 known (story, town) pairs, 11 their confidence total, 12 their retellings total, 13 story-days,
 * 14 town hunger summed over town-days, 15 town-days in famine (hunger >= 25), 16 town prosperity summed,
 * 17 town-days, 18 raids on towns, 19 goods taken in raids, 20 kingdom legitimacy summed,
 * 21 kingdom treasury summed, 22 kingdom-days, and at the end: 23 dragon hoard, 24 dragon memory integrity,
 * 25 dragon retaliations, 26 population, 27 town prosperity summed, 28 kingdom legitimacy summed. */
int cs_run(CrowdSim *w, int days, double *metrics)
{
    static CcId where[CC_MAX_CHARACTERS_HINT];
    memset(metrics, 0, 29 * sizeof(double));
    CcId seen_event = 0U;
    for (int32_t i = 0; i < w->sim.event_count; ++i) if (w->sim.events[i].id > seen_event) seen_event = w->sim.events[i].id;
    for (int32_t i = 0; i < w->sim.character_count && i < CC_MAX_CHARACTERS_HINT; ++i)
        where[i] = w->sim.characters[i].current_settlement_id;
    for (int d = 0; d < days; ++d) {
        CcSimAdvanceDays(&w->sim, 1);
        for (int32_t i = 0; i < w->sim.event_count; ++i) {
            const CcEvent *event = &w->sim.events[i];
            if (event->id <= seen_event) continue;
            if (event->kind == CC_EVENT_SETTLEMENT_RAIDED) { metrics[18] += 1.0; metrics[19] += (double)event->magnitude; }
        }
        for (int32_t i = 0; i < w->sim.event_count; ++i) if (w->sim.events[i].id > seen_event) seen_event = w->sim.events[i].id;
        for (int32_t k = 0; k < w->sim.kingdom_count; ++k) {
            metrics[20] += (double)w->sim.kingdoms[k].legitimacy;
            metrics[21] += (double)w->sim.kingdoms[k].treasury;
            metrics[22] += 1.0;
        }
        for (int32_t t = 0; t < w->sim.settlement_count; ++t) {
            const CcSettlement *town = &w->sim.settlements[t];
            if (CcSettlementIsAbandoned(town)) continue;
            metrics[14] += (double)town->hunger; metrics[15] += town->hunger >= 25;
            metrics[16] += (double)town->prosperity; metrics[17] += 1.0;
        }
        for (int g = 0; g < CC_MAX_GOSSIP; ++g) {
            const CcGossip *story = &w->sim.gossip[g];
            if (story->event_id == 0U) continue;
            metrics[13] += 1.0;
            for (int t = 0; t < w->sim.settlement_count && t < CC_MAX_SETTLEMENTS && t < 32; ++t) {
                if (!((story->settlement_mask >> t) & 1U)) continue;
                metrics[10] += 1.0;
                metrics[11] += (double)story->local[t].confidence;
                metrics[12] += (double)story->local[t].retellings;
            }
        }
        for (int32_t i = 0; i < w->sim.character_count; ++i) {
            const CcCharacter *p = &w->sim.characters[i];
            if (i < CC_MAX_CHARACTERS_HINT) {
                if (where[i] != p->current_settlement_id && where[i] != 0U &&
                    (p->role == CC_CHARACTER_SCOUT || p->role == CC_CHARACTER_TRAVELLER ||
                     p->role == CC_CHARACTER_REFUGEE || p->role == CC_CHARACTER_COURIER)) metrics[9] += 1.0;
                where[i] = p->current_settlement_id;
            }
            if (p->death_day > 0 && p->death_day <= w->sim.current_day) continue;
            int road = p->role == CC_CHARACTER_SCOUT || p->role == CC_CHARACTER_TRAVELLER ||
                       p->role == CC_CHARACTER_REFUGEE || p->role == CC_CHARACTER_COURIER;
            metrics[6] += 1.0; metrics[7] += p->hungry_days > 0; metrics[8] += p->bandit_group_id != 0U;
            if (!road) continue;
            metrics[0] += 1.0; metrics[1] += p->hungry_days > 0; metrics[2] += p->unsheltered_nights > 0;
            metrics[3] += p->bandit_group_id != 0U; metrics[4] += p->stress;
        }
    }
    for (int32_t i = 0; i < w->sim.character_count; ++i) {
        const CcCharacter *p = &w->sim.characters[i];
        if (p->role == CC_CHARACTER_SCOUT || p->role == CC_CHARACTER_TRAVELLER ||
            p->role == CC_CHARACTER_REFUGEE || p->role == CC_CHARACTER_COURIER) metrics[5] += (double)p->travel_coins;
    }
    metrics[23] = (double)w->sim.dragon.hoard; metrics[24] = (double)w->sim.dragon.memory_integrity;
    metrics[25] = (double)w->sim.dragon.retaliations;
    for (int32_t t = 0; t < w->sim.settlement_count; ++t) {
        metrics[26] += (double)w->sim.settlements[t].population; metrics[27] += (double)w->sim.settlements[t].prosperity;
    }
    for (int32_t k = 0; k < w->sim.kingdom_count; ++k) metrics[28] += (double)w->sim.kingdoms[k].legitimacy;
    return w->sim.current_day;
}
