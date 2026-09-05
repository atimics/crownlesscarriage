#ifndef CROWNLESS_MUSIC_H
#define CROWNLESS_MUSIC_H

#include "sim/cc_sim.h"

#define CC_MUSIC_CUE_COUNT 64
#define CC_MUSIC_TAKE_COUNT 149
#define CC_MUSIC_VOICE_COUNT 3
#define CC_MUSIC_REGION_COUNT 6

typedef enum CcMusicTheme {
    CC_MUSIC_NONE, CC_MUSIC_TRAVEL, CC_MUSIC_TOWN, CC_MUSIC_MARKET,
    CC_MUSIC_FARM, CC_MUSIC_MILL, CC_MUSIC_MINE, CC_MUSIC_WOOD,
    CC_MUSIC_QUARRY, CC_MUSIC_FORGE, CC_MUSIC_BAKERY, CC_MUSIC_CLOISTER,
    CC_MUSIC_ARCHIVE, CC_MUSIC_INN, CC_MUSIC_CAMP, CC_MUSIC_NIGHT,
    CC_MUSIC_RAIN, CC_MUSIC_DUNGEON, CC_MUSIC_FLOOD, CC_MUSIC_BRIDGE,
    CC_MUSIC_GOBLIN, CC_MUSIC_DRAGON, CC_MUSIC_DANGER, CC_MUSIC_BANDIT,
    CC_MUSIC_COMBAT, CC_MUSIC_SIEGE, CC_MUSIC_HUNGER, CC_MUSIC_RELIEF,
    CC_MUSIC_LOSS, CC_MUSIC_COURT, CC_MUSIC_ENDING, CC_MUSIC_THEME_COUNT
} CcMusicTheme;

typedef struct CcMusicContext {
    float theme[CC_MUSIC_THEME_COUNT];
    float region[CC_MUSIC_REGION_COUNT]; /* CcSettlementFunction order. */
    float cue[CC_MUSIC_CUE_COUNT]; /* Named place or room attraction. */
    bool combat;
} CcMusicContext;

typedef struct CcMusicScene {
    bool road;
    bool market;
    bool bandit_attack;
    bool town_attack;
    bool goblin_cave;
    bool dragon_cave;
    bool loss;
    bool rain;
    bool night;
    CcMusicTheme nearby_theme;
    const char *nearby_name;
    float nearby_weight;
} CcMusicScene;

typedef struct CcMusicCue {
    const char *title;
    const char *place;
    CcMusicTheme theme;
    CcMusicTheme secondary;
    int region; /* -1 is shared between regions. */
    bool combat;
} CcMusicCue;

typedef struct CcMusicTake {
    int cue; /* Zero based track number. */
    int variant;
    float duration;
    float weight;
} CcMusicTake;

typedef struct CcMusicVoice {
    int take; /* -1 is an empty voice. */
    float age;
    float gain;
    float from_power;
    float target_power;
} CcMusicVoice;

typedef struct CcMusicDirector {
    CcMusicVoice voice[CC_MUSIC_VOICE_COUNT];
    bool available[CC_MUSIC_TAKE_COUNT];
    bool ready[CC_MUSIC_TAKE_COUNT]; /* Local file or completed download. */
    int requested_take; /* Next choice waiting for audio, or -1. */
    float duration[CC_MUSIC_TAKE_COUNT];
    int recent_cue[4];
    int last_take[CC_MUSIC_CUE_COUNT];
    uint32_t random_state;
    int target_voice;
    float fade_age;
    float fade_seconds;
    float combat_release;
    CcMusicContext combat_context;
} CcMusicDirector;

extern const CcMusicCue cc_music_cues[CC_MUSIC_CUE_COUNT];
extern const CcMusicTake cc_music_takes[CC_MUSIC_TAKE_COUNT];

/* These functions read simulation state and use a separate shuffle seed. */
CcMusicContext CcMusicContextFor(const CcSim *sim, CcMusicScene scene);
void CcMusicAttractPlace(CcMusicContext *context, const char *name, float weight);
float CcMusicScore(const CcMusicContext *context, int cue);
void CcMusicInit(CcMusicDirector *director, uint32_t seed);
int CcMusicChoose(CcMusicDirector *director, const CcMusicContext *context);
void CcMusicUpdate(CcMusicDirector *director, const CcMusicContext *context,
                   float delta_seconds);

#endif
