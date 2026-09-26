/* Show The Return's change digest without a window.

   crownless_return_digest [--seed N] [--from TOWN] [--to TOWN] [--days N]
                           [--voice]

   A new world starts with the company in FROM. The company rides to TO by the
   real roads, waits there N days, and rides back. Each arrival prints the
   digest for that town: what changed since the company last left it.

   With --voice, each return also prints the gate voice (milestone 3): who
   speaks, each line with the evidence of every clause, and the digest again
   after the heard stories are marked as told.

   Every leg also prints the news the company met on the road (milestone 4):
   a traveller's story, a notice at the milestone, or smoke on the horizon,
   with the line the travel view shows. */

#include "sim/cc_return.h"
#include "sim/cc_return_ride.h"
#include "sim/cc_road_news.h"
#include "sim/cc_sim.h"
#include "story/cc_gate_voice.h"
#include "story/cc_road_voice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char error[256];
static bool voice_mode;

static CcId TownByName(const CcSim *sim, const char *name)
{
    return CcReturnRideTownByName(sim, name);
}

static void PrintDigest(const CcSim *sim, CcId town)
{
    CcReturnDigest digest;
    char text[8192];
    if (!CcReturnDigestBuild(sim, town, &digest)) return;
    (void)CcReturnDigestText(sim, &digest, text, sizeof(text));
    (void)fputs(text, stdout);
}

static void PrintVoice(const CcGateVoice *voice)
{
    (void)printf("   %s (%s%s): \"%s\"\n", voice->speaker, voice->speaker_label,
                 voice->remembered_face ? ", remembered" : "", voice->line);
    (void)printf("     [%s] %s, confidence %d", CcReturnChangeKindName(voice->change.kind),
                 voice->telling == CC_GATE_VOICE_HEARD ? "heard" : "seen",
                 voice->confidence);
    if (voice->telling == CC_GATE_VOICE_HEARD)
        (void)printf(", story slot %d, %d retellings, asked %d, chose fact %d of %d",
                     voice->story_slot, voice->version.retellings, (int)voice->asked_role,
                     voice->chosen_fact, voice->fact_count);
    (void)printf("\n");
    for (int32_t i = 0; i < voice->clause_count; ++i)
        (void)printf("     - %-8s %-6s%s %llu: %s\n",
                     CcGateVoicePartName(voice->clauses[i].part),
                     CcGateVoiceEvidenceName(voice->clauses[i].evidence),
                     voice->clauses[i].town_state ? "+town" : "",
                     (unsigned long long)voice->clauses[i].evidence_id,
                     voice->clauses[i].text);
}

/* Everything the gate voice would say on this return, marking each heard
   story as told the way the client does. */
static void Listen(CcSim *sim, CcId town)
{
    CcGateVoice voice;
    if (!CcGateVoiceBegin(sim, town, &voice)) {
        (void)printf("   (nobody at the gate has news)\n");
        return;
    }
    (void)printf("-- Gate voice --\n");
    for (int32_t turn = 0; turn < CC_RETURN_MAX_CHANGES; ++turn) {
        PrintVoice(&voice);
        CcCommand told;
        if (CcGateVoiceToldCommand(&voice, &told) &&
            !CcSimApply(sim, &told, error, sizeof(error)))
            (void)printf("     (not marked: %s)\n", error);
        voice.more = CcGateVoiceHasMore(sim, &voice);
        if (!voice.more || !CcGateVoiceNext(sim, &voice)) break;
    }
    (void)printf("-- Digest after the voice --\n");
    PrintDigest(sim, town);
}

/* News met on the road, written the moment it is met: the line is what the
   travel view shows then, not after the traveller has moved on. */
typedef struct RoadLog {
    uint64_t last_tick;
    int32_t last_channel;
    int32_t used;
    char text[8192];
} RoadLog;

static bool LogRoadNews(const CcSim *sim, void *context)
{
    RoadLog *log = context;
    CcId town = 0U;
    const CcRoadNews *news = CcRoadNewsLatest(sim, &town);
    if (news == NULL || (news->tick == log->last_tick && news->channel == log->last_channel))
        return false;
    log->last_tick = news->tick;
    log->last_channel = news->channel;
    CcRoadVoice voice;
    bool said = CcRoadVoiceBuild(sim, town, news, &voice);
    char line[1024];
    int written = snprintf(line, sizeof(line),
        "   On the road to %s, day %d: [%s] %s, about %s. %s%s%s%s: %s (%s, confidence %d)\n",
        CcSimSettlement(sim, sim->journey.destination_id)->name, news->day,
        CcRoadNewsChannelName((CcRoadNewsChannel)news->channel),
        CcReturnChangeKindName((CcReturnChangeKind)news->kind),
        CcSimSettlement(sim, town)->name,
        said ? voice.speaker : "",
        said && voice.speaker_label[0] != '\0' ? " (" : "",
        said ? voice.speaker_label : "",
        said && voice.speaker_label[0] != '\0' ? ")" : "",
        said ? voice.line : "(no line)", said ? voice.source : "", news->confidence);
    for (int32_t i = 0; said && voice.channel == CC_ROAD_NEWS_TOLD &&
         i < voice.voice.clause_count && written > 0 && (size_t)written < sizeof(line); ++i)
        written += snprintf(line + written, sizeof(line) - (size_t)written,
                            "     - %-8s %-6s %llu: %s\n",
                            CcGateVoicePartName(voice.voice.clauses[i].part),
                            CcGateVoiceEvidenceName(voice.voice.clauses[i].evidence),
                            (unsigned long long)voice.voice.clauses[i].evidence_id,
                            voice.voice.clauses[i].text);
    if (written > 0)
        log->used += snprintf(log->text + log->used, sizeof(log->text) - (size_t)log->used,
                              "%s", line);
    if (log->used >= (int32_t)sizeof(log->text)) log->used = (int32_t)sizeof(log->text) - 1;
    return false;
}

static bool RideAll(CcSim *sim, CcId to)
{
    CcId path[CC_MAX_SETTLEMENTS];
    int32_t legs = CcReturnRideRoadPath(sim, sim->player.location_id, to, path);
    if (legs == 0) return false;
    for (int32_t i = 0; i < legs; ++i) {
        static RoadLog log;
        log.used = 0;
        log.text[0] = '\0';
        /* The log never stops the ride; the leg ends at the next town. */
        (void)CcReturnRideUntil(sim, path[i], LogRoadNews, &log, error, sizeof(error));
        if (sim->journey.active || sim->player.location_id != path[i]) {
            (void)fprintf(stderr, "The ride to %s stopped: %s\n",
                          CcSimSettlement(sim, path[i])->name, error);
            return false;
        }
        (void)printf("\n== Arrive ");
        PrintDigest(sim, path[i]);
        (void)fputs(log.text, stdout);
        if (voice_mode) Listen(sim, path[i]);
    }
    return true;
}

int main(int argc, char **argv)
{
    uint32_t seed = 1U;
    int32_t days = 60;
    const char *from = "Thornford";
    const char *to = "Silverwick";
    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "--voice") == 0) {
            voice_mode = true;
            --i;
            continue;
        }
        if (i + 1 >= argc) {
            (void)fprintf(stderr, "%s needs a value\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--seed") == 0) seed = (uint32_t)strtoul(argv[i + 1], NULL, 10);
        else if (strcmp(argv[i], "--days") == 0) days = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "--from") == 0) from = argv[i + 1];
        else if (strcmp(argv[i], "--to") == 0) to = argv[i + 1];
        else {
            (void)fprintf(stderr, "usage: %s [--seed N] [--from TOWN] [--to TOWN] [--days N] [--voice]\n", argv[0]);
            return 2;
        }
    }
    if (days < 0 || days > 3650) {
        (void)fprintf(stderr, "days must be from 0 to 3650.\n");
        return 2;
    }
    CcSim *sim = calloc(1U, sizeof(*sim));
    if (sim == NULL) return 1;
    CcSimInit(sim, seed);
    CcId home = TownByName(sim, from), away = TownByName(sim, to);
    if (home == 0U || away == 0U || home == away) {
        (void)fprintf(stderr, "unknown or equal towns: %s, %s\n", from, to);
        free(sim);
        return 2;
    }
    if (sim->player.location_id != home) {
        sim->player.location_id = home;
        sim->carriage.location_id = home;
    }
    (void)printf("seed %u, schema %u: the company leaves %s for %s, waits %d days, and returns.\n",
                 seed, sim->schema_version, from, to, days);
    bool ok = RideAll(sim, away);
    if (ok) {
        CcSimAdvanceDays(sim, days);
        (void)printf("\n-- %d days pass in %s --\n", days, to);
        ok = RideAll(sim, home);
    }
    (void)printf("\nstate hash %016llx\n", (unsigned long long)CcSimHash(sim));
    free(sim);
    return ok ? 0 : 1;
}
