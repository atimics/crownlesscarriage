#ifndef CROWNLESS_LIMB_H
#define CROWNLESS_LIMB_H

#include <stdbool.h>
#include <stdint.h>

#define CC_LIMB_MAX_COUNT 16
#define CC_LIMB_MAX_SEGMENTS 4
#define CC_LIMB_MAX_JOINTS (CC_LIMB_MAX_SEGMENTS + 1)

typedef struct CcLimbVec3 {
    float x;
    float y;
    float z;
} CcLimbVec3;

typedef enum CcMorphologyPreset {
    CC_MORPHOLOGY_BIPED,
    CC_MORPHOLOGY_QUADRUPED,
    CC_MORPHOLOGY_HEXAPOD,
    CC_MORPHOLOGY_OCTOPOD,
    CC_MORPHOLOGY_PRESET_COUNT
} CcMorphologyPreset;

typedef enum CcLimbState {
    CC_LIMB_STANCE,
    CC_LIMB_SWING,
    CC_LIMB_SEARCHING,
    CC_LIMB_DISABLED
} CcLimbState;

typedef enum CcLimbPace {
    CC_LIMB_PACE_WALK,
    CC_LIMB_PACE_RUN,
    CC_LIMB_PACE_SPRINT,
    CC_LIMB_PACE_COUNT
} CcLimbPace;

typedef enum CcLimbSupportState {
    CC_LIMB_SUPPORT_STABLE,
    CC_LIMB_SUPPORT_MARGINAL,
    CC_LIMB_SUPPORT_CONTROLLED_AIRBORNE,
    CC_LIMB_SUPPORT_UNSUPPORTED,
    CC_LIMB_SUPPORT_RECOVERING
} CcLimbSupportState;

typedef struct CcLimbSpec {
    CcLimbVec3 socket_local;
    CcLimbVec3 rest_contact_local;
    CcLimbVec3 bend_local;
    float segment_length[CC_LIMB_MAX_SEGMENTS];
    float phase_offset;
    int32_t segment_count;
} CcLimbSpec;

typedef struct CcLimbMorphology {
    const char *name;
    CcMorphologyPreset preset;
    CcLimbSpec limbs[CC_LIMB_MAX_COUNT];
    int32_t limb_count;
    int32_t minimum_supports;
    int32_t maximum_swings;
    float body_height;
    float duty_factor;
    float step_threshold;
    float step_height;
    float swing_seconds;
    float velocity_lead;
    float support_margin;
    bool dynamic_balance;
} CcLimbMorphology;

typedef struct CcLimbRuntime {
    CcLimbVec3 joints[CC_LIMB_MAX_JOINTS];
    CcLimbVec3 previous_joints[CC_LIMB_MAX_JOINTS];
    CcLimbVec3 planted_contact;
    CcLimbVec3 contact_start;
    CcLimbVec3 contact_target;
    CcLimbVec3 desired_contact;
    CcLimbVec3 contact_normal;
    CcLimbState state;
    float swing_progress;
    float health;
} CcLimbRuntime;

typedef struct CcLimbRig {
    CcLimbMorphology morphology;
    CcLimbMorphology walking_morphology;
    CcLimbRuntime limbs[CC_LIMB_MAX_COUNT];
    CcLimbVec3 support_center;
    CcLimbVec3 support_normal;
    CcLimbVec3 body_acceleration;
    CcLimbPace pace;
    CcLimbPace requested_pace;
    CcLimbSupportState support_state;
    float gait_phase;
    float pose_phase;
    float support_margin;
    float supported_height_offset;
    float traction;
    float drive_scale;
    float unsupported_seconds;
    float recovery_seconds;
    float control_authority;
    int32_t planted_count;
    int32_t swinging_count;
    int32_t active_pose_limb;
    bool initialized;
} CcLimbRig;

typedef bool (*CcLimbTerrainProbe)(void *context, CcLimbVec3 origin,
                                   float maximum_drop, CcLimbVec3 *point,
                                   CcLimbVec3 *normal);

bool CcLimbMorphologyFromPreset(CcLimbMorphology *morphology,
                                CcMorphologyPreset preset);
void CcLimbRigInit(CcLimbRig *rig, const CcLimbMorphology *morphology,
                   CcLimbVec3 body_position, float body_yaw,
                   CcLimbTerrainProbe probe, void *probe_context);
void CcLimbRigUpdate(CcLimbRig *rig, CcLimbVec3 body_position, float body_yaw,
                     CcLimbVec3 body_velocity, bool body_grounded,
                     float delta_time, CcLimbTerrainProbe probe,
                     void *probe_context);
void CcLimbRigSetHealth(CcLimbRig *rig, int32_t limb_index, float health);
void CcLimbRigPinContact(CcLimbRig *rig, int32_t limb_index,
                         CcLimbVec3 body_position, float body_yaw,
                         CcLimbVec3 contact, CcLimbVec3 normal);
void CcLimbRigEvaluateSupport(CcLimbRig *rig,
                              CcLimbVec3 body_position, float body_yaw,
                              bool grounded, float delta_time);
bool CcLimbRigRequestPace(CcLimbRig *rig, CcLimbPace pace);
float CcLimbChainLength(const CcLimbRig *rig, int32_t limb_index);
const char *CcLimbStateName(CcLimbState state);
const char *CcLimbPaceName(CcLimbPace pace);
const char *CcLimbSupportStateName(CcLimbSupportState state);

#endif
