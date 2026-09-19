/* Read-only snapshots for two independently conditioned participants. */
#include "story/cc_core_participant.h"
#include "persistence/cc_save.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CcSim sim;
static void String(const char *value)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; *p != 0U; ++p) {
        if (*p < 32U) printf("\\u%04x", (unsigned int)*p);
        else { if (*p == '"' || *p == '\\') putchar('\\'); putchar(*p); }
    }
    putchar('"');
}
static void Id(CcId id) { printf("\"%" PRIu64 "\"", id); }
static void Field(const char *name, const char *value)
{
    printf(",\"%s\":", name); String(value);
}
static void Participant(const CcCoreParticipant *p)
{
    static const char *const goals[] = {"keep_order", "secure_livelihood", "survive_crisis", "carry_news"};
    static const char *const activities[] = {"working", "seeking_aid", "preparing", "recovering", "hiding", "travelling"};
    static const char *const histories[] = {"none", "old_friends", "former_partners", "professional_rivals", "coworkers"};
    printf("{\"self\":{\"id\":"); Id(p->id); Field("name", p->name);
    Field("occupation", CcCoreOccupationName(p->occupation));
    Field("goal", goals[p->goal]); Field("activity", activities[p->activity]);
    printf(",\"age\":%d,\"stress\":%d,\"courage\":%d,\"hungry_days\":%d,"
           "\"unsheltered_nights\":%d,\"coins\":%" PRId64 ",\"in_transit\":%s,\"home_id\":",
           p->age, p->stress, p->courage, p->hungry_days, p->unsheltered_nights,
           p->coins, p->in_transit ? "true" : "false");
    Id(p->home_id); Field("home", p->home); printf(",\"faction_id\":"); Id(p->faction_id);
    printf(",\"band_id\":"); Id(p->bandit_id); Field("band", p->band);
    printf("},\"listener\":{\"id\":"); Id(p->listener_id); Field("name", p->listener_name);
    printf("},\"place\":{\"id\":"); Id(p->place_id); Field("name", p->place);
    printf("},\"day\":%d,\"relationship\":", p->day);
    if (p->has_relationship) {
        printf("{\"affinity\":%d,\"trust\":%d,\"obligation\":%d,\"history\":",
               p->relationship.affinity, p->relationship.trust, p->relationship.obligation);
        String(histories[p->relationship.history]); printf(",\"cause_event_id\":"); Id(p->relationship.cause_event_id); putchar('}');
    } else printf("null");
    printf(",\"memories\":[");
    for (size_t i = 0; i < p->memory_count; ++i) {
        const CcCharacterMemory *m = &p->memories[i];
        printf("%s{\"kind\":%d,\"day\":%d,\"subject_id\":", i ? "," : "", (int)m->kind, m->day);
        Id(m->subject_id); printf(",\"event_id\":"); Id(m->event_id); putchar('}');
    }
    printf("],\"knowledge\":[");
    for (size_t i = 0; i < p->knowledge_count; ++i) {
        const CcCharacterKnowledge *k = &p->knowledge[i];
        printf("%s{\"kind\":%d,\"day\":%d,\"certainty\":%d,\"private\":%s,\"subject_id\":",
               i ? "," : "", (int)k->kind, k->day, (int)k->certainty, k->private_knowledge ? "true" : "false");
        Id(k->subject_id); printf(",\"event_id\":"); Id(k->event_id);
        printf(",\"source_id\":"); Id(k->source_character_id); Field("source_name", k->source_name);
        const CcCoreKnownEvent *detail = &p->knowledge_events[i];
        if (detail->available) {
            printf(",\"event_day\":%d", detail->day);
            Field("event_text", detail->text);
        } else {
            printf(",\"event_day\":null,\"event_text\":null");
        }
        putchar('}');
    }
    printf("],\"held_accounts\":[");
    for (size_t i = 0; i < p->account_count; ++i) {
        const CcCoreHeldAccount *a = &p->accounts[i];
        printf("%s{\"event_id\":", i ? "," : ""); Id(a->event_id);
        printf(",\"source_id\":"); Id(a->source_id);
        printf(",\"day\":%d,\"kind\":%d,\"confidence\":%d,\"retellings\":%d,\"parser_supported\":%s",
               a->day, (int)a->language.kind, a->language.confidence, a->language.retellings,
               a->supported ? "true" : "false");
        Field("account", a->telling); Field("model_account", a->language.account); putchar('}');
    }
    printf("],\"available_actions\":[\"end_conversation\"]}");
}
static bool Number(const char *text, uint64_t *value)
{
    if (*text < '0' || *text > '9') return false;
    errno = 0; char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0') return false;
    *value = (uint64_t)parsed; return true;
}
int main(int argc, char **argv)
{
    uint64_t seed = 1202U, days = 0U, first = 0U, second = 0U;
    const char *load = NULL;
    bool list = false, explicit_seed = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--list") == 0) { list = true; continue; }
        if (i + 1 >= argc) return 2;
        const char *flag = argv[i++];
        if (strcmp(flag, "--load") == 0) { load = argv[i]; continue; }
        uint64_t value;
        if (!Number(argv[i], &value)) return 2;
        if (strcmp(flag, "--seed") == 0 && value <= UINT32_MAX) { seed = value; explicit_seed = true; }
        else if (strcmp(flag, "--days") == 0 && value <= 36500U) days = value;
        else if (strcmp(flag, "--first") == 0) first = value;
        else if (strcmp(flag, "--second") == 0) second = value;
        else return 2;
    }
    if (load != NULL && explicit_seed) return 2;
    char error[256];
    if (load != NULL) {
        if (!CcSaveRead(load, &sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }
    } else CcSimInit(&sim, (uint32_t)seed);
    CcSimAdvanceDays(&sim, (int32_t)days);
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }
    uint64_t before = CcSimHash(&sim);
    if (list) {
        printf("{\"world_seed\":%" PRIu32 ",\"day\":%d,\"people\":[", sim.world_seed, sim.current_day);
        for (int32_t i = 0; i < sim.character_count; ++i) {
            const CcCharacter *p = &sim.characters[i];
            printf("%s{\"id\":", i ? "," : ""); Id(p->id); Field("name", p->name);
            printf(",\"place_id\":"); Id(p->current_settlement_id); printf("}");
        }
        printf("]}\n");
    } else {
        const CcCharacter *a = CcSimCharacter(&sim, first), *b = CcSimCharacter(&sim, second);
        if (a == NULL || b == NULL || first == second || a->current_settlement_id == 0U ||
            a->current_settlement_id != b->current_settlement_id ||
            a->travel_destination_id != 0U || b->travel_destination_id != 0U) {
            fputs("Choose two distinct people present in the same settlement.\n", stderr); return 2;
        }
        CcCoreParticipant p;
        printf("{\"version\":1,\"world_seed\":%" PRIu32 ",\"day\":%d,\"state_hash\":\"%016" PRIx64 "\",\"participants\":[",
               sim.world_seed, sim.current_day, before);
        if (!CcCoreParticipantBuild(&sim, first, second, &p)) return 1;
        Participant(&p); putchar(',');
        if (!CcCoreParticipantBuild(&sim, second, first, &p)) return 1;
        Participant(&p); printf("]}\n");
    }
    return !ferror(stdout) && before == CcSimHash(&sim) ? 0 : 1;
}
