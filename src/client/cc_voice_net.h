#ifndef CC_VOICE_NET_H
#define CC_VOICE_NET_H

#include "story/cc_speech.h"

#define CC_VOICE_DOWNLOAD_LIMIT 1200100U

bool CcVoiceNetStart(const CcSpeech *speech);
bool CcVoiceNetBusy(void);
/* Poll transfers ownership of completed bytes to the caller. */
int CcVoiceNetPoll(unsigned char **data, size_t *size);
void CcVoiceNetCancel(void);
void CcVoiceNetShutdown(void);

#endif
