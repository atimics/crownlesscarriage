#include "story/cc_speech.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Version-one cast order is part of saved-world voice identity. Append new
 * profiles; keep the original generated pool and its IDs in place. */
static const CcVoiceProfile VOICES[] = {
    {"mara-v1", "Mara Venn", "A woman with a low, clear voice. Practical and warm, with steady pacing and a soft northern English accent."},
    {"jory-v1", "Jory Fen", "A man with a rough mid-range voice and a light Yorkshire accent. Direct, thoughtful, and slightly breathy."},
    {"tomas-v1", "Tomas Rill", "A man with a light baritone and a soft west-country English accent. Quick, dry humour and careful, quiet words."},
    {"ilyra-v1", "Ilyra Senn", "A woman with a clear alto and a light Scottish accent. Measured, firm delivery with a worn edge."},
    {"bren-v1", "Bren Alder", "A man with a soft tenor and a northern English accent. Hesitant starts, clear words, and a close, intimate sound."},
    {"hearth-v1", "Hearth", "A woman with a rounded middle register, a gentle Welsh accent, and relaxed, welcoming speech."},
    {"reed-v1", "Reed", "A man with a bright tenor, a light Irish accent, and quick, precise speech."},
    {"flint-v1", "Flint", "A woman with a dry low register, a light northern English accent, and short, firm phrases."},
    {"oak-v1", "Oak", "A man with a deep warm bass, a soft Scottish accent, and patient, evenly paced speech."},
    {"lark-v1", "Lark", "A woman with a clear, bright voice, a light west-country English accent, and lively, gentle speech."},
    {"ash-v1", "Ash", "A man with a grainy baritone, a light Welsh accent, and deliberate, unhurried speech."},
    {"brook-v1", "Brook", "A woman with a soft middle register, a light Irish accent, and calm, direct speech."},
    {"stone-v1", "Stone", "A man with a dry middle register, a northern English accent, and clipped, watchful speech."}
};

enum { GENERATED_VOICE_FIRST = 5, GENERATED_VOICE_COUNT = 8 };

size_t CcSpeechVoiceCount(void) { return sizeof(VOICES) / sizeof(VOICES[0]); }

const CcVoiceProfile *CcSpeechVoiceAt(size_t index)
{
    return index < CcSpeechVoiceCount() ? &VOICES[index] : NULL;
}

static uint64_t MixIdentity(uint64_t value)
{
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

uint32_t CcSpeechLocalVoice(uint32_t world_seed, CcId place, CcId object)
{
    uint64_t value = MixIdentity((uint64_t)world_seed ^ MixIdentity(place) ^
                                  MixIdentity(object + UINT64_C(0x9e3779b97f4a7c15)));
    return GENERATED_VOICE_FIRST + (uint32_t)(value % GENERATED_VOICE_COUNT);
}

uint32_t CcSpeechCharacterVoice(const CcSim *sim, const CcCharacter *character)
{
    if (sim == NULL || character == NULL) return GENERATED_VOICE_FIRST;
    if (character->generation == 0) {
        for (uint32_t i = 0; i < GENERATED_VOICE_FIRST; ++i) {
            if (strcmp(character->name, VOICES[i].name) == 0) return i;
        }
    }
    /* IDs and birthplaces survive travel, changing work, and appearance edits. */
    return CcSpeechLocalVoice(sim->world_seed, character->home_settlement_id,
                               character->id);
}

const char *CcSpeechDeliveryName(CcSpeechDelivery delivery)
{
    static const char *const names[] = {"plain", "warm", "worried", "urgent", "quiet", "firm"};
    return delivery >= 0 && delivery < CC_SPEECH_DELIVERY_COUNT ? names[delivery] : "plain";
}

static uint64_t HashField(uint64_t hash, const char *value)
{
    for (const unsigned char *p = (const unsigned char *)value; *p != 0U; ++p) {
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    }
    return (hash ^ 0U) * UINT64_C(1099511628211);
}

static bool CopyText(char *out, size_t capacity, const char *text)
{
    if (text == NULL || text[0] == '\0' || strlen(text) >= capacity) return false;
    memcpy(out, text, strlen(text) + 1U);
    return true;
}

bool CcSpeechCompose(CcSpeech *speech, const char *line_id, CcId speaker_id,
                      const char *speaker, uint32_t voice_index,
                      const char *text, CcSpeechDelivery delivery,
                      CcSpeechPriority priority, CcId source_event_id)
{
    if (speech == NULL) return false;
    *speech = (CcSpeech){0};
    CcSpeech next = {.speaker_id = speaker_id, .source_event_id = source_event_id,
        .voice_index = voice_index, .delivery = delivery, .priority = priority};
    const CcVoiceProfile *voice = CcSpeechVoiceAt(voice_index);
    if (voice == NULL || delivery < 0 || delivery >= CC_SPEECH_DELIVERY_COUNT ||
        priority < CC_SPEECH_BACKGROUND || priority > CC_SPEECH_WARNING ||
        !CopyText(next.line_id, sizeof(next.line_id), line_id) ||
        !CopyText(next.speaker, sizeof(next.speaker), speaker) ||
        !CopyText(next.text, sizeof(next.text), text)) return false;
    uint64_t hash = HashField(UINT64_C(14695981039346656037), "cc-speech-v1");
    hash = HashField(hash, voice->id);
    hash = HashField(hash, CC_SPEECH_LANGUAGE);
    hash = HashField(hash, CcSpeechDeliveryName(delivery));
    next.audio_key = HashField(hash, text);
    *speech = next;
    return true;
}

bool CcSpeechCharacter(const CcSim *sim, const CcSituation *situation,
                        const CcCharacter *character, CcSpeech *speech)
{
    if (speech == NULL) return false;
    *speech = (CcSpeech){0};
    char text[CC_SPEECH_TEXT_CAPACITY];
    if (!CcStoryCharacterText(sim, situation, character, text, sizeof(text))) return false;
    const CcStoryLine *line = CcStoryCharacterLine(sim, situation, character);
    char fallback[CC_SPEECH_LINE_CAPACITY];
    (void)snprintf(fallback, sizeof(fallback), "situation.%d.discovery.%d",
                   (int)situation->kind, (int)situation->discovery_stage);
    CcSpeechDelivery delivery = CC_SPEECH_PLAIN;
    if (line != NULL) {
        switch (line->beat) {
            case CC_STORY_BEAT_HELPED:
            case CC_STORY_BEAT_RESOLVED: delivery = CC_SPEECH_WARM; break;
            case CC_STORY_BEAT_FAILED:
            case CC_STORY_BEAT_PRESSING: delivery = CC_SPEECH_WORRIED; break;
            case CC_STORY_BEAT_BREAKING: delivery = CC_SPEECH_URGENT; break;
            case CC_STORY_BEAT_WITNESS: delivery = CC_SPEECH_QUIET; break;
            case CC_STORY_BEAT_WITHDREW: delivery = CC_SPEECH_FIRM; break;
            default: break;
        }
    } else if (character->stress >= 70) delivery = CC_SPEECH_WORRIED;
    return CcSpeechCompose(speech, line != NULL ? line->id : fallback,
        character->id, character->name, CcSpeechCharacterVoice(sim, character), text,
        delivery, CC_SPEECH_CONVERSATION, situation->cause_event_id);
}

bool CcSpeechPath(const CcSpeech *speech, char *path, size_t capacity)
{
    if (path == NULL || capacity == 0) return false;
    path[0] = '\0';
    if (speech == NULL || speech->text[0] == '\0' ||
        CcSpeechVoiceAt(speech->voice_index) == NULL) return false;
    int length = snprintf(path, capacity, "assets/audio/speech/%016" PRIx64 ".wav",
                            speech->audio_key);
    if (length < 0 || (size_t)length >= capacity) { path[0] = '\0'; return false; }
    return true;
}
