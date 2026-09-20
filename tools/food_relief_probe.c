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
    if (text == NULL || text[0] < '0' || text[0] > '9') return false;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0') return false;
    *out = (uint64_t)value; return true;
}

static void JsonString(const char *text)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p != 0U; ++p) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p < 32U) printf("\\u%04x", (unsigned int)*p);
        else putchar(*p);
    }
    putchar('"');
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
    char error[256] = {0};
    if (load != NULL) { if (!CcSaveRead(load, &sim, error, sizeof(error))) { fputs(error, stderr); return 1; } }
    else CcSimInit(&sim, seed);
    if (days > 0) CcSimAdvanceDays(&sim, days);
    if (action == ACTION_LIST) {
        printf("{\"day\":%d,\"people\":[", sim.current_day);
        for (int32_t i = 0; i < sim.character_count; ++i) {
            const CcCharacter *person = &sim.characters[i];
            printf("%s{\"id\":\"%" PRIu64 "\",\"name\":", i ? "," : "", person->id);
            JsonString(person->name);
            printf(",\"place_id\":\"%" PRIu64 "\",\"hungry_days\":%d,\"coins\":%" PRId64
                   ",\"in_transit\":%s,\"alive\":%s}", person->current_settlement_id,
                person->hungry_days, person->travel_coins,
                person->travel_destination_id != 0U || person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ? "true" : "false",
                person->death_day > 0 && person->death_day <= sim.current_day ? "false" : "true");
        }
        puts("]}");
        if (save != NULL && !CcSaveWrite(save, &sim, error, sizeof(error))) { fputs(error, stderr); return 1; }
        return 0;
    }
    if (action == ACTION_OUTCOMES) {
        bool first = true;
        putchar('{'); fputs("\"outcomes\":[", stdout);
        for (int32_t i = 0; i < sim.food_agreement_count; ++i) {
            const CcFoodAgreement *record = &sim.food_agreements[i];
            CcFoodReliefOutcome outcome;
            if (!((record->payer_id == payer && record->beneficiary_id == other) ||
                  (record->payer_id == other && record->beneficiary_id == payer)) ||
                !CcFoodReliefRead(&sim, record->id, &outcome) ||
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
        printf("{\"kind\":\"food_store\",\"payer_id\":\"%" PRIu64 "\",\"beneficiary_id\":\"%" PRIu64
               "\",\"place_id\":\"%" PRIu64 "\",\"place_name\":", observation.payer_id, observation.beneficiary_id, observation.place_id);
        JsonString(observation.place_name);
        printf(",\"stock\":%d,\"reserve_target\":%d,\"unit_price\":%d,\"payer_coins\":%" PRId64
               ",\"beneficiary_hungry_days\":%d,\"day\":%d}\n", observation.stock,
            observation.reserve_target, observation.unit_price, observation.payer_coins,
            observation.beneficiary_hungry_days, observation.day);
    } else {
        CcFoodReliefOutcome outcome = {0};
        CcCommand command = {0};
        if (action == ACTION_PROMISE) {
            const CcCharacter *person = CcSimCharacter(&sim, payer);
            if (person == NULL) { fputs("Unknown payer.\n", stderr); return 1; }
            command = (CcCommand){.kind = CC_COMMAND_FOOD_RELIEF_PROPOSE,
                .actor_id = payer, .target_id = beneficiary,
                .secondary_id = person->current_settlement_id,
                .amount = quantity, .good = (CcGood)price};
        } else {
            command = (CcCommand){.kind = action == ACTION_ACCEPT ?
                CC_COMMAND_FOOD_RELIEF_ACCEPT : CC_COMMAND_FOOD_RELIEF_EXECUTE,
                .actor_id = action == ACTION_ACCEPT ? beneficiary : payer,
                .target_id = agreement};
        }
        if (!CcSimApply(&sim, &command, error, sizeof(error))) { fputs(error, stderr); return 1; }
        if (action == ACTION_PROMISE) agreement = sim.food_agreements[sim.food_agreement_count - 1].id;
        if (!CcFoodReliefRead(&sim, agreement, &outcome)) { fputs("Missing agreement.\n", stderr); return 1; }
        Print(&outcome);
    }
    if (save != NULL && !CcSaveWrite(save, &sim, error, sizeof(error))) { fputs(error, stderr); return 1; }
    return 0;
}
