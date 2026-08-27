#ifndef CROWNLESS_ROBOTICS_H
#define CROWNLESS_ROBOTICS_H

#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_ROBOT_POINT_CAPACITY 128

typedef struct CcRobotCollisionPoint {
    CcLimbVec3 center;
    float radius;
} CcRobotCollisionPoint;

/* Build an overlapping-sphere approximation of a line segment. The spacing
   never exceeds the sphere diameter, so the samples conservatively cover the
   complete link rather than only its joints. */
int32_t CcRobotSampleLink(CcLimbVec3 start, CcLimbVec3 end, float radius,
                          CcRobotCollisionPoint *points, int32_t capacity);

/* Convert every healthy articulated link into a compact point-space proxy. */
int32_t CcRobotLimbPointSpace(const CcLimbRig *rig, float radius,
                              CcRobotCollisionPoint *points,
                              int32_t capacity);

/* Return equal and opposite velocity corrections when two moving bodies are
   predicted to enter their shared clearance during the supplied horizon. */
bool CcRobotPredictiveAvoidance(
    CcLimbVec3 first_position, CcLimbVec3 first_velocity,
    CcLimbVec3 second_position, CcLimbVec3 second_velocity,
    float minimum_clearance, float horizon, int32_t stable_pair_index,
    CcLimbVec3 *first_correction, CcLimbVec3 *second_correction);

/* A deterministic terrain edge cost. It remains at least the geometric
   distance, preserving the admissibility of a straight-line A* heuristic. */
float CcRobotTraversabilityCost(float distance, float height_change,
                                float support_normal_y);

#endif
