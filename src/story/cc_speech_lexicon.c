/* The register lexicon realizes a held account in the speaker's own voice.
 *
 * The sim records accounts as structured events and renders them into one
 * shared sentence. A direct quote from that sentence makes every holder
 * speak with the same voice and the same precision, however far the telling
 * has travelled and whatever the speaker does for a living. The lexicon
 * reads the claim back out of the account the holder carries, keeps the
 * slots that speaker's register preserves, drops the ones it does not hold
   any more, and composes the line from per-register fragments.
 *
 * Rumors carry no numbers. A road telling states what happened, not how
   many: counts belong to the charter desk, where a sponsor tells the
   player exactly what to deliver. Small counts may survive as words
   ("a fortnight") when the register would naturally say them.
 *
 * Nothing here consults world truth beyond the account and its holder: the
   town, the raider and the dragon come from the account's own event, the
   stance from the holder's version. A register that lost a slot never
   states it. */

#include "story/cc_speech.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The road register belongs to travellers, couriers, refugees and laborers:
   hearsay openers and a worry that survives the count falling away. The
   scout register keeps what was seen where. The ledger register keeps what
   the record can stand behind: the place, the actors, and the standing of
   the telling. */
typedef enum CcSpeechRegister {
    CC_SPEECH_REGISTER_ROAD,
    CC_SPEECH_REGISTER_SCOUT,
    CC_SPEECH_REGISTER_LEDGER,
    CC_SPEECH_REGISTER_COUNT
} CcSpeechRegister;

static CcSpeechRegister CcSpeechRegisterForRole(CcCharacterRole role)
{
    switch (role) {
    case CC_CHARACTER_SCOUT: return CC_SPEECH_REGISTER_SCOUT;
    case CC_CHARACTER_OFFICIAL: return CC_SPEECH_REGISTER_LEDGER;
    default: return CC_SPEECH_REGISTER_ROAD;
    }
}

/* A stable choice between authored alternatives, so the same account in the
   same mouth always says the same thing, while different speakers differ. */
static uint32_t SpeechVariant(CcId event_id, CcId speaker_id, uint32_t count)
{
    if (count < 2U) return 0U;
    uint64_t mix = event_id * UINT64_C(0x9E3779B97F4A7C15) ^
                   speaker_id * UINT64_C(0xC2B2AE3D27D4EB4F);
    return (uint32_t)(mix % count);
}

/* The bandit group behind an account, when the account's event carries one. */
static const CcBanditGroup *SpeechBanditGroup(const CcSim *sim, CcId id)
{
    if (sim == NULL || id == 0U) return NULL;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].id == id) return &sim->bandits[i];
    }
    return NULL;
}

/* The sim strikes shortage notices in one authored format:
   "<place> has <weeks> weeks of food; hunger reaches pressure level <level>."
   The slots are read back out of the account the holder carries; if the
   words do not carry the numbers, the account is not one the lexicon can
   compose and the caller wraps the direct account instead. */
static bool ShortageSlots(const char *account, int32_t *weeks, int32_t *level)
{
    const char *has = strstr(account, " has ");
    if (has == NULL) return false;
    char *end = NULL;
    long weeks_value = strtol(has + 5, &end, 10);
    if (end == has + 5 || strncmp(end, " weeks of food", 14) != 0) return false;
    const char *pressure = strstr(end, "pressure level ");
    if (pressure == NULL) return false;
    char *level_end = NULL;
    long level_value = strtol(pressure + 15, &level_end, 10);
    if (level_end == pressure + 15 || *level_end != '.') return false;
    *weeks = (int32_t)weeks_value;
    *level = (int32_t)level_value;
    return true;
}

/* Time told the way a person says it: a season, not a day number. */
static const char *SpeechSeason(int32_t day)
{
    static const char *const seasons[4] = {
        "spring", "summer", "autumn", "winter"
    };
    return seasons[(day / 7) % 52 / 13];
}

static void ComposeShortage(const CcCharacter *speaker, const CcGossip *story,
                            const CcGossipVersion *version, int32_t weeks,
                            int32_t level, const char *place,
                            char *text, size_t capacity)
{
    uint32_t variant = SpeechVariant(story->event_id, speaker->id, 2U);
    char claim[CC_SPEECH_TEXT_CAPACITY];
    switch (CcSpeechRegisterForRole(speaker->role)) {
    case CC_SPEECH_REGISTER_LEDGER: {
        const char *standing = level >= 3 ? "desperate want" :
                                level == 2 ? "pressing hunger" :
                                             "the first pressure";
        if (variant == 0U) {
            (void)snprintf(claim, sizeof(claim),
                "The stores at %s have run low since %s. The ledger marks %s.",
                place, SpeechSeason(story->day), standing);
        } else {
            (void)snprintf(claim, sizeof(claim),
                "By the ledger, %s is under %s, since %s.",
                place, standing, SpeechSeason(story->day));
        }
        break;
    }
    case CC_SPEECH_REGISTER_SCOUT:
        if (weeks <= 0) {
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "There is no food left in %s. The granary is bare." :
                    "%s is starving; I have seen the empty bins myself.",
                place);
        } else if (weeks <= 3) {
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "%s is down to its last weeks of food." :
                    "The granary at %s is nearly empty.",
                place);
        } else {
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "%s is hungrier than it lets on." :
                    "Food is short in %s, and the pressure is rising.",
                place);
        }
        break;
    default:
        if (version->retellings >= 4) {
            /* Deep hearsay: the count has fallen away; the worry survives. */
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "They say %s is running out of food." :
                    "The word along the road is that %s is going hungry.",
                place);
        } else if (weeks <= 0) {
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "%s has run out of food." :
                    "They say %s is starving, and the granary is empty.",
                place);
        } else if (weeks <= 3) {
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "They say %s is down to its last weeks of food." :
                    "%s is going hungry, the way they tell it.",
                place);
        } else {
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "Word is %s is running low on food." :
                    "They say the harvest in %s was not enough.",
                place);
        }
        break;
    }
    const char *bias = version->court_bias >= 15 ?
        " Loyal voices credit the crown." :
        version->court_bias <= -15 ? " Some blame the court." : "";
    const char *alarm = version->alarm >= 30 ?
        " They fear worse is coming." : "";
    (void)snprintf(text, capacity, "%s%s%s", claim, bias, alarm);
}

/* The dramatic kinds state their actors and places from the account's own
   event; the counts stay at the charter desk. */
static void ComposeDramatic(const CcSim *sim, const CcCharacter *speaker,
                            const CcGossip *story, const CcGossipVersion *version,
                            const CcEvent *event,
                            char *text, size_t capacity)
{
    uint32_t variant = SpeechVariant(story->event_id, speaker->id, 2U);
    CcSpeechRegister register_ = CcSpeechRegisterForRole(speaker->role);
    const char *place = CcSimSettlement(sim, event->location_id) != NULL ?
        CcSimSettlement(sim, event->location_id)->name : "the road";
    const char *goblins = sim->goblins.name;
    const char *dragon = sim->dragon.name;
    const CcBanditGroup *bandits = SpeechBanditGroup(sim, event->subject_id);
    const char *company = bandits != NULL ? bandits->name : "Brigands";
    char claim[CC_SPEECH_TEXT_CAPACITY];
    switch (story->kind) {
    case CC_EVENT_GOBLIN_RAIDED:
        switch (register_) {
        case CC_SPEECH_REGISTER_LEDGER:
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "The ledger records a goblin raid on %s.", place);
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "Entered in the ledger: %s was raided by the goblin court.",
                    place);
            }
            break;
        case CC_SPEECH_REGISTER_SCOUT:
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "The goblins raided %s and made off with supplies." :
                    "Goblins struck %s. Watch the roads there.",
                place);
            break;
        default:
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "They say the goblins raided %s and carried off what they could." :
                    "Goblins hit %s, the story goes. Goods and coin gone.",
                place);
            break;
        }
        break;
    case CC_EVENT_SETTLEMENT_RAIDED:
        switch (register_) {
        case CC_SPEECH_REGISTER_LEDGER:
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "The ledger records a raid: %s took from %s." :
                    "Entered: %s raided %s.",
                company, place);
            break;
        case CC_SPEECH_REGISTER_SCOUT:
            (void)snprintf(claim, sizeof(claim),
                variant == 0U ?
                    "%s raided %s and took supplies." :
                    "%s struck %s. The road there is worse than it looks.",
                company, place);
            break;
        default:
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "Brigands hit %s, they say. The town is poorer for it.",
                    place);
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "They tell it on the road: %s took what it wanted from %s.",
                    company, place);
            }
            break;
        }
        break;
    case CC_EVENT_DRAGON_OMEN:
        switch (register_) {
        case CC_SPEECH_REGISTER_LEDGER:
            (void)snprintf(claim, sizeof(claim),
                "An omen is entered: smoke over %s, and the readers count "
                "fourteen nights until %s comes.",
                place, dragon);
            break;
        case CC_SPEECH_REGISTER_SCOUT:
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "Watch %s: the readers count a fortnight to %s.",
                    place, dragon);
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "Smoke over %s. The old readers swear %s is coming.",
                    place, dragon);
            }
            break;
        default:
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "They say smoke fell into %s's chimneys. Old readers swear %s is coming.",
                    place, dragon);
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "The old readers in %s count the nights. They say %s is coming.",
                    place, dragon);
            }
            break;
        }
        break;
    case CC_EVENT_DRAGON_RETALIATION:
        switch (register_) {
        case CC_SPEECH_REGISTER_LEDGER:
            (void)snprintf(claim, sizeof(claim),
                "%s burned %s; the ledger holds it, with the stolen crowns "
                "still unreturned.",
                dragon, place);
            break;
        case CC_SPEECH_REGISTER_SCOUT:
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "%s burned %s. The stolen crowns were never returned.",
                    dragon, place);
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "%s came down on %s. The hoard wants its own.",
                    dragon, place);
            }
            break;
        default:
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "%s burned %s, they say, over stolen crowns.",
                    dragon, place);
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "They tell it on the road: %s burned %s for its hoard.",
                    dragon, place);
            }
            break;
        }
        break;
    case CC_EVENT_GOBLIN_CULT_RALLIED:
        switch (register_) {
        case CC_SPEECH_REGISTER_LEDGER:
            (void)snprintf(claim, sizeof(claim),
                "%s",
                variant == 0U ?
                    "The ledger records a cult rally: the goblin court has grown." :
                    "Entered in the ledger: the goblin court gathers new tithe-bearers.");
            break;
        case CC_SPEECH_REGISTER_SCOUT:
            (void)snprintf(claim, sizeof(claim),
                "%s",
                variant == 0U ?
                    "The goblin cult is rallying. The court grows stronger near the lair." :
                    "New tithe-bearers have joined the goblin court.");
            break;
        default:
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "They say the goblin court is growing again. New converts, if you believe the road.");
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "The goblin cult rallies, they say. %s's court grows by the week.",
                    goblins);
            }
            break;
        }
        break;
    default:
        claim[0] = '\0';
        break;
    }
    const char *bias = version->court_bias >= 15 ?
        " Loyal voices credit the crown." :
        version->court_bias <= -15 ? " Some blame the court." : "";
    const char *alarm = version->alarm >= 30 ?
        " They fear worse is coming." : "";
    (void)snprintf(text, capacity, "%s%s%s", claim, bias, alarm);
}

/* Every other account keeps its claim exactly as the sim and the road have
   told it, but each register opens it in its own voice. */
static const char *SpeechOpener(CcSpeechRegister register_,
                                CcId event_id, CcId speaker_id,
                                int32_t retellings)
{
    bool deep = retellings >= 4;
    uint32_t variant = SpeechVariant(event_id, speaker_id, 2U);
    static const char *const openers[CC_SPEECH_REGISTER_COUNT][2][2] = {
        /* ROAD */ {
            {"I heard it on the road: ", "Word is going around: "},
            {"An old story on the road: ", "They have been telling this for a while: "},
        },
        /* SCOUT */ {
            {"I have it from good ears: ", "Counted and confirmed: "},
            {"Reported from further down the road: ", "I heard this some days out: "},
        },
        /* LEDGER */ {
            {"The ledger records it thus: ", "Entered in the ledger: "},
            {"The ledger holds an older telling: ", "Copied from an earlier page: "},
        },
    };
    return openers[register_][deep ? 1 : 0][variant];
}

/* Realize a held account in the speaker's register. Returns false when the
   account cannot be realized; the caller then keeps the direct quote. */
bool CcSpeechRealizeGossip(const CcSim *sim, const CcCharacter *speaker,
                           const CcGossip *story, const CcGossipVersion *version,
                           char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (sim == NULL || speaker == NULL || story == NULL || version == NULL) {
        return false;
    }
    if (story->kind == CC_EVENT_SHORTAGE) {
        int32_t weeks = 0, level = 0;
        if (!ShortageSlots(story->text, &weeks, &level)) return false;
        const CcSettlement *origin = CcSimSettlement(sim, story->origin_id);
        ComposeShortage(speaker, story, version, weeks, level,
                        origin != NULL ? origin->name : "the road",
                        text, capacity);
        return true;
    }
    switch (story->kind) {
    case CC_EVENT_GOBLIN_RAIDED:
    case CC_EVENT_SETTLEMENT_RAIDED:
    case CC_EVENT_DRAGON_OMEN:
    case CC_EVENT_DRAGON_RETALIATION:
    case CC_EVENT_GOBLIN_CULT_RALLIED: {
        const CcEvent *event = CcSimEvent(sim, story->event_id);
        if (event == NULL) return false;
        ComposeDramatic(sim, speaker, story, version, event, text, capacity);
        return true;
    }
    default:
        break;
    }
    char account[CC_EVENT_TEXT_CAPACITY];
    CcGossipText(sim, story, version, account, sizeof(account));
    (void)snprintf(text, capacity, "%s%s",
                   SpeechOpener(CcSpeechRegisterForRole(speaker->role),
                                story->event_id, speaker->id,
                                version->retellings),
                   account);
    return true;
}
