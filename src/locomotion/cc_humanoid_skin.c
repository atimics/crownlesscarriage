#include "locomotion/cc_humanoid_skin.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct CcHumanoidSkinBoneDefinition {
    const char *name;
    int32_t parent;
} CcHumanoidSkinBoneDefinition;

static const CcHumanoidSkinBoneDefinition BONE_DEFINITIONS[] = {
    {"root", -1},
    {"pelvis", CC_HUMANOID_SKIN_ROOT},
    {"spine", CC_HUMANOID_SKIN_PELVIS},
    {"chest", CC_HUMANOID_SKIN_SPINE},
    {"neck", CC_HUMANOID_SKIN_CHEST},
    {"head", CC_HUMANOID_SKIN_NECK},
    {"upper_arm.L", CC_HUMANOID_SKIN_CHEST},
    {"forearm.L", CC_HUMANOID_SKIN_UPPER_ARM_LEFT},
    {"hand.L", CC_HUMANOID_SKIN_FOREARM_LEFT},
    {"upper_arm.R", CC_HUMANOID_SKIN_CHEST},
    {"forearm.R", CC_HUMANOID_SKIN_UPPER_ARM_RIGHT},
    {"hand.R", CC_HUMANOID_SKIN_FOREARM_RIGHT},
    {"thigh.L", CC_HUMANOID_SKIN_PELVIS},
    {"shin.L", CC_HUMANOID_SKIN_THIGH_LEFT},
    {"foot.L", CC_HUMANOID_SKIN_SHIN_LEFT},
    {"thigh.R", CC_HUMANOID_SKIN_PELVIS},
    {"shin.R", CC_HUMANOID_SKIN_THIGH_RIGHT},
    {"foot.R", CC_HUMANOID_SKIN_SHIN_RIGHT},
};

static const char *SOCKET_NAMES[] = {
    "head", "chest.front", "back", "shoulder.L", "shoulder.R",
    "forearm.L", "forearm.R", "hand.L", "hand.R", "belt",
    "hip.L", "hip.R", "shin.L", "shin.R", "foot.L", "foot.R",
};

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

static float Length(CcLimbVec3 value)
{
    return sqrtf(Dot(value, value));
}

static CcLimbVec3 NormalizeOr(CcLimbVec3 value, CcLimbVec3 fallback)
{
    float length = Length(value);
    if (length > 0.00001f && isfinite(length)) return Scale(value, 1.0f / length);
    length = Length(fallback);
    return length > 0.00001f ? Scale(fallback, 1.0f / length) :
                              (CcLimbVec3){0.0f, 1.0f, 0.0f};
}

static CcHumanoidSkinQuaternion QuaternionFromBasis(CcLimbVec3 right,
                                                     CcLimbVec3 up,
                                                     CcLimbVec3 forward)
{
    float m00 = right.x;
    float m01 = up.x;
    float m02 = forward.x;
    float m10 = right.y;
    float m11 = up.y;
    float m12 = forward.y;
    float m20 = right.z;
    float m21 = up.z;
    float m22 = forward.z;
    float trace = m00 + m11 + m22;
    CcHumanoidSkinQuaternion result = {0.0f, 0.0f, 0.0f, 1.0f};
    if (trace > 0.0f) {
        float scale = sqrtf(trace + 1.0f) * 2.0f;
        result.w = 0.25f * scale;
        result.x = (m21 - m12) / scale;
        result.y = (m02 - m20) / scale;
        result.z = (m10 - m01) / scale;
    } else if (m00 > m11 && m00 > m22) {
        float scale = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
        result.w = (m21 - m12) / scale;
        result.x = 0.25f * scale;
        result.y = (m01 + m10) / scale;
        result.z = (m02 + m20) / scale;
    } else if (m11 > m22) {
        float scale = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
        result.w = (m02 - m20) / scale;
        result.x = (m01 + m10) / scale;
        result.y = 0.25f * scale;
        result.z = (m12 + m21) / scale;
    } else {
        float scale = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
        result.w = (m10 - m01) / scale;
        result.x = (m02 + m20) / scale;
        result.y = (m12 + m21) / scale;
        result.z = 0.25f * scale;
    }
    float length = sqrtf(result.x * result.x + result.y * result.y +
                         result.z * result.z + result.w * result.w);
    if (length > 0.00001f) {
        result.x /= length;
        result.y /= length;
        result.z /= length;
        result.w /= length;
    }
    return result;
}

static CcHumanoidSkinQuaternion QuaternionConjugate(
    CcHumanoidSkinQuaternion value)
{
    return (CcHumanoidSkinQuaternion){
        -value.x, -value.y, -value.z, value.w
    };
}

static CcHumanoidSkinQuaternion QuaternionMultiply(
    CcHumanoidSkinQuaternion a, CcHumanoidSkinQuaternion b)
{
    CcHumanoidSkinQuaternion result = {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
    float length = sqrtf(result.x * result.x + result.y * result.y +
                         result.z * result.z + result.w * result.w);
    if (length > 0.00001f) {
        result.x /= length;
        result.y /= length;
        result.z /= length;
        result.w /= length;
    }
    return result;
}

static CcLimbVec3 QuaternionRotate(CcHumanoidSkinQuaternion rotation,
                                   CcLimbVec3 point)
{
    CcLimbVec3 axis = {rotation.x, rotation.y, rotation.z};
    CcLimbVec3 twice_cross = Scale(Cross(axis, point), 2.0f);
    return Add(point, Add(Scale(twice_cross, rotation.w),
                          Cross(axis, twice_cross)));
}

static void ResolveLocalTransforms(CcHumanoidSkinPose *pose)
{
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        const CcHumanoidSkinBonePose *world = &pose->bones[bone];
        CcMotionTransform *local = &pose->local_bones[bone];
        local->scale = (CcLimbVec3){1.0f, 1.0f, 1.0f};
        if (world->parent < 0) {
            local->translation = world->head;
            local->rotation = world->world_rotation;
            continue;
        }
        const CcHumanoidSkinBonePose *parent = &pose->bones[world->parent];
        CcHumanoidSkinQuaternion inverse_parent = QuaternionConjugate(
            parent->world_rotation);
        local->translation = QuaternionRotate(
            inverse_parent, Subtract(world->head, parent->head));
        local->rotation = QuaternionMultiply(inverse_parent,
                                              world->world_rotation);
    }
}

static void ResolveBone(CcHumanoidSkinPose *pose, CcHumanoidSkinBone bone,
                        CcLimbVec3 head, CcLimbVec3 tail,
                        CcLimbVec3 reference, bool foot_frame)
{
    CcHumanoidSkinBonePose *result = &pose->bones[bone];
    result->head = head;
    result->tail = tail;
    result->parent = CcHumanoidSkinBoneParent(bone);
    result->up = NormalizeOr(Subtract(tail, head), pose->body_up);
    CcLimbVec3 right = foot_frame ? Cross(reference, result->up) :
                                   Cross(result->up, reference);
    result->right = NormalizeOr(right, pose->body_right);
    result->forward = NormalizeOr(Cross(result->right, result->up),
                                  pose->body_forward);
    result->right = NormalizeOr(Cross(result->up, result->forward),
                                pose->body_right);
    result->world_rotation = QuaternionFromBasis(result->right, result->up,
                                                 result->forward);
}

static CcLimbVec3 Midpoint(CcLimbVec3 a, CcLimbVec3 b)
{
    return Scale(Add(a, b), 0.5f);
}

static void ResolveSocket(CcHumanoidSkinPose *pose,
                          CcHumanoidSkinSocket socket,
                          CcHumanoidSkinBone bone, CcLimbVec3 position)
{
    pose->sockets[socket].position = position;
    pose->sockets[socket].world_rotation = pose->bones[bone].world_rotation;
    pose->sockets[socket].bone = bone;
}

const char *CcHumanoidSkinBoneName(CcHumanoidSkinBone bone)
{
    if (bone < 0 || bone >= CC_HUMANOID_SKIN_BONE_COUNT) return "invalid";
    return BONE_DEFINITIONS[bone].name;
}

int32_t CcHumanoidSkinBoneParent(CcHumanoidSkinBone bone)
{
    if (bone < 0 || bone >= CC_HUMANOID_SKIN_BONE_COUNT) return -1;
    return BONE_DEFINITIONS[bone].parent;
}

int32_t CcHumanoidSkinBoneFind(const char *name)
{
    if (name == NULL) return -1;
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        if (strcmp(name, BONE_DEFINITIONS[bone].name) == 0) return bone;
    }
    return -1;
}

const char *CcHumanoidSkinSocketName(CcHumanoidSkinSocket socket)
{
    if (socket < 0 || socket >= CC_HUMANOID_SOCKET_COUNT) return "invalid";
    return SOCKET_NAMES[socket];
}

void CcHumanoidSkinPoseResolve(const CcHumanoidPose *source,
                               CcHumanoidSkinPose *result)
{
    if (result == NULL) return;
    (void)memset(result, 0, sizeof(*result));
    if (source == NULL) return;

    CcLimbVec3 torso_up = Subtract(source->neck, source->pelvis);
    result->body_up = NormalizeOr(torso_up, (CcLimbVec3){0.0f, 1.0f, 0.0f});
    CcLimbVec3 shoulder_axis = Subtract(source->shoulder[1],
                                        source->shoulder[0]);
    shoulder_axis = Subtract(shoulder_axis,
                             Scale(result->body_up,
                                   Dot(shoulder_axis, result->body_up)));
    CcLimbVec3 hip_axis = Subtract(source->hip[1], source->hip[0]);
    result->body_right = NormalizeOr(shoulder_axis, hip_axis);
    result->body_forward = NormalizeOr(Cross(result->body_right,
                                             result->body_up),
                                       (CcLimbVec3){0.0f, 0.0f, 1.0f});
    result->body_right = NormalizeOr(Cross(result->body_up,
                                           result->body_forward),
                                     result->body_right);

    CcLimbVec3 root_head = Add(source->pelvis,
                               Scale(result->body_up, -0.90f));
    ResolveBone(result, CC_HUMANOID_SKIN_ROOT, root_head,
                Add(root_head, Scale(result->body_up, 0.18f)),
                result->body_forward, false);
    ResolveBone(result, CC_HUMANOID_SKIN_PELVIS, source->pelvis,
                source->spine, result->body_forward, false);
    ResolveBone(result, CC_HUMANOID_SKIN_SPINE, source->spine,
                source->chest, result->body_forward, false);
    ResolveBone(result, CC_HUMANOID_SKIN_CHEST, source->chest,
                source->neck, result->body_forward, false);
    ResolveBone(result, CC_HUMANOID_SKIN_NECK, source->neck,
                source->head, result->body_forward, false);
    CcLimbVec3 head_direction = NormalizeOr(Subtract(source->head,
                                                     source->neck),
                                             result->body_up);
    ResolveBone(result, CC_HUMANOID_SKIN_HEAD, source->head,
                Add(source->head, Scale(head_direction, 0.18f)),
                result->body_forward, false);

    const CcHumanoidSkinBone upper_bones[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    const CcHumanoidSkinBone forearm_bones[] = {
        CC_HUMANOID_SKIN_FOREARM_LEFT,
        CC_HUMANOID_SKIN_FOREARM_RIGHT,
    };
    const CcHumanoidSkinBone hand_bones[] = {
        CC_HUMANOID_SKIN_HAND_LEFT,
        CC_HUMANOID_SKIN_HAND_RIGHT,
    };
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        ResolveBone(result, upper_bones[arm], source->shoulder[arm],
                    source->elbow[arm], result->body_forward, false);
        ResolveBone(result, forearm_bones[arm], source->elbow[arm],
                    source->hand[arm], result->body_forward, false);
        CcLimbVec3 hand_direction = NormalizeOr(
            Subtract(source->hand[arm], source->elbow[arm]),
            Scale(result->body_up, -1.0f));
        ResolveBone(result, hand_bones[arm], source->hand[arm],
                    Add(source->hand[arm], Scale(hand_direction, 0.16f)),
                    result->body_forward, false);
    }

    const CcHumanoidSkinBone thigh_bones[] = {
        CC_HUMANOID_SKIN_THIGH_LEFT,
        CC_HUMANOID_SKIN_THIGH_RIGHT,
    };
    const CcHumanoidSkinBone shin_bones[] = {
        CC_HUMANOID_SKIN_SHIN_LEFT,
        CC_HUMANOID_SKIN_SHIN_RIGHT,
    };
    const CcHumanoidSkinBone foot_bones[] = {
        CC_HUMANOID_SKIN_FOOT_LEFT,
        CC_HUMANOID_SKIN_FOOT_RIGHT,
    };
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        ResolveBone(result, thigh_bones[leg], source->hip[leg],
                    source->knee[leg], result->body_forward, false);
        ResolveBone(result, shin_bones[leg], source->knee[leg],
                    source->ankle[leg], result->body_forward, false);
        ResolveBone(result, foot_bones[leg], source->ankle[leg],
                    source->toe[leg], result->body_up, true);
    }

    ResolveLocalTransforms(result);

    ResolveSocket(result, CC_HUMANOID_SOCKET_HEAD,
                  CC_HUMANOID_SKIN_HEAD, source->head);
    ResolveSocket(result, CC_HUMANOID_SOCKET_CHEST_FRONT,
                  CC_HUMANOID_SKIN_CHEST,
                  Add(source->chest, Scale(result->body_forward, 0.17f)));
    ResolveSocket(result, CC_HUMANOID_SOCKET_BACK,
                  CC_HUMANOID_SKIN_CHEST,
                  Add(source->chest, Scale(result->body_forward, -0.18f)));
    ResolveSocket(result, CC_HUMANOID_SOCKET_SHOULDER_LEFT,
                  CC_HUMANOID_SKIN_UPPER_ARM_LEFT, source->shoulder[0]);
    ResolveSocket(result, CC_HUMANOID_SOCKET_SHOULDER_RIGHT,
                  CC_HUMANOID_SKIN_UPPER_ARM_RIGHT, source->shoulder[1]);
    ResolveSocket(result, CC_HUMANOID_SOCKET_FOREARM_LEFT,
                  CC_HUMANOID_SKIN_FOREARM_LEFT,
                  Midpoint(source->elbow[0], source->hand[0]));
    ResolveSocket(result, CC_HUMANOID_SOCKET_FOREARM_RIGHT,
                  CC_HUMANOID_SKIN_FOREARM_RIGHT,
                  Midpoint(source->elbow[1], source->hand[1]));
    ResolveSocket(result, CC_HUMANOID_SOCKET_HAND_LEFT,
                  CC_HUMANOID_SKIN_HAND_LEFT, source->hand[0]);
    ResolveSocket(result, CC_HUMANOID_SOCKET_HAND_RIGHT,
                  CC_HUMANOID_SKIN_HAND_RIGHT, source->hand[1]);
    ResolveSocket(result, CC_HUMANOID_SOCKET_BELT,
                  CC_HUMANOID_SKIN_PELVIS, source->pelvis);
    ResolveSocket(result, CC_HUMANOID_SOCKET_HIP_LEFT,
                  CC_HUMANOID_SKIN_THIGH_LEFT, source->hip[0]);
    ResolveSocket(result, CC_HUMANOID_SOCKET_HIP_RIGHT,
                  CC_HUMANOID_SKIN_THIGH_RIGHT, source->hip[1]);
    ResolveSocket(result, CC_HUMANOID_SOCKET_SHIN_LEFT,
                  CC_HUMANOID_SKIN_SHIN_LEFT,
                  Midpoint(source->knee[0], source->ankle[0]));
    ResolveSocket(result, CC_HUMANOID_SOCKET_SHIN_RIGHT,
                  CC_HUMANOID_SKIN_SHIN_RIGHT,
                  Midpoint(source->knee[1], source->ankle[1]));
    ResolveSocket(result, CC_HUMANOID_SOCKET_FOOT_LEFT,
                  CC_HUMANOID_SKIN_FOOT_LEFT,
                  Midpoint(source->heel[0], source->toe[0]));
    ResolveSocket(result, CC_HUMANOID_SOCKET_FOOT_RIGHT,
                  CC_HUMANOID_SKIN_FOOT_RIGHT,
                  Midpoint(source->heel[1], source->toe[1]));
    result->valid = true;
}
