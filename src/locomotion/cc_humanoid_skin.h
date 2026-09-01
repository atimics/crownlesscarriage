#ifndef CROWNLESS_HUMANOID_SKIN_H
#define CROWNLESS_HUMANOID_SKIN_H

#include "locomotion/cc_humanoid.h"

#include <stdbool.h>
#include <stdint.h>


typedef enum CcHumanoidSkinBone {
    CC_HUMANOID_SKIN_ROOT,
    CC_HUMANOID_SKIN_PELVIS,
    CC_HUMANOID_SKIN_SPINE,
    CC_HUMANOID_SKIN_CHEST,
    CC_HUMANOID_SKIN_NECK,
    CC_HUMANOID_SKIN_HEAD,
    CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
    CC_HUMANOID_SKIN_FOREARM_LEFT,
    CC_HUMANOID_SKIN_HAND_LEFT,
    CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    CC_HUMANOID_SKIN_FOREARM_RIGHT,
    CC_HUMANOID_SKIN_HAND_RIGHT,
    CC_HUMANOID_SKIN_THIGH_LEFT,
    CC_HUMANOID_SKIN_SHIN_LEFT,
    CC_HUMANOID_SKIN_FOOT_LEFT,
    CC_HUMANOID_SKIN_THIGH_RIGHT,
    CC_HUMANOID_SKIN_SHIN_RIGHT,
    CC_HUMANOID_SKIN_FOOT_RIGHT,
    CC_HUMANOID_SKIN_BONE_COUNT
} CcHumanoidSkinBone;

typedef enum CcHumanoidSkinSocket {
    CC_HUMANOID_SOCKET_HEAD,
    CC_HUMANOID_SOCKET_CHEST_FRONT,
    CC_HUMANOID_SOCKET_BACK,
    CC_HUMANOID_SOCKET_SHOULDER_LEFT,
    CC_HUMANOID_SOCKET_SHOULDER_RIGHT,
    CC_HUMANOID_SOCKET_FOREARM_LEFT,
    CC_HUMANOID_SOCKET_FOREARM_RIGHT,
    CC_HUMANOID_SOCKET_HAND_LEFT,
    CC_HUMANOID_SOCKET_HAND_RIGHT,
    CC_HUMANOID_SOCKET_BELT,
    CC_HUMANOID_SOCKET_HIP_LEFT,
    CC_HUMANOID_SOCKET_HIP_RIGHT,
    CC_HUMANOID_SOCKET_SHIN_LEFT,
    CC_HUMANOID_SOCKET_SHIN_RIGHT,
    CC_HUMANOID_SOCKET_FOOT_LEFT,
    CC_HUMANOID_SOCKET_FOOT_RIGHT,
    CC_HUMANOID_SOCKET_COUNT
} CcHumanoidSkinSocket;

typedef CcMotionQuaternion CcHumanoidSkinQuaternion;

typedef struct CcHumanoidSkinBonePose {
    CcLimbVec3 head;
    CcLimbVec3 tail;
    CcLimbVec3 right;
    CcLimbVec3 up;
    CcLimbVec3 forward;
    CcHumanoidSkinQuaternion world_rotation;
    int32_t parent;
} CcHumanoidSkinBonePose;

typedef struct CcHumanoidSkinSocketPose {
    CcLimbVec3 position;
    CcHumanoidSkinQuaternion world_rotation;
    CcHumanoidSkinBone bone;
} CcHumanoidSkinSocketPose;

typedef struct CcHumanoidSkinPose {
    CcHumanoidSkinBonePose bones[CC_HUMANOID_SKIN_BONE_COUNT];
    CcMotionTransform local_bones[CC_HUMANOID_SKIN_BONE_COUNT];
    CcHumanoidSkinSocketPose sockets[CC_HUMANOID_SOCKET_COUNT];
    CcLimbVec3 body_right;
    CcLimbVec3 body_up;
    CcLimbVec3 body_forward;
    bool valid;
} CcHumanoidSkinPose;

const char *CcHumanoidSkinBoneName(CcHumanoidSkinBone bone);
int32_t CcHumanoidSkinBoneParent(CcHumanoidSkinBone bone);
int32_t CcHumanoidSkinBoneFind(const char *name);
const char *CcHumanoidSkinSocketName(CcHumanoidSkinSocket socket);
void CcHumanoidSkinPoseResolve(const CcHumanoidPose *source,
                               CcHumanoidSkinPose *result);

#endif
