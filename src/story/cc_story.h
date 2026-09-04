#ifndef CC_STORY_H
#define CC_STORY_H

#include "sim/cc_sim.h"

#include <stddef.h>

typedef enum CcStoryBeat {
    CC_STORY_BEAT_OFFER,
    CC_STORY_BEAT_LEAD,
    CC_STORY_BEAT_WITNESS,
    CC_STORY_BEAT_DECISION,
    CC_STORY_BEAT_AUTHORITY,
    CC_STORY_BEAT_HEARD,
    CC_STORY_BEAT_PROMISED,
    CC_STORY_BEAT_HELPED,
    CC_STORY_BEAT_WITHDREW,
    CC_STORY_BEAT_RESOLVED,
    CC_STORY_BEAT_FAILED,
    CC_STORY_BEAT_PRESSING,
    CC_STORY_BEAT_BREAKING
} CcStoryBeat;

typedef enum CcStorySpeakerRole {
    CC_STORY_SPEAKER_ANY,
    CC_STORY_SPEAKER_SPONSOR,
    CC_STORY_SPEAKER_AFFECTED,
    CC_STORY_SPEAKER_WITNESS
} CcStorySpeakerRole;

typedef enum CcStoryPlayerChoice {
    CC_STORY_PLAYER_ASK,
    CC_STORY_PLAYER_PROMISE,
    CC_STORY_PLAYER_REPORT,
    CC_STORY_PLAYER_KEEP_CONFIDENCE,
    CC_STORY_PLAYER_LEAVE
} CcStoryPlayerChoice;

typedef struct CcStoryLine {
    const char *id;
    const char *text;
    CcStoryBeat beat;
    CcStorySpeakerRole speaker_role;
} CcStoryLine;

const CcStoryLine *CcStoryCharacterLine(
    const CcSim *sim, const CcSituation *situation,
    const CcCharacter *character);
bool CcStoryCharacterText(
    const CcSim *sim, const CcSituation *situation,
    const CcCharacter *character, char *text, size_t text_capacity);
size_t CcStoryAuthoredLineCount(void);
const CcStoryLine *CcStoryAuthoredLineAt(size_t index);
const char *CcStoryAuthoredSpeakerAt(size_t index);
const char *CcStoryBeatName(CcStoryBeat beat);
const char *CcStoryPlayerChoiceText(CcSituationKind kind,
                                    CcStoryPlayerChoice choice);
const char *CcStoryRoadCompanyLine(const CcBanditGroup *company);

#endif
