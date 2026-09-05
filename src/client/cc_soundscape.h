#ifndef CC_SOUNDSCAPE_H
#define CC_SOUNDSCAPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CC_SOUND_SAMPLE_RATE 22050U

typedef enum CcSoundCue {
    CC_SOUND_STEP_STONE,
    CC_SOUND_STEP_WOOD,
    CC_SOUND_STEP_DIRT,
    CC_SOUND_STEP_GRASS,
    CC_SOUND_SPLASH,
    CC_SOUND_HOOF,
    CC_SOUND_WHEEL,
    CC_SOUND_JUMP,
    CC_SOUND_LAND,
    CC_SOUND_SWING,
    CC_SOUND_HIT,
    CC_SOUND_BLOCK,
    CC_SOUND_PAGE,
    CC_SOUND_COINS,
    CC_SOUND_PROMISE,
    CC_SOUND_COUNT
} CcSoundCue;

typedef struct CcSoundFrame {
    float x, z;
    float travel_pace;
    float strike_time;
    float impact_time;
    uint64_t place;
    int scene;
    CcSoundCue foot_surface[2];
    bool footfall[2]; /* Heel-strike events consumed from the walk cycle. */
    bool walking, grounded, swimming, jumping, striking, blocked;
} CcSoundFrame;

typedef struct CcSoundscape {
    CcSoundFrame previous;
    float swim_distance, hoof_time, wheel_time;
    bool initialized;
} CcSoundscape;

/* Presentation reads a frame snapshot and keeps its own clocks and random seed. */
uint32_t CcSoundscapeStep(CcSoundscape *state, CcSoundFrame frame, float dt);
size_t CcSoundSampleCount(CcSoundCue cue);
bool CcSoundSynthesize(CcSoundCue cue, uint32_t variation,
                       int16_t *samples, size_t capacity);
bool CcSoundVoicePath(const char *id, const char *speaker, const char *text,
                      char *path, size_t capacity);

#endif
