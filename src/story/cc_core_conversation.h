#ifndef CC_CORE_CONVERSATION_H
#define CC_CORE_CONVERSATION_H
#include "story/cc_core_model.h"
#include "story/cc_speech.h"

typedef struct CcCoreConversation {
    CcCoreModel *model;
    CcCoreSpoken history[CC_CORE_HISTORY];
    size_t count;
    CcCoreAccount account;
    CcSpeech original, reply;
    bool cached, pending;
    /* A Chat round generates the player first, then the listener. */
    unsigned int round_phase;
    bool round_shown;
    CcSpeech player_line, listener_line;
    CcCoreAccount listener_account;
} CcCoreConversation;

void CcCoreConversationReset(CcCoreConversation *conversation);
void CcCoreConversationHear(CcCoreConversation *conversation, CcId speaker, const char *text);
/* The prepared speech supplies the speaker and source identity. */
bool CcCoreConversationPrepare(CcCoreConversation *conversation,
                               const CcCoreAccount *account, CcSpeech *speech);
bool CcCoreConversationStartRound(CcCoreConversation *conversation,
    const CcCoreAccount *player_account, const CcSpeech *player,
    const CcCoreAccount *listener_account, const CcSpeech *listener);
bool CcCoreConversationShown(const CcCoreConversation *conversation, CcSpeech *speech);
void CcCoreConversationStep(CcCoreConversation *conversation, unsigned int budget);
#endif
