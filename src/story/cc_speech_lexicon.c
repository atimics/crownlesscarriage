/* Bounded English realization of held accounts, not a view of world truth.
 * Parse the persisted telling (after its existing retelling mutations), never
 * the event ring or today's actors. Roles affect wording, not evidence.
 * Exact quantities stay in quest instructions and trade, outside this layer.
 */
#include "story/cc_speech.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum SpeechRegister {
    SPEECH_ROAD,
    SPEECH_SCOUT,
    SPEECH_RECORD,
    SPEECH_REGISTER_COUNT
} SpeechRegister;

static SpeechRegister RegisterFor(CcCharacterRole role)
{
    if (role == CC_CHARACTER_SCOUT) return SPEECH_SCOUT;
    if (role == CC_CHARACTER_OFFICIAL) return SPEECH_RECORD;
    return SPEECH_ROAD;
}

/* Mix high bits too: multiplying IDs and taking modulo two only alternates
   parity. Salt independent choices so opener and clause need not move together.
   Version this salt when changing the realization tables. */
static uint64_t Choice(CcId account, CcId speaker, uint64_t salt)
{
    uint64_t hash = account ^ (speaker * UINT64_C(0x9e3779b97f4a7c15)) ^ salt;
    hash = (hash ^ (hash >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    hash = (hash ^ (hash >> 27)) * UINT64_C(0x94d049bb133111eb);
    return hash ^ (hash >> 31);
}

static bool WordIs(const char *start, size_t length, const char *word)
{
    if (strlen(word) != length) return false;
    for (size_t i = 0U; i < length; ++i) {
        if (tolower((unsigned char)start[i]) != word[i]) return false;
    }
    return true;
}

/* Unknown numerical clauses fail closed instead of falling back to telemetry.
   Spelling out a count (or saying 'a fortnight') is not removing the count. */
static bool HasQuantity(const char *text)
{
    static const char *const numbers[] = {
        "zero", "one", "two", "three", "four", "five", "six", "seven",
        "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty",
        "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety",
        "hundred", "thousand", "million", "billion", "dozen", "fortnight"
    };
    for (const char *at = text; *at != '\0';) {
        if (isdigit((unsigned char)*at)) return true;
        if (!isalpha((unsigned char)*at)) { ++at; continue; }
        const char *start = at;
        while (isalpha((unsigned char)*at)) ++at;
        for (size_t i = 0U; i < sizeof(numbers) / sizeof(numbers[0]); ++i) {
            if (WordIs(start, (size_t)(at - start), numbers[i])) return true;
        }
    }
    return false;
}

static bool CopySpan(char *out, size_t capacity,
                      const char *first, const char *last)
{
    if (first == NULL || last == NULL || last <= first) return false;
    size_t length = (size_t)(last - first);
    if (length >= capacity) return false;
    memcpy(out, first, length);
    out[length] = '\0';
    return true;
}

static bool Before(const char *text, const char *separator,
                    char *out, size_t capacity)
{
    return CopySpan(out, capacity, text, strstr(text, separator));
}

static bool Between(const char *text, const char *left, const char *right,
                     char *out, size_t capacity)
{
    const char *first = strstr(text, left);
    if (first == NULL) return false;
    first += strlen(left);
    return CopySpan(out, capacity, first, strstr(first, right));
}

/* Every core is a report of a past claim. A stock snapshot does not prove a
   continuing famine; a scout's occupation does not make hearsay eyewitness
   testimony; an official's voice does not prove a ledger exists. */
static bool ComposeCore(CcEventKind kind, const char *account, uint64_t choice,
                         CcGossipDetail detail_mode, char *core, size_t capacity)
{
    char actor[CC_EVENT_TEXT_CAPACITY];
    char place[CC_EVENT_TEXT_CAPACITY];
    char detail[CC_EVENT_TEXT_CAPACITY];
    bool alternate = (choice & 1U) != 0U;
    switch (kind) {
    case CC_EVENT_SHORTAGE:
        if (Before(account, " has ", place, sizeof(place)) &&
            (strstr(account, " weeks of food; hunger reaches pressure level ") != NULL ||
             strstr(account, " food in store. Its reserve target is ") != NULL)) {
            (void)snprintf(core, capacity, alternate ?
                "Food was running short in %s." :
                "%s was short of food.", place);
            return true;
        }
        break;
    case CC_EVENT_GOBLIN_RAIDED:
    case CC_EVENT_SETTLEMENT_RAIDED:
        if (Before(account, " raids ", actor, sizeof(actor)) &&
            (Between(account, " raids ", " and takes ", place, sizeof(place)) ||
             Between(account, " raids ", ":", place, sizeof(place)) ||
             Between(account, " raids ", ".", place, sizeof(place)))) {
            (void)snprintf(core, capacity, alternate ?
                "%s had been raided by %s." : "%s raided %s.",
                alternate ? place : actor, alternate ? actor : place);
            return true;
        }
        break;
    case CC_EVENT_DRAGON_OMEN:
        if (Between(account, "Smoke falls into ", "'s chimneys;", place, sizeof(place)) &&
            Between(account, " until ", " comes.", actor, sizeof(actor))) {
            (void)snprintf(core, capacity, alternate ?
                "Smoke fell into %s's chimneys. The old readers feared %s was coming." :
                "The old readers in %s took the smoke as a warning of %s.",
                place, actor);
            return true;
        }
        break;
    case CC_EVENT_DRAGON_RETALIATION:
        if (Before(account, " burns ", actor, sizeof(actor)) &&
            Between(account, " burns ", " because ", place, sizeof(place)) &&
            strstr(account, " stolen crowns remain missing.") != NULL) {
            (void)snprintf(core, capacity, alternate ?
                "%s burned %s. The telling blamed stolen hoard money." :
                "%s burned %s over crowns missing from the hoard.", actor, place);
            return true;
        }
        break;
    case CC_EVENT_GOBLIN_CULT_RALLIED:
        if (Before(account, " feeds and binds ", actor, sizeof(actor)) &&
            strstr(account, "new ash-sworn; the dead dragon's court") != NULL) {
            (void)snprintf(core, capacity, alternate ?
                "%s was binding new ash-sworn to the dead dragon's court." :
                "New ash-sworn were joining %s in the dead dragon's service.", actor);
            return true;
        }
        if (Before(account, " gathers ", actor, sizeof(actor)) &&
            strstr(account, "new tithe-bearers") != NULL) {
            (void)snprintf(core, capacity, alternate ?
                "%s was gathering new tithe-bearers for the goblin court." :
                "New tithe-bearers were joining %s. The goblin court was gathering again.", actor);
            return true;
        }
        break;
    case CC_EVENT_DRAGON_BROOD:
        if (Before(account, " accepts a distant heart-scale and lays ", actor, sizeof(actor)) &&
            strstr(account, "goblin Ashkeepers seal the brood hoard.") != NULL) {
            (void)snprintf(core, capacity, alternate ?
                "%s had laid a clutch. Goblin Ashkeepers sealed the brood hoard." :
                "There was a new clutch from %s, with goblin Ashkeepers sealing the hoard.", actor);
            return true;
        }
        break;
    case CC_EVENT_HARVEST_FAILED:
        if (Before(account, "'s drought harvest cannot supply the ", actor, sizeof(actor)) &&
            Between(account, "'s drought harvest cannot supply the ", ".", place, sizeof(place))) {
            (void)snprintf(core, capacity, alternate ?
                "%s's drought harvest fell short of what the %s needed." :
                "The drought harvest in %s fell short of supplies for the %s.", actor, place);
            return true;
        }
        break;
    case CC_EVENT_ROUTE_CLOSED:
        if (Before(account, " closes the treaty bridge and delays the relief convoy.",
                   actor, sizeof(actor))) {
            (void)snprintf(core, capacity, alternate ?
                "%s closed the treaty bridge, delaying the relief convoy." :
                "The relief convoy was delayed when %s closed the treaty bridge.", actor);
            return true;
        }
        break;
    case CC_EVENT_BANDIT_PRESSURE:
        if (Before(account, " reinforce ", actor, sizeof(actor)) &&
            Between(account, " reinforce ", " on the old road.", place, sizeof(place))) {
            (void)snprintf(core, capacity, alternate ?
                "%s reinforced %s on the old road." :
                "%s gave %s support on the old road.", actor, place);
            return true;
        }
        if (Before(account, " recruits from hungry debtors and unpaid households;", actor, sizeof(actor))) {
            (void)snprintf(core, capacity, alternate ?
                "%s recruited hungry debtors and people from unpaid households." :
                "Hungry debtors and people from unpaid households were being recruited by %s.", actor);
            return true;
        }
        break;
    case CC_EVENT_NOTICE_POSTED:
        if (Before(account, " posts a notice at ", actor, sizeof(actor)) &&
            Between(account, " posts a notice at ", ": ", place, sizeof(place)) &&
            Between(account, ": ", ".", detail, sizeof(detail))) {
            const char *notice = strcmp(detail, "Relief charter") == 0 ? "a relief charter" :
                strcmp(detail, "Road compact") == 0 ? "a road compact" :
                strcmp(detail, "Depth warrant") == 0 ? "a depth warrant" :
                strcmp(detail, "Quiet commission") == 0 ? "a quiet commission" :
                strcmp(detail, "Sealed dispatch") == 0 ? "a sealed dispatch" : NULL;
            if (detail_mode == CC_GOSSIP_DETAIL_SUBJECT) {
                (void)snprintf(core, capacity, "%s posted something in %s.", actor, place);
            } else if (notice != NULL) {
                (void)snprintf(core, capacity, "%s put up %s in %s.",
                    detail_mode == CC_GOSSIP_DETAIL_ACTOR ? "Someone" : actor, notice, place);
            } else {
                (void)snprintf(core, capacity, "%s posted a notice about %s in %s.",
                    detail_mode == CC_GOSSIP_DETAIL_ACTOR ? "Someone" : actor, detail, place);
            }
            return true;
        }
        break;
    case CC_EVENT_CHARACTER_DIED:
        if (Before(account, " died at age ", actor, sizeof(actor)) &&
            Between(account, " after a life in ", ".", place, sizeof(place))) {
            (void)snprintf(core, capacity, alternate ?
                "%s died after a life in %s." : "%s had lived in %s and died.",
                actor, place);
            return true;
        }
        break;
    case CC_EVENT_TREASURE_CRAFTED:
        if (Before(account, " finishes ", place, sizeof(place)) &&
            Between(account, " finishes ", " from ", actor, sizeof(actor)) &&
            strstr(account, "Raw Gold") != NULL && strstr(account, "weeks of work.") != NULL) {
            (void)snprintf(core, capacity, alternate ?
                "%s finished making %s." : "%s completed %s.", place, actor);
            return true;
        }
        break;
    case CC_EVENT_WAR_DECLARED:
    case CC_EVENT_PEACE_DECLARED:
        if (Before(account, "'s courier reaches ", actor, sizeof(actor)) &&
            Between(account, "'s courier reaches ", ":", place, sizeof(place)) &&
            strstr(account, " now binds the ") != NULL && strstr(account, " courts.") != NULL) {
            /* Read the held wording: a retelling can change its diplomatic claim. */
            const char *state = strstr(account, ": war now binds") != NULL ? "war" :
                strstr(account, ": peace now binds") != NULL ? "peace" : NULL;
            if (state != NULL) {
                if (detail_mode == CC_GOSSIP_DETAIL_SUBJECT) {
                    (void)snprintf(core, capacity, "Something changed between %s and %s.", actor, place);
                } else if (detail_mode == CC_GOSSIP_DETAIL_ACTOR) {
                    (void)snprintf(core, capacity, "Some courts %s.",
                        strcmp(state, "peace") == 0 ? "made peace" : "went to war");
                } else {
                    (void)snprintf(core, capacity, "%s and %s %s.", actor, place,
                        strcmp(state, "peace") == 0 ? "made peace" : "went to war");
                }
                return true;
            }
        }
        break;
    case CC_EVENT_DRAGON_SLAIN:
        if (Before(account, ":", actor, sizeof(actor)) &&
            strstr(actor, " slays ") != NULL) {
            (void)snprintf(core, capacity, "%s.", actor);
            return true;
        }
        break;
    default: break;
    }
    /* Existing free-text raid accounts preserve their mutated actors and
       directions, without exposing either written or digit quantities. */
    if (Before(account, " took ", actor, sizeof(actor)) &&
        Between(account, " from ", ".", place, sizeof(place))) {
        (void)snprintf(core, capacity, "%s took supplies from %s.", actor, place);
        return true;
    }
    return false;
}

static const char *RoleTail(SpeechRegister voice)
{
    switch (voice) {
    case SPEECH_RECORD: return " That's the report I have.";
    case SPEECH_SCOUT: return " That's what reached me.";
    default: return "";
    }
}

bool CcSpeechPrepareGossip(const CcSim *sim, const CcGossip *story,
                            const CcGossipVersion *version, uint32_t variant,
                            CcGossipLanguage *language)
{
    if (language == NULL) return false;
    *language = (CcGossipLanguage){0};
    if (sim == NULL || story == NULL || version == NULL || variant > 1U) return false;
    language->kind = story->kind;
    language->variant = variant;
    language->confidence = version->confidence;
    language->retellings = version->retellings;
    /* Scalar confidence supports a conservative choice to omit a detail.
       The packet records that choice for training; the held account stays intact. */
    if (version->confidence < 40 &&
        (story->kind == CC_EVENT_NOTICE_POSTED || story->kind == CC_EVENT_WAR_DECLARED ||
         story->kind == CC_EVENT_PEACE_DECLARED)) {
        language->detail = variant == 0U ? CC_GOSSIP_DETAIL_ACTOR : CC_GOSSIP_DETAIL_SUBJECT;
    }
    CcGossipVersion unstanced = *version;
    unstanced.court_bias = 0;
    unstanced.alarm = 0;
    CcGossipText(sim, story, &unstanced, language->account, sizeof(language->account));
    if (!ComposeCore(story->kind, language->account, variant, language->detail,
                     language->claim, sizeof(language->claim)) ||
        language->claim[0] == '\0' || HasQuantity(language->claim)) {
        language->claim[0] = '\0';
        return false;
    }
    return true;
}

bool CcSpeechCoreGossip(const CcGossipLanguage *language,
                         char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (language == NULL || language->claim[0] == '\0' || HasQuantity(language->claim)) return false;
    char clause[CC_SPEECH_TEXT_CAPACITY];
    (void)snprintf(clause, sizeof(clause), "%s", language->claim);
    size_t length = strlen(clause);
    if (length > 0U && clause[length - 1U] == '.') clause[length - 1U] = '\0';
    const char *opening = "";
    const char *ending = ".";
    if (language->confidence < 40) {
        if (language->detail == CC_GOSSIP_DETAIL_FULL) {
            ending = language->variant == 0U ? ", if the story is right." :
                                               ", if there's truth in the rumour.";
        }
    } else if (language->retellings >= 4) {
        if (language->variant == 0U) {
            opening = "Have you heard? ";
            ending = ". That's the word going round.";
        } else ending = ", so people say.";
    } else if (language->variant == 0U) ending = ", I hear.";
    else opening = "Have you heard the news? ";
    int written = snprintf(text, capacity, "%s%s%s", opening, clause, ending);
    if (written < 0 || (size_t)written >= capacity) {
        text[0] = '\0';
        return false;
    }
    return true;
}

bool CcSpeechRealizeGossip(const CcSim *sim, const CcCharacter *speaker,
                           const CcGossip *story, const CcGossipVersion *version,
                           char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (sim == NULL || speaker == NULL || story == NULL || version == NULL) return false;

    char core[CC_SPEECH_TEXT_CAPACITY];
    uint64_t choice = Choice(story->event_id, speaker->id, UINT64_C(0x63632d72756d6f32));
    CcGossipLanguage language;
    bool supported = CcSpeechPrepareGossip(sim, story, version,
                                          (uint32_t)(choice & 1U), &language);
    (void)snprintf(core, sizeof(core), "%s", supported ? language.claim : language.account);
    if (core[0] == '\0' || HasQuantity(core)) {
        /* Do not mechanically erase digits and leave a broken assertion.
           Unsupported numerical accounts remain stored in full for quests,
           archives and later templates, but are not read aloud as telemetry. */
        int written = snprintf(text, capacity,
            "Something happened, but I've only heard bits of it.");
        if (written < 0 || (size_t)written >= capacity) {
            text[0] = '\0';
            return false;
        }
        return true;
    }
    if (!supported) {
        (void)snprintf(language.claim, sizeof(language.claim), "%s", core);
        language.detail = CC_GOSSIP_DETAIL_FULL;
    }
    if (!CcSpeechCoreGossip(&language, core, sizeof(core))) return false;
    const char *stance = version->court_bias <= -15 ?
        " I do not trust the court's telling of it." :
        version->court_bias >= 15 ? " I would hear the court's account before laying blame." : "";
    const char *worry = version->alarm >= 30 ? " That is what worries me." : "";
    int written = snprintf(text, capacity, "%s%s%s%s",
        core, RoleTail(RegisterFor(speaker->role)), stance, worry);
    if (written < 0 || (size_t)written >= capacity) {
        text[0] = '\0';
        return false;
    }
    return true;
}
