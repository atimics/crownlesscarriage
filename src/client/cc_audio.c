#include "client/cc_audio.h"
#include "client/cc_voice_net.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VARIATIONS 3
#define SPEECH_QUEUE_CAPACITY 4

typedef struct QueuedSpeech {
    CcSpeech speech;
    char path[768];
    double expires;
} QueuedSpeech;

static struct {
    Sound sounds[CC_SOUND_COUNT][VARIATIONS];
    Music voice;
    char voice_path[768];
    unsigned int next[CC_SOUND_COUNT];
    double last_play[CC_SOUND_COUNT];
    int mode, voice_percent;
    bool attempted, ready, focused, voice_loaded;
    CcSpeech speech;
    bool has_speech;
    unsigned char *voice_bytes;
    uint64_t request_key, generation, request_generation;
    bool speech_pending;
    double request_after;
    uint64_t context;
    CcSpeech desired;
    char desired_path[768];
    bool has_desired, override;
    double override_expires, override_started;
    QueuedSpeech queue[SPEECH_QUEUE_CAPACITY];
    int queue_count;
} audio = {.voice_percent = 100};

static void BeginSpeech(const CcSpeech *speech, const char *path);

static Music LoadVoiceStream(const char *path, const unsigned char *data, size_t size)
{
    SetAudioStreamBufferSizeDefault(24000);
    Music voice = data != NULL ? LoadMusicStreamFromMemory(".wav", data, (int)size) :
        LoadMusicStream(path);
    SetAudioStreamBufferSizeDefault(0);
    return voice;
}

static void CancelRequest(void)
{
    ++audio.generation;
    CcVoiceNetCancel();
}

static void StopVoice(void)
{
    if (audio.voice_loaded) {
        StopMusicStream(audio.voice);
        UnloadMusicStream(audio.voice);
        audio.voice_loaded = false;
    }
    free(audio.voice_bytes);
    audio.voice_bytes = NULL;
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
    CcVoiceNetShutdown();
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
    audio.voice_percent = 100;
}

void CcAudioSetMode(int mode)
{
    audio.mode = mode >= 0 && mode <= 2 ? mode : 0;
    if (audio.mode > 0) {
        CcAudioClearSpeech();
    }
    if (audio.mode == 2) StopEffects();
}

void CcAudioSetFocused(bool focused)
{
    if (audio.focused && !focused) {
        CcAudioClearSpeech();
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
    if (cue == CC_SOUND_HOOF) volume = 0.46f;
    if (cue == CC_SOUND_WHEEL) volume = 0.42f;
    if (cue <= CC_SOUND_SPLASH) volume = 0.18f;
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
    SetSoundPitch(sound, CC_SOUND_PLAYBACK_PITCH);
    PlaySound(sound);
}

void CcAudioVoice(const char *path)
{
    if (!audio.ready) return;
    const char *requested = path != NULL ? path : "";
    if (strcmp(requested, audio.voice_path) == 0) return;
    StopVoice();
    (void)snprintf(audio.voice_path, sizeof(audio.voice_path), "%s", requested);
    if (!audio.focused || audio.mode > 0 || audio.voice_percent == 0 || requested[0] == '\0' ||
        !FileExists(requested)) return;
    audio.voice = LoadVoiceStream(requested, NULL, 0);
    audio.voice_loaded = IsMusicValid(audio.voice);
    if (audio.voice_loaded) {
        audio.voice.looping = false;
        SetMusicVolume(audio.voice, 0.85f * (float)audio.voice_percent / 100.0f);
        PlayMusicStream(audio.voice);
        UpdateMusicStream(audio.voice);
    }
}

void CcAudioUpdate(void)
{
    if (!audio.ready) return;
    if (audio.voice_loaded) UpdateMusicStream(audio.voice);
    unsigned char *data = NULL;
    size_t size = 0;
    int result = CcVoiceNetPoll(&data, &size);
    if (result != 0) {
        if (result > 0 && audio.has_speech && audio.speech_pending && audio.focused &&
            audio.mode == 0 && audio.voice_percent > 0 && audio.request_generation == audio.generation &&
            audio.request_key == audio.speech.audio_key && size <= CC_VOICE_DOWNLOAD_LIMIT) {
            StopVoice();
            audio.voice = LoadVoiceStream(NULL, data, size);
            audio.voice_loaded = IsMusicValid(audio.voice);
            if (audio.voice_loaded) {
                audio.voice_bytes = data;
                data = NULL;
                audio.voice.looping = false;
                SetMusicVolume(audio.voice, 0.85f * (float)audio.voice_percent / 100.0f);
                PlayMusicStream(audio.voice);
                UpdateMusicStream(audio.voice);
            }
        }
        if (audio.request_generation == audio.generation) audio.speech_pending = false;
        free(data);
    }
    if (audio.speech_pending && !CcVoiceNetBusy() &&
        audio.focused && audio.mode == 0 && audio.voice_percent > 0 && GetTime() >= audio.request_after) {
        audio.request_key = audio.speech.audio_key;
        audio.request_generation = audio.generation;
        if (!CcVoiceNetStart(&audio.speech)) audio.speech_pending = false;
    }
    if (audio.voice_loaded) {
        UpdateMusicStream(audio.voice);
        SetMusicVolume(audio.voice, 0.85f * (float)audio.voice_percent / 100.0f);
    }
    bool complete = audio.voice_loaded ? !IsMusicStreamPlaying(audio.voice) :
        !audio.speech_pending && GetTime() - audio.override_started >= 2.0;
    if (audio.override && (complete || GetTime() >= audio.override_expires)) {
        audio.override = false;
        BeginSpeech(NULL, NULL);
        while (!audio.override && audio.queue_count > 0) {
            QueuedSpeech next = audio.queue[0];
            --audio.queue_count;
            memmove(audio.queue, audio.queue + 1, (size_t)audio.queue_count * sizeof(audio.queue[0]));
            if (next.expires > GetTime()) CcAudioSay(&next.speech, next.path);
        }
        if (!audio.override) BeginSpeech(audio.has_desired ? &audio.desired : NULL, audio.desired_path);
    }
    for (int cue = 0; cue < CC_SOUND_COUNT; ++cue) {
        for (int variant = 0; variant < VARIATIONS; ++variant) {
            Sound sound = audio.sounds[cue][variant];
            if (IsSoundValid(sound)) SetSoundVolume(sound, CueVolume((CcSoundCue)cue));
        }
    }
}

static void BeginSpeech(const CcSpeech *speech, const char *path)
{
    if (speech == NULL || speech->text[0] == '\0') {
        audio.has_speech = false;
        audio.speech_pending = false;
        CancelRequest();
        StopVoice();
        audio.voice_path[0] = '\0';
        return;
    }
    if (!audio.ready) return;
    if (audio.has_speech && audio.speech.audio_key == speech->audio_key &&
        audio.speech.speaker_id == speech->speaker_id) return;
    audio.speech = *speech;
    audio.has_speech = true;
    CancelRequest();
    /* The same words may come from a different person in the next turn. */
    StopVoice();
    audio.voice_path[0] = '\0';
    CcAudioVoice(path);
    audio.speech_pending = !audio.voice_loaded && audio.focused && audio.mode == 0 && audio.voice_percent > 0;
    audio.request_after = GetTime() + 0.35;
}

void CcAudioSpeech(const CcSpeech *speech, const char *path)
{
    bool present = speech != NULL && speech->text[0] != '\0';
    if (present && audio.has_desired && speech->speaker_id != audio.desired.speaker_id) {
        audio.queue_count = 0;
        audio.override = false;
    }
    audio.has_desired = present;
    if (present) audio.desired = *speech;
    (void)snprintf(audio.desired_path, sizeof(audio.desired_path), "%s", path != NULL ? path : "");
    if (!audio.override) BeginSpeech(present ? speech : NULL, path);
}

void CcAudioSay(const CcSpeech *speech, const char *path)
{
    if (!audio.ready || speech == NULL || speech->text[0] == '\0' ||
        !audio.focused || audio.mode != 0 || audio.voice_percent == 0) return;
    if (speech->priority == CC_SPEECH_BACKGROUND && (audio.has_desired || audio.override)) return;
    if (audio.override && speech->priority <= audio.speech.priority) {
        if (audio.queue_count >= SPEECH_QUEUE_CAPACITY) return;
        for (int i = 0; i < audio.queue_count; ++i)
            if (audio.queue[i].speech.audio_key == speech->audio_key) return;
        QueuedSpeech *next = &audio.queue[audio.queue_count++];
        next->speech = *speech;
        next->expires = GetTime() + (strcmp(speech->line_id, "reader.page") == 0 ? 120.0 : 30.0);
        (void)snprintf(next->path, sizeof(next->path), "%s", path != NULL ? path : "");
        return;
    }
    if (speech->priority == CC_SPEECH_WARNING) audio.queue_count = 0;
    BeginSpeech(NULL, NULL);
    audio.override = true;
    audio.override_started = GetTime();
    audio.override_expires = GetTime() + ((speech->priority == CC_SPEECH_BACKGROUND || speech->priority == CC_SPEECH_WARNING ||
        strcmp(speech->line_id, "player.field") == 0) ? 6.0 : 45.0);
    BeginSpeech(speech, path);
}

void CcAudioClearSpeech(void)
{
    audio.queue_count = 0;
    audio.override = false;
    audio.has_desired = false;
    BeginSpeech(NULL, NULL);
}

void CcAudioSetContext(uint64_t context)
{
    if (audio.context == context) return;
    audio.context = context;
    CcAudioClearSpeech();
}

void CcAudioSetVoiceVolume(int percent)
{
    audio.voice_percent = percent >= 0 && percent <= 100 ? percent : 100;
    if (audio.voice_percent == 0) CcAudioClearSpeech();
}

void CcAudioReplaySpeech(void)
{
    if (!audio.has_speech) return;
    if (audio.voice_loaded) {
        StopMusicStream(audio.voice);
        if (audio.focused && audio.mode == 0 && audio.voice_percent > 0) {
            PlayMusicStream(audio.voice);
            UpdateMusicStream(audio.voice);
        }
        return;
    }
    char path[sizeof(audio.voice_path)];
    (void)snprintf(path, sizeof(path), "%s", audio.voice_path);
    audio.voice_path[0] = '\0';
    CcAudioVoice(path);
    audio.speech_pending = !audio.voice_loaded && audio.focused && audio.mode == 0 && audio.voice_percent > 0;
    audio.request_after = GetTime();
}

void CcAudioSkipSpeech(void)
{
    if (audio.override) audio.override_expires = GetTime();
    StopVoice();
    audio.speech_pending = false;
    CancelRequest();
}

const CcSpeech *CcAudioCurrentSpeech(void)
{
    return audio.has_speech ? &audio.speech : NULL;
}
