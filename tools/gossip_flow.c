/* Daily observations of gossip, intake, and physical archive work. */
#include "story/cc_speech.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CcSim sim;
static CcGossip previous[CC_MAX_GOSSIP];
typedef struct Held { CcId person, event, place; CcGossipVersion version; } Held;
static Held held[CC_MAX_CHARACTERS][CC_MAX_GOSSIP];
static CcTreasure volumes[CC_MAX_TREASURES];
static CcId last_event;

static void String(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p < 32) printf("\\u%04x", (unsigned)*p);
        else { if (*p == '"' || *p == '\\') putchar('\\'); putchar(*p); }
    }
    putchar('"');
}

static void Version(const CcGossip *g, const CcGossipVersion *v)
{
    char text[CC_EVENT_TEXT_CAPACITY];
    CcGossipText(&sim, g, v, text, sizeof(text));
    printf(",\"source_id\":\"%" PRIu64 "\",\"retellings\":%d,\"confidence\":%d,"
           "\"bias\":%d,\"alarm\":%d,\"account\":", v->source_character_id,
           v->retellings, v->confidence, v->court_bias, v->alarm);
    String(text);
    CcGossipLanguage language;
    char speech[CC_SPEECH_TEXT_CAPACITY];
    bool supported = CcSpeechPrepareGossip(&sim, g, v, 0, &language);
    printf(",\"core_supported\":%s,\"core_account\":", supported ? "true" : "false");
    String(language.account);
    if (supported && CcSpeechCoreGossip(&language, speech, sizeof(speech))) {
        printf(",\"reference_speech\":"); String(speech);
    }
}

static bool Observe(void)
{
    uint64_t before = CcSimHash(&sim);
    for (int32_t i = sim.event_count - 1; i >= 0; --i) {
        const CcEvent *e = CcSimRecentEvent(&sim, i);
        if (e == NULL || e->id <= last_event) continue;
        printf("{\"type\":\"event\",\"day\":%d,\"id\":\"%" PRIu64
               "\",\"kind\":%d,\"subject_id\":\"%" PRIu64 "\",\"parent_id\":\"%" PRIu64
               "\",\"place_id\":\"%" PRIu64 "\",\"actor_id\":\"%" PRIu64 "\",\"text\":",
               e->day, e->id, (int)e->kind, e->subject_id, e->parent_id, e->location_id, e->actor_id);
        String(e->text); puts("}"); last_event = e->id;
    }
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        const CcGossip *g = &sim.gossip[i];
        if (g->event_id == 0) continue;
        if (memcmp(g, &previous[i], sizeof(*g)) != 0) {
            printf("{\"type\":\"story\",\"day\":%d,\"event_id\":\"%" PRIu64
                   "\",\"event_day\":%d,\"kind\":%d,\"origin_id\":\"%" PRIu64
                   "\",\"heard_day\":%d,\"heard_event_id\":\"%" PRIu64
                   "\",\"recorded\":%s,\"original\":", sim.current_day, g->event_id,
                   g->day, (int)g->kind, g->origin_id, g->heard_day, g->heard_event_id,
                   g->recorded ? "true" : "false");
            String(g->text); printf(",\"heard_from\":"); String(g->heard_from);
            printf(",\"intake\":{"); printf("\"present\":%s", g->heard_day > 0 ? "true" : "false");
            Version(g, &g->heard); printf("},\"towns\":[");
            bool comma = false;
            for (int32_t s = 0; s < sim.settlement_count; ++s) {
                if ((g->settlement_mask & (UINT32_C(1) << (uint32_t)s)) == 0) continue;
                printf("%s{\"place_id\":\"%" PRIu64 "\",\"name\":", comma ? "," : "", sim.settlements[s].id);
                String(sim.settlements[s].name); Version(g, &g->local[s]); putchar('}'); comma = true;
            }
            puts("]}"); previous[i] = *g;
        }
        for (int32_t c = 0; c < sim.character_count; ++c) {
            const CcCharacter *person = &sim.characters[c];
            const CcGossipCarrier *carrier = CcSimGossipCarrier(&sim, person->id);
            if (carrier == NULL || (carrier->stories & (UINT32_C(1) << (uint32_t)i)) == 0) continue;
            Held *prior = &held[c][i];
            const CcGossipVersion *v = &carrier->versions[i];
            if (prior->person == person->id && prior->event == g->event_id &&
                prior->place == person->current_settlement_id && memcmp(&prior->version, v, sizeof(*v)) == 0) continue;
            *prior = (Held){person->id, g->event_id, person->current_settlement_id, *v};
            printf("{\"type\":\"held\",\"day\":%d,\"event_id\":\"%" PRIu64
                   "\",\"kind\":%d,\"person_id\":\"%" PRIu64 "\",\"place_id\":\"%" PRIu64
                   "\",\"occupation\":%d,\"name\":", sim.current_day, g->event_id, (int)g->kind,
                   person->id, person->current_settlement_id, (int)person->occupation);
            String(person->name); Version(g, v); puts("}");
        }
    }
    CcMaterialChainSnapshot chain = CcSimMaterialChainSnapshot(&sim);
    CcArchiveWorkPlan work = CcSimArchiveWorkPlan(&sim);
    printf("{\"type\":\"archive\",\"day\":%d,\"seat_id\":\"%" PRIu64
           "\",\"scribes\":%d,\"eligible_scribes\":%d,\"ready\":%s,\"wheat\":%d,"
           "\"paper\":%d,\"tools\":%d,\"lore\":%d,\"lost\":%d,\"blocker\":",
           sim.current_day, chain.scriptorium_id, sim.archives.scribes, work.eligible_scribes,
           work.recording_ready ? "true" : "false", chain.wheat, chain.paper, chain.tools,
           sim.archives.lore_stored, sim.archives.lore_lost_total);
    String(CcMaterialChainBlockerName(chain.blocker)); puts("}");
    for (int32_t i = 0; i < sim.treasure_count; ++i) {
        const CcTreasure *v = &sim.treasures[i];
        if (memcmp(v, &volumes[i], sizeof(*v)) == 0) continue;
        printf("{\"type\":\"treasure\",\"day\":%d,\"id\":\"%" PRIu64
               "\",\"created_day\":%d,\"owner_id\":\"%" PRIu64 "\",\"place_id\":\"%" PRIu64
               "\",\"destroyed\":%s,\"name\":", sim.current_day, v->id, v->created_day,
               v->owner_id, v->location_id, v->destroyed ? "true" : "false");
        String(v->name); puts("}"); volumes[i] = *v;
    }
    return !ferror(stdout) && before == CcSimHash(&sim);
}

static bool Number(const char *s, uint32_t max, uint32_t *out)
{
    if (*s < '0' || *s > '9') return false;
    errno = 0; char *end = NULL; unsigned long v = strtoul(s, &end, 10);
    if (errno || *end || v > max) return false;
    *out = (uint32_t)v; return true;
}

int main(int argc, char **argv)
{
    uint32_t seed = 73, days = 1092;
    bool hash_only = false;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 < argc && strcmp(argv[i], "--seed") == 0) {
            if (!Number(argv[++i], UINT32_MAX, &seed)) return 2;
        } else if (i + 1 < argc && strcmp(argv[i], "--days") == 0) {
            if (!Number(argv[++i], 36500, &days)) return 2;
        } else if (strcmp(argv[i], "--hash-only") == 0) hash_only = true;
        else return 2;
    }
    CcSimInit(&sim, seed);
    printf("{\"type\":\"world\",\"seed\":%" PRIu32 ",\"days\":%" PRIu32
           ",\"schema\":%" PRIu32 ",\"settlements\":[", seed, days, sim.schema_version);
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        printf("%s{\"id\":\"%" PRIu64 "\",\"name\":", i ? "," : "", sim.settlements[i].id);
        String(sim.settlements[i].name); putchar('}');
    }
    puts("]}");
    for (uint32_t day = 0; day <= days; ++day) {
        if (!hash_only && !Observe()) { fputs("Observation changed state or output failed.\n", stderr); return 1; }
        if (day < days) CcSimAdvanceDays(&sim, 1);
    }
    char error[512];
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }
    printf("{\"type\":\"finish\",\"day\":%d,\"hash\":\"%016" PRIx64 "\",\"valid\":true}\n",
           sim.current_day, CcSimHash(&sim));
    return fflush(stdout) == 0 ? 0 : 1;
}
