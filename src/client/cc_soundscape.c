#include "client/cc_soundscape.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#define TAU 6.28318530718f
#define CUE(cue) (UINT32_C(1) << (unsigned int)(cue))

uint32_t CcSoundscapeStep(CcSoundscape *state, CcSoundFrame frame, float dt)
{
    if (state == NULL) return 0U;
    if (!isfinite(dt) || dt <= 0.0f || dt > 0.25f ||
        !isfinite(frame.x) || !isfinite(frame.z) ||
        !isfinite(frame.travel_pace)) {
        *state = (CcSoundscape){0};
        return 0U;
    }
    float dx = frame.x - state->previous.x;
    float dz = frame.z - state->previous.z;
    float distance = sqrtf(dx * dx + dz * dz);
    if (!state->initialized || frame.place != state->previous.place ||
        frame.scene != state->previous.scene || distance > 3.0f) {
        *state = (CcSoundscape){.previous = frame, .initialized = true};
        return 0U;
    }
    uint32_t cues = 0U;
    if (frame.walking && state->previous.walking &&
        frame.grounded && state->previous.grounded &&
        !frame.swimming && !state->previous.swimming) {
        for (int foot = 0; foot < 2; ++foot) {
            if (!frame.footfall[foot]) continue;
            CcSoundCue surface = frame.foot_surface[foot];
            if (surface < CC_SOUND_STEP_STONE || surface > CC_SOUND_SPLASH)
                surface = CC_SOUND_STEP_DIRT;
            cues |= CUE(surface);
        }
    }
    if (frame.walking && frame.swimming && state->previous.swimming) {
        state->swim_distance += distance;
        if (state->swim_distance >= 0.85f) {
            cues |= CUE(CC_SOUND_SPLASH);
            state->swim_distance = fmodf(state->swim_distance, 0.85f);
        }
    } else {
        state->swim_distance = 0.0f;
    }
    if (frame.walking && state->previous.walking) {
        if (frame.jumping && !state->previous.jumping) cues |= CUE(CC_SOUND_JUMP);
        if (frame.grounded && !state->previous.grounded) cues |= CUE(CC_SOUND_LAND);
        if (frame.striking && (!state->previous.striking ||
            frame.strike_time < state->previous.strike_time)) cues |= CUE(CC_SOUND_SWING);
        if (frame.impact_time > state->previous.impact_time) {
            cues |= CUE(frame.blocked ? CC_SOUND_BLOCK : CC_SOUND_HIT);
        }
    }
    if (frame.travel_pace > 0.05f) {
        float pace = fminf(frame.travel_pace, 1.0f);
        state->hoof_time += dt;
        state->wheel_time += dt;
        float hoof_interval = 0.42f - 0.19f * pace;
        float wheel_interval = 0.95f - 0.30f * pace;
        if (state->hoof_time >= hoof_interval) {
            cues |= CUE(CC_SOUND_HOOF);
            state->hoof_time = fmodf(state->hoof_time, hoof_interval);
        }
        if (state->wheel_time >= wheel_interval) {
            cues |= CUE(CC_SOUND_WHEEL);
            state->wheel_time = fmodf(state->wheel_time, wheel_interval);
        }
    } else {
        state->hoof_time = 0.0f;
        state->wheel_time = 0.0f;
    }
    state->previous = frame;
    return cues;
}

size_t CcSoundSampleCount(CcSoundCue cue)
{
    static const float seconds[CC_SOUND_COUNT] = {
        0.13f, 0.15f, 0.15f, 0.18f, 0.24f, 0.20f, 0.78f, 0.16f,
        0.21f, 0.18f, 0.18f, 0.28f, 0.14f, 0.32f, 0.48f
    };
    if (cue < 0 || cue >= CC_SOUND_COUNT) return 0U;
    return (size_t)(seconds[cue] * (float)CC_SOUND_SAMPLE_RATE);
}

static float Noise(uint32_t *seed)
{
    *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return (float)(*seed >> 8U) / 8388607.5f - 1.0f;
}

static float Ring(float time, float frequency, float decay)
{
    return sinf(TAU * frequency * time) * expf(-decay * time);
}

static float ChipNote(float time, float frequency, float decay)
{
    if (time <= 0.0f) return 0.0f;
    float phase = fmodf(time * frequency, 1.0f);
    float triangle = 1.0f - 4.0f * fabsf(phase - 0.5f);
    float pulse = phase < 0.25f ? 0.75f : -0.25f;
    return (0.82f * triangle + 0.18f * pulse) * expf(-decay * time) *
           fminf(1.0f, time / 0.003f);
}

bool CcSoundSynthesize(CcSoundCue cue, uint32_t variation,
                       int16_t *samples, size_t capacity)
{
    size_t count = CcSoundSampleCount(cue);
    if (samples == NULL || count == 0U || capacity < count) return false;
    uint32_t seed = UINT32_C(0x7193ac51) ^ variation ^ (uint32_t)cue;
    float pitch = 0.94f + (float)(variation % 13U) * 0.01f;
    float low = 0.0f;
    float soft = 0.0f;
    float held = 0.0f;
    double energy = 0.0;
    for (size_t i = 0; i < count; ++i) {
        float time = (float)i / (float)CC_SOUND_SAMPLE_RATE;
        float noise = Noise(&seed);
        low += 0.13f * (noise - low);
        soft += 0.045f * (noise - soft);
        float sample = 0.0f;
        switch (cue) {
            case CC_SOUND_STEP_STONE:
                sample = 0.23f * (noise - low) * expf(-200.0f * time) +
                         0.08f * Ring(time, 1350.0f * pitch, 180.0f); break;
            case CC_SOUND_STEP_WOOD:
                sample = 0.18f * Ring(time, 115.0f * pitch, 48.0f) +
                         0.07f * Ring(time, 310.0f * pitch, 75.0f) +
                         0.20f * low * expf(-32.0f * time); break;
            case CC_SOUND_STEP_DIRT:
                sample = 0.16f * Ring(time, 78.0f * pitch, 48.0f) +
                         0.18f * soft * expf(-36.0f * time); break;
            case CC_SOUND_STEP_GRASS:
                sample = (0.42f * soft + 0.07f * (low - soft)) *
                         expf(-22.0f * time) +
                         0.025f * Ring(time, 70.0f * pitch, 65.0f); break;
            case CC_SOUND_SPLASH:
                sample = 0.48f * low * expf(-13.0f * time) +
                         0.16f * sinf(TAU * (420.0f * time - 380.0f * time * time)) *
                         expf(-18.0f * time); break;
            case CC_SOUND_HOOF: {
                float second = fmaxf(0.0f, time - 0.073f);
                sample = 0.43f * Ring(time, 230.0f * pitch, 60.0f) +
                         0.32f * Ring(second, 165.0f * pitch, 48.0f) +
                         0.14f * noise * expf(-80.0f * time); break;
            }
            case CC_SOUND_WHEEL: {
                float envelope = sinf(3.14159265f * (float)i / (float)count);
                float rattle = 0.65f + 0.35f * sinf(TAU * 19.0f * time);
                sample = (0.34f * low * rattle +
                          0.13f * sinf(TAU * 74.0f * pitch * time) +
                          0.06f * sinf(TAU * 121.0f * pitch * time) +
                          0.035f * Ring(fmodf(time, 0.17f), 310.0f * pitch, 38.0f)) *
                         envelope; break;
            }
            case CC_SOUND_JUMP:
                sample = 0.22f * ChipNote(time, 220.0f * pitch, 42.0f) +
                         0.19f * ChipNote(time - 0.04f, 293.66f * pitch, 42.0f) +
                         0.16f * ChipNote(time - 0.08f, 392.0f * pitch, 42.0f) +
                         0.20f * low * expf(-24.0f * time); break;
            case CC_SOUND_SWING:
            case CC_SOUND_PAGE:
                sample = (cue == CC_SOUND_PAGE ? 0.28f * noise : 0.68f * low) *
                         sinf(3.14159265f * (float)i / (float)count) *
                         expf(-8.0f * time); break;
            case CC_SOUND_LAND:
                sample = 0.25f * Ring(time, 72.0f * pitch, 35.0f) +
                         0.24f * low * expf(-25.0f * time); break;
            case CC_SOUND_HIT:
                sample = 0.50f * Ring(time, 135.0f * pitch, 25.0f) +
                         0.26f * low * expf(-18.0f * time) +
                         0.18f * noise * expf(-80.0f * time); break;
            case CC_SOUND_BLOCK:
                sample = 0.30f * Ring(time, 870.0f * pitch, 17.0f) +
                         0.21f * Ring(time, 1423.0f * pitch, 24.0f) +
                         0.20f * noise * expf(-95.0f * time); break;
            case CC_SOUND_COINS: {
                sample = 0.27f * ChipNote(time, 1568.0f * pitch, 25.0f) +
                         0.22f * ChipNote(time - 0.07f, 2093.0f * pitch, 25.0f); break;
            }
            case CC_SOUND_PROMISE:
                sample = 0.27f * ChipNote(time, 392.0f, 9.0f) +
                         0.19f * ChipNote(time - 0.10f, 587.33f, 10.0f); break;
            default: break;
        }
        /* Keep foot contacts soft; retain the chip texture for action cues. */
        if (cue > CC_SOUND_SPLASH && cue != CC_SOUND_LAND) {
            if (i % 3U == 0U) held = roundf(sample * 127.0f) / 127.0f;
            sample = 0.65f * sample + 0.35f * held;
        }
        /* A short fade at both ends keeps each cue smooth at its boundary. */
        float attack = cue == CC_SOUND_STEP_GRASS ? 154.0f :
                       cue == CC_SOUND_STEP_STONE ? 11.0f : 44.0f;
        float fade = fminf(1.0f, (float)i / attack) *
                     fminf(1.0f, (float)(count - 1U - i) / 440.0f);
        sample = fmaxf(-0.9f, fminf(0.9f, sample * fade));
        samples[i] = (int16_t)lroundf(sample * 32767.0f);
        energy += (double)samples[i] * (double)samples[i];
    }
    if (cue <= CC_SOUND_SPLASH && energy > 0.0) {
        /* Match every surface and variation to the quiet grass sample level. */
        double gain = 0.014 * 32767.0 / sqrt(energy / (double)count);
        for (size_t i = 0; i < count; ++i) {
            samples[i] = (int16_t)lround((double)samples[i] * gain);
        }
    }
    return true;
}

bool CcSoundVoicePath(const char *id, const char *speaker, const char *text,
                      char *path, size_t capacity)
{
    if (id == NULL || id[0] == '\0' || text == NULL || text[0] == '\0' ||
        path == NULL || capacity == 0U) return false;
    for (const char *p = id; *p != '\0'; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
              *p == '.' || *p == '_')) return false;
    }
    uint32_t hash = UINT32_C(2166136261);
    const char *voice = speaker != NULL ? speaker : "";
    for (const unsigned char *p = (const unsigned char *)voice; *p != 0U; ++p) {
        hash = (hash ^ *p) * UINT32_C(16777619);
    }
    hash = (hash ^ (unsigned char)'\n') * UINT32_C(16777619);
    for (const unsigned char *p = (const unsigned char *)text; *p != 0U; ++p) {
        hash = (hash ^ *p) * UINT32_C(16777619);
    }
    int length = snprintf(path, capacity, "assets/audio/voice/%s-%08" PRIx32 ".wav", id, hash);
    return length > 0 && (size_t)length < capacity;
}
