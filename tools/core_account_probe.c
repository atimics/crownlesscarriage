/* Small process boundary for checking the game's grammar from Python. */
#include "story/cc_core_account.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void JsonSpan(const char *text, size_t length)
{
    (void)putchar('"');
    for (size_t i = 0U; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32U) (void)printf("\\u%04x", (unsigned int)c);
        else {
            if (c == '"' || c == '\\') (void)putchar('\\');
            (void)putchar((int)c);
        }
    }
    (void)putchar('"');
}

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
    bool packet = argc == 6 && strcmp(argv[5], "--packet") == 0;
    if (argc == 6 && !packet) {
        long field = strtol(argv[5], &end, 10);
        if (*end != '\0' || field < 0 || (size_t)field >= account.field_count) return 2;
        account.fields[field].knowledge = CC_CORE_UNKNOWN;
    }
    char text[512];
    if (!CcCoreAccountRender(&account, (uint32_t)variant, text, sizeof(text))) return 1;
    if (!packet) (void)puts(text);
    else {
        (void)printf("{\"kind\":%ld,\"confidence\":%ld,\"rule\":", kind, confidence);
        JsonSpan(CcCoreAccountRule(&account), strlen(CcCoreAccountRule(&account)));
        (void)printf(",\"grammar_sha256\":");
        JsonSpan(CcCoreAccountGrammar(), strlen(CcCoreAccountGrammar()));
        (void)printf(",\"text\":");
        JsonSpan(account.text, strlen(account.text));
        (void)printf(",\"claim\":");
        JsonSpan(text, strlen(text));
        (void)printf(",\"fields\":[");
        for (size_t i = 0U; i < account.field_count; ++i) {
            const CcCoreField *f = &account.fields[i];
            (void)printf("%s{\"field\":%zu,\"start\":%zu,\"end\":%zu,\"role\":%d,\"knowledge\":%d,\"spoken\":%s,\"provenance\":3,\"event\":1,\"text\":",
                i == 0U ? "" : ",", i, f->start, f->start + f->length, (int)f->role, (int)f->knowledge, f->spoken ? "true" : "false");
            JsonSpan(account.text + f->start, f->length);
            (void)putchar('}');
        }
        (void)puts("]}");
    }
    return 0;
}
