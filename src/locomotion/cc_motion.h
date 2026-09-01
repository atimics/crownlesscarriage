#ifndef CROWNLESS_MOTION_H
#define CROWNLESS_MOTION_H

#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_MOTION_MAX_MARKERS 8

typedef enum CcMotionClipId {
    CC_MOTION_CLIP_NONE,
    CC_MOTION_CLIP_IDLE,
    CC_MOTION_CLIP_WALK,
    CC_MOTION_CLIP_GUARD,
    CC_MOTION_CLIP_STRIKE_CUT,
    CC_MOTION_CLIP_STRIKE_HEAVY,
    CC_MOTION_CLIP_STRIKE_SWEEP,
    CC_MOTION_CLIP_JUMP,
    CC_MOTION_CLIP_CLIMB,
    CC_MOTION_CLIP_SWIM,
    CC_MOTION_CLIP_FALL,
    CC_MOTION_CLIP_GET_UP_SUPINE,
    CC_MOTION_CLIP_GET_UP_PRONE,
    CC_MOTION_CLIP_GET_UP_LEFT,
    CC_MOTION_CLIP_GET_UP_RIGHT,
    CC_MOTION_CLIP_COUNT
} CcMotionClipId;

typedef enum CcMotionMarkerEvent {
    CC_MOTION_MARKER_NONE = 0,
    CC_MOTION_MARKER_LEFT_CONTACT = UINT32_C(1) << 0,
    CC_MOTION_MARKER_RIGHT_CONTACT = UINT32_C(1) << 1,
    CC_MOTION_MARKER_WEAPON_ACTIVE = UINT32_C(1) << 2,
    CC_MOTION_MARKER_WEAPON_IMPACT = UINT32_C(1) << 3,
    CC_MOTION_MARKER_WEAPON_INACTIVE = UINT32_C(1) << 4,
    CC_MOTION_MARKER_RECOVERY = UINT32_C(1) << 5,
    CC_MOTION_MARKER_CONTROL = UINT32_C(1) << 6,
    CC_MOTION_MARKER_LEFT_HAND_CONTACT = UINT32_C(1) << 7,
    CC_MOTION_MARKER_RIGHT_HAND_CONTACT = UINT32_C(1) << 8,
    CC_MOTION_MARKER_WEIGHT_TRANSFER = UINT32_C(1) << 9
} CcMotionMarkerEvent;

typedef struct CcMotionQuaternion {
    float x;
    float y;
    float z;
    float w;
} CcMotionQuaternion;


typedef struct CcMotionTransform {
    CcLimbVec3 translation;
    CcMotionQuaternion rotation;
    CcLimbVec3 scale;
} CcMotionTransform;

typedef struct CcMotionMarker {
    float time;
    uint32_t events;
} CcMotionMarker;

typedef struct CcMotionClip {
    CcMotionClipId id;
    const char *name;
    float duration;
    float sample_rate;
    int32_t bone_count;
    int32_t sample_count;
    const CcMotionTransform *samples;
    CcMotionMarker markers[CC_MOTION_MAX_MARKERS];
    int32_t marker_count;
    bool loop;
} CcMotionClip;

typedef struct CcMotionPlayer {
    const CcMotionClip *clip;
    float time;
    float previous_time;
    uint32_t pending_markers;
    uint64_t loop_count;
    bool finished;
} CcMotionPlayer;


typedef struct CcMotionMantleSample {
    float root_progress;
    float root_depth_progress;
    float root_vertical;
    float root_outward;
    float root_lateral;
    float pelvis_height;
    float pelvis_outward;
    float pelvis_lateral;
    float chest_inward;
    float chest_drop;
    float chest_lateral;
    float pelvis_roll;
    float pelvis_yaw;
    float chest_roll;
    float chest_yaw;
    float hand_grip[2];
    float hand_press[2];
    float lead_wall_step;
    float lead_top_step;
    float trail_tuck;
    float trail_top_step;
    float foot_support[2];
    float exit_weight;
} CcMotionMantleSample;

const CcMotionClip *CcMotionClipGet(CcMotionClipId id);
const char *CcMotionClipName(CcMotionClipId id);
void CcMotionPlayerInit(CcMotionPlayer *player, CcMotionClipId id);
void CcMotionPlayerPlay(CcMotionPlayer *player, CcMotionClipId id,
                        bool restart);
void CcMotionPlayerAdvance(CcMotionPlayer *player, float delta_time);
void CcMotionPlayerSetNormalizedTime(CcMotionPlayer *player, float phase);
float CcMotionPlayerNormalizedTime(const CcMotionPlayer *player);
uint32_t CcMotionPlayerConsumeMarkers(CcMotionPlayer *player);
bool CcMotionClipSampleLocalPose(const CcMotionClip *clip, float time,
                                 CcMotionTransform *result,
                                 int32_t result_capacity);
bool CcMotionClipSampleMantle(float phase, CcMotionMantleSample *result);

#endif
