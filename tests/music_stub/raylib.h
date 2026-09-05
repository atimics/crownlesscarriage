#ifndef MUSIC_TEST_RAYLIB_H
#define MUSIC_TEST_RAYLIB_H
#include <stdbool.h>
typedef struct Music { const unsigned char *bytes; int kind; bool looping; } Music;
bool FileExists(const char *path);
const char *GetApplicationDirectory(void);
bool IsAudioDeviceReady(void);
Music LoadMusicStream(const char *path);
Music LoadMusicStreamFromMemory(const char *type, const unsigned char *data, int size);
bool IsMusicValid(Music music);
float GetMusicTimeLength(Music music);
void StopMusicStream(Music music);
void UnloadMusicStream(Music music);
void PlayMusicStream(Music music);
void PauseMusicStream(Music music);
void ResumeMusicStream(Music music);
void UpdateMusicStream(Music music);
void SetMusicVolume(Music music, float volume);
#endif
