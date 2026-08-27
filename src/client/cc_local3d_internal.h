#ifndef CROWNLESS_LOCAL3D_INTERNAL_H
#define CROWNLESS_LOCAL3D_INTERNAL_H

#include "client/cc_local3d.h"

typedef enum CcLocalFaceLodInternal {
    CC_LOCAL_FACE_LOD_SILHOUETTE,
    CC_LOCAL_FACE_LOD_READABLE,
    CC_LOCAL_FACE_LOD_CLOSE
} CcLocalFaceLodInternal;

typedef enum CcLocalFaceViewInternal {
    CC_LOCAL_FACE_VIEW_FRONT,
    CC_LOCAL_FACE_VIEW_THREE_QUARTER,
    CC_LOCAL_FACE_VIEW_PROFILE
} CcLocalFaceViewInternal;

void CcLocalAgentFixedStepInternal(CcLocalAgent *agent, float delta_time,
                                   bool market_interior);
void CcLocalCourseFixedStepInternal(CcLocalCourse *course,
                                    CcLocalAgent *player,
                                    const CcSim *sim, float delta_time);
void CcLocalAgentInterpolateInternal(CcLocalAgent *agent, float amount);
void CcLocalCourseInterpolateInternal(CcLocalCourse *course, float amount);
void CcLocalSetStreetMarketCratesInternal(int32_t count);
bool CcLocalProbePhysicsSphereInternal(
    CcLocalSceneKind scene, Vector3 previous, Vector3 proposed, float radius,
    Vector3 *corrected, Vector3 *normal);
float CcLocalRoomArtRayDistanceInternal(Ray ray, Vector3 focus);

Camera3D CcLocalStreetCameraInternal(const CcLocalAgent *agent, float clock,
                                     bool advance, int32_t art_height);
Camera3D CcLocalCombatCameraInternal(Camera3D base,
                                     const CcLocalAgent *player,
                                     const CcLocalCourse *course,
                                     float clock, bool advance,
                                     int32_t art_height);
float CcLocalCameraTreeOcclusionScoreInternal(Camera3D camera,
                                               Vector3 first_subject,
                                               Vector3 second_subject);
Camera3D CcLocalCameraClearSightlinesInternal(Camera3D camera,
                                              Vector3 first_subject,
                                              Vector3 second_subject,
                                              float preferred_angle,
                                              float *chosen_angle);

CcLocalFaceLodInternal CcLocalFaceLodForProjectedHeightInternal(
    float projected_face_height);
CcLocalFaceViewInternal CcLocalFaceViewForFrontAmountInternal(
    float front_amount);

void CcLocalRendererRecordBiped(bool high_detail);
void CcLocalRendererRecordSkinUpdate(int32_t mesh_count);

#endif
