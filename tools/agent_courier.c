#include "metagame/cc_metagame.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void PrintJsonString(const char *text)
{
    (void)fputc('"', stdout);
    if (text != NULL) {
        for (const unsigned char *cursor = (const unsigned char *)text;
             *cursor != '\0'; ++cursor) {
            switch (*cursor) {
                case '"': (void)fputs("\\\"", stdout); break;
                case '\\': (void)fputs("\\\\", stdout); break;
                case '\b': (void)fputs("\\b", stdout); break;
                case '\f': (void)fputs("\\f", stdout); break;
                case '\n': (void)fputs("\\n", stdout); break;
                case '\r': (void)fputs("\\r", stdout); break;
                case '\t': (void)fputs("\\t", stdout); break;
                default:
                    if (*cursor < 0x20U) {
                        (void)fprintf(stdout, "\\u%04x", *cursor);
                    } else {
                        (void)fputc((int)*cursor, stdout);
                    }
                    break;
            }
        }
    }
    (void)fputc('"', stdout);
}

static void PrintObservation(const CcMetagame *metagame, uint64_t turn)
{
    char observation[32768];
    CcMetagameAgentObserve(metagame, observation, sizeof(observation));
    (void)fprintf(stdout,
                  "{\"type\":\"observation\","
                  "\"protocol\":\"crownless-courier/1\","
                  "\"turn\":%" PRIu64 ",\"day\":%d,"
                  "\"tick\":%" PRIu64 ","
                  "\"state_hash\":\"%016" PRIx64 "\",\"text\":",
                  turn, metagame->sim.current_day,
                  metagame->sim.clock.tick, CcSimHash(&metagame->sim));
    PrintJsonString(observation);
    (void)fputs("}\n", stdout);
    (void)fflush(stdout);
}

static void PrintResult(const CcMetagame *metagame, uint64_t turn,
                        bool accepted, const char *action,
                        const char *result)
{
    (void)fprintf(stdout,
                  "{\"type\":\"result\",\"turn\":%" PRIu64
                  ",\"accepted\":%s,\"day\":%d,"
                  "\"tick\":%" PRIu64 ","
                  "\"state_hash\":\"%016" PRIx64 "\",\"action\":",
                  turn, accepted ? "true" : "false",
                  metagame->sim.current_day, metagame->sim.clock.tick,
                  CcSimHash(&metagame->sim));
    PrintJsonString(action);
    (void)fputs(",\"text\":", stdout);
    PrintJsonString(result);
    (void)fputs("}\n", stdout);
    (void)fflush(stdout);
}

static bool PrintCounterfactual(const CcMetagame *metagame)
{
    char report[32768];
    uint64_t control_hash = 0U;
    if (!CcMetagameAgentCounterfactual(
            metagame, report, sizeof(report), &control_hash)) {
        (void)fprintf(stderr, "%s", report);
        return false;
    }
    (void)fprintf(stdout,
                  "{\"type\":\"counterfactual\",\"day\":%d,"
                  "\"actual_state_hash\":\"%016" PRIx64 "\","
                  "\"control_state_hash\":\"%016" PRIx64 "\",\"text\":",
                  metagame->sim.current_day, CcSimHash(&metagame->sim),
                  control_hash);
    PrintJsonString(report);
    (void)fputs("}\n", stdout);
    (void)fflush(stdout);
    return true;
}

static bool ParseSeed(const char *text, uint32_t *seed)
{
    if (text == NULL || seed == NULL) return false;
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) return false;
    *seed = (uint32_t)parsed;
    return true;
}

static void Usage(const char *program)
{
    (void)fprintf(stderr,
                  "Usage: %s --journal PATH [--seed NUMBER | --resume] [--counterfactual]\n",
                  program);
}

int main(int argc, char **argv)
{
    const char *journal_path = NULL;
    uint32_t seed = UINT32_C(0xc0a71a9e);
    bool seed_set = false;
    bool resume = false;
    bool counterfactual = false;
    for (int32_t argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--journal") == 0 &&
            argument + 1 < argc) {
            journal_path = argv[++argument];
        } else if (strcmp(argv[argument], "--seed") == 0 &&
                   argument + 1 < argc &&
                   ParseSeed(argv[argument + 1], &seed)) {
            argument += 1;
            seed_set = true;
        } else if (strcmp(argv[argument], "--resume") == 0) {
            resume = true;
        } else if (strcmp(argv[argument], "--counterfactual") == 0) {
            counterfactual = true;
        } else {
            Usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (journal_path == NULL || (resume && seed_set)) {
        Usage(argv[0]);
        return EXIT_FAILURE;
    }

    CcMetagame *metagame = calloc(1U, sizeof(*metagame));
    if (metagame == NULL) {
        (void)fputs("Could not allocate the courier world.\n", stderr);
        return EXIT_FAILURE;
    }
    char error[192];
    bool opened = false;
    if (resume) {
        opened = CcMetagameResumeJournal(
            metagame, journal_path, error, sizeof(error));
    } else {
        CcMetagameInit(metagame, seed);
        opened = CcMetagameStartJournal(
            metagame, journal_path, error, sizeof(error));
    }
    if (!opened) {
        (void)fprintf(stderr, "%s\n", error);
        free(metagame);
        return EXIT_FAILURE;
    }

    uint64_t turn = 0U;
    PrintObservation(metagame, turn);
    char line[512];
    char result[32768];
    while (!metagame->quit_requested && fgets(line, sizeof(line), stdin) != NULL) {
        size_t length = strlen(line);
        bool complete = length > 0U && line[length - 1U] == '\n';
        if (complete) line[--length] = '\0';
        if (length > 0U && line[length - 1U] == '\r') line[--length] = '\0';
        if (!complete && !feof(stdin)) {
            int character = 0;
            while ((character = fgetc(stdin)) != '\n' && character != EOF) {}
            (void)snprintf(result, sizeof(result),
                           "The action is longer than the courier protocol allows.\n");
            turn += 1U;
            PrintResult(metagame, turn, false, line, result);
            PrintObservation(metagame, turn);
            continue;
        }
        turn += 1U;
        bool accepted = CcMetagameAgentExecute(
            metagame, line, result, sizeof(result));
        PrintResult(metagame, turn, accepted, line, result);
        if (!metagame->quit_requested) PrintObservation(metagame, turn);
    }

    bool closed = CcMetagameCloseJournal(
        metagame, error, sizeof(error));
    if (!closed) (void)fprintf(stderr, "%s\n", error);
    bool reported = !counterfactual ||
        (closed && PrintCounterfactual(metagame));
    free(metagame);
    return closed && reported ? EXIT_SUCCESS : EXIT_FAILURE;
}
