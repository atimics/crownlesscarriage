#include "persistence/cc_save.h"
#include "sim/cc_food_relief.h"

#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool Id(const char *text, uint64_t *out)
{
    char *end = NULL;
    if (text == NULL || text[0] == '\0') return false;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0' || value == 0U) return false;
    *out = (uint64_t)value; return true;
}

static void Print(const CcFoodReliefOutcome *out)
{
    static const char *const names[] = {"none", "proposed", "accepted", "fulfilled", "failed"};
    printf("{\"agreement_id\":\"%" PRIu64 "\",\"status\":\"%s\",\"payer_id\":\"%" PRIu64
           "\",\"beneficiary_id\":\"%" PRIu64 "\",\"place_id\":\"%" PRIu64
           "\",\"quantity\":%d,\"unit_price\":%d,\"total_cost\":%" PRId64
           ",\"beneficiary_hungry_days\":%d,\"event_id\":\"%" PRIu64 "\"}\n",
        out->agreement_id, out->kind >= 0 && out->kind <= CC_FOOD_RELIEF_OUTCOME_FAILED ? names[out->kind] : "unknown", out->payer_id, out->beneficiary_id,
        out->place_id, out->quantity, out->unit_price, out->total_cost,
        out->beneficiary_hungry_days, out->event_id);
}

int main(int argc, char **argv)
{
    static CcSim sim;
    uint32_t seed = 1202U;
    const char *load = NULL, *save = NULL;
    CcId payer = 0U, beneficiary = 0U, agreement = 0U;
    int32_t quantity = 0, price = 0;
    int32_t days = 0;
    enum { ACTION_LIST, ACTION_OBSERVE, ACTION_PROMISE, ACTION_ACCEPT, ACTION_EXECUTE, ACTION_OUTCOMES } action = ACTION_LIST;
    CcId other = 0U;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 >= argc && strcmp(argv[i], "--list") != 0) return 2;
        uint64_t parsed = 0U;
        if (strcmp(argv[i], "--list") == 0) action = ACTION_LIST;
        else if (strcmp(argv[i], "--seed") == 0) { if (!Id(argv[++i], &parsed) || parsed > UINT32_MAX) return 2; seed = (uint32_t)parsed; }
        else if (strcmp(argv[i], "--load") == 0) load = argv[++i];
        else if (strcmp(argv[i], "--save") == 0) save = argv[++i];
        else if (strcmp(argv[i], "--days") == 0) { if (!Id(argv[++i], &parsed) || parsed > 36500U) return 2; days = (int32_t)parsed; }
        else if (strcmp(argv[i], "--observe") == 0) { if (i + 2 >= argc || !Id(argv[++i], &parsed)) return 2; payer = (CcId)parsed; if (!Id(argv[++i], &parsed)) return 2; beneficiary = (CcId)parsed; action = ACTION_OBSERVE; }
        else if (strcmp(argv[i], "--promise") == 0) { if (i + 4 >= argc || !Id(argv[++i], &parsed)) return 2; payer = (CcId)parsed; if (!Id(argv[++i], &parsed)) return 2; beneficiary = (CcId)parsed; if (!Id(argv[++i], &parsed) || parsed > CC_SIM_MAX_UNITS) return 2; quantity = (int32_t)parsed; if (!Id(argv[++i], &parsed) || parsed > INT32_MAX) return 2; price = (int32_t)parsed; action = ACTION_PROMISE; }
        else if (strcmp(argv[i], "--accept") == 0) { if (i + 2 >= argc || !Id(argv[++i], &parsed)) return 2; agreement = (CcId)parsed; if (!Id(argv[++i], &parsed)) return 2; beneficiary = (CcId)parsed; action = ACTION_ACCEPT; }
        else if (strcmp(argv[i], "--execute") == 0) { if (i + 2 >= argc || !Id(argv[++i], &parsed)) return 2; agreement = (CcId)parsed; if (!Id(argv[++i], &parsed)) return 2; payer = (CcId)parsed; action = ACTION_EXECUTE; }
        else if (strcmp(argv[i], "--outcomes") == 0) { if (i + 2 >= argc || !Id(argv[++i], &parsed)) return 2; payer = (CcId)parsed; if (!Id(argv[++i], &parsed)) return 2; other = (CcId)parsed; action = ACTION_OUTCOMES; }
        else return 2;
    }
    char error[256];
    if (load != NULL) { if (!CcSaveRead(load, &sim, error, sizeof(error))) { fputs(error, stderr); return 1; } }
    else CcSimInit(&sim, seed);
    if (days > 0 && load == NULL) CcSimAdvanceDays(&sim, days);
    if (action == ACTION_LIST) {
        printf("{\"day\":%d,\"people\":[", sim.current_day);
        for (int32_t i = 0; i < sim.character_count; ++i) {
            const CcCharacter *person = &sim.characters[i];
            printf("%s{\"id\":\"%" PRIu64 "\",\"name\":\"%s\",\"place_id\":\"%" PRIu64 "\",\"hungry_days\":%d}",
                i ? "," : "", person->id, person->name, person->current_settlement_id, person->hungry_days);
        }
        puts("]}");
        if (save != NULL && !CcSaveWrite(save, &sim, error, sizeof(error))) { fputs(error, stderr); return 1; }
        return 0;
    }
    if (action == ACTION_OUTCOMES) {
        bool first = true;
        putchar('{'); fputs("\"outcomes\":[", stdout);
        for (int32_t i = 0; i < sim.event_count; ++i) {
            const CcEvent *event = &sim.events[i];
            CcFoodReliefOutcome outcome;
            if (event->kind != CC_EVENT_RELIEF || event->subject_id != event->id ||
                event->actor_id != payer || event->target_id != other ||
                !CcFoodReliefRead(&sim, event->id, &outcome) ||
                (outcome.kind != CC_FOOD_RELIEF_OUTCOME_FULFILLED && outcome.kind != CC_FOOD_RELIEF_OUTCOME_FAILED)) continue;
            printf("%s{\"outcome\":\"%s\",\"quantity\":%d,\"total_cost\":%" PRId64 ",\"event_id\":\"%" PRIu64 "\",\"actor_id\":\"%" PRIu64 "\",\"beneficiary_id\":\"%" PRIu64 "\",\"reason\":\"outcome\"}",
                first ? "" : ",", outcome.kind == CC_FOOD_RELIEF_OUTCOME_FULFILLED ? "fulfilled" : "failed",
                outcome.quantity, outcome.total_cost, outcome.event_id, outcome.payer_id, outcome.beneficiary_id);
            first = false;
        }
        puts("]}");
    } else if (action == ACTION_OBSERVE) {
        CcFoodReliefObservation observation;
        if (!CcFoodReliefObserve(&sim, payer, beneficiary, &observation, error, sizeof(error))) { fputs(error, stderr); return 1; }
        printf("{\"kind\":\"food_store\",\"payer_id\":\"%" PRIu64 "\",\"beneficiary_id\":\"%" PRIu64 "\",\"place_id\":\"%" PRIu64 "\",\"place_name\":\"%s\",\"stock\":%d,\"unit_price\":%d,\"day\":%d}\n", observation.payer_id, observation.beneficiary_id, observation.place_id, observation.place_name, observation.stock, observation.unit_price, observation.day);
    } else {
        CcFoodReliefOutcome outcome;
        const CcCharacter *payer_character = CcSimCharacter(&sim, payer);
        bool ok = action == ACTION_PROMISE && payer_character != NULL ? CcFoodReliefPropose(&sim, &(CcFoodReliefProposal){payer, beneficiary, payer_character->current_settlement_id, quantity, price}, &outcome, error, sizeof(error)) :
            action == ACTION_ACCEPT ? CcFoodReliefAccept(&sim, agreement, beneficiary, &outcome, error, sizeof(error)) :
            CcFoodReliefExecute(&sim, agreement, payer, &outcome, error, sizeof(error));
        if (!ok && action != ACTION_EXECUTE) { fputs(error, stderr); return 1; }
        Print(&outcome);
    }
    if (save != NULL && !CcSaveWrite(save, &sim, error, sizeof(error))) { fputs(error, stderr); return 1; }
    return 0;
}
