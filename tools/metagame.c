#include "metagame/cc_metagame.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    uint32_t seed = UINT32_C(0xc0a71a9e);
    for (int32_t argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--seed") == 0 && argument + 1 < argc) {
            char *end = NULL;
            unsigned long parsed = strtoul(argv[++argument], &end, 0);
            if (end == argv[argument] || *end != '\0' ||
                parsed > UINT32_MAX) {
                (void)fprintf(stderr, "Seed must be a 32-bit number.\n");
                return EXIT_FAILURE;
            }
            seed = (uint32_t)parsed;
        } else {
            (void)fprintf(stderr, "Usage: %s [--seed NUMBER]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    CcMetagame metagame;
    CcMetagameInit(&metagame, seed);
    char output[16384];
    CcMetagameIntro(&metagame, output, sizeof(output));
    (void)fputs(output, stdout);

    char line[256];
    while (!metagame.quit_requested) {
        (void)fputs("\n> ", stdout);
        (void)fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        (void)CcMetagameExecute(&metagame, line, output, sizeof(output));
        (void)fputs(output, stdout);
    }
    return EXIT_SUCCESS;
}
