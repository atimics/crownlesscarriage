#ifndef CROWNLESS_LOCAL3D_INTERNAL_H
#define CROWNLESS_LOCAL3D_INTERNAL_H

#include "client/cc_local3d.h"

void CcLocalAgentFixedStepInternal(CcLocalAgent *agent, float delta_time,
                                   bool market_interior);
void CcLocalCourseFixedStepInternal(CcLocalCourse *course,
                                    CcLocalAgent *player,
                                    const CcSim *sim, float delta_time);
void CcLocalAgentInterpolateInternal(CcLocalAgent *agent, float amount);
void CcLocalCourseInterpolateInternal(CcLocalCourse *course, float amount);

void CcLocalRendererRecordBiped(bool high_detail);
void CcLocalRendererRecordSkinUpdate(int32_t mesh_count);

#endif
