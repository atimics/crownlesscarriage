#ifndef CROWNLESS_FOOTING_H
#define CROWNLESS_FOOTING_H

#include "locomotion/cc_limb.h"
#include "locomotion/cc_biomech.h"

/* Terrain contacts are world positions on an upward-facing surface. */
bool CcFootingProbe(CcLimbTerrainProbe probe, void *context,
                    CcLimbVec3 desired, float rise, float drop,
                    CcLimbVec3 *point, CcLimbVec3 *normal);
bool CcFootingContactValid(CcLimbTerrainProbe probe, void *context,
                           CcLimbVec3 contact, float tolerance,
                           CcLimbVec3 *normal);
/* The swing uses eased horizontal travel and a sine arch. */
bool CcFootingPlanSwing(CcLimbVec3 start, CcLimbVec3 target,
                        float base_lift, float maximum_lift, float radius,
                        CcLimbTerrainProbe terrain,
                        CcBiomechRagdollCollisionProbe collision,
                        void *context, float *lift);
CcLimbVec3 CcFootingSwingPoint(CcLimbVec3 start, CcLimbVec3 target,
                               float travel, float lift);

#endif
