#ifndef CROWNLESS_MUSIC_PLAYER_H
#define CROWNLESS_MUSIC_PLAYER_H

#include "client/cc_music.h"

/* Playback starts after a play input. Fades advance in real seconds. */
void CcMusicPlayerUpdate(const CcMusicContext *context, float delta_seconds,
                         bool focused, bool play_input, float volume, uint32_t seed);
void CcMusicPlayerShutdown(void);

#endif
