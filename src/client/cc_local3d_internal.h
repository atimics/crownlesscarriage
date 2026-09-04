#ifndef CROWNLESS_LOCAL3D_INTERNAL_H
#define CROWNLESS_LOCAL3D_INTERNAL_H

/* Narrow test and cross-source seams for crownless_local_renderer. */

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

typedef struct CcLocalFaceAnchorInternal {
    float forward;
    float right;
} CcLocalFaceAnchorInternal;

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
bool CcLocalAgentPointSpaceBlockedInternal(const CcLocalAgent *agent,
                                            Vector3 proposed);

Camera3D CcLocalStreetCameraInternal(const CcLocalAgent *agent, float clock,
                                     bool advance, int32_t art_height);
Camera3D CcLocalCombatCameraInternal(Camera3D base,
                                     const CcLocalAgent *player,
                                     const CcLocalCourse *course,
                                     float clock, bool advance,
                                     int32_t art_height);
Camera3D CcLocalConversationCameraInternal(
    Camera3D base, const CcLocalAgent *player, const CcLocalAgent *partner,
    bool active, float clock, bool advance, int32_t art_height);
int32_t CcLocalCargoBoxCountInternal(const CcPlayerCompany *player);
float CcLocalCameraTreeOcclusionScoreInternal(Camera3D camera,
                                               Vector3 first_subject,
                                               Vector3 second_subject);
Camera3D CcLocalCameraClearSightlinesInternal(Camera3D camera,
                                              Vector3 first_subject,
                                              Vector3 second_subject,
                                              float preferred_angle,
                                              float *chosen_angle);
bool CcLocalBuildingObscuresHeroInternal(Rectangle footprint, float height,
                                         Camera3D camera,
                                         Vector3 hero_center,
                                         int32_t render_width,
                                         int32_t render_height);

CcLocalFaceLodInternal CcLocalFaceLodForProjectedHeightInternal(
    float projected_face_height);
CcLocalFaceViewInternal CcLocalFaceViewForFrontAmountInternal(
    float front_amount);
CcLocalFaceAnchorInternal CcLocalFaceAnchorForCameraInternal(
    float front_amount, float side_amount);

void CcLocalRendererRecordBiped(bool high_detail);
void CcLocalRendererUpdateAtmosphereInternal(float delta_time);
void CcLocalRendererRecordSkinUpdate(int32_t mesh_count);
void CcLocalRendererRecordHeroSkinUpdate(int32_t mesh_count);
void CcLocalRendererRecordNpcSkinUpdate(int32_t mesh_count);
void CcLocalRendererRecordCreatureSkinUpdate(int32_t mesh_count);
void CcLocalRendererRecordStaticBatch(int32_t draw_count,
                                      int32_t instance_count);

#endif
