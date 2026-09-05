#ifndef CC_AUDIO_H
#define CC_AUDIO_H

#include "client/cc_soundscape.h"
#include "story/cc_speech.h"

/* Call initialization on the first play input so browsers can start audio. */
void CcAudioInit(void);
void CcAudioShutdown(void);
void CcAudioSetMode(int mode); /* 0: full, 1: effects, 2: muted */
void CcAudioSetFocused(bool focused);
void CcAudioSetVoiceVolume(int percent);
void CcAudioSetContext(uint64_t context);
void CcAudioSay(const CcSpeech *speech, const char *path);
void CcAudioClearSpeech(void);
float CcAudioMusicGain(void);
void CcAudioPlay(CcSoundCue cue);
void CcAudioVoice(const char *path);
void CcAudioSpeech(const CcSpeech *speech, const char *path);
void CcAudioReplaySpeech(void);
void CcAudioSkipSpeech(void);
const CcSpeech *CcAudioCurrentSpeech(void);
void CcAudioUpdate(void);

#endif
