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
    if (frame.walking && frame.grounded && distance > 0.002f) {
        state->step_distance += distance;
        if (state->step_distance >= 0.85f) {
            CcSoundCue surface = frame.surface;
            if (surface < CC_SOUND_STEP_STONE || surface > CC_SOUND_SPLASH) {
                surface = CC_SOUND_STEP_DIRT;
            }
            cues |= CUE(surface);
            state->step_distance = fmodf(state->step_distance, 0.85f);
        }
    } else {
        state->step_distance = 0.0f;
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
        if (state->hoof_time >= 0.42f - 0.19f * pace) {
            cues |= CUE(CC_SOUND_HOOF);
            state->hoof_time = 0.0f;
        }
        if (state->wheel_time >= 0.95f - 0.30f * pace) {
            cues |= CUE(CC_SOUND_WHEEL);
            state->wheel_time = 0.0f;
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
        0.16f, 0.18f, 0.19f, 0.30f, 0.23f, 0.50f, 0.18f,
        0.26f, 0.24f, 0.22f, 0.36f, 0.20f, 0.42f, 0.65f
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

bool CcSoundSynthesize(CcSoundCue cue, uint32_t variation,
                       int16_t *samples, size_t capacity)
{
    size_t count = CcSoundSampleCount(cue);
    if (samples == NULL || count == 0U || capacity < count) return false;
    uint32_t seed = UINT32_C(0x7193ac51) ^ variation ^ (uint32_t)cue;
    float pitch = 0.94f + (float)(variation % 13U) * 0.01f;
    float low = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        float time = (float)i / (float)CC_SOUND_SAMPLE_RATE;
        float noise = Noise(&seed);
        low += 0.13f * (noise - low);
        float sample = 0.0f;
        switch (cue) {
            case CC_SOUND_STEP_STONE:
                sample = 0.48f * Ring(time, 185.0f * pitch, 42.0f) +
                         0.30f * noise * expf(-65.0f * time); break;
            case CC_SOUND_STEP_WOOD:
                sample = 0.46f * Ring(time, 115.0f * pitch, 32.0f) +
                         0.22f * Ring(time, 310.0f * pitch, 55.0f) +
                         0.16f * low * expf(-28.0f * time); break;
            case CC_SOUND_STEP_DIRT:
                sample = 0.40f * low * expf(-24.0f * time) +
                         0.24f * Ring(time, 90.0f * pitch, 48.0f) +
                         0.10f * noise * expf(-35.0f * time); break;
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
            case CC_SOUND_WHEEL:
                sample = 0.23f * low * (0.6f + 0.4f * sinf(TAU * 19.0f * time)) +
                         0.10f * sinf(TAU * (88.0f * time + 8.0f * time * time)) *
                         sinf(3.14159265f * time / 0.50f); break;
            case CC_SOUND_JUMP:
            case CC_SOUND_SWING:
            case CC_SOUND_PAGE:
                sample = (cue == CC_SOUND_PAGE ? 0.28f * noise : 0.68f * low) *
                         sinf(3.14159265f * (float)i / (float)count) *
                         expf(-8.0f * time); break;
            case CC_SOUND_LAND:
            case CC_SOUND_HIT:
                sample = 0.50f * Ring(time, (cue == CC_SOUND_HIT ? 135.0f : 72.0f) * pitch, 25.0f) +
                         0.26f * low * expf(-18.0f * time) +
                         0.18f * noise * expf(-80.0f * time); break;
            case CC_SOUND_BLOCK:
                sample = 0.30f * Ring(time, 870.0f * pitch, 17.0f) +
                         0.21f * Ring(time, 1423.0f * pitch, 24.0f) +
                         0.20f * noise * expf(-95.0f * time); break;
            case CC_SOUND_COINS: {
                float second = fmaxf(0.0f, time - 0.09f);
                sample = 0.27f * Ring(time, 1870.0f * pitch, 25.0f) +
                         0.22f * Ring(second, 2460.0f * pitch, 21.0f); break;
            }
            case CC_SOUND_PROMISE:
                sample = 0.27f * Ring(time, 392.0f, 7.0f) +
                         0.19f * Ring(fmaxf(0.0f, time - 0.12f), 587.33f, 8.0f); break;
            default: break;
        }
        /* A short fade at both ends keeps each cue smooth at its boundary. */
        float fade = fminf(1.0f, (float)i / 44.0f) *
                     fminf(1.0f, (float)(count - 1U - i) / 440.0f);
        sample = fmaxf(-0.9f, fminf(0.9f, sample * fade));
        samples[i] = (int16_t)lroundf(sample * 32767.0f);
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
