#ifndef CROWNLESS_MULTILEG_H
#define CROWNLESS_MULTILEG_H

#include "locomotion/cc_biomech.h"
#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_MULTILEG_BODY_PARTICLE_COUNT 5

typedef enum CcMultilegBodyParticle {
    CC_MULTILEG_BODY_CENTER,
    CC_MULTILEG_BODY_FRONT,
    CC_MULTILEG_BODY_BACK,
    CC_MULTILEG_BODY_LEFT,
    CC_MULTILEG_BODY_RIGHT
} CcMultilegBodyParticle;

typedef struct CcMultilegRagdoll {
    CcBiomechRagdoll physics;
    int32_t body_particle[CC_MULTILEG_BODY_PARTICLE_COUNT];
    int32_t limb_particle[CC_LIMB_MAX_COUNT][CC_LIMB_MAX_JOINTS];
    CcLimbVec3 body_center;
    CcLimbVec3 body_velocity;
    CcLimbVec3 recovery_ground;
    float body_yaw;
    float active_seconds;
    float settled_seconds;
    float recovery_seconds;
    float recovery_error;
    float control_authority;
    int32_t support_contacts;
    bool initialized;
    bool active;
    bool recovering;
    bool recovery_allowed;
} CcMultilegRagdoll;

bool CcMultilegRagdollCollapse(
    CcMultilegRagdoll *body, CcLimbRig *rig,
    CcLimbVec3 body_center, float body_yaw, CcLimbVec3 velocity,
    CcLimbVec3 impact_direction, CcLimbVec3 impact_point,
    float impact_speed, bool recovery_allowed);
bool CcMultilegRagdollStep(
    CcMultilegRagdoll *body, CcLimbRig *rig, float delta_time,
    CcLimbTerrainProbe terrain_probe,
    CcBiomechRagdollCollisionProbe collision_probe, void *probe_context);
void CcMultilegRagdollAllowRecovery(CcMultilegRagdoll *body);
int32_t CcMultilegRagdollSupportContactCount(
    const CcMultilegRagdoll *body);
const char *CcMultilegRagdollStateName(const CcMultilegRagdoll *body);

#endif
