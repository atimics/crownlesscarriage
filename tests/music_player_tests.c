#include "client/cc_music_player.h"
#include "client/cc_music_net.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static bool local_files, fail_audio, active, catalog_request;
static int ticks, remote_plays, calm_plays, combat_plays, cancelled;
static float calm_gain;
bool FileExists(const char *path) {
    return local_files && (strstr(path, "03-03.mp3") || strstr(path, "59-01.mp3"));
}
const char *GetApplicationDirectory(void) { return "/test"; }
bool IsAudioDeviceReady(void) { return true; }
Music LoadMusicStream(const char *path) {
    int kind = strstr(path, "59-01") != NULL ? 2 : 1;
    return (Music){.kind = kind};
}
Music LoadMusicStreamFromMemory(const char *type, const unsigned char *data, int size) {
    assert(strcmp(type, ".mp3") == 0 && size == 2048 && data[0] == 42);
    return (Music){.bytes = data, .kind = 3};
}
bool IsMusicValid(Music music) { return music.kind > 0; }
float GetMusicTimeLength(Music music) { (void)music; return 180.0f; }
void StopMusicStream(Music music) { (void)music; }
void UnloadMusicStream(Music music) { if (music.bytes) assert(music.bytes[0] == 42); }
void PlayMusicStream(Music music) {
    if (music.kind == 3) remote_plays++;
    if (music.kind == 2) combat_plays++;
    if (music.kind == 1) calm_plays++;
}
void PauseMusicStream(Music music) { (void)music; }
void ResumeMusicStream(Music music) { (void)music; }
void UpdateMusicStream(Music music) { if (music.bytes) assert(music.bytes[0] == 42); }
void SetMusicVolume(Music music, float volume) { if (music.kind == 1) calm_gain = volume; }
bool CcMusicNetStart(const char *url, size_t limit) {
    assert(!active && limit > 1024);
    assert(strncmp(url, "https://crownless-music.pages.dev/", 34) == 0);
    active = true; ticks = 0; catalog_request = strstr(url, "catalog.txt") != NULL;
    return true;
}
int CcMusicNetPoll(unsigned char **data, size_t *size) {
    assert(active);
    if (++ticks < (catalog_request ? 2 : 120)) return 0;
    active = false;
    if (!catalog_request && fail_audio) return -1;
    static const char manifest[] = "CROWNLESS_MUSIC 1\n07-01-"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.mp3\n";
    *size = catalog_request ? sizeof(manifest) - 1 : 2048U;
    *data = malloc(*size);
    assert(*data);
    if (catalog_request) memcpy(*data, manifest, *size); else memset(*data, 42, *size);
    return 1;
}
void CcMusicNetShutdown(void) { if (active) cancelled++; active = false; }
static void Frames(CcMusicContext context, int count) {
    for (int i = 0; i < count; ++i) CcMusicPlayerUpdate(&context, 1.0f/60.0f, true, true, 1.0f, 17);
}
int main(void) {
    CcMusicContext travel = {.theme = {[CC_MUSIC_TRAVEL] = 1.0f}};
    CcMusicContext fields = {.theme = {[CC_MUSIC_TRAVEL] = 1.0f}, .cue = {[6] = 1.0f}, .region = {[0] = 1.0f}};
    CcMusicContext combat = {.combat = true, .theme = {[CC_MUSIC_COMBAT] = 1.0f, [CC_MUSIC_BANDIT] = 1.0f}};
    local_files = true;
    Frames(travel, 300);
    assert(calm_plays == 1 && calm_gain > 0.54f);
    Frames(fields, 1250);
    assert(active && !catalog_request && calm_gain > 0.54f);
    Frames(fields, 800);
    assert(remote_plays == 1);
    Frames(combat, 120);
    assert(combat_plays == 1);
    CcMusicPlayerShutdown();

    fail_audio = true; calm_plays = remote_plays = 0;
    Frames(travel, 300); Frames(fields, 1800);
    assert(remote_plays == 0 && calm_plays == 1 && calm_gain > 0.54f);
    CcMusicPlayerShutdown();

    local_files = false; fail_audio = false;
    Frames(fields, 600);
    assert(remote_plays == 1);
    CcMusicPlayerShutdown();
    Frames(fields, 1);
    CcMusicPlayerShutdown();
    assert(cancelled == 1);
    puts("Music player: remote loading, held fades, bundled combat, failed download and shutdown passed.");
    return 0;
}
