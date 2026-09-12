#include "story/cc_hrakhor.h"

#include <ctype.h>
#include <string.h>

/* Whole roots retain English tense and clause order in this contact dialect. */
static const struct { const char *english, *goblin; } Words[] = {
    {"voice", "hra"}, {"breath", "hra"}, {"clan", "khor"},
    {"stone", "grak"}, {"fire", "vrik"}, {"food", "zhek"},
    {"small", "skrit"}, {"clever", "skrit"}, {"large", "drok"},
    {"strong", "drok"}, {"home", "nukh"}, {"shelter", "nukh"},
    {"friend", "vesh"}, {"friends", "veshuk"}, {"danger", "krath"},
    {"mushroom", "muk"}, {"mushrooms", "mukuk"},
    {"gather", "rakh"}, {"gathers", "rakhs"}, {"gathered", "rakh'ed"},
    {"take", "rakh"}, {"takes", "rakhs"}, {"took", "rakh'ed"},
    {"build", "grosh"}, {"builds", "groshs"}, {"built", "grosh'ed"},
    {"i", "sha"}, {"me", "sha"}, {"you", "thu"},
    {"court", "drok'khor"}, {"courts", "drok'khoruk"},
    {"dragon", "vrik'drok"}, {"dragons", "vrik'drokuk"},
    {"hoard", "rakh'nukh"}, {"tribute", "khor'rakh"},
    {"raid", "krath'rakh"}, {"raided", "krath'rakh'ed"},
    {"tunnel", "grak'nukh"}, {"tunnels", "grak'nukhuk"},
    {"trade", "vesh'rakh"}, {"traded", "vesh'rakh'ed"},
    {"peace", "vesh'khor"}, {"war", "krath'khor"}
};

static bool WordByte(unsigned char c)
{
    return isalnum(c) != 0 || c == '_' || c == '\'' || c >= 128U;
}

static bool Equal(const char *a, const char *b, size_t length)
{
    for (size_t i = 0U; i < length; ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

static bool Append(char *text, size_t capacity, size_t *used,
                   const char *value, size_t length)
{
    if (length >= capacity - *used) return false;
    memcpy(text + *used, value, length);
    *used += length;
    text[*used] = '\0';
    return true;
}

bool CcHrakhorCorrupt(const CcCoreAccount *account, const char *english,
                      unsigned int strength, char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (english == NULL || strength > 100U ||
        CcCoreAccountRule(account)[0] == '\0' || account->field_count > CC_CORE_FIELDS) return false;
    size_t held = strlen(account->text), length = strlen(english), used = 0U;
    for (size_t f = 0U; f < account->field_count; ++f) {
        const CcCoreField *field = &account->fields[f];
        if (field->start > held || field->length > held - field->start) return false;
    }
    for (size_t at = 0U; at < length;) {
        size_t protected = 0U;
        if (at == 0U || !WordByte((unsigned char)english[at - 1U])) {
            for (size_t f = 0U; f < account->field_count; ++f) {
                const CcCoreField *field = &account->fields[f];
                if (field->role < CC_CORE_ACTOR || field->role > CC_CORE_GROUP ||
                    field->length == 0U || field->length > length - at) continue;
                size_t end = at + field->length;
                if (end < length && WordByte((unsigned char)english[end]) && english[end] != '\'') continue;
                if (Equal(english + at, account->text + field->start, field->length) &&
                    field->length > protected) protected = field->length;
            }
        }
        if (protected > 0U) {
            if (!Append(text, capacity, &used, english + at, protected)) goto fail;
            at += protected;
            continue;
        }
        size_t end = at;
        while (end < length && WordByte((unsigned char)english[end])) ++end;
        if (end == at) ++end;
        const char *replacement = NULL;
        uint32_t hash = UINT32_C(2166136261);
        for (size_t i = at; i < end; ++i)
            hash = (hash ^ (uint32_t)tolower((unsigned char)english[i])) * UINT32_C(16777619);
        if (hash % 100U < strength) {
            for (size_t i = 0U; i < sizeof(Words) / sizeof(Words[0]); ++i) {
                if (strlen(Words[i].english) == end - at && Equal(english + at, Words[i].english, end - at)) {
                    replacement = Words[i].goblin;
                    break;
                }
            }
        }
        size_t start = used;
        if (!Append(text, capacity, &used, replacement != NULL ? replacement : english + at,
                    replacement != NULL ? strlen(replacement) : end - at)) goto fail;
        if (replacement != NULL && isupper((unsigned char)english[at]))
            text[start] = (char)toupper((unsigned char)text[start]);
        at = end;
    }
    return true;
fail:
    text[0] = '\0';
    return false;
}
