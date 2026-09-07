/* The register lexicon realizes a held account in the speaker's own voice.
 *
 * The sim records accounts as structured events and renders them into one
 * shared sentence. A direct quote from that sentence makes every holder
 * speak with the same voice and the same precision, however far the telling
   has travelled and whatever the speaker does for a living. The lexicon
 * reads the claim back out of the account the holder carries, keeps the
 * slots that speaker's register preserves, drops the ones it does not hold
   any more, and composes the line from per-register fragments.
 *
 * Nothing here consults world truth beyond the account and its holder: the
 * town comes from the story's origin, the quantities from the account's own
 * words, the stance from the holder's version. A register that lost the
   count never states one. */

#include "story/cc_speech.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The road register belongs to travellers, couriers, refugees and laborers:
   hearsay openers, road-relative space, and a count that falls away as the
   telling travels. The scout register keeps the granary count and the
   pressure it measured. The ledger register keeps the count, the pressure
   and the day the account was struck. */
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

/* The sim strikes shortage notices in one authored format:
   "<place> has <weeks> weeks of food; hunger reaches pressure level <level>."
   The slots are read back out of the account the holder carries; if the
   words do not carry the numbers, the account is not one the lexicon can
   realize and the caller keeps the direct quote. */
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

/* Realize a held account in the speaker's register. Returns false when the
   account is not one the lexicon knows, and the caller keeps the direct
   quote path. */
bool CcSpeechRealizeGossip(const CcSim *sim, const CcCharacter *speaker,
                           const CcGossip *story, const CcGossipVersion *version,
                           char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (sim == NULL || speaker == NULL || story == NULL || version == NULL) {
        return false;
    }
    if (story->kind != CC_EVENT_SHORTAGE) return false;
    int32_t weeks = 0, level = 0;
    if (!ShortageSlots(story->text, &weeks, &level)) return false;
    const CcSettlement *origin = CcSimSettlement(sim, story->origin_id);
    const char *place = origin != NULL ? origin->name : "the road";
    uint32_t variant = SpeechVariant(story->event_id, speaker->id, 2U);
    char claim[CC_SPEECH_TEXT_CAPACITY];
    switch (CcSpeechRegisterForRole(speaker->role)) {
    case CC_SPEECH_REGISTER_LEDGER:
        if (variant == 0U) {
            (void)snprintf(claim, sizeof(claim),
                "The stores at %s stand at %d weeks of food. "
                "The ledger calls it pressure level %d, from day %d.",
                place, weeks, level, story->day);
        } else {
            (void)snprintf(claim, sizeof(claim),
                "By the ledger, %s holds %d weeks of food at pressure "
                "level %d, as of day %d.",
                place, weeks, level, story->day);
        }
        break;
    case CC_SPEECH_REGISTER_SCOUT:
        if (variant == 0U) {
            (void)snprintf(claim, sizeof(claim),
                "The granary at %s holds %d weeks of food. "
                "Hunger there has reached level %d.",
                place, weeks, level);
        } else {
            (void)snprintf(claim, sizeof(claim),
                "Counting the granary at %s: %d weeks left, "
                "with hunger at level %d.",
                place, weeks, level);
        }
        break;
    default:
        if (version->retellings >= 4) {
            /* Deep hearsay: the count has fallen away; the worry survives. */
            if (variant == 0U) {
                (void)snprintf(claim, sizeof(claim),
                    "They say %s is running out of food.", place);
            } else {
                (void)snprintf(claim, sizeof(claim),
                    "The word along the road is that %s is going hungry.",
                    place);
            }
        } else if (variant == 0U) {
            (void)snprintf(claim, sizeof(claim),
                "%s is down to %d weeks of food.", place, weeks);
        } else {
            (void)snprintf(claim, sizeof(claim),
                "Word from %s: the granary is down to %d weeks.",
                place, weeks);
        }
        break;
    }
    const char *bias = version->court_bias >= 15 ?
        " Loyal voices credit the crown." :
        version->court_bias <= -15 ? " Some blame the court." : "";
    const char *alarm = version->alarm >= 30 ?
        " They fear worse is coming." : "";
    (void)snprintf(text, capacity, "%s%s%s", claim, bias, alarm);
    return true;
}
