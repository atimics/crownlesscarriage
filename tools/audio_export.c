#include "client/cc_soundscape.h"
#include "story/cc_story.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void JsonString(FILE *file, const char *text)
{
    (void)fputc('"', file);
    for (const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");
         *p != 0U; ++p) {
        if (*p == '"' || *p == '\\') (void)fputc('\\', file);
        if (*p < 32U) (void)fprintf(file, "\\u%04x", (unsigned int)*p);
        else (void)fputc(*p, file);
    }
    (void)fputc('"', file);
}

static bool WriteLine(FILE *file, const char *id, const char *speaker, const char *text)
{
    char path[256];
    if (!CcSoundVoicePath(id, speaker, text, path, sizeof(path))) return false;
    (void)fputs("  {\"id\": ", file); JsonString(file, id);
    (void)fputs(", \"speaker\": ", file); JsonString(file, speaker);
    (void)fputs(", \"text\": ", file); JsonString(file, text);
    (void)fputs(", \"path\": ", file); JsonString(file, path);
    (void)fputc('}', file);
    return !ferror(file);
}

static bool ExportOpening(FILE *file)
{
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    const CcSituation *relief = NULL;
    for (int32_t i = 0; i < sim.situation_count; ++i) {
        if (sim.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            relief = &sim.situations[i];
            break;
        }
    }
    if (relief == NULL) return false;
    sim.player.location_id = CcSimSituationOfferSettlementId(&sim, relief);
    sim.carriage.location_id = sim.player.location_id;
    (void)fputs("[\n", file);
    for (int stage = 0; stage < 4; ++stage) {
        if (stage > 0) {
            CcCommand command = {
                .kind = stage == 3 ? CC_COMMAND_ABANDON_SITUATION : CC_COMMAND_CHARACTER_RESPONSE,
                .target_id = relief->id,
                .amount = stage == 1 ? CC_CHARACTER_RESPONSE_LISTEN : CC_CHARACTER_RESPONSE_PLEDGE_HELP
            };
            char error[192];
            if (!CcSimApply(&sim, &command, error, sizeof(error))) {
                (void)fprintf(stderr, "%s\n", error);
                return false;
            }
            (void)fputs(",\n", file);
        }
        const CcCharacter *speaker = CcSimSituationSponsorCharacter(&sim, relief);
        const CcStoryLine *line = CcStoryCharacterLine(&sim, relief, speaker);
        char spoken[192];
        if (speaker == NULL || line == NULL ||
            !CcStoryCharacterText(&sim, relief, speaker, spoken, sizeof(spoken)) ||
            !WriteLine(file, line->id, speaker->name, spoken)) return false;
    }
    (void)fputs("\n]\n", file);
    return !ferror(file);
}

static bool ExportScript(FILE *file)
{
    (void)fputs("[\n", file);
    for (size_t i = 0U; i < CcStoryAuthoredLineCount(); ++i) {
        const CcStoryLine *line = CcStoryAuthoredLineAt(i);
        if (line == NULL) return false;
        if (i > 0U) (void)fputs(",\n", file);
        if (!WriteLine(file, line->id, CcStoryAuthoredSpeakerAt(i), line->text)) return false;
    }
    (void)fputs("\n]\n", file);
    return !ferror(file);
}

static void LittleEndian(FILE *file, uint32_t value, unsigned int bytes)
{
    for (unsigned int i = 0U; i < bytes; ++i) {
        (void)fputc((int)((value >> (i * 8U)) & 255U), file);
    }
}

static bool ExportPreview(FILE *file)
{
    const size_t gap = CC_SOUND_SAMPLE_RATE / 3U;
    size_t count = 0U;
    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) count += CcSoundSampleCount((CcSoundCue)cue) + gap;
    (void)fputs("RIFF", file); LittleEndian(file, (uint32_t)(36U + count * 2U), 4U);
    (void)fputs("WAVEfmt ", file); LittleEndian(file, 16U, 4U);
    LittleEndian(file, 1U, 2U); LittleEndian(file, 1U, 2U);
    LittleEndian(file, CC_SOUND_SAMPLE_RATE, 4U);
    LittleEndian(file, CC_SOUND_SAMPLE_RATE * 2U, 4U);
    LittleEndian(file, 2U, 2U); LittleEndian(file, 16U, 2U);
    (void)fputs("data", file); LittleEndian(file, (uint32_t)(count * 2U), 4U);
    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
        size_t frames = CcSoundSampleCount((CcSoundCue)cue);
        int16_t *samples = malloc(frames * sizeof(*samples));
        if (samples == NULL) return false;
        bool generated = CcSoundSynthesize((CcSoundCue)cue, 37U, samples, frames);
        if (generated) {
            for (size_t i = 0U; i < frames; ++i) LittleEndian(file, (uint16_t)samples[i], 2U);
            for (size_t i = 0U; i < gap; ++i) LittleEndian(file, 0U, 2U);
        }
        free(samples);
        if (!generated) return false;
    }
    return !ferror(file);
}

int main(int argc, char **argv)
{
    if (argc != 3 || (strcmp(argv[1], "--script") != 0 && strcmp(argv[1], "--opening") != 0 &&
                      strcmp(argv[1], "--preview") != 0)) {
        (void)fprintf(stderr, "Usage: crownless_audio_export --script script.json | --opening opening.json | --preview preview.wav\n");
        return 1;
    }
    FILE *file = fopen(argv[2], "wb");
    if (file == NULL) return 1;
    bool ok = strcmp(argv[1], "--script") == 0 ? ExportScript(file) :
              strcmp(argv[1], "--opening") == 0 ? ExportOpening(file) : ExportPreview(file);
    if (fclose(file) != 0) ok = false;
    return ok ? 0 : 1;
}
