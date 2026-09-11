/* Stream real personal accounts through the game's shared language rules. */
#include "story/cc_speech.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CcSim sim;
#define KIND_COUNT ((int)CC_EVENT_NOTICE_POSTED + 1)
static uint64_t unsupported_kinds[KIND_COUNT];
static char unsupported_sample[KIND_COUNT][CC_EVENT_TEXT_CAPACITY];
typedef struct SeenAccount {
    CcId speaker;
    CcId event;
    CcGossipLanguage language;
} SeenAccount;
static SeenAccount seen[CC_MAX_CHARACTERS][CC_MAX_GOSSIP];

static void JsonString(FILE *stream, const char *text)
{
    (void)fputc('"', stream);
    for (const unsigned char *at = (const unsigned char *)text; *at != 0U; ++at) {
        if (*at < 32U) (void)fprintf(stream, "\\u%04x", (unsigned int)*at);
        else {
            if (*at == '"' || *at == '\\') (void)fputc('\\', stream);
            (void)fputc((int)*at, stream);
        }
    }
    (void)fputc('"', stream);
}

static void JsonEvent(const CcGossip *story, const CcGossipVersion *version,
                       const CcGossipLanguage *language)
{
    (void)printf("{\"event_id\":\"%" PRIu64 "\",\"day\":%" PRId32 ",\"text\":",
                 story->event_id, story->day);
    /* Generalised details are also removed from the model's visible event. */
    JsonString(stdout, language->detail == CC_GOSSIP_DETAIL_FULL || language->claim[0] == '\0' ?
        language->account : language->claim);
    (void)printf(",\"confidence\":%" PRId32 ",\"retellings\":%" PRId32 "}",
                 version->confidence, version->retellings);
}

static void JsonEvents(const CcCharacter *speaker, int32_t offset,
                        const CcGossip *story, const CcGossipVersion *version,
                        const CcGossipLanguage *language)
{
    const CcGossip *prior[2];
    const CcGossipVersion *versions[2];
    int32_t count = 0;
    for (int32_t i = offset + 1; i < CC_MAX_GOSSIP && count < 2; ++i) {
        const CcGossipVersion *held = NULL;
        const CcGossip *older = CcSimPersonalGossip(&sim, speaker->id, i, &held);
        if (older == NULL || held == NULL) break;
        if (older->day > story->day) continue;
        prior[count] = older;
        versions[count++] = held;
    }
    (void)printf(",\"events\":[");
    for (int32_t i = count; i > 0; --i) {
        CcGossipLanguage earlier;
        (void)CcSpeechPrepareGossip(&sim, prior[i - 1], versions[i - 1], 0U, &earlier);
        JsonEvent(prior[i - 1], versions[i - 1], &earlier);
        (void)putchar(',');
    }
    JsonEvent(story, version, language);
    (void)putchar(']');
}

static bool Number(const char *text, uint32_t maximum, uint32_t *value)
{
    if (text[0] < '0' || text[0] > '9') return false;
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > maximum) return false;
    *value = (uint32_t)parsed;
    return true;
}

static bool ExportDay(uint64_t *rows, uint64_t *unsupported)
{
    uint64_t before = CcSimHash(&sim);
    for (int32_t c = 0; c < sim.character_count; ++c) {
        const CcCharacter *speaker = &sim.characters[c];
        for (int32_t offset = 0; offset < CC_MAX_GOSSIP; ++offset) {
            const CcGossipVersion *version = NULL;
            const CcGossip *story = CcSimPersonalGossip(&sim, speaker->id, offset, &version);
            if (story == NULL || version == NULL) break;
            CcGossipLanguage language;
            bool supported = CcSpeechPrepareGossip(&sim, story, version, 0U, &language);
            /* Personal offsets move as newer stories arrive. The backing gossip
               slot stays stable for this event, so keep its sample there. */
            size_t story_slot = (size_t)(story - sim.gossip);
            SeenAccount *previous = &seen[c][story_slot];
            if (previous->speaker == speaker->id && previous->event == story->event_id &&
                memcmp(&previous->language, &language, sizeof(language)) == 0) continue;
            previous->speaker = speaker->id;
            previous->event = story->event_id;
            previous->language = language;
            if (!supported) {
                ++*unsupported;
                int kind = (int)story->kind;
                if (kind >= 0 && kind < KIND_COUNT) {
                    ++unsupported_kinds[kind];
                    if (unsupported_sample[kind][0] == '\0') {
                        (void)snprintf(unsupported_sample[kind], CC_EVENT_TEXT_CAPACITY,
                                       "%s", language.account);
                    }
                }
                continue;
            }
            for (uint32_t variant = 0; variant < 2U; ++variant) {
                char speech[CC_SPEECH_TEXT_CAPACITY];
                if (!CcSpeechPrepareGossip(&sim, story, version, variant, &language) ||
                    !CcSpeechCoreGossip(&language, speech, sizeof(speech))) return false;
                (void)printf("{\"version\":%d,\"provenance\":{\"world_seed\":%" PRIu32
                    ",\"schema\":%" PRIu32 ",\"day\":%" PRId32
                    ",\"event_id\":\"%" PRIu64 "\",\"speaker_id\":\"%" PRIu64
                    "\",\"source_character_id\":\"%" PRIu64 "\"},\"input\":{\"kind\":",
                    CC_GOSSIP_LANGUAGE_VERSION, sim.world_seed, sim.schema_version,
                    sim.current_day, story->event_id, speaker->id, version->source_character_id);
                JsonString(stdout, CcEventKindName(language.kind));
                (void)printf(",\"account\":");
                JsonString(stdout, language.account);
                (void)printf(",\"detail\":");
                JsonString(stdout, language.detail == CC_GOSSIP_DETAIL_ACTOR ? "actor" :
                    language.detail == CC_GOSSIP_DETAIL_SUBJECT ? "subject" : "full");
                (void)printf(",\"confidence\":%" PRId32 ",\"retellings\":%" PRId32
                    ",\"variant\":%" PRIu32 "},\"output\":", language.confidence,
                    language.retellings, variant);
                JsonString(stdout, speech);
                JsonEvents(speaker, offset, story, version, &language);
                (void)printf(",\"rule\":\"%d:%" PRIu32 "\"}\n", (int)language.kind, variant);
                ++*rows;
            }
        }
    }
    return !ferror(stdout) && CcSimHash(&sim) == before;
}

int main(int argc, char **argv)
{
    uint32_t seed = 1U, days = 120U;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 < argc && strcmp(argv[i], "--seed") == 0) {
            if (!Number(argv[++i], UINT32_MAX, &seed)) return 2;
        } else if (i + 1 < argc && strcmp(argv[i], "--days") == 0) {
            if (!Number(argv[++i], 36500U, &days)) return 2;
        } else {
            (void)fprintf(stderr, "Usage: %s [--seed UINT32] [--days 0..36500]\n", argv[0]);
            return 2;
        }
    }
    CcSimInit(&sim, seed);
    uint64_t rows = 0U, unsupported = 0U;
    for (uint32_t day = 0U; day <= days; ++day) {
        if (!ExportDay(&rows, &unsupported)) {
            (void)fprintf(stderr, "Gossip export failed or changed simulation state.\n");
            return 1;
        }
        if (day < days) CcSimAdvanceDays(&sim, 1);
    }
    char error[512];
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        (void)fprintf(stderr, "Simulation validation: %s\n", error);
        return 1;
    }
    if (fflush(stdout) != 0) return 1;
    (void)fprintf(stderr, "{\"world_seed\":%" PRIu32 ",\"rows\":%" PRIu64
        ",\"unsupported_observations\":%" PRIu64 ",\"unsupported_kinds\":[",
        seed, rows, unsupported);
    bool comma = false;
    for (int kind = 0; kind < KIND_COUNT; ++kind) {
        if (unsupported_kinds[kind] == 0U) continue;
        (void)fprintf(stderr, "%s{\"kind\":", comma ? "," : "");
        JsonString(stderr, CcEventKindName((CcEventKind)kind));
        (void)fprintf(stderr, ",\"observations\":%" PRIu64 ",\"sample\":", unsupported_kinds[kind]);
        JsonString(stderr, unsupported_sample[kind]);
        (void)fputc('}', stderr);
        comma = true;
    }
    (void)fprintf(stderr, "]}\n");
    return 0;
}
