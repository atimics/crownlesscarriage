#ifndef CROWNLESS_ROBOTICS_H
#define CROWNLESS_ROBOTICS_H

#include "locomotion/cc_limb.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_ROBOT_POINT_CAPACITY 128
#define CC_ROBOT_CLIMB_ROUTE_CAPACITY 128

typedef struct CcRobotCollisionPoint {
    CcLimbVec3 center;
    float radius;
} CcRobotCollisionPoint;

typedef enum CcRobotSurfaceKind {
    CC_ROBOT_SURFACE_FLOOR = 0,
    CC_ROBOT_SURFACE_WALL,
    CC_ROBOT_SURFACE_CEILING,
} CcRobotSurfaceKind;

typedef struct CcRobotClimbNode {
    CcLimbVec3 point;
    CcLimbVec3 normal;
    CcRobotSurfaceKind surface;
    float cost;
} CcRobotClimbNode;

typedef struct CcRobotClimbRoute {
    CcRobotClimbNode nodes[CC_ROBOT_CLIMB_ROUTE_CAPACITY];
    int32_t count;
    float length;
} CcRobotClimbRoute;


typedef bool (*CcRobotSurfaceProbe)(void *context, CcLimbVec3 sample,
                                    float search_radius,
                                    CcLimbVec3 *point,
                                    CcLimbVec3 *normal);


int32_t CcRobotSampleLink(CcLimbVec3 start, CcLimbVec3 end, float radius,
                          CcRobotCollisionPoint *points, int32_t capacity);


int32_t CcRobotLimbPointSpace(const CcLimbRig *rig, float radius,
                              CcRobotCollisionPoint *points,
                              int32_t capacity);


bool CcRobotPredictiveAvoidance(
    CcLimbVec3 first_position, CcLimbVec3 first_velocity,
    CcLimbVec3 second_position, CcLimbVec3 second_velocity,
    float minimum_clearance, float horizon, int32_t stable_pair_index,
    CcLimbVec3 *first_correction, CcLimbVec3 *second_correction);


float CcRobotTraversabilityCost(float distance, float height_change,
                                float support_normal_y);


bool CcRobotPlanFreeClimb(CcLimbVec3 start_point,
                          CcLimbVec3 start_normal,
                          CcLimbVec3 goal_point,
                          CcLimbVec3 goal_normal,
                          float step_length,
                          float maximum_reach,
                          CcRobotSurfaceProbe probe,
                          void *context,
                          CcRobotClimbRoute *route);

#endif
