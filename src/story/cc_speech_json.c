#include "story/cc_speech.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool JsonText(char *out, size_t capacity, size_t *used, const char *text)
{
    if (*used + 2 >= capacity) return false;
    out[(*used)++] = '"';
    for (const unsigned char *p = (const unsigned char *)text; *p != 0U; ++p) {
        if (*used + 7 >= capacity) return false;
        if (*p < 32U) {
            (void)snprintf(out + *used, capacity - *used, "\\u%04x", (unsigned int)*p);
            *used += 6;
        } else {
            if (*p == '"' || *p == '\\') out[(*used)++] = '\\';
            out[(*used)++] = (char)*p;
        }
    }
    out[(*used)++] = '"';
    out[*used] = '\0';
    return true;
}

bool CcSpeechJson(const CcSpeech *speech, char *json, size_t capacity)
{
    if (json == NULL || capacity == 0) return false;
    json[0] = '\0';
    if (speech == NULL || speech->text[0] == '\0') return false;
    const CcVoiceProfile *voice = CcSpeechVoiceAt(speech->voice_index);
    if (voice == NULL) return false;
    int size = snprintf(json, capacity,
        "{\"version\":1,\"key\":\"%016" PRIx64 "\",\"speaker_id\":\"%" PRIu64
        "\",\"source_event_id\":\"%" PRIu64 "\",\"priority\":%d",
        speech->audio_key, speech->speaker_id, speech->source_event_id, (int)speech->priority);
    if (size < 0 || (size_t)size >= capacity) { json[0] = '\0'; return false; }
    size_t used = (size_t)size;
    const char *keys[] = {"voice", "language", "delivery", "id", "speaker", "text"};
    const char *values[] = {voice->id, CC_SPEECH_LANGUAGE, CcSpeechDeliveryName(speech->delivery),
        speech->line_id, speech->speaker, speech->text};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        if (used + 2 >= capacity) { json[0] = '\0'; return false; }
        json[used++] = ',';
        if (!JsonText(json, capacity, &used, keys[i]) || used + 2 >= capacity) {
            json[0] = '\0'; return false;
        }
        json[used++] = ':';
        if (!JsonText(json, capacity, &used, values[i])) { json[0] = '\0'; return false; }
    }
    if (used + 2 >= capacity) { json[0] = '\0'; return false; }
    json[used++] = '}';
    json[used] = '\0';
    return true;
}
