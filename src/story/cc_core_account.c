#include "story/cc_core_account.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct CoreRule {
    CcEventKind kind;
    const char *id, *pattern, *outputs[2], *challenge;
    size_t field_count;
    CcCoreRole roles[CC_CORE_FIELDS];
    const char *allowed[CC_CORE_FIELDS];
    unsigned int positive;
    int less_left, less_right;
} CoreRule;
static const CoreRule Rules[] = {
#include "cc_core_account_rules.inc"
};

const char *CcCoreAccountGrammar(void) { return CC_CORE_GRAMMAR_SHA256; }

bool CcCoreAccountGrammarCompatible(const char *hash)
{
    static const char *const compatible[] = CC_CORE_COMPATIBLE_GRAMMARS;
    if (hash == NULL) return false;
    for (size_t i = 0U; i < sizeof(compatible) / sizeof(compatible[0]); ++i) {
        if (strcmp(hash, compatible[i]) == 0) return true;
    }
    return false;
}

static bool Quantity(const char *at, size_t length, uint64_t *value)
{
    static const char *const words[] = {"zero", "one", "two", "three", "four", "five", "six",
        "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty",
        "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred"};
    bool digits = length > 0U;
    uint64_t number = 0U;
    for (size_t i = 0U; i < length; ++i) {
        if (!isdigit((unsigned char)at[i])) { digits = false; break; }
        uint64_t digit = (uint64_t)(at[i] - '0');
        if (number > (UINT64_MAX - digit) / 10U) return false;
        number = number * 10U + digit;
    }
    if (digits) { *value = number; return true; }
    for (size_t i = 0U; i < sizeof(words) / sizeof(words[0]); ++i) {
        if (strlen(words[i]) == length && strncmp(at, words[i], length) == 0) {
            *value = i <= 20U ? (uint64_t)i : (uint64_t)(i - 18U) * 10U;
            return true;
        }
    }
    return false;
}

typedef struct MatchBudget { unsigned int attempts; } MatchBudget;

static bool MatchField(const CoreRule *rule, CcCoreAccount *account, unsigned int slot,
                       const char *at, const char *end)
{
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
        uint64_t value = 0U;
        if (!Quantity(at, field->length, &value)) return false;
        if ((rule->positive & (1U << slot)) != 0U &&
            (strspn(at, "0") == field->length ||
             (field->length == 4U && strncmp(at, "zero", 4U) == 0))) return false;
    }
    return true;
}

static bool MatchPattern(const CoreRule *rule, const char *pattern, CcCoreAccount *account,
                         const char *at, MatchBudget *budget)
{
    if (*pattern == '\0') return *at == '\0';
    if (*pattern != '{') {
        const char *literal = pattern;
        while (*pattern != '\0' && *pattern != '{') ++pattern;
        size_t length = (size_t)(pattern - literal);
        return strncmp(at, literal, length) == 0 &&
               MatchPattern(rule, pattern, account, at + length, budget);
    }
    if (pattern[1] < '0' || pattern[1] > '7' || pattern[2] != '}') return false;
    unsigned int slot = (unsigned int)(pattern[1] - '0');
    if (slot >= rule->field_count) return false;
    pattern += 3;
    const char *next = strchr(pattern, '{');
    size_t separator_length = next == NULL ? strlen(pattern) : (size_t)(next - pattern);
    const char *limit = account->text + strlen(account->text);
    if (at > limit || separator_length > (size_t)(limit - at)) return false;
    const char *last = separator_length == 0U && next == NULL ? limit : limit - separator_length;
    bool terminal_separator = separator_length != 0U &&
        (pattern[separator_length - 1U] == '.' || pattern[separator_length - 1U] == '!' ||
         pattern[separator_length - 1U] == '?');
    for (const char *end = at; end <= last; ++end) {
        if (++budget->attempts > 4096U) return false;
        if (separator_length != 0U && strncmp(end, pattern, separator_length) != 0) continue;
        CcCoreField saved = account->fields[slot];
        if (!MatchField(rule, account, slot, at, end)) { account->fields[slot] = saved; continue; }
        const char *rest = next == NULL ? pattern + separator_length : next;
        if (MatchPattern(rule, rest, account, end + separator_length, budget)) return true;
        account->fields[slot] = saved;
        if (terminal_separator) return false;
    }
    return false;
}

static bool Match(const CoreRule *rule, const char *pattern, CcCoreAccount *account,
                  bool numeric)
{
    MatchBudget budget = {0U};
    if (!MatchPattern(rule, pattern, account, account->text, &budget)) return false;
    if (numeric && rule->less_left >= 0) {
        const CcCoreField *left = &account->fields[rule->less_left];
        const CcCoreField *right = &account->fields[rule->less_right];
        uint64_t a = 0U, b = 0U;
        if (!Quantity(account->text + left->start, left->length, &a) ||
            !Quantity(account->text + right->start, right->length, &b) || a >= b) return false;
    }
    return true;
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
        if (Match(&Rules[i], Rules[i].pattern, account, true)) {
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

CcEventKind CcCoreAccountKind(const CcCoreAccount *account)
{
    if (account == NULL || account->rule_index >= sizeof(Rules) / sizeof(Rules[0]))
        return CC_EVENT_KIND_COUNT;
    return Rules[account->rule_index].kind;
}

/* The evidential tails the corpus appends to a rendered claim. A hedged or
   retold telling carries one in place of the closing period, so trimming it
   recovers the template the claim was rendered from. Finite and authored, the
   same discipline the pools run on. */
static bool TrimTail(char *text)
{
    static const char *const tails[] = {
        ", so people say.", ", according to the word going round.",
        ", if the story is right.", ", if the rumour is true."};
    size_t length = strlen(text);
    for (size_t i = 0U; i < sizeof(tails) / sizeof(tails[0]); ++i) {
        size_t tail = strlen(tails[i]);
        if (length >= tail && strcmp(text + length - tail, tails[i]) == 0) {
            text[length - tail] = '.';
            text[length - tail + 1U] = '\0';
            return true;
        }
    }
    return false;
}

/* A rendering is more specific the more literal text it fixes. Without this,
   "X and Y went to war." claims "The courts of X and Y went to war." first,
   and "{0} raided {1}." claims "Thornford was raided by Mara Venn." with the
   actor read as "Thornford was". Among renderings that match, the most
   literal wins; ties keep authoring order. */
static size_t LiteralLength(const char *pattern)
{
    size_t literal = 0U;
    for (size_t i = 0U; pattern[i] != '\0'; ++i) {
        if (pattern[i] == '{' && pattern[i + 1U] != '\0' && pattern[i + 2U] == '}') {
            i += 2U;
            continue;
        }
        ++literal;
    }
    return literal;
}

static bool ParseInto(const char *speech, CcEventKind kind, bool any,
                      CcCoreAccount *account)
{
    if (account == NULL) return false;
    *account = (CcCoreAccount){0};
    if (speech == NULL || strlen(speech) >= sizeof(account->text)) return false;
    (void)snprintf(account->text, sizeof(account->text), "%s", speech);
    account->confidence = 80;
    for (int pass = 0; pass < 2; ++pass) {
        /* The corpus hedges a rendered claim with a known tail; retry without it. */
        if (pass == 1 && !TrimTail(account->text)) break;
        bool found = false;
        size_t best_score = 0U, best_rule = 0U, best_count = 0U;
        CcCoreField best_fields[CC_CORE_FIELDS];
        for (size_t i = 0U; i < sizeof(Rules) / sizeof(Rules[0]); ++i) {
            if (!any && Rules[i].kind != kind) continue;
            const char *const forms[3] = {Rules[i].outputs[0], Rules[i].outputs[1],
                                          Rules[i].challenge};
            for (size_t f = 0U; f < 3U; ++f) {
                size_t score = LiteralLength(forms[f]);
                if (found && score <= best_score) continue;
                account->field_count = Rules[i].field_count;
                if (Match(&Rules[i], forms[f], account, false)) {
                    found = true;
                    best_score = score;
                    best_rule = i;
                    best_count = account->field_count;
                    memcpy(best_fields, account->fields, sizeof(best_fields));
                }
            }
        }
        if (found) {
            account->rule_index = best_rule;
            account->field_count = best_count;
            memcpy(account->fields, best_fields, sizeof(best_fields));
            return true;
        }
    }
    *account = (CcCoreAccount){0};
    return false;
}

bool CcCoreAccountParse(const char *speech, CcCoreAccount *account)
{
    return ParseInto(speech, CC_EVENT_KIND_COUNT, true, account);
}

bool CcCoreAccountParseKind(CcEventKind kind, const char *speech, CcCoreAccount *account)
{
    return ParseInto(speech, kind, false, account);
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
