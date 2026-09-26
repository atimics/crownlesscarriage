#include "story/cc_road_voice.h"

#include <stdio.h>
#include <string.h>

static void Copy(char *out, size_t capacity, const char *text)
{
    (void)snprintf(out, capacity, "%s", text != NULL ? text : "");
}

/* The change the news is about, from the town's digest; or, when the town
   has moved on since, the news's own fields with the town as it is now. */
static void ChangeFor(const CcSim *sim, CcId town, const CcRoadNews *news,
                      CcReturnChange *change)
{
    CcReturnDigest digest;
    if (CcReturnDigestBuild(sim, town, &digest)) {
        for (int32_t i = 0; i < digest.change_count; ++i) {
            const CcReturnChange *found = &digest.changes[i];
            if ((int32_t)found->kind == news->kind && found->detail == news->detail &&
                found->subject_id == news->subject_id) {
                *change = *found;
                return;
            }
        }
    }
    const CcSettlement *place = CcSimSettlement(sim, town);
    *change = (CcReturnChange){
        .kind = (CcReturnChangeKind)news->kind,
        .detail = news->detail,
        .subject_id = news->subject_id,
        .evidence_event_id = news->event_id,
        .after = place != NULL && news->kind == CC_RETURN_CHANGE_FIRE ?
            place->fire_damage : 0,
    };
    const CcCharacter *person = CcSimCharacter(sim, news->subject_id);
    if (person != NULL) Copy(change->subject_name, sizeof(change->subject_name), person->name);
}

static const char *BandNear(const CcSim *sim, CcId town)
{
    for (int32_t i = 0; i < sim->bandit_count && i < CC_MAX_BANDITS; ++i)
        if (sim->bandits[i].members > 0 && sim->bandits[i].camp_settlement_id == town &&
            sim->bandits[i].name[0] != '\0') return sim->bandits[i].name;
    return NULL;
}

/* The words on the notice. Every value comes from the change, which compares
   the town now with the company's last view, or from the band that camps
   there now. */
static bool NoticeText(const CcSim *sim, const CcReturnChange *change,
                       const CcSettlement *place, char *text, size_t capacity)
{
    const char *town = place->name;
    const char *band = NULL;
    switch (change->kind) {
    case CC_RETURN_CHANGE_NEW_RULER:
        if (change->subject_name[0] == '\0') return false;
        (void)snprintf(text, capacity, "By order of the crown: %s rules %s.",
                       change->subject_name, town);
        return true;
    case CC_RETURN_CHANGE_NEW_KINGDOM:
        if (change->subject_name[0] == '\0') return false;
        (void)snprintf(text, capacity, "Let it be known: %s answers to %s now.",
                       town, change->subject_name);
        return true;
    case CC_RETURN_CHANGE_HUNGER:
        (void)snprintf(text, capacity,
                       "Famine in %s. Grain and bread are wanted at the gate.", town);
        return true;
    case CC_RETURN_CHANGE_BANDIT_CAMP:
        band = BandNear(sim, place->id);
        (void)snprintf(text, capacity,
                       "Bounty: %s have made camp near %s. Word of them is paid for at the gate.",
                       band != NULL ? band : "bandits", town);
        return true;
    default:
        return false;
    }
}

bool CcRoadVoiceBuild(const CcSim *sim, CcId town, const CcRoadNews *news,
                      CcRoadVoice *voice)
{
    if (voice == NULL) return false;
    *voice = (CcRoadVoice){0};
    const CcSettlement *place = CcSimSettlement(sim, town);
    if (sim == NULL || news == NULL || place == NULL ||
        news->channel == CC_ROAD_NEWS_NONE) return false;
    voice->channel = (CcRoadNewsChannel)news->channel;
    voice->town = town;
    voice->news = *news;
    CcReturnChange change;
    ChangeFor(sim, town, news, &change);
    switch (voice->channel) {
    case CC_ROAD_NEWS_TOLD: {
        if (!CcGateVoiceSayOnRoad(sim, news->source_id, town, &change, &voice->voice))
            return false;
        Copy(voice->speaker, sizeof(voice->speaker), voice->voice.speaker);
        Copy(voice->speaker_label, sizeof(voice->speaker_label),
             voice->voice.speaker_label[0] != '\0' &&
             strcmp(voice->voice.speaker_label, "resident") != 0 ?
                 voice->voice.speaker_label : "on the road");
        const CcCharacter *traveller = CcSimCharacter(sim, news->source_id);
        const CcSettlement *from = traveller != NULL ?
            CcSimSettlement(sim, traveller->current_settlement_id) : NULL;
        if (from != NULL)
            (void)snprintf(voice->source, sizeof(voice->source),
                           "a traveller on the road from %s", from->name);
        else
            Copy(voice->source, sizeof(voice->source), "a traveller on the road");
        Copy(voice->line, sizeof(voice->line), voice->voice.line);
        return voice->line[0] != '\0';
    }
    case CC_ROAD_NEWS_READ: {
        char notice[CC_SPEECH_TEXT_CAPACITY];
        if (!NoticeText(sim, &change, place, notice, sizeof(notice))) return false;
        Copy(voice->speaker, sizeof(voice->speaker), "A notice at the milestone");
        Copy(voice->speaker_label, sizeof(voice->speaker_label), "");
        Copy(voice->source, sizeof(voice->source), "a notice at the milestone");
        (void)snprintf(voice->line, sizeof(voice->line), "\"%s\"", notice);
        return true;
    }
    case CC_ROAD_NEWS_WITNESSED:
        if (change.kind != CC_RETURN_CHANGE_FIRE) return false;
        Copy(voice->speaker, sizeof(voice->speaker), "From the bench");
        Copy(voice->speaker_label, sizeof(voice->speaker_label), "");
        Copy(voice->source, sizeof(voice->source), "the company's own eyes");
        (void)snprintf(voice->line, sizeof(voice->line),
                       "Black smoke hangs over %s, ahead. Something there has burned.",
                       place->name);
        return true;
    default:
        return false;
    }
}

bool CcRoadVoiceSpeech(const CcSim *sim, const CcRoadVoice *voice, CcSpeech *speech)
{
    if (voice == NULL || voice->channel != CC_ROAD_NEWS_TOLD) return false;
    return CcGateVoiceSpeech(sim, &voice->voice, speech);
}
