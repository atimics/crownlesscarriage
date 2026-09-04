#include "client/cc_soundscape.h"
#include "test_support.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CUE(cue) (UINT32_C(1) << (unsigned int)(cue))

int main(void)
{
    CcSoundscape state = {0};
    CcSoundFrame frame = {.grounded = true, .walking = true,
                          .surface = CC_SOUND_STEP_STONE};
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    for (int i = 0; i < 120; ++i) CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.x = 0.9f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_STEP_STONE));
    frame.surface = CC_SOUND_STEP_WOOD;
    frame.x += 0.9f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_STEP_WOOD));
    frame.grounded = false;
    frame.jumping = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_JUMP));
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.x += 0.9f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.grounded = true;
    frame.jumping = false;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_LAND));
    frame.striking = true;
    frame.strike_time = 0.1f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_SWING));
    frame.strike_time = 0.2f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.impact_time = 1.0f;
    frame.blocked = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_BLOCK));
    frame.impact_time = 0.98f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.walking = false;
    frame.x += 0.9f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.walking = true;
    frame.place = UINT64_C(1) << 40U;
    frame.grounded = false;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.x += 100.0f;
    frame.grounded = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    CC_CHECK(CcSoundscapeStep(&state, frame, 5.0f) == 0U);
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    CC_CHECK(CcSoundscapeStep(&state, frame, NAN) == 0U);
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.travel_pace = 8.0f;
    int hooves = 0, wheels = 0;
    for (int i = 0; i < 600; ++i) {
        uint32_t cues = CcSoundscapeStep(&state, frame, 1.0f / 60.0f);
        if ((cues & CUE(CC_SOUND_HOOF)) != 0U) ++hooves;
        if ((cues & CUE(CC_SOUND_WHEEL)) != 0U) ++wheels;
    }
    CC_CHECK(hooves >= 40 && hooves <= 44);
    CC_CHECK(wheels >= 14 && wheels <= 16);
    frame.travel_pace = 0.0f;
    for (int i = 0; i < 600; ++i) CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);

    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
        size_t count = CcSoundSampleCount((CcSoundCue)cue);
        CC_CHECK(count > 1000U && count < CC_SOUND_SAMPLE_RATE);
        int16_t *samples = malloc((count + 1U) * sizeof(*samples));
        int16_t *repeat = malloc(count * sizeof(*repeat));
        CC_CHECK(samples != NULL && repeat != NULL);
        samples[count] = 12345;
        CC_CHECK(!CcSoundSynthesize((CcSoundCue)cue, 37U, samples, count - 1U));
        CC_CHECK(CcSoundSynthesize((CcSoundCue)cue, 37U, samples, count));
        CC_CHECK(CcSoundSynthesize((CcSoundCue)cue, 37U, repeat, count));
        CC_CHECK(memcmp(samples, repeat, count * sizeof(*samples)) == 0);
        CC_CHECK(samples[0] == 0 && samples[count - 1U] == 0 && samples[count] == 12345);
        double energy = 0.0;
        for (size_t i = 0; i < count; ++i) {
            CC_CHECK(abs(samples[i]) < 30000);
            double sample = (double)samples[i] / 32768.0;
            energy += sample * sample;
        }
        CC_CHECK(energy / (double)count > 0.00001);
        CC_CHECK(energy / (double)count < 0.12);
        free(samples);
        free(repeat);
    }
    char path[256], changed[256];
    CC_CHECK(CcSoundVoicePath("test.line", "Mara", "hello", path, sizeof(path)));
    CC_CHECK(strcmp(path, "assets/audio/voice/test.line-32f1dd9c.wav") == 0);
    CC_CHECK(CcSoundVoicePath("test.line", "Mara", "Hello", changed, sizeof(changed)));
    CC_CHECK(strcmp(path, changed) != 0);
    CC_CHECK(CcSoundVoicePath("test.line", "Tomas", "hello", changed, sizeof(changed)));
    CC_CHECK(strcmp(path, changed) != 0);
    CC_CHECK(!CcSoundVoicePath("../line", "Mara", "hello", path, sizeof(path)));
    CC_CHECK(!CcSoundVoicePath("line", "Mara", "hello", path, 4U));
    CC_CHECK(!CcSoundVoicePath(NULL, "Mara", "hello", path, sizeof(path)));
    return 0;
}
