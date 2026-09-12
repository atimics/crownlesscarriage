/* Small process boundary for checking the game's grammar from Python. */
#include "story/cc_core_account.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 5 || argc > 6) return 2;
    char *end = NULL;
    long kind = strtol(argv[1], &end, 10);
    if (*end != '\0' || kind < 0 || kind > CC_EVENT_PROPHECY_DELIVERED) return 2;
    long confidence = strtol(argv[2], &end, 10);
    if (*end != '\0' || confidence < 0 || confidence > 100) return 2;
    long variant = strtol(argv[3], &end, 10);
    if (*end != '\0' || variant < 0 || variant > 1) return 2;
    CcCoreAccount account;
    if (!CcCoreAccountPrepare((CcEventKind)kind, argv[4], (int32_t)confidence, 0, &account)) return 1;
    if (argc == 6) {
        long field = strtol(argv[5], &end, 10);
        if (*end != '\0' || field < 0 || (size_t)field >= account.field_count) return 2;
        account.fields[field].knowledge = CC_CORE_UNKNOWN;
    }
    char text[512];
    if (!CcCoreAccountRender(&account, (uint32_t)variant, text, sizeof(text))) return 1;
    (void)puts(text);
    return 0;
}
