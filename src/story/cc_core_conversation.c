#include "story/cc_core_conversation.h"
#include <stdio.h>
#include <string.h>

void CcCoreConversationReset(CcCoreConversation *c)
{
    if (c == NULL) return;
    CcCoreModel *model = c->model;
    *c = (CcCoreConversation){0}; c->model = model;
}

static void Remember(CcCoreConversation *c, CcId speaker, const char *text)
{
    if (text == NULL || text[0] == '\0' || strlen(text) >= CC_CORE_UTTERANCE) return;
    if (c->count == CC_CORE_HISTORY) {
        memmove(c->history, c->history + 1, (CC_CORE_HISTORY - 1U) * sizeof(c->history[0]));
        --c->count;
    }
    c->history[c->count].speaker = speaker;
    (void)snprintf(c->history[c->count].text, sizeof(c->history[0].text), "%s", text);
    ++c->count;
}
void CcCoreConversationHear(CcCoreConversation *c, CcId speaker, const char *text)
{
    if (c == NULL) return;
    Remember(c, speaker, text); c->cached = false; c->pending = false;
}

bool CcCoreConversationPrepare(CcCoreConversation *c, const CcCoreAccount *account,
                               CcSpeech *speech)
{
    if (c == NULL || c->model == NULL || account == NULL || speech == NULL) return false;
    if (!c->cached || c->original.audio_key != speech->audio_key ||
        c->original.speaker_id != speech->speaker_id || c->original.source_event_id != speech->source_event_id ||
        strcmp(c->account.text, account->text) != 0 || c->account.rule_index != account->rule_index ||
        c->account.confidence != account->confidence || c->account.retellings != account->retellings ||
        c->account.field_count != account->field_count ||
        memcmp(c->account.fields, account->fields, sizeof(account->fields)) != 0) {
        c->account = *account; c->original = *speech; c->reply = *speech; c->cached = true;
        c->pending = CcCoreModelBegin(c->model, account, speech->speaker_id, c->history, c->count);
    }
    if (c->pending) {
        return CcSpeechCompose(speech, "gossip.thinking", c->original.speaker_id,
            c->original.speaker, c->original.voice_index, "...", c->original.delivery,
            c->original.priority, c->original.source_event_id);
    }
    *speech = c->reply;
    return true;
}

void CcCoreConversationStep(CcCoreConversation *c, unsigned int budget)
{
    if (c == NULL || !c->pending) return;
    int status = CcCoreModelStep(c->model, budget);
    if (status == 0) return;
    c->pending = false;
    if (status == 1) {
        CcSpeech generated;
        if (CcSpeechCompose(&generated, "gossip.core", c->original.speaker_id,
                c->original.speaker, c->original.voice_index, CcCoreModelText(c->model),
                c->original.delivery, c->original.priority, c->original.source_event_id)) c->reply = generated;
    }
    Remember(c, c->reply.speaker_id, c->reply.text);
}
