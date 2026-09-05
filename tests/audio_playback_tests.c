#include "client/cc_audio.h"
#include "client/cc_voice_net.h"
#include "raylib.h"
#include "test_support.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* A small device double verifies lifetime and mixing without a speaker. */
static unsigned char buffers[CC_SOUND_COUNT * 3];
static bool device_available, playing[CC_SOUND_COUNT * 3], voice_playing;
static float volumes[CC_SOUND_COUNT * 3];
static float pitches[CC_SOUND_COUNT * 3];
static int allocated, freed, device_opens, device_closes, voices_loaded, voices_freed;
static double clock_seconds = 1.0;

static bool net_busy;
static int net_result, net_starts;
bool CcVoiceNetStart(const CcSpeech *speech)
{
    CC_CHECK(speech != NULL && !net_busy);
    net_busy = true;
    ++net_starts;
    return true;
}
bool CcVoiceNetBusy(void) { return net_busy; }
void CcVoiceNetCancel(void) { /* Deliberately allow a late success. */ }
void CcVoiceNetShutdown(void) { net_busy = false; net_result = 0; }
int CcVoiceNetPoll(unsigned char **data, size_t *size)
{
    if (!net_busy || net_result == 0) return 0;
    int result = net_result;
    net_result = 0;
    net_busy = false;
    if (result > 0) { *data = calloc(44, 1); *size = 44; }
    return result;
}
Music LoadMusicStreamFromMemory(const char *type, const unsigned char *bytes, int size)
{
    CC_CHECK(strcmp(type, ".wav") == 0 && bytes != NULL && size == 44);
    ++voices_loaded;
    return (Music){.frameCount = 24000U};
}

void InitAudioDevice(void) { ++device_opens; }
bool IsAudioDeviceReady(void) { return device_available; }
void CloseAudioDevice(void) { ++device_closes; }
bool IsSoundValid(Sound sound) { return sound.frameCount > 0U; }
Sound LoadSoundFromWave(Wave wave)
{
    CC_CHECK(allocated < CC_SOUND_COUNT * 3);
    CC_CHECK(wave.data != NULL && wave.channels == 1U && wave.sampleSize == 16U);
    Sound sound = {.frameCount = wave.frameCount};
    sound.stream.buffer = (rAudioBuffer *)&buffers[allocated++];
    return sound;
}
static size_t SoundIndex(Sound sound)
{
    return (size_t)((unsigned char *)sound.stream.buffer - buffers);
}
void UnloadSound(Sound sound) { CC_CHECK(IsSoundValid(sound)); ++freed; }
void StopSound(Sound sound) { playing[SoundIndex(sound)] = false; }
void PlaySound(Sound sound) { playing[SoundIndex(sound)] = true; }
void SetSoundVolume(Sound sound, float volume) { volumes[SoundIndex(sound)] = volume; }
void SetSoundPitch(Sound sound, float pitch) { pitches[SoundIndex(sound)] = pitch; }
double GetTime(void) { return clock_seconds; }
bool FileExists(const char *path) { return strcmp(path, "missing.wav") != 0; }
Music LoadMusicStream(const char *path)
{
    CC_CHECK(path != NULL);
    ++voices_loaded;
    return (Music){.frameCount = 24000U};
}
bool IsMusicValid(Music music) { return music.frameCount > 0U; }
bool IsMusicStreamPlaying(Music music) { (void)music; return voice_playing; }
void PlayMusicStream(Music music) { CC_CHECK(!music.looping); voice_playing = true; }
void StopMusicStream(Music music) { (void)music; voice_playing = false; }
void UnloadMusicStream(Music music) { (void)music; ++voices_freed; }
void SetMusicVolume(Music music, float volume) { (void)music; CC_CHECK(volume > 0.0f && volume <= 1.0f); }
void UpdateMusicStream(Music music) { CC_CHECK(IsMusicValid(music)); }

static int PlayingCount(void)
{
    int count = 0;
    for (int i = 0; i < allocated; ++i) if (playing[i]) ++count;
    return count;
}

int main(void)
{
    CcAudioPlay(CC_SOUND_PAGE);
    CcAudioVoice("opening.wav");
    CcAudioUpdate();
    CC_CHECK(CcAudioMusicGain() == 0.0f);
    CcAudioInit();
    CcAudioInit();
    CC_CHECK(device_opens == 1 && allocated == 0 && voices_loaded == 0);
    CcAudioShutdown();
    device_available = true;
    CcAudioInit();
    CC_CHECK(allocated == CC_SOUND_COUNT * 3);
    CC_CHECK(CcAudioMusicGain() == 1.0f);
    CcAudioUpdate();
    for (int cue = CC_SOUND_STEP_STONE; cue <= CC_SOUND_SPLASH; ++cue) {
        CC_CHECK(volumes[cue * 3] == volumes[CC_SOUND_STEP_GRASS * 3]);
        CC_CHECK(volumes[cue * 3] == 0.18f);
    }
    CC_CHECK(volumes[CC_SOUND_STEP_STONE * 3] < volumes[CC_SOUND_HIT * 3] * 0.5f);
    CcAudioPlay(CC_SOUND_HOOF);
    CC_CHECK(PlayingCount() == 1);
    CC_CHECK(volumes[CC_SOUND_HOOF * 3] > 0.40f);
    CC_CHECK(volumes[CC_SOUND_WHEEL * 3] > 0.40f);
    float hoof_volume = volumes[CC_SOUND_HOOF * 3];
    CC_CHECK(pitches[CC_SOUND_HOOF * 3] == CC_SOUND_PLAYBACK_PITCH);
    CcAudioPlay(CC_SOUND_HOOF);
    CC_CHECK(PlayingCount() == 1);
    clock_seconds += 0.3;
    CcAudioPlay(CC_SOUND_HOOF);
    CC_CHECK(PlayingCount() == 2);
    CcAudioVoice("opening.wav");
    CcAudioVoice("opening.wav");
    CC_CHECK(voices_loaded == 1 && voice_playing);
    CC_CHECK(CcAudioMusicGain() == 0.36f);
    CcAudioUpdate();
    CC_CHECK(fabsf(volumes[CC_SOUND_HOOF * 3] - hoof_volume * 0.36f) < 0.0001f);
    CcAudioVoice("reply.wav");
    CC_CHECK(voices_loaded == 2 && voices_freed == 1 && voice_playing);
    CcAudioVoice(NULL);
    CcAudioUpdate();
    CC_CHECK(!voice_playing && voices_freed == 2);
    CC_CHECK(CcAudioMusicGain() == 1.0f);
    CC_CHECK(fabsf(volumes[CC_SOUND_HOOF * 3] - hoof_volume) < 0.0001f);
    CcAudioVoice("missing.wav");
    CcAudioVoice("missing.wav");
    CC_CHECK(voices_loaded == 2);
    CcAudioVoice("opening.wav");
    CcAudioSetMode(1);
    CC_CHECK(CcAudioMusicGain() == 0.0f);
    CC_CHECK(!voice_playing);
    CcAudioVoice("reply.wav");
    CC_CHECK(voices_loaded == 3);
    CcAudioSetMode(2);
    CC_CHECK(CcAudioMusicGain() == 0.0f);
    CC_CHECK(PlayingCount() == 0);
    clock_seconds += 1.0;
    CcAudioPlay(CC_SOUND_HIT);
    CC_CHECK(PlayingCount() == 0);
    CcAudioSetMode(0);
    CcAudioPlay(CC_SOUND_STEP_STONE);
    CC_CHECK(pitches[CC_SOUND_STEP_STONE * 3] == CC_SOUND_PLAYBACK_PITCH);
    CcAudioSetMode(2);
    CcAudioSetMode(0);
    CcAudioVoice(NULL);
    CcAudioVoice("reply.wav");
    CcAudioPlay(CC_SOUND_PAGE);
    CC_CHECK(voice_playing && PlayingCount() == 1);
    CcAudioSetFocused(false);
    CC_CHECK(CcAudioMusicGain() == 0.0f);
    CC_CHECK(!voice_playing && PlayingCount() == 0);
    CcAudioPlay(CC_SOUND_COINS);
    CcAudioVoice("other.wav");
    CC_CHECK(!voice_playing && PlayingCount() == 0);
    CcAudioSetFocused(true);
    CcAudioUpdate();
    CC_CHECK(!voice_playing && PlayingCount() == 0);
    CcSpeech speech = {.speaker_id = 7, .audio_key = 42};
    (void)strcpy(speech.text, "Eight boxes.");
    int previous_loads = voices_loaded;
    CcAudioSpeech(&speech, "speech.wav");
    CcAudioSpeech(&speech, "speech.wav");
    CC_CHECK(voice_playing && voices_loaded == previous_loads + 1);
    CC_CHECK(strcmp(CcAudioCurrentSpeech()->text, "Eight boxes.") == 0);
    CcAudioSkipSpeech();
    CcAudioSpeech(&speech, "speech.wav");
    CC_CHECK(!voice_playing && voices_loaded == previous_loads + 1);
    CcAudioReplaySpeech();
    CC_CHECK(voice_playing && voices_loaded == previous_loads + 2);
    speech.speaker_id = 8;
    CcAudioSpeech(&speech, "speech.wav");
    CC_CHECK(voice_playing && voices_loaded == previous_loads + 3);
    CcAudioSpeech(NULL, NULL);
    CC_CHECK(!voice_playing && CcAudioCurrentSpeech() == NULL);
    CcAudioSpeech(&speech, "speech.wav");
    CC_CHECK(voice_playing && voices_loaded == previous_loads + 4);
    CcAudioSpeech(NULL, NULL);
    previous_loads = voices_loaded;
    CcAudioSpeech(&speech, "missing.wav");
    CcAudioUpdate();
    CC_CHECK(net_starts == 0);
    clock_seconds += 0.4;
    CcAudioUpdate();
    CC_CHECK(net_starts == 1);
    CcAudioSkipSpeech();
    net_result = 1;
    CcAudioUpdate();
    CC_CHECK(voices_loaded == previous_loads && !voice_playing);
    CcAudioReplaySpeech();
    CcAudioUpdate();
    CC_CHECK(net_starts == 2);
    /* A second person can use the same voice and words. The old turn is stale. */
    speech.speaker_id++;
    CcAudioSpeech(&speech, "missing.wav");
    clock_seconds += 0.4;
    net_result = 1;
    CcAudioUpdate();
    CC_CHECK(voices_loaded == previous_loads && net_starts == 3);
    net_result = 1;
    CcAudioUpdate();
    CC_CHECK(voices_loaded == previous_loads + 1 && voice_playing);
    CcAudioReplaySpeech();
    CC_CHECK(voices_loaded == previous_loads + 1 && voice_playing);
    speech.audio_key++;
    CcAudioSpeech(&speech, "missing.wav");
    clock_seconds += 0.4;
    CcAudioUpdate();
    CcAudioSpeech(NULL, NULL);
    net_result = 1;
    CcAudioUpdate();
    CC_CHECK(voices_loaded == previous_loads + 1 && !voice_playing);
    CcAudioClearSpeech();
    CcAudioSetContext(100);
    speech.priority = CC_SPEECH_CONVERSATION;
    CcAudioSpeech(&speech, "npc.wav");
    CcSpeech reply = speech;
    reply.speaker_id = 99;
    reply.audio_key += 10;
    (void)strcpy(reply.text, "I will take the job.");
    CcAudioSay(&reply, "reply.wav");
    speech.audio_key++;
    CcAudioSpeech(&speech, "npc-next.wav");
    CC_CHECK(CcAudioCurrentSpeech()->speaker_id == 99);
    voice_playing = false;
    CcAudioUpdate();
    CC_CHECK(CcAudioCurrentSpeech()->audio_key == speech.audio_key && voice_playing);
    reply.priority = CC_SPEECH_BACKGROUND;
    CcAudioSay(&reply, "ambient.wav");
    CC_CHECK(CcAudioCurrentSpeech()->speaker_id == speech.speaker_id);
    reply.priority = CC_SPEECH_WARNING;
    CcAudioSay(&reply, "warning.wav");
    CC_CHECK(CcAudioCurrentSpeech()->speaker_id == 99);
    CcAudioSetContext(101);
    CC_CHECK(CcAudioCurrentSpeech() == NULL && !voice_playing);
    CcAudioSay(&reply, "warning.wav");
    CcAudioSetVoiceVolume(0);
    CC_CHECK(CcAudioCurrentSpeech() == NULL && !voice_playing && CcAudioMusicGain() == 1.0f);
    CcAudioSay(&reply, "warning.wav");
    CC_CHECK(CcAudioCurrentSpeech() == NULL);
    CcAudioSetVoiceVolume(50);
    reply.priority = CC_SPEECH_FEEDBACK;
    CcAudioSay(&reply, "first.wav");
    for (int i = 0; i < 7; ++i) { reply.audio_key++; CcAudioSay(&reply, "queued.wav"); }
    previous_loads = voices_loaded;
    for (int i = 0; i < 5; ++i) { voice_playing = false; CcAudioUpdate(); }
    CC_CHECK(voices_loaded == previous_loads + 4 && CcAudioCurrentSpeech() == NULL);
    reply.priority = CC_SPEECH_WARNING;
    CcAudioSay(&reply, "missing.wav");
    clock_seconds += 7.0;
    CcAudioUpdate();
    CC_CHECK(CcAudioCurrentSpeech() == NULL);
    net_result = 1;
    CcAudioUpdate();
    CC_CHECK(CcAudioCurrentSpeech() == NULL && !voice_playing);
    CcAudioShutdown();
    CC_CHECK(freed == allocated && voices_freed == voices_loaded && device_closes == 1);
    CcAudioShutdown();
    CC_CHECK(device_closes == 1);
    return 0;
}
