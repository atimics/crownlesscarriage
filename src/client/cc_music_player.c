#include "client/cc_music_player.h"
#include "client/cc_music_library.h"
#include "client/cc_music_net.h"
#include "client/cc_music_stream.h"
#include "raylib.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static struct {
    CcMusicDirector director;
    CcMusicStream stream[CC_MUSIC_VOICE_COUNT];
    int loaded_take[CC_MUSIC_VOICE_COUNT];
    CcMusicStream prepared;
    int prepared_take;
    char path[CC_MUSIC_TAKE_COUNT][512];
    CcMusicLibrary library;
    bool bundled[CC_MUSIC_TAKE_COUNT];
    unsigned char *data[CC_MUSIC_TAKE_COUNT];
    size_t size[CC_MUSIC_TAKE_COUNT];
    bool downloading;
    int download_take; /* -1 is the catalog. */
    bool local_download;
    bool cancelled_download;
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
        player.director.ready[i] = player.prepared_take == i && CcMusicStreamReady(player.prepared) == 1;
        for (int slot = 0; slot < CC_MUSIC_VOICE_COUNT; ++slot)
            player.director.ready[i] = player.director.ready[i] || player.loaded_take[slot] == i;
        player.director.available[i] = player.retry[i] <= 0.0f &&
            (player.bundled[i] || player.data[i] != NULL || player.library.file[i][0] != '\0');
    }
}

static void Offline(void)
{
    /* Keep the last good catalog through a brief connection failure. */
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
        if (player.cancelled_download) {
            player.cancelled_download = false;
            free(data);
        } else if (take < 0) {
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
            if (player.local_download && player.library.file[take][0] != '\0')
                RequestTake(take, false);
            if (!player.downloading && player.path[take][0] == '\0')
                player.retry[take] = 60.0f;
        }
    }
    Availability();
}

static void RequestAudio(void)
{
    int wanted = player.director.requested_take;
    bool needs_file = wanted >= 0 && player.director.available[wanted] &&
        player.path[wanted][0] == '\0' && player.data[wanted] == NULL;
    if (player.downloading && !player.cancelled_download &&
        ((player.download_take >= 0 && player.download_take != wanted) ||
         (player.download_take < 0 && needs_file))) {
        CcMusicNetCancel();
        player.cancelled_download = true;
    }
    /* Keep only the active fades and the next selected take in memory. */
    for (int take = 0; take < CC_MUSIC_TAKE_COUNT; ++take) {
        if (player.data[take] == NULL || take == wanted || take == player.prepared_take) continue;
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
    if (player.downloading) return;
    int take = player.director.requested_take;
    if (take >= 0 && player.director.available[take] && !player.director.ready[take] &&
        player.path[take][0] == '\0' && player.data[take] == NULL) {
        bool local = player.bundled[take];
        RequestTake(take, local);
    } else if (player.refresh <= 0.0f) {
        if (CcMusicNetStart(CC_MUSIC_HOST "/catalog.txt", 16384U)) {
            player.downloading = true;
            player.download_take = -1;
        } else player.refresh = 60.0f;
    }
}

static void PrepareAudio(void)
{
    int take = player.director.requested_take;
    if (player.prepared_take >= 0 && player.prepared_take != take) {
        CcMusicStreamClose(player.prepared);
        player.prepared_take = -1;
        Availability();
    }
    if (take < 0 || !player.director.available[take]) return;
    if (player.prepared_take < 0) {
        if (player.director.ready[take] ||
            (player.path[take][0] == '\0' && player.data[take] == NULL)) return;
        player.prepared = CcMusicStreamOpen(player.path[take], player.data[take], (int)player.size[take]);
        player.prepared_take = take;
    }
    int ready = CcMusicStreamReady(player.prepared);
    if (ready < 0) {
        CcMusicStreamClose(player.prepared);
        player.prepared_take = -1;
        player.retry[take] = 60.0f;
        free(player.data[take]);
        player.data[take] = NULL;
        player.size[take] = 0;
    } else if (ready > 0) {
        float duration = CcMusicStreamLength(player.prepared);
        if (isfinite(duration) && duration > 1.0f) player.director.duration[take] = duration;
    }
    Availability();
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
    player.prepared_take = -1;
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
            if (focused) CcMusicStreamResume(player.stream[i]);
            else CcMusicStreamPause(player.stream[i]);
        }
        player.focused = focused;
    }
    if (!focused) return;
    float dt = isfinite(delta_seconds) ? fminf(1.0f, fmaxf(0.0f, delta_seconds)) : 0.0f;
    /* Refill playing streams before file opening, cache copies and scene work. */
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        if (player.loaded_take[i] < 0) continue;
        if (CcMusicStreamReady(player.stream[i]) < 0)
            player.retry[player.loaded_take[i]] = 60.0f;
        CcMusicStreamUpdate(player.stream[i]);
        float position = CcMusicStreamPosition(player.stream[i]);
        if (isfinite(position) && position >= 0.0f)
            player.director.voice[i].age = position;
    }
    UpdateSources(dt);
    PrepareAudio();
    float rate = volume < player.volume ? 4.0f : 1.25f;
    player.volume += (volume - player.volume) * fminf(1.0f, dt * rate);
    CcMusicUpdate(&player.director, context, delta_seconds);
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        CcMusicVoice *voice = &player.director.voice[i];
        if (voice->take != player.loaded_take[i]) {
            if (player.loaded_take[i] >= 0) {
                CcMusicStreamClose(player.stream[i]);
                player.loaded_take[i] = -1;
            }
            if (voice->take >= 0 && voice->take == player.prepared_take) {
                player.stream[i] = player.prepared;
                player.loaded_take[i] = voice->take;
                player.prepared_take = -1;
                CcMusicStreamResume(player.stream[i]);
            }
        }
        if (player.loaded_take[i] >= 0) {
            CcMusicStreamVolume(player.stream[i], voice->gain * player.volume * 0.55f);
        }
    }
    RequestAudio();
}

void CcMusicPlayerShutdown(void)
{
    if (!player.initialized) return;
    CcMusicNetShutdown();
    if (player.prepared_take >= 0) {
        CcMusicStreamClose(player.prepared);
    }
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        if (player.loaded_take[i] < 0) continue;
        CcMusicStreamClose(player.stream[i]);
    }
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) free(player.data[i]);
    memset(&player, 0, sizeof(player));
}
