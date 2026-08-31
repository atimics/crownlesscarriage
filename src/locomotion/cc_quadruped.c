#include "locomotion/cc_quadruped.h"

#include "locomotion/cc_creature.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct CcQuadrupedBoneDefinition {
    const char *name;
    int32_t parent;
} CcQuadrupedBoneDefinition;

typedef struct CcQuadrupedProfile {
    float body_height;
    float half_width;
    float body_back;
    float body_front;
    float chest_back;
    float chest_front;
    float neck_start_height;
    float neck_start_forward;
    float neck_end_height;
    float neck_end_forward;
    float head_height;
    float head_forward;
    float muzzle_drop;
    float muzzle_forward;
    float front_leg_forward;
    float hind_leg_back;
    CcLimbVec3 tail_base;
    CcLimbVec3 tail_mid;
    CcLimbVec3 tail_end;
} CcQuadrupedProfile;

static const CcQuadrupedBoneDefinition BONE_DEFINITIONS[] = {
    {"root", -1},
    {"body", CC_QUADRUPED_ROOT},
    {"chest", CC_QUADRUPED_BODY},
    {"neck", CC_QUADRUPED_CHEST},
    {"head", CC_QUADRUPED_NECK},
    {"upper_leg.FL", CC_QUADRUPED_CHEST},
    {"lower_leg.FL", CC_QUADRUPED_UPPER_LEG_FL},
    {"hoof.FL", CC_QUADRUPED_LOWER_LEG_FL},
    {"upper_leg.FR", CC_QUADRUPED_CHEST},
    {"lower_leg.FR", CC_QUADRUPED_UPPER_LEG_FR},
    {"hoof.FR", CC_QUADRUPED_LOWER_LEG_FR},
    {"upper_leg.HL", CC_QUADRUPED_BODY},
    {"lower_leg.HL", CC_QUADRUPED_UPPER_LEG_HL},
    {"hoof.HL", CC_QUADRUPED_LOWER_LEG_HL},
    {"upper_leg.HR", CC_QUADRUPED_BODY},
    {"lower_leg.HR", CC_QUADRUPED_UPPER_LEG_HR},
    {"hoof.HR", CC_QUADRUPED_LOWER_LEG_HR},
    {"tail.root", CC_QUADRUPED_BODY},
    {"tail", CC_QUADRUPED_TAIL_ROOT},
};

static const CcQuadrupedProfile PROFILES[] = {
    [CC_QUADRUPED_HORSE] = {
        0.98f, 0.34f,
        -0.36f, 0.28f, 0.14f, 0.60f,
        1.20f, 0.42f, 1.41f, 0.70f,
        1.41f, 0.93f, 0.08f, 1.18f,
        0.52f, -0.52f,
        {0.0f, 1.10f, -0.68f},
        {0.0f, 0.96f, -0.94f},
        {0.0f, 0.50f, -1.17f},
    },
    [CC_QUADRUPED_COW] = {
        1.08f, 0.38f,
        -0.42f, 0.30f, 0.16f, 0.72f,
        1.28f, 0.50f, 1.16f, 0.91f,
        1.10f, 1.15f, 0.06f, 1.46f,
        0.57f, -0.57f,
        {0.0f, 1.16f, -0.82f},
        {0.0f, 1.00f, -1.10f},
        {0.0f, 0.46f, -1.28f},
    },
    [CC_QUADRUPED_SHEEP] = {
        0.76f, 0.30f,
        -0.34f, 0.24f, 0.12f, 0.52f,
        0.90f, 0.36f, 0.84f, 0.62f,
        0.80f, 0.82f, 0.07f, 1.10f,
        0.43f, -0.43f,
        {0.0f, 0.90f, -0.60f},
        {0.0f, 0.86f, -0.75f},
        {0.0f, 0.74f, -0.86f},
    },
};

static CcLimbVec3 Subtract(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CcLimbVec3 Add(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static CcLimbVec3 NormalizeOr(CcLimbVec3 value, CcLimbVec3 fallback)
{
    float length = sqrtf(value.x * value.x + value.y * value.y +
                         value.z * value.z);
    if (length > 0.00001f && isfinite(length)) {
        float inverse = 1.0f / length;
        return (CcLimbVec3){value.x * inverse, value.y * inverse,
                            value.z * inverse};
    }
    return fallback;
}

static void ResolveBone(CcQuadrupedPose *pose, CcQuadrupedBone bone,
                        CcLimbVec3 head, CcLimbVec3 tail)
{
    CcQuadrupedBonePose *result = &pose->bones[bone];
    result->head = head;
    result->tail = tail;
    result->up = NormalizeOr(Subtract(tail, head),
                             (CcLimbVec3){0.0f, 1.0f, 0.0f});
    result->parent = CcQuadrupedBoneParent(bone);
}

static void ResolveLeg(CcQuadrupedPose *pose, int32_t leg,
                       const CcQuadrupedProfile *profile, float body_height,
                       const CcCreatureRigPose *rig_rest,
                       const CcCreatureRigPose *rig_target)
{
    static const CcQuadrupedBone upper_bones[] = {
        CC_QUADRUPED_UPPER_LEG_FL, CC_QUADRUPED_UPPER_LEG_FR,
        CC_QUADRUPED_UPPER_LEG_HL, CC_QUADRUPED_UPPER_LEG_HR,
    };
    static const CcQuadrupedBone lower_bones[] = {
        CC_QUADRUPED_LOWER_LEG_FL, CC_QUADRUPED_LOWER_LEG_FR,
        CC_QUADRUPED_LOWER_LEG_HL, CC_QUADRUPED_LOWER_LEG_HR,
    };
    static const CcQuadrupedBone hoof_bones[] = {
        CC_QUADRUPED_HOOF_FL, CC_QUADRUPED_HOOF_FR,
        CC_QUADRUPED_HOOF_HL, CC_QUADRUPED_HOOF_HR,
    };
    bool front = leg < 2;
    bool left = (leg & 1) == 0;
    float x = left ? -profile->half_width : profile->half_width;
    float root_z = front ? profile->front_leg_forward :
                           profile->hind_leg_back;
    CcLimbVec3 root = {x, body_height - (front ? 0.08f : 0.10f), root_z};
    CcLimbVec3 hoof = {x, 0.10f, root_z};
    float bend = front ? 0.10f : -0.10f;
    CcLimbVec3 knee = {
        0.5f * (root.x + hoof.x),
        0.5f * (root.y + hoof.y) + 0.02f,
        0.5f * (root.z + hoof.z) + bend,
    };
    if (rig_rest != NULL && rig_target != NULL) {
        root = Add(root, Subtract(rig_target->limbs[leg].joints[0],
                                  rig_rest->limbs[leg].joints[0]));
        knee = Add(knee, Subtract(rig_target->limbs[leg].joints[1],
                                  rig_rest->limbs[leg].joints[1]));
        hoof = Add(hoof, Subtract(rig_target->limbs[leg].joints[2],
                                  rig_rest->limbs[leg].joints[2]));
    }
    CcLimbVec3 hoof_tail = {hoof.x, hoof.y - 0.015f,
                            hoof.z + 0.20f};
    ResolveBone(pose, upper_bones[leg], root, knee);
    ResolveBone(pose, lower_bones[leg], knee, hoof);
    ResolveBone(pose, hoof_bones[leg], hoof, hoof_tail);
}

const char *CcQuadrupedBoneName(CcQuadrupedBone bone)
{
    if (bone < 0 || bone >= CC_QUADRUPED_BONE_COUNT) return "invalid";
    return BONE_DEFINITIONS[bone].name;
}

int32_t CcQuadrupedBoneParent(CcQuadrupedBone bone)
{
    if (bone < 0 || bone >= CC_QUADRUPED_BONE_COUNT) return -1;
    return BONE_DEFINITIONS[bone].parent;
}

int32_t CcQuadrupedBoneFind(const char *name)
{
    if (name == NULL) return -1;
    for (int32_t bone = 0; bone < CC_QUADRUPED_BONE_COUNT; ++bone) {
        if (strcmp(name, BONE_DEFINITIONS[bone].name) == 0) return bone;
    }
    return -1;
}

static void ResolveFromRig(CcQuadrupedMorphology morphology,
                           const CcCreatureRigPose *rig_target,
                           CcQuadrupedPose *result)
{
    if (result == NULL) return;
    (void)memset(result, 0, sizeof(*result));
    if (morphology < 0 || morphology >= CC_QUADRUPED_MORPHOLOGY_COUNT ||
        rig_target == NULL || !rig_target->valid ||
        rig_target->limb_count != 4) {
        return;
    }

    const CcQuadrupedProfile *profile = &PROFILES[morphology];
    CcCreatureRigProfile rig_profile = morphology == CC_QUADRUPED_HORSE ?
        CC_CREATURE_RIG_HORSE : CC_CREATURE_RIG_COW;
    if (rig_target->profile != rig_profile) return;
    float movement = fmaxf(0.0f, fminf(rig_target->movement, 1.0f));
    float radians = rig_target->phase * 2.0f * 3.14159265358979323846f;
    float held_phase = floorf(rig_target->phase * 8.0f) / 8.0f;
    float held_radians = held_phase * 2.0f * 3.14159265358979323846f;
    float secondary_strength = 0.48f + movement * 0.52f;
    float head_sway_scale = morphology == CC_QUADRUPED_HORSE ? 0.065f :
                            morphology == CC_QUADRUPED_SHEEP ? 0.060f :
                                                               0.035f;
    float head_sway = sinf(held_radians - 0.70f) * head_sway_scale *
                      secondary_strength;
    float head_nod = sinf(held_radians - 1.10f) *
                     (morphology == CC_QUADRUPED_HORSE ? 0.035f :
                      morphology == CC_QUADRUPED_SHEEP ? 0.030f : 0.020f) *
                     secondary_strength;
    float breath = sinf(held_radians * 0.5f) * 0.012f * (1.0f - movement);
    float bob = -0.030f * fabsf(sinf(radians * 2.0f)) * movement;
    float pitch_scale = morphology == CC_QUADRUPED_HORSE ? 0.045f : 0.024f;
    float pitch = pitch_scale * sinf(radians) * movement;
    float prance_lift = morphology == CC_QUADRUPED_HORSE ?
        0.045f * (0.5f + 0.5f * cosf(radians * 2.0f)) * movement : 0.0f;
    float body_height = profile->body_height + bob + breath + prance_lift;
    CcCreatureRigPose rig_rest = {0};
    bool rig_ready = CcCreatureRigPoseResolve(
        rig_profile, 0.0f, 0.0f, (CcLimbVec3){0}, 0.0f, 1.0f, &rig_rest);
    ResolveBone(result, CC_QUADRUPED_ROOT,
                (CcLimbVec3){0.0f, bob, 0.0f},
                (CcLimbVec3){0.0f, bob + 0.20f, 0.0f});
    ResolveBone(result, CC_QUADRUPED_BODY,
                (CcLimbVec3){0.0f, body_height - pitch * 0.42f,
                             profile->body_back},
                (CcLimbVec3){0.0f, body_height + pitch * 0.30f,
                             profile->body_front});
    ResolveBone(result, CC_QUADRUPED_CHEST,
                (CcLimbVec3){0.0f, body_height + 0.06f + pitch * 0.16f,
                             profile->chest_back},
                (CcLimbVec3){0.0f, body_height + 0.06f + pitch * 0.72f,
                             profile->chest_front});
    ResolveBone(result, CC_QUADRUPED_NECK,
                (CcLimbVec3){0.0f,
                             profile->neck_start_height + bob + breath +
                                 pitch * 0.50f,
                             profile->neck_start_forward},
                (CcLimbVec3){head_sway * 0.30f,
                             profile->neck_end_height + bob + breath +
                                 head_nod - pitch * 0.35f,
                             profile->neck_end_forward});
    ResolveBone(result, CC_QUADRUPED_HEAD,
                (CcLimbVec3){head_sway * 0.30f,
                             profile->head_height + bob + breath + head_nod -
                                 pitch * 0.30f,
                             profile->head_forward},
                (CcLimbVec3){head_sway,
                             profile->head_height - profile->muzzle_drop +
                                 bob + breath + head_nod - pitch * 0.24f,
                             profile->muzzle_forward});

    for (int32_t leg = 0; leg < 4; ++leg) {
        ResolveLeg(result, leg, profile, body_height,
                   rig_ready ? &rig_rest : NULL,
                   rig_ready ? rig_target : NULL);
    }

    float tail_amplitude = morphology == CC_QUADRUPED_HORSE ? 0.27f :
                           morphology == CC_QUADRUPED_SHEEP ? 0.10f : 0.13f;
    float tail_sway = tail_amplitude * sinf(held_radians + 0.65f) *
                      secondary_strength;
    CcLimbVec3 tail_base = profile->tail_base;
    CcLimbVec3 tail_mid = profile->tail_mid;
    CcLimbVec3 tail_end = profile->tail_end;
    tail_base.y += bob + breath;
    tail_mid.x += tail_sway * 0.35f;
    tail_mid.y += bob + breath;
    tail_end.x += tail_sway;
    tail_end.y += bob + breath;
    ResolveBone(result, CC_QUADRUPED_TAIL_ROOT, tail_base, tail_mid);
    ResolveBone(result, CC_QUADRUPED_TAIL, tail_mid, tail_end);
    result->valid = true;
}

void CcQuadrupedPoseResolveFromRig(CcQuadrupedMorphology morphology,
                                   const CcCreatureRigPose *rig,
                                   CcQuadrupedPose *result)
{
    ResolveFromRig(morphology, rig, result);
}

void CcQuadrupedPoseResolve(CcQuadrupedMorphology morphology, float phase,
                            bool moving, CcQuadrupedPose *result)
{
    if (result == NULL) return;
    (void)memset(result, 0, sizeof(*result));
    if (morphology < 0 || morphology >= CC_QUADRUPED_MORPHOLOGY_COUNT ||
        !isfinite(phase)) {
        return;
    }
    CcCreatureRigProfile rig_profile = morphology == CC_QUADRUPED_HORSE ?
        CC_CREATURE_RIG_HORSE : CC_CREATURE_RIG_COW;
    CcCreatureRigPose rig = {0};
    if (!CcCreatureRigPoseResolve(
            rig_profile, phase / (2.0f * 3.14159265358979323846f),
            moving ? 1.0f : 0.0f, (CcLimbVec3){0}, 0.0f, 1.0f, &rig)) {
        return;
    }
    ResolveFromRig(morphology, &rig, result);
}
