#ifndef CROWNLESS_COLLISION_H
#define CROWNLESS_COLLISION_H

#include "locomotion/cc_limb.h"

#define CC_COLLISION_SKIN 0.002f

/* Bounds before rotation about their center. Yaw follows the world Y axis. */
typedef struct CcCollisionBox {
    CcLimbVec3 minimum;
    CcLimbVec3 maximum;
    float yaw;
} CcCollisionBox;

typedef struct CcCollisionHit {
    float fraction;
    CcLimbVec3 normal;
} CcCollisionHit;

typedef struct CcCollisionMove {
    CcLimbVec3 position;
    CcLimbVec3 normal;
    bool collided;
} CcCollisionMove;

bool CcCollisionSweepSphere(const CcCollisionBox *box, CcLimbVec3 start,
                            CcLimbVec3 end, float radius,
                            CcCollisionHit *hit);
bool CcCollisionPushSphere(const CcCollisionBox *box, float radius,
                           CcLimbVec3 *position, CcLimbVec3 *normal);
/* Physical particles keep contact at their true radius. */
CcCollisionMove CcCollisionMoveSphere(const CcCollisionBox *boxes, int32_t count,
                                     CcLimbVec3 center, CcLimbVec3 displacement,
                                     float radius);
/* The upright capsule starts at its feet; height includes both round ends. */
CcCollisionMove CcCollisionMoveCapsule(const CcCollisionBox *boxes,
                                      int32_t count, CcLimbVec3 feet,
                                      CcLimbVec3 displacement,
                                      float radius, float height);

#endif
