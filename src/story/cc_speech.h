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
/* Realize an already-held account. Roles change wording, not evidence.
   Unsupported numerical claims get a cautious non-numeric fallback.
   False means invalid input or insufficient space, never permission to
   bypass the knowledge boundary by quoting the raw account. */
bool CcSpeechRealizeGossip(const CcSim *sim, const CcCharacter *speaker,
                           const CcGossip *story, const CcGossipVersion *version,
                           char *text, size_t capacity);
/* Shared language packet for game speech and core-model examples.
   account is the held telling; claim is a supported, quantity-free rendering. */
#define CC_GOSSIP_LANGUAGE_VERSION 1
typedef struct CcGossipLanguage {
    CcEventKind kind;
    uint32_t variant;
    int32_t confidence;
    int32_t retellings;
    char account[CC_EVENT_TEXT_CAPACITY];
    char claim[CC_SPEECH_TEXT_CAPACITY];
} CcGossipLanguage;

/* False leaves claim empty. A valid held account remains available to the game. */
bool CcSpeechPrepareGossip(const CcSim *sim, const CcGossip *story,
                            const CcGossipVersion *version, uint32_t variant,
                            CcGossipLanguage *language);
/* Plain Crownless wording for core training, with explicit hearsay. */
bool CcSpeechCoreGossip(const CcGossipLanguage *language,
                         char *text, size_t capacity);
bool CcSpeechGossip(const CcSim *sim, CcId character_id, int32_t offset,
                      bool source, CcSpeech *speech);
bool CcSpeechStory(const CcSim *sim, CcId character_id,
                   const CcGossip *story, const CcGossipVersion *version,
                   bool source, CcSpeech *speech);
bool CcSpeechRoad(const CcSim *sim, CcSpeech *speech);
bool CcSpeechPlayerChoice(const CcSim *sim, const CcSituation *situation,
                           CcStoryPlayerChoice choice, uint32_t voice_index,
                           CcSpeech *speech);
bool CcSpeechTrade(const CcSim *sim, const char *keeper, CcGood good,
                    int32_t quantity, CcMoney total, int mode,
                    const char *response, CcSpeech *speech);

#endif
