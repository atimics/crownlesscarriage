#ifndef CC_SPEECH_H
#define CC_SPEECH_H

#include "story/cc_story.h"

#define CC_SPEECH_TEXT_CAPACITY 512
#define CC_SPEECH_LINE_CAPACITY 96
#define CC_SPEECH_LANGUAGE "en"
#define CC_SPEECH_VERSION 1
#define CC_SPEECH_JSON_CAPACITY 4096

typedef enum CcSpeechDelivery {
    CC_SPEECH_PLAIN,
    CC_SPEECH_WARM,
    CC_SPEECH_WORRIED,
    CC_SPEECH_URGENT,
    CC_SPEECH_QUIET,
    CC_SPEECH_FIRM,
    CC_SPEECH_DELIVERY_COUNT
} CcSpeechDelivery;

typedef enum CcSpeechPriority {
    CC_SPEECH_BACKGROUND = 1,
    CC_SPEECH_FEEDBACK,
    CC_SPEECH_CONVERSATION,
    CC_SPEECH_WARNING
} CcSpeechPriority;

typedef struct CcVoiceProfile {
    const char *id;
    const char *name;
    const char *description;
} CcVoiceProfile;

/* A complete turn. Audio and subtitles share these exact words. */
typedef struct CcSpeech {
    char line_id[CC_SPEECH_LINE_CAPACITY];
    char speaker[CC_NAME_CAPACITY];
    char text[CC_SPEECH_TEXT_CAPACITY];
    CcId speaker_id;
    CcId source_event_id;
    uint32_t voice_index;
    CcSpeechDelivery delivery;
    CcSpeechPriority priority;
    uint64_t audio_key;
} CcSpeech;

size_t CcSpeechVoiceCount(void);
const CcVoiceProfile *CcSpeechVoiceAt(size_t index);
uint32_t CcSpeechCharacterVoice(const CcSim *sim, const CcCharacter *character);
uint32_t CcSpeechLocalVoice(uint32_t world_seed, CcId place, CcId object);
const char *CcSpeechDeliveryName(CcSpeechDelivery delivery);
CcSpeechDelivery CcSpeechDeliveryForBeat(CcStoryBeat beat);
bool CcSpeechCompose(CcSpeech *speech, const char *line_id, CcId speaker_id,
                      const char *speaker, uint32_t voice_index,
                      const char *text, CcSpeechDelivery delivery,
                      CcSpeechPriority priority, CcId source_event_id);
bool CcSpeechCharacter(const CcSim *sim, const CcSituation *situation,
                        const CcCharacter *character, CcSpeech *speech);
bool CcSpeechPath(const CcSpeech *speech, char *path, size_t capacity);
bool CcSpeechJson(const CcSpeech *speech, char *json, size_t capacity);
bool CcSpeechGreeting(const CcSim *sim, CcId place_id, CcId object_id,
                       const char *speaker, const char *service, CcSpeech *speech);
bool CcSpeechGossip(const CcSim *sim, CcId character_id, int32_t offset,
                      bool source, CcSpeech *speech);
bool CcSpeechRoad(const CcSim *sim, CcSpeech *speech);
bool CcSpeechPlayerChoice(const CcSim *sim, const CcSituation *situation,
                           CcStoryPlayerChoice choice, uint32_t voice_index,
                           CcSpeech *speech);
bool CcSpeechTrade(const CcSim *sim, const char *keeper, CcGood good,
                    int32_t quantity, CcMoney total, int mode,
                    const char *response, CcSpeech *speech);

#endif
