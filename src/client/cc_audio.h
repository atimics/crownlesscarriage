#ifndef CC_AUDIO_H
#define CC_AUDIO_H

#include "client/cc_soundscape.h"

/* Call initialization on the first play input so browsers can start audio. */
void CcAudioInit(void);
void CcAudioShutdown(void);
void CcAudioSetMode(int mode); /* 0: full, 1: effects, 2: muted */
void CcAudioSetFocused(bool focused);
float CcAudioMusicGain(void);
void CcAudioPlay(CcSoundCue cue);
void CcAudioVoice(const char *path);
void CcAudioUpdate(void);

#endif
