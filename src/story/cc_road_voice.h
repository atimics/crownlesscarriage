#ifndef CC_ROAD_VOICE_H
#define CC_ROAD_VOICE_H

#include "sim/cc_road_news.h"
#include "story/cc_gate_voice.h"

/* The Return, milestone 4: the words for news met on the road
   (docs/design/the-return.md). The sim records what the company met
   (sim/cc_road_news.h); this builds the one short line the travel view shows.
   Pure: it reads the simulation and writes only the voice.

   - Told: a traveller's own telling, through the gate voice builder in road
     mode: "Varkesh burned most of Gloamgate, I hear. I left before the smoke
     cleared."
   - Read: a notice at the milestone, at full confidence: "By order of the
     crown: Queen Sera rules Gloamgate."
   - Witnessed: the company's own eyes: "Black smoke hangs over Gloamgate." */

typedef struct CcRoadVoice {
    CcRoadNewsChannel channel;
    CcId town;
    CcRoadNews news;
    /* The name plate: the traveller and their trade, or the source. */
    char speaker[CC_NAME_CAPACITY];
    char speaker_label[CC_NAME_CAPACITY];
    /* Where it came from, for the log: "a notice at the milestone". */
    char source[CC_NAME_CAPACITY * 2];
    char line[CC_SPEECH_TEXT_CAPACITY];
    /* The traveller's line, clause by clause, for the told channel. */
    CcGateVoice voice;
} CcRoadVoice;

/* Build the line for one piece of road news about `town`. The change is
   found again in the town's digest by its kind, detail and subject. */
bool CcRoadVoiceBuild(const CcSim *sim, CcId town, const CcRoadNews *news,
                      CcRoadVoice *voice);
/* The line as a speech turn ("return.road") for subtitles and the voice
   seam. Only a traveller speaks; a notice and smoke are not speech. */
bool CcRoadVoiceSpeech(const CcSim *sim, const CcRoadVoice *voice, CcSpeech *speech);

#endif
