#include "client/cc_soundscape.h"
#include "locomotion/cc_humanoid.h"
#include "test_support.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CUE(cue) (UINT32_C(1) << (unsigned int)(cue))

static bool PlaneProbe(void *context, CcLimbVec3 origin, float distance,
                        CcLimbVec3 *point, CcLimbVec3 *normal)
{
    (void)context;
    if (origin.y < 0.0f || origin.y > distance) return false;
    *point = (CcLimbVec3){origin.x, 0.0f, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static int CheckGaitTiming(int render_rate, float cadence)
{
    CcHumanoidGait gait;
    CcLimbVec3 body = {0};
    CcHumanoidGaitInit(&gait, body, 0.0f, PlaneProbe, NULL);
    CcHumanoidGaitSetWalkingProfile(&gait, cadence, 1.0f);
    CcSoundscape soundscape = {0};
    CcSoundFrame frame = {.walking = true, .grounded = true,
        .foot_surface = {CC_SOUND_STEP_STONE, CC_SOUND_STEP_GRASS}};
    float dt = 1.0f / (float)render_rate;
    CC_CHECK(CcSoundscapeStep(&soundscape, frame, dt) == 0U);
    int contacts = 0, sounds = 0, ticks = 0;
    for (int render = 0; render < render_rate * 8; ++render) {
        /* Match the client's fixed physics step and variable render rate. */
        int target_ticks = (render + 1) * 60 / render_rate;
        for (; ticks < target_ticks; ++ticks) {
            CcLimbVec3 velocity = {0.0f, 0.0f, ticks < 300 ? 1.0f : 0.0f};
            CcHumanoidContact before[2] = {gait.feet[0].contact, gait.feet[1].contact};
            CcHumanoidGaitAdvance(&gait, body, 0.0f, velocity, true,
                                  1.0f / 60.0f, PlaneProbe, NULL);
            body.x += gait.root_velocity.x / 60.0f;
            body.z += gait.root_velocity.z / 60.0f;
            for (int foot = 0; foot < 2; ++foot) {
                if (before[foot] == CC_HUMANOID_CONTACT_SWING &&
                    gait.feet[foot].contact == CC_HUMANOID_CONTACT_HEEL) ++contacts;
            }
        }
        uint32_t markers = CcHumanoidGaitConsumeMotionMarkers(&gait);
        frame.x = body.x;
        frame.z = body.z;
        frame.footfall[0] = (markers & CC_MOTION_MARKER_LEFT_CONTACT) != 0U;
        frame.footfall[1] = (markers & CC_MOTION_MARKER_RIGHT_CONTACT) != 0U;
        uint32_t cues = CcSoundscapeStep(&soundscape, frame, dt);
        CC_CHECK(cues == ((frame.footfall[0] ? CUE(CC_SOUND_STEP_STONE) : 0U) |
                          (frame.footfall[1] ? CUE(CC_SOUND_STEP_GRASS) : 0U)));
        if ((cues & CUE(CC_SOUND_STEP_STONE)) != 0U) ++sounds;
        if ((cues & CUE(CC_SOUND_STEP_GRASS)) != 0U) ++sounds;
        if (render >= render_rate * 7) CC_CHECK(cues == 0U);
    }
    CC_CHECK(sounds == contacts && sounds >= 8);
    return sounds;
}

static void CheckSurfaceSoundProfiles(void)
{
    for (uint32_t variation = 0; variation <= 74U; variation += 37U) {
        double brightness[CC_SOUND_SPLASH + 1] = {0};
        double tail[CC_SOUND_SPLASH + 1] = {0};
        for (int cue = CC_SOUND_STEP_STONE; cue <= CC_SOUND_SPLASH; ++cue) {
            size_t count = CcSoundSampleCount((CcSoundCue)cue);
            int16_t *samples = malloc(count * sizeof(*samples));
            CC_CHECK(samples != NULL);
            CC_CHECK(CcSoundSynthesize((CcSoundCue)cue, variation, samples, count));
            double energy = 0.0, changes = 0.0, late = 0.0;
            for (size_t i = 0; i < count; ++i) {
                double sample = (double)samples[i] / 32768.0;
                double previous = i > 0 ? (double)samples[i - 1U] / 32768.0 : 0.0;
                energy += sample * sample;
                changes += (sample - previous) * (sample - previous);
                if (i > CC_SOUND_SAMPLE_RATE / 50U) late += sample * sample;
            }
            CC_CHECK(fabs(sqrt(energy / (double)count) - 0.014) < 0.00002);
            brightness[cue] = changes / energy;
            tail[cue] = late / energy;
            free(samples);
        }
        /* Stone is a brief click; road steps carry a low, rounded thud. */
        CC_CHECK(brightness[CC_SOUND_STEP_STONE] > brightness[CC_SOUND_STEP_DIRT] * 20.0);
        CC_CHECK(tail[CC_SOUND_STEP_STONE] < 0.01);
        CC_CHECK(tail[CC_SOUND_STEP_DIRT] > 0.10);
    }
}

static void CheckTravelTiming(void)
{
    const int rates[] = {30, 60, 120};
    const float paces[] = {0.48f, 0.72f, 1.0f};
    for (int pace = 0; pace < 3; ++pace) {
        for (int rate = 0; rate < 3; ++rate) {
            CcSoundscape state = {0};
            CcSoundFrame frame = {.travel_pace = paces[pace]};
            float dt = 1.0f / (float)rates[rate];
            (void)CcSoundscapeStep(&state, frame, dt);
            int hooves = 0, wheels = 0;
            for (int i = 0; i < rates[rate] * 10; ++i) {
                frame.x += dt;
                uint32_t cues = CcSoundscapeStep(&state, frame, dt);
                if ((cues & CUE(CC_SOUND_HOOF)) != 0U) ++hooves;
                if ((cues & CUE(CC_SOUND_WHEEL)) != 0U) ++wheels;
            }
            CC_CHECK(hooves == (int)(10.0f / (0.42f - 0.19f * paces[pace])));
            CC_CHECK(wheels == (int)(10.0f / (0.95f - 0.30f * paces[pace])));
            frame.travel_pace = 0;
            CC_CHECK(CcSoundscapeStep(&state, frame, dt) == 0U);
            CC_CHECK(state.hoof_time == 0 && state.wheel_time == 0);
        }
    }
    float wheel_seconds = (float)CcSoundSampleCount(CC_SOUND_WHEEL) /
        ((float)CC_SOUND_SAMPLE_RATE * CC_SOUND_PLAYBACK_PITCH);
    CC_CHECK(wheel_seconds > 0.90f && wheel_seconds < 1.0f);
}

int main(void)
{
    CheckSurfaceSoundProfiles();
    CheckTravelTiming();
    int slow = CheckGaitTiming(60, 0.78f);
    int fast = CheckGaitTiming(60, 1.22f);
    CC_CHECK(fast > slow);
    CC_CHECK(CheckGaitTiming(30, 1.22f) == fast);
    CC_CHECK(CheckGaitTiming(120, 1.22f) == fast);
    CcSoundscape state = {0};
    CcSoundFrame frame = {.grounded = true, .walking = true,
                          .foot_surface = {CC_SOUND_STEP_STONE, CC_SOUND_STEP_WOOD}};
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    for (int i = 0; i < 120; ++i) CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.x = 0.9f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.footfall[0] = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_STEP_STONE));
    frame.footfall[0] = false;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.footfall[1] = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_STEP_WOOD));
    frame.foot_surface[1] = CC_SOUND_STEP_GRASS;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_STEP_GRASS));
    frame.grounded = false;
    frame.jumping = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_JUMP));
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.x += 0.9f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.grounded = true;
    frame.jumping = false;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_LAND));
    frame.footfall[1] = false;
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
    frame.footfall[0] = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
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
    frame.footfall[0] = false;
    frame.swimming = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.x += 0.9f;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == CUE(CC_SOUND_SPLASH));
    frame.swimming = false;
    frame.footfall[0] = true;
    CC_CHECK(CcSoundscapeStep(&state, frame, 0.016f) == 0U);
    frame.footfall[0] = false;
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

    double mean_energy[CC_SOUND_COUNT] = {0};
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
        mean_energy[cue] = energy / (double)count;
        free(samples);
        free(repeat);
    }
    CC_CHECK(fabs(mean_energy[CC_SOUND_STEP_GRASS] - mean_energy[CC_SOUND_STEP_DIRT]) < 0.000001);
    CC_CHECK(fabs(mean_energy[CC_SOUND_STEP_GRASS] - mean_energy[CC_SOUND_STEP_STONE]) < 0.000001);
    CC_CHECK(mean_energy[CC_SOUND_STEP_STONE] < mean_energy[CC_SOUND_HIT] * 0.2);
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
