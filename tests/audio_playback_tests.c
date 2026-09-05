#include "client/cc_audio.h"
#include "raylib.h"
#include "test_support.h"

#include <math.h>
#include <string.h>

/* A small device double verifies lifetime and mixing without a speaker. */
static unsigned char buffers[CC_SOUND_COUNT * 3];
static bool device_available, playing[CC_SOUND_COUNT * 3], voice_playing;
static float volumes[CC_SOUND_COUNT * 3];
static float pitches[CC_SOUND_COUNT * 3];
static int allocated, freed, device_opens, device_closes, voices_loaded, voices_freed;
static double clock_seconds = 1.0;

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
    CC_CHECK(fabsf(volumes[CC_SOUND_HOOF * 3] - 0.32f * 0.36f) < 0.0001f);
    CcAudioVoice("reply.wav");
    CC_CHECK(voices_loaded == 2 && voices_freed == 1 && voice_playing);
    CcAudioVoice(NULL);
    CcAudioUpdate();
    CC_CHECK(!voice_playing && voices_freed == 2);
    CC_CHECK(CcAudioMusicGain() == 1.0f);
    CC_CHECK(fabsf(volumes[CC_SOUND_HOOF * 3] - 0.32f) < 0.0001f);
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
    CcAudioShutdown();
    CC_CHECK(freed == allocated && voices_freed == voices_loaded && device_closes == 1);
    CcAudioShutdown();
    CC_CHECK(device_closes == 1);
    return 0;
}
