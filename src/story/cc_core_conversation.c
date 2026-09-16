#include "story/cc_hrakhor.h"
#include "story/cc_core_conversation.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void CcCoreConversationReset(CcCoreConversation *c)
{
    if (c == NULL) return;
    CcCoreModel *model = c->model;
    *c = (CcCoreConversation){0}; c->model = model;
}

/* What the corpus answers with. The four rows below are measured over the 904
   turns of crownless-moves-v2 whose preceding move can be identified: after
   hedge it settles or defers evenly (416 turns), after settle it parts every
   time (208), after answer it remarks, recalls, disputes or affirms (156), and
   after open it affirms about half the time (124). Four slots per row
   approximate those shares.

   The remaining rows are authored from what each move does, because the
   corpus's other turns are questions rather than moves and cannot be
   classified. They carry the same arc the measured ones do: an account is
   given, it is corroborated or disputed, its certainty and source are pressed,
   and then the exchange is closed and left. */
static const CcCoreControl MOVE_REPLIES[CC_CORE_CONTROL_COUNT][4] = {
    [CC_CORE_CONTROL_OPEN] = {CC_CORE_CONTROL_AFFIRM, CC_CORE_CONTROL_AFFIRM,
                              CC_CORE_CONTROL_REMARK, CC_CORE_CONTROL_DISPUTE},
    [CC_CORE_CONTROL_ANSWER] = {CC_CORE_CONTROL_REMARK, CC_CORE_CONTROL_RECALL,
                                CC_CORE_CONTROL_DISPUTE, CC_CORE_CONTROL_AFFIRM},
    [CC_CORE_CONTROL_HEDGE] = {CC_CORE_CONTROL_SETTLE, CC_CORE_CONTROL_DEFER,
                               CC_CORE_CONTROL_SETTLE, CC_CORE_CONTROL_DEFER},
    [CC_CORE_CONTROL_SETTLE] = {CC_CORE_CONTROL_PART, CC_CORE_CONTROL_PART,
                                CC_CORE_CONTROL_PART, CC_CORE_CONTROL_PART},
    [CC_CORE_CONTROL_AFFIRM] = {CC_CORE_CONTROL_HEDGE, CC_CORE_CONTROL_REMARK,
                                CC_CORE_CONTROL_HEDGE, CC_CORE_CONTROL_ATTRIBUTE},
    [CC_CORE_CONTROL_DISPUTE] = {CC_CORE_CONTROL_ATTRIBUTE, CC_CORE_CONTROL_HEDGE,
                                 CC_CORE_CONTROL_ATTRIBUTE, CC_CORE_CONTROL_HEDGE},
    [CC_CORE_CONTROL_ATTRIBUTE] = {CC_CORE_CONTROL_HEDGE, CC_CORE_CONTROL_DEFER,
                                   CC_CORE_CONTROL_HEDGE, CC_CORE_CONTROL_DEFER},
    [CC_CORE_CONTROL_DEFER] = {CC_CORE_CONTROL_SETTLE, CC_CORE_CONTROL_SETTLE,
                               CC_CORE_CONTROL_SETTLE, CC_CORE_CONTROL_SETTLE},
    [CC_CORE_CONTROL_REMARK] = {CC_CORE_CONTROL_ATTRIBUTE, CC_CORE_CONTROL_HEDGE,
                                CC_CORE_CONTROL_ATTRIBUTE, CC_CORE_CONTROL_HEDGE},
    [CC_CORE_CONTROL_RECALL] = {CC_CORE_CONTROL_REMARK, CC_CORE_CONTROL_HEDGE,
                                CC_CORE_CONTROL_REMARK, CC_CORE_CONTROL_HEDGE},
    [CC_CORE_CONTROL_MUSE] = {CC_CORE_CONTROL_OPEN, CC_CORE_CONTROL_OPEN,
                              CC_CORE_CONTROL_OPEN, CC_CORE_CONTROL_OPEN},
    /* Parting is the end of it; a speaker who has said goodbye says it again
       rather than reopening the subject. */
    [CC_CORE_CONTROL_PART] = {CC_CORE_CONTROL_PART, CC_CORE_CONTROL_PART,
                              CC_CORE_CONTROL_PART, CC_CORE_CONTROL_PART},
};

CcCoreControl CcCoreConversationNextMove(const CcCoreConversation *c, CcId speaker)
{
    if (c == NULL) return CC_CORE_CONTROL_OPEN;
    if (!c->has_last_move || c->count == 0U) return CC_CORE_CONTROL_OPEN;
    if (c->last_move < CC_CORE_CONTROL_OPEN || c->last_move >= CC_CORE_CONTROL_COUNT)
        return CC_CORE_CONTROL_ANSWER;
    /* Deterministic in the speaker and the turn, so a replayed conversation
       takes the same path without drawing on any shared generator. */
    uint64_t hash = UINT64_C(14695981039346656037);
    hash = (hash ^ (uint64_t)speaker) * UINT64_C(1099511628211);
    hash = (hash ^ (uint64_t)c->count) * UINT64_C(1099511628211);
    return MOVE_REPLIES[c->last_move][(hash >> 32) & 3U];
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

bool CcCoreConversationPrepareMind(CcCoreConversation *c, const CcCoreAccount *account,
                                   const CcCoreMind *mind, CcCoreControl control,
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
        if (mind != NULL) c->mind = *mind;
        c->pending = CcCoreModelBeginMind(c->model, account, speech->speaker_id,
            c->history, c->count, mind, control);
        c->last_move = control; c->has_last_move = true;
    }
    if (c->pending) {
        return CcSpeechCompose(speech, "gossip.thinking", c->original.speaker_id,
            c->original.speaker, c->original.voice_index, "...", c->original.delivery,
            c->original.priority, c->original.source_event_id);
    }
    *speech = c->reply;
    return true;
}

bool CcCoreConversationPrepare(CcCoreConversation *c, const CcCoreAccount *account,
                               CcSpeech *speech)
{
    return CcCoreConversationPrepareMind(c, account, NULL,
        CcCoreConversationNextMove(c, speech->speaker_id), speech);
}

void CcCoreConversationStep(CcCoreConversation *c, unsigned int budget)
{
    if (c == NULL || !c->pending) return;
    int status = CcCoreModelStep(c->model, budget);
    if (status == 0) return;
    c->pending = false;
    const char *memory_text = NULL;
    if (status == 1) {
        CcSpeech generated;
        const char *words = CcCoreModelText(c->model);
        char dialect[CC_SPEECH_TEXT_CAPACITY];
        if (c->original.voice_index == CC_SPEECH_GOBLIN_VOICE &&
            CcHrakhorCorrupt(&c->account, words, 100U, dialect, sizeof(dialect))) {
            memory_text = words;
            words = dialect;
        }
        if (CcSpeechCompose(&generated, "gossip.core", c->original.speaker_id,
                c->original.speaker, c->original.voice_index, words,
                c->original.delivery, c->original.priority, c->original.source_event_id)) c->reply = generated;
    }
    Remember(c, c->reply.speaker_id, memory_text != NULL ? memory_text : c->reply.text);
    if (c->round_phase == 1U) {
        c->player_line = c->reply;
        c->player_seconds = 2.0f + (float)strlen(c->player_line.text) / 16.0f;
        c->cached = false;
        c->round_phase = 2U;
        CcSpeech listener = c->listener_line;
        (void)CcCoreConversationPrepareMind(c, &c->listener_account,
            c->listener_has_mind ? &c->listener_mind : NULL,
            CcCoreConversationNextMove(c, listener.speaker_id), &listener);
        if (!c->pending) {
            Remember(c, c->reply.speaker_id, c->reply.text);
            c->round_phase = 3U;
        }
    } else if (c->round_phase == 2U) c->round_phase = 3U;
}

static void CopyMind(CcCoreMind *destination, const CcCoreMind *source,
                     char memory[CC_CORE_MIND_LINES][CC_CORE_UTTERANCE],
                     char thoughts[CC_CORE_MIND_LINES][CC_CORE_UTTERANCE])
{
    *destination = *source;
    destination->memory_count = source->memory_count < CC_CORE_MIND_LINES ?
        source->memory_count : CC_CORE_MIND_LINES;
    destination->thought_count = source->thought_count < CC_CORE_MIND_LINES ?
        source->thought_count : CC_CORE_MIND_LINES;
    for (size_t i = 0; i < destination->memory_count; ++i) {
        (void)snprintf(memory[i], CC_CORE_UTTERANCE, "%s",
                       source->memories[i] != NULL ? source->memories[i] : "");
        destination->memories[i] = memory[i];
    }
    for (size_t i = 0; i < destination->thought_count; ++i) {
        (void)snprintf(thoughts[i], CC_CORE_UTTERANCE, "%s",
                       source->thoughts[i] != NULL ? source->thoughts[i] : "");
        destination->thoughts[i] = thoughts[i];
    }
}

bool CcCoreConversationStartRoundMind(CcCoreConversation *c,
    const CcCoreAccount *player_account, const CcCoreMind *player_mind, const CcSpeech *player,
    const CcCoreAccount *listener_account, const CcCoreMind *listener_mind, const CcSpeech *listener)
{
    if (c == NULL || c->model == NULL || c->pending || c->round_phase != 0U ||
        player_account == NULL || player == NULL || listener_account == NULL || listener == NULL ||
        player->speaker_id == listener->speaker_id) return false;
    c->listener_account = *listener_account;
    c->listener_has_mind = listener_mind != NULL;
    if (listener_mind != NULL) CopyMind(&c->listener_mind, listener_mind,
        c->listener_memory, c->listener_thoughts);
    c->listener_line = *listener;
    c->player_line = *player;
    c->cached = false;
    CcSpeech first = *player;
    if (!CcCoreConversationPrepareMind(c, player_account, player_mind,
            CcCoreConversationNextMove(c, first.speaker_id), &first) || !c->pending) return false;
    c->round_phase = 1U;
    c->round_shown = true;
    return true;
}

bool CcCoreConversationStartRound(CcCoreConversation *c,
    const CcCoreAccount *player_account, const CcSpeech *player,
    const CcCoreAccount *listener_account, const CcSpeech *listener)
{
    return CcCoreConversationStartRoundMind(c, player_account, NULL, player,
        listener_account, NULL, listener);
}

bool CcCoreConversationShown(const CcCoreConversation *c, CcSpeech *speech)
{
    if (c == NULL || speech == NULL || !c->round_shown) return false;
    if (c->round_phase == 1U) return CcSpeechCompose(speech, "gossip.thinking",
        c->player_line.speaker_id, c->player_line.speaker, c->player_line.voice_index,
        "...", c->player_line.delivery, c->player_line.priority, c->player_line.source_event_id);
    *speech = c->round_phase >= 2U ? c->player_line : c->reply;
    return true;
}

void CcCoreConversationAdvance(CcCoreConversation *c, unsigned int budget, float seconds)
{
    if (c == NULL) return;
    CcCoreConversationStep(c, budget);
    if (c->round_phase >= 2U && seconds > 0.0f) {
        c->player_seconds -= seconds;
        if (c->round_phase == 3U && c->player_seconds <= 0.0f) c->round_phase = 0U;
    }
}
