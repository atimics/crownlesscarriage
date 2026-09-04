#include "client/cc_music_player.h"
#include "raylib.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

static struct {
    CcMusicDirector director;
    Music stream[CC_MUSIC_VOICE_COUNT];
    int loaded_take[CC_MUSIC_VOICE_COUNT];
    char path[CC_MUSIC_TAKE_COUNT][512];
    bool initialized;
    bool ready;
    bool focused;
    float volume;
} player;

static bool FindTrack(int take, char *path, size_t capacity)
{
    static const char *extensions[] = {"ogg", "mp3", "wav", "flac"};
    const CcMusicTake *track = &cc_music_takes[take];
    for (int i = 0; i < 4; ++i) {
        char relative[96];
        (void)snprintf(relative, sizeof(relative), "assets/audio/music/%02d-%02d.%s",
                       track->cue + 1, track->variant, extensions[i]);
        (void)snprintf(path, capacity, "%s", relative);
        if (FileExists(path)) return true;
#if defined(CC_ASSET_SOURCE_ROOT)
        (void)snprintf(path, capacity, "%s/%s", CC_ASSET_SOURCE_ROOT, relative);
        if (FileExists(path)) return true;
#endif
        (void)snprintf(path, capacity, "%s/../Resources/%s",
                       GetApplicationDirectory(), relative);
        if (FileExists(path)) return true;
    }
    path[0] = '\0';
    return false;
}

static void Initialize(uint32_t seed)
{
    player.initialized = true;
    CcMusicInit(&player.director, seed ^ UINT32_C(0x53434f52));
    bool found = false;
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) player.loaded_take[i] = -1;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
        player.director.available[i] = FindTrack(i, player.path[i], sizeof(player.path[i]));
        found = found || player.director.available[i];
    }
    if (!found) return;
    player.ready = IsAudioDeviceReady();
    player.focused = true;
}

void CcMusicPlayerUpdate(const CcMusicContext *context, float delta_seconds,
                         bool focused, bool play_input, float volume, uint32_t seed)
{
    if (!player.initialized && play_input && IsAudioDeviceReady()) Initialize(seed);
    if (!player.ready) return;
    volume = isfinite(volume) ? fminf(1.0f, fmaxf(0.0f, volume)) : 0.0f;
    focused = focused && volume > 0.0f;
    if (player.focused != focused) {
        for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
            if (player.loaded_take[i] < 0) continue;
            if (focused) ResumeMusicStream(player.stream[i]);
            else PauseMusicStream(player.stream[i]);
        }
        player.focused = focused;
    }
    if (!focused) return;
    float dt = isfinite(delta_seconds) ? fminf(1.0f, fmaxf(0.0f, delta_seconds)) : 0.0f;
    float rate = volume < player.volume ? 4.0f : 1.25f;
    player.volume += (volume - player.volume) * fminf(1.0f, dt * rate);
    CcMusicUpdate(&player.director, context, delta_seconds);
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        CcMusicVoice *voice = &player.director.voice[i];
        if (voice->take != player.loaded_take[i]) {
            if (player.loaded_take[i] >= 0) {
                StopMusicStream(player.stream[i]);
                UnloadMusicStream(player.stream[i]);
                player.loaded_take[i] = -1;
            }
            if (voice->take >= 0 && player.director.available[voice->take]) {
                Music stream = LoadMusicStream(player.path[voice->take]);
                if (!IsMusicValid(stream)) {
                    player.director.available[voice->take] = false;
                    continue;
                }
                stream.looping = true;
                player.stream[i] = stream;
                player.loaded_take[i] = voice->take;
                float duration = GetMusicTimeLength(stream);
                if (duration > 1.0f) player.director.duration[voice->take] = duration;
                SetMusicVolume(stream, 0.0f);
                PlayMusicStream(stream);
            }
        }
        if (player.loaded_take[i] >= 0) {
            SetMusicVolume(player.stream[i], voice->gain * player.volume * 0.55f);
            UpdateMusicStream(player.stream[i]);
        }
    }
}

void CcMusicPlayerShutdown(void)
{
    if (!player.initialized) return;
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        if (player.loaded_take[i] < 0) continue;
        StopMusicStream(player.stream[i]);
        UnloadMusicStream(player.stream[i]);
    }
    memset(&player, 0, sizeof(player));
}
