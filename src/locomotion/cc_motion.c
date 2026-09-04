#include "locomotion/cc_motion.h"

#include <math.h>
#include <stddef.h>

static const CcMotionClip CLIPS[CC_MOTION_CLIP_COUNT] = {
    [CC_MOTION_CLIP_NONE] = {
        .id = CC_MOTION_CLIP_NONE, .name = "none"
    },
    [CC_MOTION_CLIP_IDLE] = {
        .id = CC_MOTION_CLIP_IDLE, .name = "idle", .duration = 2.4f,
        .loop = true
    },
    [CC_MOTION_CLIP_WALK] = {
        .id = CC_MOTION_CLIP_WALK, .name = "walk", .duration = 1.0f,
        .markers = {
            {.time = 0.0f, .events = CC_MOTION_MARKER_LEFT_CONTACT},
            {.time = 0.5f, .events = CC_MOTION_MARKER_RIGHT_CONTACT},
        },
        .marker_count = 2, .loop = true
    },
    [CC_MOTION_CLIP_GUARD] = {
        .id = CC_MOTION_CLIP_GUARD, .name = "guard", .duration = 1.8f,
        .loop = true
    },
    [CC_MOTION_CLIP_STRIKE_CUT] = {
        .id = CC_MOTION_CLIP_STRIKE_CUT, .name = "strike.cut",
        .duration = 1.20f,
        .markers = {
            {.time = 0.52f, .events = CC_MOTION_MARKER_WEAPON_ACTIVE},
            {.time = 0.61f, .events = CC_MOTION_MARKER_WEAPON_IMPACT},
            {.time = 0.72f, .events = CC_MOTION_MARKER_WEAPON_INACTIVE},
            {.time = 0.94f, .events = CC_MOTION_MARKER_RECOVERY},
        },
        .marker_count = 4
    },
    [CC_MOTION_CLIP_STRIKE_HEAVY] = {
        .id = CC_MOTION_CLIP_STRIKE_HEAVY, .name = "strike.heavy",
        .duration = 1.20f,
        .markers = {
            {.time = 0.54f, .events = CC_MOTION_MARKER_WEAPON_ACTIVE},
            {.time = 0.61f, .events = CC_MOTION_MARKER_WEAPON_IMPACT},
            {.time = 0.75f, .events = CC_MOTION_MARKER_WEAPON_INACTIVE},
            {.time = 0.98f, .events = CC_MOTION_MARKER_RECOVERY},
        },
        .marker_count = 4
    },
    [CC_MOTION_CLIP_STRIKE_SWEEP] = {
        .id = CC_MOTION_CLIP_STRIKE_SWEEP, .name = "strike.sweep",
        .duration = 1.20f,
        .markers = {
            {.time = 0.49f, .events = CC_MOTION_MARKER_WEAPON_ACTIVE},
            {.time = 0.61f, .events = CC_MOTION_MARKER_WEAPON_IMPACT},
            {.time = 0.77f, .events = CC_MOTION_MARKER_WEAPON_INACTIVE},
            {.time = 0.96f, .events = CC_MOTION_MARKER_RECOVERY},
        },
        .marker_count = 4
    },
    [CC_MOTION_CLIP_JUMP] = {
        .id = CC_MOTION_CLIP_JUMP, .name = "jump", .duration = 0.95f,
        .markers = {
            {.time = 0.08f, .events = CC_MOTION_MARKER_CONTROL},
            {.time = 0.72f, .events = CC_MOTION_MARKER_RECOVERY},
        },
        .marker_count = 2
    },
    [CC_MOTION_CLIP_CLIMB] = {
        .id = CC_MOTION_CLIP_CLIMB, .name = "mantle.high", .duration = 1.0f,
        .markers = {
            {.time = 0.13f, .events = CC_MOTION_MARKER_LEFT_HAND_CONTACT},
            {.time = 0.18f, .events = CC_MOTION_MARKER_RIGHT_HAND_CONTACT},
            {.time = 0.40f, .events = CC_MOTION_MARKER_LEFT_CONTACT},
            {.time = 0.56f, .events = CC_MOTION_MARKER_WEIGHT_TRANSFER},
            {.time = 0.78f, .events = CC_MOTION_MARKER_LEFT_CONTACT},
            {.time = 0.90f, .events = CC_MOTION_MARKER_RIGHT_CONTACT},
            {.time = 0.94f, .events = CC_MOTION_MARKER_RECOVERY},
        },
        .marker_count = 7
    },
    [CC_MOTION_CLIP_SWIM] = {
        .id = CC_MOTION_CLIP_SWIM, .name = "swim", .duration = 1.0f,
        .loop = true
    },
    [CC_MOTION_CLIP_FALL] = {
        .id = CC_MOTION_CLIP_FALL, .name = "fall", .duration = 1.0f,
        .loop = true
    },
    [CC_MOTION_CLIP_GET_UP_SUPINE] = {
        .id = CC_MOTION_CLIP_GET_UP_SUPINE, .name = "get_up.supine",
        .duration = 2.85f,
        .markers = {
            {.time = 2.42f, .events = CC_MOTION_MARKER_CONTROL}
        },
        .marker_count = 1
    },
    [CC_MOTION_CLIP_GET_UP_PRONE] = {
        .id = CC_MOTION_CLIP_GET_UP_PRONE, .name = "get_up.prone",
        .duration = 2.85f,
        .markers = {
            {.time = 2.42f, .events = CC_MOTION_MARKER_CONTROL}
        },
        .marker_count = 1
    },
    [CC_MOTION_CLIP_GET_UP_LEFT] = {
        .id = CC_MOTION_CLIP_GET_UP_LEFT, .name = "get_up.left",
        .duration = 2.85f,
        .markers = {
            {.time = 2.42f, .events = CC_MOTION_MARKER_CONTROL}
        },
        .marker_count = 1
    },
    [CC_MOTION_CLIP_GET_UP_RIGHT] = {
        .id = CC_MOTION_CLIP_GET_UP_RIGHT, .name = "get_up.right",
        .duration = 2.85f,
        .markers = {
            {.time = 2.42f, .events = CC_MOTION_MARKER_CONTROL}
        },
        .marker_count = 1
    },
};

static float Clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

const CcMotionClip *CcMotionClipGet(CcMotionClipId id)
{
    if (id < CC_MOTION_CLIP_NONE || id >= CC_MOTION_CLIP_COUNT) {
        id = CC_MOTION_CLIP_NONE;
    }
    return &CLIPS[id];
}

const char *CcMotionClipName(CcMotionClipId id)
{
    return CcMotionClipGet(id)->name;
}

void CcMotionPlayerInit(CcMotionPlayer *player, CcMotionClipId id)
{
    if (player == NULL) return;
    *player = (CcMotionPlayer){0};
    player->clip = CcMotionClipGet(id);
}

void CcMotionPlayerPlay(CcMotionPlayer *player, CcMotionClipId id,
                        bool restart)
{
    if (player == NULL) return;
    const CcMotionClip *clip = CcMotionClipGet(id);
    if (player->clip == clip && !restart) return;
    *player = (CcMotionPlayer){.clip = clip};
}

static uint32_t MarkersBetween(const CcMotionClip *clip, float start,
                               float end, bool include_start)
{
    if (clip == NULL || end < start) return 0;
    uint32_t result = 0;
    for (int32_t marker = 0; marker < clip->marker_count; ++marker) {
        float time = clip->markers[marker].time;
        bool after_start = include_start ? time >= start : time > start;
        if (after_start && time <= end) result |= clip->markers[marker].events;
    }
    return result;
}

void CcMotionPlayerAdvance(CcMotionPlayer *player, float delta_time)
{
    if (player == NULL || player->clip == NULL || player->finished ||
        !isfinite(delta_time) || delta_time <= 0.0f) {
        return;
    }
    const CcMotionClip *clip = player->clip;
    if (clip->duration <= 0.0f) {
        player->finished = !clip->loop;
        return;
    }
    player->previous_time = player->time;
    float remaining = delta_time;
    while (remaining > 0.0f) {
        float until_end = clip->duration - player->time;
        float advance = fminf(remaining, until_end);
        player->pending_markers |= MarkersBetween(
            clip, player->time, player->time + advance,
            player->time == 0.0f);
        player->time += advance;
        remaining -= advance;
        if (player->time + 0.000001f < clip->duration) break;
        if (!clip->loop) {
            player->time = clip->duration;
            player->finished = true;
            break;
        }
        player->time = 0.0f;
        player->loop_count += 1;
        if (remaining <= 0.000001f) break;
    }
}

void CcMotionPlayerSetNormalizedTime(CcMotionPlayer *player, float phase)
{
    if (player == NULL || player->clip == NULL ||
        player->clip->duration <= 0.0f) {
        return;
    }
    player->previous_time = player->time;
    player->time = Clamp(phase, 0.0f, 1.0f) * player->clip->duration;
    if (player->time >= player->previous_time) {
        player->pending_markers |= MarkersBetween(
            player->clip, player->previous_time, player->time,
            player->previous_time == 0.0f);
    }
    player->finished = !player->clip->loop && phase >= 1.0f;
}

float CcMotionPlayerNormalizedTime(const CcMotionPlayer *player)
{
    if (player == NULL || player->clip == NULL ||
        player->clip->duration <= 0.0f) {
        return 0.0f;
    }
    return Clamp(player->time / player->clip->duration, 0.0f, 1.0f);
}

uint32_t CcMotionPlayerConsumeMarkers(CcMotionPlayer *player)
{
    if (player == NULL) return 0;
    uint32_t result = player->pending_markers;
    player->pending_markers = 0;
    return result;
}

static CcMotionQuaternion QuaternionNlerp(CcMotionQuaternion a,
                                          CcMotionQuaternion b,
                                          float amount)
{
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f) {
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }
    CcMotionQuaternion result = {
        a.x + (b.x - a.x) * amount,
        a.y + (b.y - a.y) * amount,
        a.z + (b.z - a.z) * amount,
        a.w + (b.w - a.w) * amount
    };
    float length = sqrtf(result.x * result.x + result.y * result.y +
                         result.z * result.z + result.w * result.w);
    if (length <= 0.00001f) {
        return (CcMotionQuaternion){0.0f, 0.0f, 0.0f, 1.0f};
    }
    result.x /= length;
    result.y /= length;
    result.z /= length;
    result.w /= length;
    return result;
}

static CcLimbVec3 LerpVector(CcLimbVec3 a, CcLimbVec3 b, float amount)
{
    return (CcLimbVec3){
        a.x + (b.x - a.x) * amount,
        a.y + (b.y - a.y) * amount,
        a.z + (b.z - a.z) * amount
    };
}

bool CcMotionClipSampleLocalPose(const CcMotionClip *clip, float time,
                                 CcMotionTransform *result,
                                 int32_t result_capacity)
{
    if (clip == NULL || result == NULL || clip->samples == NULL ||
        clip->sample_count <= 0 || clip->bone_count <= 0 ||
        clip->bone_count > result_capacity || clip->sample_rate <= 0.0f) {
        return false;
    }
    if (clip->loop && clip->duration > 0.0f) {
        time = fmodf(fmaxf(0.0f, time), clip->duration);
    } else {
        time = Clamp(time, 0.0f, clip->duration);
    }
    float sample = time * clip->sample_rate;
    int32_t before = (int32_t)floorf(sample);
    if (before < 0) before = 0;
    if (before >= clip->sample_count) before = clip->sample_count - 1;
    int32_t after = before + 1;
    if (after >= clip->sample_count) after = clip->sample_count - 1;
    float amount = Clamp(sample - (float)before, 0.0f, 1.0f);
    for (int32_t bone = 0; bone < clip->bone_count; ++bone) {
        const CcMotionTransform *a =
            &clip->samples[before * clip->bone_count + bone];
        const CcMotionTransform *b =
            &clip->samples[after * clip->bone_count + bone];
        result[bone].translation = LerpVector(a->translation,
                                              b->translation, amount);
        result[bone].rotation = QuaternionNlerp(a->rotation, b->rotation,
                                                amount);
        result[bone].scale = LerpVector(a->scale, b->scale, amount);
    }
    return true;
}

typedef struct CcMotionMantleKey {
    float phase;
    CcMotionMantleSample sample;
} CcMotionMantleKey;


static const CcMotionMantleKey MANTLE_KEYS[] = {
    {0.00f, {.root_progress = 0.00f, .root_depth_progress = 0.00f,
             .pelvis_height = 0.90f,
             .foot_support = {1.0f, 1.0f}}},
    {0.10f, {.root_progress = 0.00f, .root_depth_progress = 0.00f,
             .root_outward = 0.025f,
             .pelvis_height = 0.81f, .chest_inward = 0.035f,
             .pelvis_lateral = 0.025f, .hand_grip = {0.72f, 0.50f},
             .foot_support = {1.0f, 1.0f}}},
    {0.18f, {.root_progress = 0.018f, .root_depth_progress = 0.005f,
             .root_outward = 0.045f,
             .pelvis_height = 0.85f, .chest_inward = 0.095f,
             .chest_drop = 0.025f, .pelvis_lateral = 0.055f,
             .pelvis_roll = -0.025f, .pelvis_yaw = -0.035f,
             .chest_roll = 0.018f, .chest_yaw = 0.045f,
             .hand_grip = {1.0f, 1.0f},
             .foot_support = {1.0f, 1.0f}}},
    {0.28f, {.root_progress = 0.025f, .root_depth_progress = 0.012f,
             .root_outward = 0.060f,
             .pelvis_height = 0.82f, .chest_inward = 0.125f,
             .chest_drop = 0.060f, .pelvis_lateral = 0.085f,
             .pelvis_roll = -0.045f, .pelvis_yaw = -0.060f,
             .chest_roll = 0.030f, .chest_yaw = 0.075f,
             .hand_grip = {1.0f, 1.0f}, .lead_wall_step = 0.18f,
             .foot_support = {0.0f, 1.0f}}},
    {0.40f, {.root_progress = 0.070f, .root_depth_progress = 0.030f,
             .root_outward = 0.135f,
             .pelvis_height = 0.86f, .chest_inward = 0.145f,
             .chest_drop = 0.045f, .pelvis_lateral = 0.035f,
             .pelvis_roll = 0.025f, .pelvis_yaw = 0.045f,
             .chest_roll = -0.018f, .chest_yaw = -0.040f,
             .hand_grip = {1.0f, 1.0f}, .lead_wall_step = 1.0f,
             .foot_support = {1.0f, 1.0f}}},
    {0.56f, {.root_progress = 0.340f, .root_depth_progress = 0.080f,
             .root_vertical = 0.020f,
             .root_outward = 0.180f, .root_lateral = -0.025f,
             .pelvis_height = 0.88f, .pelvis_outward = -0.015f,
             .pelvis_lateral = -0.075f, .chest_inward = 0.180f,
             .chest_drop = 0.035f, .chest_lateral = 0.025f,
             .pelvis_roll = 0.055f, .pelvis_yaw = 0.070f,
             .chest_roll = -0.035f, .chest_yaw = -0.070f,
             .hand_grip = {1.0f, 1.0f}, .lead_wall_step = 1.0f,
             .trail_tuck = 1.0f, .foot_support = {1.0f, 0.0f}}},
    {0.68f, {.root_progress = 0.560f, .root_depth_progress = 0.140f,
             .root_vertical = 0.050f,
             .root_outward = 0.120f, .root_lateral = -0.020f,
             .pelvis_height = 0.65f, .pelvis_outward = -0.035f,
             .pelvis_lateral = -0.060f, .chest_inward = 0.255f,
             .chest_drop = 0.105f, .chest_lateral = 0.020f,
             .pelvis_roll = 0.045f, .pelvis_yaw = 0.055f,
             .chest_roll = -0.025f, .chest_yaw = -0.050f,
             .hand_grip = {1.0f, 1.0f}, .hand_press = {0.34f, 0.20f},
             .lead_wall_step = 1.0f, .trail_tuck = 1.0f,
             .foot_support = {1.0f, 0.0f}}},
    {0.78f, {.root_progress = 0.720f, .root_depth_progress = 0.200f,
             .root_vertical = 0.040f,
             .root_outward = 0.060f, .root_lateral = -0.020f,
             .pelvis_height = 0.48f, .pelvis_outward = -0.055f,
             .pelvis_lateral = -0.045f, .chest_inward = 0.285f,
             .chest_drop = 0.125f, .chest_lateral = 0.015f,
             .pelvis_roll = 0.035f, .pelvis_yaw = 0.040f,
             .chest_roll = -0.020f, .chest_yaw = -0.035f,
             .hand_grip = {1.0f, 1.0f}, .hand_press = {1.0f, 0.82f},
             .lead_wall_step = 1.0f, .lead_top_step = 1.0f,
             .trail_tuck = 1.0f, .foot_support = {1.0f, 0.0f}}},
    {0.90f, {.root_progress = 0.900f, .root_depth_progress = 0.550f,
             .root_vertical = 0.020f,
             .root_outward = 0.020f, .root_lateral = -0.010f,
             .pelvis_height = 0.55f, .pelvis_outward = -0.025f,
             .pelvis_lateral = -0.025f, .chest_inward = 0.145f,
             .chest_drop = 0.045f, .pelvis_roll = 0.015f,
             .pelvis_yaw = 0.018f, .chest_roll = -0.010f,
             .chest_yaw = -0.015f, .hand_grip = {1.0f, 1.0f},
             .hand_press = {1.0f, 1.0f}, .lead_wall_step = 1.0f,
             .lead_top_step = 1.0f, .trail_tuck = 1.0f,
             .trail_top_step = 1.0f, .foot_support = {1.0f, 1.0f},
             .exit_weight = 0.34f}},
    {1.00f, {.root_progress = 1.00f, .root_depth_progress = 1.00f,
             .pelvis_height = 0.90f,
             .lead_wall_step = 1.0f, .lead_top_step = 1.0f,
             .trail_tuck = 1.0f, .trail_top_step = 1.0f,
             .foot_support = {1.0f, 1.0f}, .exit_weight = 1.0f}},
};

static float Smooth01(float value)
{
    value = Clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

bool CcMotionClipSampleMantle(float phase, CcMotionMantleSample *result)
{
    if (result == NULL) return false;
    phase = Clamp(phase, 0.0f, 1.0f);
    int32_t after = 1;
    const int32_t count = (int32_t)(sizeof(MANTLE_KEYS) /
                                    sizeof(MANTLE_KEYS[0]));
    while (after < count && phase > MANTLE_KEYS[after].phase) after += 1;
    if (after >= count) after = count - 1;
    int32_t before = after > 0 ? after - 1 : 0;
    float span = MANTLE_KEYS[after].phase - MANTLE_KEYS[before].phase;
    float amount = span > 0.00001f ?
        Smooth01((phase - MANTLE_KEYS[before].phase) / span) : 0.0f;
    const CcMotionMantleSample *a = &MANTLE_KEYS[before].sample;
    const CcMotionMantleSample *b = &MANTLE_KEYS[after].sample;
#define INTERPOLATE_MANTLE_FIELD(field) \
    result->field = a->field + (b->field - a->field) * amount
    INTERPOLATE_MANTLE_FIELD(root_progress);
    INTERPOLATE_MANTLE_FIELD(root_depth_progress);
    INTERPOLATE_MANTLE_FIELD(root_vertical);
    INTERPOLATE_MANTLE_FIELD(root_outward);
    INTERPOLATE_MANTLE_FIELD(root_lateral);
    INTERPOLATE_MANTLE_FIELD(pelvis_height);
    INTERPOLATE_MANTLE_FIELD(pelvis_outward);
    INTERPOLATE_MANTLE_FIELD(pelvis_lateral);
    INTERPOLATE_MANTLE_FIELD(chest_inward);
    INTERPOLATE_MANTLE_FIELD(chest_drop);
    INTERPOLATE_MANTLE_FIELD(chest_lateral);
    INTERPOLATE_MANTLE_FIELD(pelvis_roll);
    INTERPOLATE_MANTLE_FIELD(pelvis_yaw);
    INTERPOLATE_MANTLE_FIELD(chest_roll);
    INTERPOLATE_MANTLE_FIELD(chest_yaw);
    for (int32_t limb = 0; limb < 2; ++limb) {
        result->hand_grip[limb] = a->hand_grip[limb] +
            (b->hand_grip[limb] - a->hand_grip[limb]) * amount;
        result->hand_press[limb] = a->hand_press[limb] +
            (b->hand_press[limb] - a->hand_press[limb]) * amount;
        result->foot_support[limb] = a->foot_support[limb] +
            (b->foot_support[limb] - a->foot_support[limb]) * amount;
    }
    INTERPOLATE_MANTLE_FIELD(lead_wall_step);
    INTERPOLATE_MANTLE_FIELD(lead_top_step);
    INTERPOLATE_MANTLE_FIELD(trail_tuck);
    INTERPOLATE_MANTLE_FIELD(trail_top_step);
    INTERPOLATE_MANTLE_FIELD(exit_weight);
#undef INTERPOLATE_MANTLE_FIELD
    return true;
}
