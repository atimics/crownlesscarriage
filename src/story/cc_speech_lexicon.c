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
                         char *core, size_t capacity)
{
    char actor[CC_EVENT_TEXT_CAPACITY];
    char place[CC_EVENT_TEXT_CAPACITY];
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

static const char *Opener(SpeechRegister voice, const CcGossipVersion *version,
                          uint64_t choice)
{
    static const char *const openings[SPEECH_REGISTER_COUNT][3] = {
        {"Word is going around: ", "This is the story I heard: ",
         "People are passing this along: "},
        {"The report that reached me said: ", "This is what I was told: ",
         "The account I heard went like this: "},
        {"The account passed to me says: ", "This is the telling I have: ",
         "Let me separate the report from the reckoning: "}
    };
    if (version->confidence < 40) return "I am not sure of this telling: ";
    if (version->retellings >= 4) {
        return voice == SPEECH_RECORD ? "This account has passed through other hands: " :
                                       "It has travelled through other mouths: ";
    }
    return openings[voice][choice % 3U];
}

bool CcSpeechRealizeGossip(const CcSim *sim, const CcCharacter *speaker,
                           const CcGossip *story, const CcGossipVersion *version,
                           char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (sim == NULL || speaker == NULL || story == NULL || version == NULL) return false;

    char account[CC_EVENT_TEXT_CAPACITY];
    /* Keep the held retelling's mutations. Realization does not mutate the
       knowledge state, and the old sim-level archival rendering stays intact. */
    CcGossipVersion unstanced = *version;
    unstanced.court_bias = 0;
    unstanced.alarm = 0;
    CcGossipText(sim, story, &unstanced, account, sizeof(account));
    char core[CC_SPEECH_TEXT_CAPACITY];
    uint64_t choice = Choice(story->event_id, speaker->id, UINT64_C(0x63632d72756d6f32));
    if (!ComposeCore(story->kind, account, choice, core, sizeof(core))) {
        (void)snprintf(core, sizeof(core), "%s", account);
    }
    if (core[0] == '\0' || HasQuantity(core)) {
        /* Do not mechanically erase digits and leave a broken assertion.
           Unsupported numerical accounts remain stored in full for quests,
           archives and later templates, but are not read aloud as telemetry. */
        int written = snprintf(text, capacity,
            "I have an account, but I cannot give you its particulars reliably.");
        if (written < 0 || (size_t)written >= capacity) {
            text[0] = '\0';
            return false;
        }
        return true;
    }
    const char *stance = version->court_bias <= -15 ?
        " I do not trust the court's telling of it." :
        version->court_bias >= 15 ? " I would hear the court's account before laying blame." : "";
    const char *worry = version->alarm >= 30 ? " That is what worries me." : "";
    int written = snprintf(text, capacity, "%s%s%s%s",
        Opener(RegisterFor(speaker->role), version, choice >> 16), core, stance, worry);
    if (written < 0 || (size_t)written >= capacity) {
        text[0] = '\0';
        return false;
    }
    return true;
}
