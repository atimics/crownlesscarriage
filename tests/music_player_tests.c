#include "client/cc_music_player.h"
#include "client/cc_music_net.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); abort(); \
} } while (0)

static bool local_files, fail_audio, bad_audio, active, catalog_request, net_cancelled;
static bool town_catalog;
static int ticks, remote_plays, calm_plays, combat_plays, cancelled, replaced;
static float calm_gain;
static double now;
static int next_stream, buffer_frames;
static struct { double started, position; bool playing, primed, heard; } streams[128];
bool FileExists(const char *path) {
    return local_files && (strstr(path, "03-03.mp3") || strstr(path, "59-01.mp3"));
}
const char *GetApplicationDirectory(void) { return "/test"; }
bool IsAudioDeviceReady(void) { return true; }
Music LoadMusicStream(const char *path) {
    int kind = strstr(path, "59-01") != NULL ? 2 : 1;
    CHECK(buffer_frames == 48000);
    return (Music){.kind = kind, .id = ++next_stream};
}
Music LoadMusicStreamFromMemory(const char *type, const unsigned char *data, int size) {
    CHECK(strcmp(type, ".mp3") == 0 && size == 2048 && data[0] == 42);
    CHECK(buffer_frames == 48000);
    return bad_audio ? (Music){0} : (Music){.bytes = data, .kind = 3, .id = ++next_stream};
}
void SetAudioStreamBufferSizeDefault(int size) { buffer_frames = size; }
bool IsMusicValid(Music music) { return music.kind > 0; }
float GetMusicTimeLength(Music music) { (void)music; return 180.0f; }
void StopMusicStream(Music music) { streams[music.id].playing = false; }
void UnloadMusicStream(Music music) { if (music.bytes) CHECK(music.bytes[0] == 42); }
void PlayMusicStream(Music music) {
    streams[music.id].started = now;
    streams[music.id].playing = true;
}
float GetMusicTimePlayed(Music music) {
    double elapsed = streams[music.id].position;
    if (streams[music.id].playing) elapsed += now - streams[music.id].started;
    return (float)fmod(elapsed, 180.0);
}
void PauseMusicStream(Music music) {
    streams[music.id].position = GetMusicTimePlayed(music);
    streams[music.id].playing = false;
}
void ResumeMusicStream(Music music) {
    CHECK(streams[music.id].primed);
    streams[music.id].started = now;
    streams[music.id].playing = true;
    if (streams[music.id].heard) return;
    streams[music.id].heard = true;
    if (music.kind == 3) remote_plays++;
    if (music.kind == 2) combat_plays++;
    if (music.kind == 1) calm_plays++;
}
void UpdateMusicStream(Music music) {
    if (music.bytes) CHECK(music.bytes[0] == 42);
    streams[music.id].primed = true;
}
void SetMusicVolume(Music music, float volume) { if (music.kind == 1) calm_gain = volume; }
bool CcMusicNetStart(const char *url, size_t limit) {
    CHECK(!active && limit > 1024);
    CHECK(strncmp(url, "https://crownless-music.pages.dev/", 34) == 0);
    active = true; ticks = 0; net_cancelled = false; catalog_request = strstr(url, "catalog.txt") != NULL;
    if (town_catalog && !catalog_request) CHECK(strstr(url, "/audio/65-01-") != NULL);
    return true;
}
int CcMusicNetPoll(unsigned char **data, size_t *size) {
    CHECK(active);
    if (net_cancelled) { active = false; return -1; }
    if (++ticks < (catalog_request ? 2 : 120)) return 0;
    active = false;
    if (!catalog_request && fail_audio) return -1;
    static const char manifest[] = "CROWNLESS_MUSIC 1\n07-01-"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.mp3\n";
    static const char town_manifest[] = "CROWNLESS_MUSIC 1\n65-01-"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.mp3\n";
    const char *selected_manifest = town_catalog ? town_manifest : manifest;
    *size = catalog_request ? sizeof(manifest) - 1 : 2048U;
    *data = malloc(*size);
    CHECK(*data);
    if (catalog_request) memcpy(*data, selected_manifest, *size); else memset(*data, 42, *size);
    return 1;
}
void CcMusicNetShutdown(void) { if (active) cancelled++; active = false; }
void CcMusicNetCancel(void) { CHECK(active); net_cancelled = true; replaced++; }
static void Frames(CcMusicContext context, int count) {
    for (int i = 0; i < count; ++i) {
        now += 1.0 / 60.0;
        CcMusicPlayerUpdate(&context, 1.0f/60.0f, true, true, 1.0f, 17);
    }
}
int main(void) {
    CcMusicContext travel = {.theme = {[CC_MUSIC_TRAVEL] = 1.0f}};
    CcMusicContext fields = {.theme = {[CC_MUSIC_TRAVEL] = 1.0f}, .cue = {[6] = 1.0f}, .region = {[0] = 1.0f}};
    CcMusicContext combat = {.combat = true, .theme = {[CC_MUSIC_COMBAT] = 1.0f, [CC_MUSIC_BANDIT] = 1.0f}};
    local_files = true;
    Frames(travel, 300);
    CHECK(calm_plays == 1 && calm_gain > 0.54f);
    Frames(fields, 60);
    CHECK(active && !catalog_request && calm_gain > 0.54f);
    Frames(fields, 300);
    CHECK(remote_plays == 0 && calm_gain > 0.54f);
    Frames(fields, 1200);
    CHECK(remote_plays == 1);
    Frames(combat, 120);
    CHECK(combat_plays == 1);
    CcMusicPlayerShutdown();

    /* A fight can take over a slow prefetch on the next frame. */
    calm_plays = remote_plays = combat_plays = replaced = 0;
    Frames(travel, 300); Frames(fields, 60);
    CHECK(active && !catalog_request);
    Frames(combat, 3);
    CHECK(replaced == 1 && combat_plays == 1 && remote_plays == 0);
    CcMusicPlayerShutdown();

    /* The audio clock, including a delayed game frame, drives the ending. */
    calm_plays = remote_plays = 0;
    Frames(travel, 300); Frames(fields, 300);
    CHECK(calm_plays == 1 && remote_plays == 0);
    now += 164.0;
    Frames(fields, 2);
    CHECK(remote_plays == 1);
    CcMusicPlayerShutdown();

    /* A damaged download leaves the current song at full gain. */
    bad_audio = true; calm_plays = remote_plays = 0;
    Frames(travel, 300); Frames(fields, 1800);
    CHECK(remote_plays == 0 && calm_plays == 1 && calm_gain > 0.54f);
    CcMusicPlayerShutdown();
    bad_audio = false;

    fail_audio = true; calm_plays = remote_plays = 0;
    Frames(travel, 300); Frames(fields, 1800);
    CHECK(remote_plays == 0 && calm_plays == 1 && calm_gain > 0.54f);
    CcMusicPlayerShutdown();

    local_files = false; fail_audio = false;
    remote_plays = 0;
    Frames(fields, 600);
    CHECK(remote_plays == 1);
    CcMusicPlayerShutdown();
    Frames(fields, 1);
    CcMusicPlayerShutdown();
    CHECK(cancelled >= 1);
    /* A later catalog refresh makes a registered town export playable. */
    town_catalog = true; remote_plays = 0;
    CcMusicContext town = {.theme = {[CC_MUSIC_TOWN] = 1.0f},
        .cue = {[64] = 0.35f}, .region = {[0] = 1.0f},
        .town_mood = CC_MUSIC_MOOD_EVERYDAY};
    Frames(town, 600);
    CHECK(remote_plays == 1);
    CcMusicPlayerShutdown();
    puts("Music player: remote loading, held fades, bundled combat, failed download and shutdown passed.");
    return 0;
}
