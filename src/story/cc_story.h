#ifndef CC_STORY_H
#define CC_STORY_H

#include "sim/cc_sim.h"

#include <stddef.h>

typedef enum CcStoryBeat {
    CC_STORY_BEAT_OFFER,
    CC_STORY_BEAT_HEARD,
    CC_STORY_BEAT_PROMISED,
    CC_STORY_BEAT_HELPED,
    CC_STORY_BEAT_WITHDREW,
    CC_STORY_BEAT_RESOLVED,
    CC_STORY_BEAT_FAILED
} CcStoryBeat;

typedef enum CcStorySpeakerRole {
    CC_STORY_SPEAKER_ANY,
    CC_STORY_SPEAKER_SPONSOR,
    CC_STORY_SPEAKER_AFFECTED
} CcStorySpeakerRole;

typedef struct CcStoryLine {
    const char *id;
    const char *text;
    CcStoryBeat beat;
    CcStorySpeakerRole speaker_role;
} CcStoryLine;

const CcStoryLine *CcStoryCharacterLine(
    const CcSim *sim, const CcSituation *situation,
    const CcCharacter *character);
size_t CcStoryAuthoredLineCount(void);
const CcStoryLine *CcStoryAuthoredLineAt(size_t index);
const char *CcStoryBeatName(CcStoryBeat beat);

#endif
