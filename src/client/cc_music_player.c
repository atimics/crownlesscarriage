#include "client/cc_music_player.h"
#include "client/cc_music_library.h"
#include "client/cc_music_net.h"
#include "raylib.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static struct {
    CcMusicDirector director;
    Music stream[CC_MUSIC_VOICE_COUNT];
    int loaded_take[CC_MUSIC_VOICE_COUNT];
    char path[CC_MUSIC_TAKE_COUNT][512];
    CcMusicLibrary library;
    bool bundled[CC_MUSIC_TAKE_COUNT];
    unsigned char *data[CC_MUSIC_TAKE_COUNT];
    size_t size[CC_MUSIC_TAKE_COUNT];
    bool downloading;
    int download_take; /* -1 is the catalog. */
    bool local_download;
    float refresh;
    float retry[CC_MUSIC_TAKE_COUNT];
    bool initialized;
    bool ready;
    bool focused;
    float volume;
} player;

static void Availability(void)
{
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
        player.director.ready[i] = player.path[i][0] != '\0' || player.data[i] != NULL;
        player.director.available[i] = player.retry[i] <= 0.0f &&
            (player.bundled[i] || player.data[i] != NULL || player.library.file[i][0] != '\0');
    }
}

static void Offline(void)
{
    memset(&player.library, 0, sizeof(player.library));
    player.refresh = 60.0f;
    Availability();
}

static void RequestTake(int take, bool local)
{
    char url[256];
    if (local) {
        (void)snprintf(url, sizeof(url), "assets/audio/music/%02d-%02d.mp3",
                       cc_music_takes[take].cue + 1, cc_music_takes[take].variant);
    } else {
        (void)snprintf(url, sizeof(url), CC_MUSIC_HOST "/audio/%s", player.library.file[take]);
    }
    if (CcMusicNetStart(url, CC_MUSIC_DOWNLOAD_LIMIT)) {
        player.downloading = true;
        player.download_take = take;
        player.local_download = local;
    }
}

static void UpdateSources(float dt)
{
    player.refresh -= dt;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i)
        player.retry[i] = fmaxf(0.0f, player.retry[i] - dt);
    unsigned char *data = NULL;
    size_t size = 0;
    int result = player.downloading ? CcMusicNetPoll(&data, &size) : 0;
    if (result != 0) {
        player.downloading = false;
        int take = player.download_take;
        if (take < 0) {
            if (result > 0 && CcMusicLibraryParse(&player.library, data, size))
                player.refresh = 300.0f;
            else Offline();
            free(data);
        } else if (result > 0 && size >= 1024U) {
            free(player.data[take]);
            player.data[take] = data;
            player.size[take] = size;
        } else {
            free(data);
            if (!player.local_download) {
                Offline();
#if defined(PLATFORM_WEB)
                if (player.bundled[take]) RequestTake(take, true);
#endif
            }
            if (!player.downloading && player.path[take][0] == '\0')
                player.retry[take] = 60.0f;
        }
    }
    Availability();
}

static void RequestAudio(void)
{
    if (player.downloading) return;
    /* Keep only the active fades and the next selected take in memory. */
    for (int take = 0; take < CC_MUSIC_TAKE_COUNT; ++take) {
        if (player.data[take] == NULL || take == player.director.requested_take) continue;
        bool active = false;
        for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i)
            active = active || player.loaded_take[i] == take;
        if (!active) {
            free(player.data[take]);
            player.data[take] = NULL;
            player.size[take] = 0;
        }
    }
    Availability();
    int take = player.director.requested_take;
    if (take >= 0 && player.director.available[take] && !player.director.ready[take]) {
        bool local = player.library.file[take][0] == '\0';
        RequestTake(take, local);
    } else if (player.refresh <= 0.0f) {
        if (CcMusicNetStart(CC_MUSIC_HOST "/catalog.txt", 16384U)) {
            player.downloading = true;
            player.download_take = -1;
        } else player.refresh = 60.0f;
    }
}

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
        (void)snprintf(path, capacity, "%s/%s", GetApplicationDirectory(), relative);
        if (FileExists(path)) return true;
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
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) player.loaded_take[i] = -1;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
        player.bundled[i] = FindTrack(i, player.path[i], sizeof(player.path[i]));
#if defined(PLATFORM_WEB)
        player.bundled[i] = player.bundled[i] || CcMusicBundled(i);
#endif
    }
    Availability();
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
    UpdateSources(dt);
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
                int take = voice->take;
                Music stream = player.data[take] != NULL ?
                    LoadMusicStreamFromMemory(".mp3", player.data[take], (int)player.size[take]) :
                    LoadMusicStream(player.path[take]);
                if (!IsMusicValid(stream)) {
                    player.retry[take] = 60.0f;
                    free(player.data[take]);
                    player.data[take] = NULL;
                    player.director.available[take] = false;
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
    RequestAudio();
}

void CcMusicPlayerShutdown(void)
{
    if (!player.initialized) return;
    CcMusicNetShutdown();
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        if (player.loaded_take[i] < 0) continue;
        StopMusicStream(player.stream[i]);
        UnloadMusicStream(player.stream[i]);
    }
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) free(player.data[i]);
    memset(&player, 0, sizeof(player));
}
