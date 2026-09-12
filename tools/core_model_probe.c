#include "story/cc_core_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--encode") == 0) {
        int tokens[4096], n = CcCoreModelEncode(argv[2], tokens, 4096);
        if (n < 0) return 2;
        for (int i = 0; i < n; ++i) (void)printf("%s%d", i == 0 ? "" : " ", tokens[i]);
        (void)puts(""); return 0;
    }
    if (argc < 6 || argc > 10) return 2;
    CcCoreModel *model = CcCoreModelLoad(argv[1]);
    if (model == NULL) return 3;
    char *end = NULL;
    long kind = strtol(argv[2], &end, 10);
    if (*end != '\0' || kind < 0 || kind >= CC_EVENT_KIND_COUNT) { CcCoreModelFree(model); return 2; }
    long confidence = strtol(argv[3], &end, 10);
    if (*end != '\0' || confidence < 0 || confidence > 100) { CcCoreModelFree(model); return 2; }
    long retellings = strtol(argv[4], &end, 10);
    if (*end != '\0' || retellings < 0 || retellings > 100) { CcCoreModelFree(model); return 2; }
    CcCoreAccount account;
    bool okay = CcCoreAccountPrepare((CcEventKind)kind, argv[5], (int32_t)confidence, (int32_t)retellings, &account);
    CcCoreSpoken history[CC_CORE_HISTORY] = {0};
    for (int i = 6; i < argc; ++i) {
        if (strlen(argv[i]) >= sizeof(history[0].text)) { CcCoreModelFree(model); return 2; }
        history[i - 6].speaker = ((argc - 1 - i) % 2) == 0 ? 2U : 1U;
        (void)snprintf(history[i - 6].text, sizeof(history[0].text), "%s", argv[i]);
    }
    char text[CC_CORE_UTTERANCE];
    okay = okay && CcCoreModelGenerate(model, &account, 1U, history, (size_t)(argc - 6), text, sizeof(text));
    if (okay) (void)puts(text);
    CcCoreModelFree(model);
    return okay ? 0 : 1;
}
