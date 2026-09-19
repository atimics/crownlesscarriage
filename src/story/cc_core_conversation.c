#include "story/cc_hrakhor.h"
#include "story/cc_core_conversation.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void CcCoreConversationReset(CcCoreConversation *c)
{
    if (c == NULL) return;
    CcCoreModel *model = c->model;
    *c = (CcCoreConversation){0}; c->model = model;
}

CcCoreControl CcCoreConversationNextMove(const CcCoreConversation *c, CcId speaker)
{
    if (c == NULL || c->count == 0U) return CC_CORE_CONTROL_OPEN;
    const CcCoreSpoken *last = &c->history[c->count - 1U];
    if (last->speaker != speaker && strchr(last->text, '?') != NULL)
        return CC_CORE_CONTROL_ANSWER;
    return CC_CORE_CONTROL_REMARK;
}

/* A correction needs a differing spoken field in a recognised account.
   Matching claims invite a personal response. The next model can select
   richer intents using the participant view and its own observed turns. */
CcCoreControl CcCoreConversationReplyMove(const CcCoreConversation *c,
                                         const CcCoreAccount *own, CcId event, CcId speaker)
{
    CcCoreControl move = CcCoreConversationNextMove(c, speaker);
    if (c == NULL || c->count == 0U || own == NULL || own->field_count > CC_CORE_FIELDS || move == CC_CORE_CONTROL_ANSWER)
        return move;
    const CcCoreSpoken *last = &c->history[c->count - 1U];
    CcCoreAccount heard;
    if (event == 0U || last->source_event_id != event || last->speaker == speaker ||
        !CcCoreAccountParseKind(CcCoreAccountKind(own), last->text, &heard) ||
        heard.rule_index != own->rule_index || heard.field_count != own->field_count)
        return move;
    for (size_t i = 0; i < own->field_count; ++i) {
        const CcCoreField *a = &own->fields[i], *b = &heard.fields[i];
        if (a->start >= sizeof(own->text) || a->length >= sizeof(own->text) - a->start) return move;
        if (!a->spoken || !b->spoken || a->knowledge != CC_CORE_KNOWN ||
            a->length == 0U || b->length == 0U) continue;
        if (a->length != b->length) return CC_CORE_CONTROL_DISPUTE;
        for (size_t j = 0; j < a->length; ++j)
            if (tolower((unsigned char)own->text[a->start + j]) !=
                tolower((unsigned char)heard.text[b->start + j])) return CC_CORE_CONTROL_DISPUTE;
    }
    return move;
}

/* Words long enough to be names or content, matched case-insensitively. The
   retrieval measurement put lexical overlap at 83% top-1 against 21% for the
   budget-free embedder, so the cheap exact signal is the one to use. */
static size_t SharedWords(const char *a, const char *b)
{
    size_t score = 0U;
    for (const char *p = a; *p != '\0';) {
        if (!isalnum((unsigned char)*p)) { ++p; continue; }
        const char *start = p;
        while (*p != '\0' && isalnum((unsigned char)*p)) ++p;
        size_t length = (size_t)(p - start);
        if (length < 4U) continue;
        for (const char *q = b; *q != '\0';) {
            if (!isalnum((unsigned char)*q)) { ++q; continue; }
            const char *other = q;
            while (*q != '\0' && isalnum((unsigned char)*q)) ++q;
            if ((size_t)(q - other) != length) continue;
            size_t k = 0U;
            while (k < length &&
                   tolower((unsigned char)start[k]) == tolower((unsigned char)other[k])) ++k;
            if (k == length) { ++score; break; }
        }
    }
    return score;
}

const char *CcCoreMemoryShare(const CcCoreMind *mind, const CcCoreSpoken *history,
                              size_t count, bool willing)
{
    if (mind == NULL || !willing || (count > 0U && history == NULL)) return NULL;
    const char *best = NULL;
    size_t best_score = 0U;
    for (size_t i = 0U; i < mind->memory_count && i < CC_CORE_MIND_LINES; ++i) {
        const char *memory = mind->memories[i];
        if (memory == NULL || memory[0] == '\0') continue;
        size_t score = 0U;
        for (size_t h = 0U; h < count; ++h)
            score += SharedWords(memory, history[h].text);
        if (score > best_score) { best_score = score; best = memory; }
    }
    return best;
}

static void Remember(CcCoreConversation *c, CcId speaker, CcId event, const char *text){
    if (text == NULL || text[0] == '\0' || strlen(text) >= CC_CORE_UTTERANCE) return;
    if (c->count == CC_CORE_HISTORY) {
        memmove(c->history, c->history + 1, (CC_CORE_HISTORY - 1U) * sizeof(c->history[0]));
        --c->count;
    }
    c->history[c->count].speaker = speaker;
    c->history[c->count].source_event_id = event;
    (void)snprintf(c->history[c->count].text, sizeof(c->history[0].text), "%s", text);
    ++c->count;
}
void CcCoreConversationHear(CcCoreConversation *c, CcId speaker, const char *text)
{
    if (c == NULL) return;
    Remember(c, speaker, 0U, text); c->cached = false; c->pending = false;
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
        CcCoreConversationReplyMove(c, account, speech->source_event_id, speech->speaker_id), speech);
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
    Remember(c, c->reply.speaker_id, c->reply.source_event_id, memory_text != NULL ? memory_text : c->reply.text);
    if (c->round_phase == 1U) {
        c->player_line = c->reply;
        c->player_seconds = 2.0f + (float)strlen(c->player_line.text) / 16.0f;
        c->cached = false;
        c->round_phase = 2U;
        CcSpeech listener = c->listener_line;
        (void)CcCoreConversationPrepareMind(c, &c->listener_account,
            c->listener_has_mind ? &c->listener_mind : NULL,
            CcCoreConversationReplyMove(c, &c->listener_account, listener.source_event_id, listener.speaker_id), &listener);
        if (!c->pending) {
            Remember(c, c->reply.speaker_id, c->reply.source_event_id, c->reply.text);
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
            CcCoreConversationReplyMove(c, player_account, first.source_event_id, first.speaker_id), &first) || !c->pending) return false;
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
