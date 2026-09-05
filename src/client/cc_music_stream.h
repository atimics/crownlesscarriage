#ifndef CROWNLESS_MUSIC_STREAM_H
#define CROWNLESS_MUSIC_STREAM_H

#include <stddef.h>
#if defined(PLATFORM_WEB)
typedef int CcMusicStream;
CcMusicStream CcMusicStreamOpen(const char *path, const unsigned char *data, int size);
int CcMusicStreamReady(CcMusicStream stream);
float CcMusicStreamLength(CcMusicStream stream);
float CcMusicStreamPosition(CcMusicStream stream);
void CcMusicStreamClose(CcMusicStream stream);
void CcMusicStreamPause(CcMusicStream stream);
void CcMusicStreamResume(CcMusicStream stream);
void CcMusicStreamVolume(CcMusicStream stream, float volume);
static inline void CcMusicStreamUpdate(CcMusicStream stream) { (void)stream; }
#else
#include "raylib.h"
typedef Music CcMusicStream;
static inline CcMusicStream CcMusicStreamOpen(const char *path, const unsigned char *data, int size)
{
    /* Two one-second buffers at 48 kHz cover brief stalls in the game frame. */
    SetAudioStreamBufferSizeDefault(48000);
    Music stream = data != NULL ? LoadMusicStreamFromMemory(".mp3", data, size) :
        LoadMusicStream(path);
    SetAudioStreamBufferSizeDefault(0);
    if (IsMusicValid(stream)) {
        stream.looping = true;
        SetMusicVolume(stream, 0.0f);
        PlayMusicStream(stream);
        UpdateMusicStream(stream);
        PauseMusicStream(stream);
    }
    return stream;
}
static inline int CcMusicStreamReady(CcMusicStream stream) { return IsMusicValid(stream) ? 1 : -1; }
static inline void CcMusicStreamClose(CcMusicStream stream)
{
    if (IsMusicValid(stream)) { StopMusicStream(stream); UnloadMusicStream(stream); }
}
#define CcMusicStreamLength GetMusicTimeLength
#define CcMusicStreamPosition GetMusicTimePlayed
#define CcMusicStreamPause PauseMusicStream
#define CcMusicStreamResume ResumeMusicStream
#define CcMusicStreamVolume SetMusicVolume
#define CcMusicStreamUpdate UpdateMusicStream
#endif
#endif
