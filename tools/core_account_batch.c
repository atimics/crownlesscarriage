/* Batch grammar parser: read "kind<TAB>confidence<TAB>account" lines on stdin
   and emit one packet JSON per line on stdout. Speeds up corpus building that
   would otherwise spawn one probe process per account. */
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

int main(void)
{
    char line[4096];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *kind_text = line;
        char *tab = strchr(kind_text, '\t');
        if (tab == NULL) return 2;
        *tab = '\0';
        char *confidence_text = tab + 1;
        tab = strchr(confidence_text, '\t');
        if (tab == NULL) return 2;
        *tab = '\0';
        char *account = tab + 1;
        account[strcspn(account, "\r\n")] = '\0';
        char *end = NULL;
        long kind = strtol(kind_text, &end, 10);
        if (end == kind_text || *end != '\0' || kind < 0 || kind > CC_EVENT_PROPHECY_DELIVERED) return 2;
        long confidence = strtol(confidence_text, &end, 10);
        if (end == confidence_text || *end != '\0' || confidence < 0 || confidence > 100) return 2;
        CcCoreAccount parsed;
        bool okay = CcCoreAccountPrepare((CcEventKind)kind, account, (int32_t)confidence, 0, &parsed);
        if (!okay) { (void)puts("{}"); continue; }
        char claim[512];
        if (!CcCoreAccountRender(&parsed, 0U, claim, sizeof(claim))) claim[0] = '\0';
        (void)printf("{\"kind\":%ld,\"confidence\":%ld,\"rule\":", kind, confidence);
        JsonSpan(CcCoreAccountRule(&parsed), strlen(CcCoreAccountRule(&parsed)));
        (void)printf(",\"text\":");
        JsonSpan(parsed.text, strlen(parsed.text));
        (void)printf(",\"claim\":");
        JsonSpan(claim, strlen(claim));
        (void)printf(",\"fields\":[");
        for (size_t i = 0U; i < parsed.field_count; ++i) {
            const CcCoreField *f = &parsed.fields[i];
            (void)printf("%s{\"field\":%zu,\"start\":%zu,\"end\":%zu,\"role\":%d,\"knowledge\":%d,\"spoken\":%s,\"provenance\":3,\"event\":1,\"text\":",
                i == 0U ? "" : ",", i, f->start, f->start + f->length, (int)f->role, (int)f->knowledge, f->spoken ? "true" : "false");
            JsonSpan(parsed.text + f->start, f->length);
            (void)putchar('}');
        }
        (void)puts("]}");
    }
    return ferror(stdin) ? 1 : 0;
}