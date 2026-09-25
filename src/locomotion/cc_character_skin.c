#include "locomotion/cc_character_skin.h"

#include "locomotion/cc_humanoid_skin.h"
#include "locomotion/cc_quadruped.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const char *FAMILY_NAMES[CC_SKELETON_FAMILY_COUNT] = {
    "none", "humanoid", "quadruped", "dragon",
};

const char *CcSkeletonFamilyName(CcSkeletonFamily family)
{
    if (family < 0 || family >= CC_SKELETON_FAMILY_COUNT) return "invalid";
    return FAMILY_NAMES[family];
}

CcSkeletonFamily CcSkeletonFamilyFind(const char *name)
{
    if (name == NULL) return CC_SKELETON_NONE;
    for (int32_t family = 0; family < CC_SKELETON_FAMILY_COUNT; ++family) {
        if (strcmp(name, FAMILY_NAMES[family]) == 0) {
            return (CcSkeletonFamily)family;
        }
    }
    return CC_SKELETON_NONE;
}

int32_t CcSkeletonFamilyBoneCount(CcSkeletonFamily family)
{
    switch (family) {
        case CC_SKELETON_HUMANOID: return CC_HUMANOID_SKIN_BONE_COUNT;
        case CC_SKELETON_QUADRUPED: return CC_QUADRUPED_BONE_COUNT;
        case CC_SKELETON_DRAGON:
        case CC_SKELETON_NONE:
        case CC_SKELETON_FAMILY_COUNT:
        default:
            return 0;
    }
}

const char *CcSkeletonFamilyBoneName(CcSkeletonFamily family, int32_t bone)
{
    if (bone < 0 || bone >= CcSkeletonFamilyBoneCount(family)) {
        return "invalid";
    }
    if (family == CC_SKELETON_HUMANOID) {
        return CcHumanoidSkinBoneName((CcHumanoidSkinBone)bone);
    }
    return CcQuadrupedBoneName((CcQuadrupedBone)bone);
}

int32_t CcSkeletonFamilyBoneFind(CcSkeletonFamily family, const char *name)
{
    if (family == CC_SKELETON_HUMANOID) return CcHumanoidSkinBoneFind(name);
    if (family == CC_SKELETON_QUADRUPED) return CcQuadrupedBoneFind(name);
    return -1;
}

/* Goblin numbers come from the hand-drawn rig this skin replaced: the chest
   sits 0.36 over the pelvis, the head 0.76, the shoulders 0.26 out. */
static const CcBipedBodyPlan BODY_PLANS[CC_BIPED_BODY_KIND_COUNT] = {
    [CC_BIPED_BODY_GOBLIN] = {
        .spine_rise = 0.18f,
        .chest_rise = 0.36f,
        .neck_rise = 0.54f,
        .head_rise = 0.64f,
        .head_forward = 0.02f,
        .chest_sway = 0.018f,
        .shoulder_half_width = 0.26f,
        .shoulder_rise = 0.02f,
        .upper_arm_length = 0.26f,
        .forearm_length = 0.25f,
        .arm_spread = 0.24f,
        .elbow_bend = 0.16f,
        .arm_swing = 1.6f,
        .arm_swing_limit = 0.55f,
        .foot_length = 0.20f,
        .heel_length = 0.08f,
        .carry = false,
    },
    [CC_BIPED_BODY_GOBLIN_CARRIER] = {
        .spine_rise = 0.18f,
        .chest_rise = 0.36f,
        .neck_rise = 0.54f,
        .head_rise = 0.64f,
        .head_forward = 0.02f,
        .chest_sway = 0.012f,
        .shoulder_half_width = 0.26f,
        .shoulder_rise = 0.02f,
        .upper_arm_length = 0.26f,
        .forearm_length = 0.25f,
        .arm_spread = 0.24f,
        .elbow_bend = 0.16f,
        .arm_swing = 0.0f,
        .arm_swing_limit = 0.0f,
        .foot_length = 0.20f,
        .heel_length = 0.08f,
        .carry = true,
        .carry_elbow = {0.34f, -0.31f, 0.08f},
        .carry_hand = {0.27f, -0.28f, 0.39f},
    },
};

const CcBipedBodyPlan *CcBipedBodyPlanFor(CcBipedBodyKind kind)
{
    if (kind < 0 || kind >= CC_BIPED_BODY_KIND_COUNT) return NULL;
    return &BODY_PLANS[kind];
}

static CcLimbVec3 Add(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static CcLimbVec3 Subtract(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CcLimbVec3 Scale(CcLimbVec3 value, float amount)
{
    return (CcLimbVec3){value.x * amount, value.y * amount, value.z * amount};
}

static float Dot(CcLimbVec3 a, CcLimbVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CcLimbVec3 Cross(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.y * b.z - a.z * b.y,
                        a.z * b.x - a.x * b.z,
                        a.x * b.y - a.y * b.x};
}

static CcLimbVec3 NormalizeOr(CcLimbVec3 value, CcLimbVec3 fallback)
{
    float length = sqrtf(Dot(value, value));
    if (length > 0.00001f && isfinite(length)) {
        return Scale(value, 1.0f / length);
    }
    return fallback;
}

static bool Finite(CcLimbVec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static float Clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

/* World point to model space: undo the origin, the yaw and the scale. */
static CcLimbVec3 LocalPoint(CcLimbVec3 point, CcLimbVec3 origin, float yaw,
                             float inverse_scale)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    float x = point.x - origin.x;
    float z = point.z - origin.z;
    return (CcLimbVec3){(x * cosine - z * sine) * inverse_scale,
                        (point.y - origin.y) * inverse_scale,
                        (x * sine + z * cosine) * inverse_scale};
}

static CcLimbVec3 LocalDirection(CcLimbVec3 direction, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (CcLimbVec3){direction.x * cosine - direction.z * sine,
                        direction.y,
                        direction.x * sine + direction.z * cosine};
}

/* A point written in the body basis (lateral, up, forward). */
static CcLimbVec3 BodyOffset(CcLimbVec3 lateral, CcLimbVec3 up,
                             CcLimbVec3 forward, float x, float y, float z)
{
    return Add(Add(Scale(lateral, x), Scale(up, y)), Scale(forward, z));
}

/* An arm segment that hangs down and out, turned forward by angle. */
static CcLimbVec3 SwungDirection(CcLimbVec3 lateral, CcLimbVec3 up,
                                 CcLimbVec3 forward, float spread,
                                 float angle)
{
    float down = -cosf(angle);
    float ahead = sinf(angle);
    return NormalizeOr(BodyOffset(lateral, up, forward, spread, down, ahead),
                       Scale(up, -1.0f));
}

bool CcHumanoidPoseFromBipedRig(const CcCreatureRigPose *rig,
                                const CcBipedBodyPlan *plan,
                                CcLimbVec3 origin, float yaw, float scale,
                                CcHumanoidPose *result)
{
    if (result == NULL) return false;
    (void)memset(result, 0, sizeof(*result));
    if (rig == NULL || plan == NULL || !rig->valid || rig->limb_count < 2 ||
        !isfinite(yaw) || !isfinite(scale) || scale <= 0.0f ||
        !Finite(origin)) {
        return false;
    }
    float inverse_scale = 1.0f / scale;
    CcLimbVec3 hip[2];
    CcLimbVec3 knee[2];
    CcLimbVec3 ankle[2];
    for (int32_t leg = 0; leg < 2; ++leg) {
        const CcCreatureRigLimbPose *limb = &rig->limbs[leg];
        if (limb->segment_count < 2) return false;
        hip[leg] = LocalPoint(limb->joints[0], origin, yaw, inverse_scale);
        knee[leg] = LocalPoint(limb->joints[1], origin, yaw, inverse_scale);
        ankle[leg] = LocalPoint(limb->joints[limb->segment_count], origin,
                                yaw, inverse_scale);
    }
    CcLimbVec3 pelvis = LocalPoint(rig->body, origin, yaw, inverse_scale);
    CcLimbVec3 up = NormalizeOr(LocalDirection(rig->up, yaw),
                                (CcLimbVec3){0.0f, 1.0f, 0.0f});
    CcLimbVec3 lateral = Subtract(hip[1], hip[0]);
    lateral = NormalizeOr(Subtract(lateral, Scale(up, Dot(lateral, up))),
                          (CcLimbVec3){1.0f, 0.0f, 0.0f});
    CcLimbVec3 forward = NormalizeOr(Cross(lateral, up),
                                     (CcLimbVec3){0.0f, 0.0f, 1.0f});
    CcLimbVec3 ground_forward = NormalizeOr(
        (CcLimbVec3){forward.x, 0.0f, forward.z},
        (CcLimbVec3){0.0f, 0.0f, 1.0f});

    float movement = Clamp(rig->movement, 0.0f, 1.0f);
    float sway = sinf(rig->phase * 6.28318530718f) * movement;
    result->pelvis = pelvis;
    result->spine = Add(pelvis, Scale(up, plan->spine_rise));
    result->chest = Add(Add(pelvis, Scale(up, plan->chest_rise)),
                        Scale(forward, plan->chest_sway * sway));
    result->neck = Add(pelvis, Scale(up, plan->neck_rise));
    result->head = Add(Add(pelvis, Scale(up, plan->head_rise)),
                       Scale(forward, plan->head_forward));

    for (int32_t leg = 0; leg < 2; ++leg) {
        result->hip[leg] = hip[leg];
        result->knee[leg] = knee[leg];
        result->ankle[leg] = ankle[leg];
        result->heel[leg] = Subtract(ankle[leg],
                                     Scale(ground_forward, plan->heel_length));
        result->ball[leg] = Add(ankle[leg], Scale(ground_forward,
                                                  plan->foot_length * 0.70f));
        result->toe[leg] = Add(ankle[leg],
                               Scale(ground_forward, plan->foot_length));
    }

    for (int32_t arm = 0; arm < 2; ++arm) {
        float side = arm == 0 ? -1.0f : 1.0f;
        CcLimbVec3 shoulder = Add(result->chest, BodyOffset(
            lateral, up, forward, side * plan->shoulder_half_width,
            plan->shoulder_rise, 0.0f));
        CcLimbVec3 elbow;
        CcLimbVec3 hand;
        if (plan->carry) {
            elbow = Add(result->chest, BodyOffset(
                lateral, up, forward, side * plan->carry_elbow.x,
                plan->carry_elbow.y, plan->carry_elbow.z));
            hand = Add(result->chest, BodyOffset(
                lateral, up, forward, side * plan->carry_hand.x,
                plan->carry_hand.y, plan->carry_hand.z));
        } else {
            /* Each arm swings against the leg on its own side. */
            float lead = 0.5f * Dot(Subtract(ankle[arm], ankle[1 - arm]),
                                    ground_forward);
            float angle = Clamp(-plan->arm_swing * lead,
                                -plan->arm_swing_limit,
                                plan->arm_swing_limit);
            CcLimbVec3 upper = SwungDirection(lateral, up, forward,
                                              side * plan->arm_spread, angle);
            CcLimbVec3 lower = SwungDirection(
                lateral, up, forward, 0.0f,
                angle * 1.4f + plan->elbow_bend);
            elbow = Add(shoulder, Scale(upper, plan->upper_arm_length));
            hand = Add(elbow, Scale(lower, plan->forearm_length));
        }
        result->shoulder[arm] = shoulder;
        result->elbow[arm] = elbow;
        result->hand[arm] = hand;
    }

    const CcLimbVec3 *points[] = {
        &result->pelvis, &result->spine, &result->chest, &result->neck,
        &result->head,
    };
    for (size_t index = 0; index < sizeof(points) / sizeof(points[0]);
         ++index) {
        if (!Finite(*points[index])) return false;
    }
    for (int32_t side = 0; side < 2; ++side) {
        if (!Finite(result->hip[side]) || !Finite(result->knee[side]) ||
            !Finite(result->ankle[side]) || !Finite(result->toe[side]) ||
            !Finite(result->shoulder[side]) || !Finite(result->elbow[side]) ||
            !Finite(result->hand[side])) {
            return false;
        }
    }
    return true;
}
