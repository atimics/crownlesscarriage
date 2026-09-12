#include "story/cc_core_account.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct CoreRule {
    CcEventKind kind;
    const char *id, *pattern, *outputs[2];
    size_t field_count;
    CcCoreRole roles[CC_CORE_FIELDS];
    const char *allowed[CC_CORE_FIELDS];
    unsigned int positive;
} CoreRule;
static const CoreRule Rules[] = {
#include "cc_core_account_rules.inc"
};

const char *CcCoreAccountGrammar(void) { return CC_CORE_GRAMMAR_SHA256; }

static bool Quantity(const char *at, size_t length)
{
    static const char *const words[] = {"zero", "one", "two", "three", "four", "five", "six",
        "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty",
        "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred"};
    bool digits = length > 0U;
    for (size_t i = 0U; i < length; ++i) digits = digits && isdigit((unsigned char)at[i]);
    if (digits) return true;
    for (size_t i = 0U; i < sizeof(words) / sizeof(words[0]); ++i) {
        if (strlen(words[i]) == length && strncmp(at, words[i], length) == 0) return true;
    }
    return false;
}

static bool Match(const CoreRule *rule, CcCoreAccount *account)
{
    const char *pattern = rule->pattern, *at = account->text;
    while (*pattern != '\0') {
        if (pattern[0] != '{') {
            if (*pattern++ != *at++) return false;
            continue;
        }
        unsigned int slot = (unsigned int)(pattern[1] - '0');
        if (slot >= rule->field_count || pattern[2] != '}') return false;
        pattern += 3;
        const char *next = strchr(pattern, '{');
        size_t length = next == NULL ? strlen(pattern) : (size_t)(next - pattern);
        char separator[CC_EVENT_TEXT_CAPACITY];
        if (length >= sizeof(separator)) return false;
        memcpy(separator, pattern, length);
        separator[length] = '\0';
        const char *end = length == 0U ? at + strlen(at) : strstr(at, separator);
        if (end == NULL) return false;
        CcCoreField *field = &account->fields[slot];
        field->start = (size_t)(at - account->text);
        field->length = (size_t)(end - at);
        field->role = rule->roles[slot];
        char marker[] = {'{', (char)('0' + slot), '}', '\0'};
        field->spoken = strstr(rule->outputs[0], marker) != NULL || strstr(rule->outputs[1], marker) != NULL;
        field->knowledge = account->confidence < 40 ? CC_CORE_UNCERTAIN : CC_CORE_KNOWN;
        if (rule->allowed[slot] != NULL) {
            char option[CC_EVENT_TEXT_CAPACITY + 3];
            option[0] = '|';
            memcpy(option + 1, at, field->length);
            option[field->length + 1U] = '|';
            option[field->length + 2U] = '\0';
            if (strstr(rule->allowed[slot], option) == NULL) return false;
        }
        if (field->length == 0U && field->role != CC_CORE_DETAIL) return false;
        if (field->role == CC_CORE_QUANTITY) {
            if (!Quantity(at, field->length)) return false;
            if ((rule->positive & (1U << slot)) != 0U &&
                (strspn(at, "0") == field->length ||
                 (field->length == 4U && strncmp(at, "zero", 4U) == 0))) return false;
        }
        at = end;
    }
    return *at == '\0';
}

bool CcCoreAccountPrepare(CcEventKind kind, const char *held_text,
                          int32_t confidence, int32_t retellings,
                          CcCoreAccount *account)
{
    if (account == NULL) return false;
    *account = (CcCoreAccount){0};
    if (held_text == NULL || strlen(held_text) >= sizeof(account->text)) return false;
    (void)snprintf(account->text, sizeof(account->text), "%s", held_text);
    account->confidence = confidence;
    account->retellings = retellings;
    for (size_t i = 0U; i < sizeof(Rules) / sizeof(Rules[0]); ++i) {
        if (Rules[i].kind != kind) continue;
        account->field_count = Rules[i].field_count;
        if (Match(&Rules[i], account)) {
            account->rule_index = i;
            return true;
        }
    }
    *account = (CcCoreAccount){0};
    return false;
}

const char *CcCoreAccountRule(const CcCoreAccount *account)
{
    if (account == NULL || account->text[0] == '\0' ||
        account->rule_index >= sizeof(Rules) / sizeof(Rules[0])) return "";
    return Rules[account->rule_index].id;
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

bool CcCoreAccountRender(const CcCoreAccount *account, uint32_t variant,
                         char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (CcCoreAccountRule(account)[0] == '\0' || variant > 1U) return false;
    const char *pattern = Rules[account->rule_index].outputs[variant];
    size_t used = 0U;
    while (*pattern != '\0') {
        if (*pattern != '{') {
            if (!Append(text, capacity, &used, pattern++, 1U)) goto fail;
        } else {
            size_t slot = (size_t)(pattern[1] - '0');
            if (slot >= account->field_count) goto fail;
            const CcCoreField *field = &account->fields[slot];
            if (field->role == CC_CORE_QUANTITY || field->start + field->length > strlen(account->text)) goto fail;
            const char *value = account->text + field->start;
            size_t length = field->length;
            if (field->knowledge == CC_CORE_UNKNOWN || field->knowledge == CC_CORE_COARSE) {
                value = field->role == CC_CORE_ACTOR || field->role == CC_CORE_RECIPIENT ? "someone" :
                        field->role == CC_CORE_PLACE ? "somewhere" :
                        field->role == CC_CORE_GROUP ? "a court" : "something";
                length = strlen(value);
            }
            if (!Append(text, capacity, &used, value, length)) goto fail;
            pattern += 3;
        }
    }
    if (used > 0U) text[0] = (char)toupper((unsigned char)text[0]);
    return true;
fail:
    text[0] = '\0';
    return false;
}
