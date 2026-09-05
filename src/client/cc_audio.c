#include "client/cc_audio.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VARIATIONS 3

static struct {
    Sound sounds[CC_SOUND_COUNT][VARIATIONS];
    Music voice;
    char voice_path[768];
    unsigned int next[CC_SOUND_COUNT];
    double last_play[CC_SOUND_COUNT];
    int mode;
    bool attempted, ready, focused, voice_loaded;
} audio;

static void StopVoice(void)
{
    if (audio.voice_loaded) {
        StopMusicStream(audio.voice);
        UnloadMusicStream(audio.voice);
        audio.voice_loaded = false;
    }
}

static void StopEffects(void)
{
    if (!audio.ready) return;
    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
        for (int variant = 0; variant < VARIATIONS; ++variant) {
            Sound sound = audio.sounds[cue][variant];
            if (IsSoundValid(sound)) StopSound(sound);
        }
    }
}

void CcAudioInit(void)
{
    if (audio.attempted) return;
    audio.attempted = true;
    InitAudioDevice();
    audio.ready = IsAudioDeviceReady();
    audio.focused = true;
    if (!audio.ready) return;
    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
        audio.last_play[cue] = -1.0;
        size_t count = CcSoundSampleCount((CcSoundCue)cue);
        int16_t *samples = malloc(count * sizeof(*samples));
        if (samples == NULL) continue;
        for (int variant = 0; variant < VARIATIONS; ++variant) {
            if (!CcSoundSynthesize((CcSoundCue)cue, (uint32_t)variant * 37U,
                                   samples, count)) continue;
            Wave wave = {.frameCount = (unsigned int)count,
                         .sampleRate = CC_SOUND_SAMPLE_RATE,
                         .sampleSize = 16U, .channels = 1U, .data = samples};
            audio.sounds[cue][variant] = LoadSoundFromWave(wave);
        }
        free(samples);
    }
}

void CcAudioShutdown(void)
{
    StopVoice();
    if (audio.ready) {
        for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
            for (int variant = 0; variant < VARIATIONS; ++variant) {
                Sound sound = audio.sounds[cue][variant];
                if (IsSoundValid(sound)) UnloadSound(sound);
            }
        }
        CloseAudioDevice();
    }
    memset(&audio, 0, sizeof(audio));
}

void CcAudioSetMode(int mode)
{
    audio.mode = mode >= 0 && mode <= 2 ? mode : 0;
    if (audio.mode > 0) StopVoice();
    if (audio.mode == 2) StopEffects();
}

void CcAudioSetFocused(bool focused)
{
    if (audio.focused && !focused) {
        StopVoice();
        StopEffects();
    }
    audio.focused = focused;
}

float CcAudioMusicGain(void)
{
    if (!audio.ready || !audio.focused || audio.mode > 0) return 0.0f;
    return audio.voice_loaded && IsMusicStreamPlaying(audio.voice) ? 0.36f : 1.0f;
}

static float CueVolume(CcSoundCue cue)
{
    float volume = cue <= CC_SOUND_WHEEL ? 0.32f : 0.48f;
    if (cue <= CC_SOUND_SPLASH) volume = 0.22f;
    if (cue == CC_SOUND_STEP_WOOD || cue == CC_SOUND_STEP_DIRT) volume = 0.20f;
    if (cue == CC_SOUND_STEP_GRASS) volume = 0.18f;
    if (cue == CC_SOUND_LAND) volume = 0.28f;
    if (audio.voice_loaded && IsMusicStreamPlaying(audio.voice)) volume *= 0.36f;
    return volume;
}

void CcAudioPlay(CcSoundCue cue)
{
    if (!audio.ready || !audio.focused || audio.mode == 2 ||
        cue < 0 || cue >= CC_SOUND_COUNT) return;
    double now = GetTime();
    if (now - audio.last_play[cue] < 0.075) return;
    audio.last_play[cue] = now;
    unsigned int variant = audio.next[cue]++ % VARIATIONS;
    Sound sound = audio.sounds[cue][variant];
    if (!IsSoundValid(sound)) return;
    SetSoundVolume(sound, CueVolume(cue));
    PlaySound(sound);
}

void CcAudioVoice(const char *path)
{
    if (!audio.ready) return;
    const char *requested = path != NULL ? path : "";
    if (strcmp(requested, audio.voice_path) == 0) return;
    StopVoice();
    (void)snprintf(audio.voice_path, sizeof(audio.voice_path), "%s", requested);
    if (!audio.focused || audio.mode > 0 || requested[0] == '\0' ||
        !FileExists(requested)) return;
    audio.voice = LoadMusicStream(requested);
    audio.voice_loaded = IsMusicValid(audio.voice);
    if (audio.voice_loaded) {
        audio.voice.looping = false;
        SetMusicVolume(audio.voice, 0.85f);
        PlayMusicStream(audio.voice);
    }
}

void CcAudioUpdate(void)
{
    if (!audio.ready) return;
    if (audio.voice_loaded) UpdateMusicStream(audio.voice);
    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
        for (int variant = 0; variant < VARIATIONS; ++variant) {
            Sound sound = audio.sounds[cue][variant];
            if (IsSoundValid(sound)) SetSoundVolume(sound, CueVolume((CcSoundCue)cue));
        }
    }
}
