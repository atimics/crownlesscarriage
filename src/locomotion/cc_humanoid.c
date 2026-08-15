#include "locomotion/cc_humanoid.h"

#include <math.h>
#include <stddef.h>

#define CC_HUMANOID_PI 3.14159265358979323846f
static const float CC_HUMANOID_STRIKE_DURATION = 1.20f;
static const float CC_HUMANOID_STRIKE_IMPACT_TIME = 0.61f;
static const float CC_HUMANOID_STRIKE_RECOVERY_DURATION = 0.32f;

typedef enum CcHumanoidRagdollNode {
    CC_RAGDOLL_PELVIS,
    CC_RAGDOLL_SPINE,
    CC_RAGDOLL_CHEST,
    CC_RAGDOLL_NECK,
    CC_RAGDOLL_HEAD,
    CC_RAGDOLL_LEFT_HIP,
    CC_RAGDOLL_LEFT_KNEE,
    CC_RAGDOLL_LEFT_ANKLE,
    CC_RAGDOLL_RIGHT_HIP,
    CC_RAGDOLL_RIGHT_KNEE,
    CC_RAGDOLL_RIGHT_ANKLE,
    CC_RAGDOLL_LEFT_SHOULDER,
    CC_RAGDOLL_LEFT_ELBOW,
    CC_RAGDOLL_LEFT_HAND,
    CC_RAGDOLL_RIGHT_SHOULDER,
    CC_RAGDOLL_RIGHT_ELBOW,
    CC_RAGDOLL_RIGHT_HAND,
    CC_RAGDOLL_LEFT_HEEL,
    CC_RAGDOLL_LEFT_TOE,
    CC_RAGDOLL_RIGHT_HEEL,
    CC_RAGDOLL_RIGHT_TOE,
    CC_RAGDOLL_NODE_COUNT
} CcHumanoidRagdollNode;

typedef struct CcHumanoidRagdollProbeContext {
    CcLimbTerrainProbe probe;
    void *probe_context;
    float fallback_ground;
    bool grounded;
} CcHumanoidRagdollProbeContext;

static CcLimbVec3 Add(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static CcLimbVec3 Subtract(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CcLimbVec3 Scale(CcLimbVec3 value, float scale)
{
    return (CcLimbVec3){value.x * scale, value.y * scale, value.z * scale};
}

static CcLimbVec3 Cross(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.y * b.z - a.z * b.y,
                        a.z * b.x - a.x * b.z,
                        a.x * b.y - a.y * b.x};
}

static CcBiomechVec3 ToBiomech(CcLimbVec3 value)
{
    return (CcBiomechVec3){value.x, value.y, value.z};
}

static CcLimbVec3 FromBiomech(CcBiomechVec3 value)
{
    return (CcLimbVec3){value.x, value.y, value.z};
}

static CcLimbVec3 Lerp(CcLimbVec3 a, CcLimbVec3 b, float amount)
{
    return Add(a, Scale(Subtract(b, a), amount));
}

static float LerpScalar(float a, float b, float amount)
{
    return a + (b - a) * amount;
}

static float Dot(CcLimbVec3 a, CcLimbVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float Length(CcLimbVec3 value)
{
    return sqrtf(Dot(value, value));
}

static float Clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static float Wrap01(float value)
{
    value = fmodf(value, 1.0f);
    return value < 0.0f ? value + 1.0f : value;
}

static float WrapAngle(float angle)
{
    while (angle > CC_HUMANOID_PI) angle -= 2.0f * CC_HUMANOID_PI;
    while (angle < -CC_HUMANOID_PI) angle += 2.0f * CC_HUMANOID_PI;
    return angle;
}

static float Smooth01(float amount)
{
    amount = Clamp(amount, 0.0f, 1.0f);
    return amount * amount * (3.0f - 2.0f * amount);
}

static float StrikePoseCurve(float guard, float chamber, float cut,
                             float phase, float chamber_end, float cut_end)
{
    if (phase < chamber_end) {
        return LerpScalar(guard, chamber,
                          Smooth01(phase / chamber_end));
    }
    if (phase < cut_end) {
        return LerpScalar(chamber, cut,
                          Smooth01((phase - chamber_end) /
                                   (cut_end - chamber_end)));
    }
    return LerpScalar(cut, guard,
                      Smooth01((phase - cut_end) / (1.0f - cut_end)));
}

static void SetAction(CcHumanoidGait *gait, CcHumanoidAction action)
{
    if (gait->action == action) return;
    gait->previous_action = gait->action;
    gait->action = action;
    gait->action_time = 0.0f;
    gait->action_blend = 0.0f;
}

static void AdvanceAction(CcHumanoidGait *gait, float delta_time)
{
    float previous_time = gait->action_time;
    gait->action_time += delta_time;
    gait->strike_recovery_seconds = fmaxf(
        0.0f, gait->strike_recovery_seconds - delta_time);
    gait->impact_response *= expf(-6.5f * delta_time);
    if (gait->impact_response < 0.001f) gait->impact_response = 0.0f;
    gait->action_blend += (1.0f - gait->action_blend) *
                          (1.0f - expf(-10.0f * delta_time));
    if (gait->action == CC_HUMANOID_ACTION_STRIKE) {
        const float impact_time = CC_HUMANOID_STRIKE_IMPACT_TIME;
        if (!gait->strike_impact_emitted && previous_time < impact_time &&
            gait->action_time >= impact_time) {
            gait->strike_impact_pending = true;
            gait->strike_impact_emitted = true;
        }
        if (gait->action_time >= CC_HUMANOID_STRIKE_DURATION) {
            bool return_to_guard = gait->guard_requested;
            SetAction(gait, return_to_guard ? CC_HUMANOID_ACTION_GUARD :
                                             CC_HUMANOID_ACTION_LOCOMOTION);
            gait->strike_recovery_seconds =
                CC_HUMANOID_STRIKE_RECOVERY_DURATION;
            if (return_to_guard) {
                /* The authored cut already ends in the guard pose. Preserve
                   the blend when guard is still explicitly requested. */
                gait->action_blend = 1.0f;
            }
        }
    }
}

static CcLimbVec3 Forward(float yaw)
{
    return (CcLimbVec3){sinf(yaw), 0.0f, cosf(yaw)};
}

static CcLimbVec3 Right(float yaw)
{
    return (CcLimbVec3){cosf(yaw), 0.0f, -sinf(yaw)};
}

static CcLimbVec3 NormalizeOr(CcLimbVec3 value, CcLimbVec3 fallback)
{
    float length = Length(value);
    return length > 0.00001f ? Scale(value, 1.0f / length) : fallback;
}

static bool AddDrivenJoint(CcBiomechMorphology *morphology,
                           CcHumanoidJoint expected, const char *joint_name,
                           const char *flexor_name, const char *extensor_name,
                           int32_t parent_bone, int32_t child_bone,
                           float rest, float lower, float upper, float inertia,
                           float passive_stiffness, float damping,
                           float ligament_stiffness, float maximum_force,
                           float moment_arm)
{
    int32_t joint = CcBiomechAddJoint(morphology, joint_name, parent_bone,
                                      child_bone, rest, lower, upper, inertia,
                                      passive_stiffness, damping,
                                      ligament_stiffness);
    if (joint != (int32_t)expected) return false;
    float width = (upper - lower) * 0.58f;
    if (CcBiomechAddMuscle(morphology, flexor_name, joint, moment_arm,
                           maximum_force, rest, width, 18.0f, 9.0f) < 0) {
        return false;
    }
    return CcBiomechAddMuscle(morphology, extensor_name, joint, -moment_arm,
                              maximum_force, rest, width, 18.0f, 9.0f) >= 0;
}

static bool ConfigureBiomechanicalBody(CcBiomechRig *rig)
{
    CcBiomechMorphology morphology;
    CcBiomechMorphologyInit(&morphology);
    int32_t pelvis = CcBiomechAddBone(&morphology, "pelvis", -1,
                                      0.24f, 10.5f, 0.50f);
    int32_t left_thigh = CcBiomechAddBone(&morphology, "left thigh", pelvis,
                                          0.465f, 7.2f, 0.43f);
    int32_t left_shin = CcBiomechAddBone(&morphology, "left shin", left_thigh,
                                         0.475f, 3.7f, 0.42f);
    int32_t left_foot = CcBiomechAddBone(&morphology, "left foot", left_shin,
                                         0.31f, 1.1f, 0.48f);
    int32_t right_thigh = CcBiomechAddBone(&morphology, "right thigh", pelvis,
                                           0.465f, 7.2f, 0.43f);
    int32_t right_shin = CcBiomechAddBone(&morphology, "right shin", right_thigh,
                                          0.475f, 3.7f, 0.42f);
    int32_t right_foot = CcBiomechAddBone(&morphology, "right foot", right_shin,
                                          0.31f, 1.1f, 0.48f);
    int32_t spine = CcBiomechAddBone(&morphology, "spine", pelvis,
                                     0.48f, 16.0f, 0.48f);
    int32_t chest = CcBiomechAddBone(&morphology, "chest", spine,
                                     0.31f, 12.0f, 0.46f);
    int32_t left_upper_arm = CcBiomechAddBone(&morphology, "left upper arm",
                                              chest, 0.34f, 2.1f, 0.44f);
    int32_t left_forearm = CcBiomechAddBone(&morphology, "left forearm",
                                            left_upper_arm, 0.35f, 1.4f, 0.43f);
    int32_t right_upper_arm = CcBiomechAddBone(&morphology, "right upper arm",
                                               chest, 0.34f, 2.1f, 0.44f);
    int32_t right_forearm = CcBiomechAddBone(&morphology, "right forearm",
                                             right_upper_arm, 0.35f, 1.4f, 0.43f);
    if (right_forearm < 0) return false;

    bool valid = true;
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_LEFT_HIP,
        "left hip", "left iliopsoas", "left gluteus", pelvis, left_thigh,
        0.0f, -1.25f, 1.10f, 0.18f, 1.4f, 1.1f, 42.0f, 1650.0f, 0.047f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_LEFT_KNEE,
        "left knee", "left hamstring", "left quadriceps", left_thigh,
        left_shin, 0.08f, 0.0f, 2.55f, 0.12f, 1.1f, 0.9f, 55.0f,
        1450.0f, 0.043f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_LEFT_ANKLE,
        "left ankle", "left tibialis", "left calf", left_shin, left_foot,
        0.0f, -0.72f, 0.68f, 0.055f, 1.8f, 0.65f, 38.0f, 850.0f, 0.038f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_RIGHT_HIP,
        "right hip", "right iliopsoas", "right gluteus", pelvis, right_thigh,
        0.0f, -1.25f, 1.10f, 0.18f, 1.4f, 1.1f, 42.0f, 1650.0f, 0.047f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_RIGHT_KNEE,
        "right knee", "right hamstring", "right quadriceps", right_thigh,
        right_shin, 0.08f, 0.0f, 2.55f, 0.12f, 1.1f, 0.9f, 55.0f,
        1450.0f, 0.043f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_RIGHT_ANKLE,
        "right ankle", "right tibialis", "right calf", right_shin,
        right_foot, 0.0f, -0.72f, 0.68f, 0.055f, 1.8f, 0.65f, 38.0f,
        850.0f, 0.038f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_SPINE_PITCH,
        "spine pitch", "abdominals", "erector spinae", pelvis, spine,
        0.015f, -0.34f, 0.42f, 0.24f, 3.4f, 1.5f, 28.0f, 920.0f, 0.042f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_SPINE_ROLL,
        "spine roll", "left oblique", "right oblique", pelvis, chest,
        0.0f, -0.28f, 0.28f, 0.22f, 3.0f, 1.4f, 26.0f, 760.0f, 0.038f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_SPINE_YAW,
        "spine yaw", "left rotator", "right rotator", pelvis, chest,
        0.0f, -0.40f, 0.40f, 0.20f, 2.8f, 1.3f, 25.0f, 720.0f, 0.036f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_LEFT_SHOULDER,
        "left shoulder", "left anterior deltoid", "left posterior deltoid",
        chest, left_upper_arm, 0.0f, -1.65f, 1.65f, 0.065f, 0.5f, 1.05f,
        12.0f, 520.0f, 0.030f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_LEFT_ELBOW,
        "left elbow", "left biceps", "left triceps", left_upper_arm,
        left_forearm, 0.18f, 0.0f, 2.60f, 0.035f, 0.45f, 0.78f, 14.0f,
        430.0f, 0.028f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_RIGHT_SHOULDER,
        "right shoulder", "right anterior deltoid", "right posterior deltoid",
        chest, right_upper_arm, 0.0f, -1.65f, 1.65f, 0.065f, 0.5f, 1.05f,
        12.0f, 520.0f, 0.030f);
    valid = valid && AddDrivenJoint(&morphology, CC_HUMANOID_RIGHT_ELBOW,
        "right elbow", "right biceps", "right triceps", right_upper_arm,
        right_forearm, 0.18f, 0.0f, 2.60f, 0.035f, 0.45f, 0.78f, 14.0f,
        430.0f, 0.028f);
    return valid && morphology.joint_count == CC_HUMANOID_JOINT_COUNT &&
           CcBiomechRigInit(rig, &morphology);
}

static CcLimbVec3 RagdollPosePoint(const CcHumanoidPose *pose,
                                   CcHumanoidRagdollNode node)
{
    switch (node) {
        case CC_RAGDOLL_PELVIS: return pose->pelvis;
        case CC_RAGDOLL_SPINE: return pose->spine;
        case CC_RAGDOLL_CHEST: return pose->chest;
        case CC_RAGDOLL_NECK: return pose->neck;
        case CC_RAGDOLL_HEAD: return pose->head;
        case CC_RAGDOLL_LEFT_HIP: return pose->hip[0];
        case CC_RAGDOLL_LEFT_KNEE: return pose->knee[0];
        case CC_RAGDOLL_LEFT_ANKLE: return pose->ankle[0];
        case CC_RAGDOLL_RIGHT_HIP: return pose->hip[1];
        case CC_RAGDOLL_RIGHT_KNEE: return pose->knee[1];
        case CC_RAGDOLL_RIGHT_ANKLE: return pose->ankle[1];
        case CC_RAGDOLL_LEFT_SHOULDER: return pose->shoulder[0];
        case CC_RAGDOLL_LEFT_ELBOW: return pose->elbow[0];
        case CC_RAGDOLL_LEFT_HAND: return pose->hand[0];
        case CC_RAGDOLL_RIGHT_SHOULDER: return pose->shoulder[1];
        case CC_RAGDOLL_RIGHT_ELBOW: return pose->elbow[1];
        case CC_RAGDOLL_RIGHT_HAND: return pose->hand[1];
        case CC_RAGDOLL_LEFT_HEEL: return pose->heel[0];
        case CC_RAGDOLL_LEFT_TOE: return pose->toe[0];
        case CC_RAGDOLL_RIGHT_HEEL: return pose->heel[1];
        case CC_RAGDOLL_RIGHT_TOE: return pose->toe[1];
        case CC_RAGDOLL_NODE_COUNT:
        default: return (CcLimbVec3){0};
    }
}

static bool AddRagdollPoseParticle(CcHumanoidGait *gait,
                                   CcHumanoidRagdollNode node,
                                   float inverse_mass, float radius)
{
    CcLimbVec3 current = RagdollPosePoint(&gait->pose, node);
    CcLimbVec3 previous = RagdollPosePoint(&gait->previous_pose, node);
    int32_t index = CcBiomechRagdollAddParticle(
        &gait->ragdoll, ToBiomech(current), inverse_mass, radius);
    if (index != (int32_t)node) return false;
    gait->ragdoll.particles[index].previous_position = ToBiomech(previous);
    return true;
}

static bool AddRagdollBone(CcHumanoidGait *gait,
                           CcHumanoidRagdollNode a,
                           CcHumanoidRagdollNode b, float compliance)
{
    return CcBiomechRagdollAddConstraint(&gait->ragdoll, (int32_t)a,
                                         (int32_t)b, compliance) >= 0;
}

static bool ActivateRagdoll(CcHumanoidGait *gait)
{
    SetAction(gait, CC_HUMANOID_ACTION_FALL);
    CcBiomechRagdollInit(&gait->ragdoll);
    bool valid = true;
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_PELVIS,
                                             0.075f, 0.14f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_SPINE,
                                             0.085f, 0.12f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_CHEST,
                                             0.060f, 0.20f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_NECK,
                                             0.16f, 0.08f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_HEAD,
                                             0.18f, 0.18f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_HIP,
                                             0.13f, 0.09f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_KNEE,
                                             0.22f, 0.075f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_ANKLE,
                                             0.36f, 0.075f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_HIP,
                                             0.13f, 0.09f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_KNEE,
                                             0.22f, 0.075f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_ANKLE,
                                             0.36f, 0.075f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_SHOULDER,
                                             0.16f, 0.09f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_ELBOW,
                                             0.28f, 0.065f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_HAND,
                                             0.46f, 0.055f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_SHOULDER,
                                             0.16f, 0.09f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_ELBOW,
                                             0.28f, 0.065f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_HAND,
                                             0.46f, 0.055f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_HEEL,
                                             1.25f, 0.012f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_LEFT_TOE,
                                             1.25f, 0.010f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_HEEL,
                                             1.25f, 0.012f);
    valid = valid && AddRagdollPoseParticle(gait, CC_RAGDOLL_RIGHT_TOE,
                                             1.25f, 0.010f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_PELVIS,
                                    CC_RAGDOLL_SPINE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_SPINE,
                                    CC_RAGDOLL_CHEST, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_CHEST,
                                    CC_RAGDOLL_NECK, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_NECK,
                                    CC_RAGDOLL_HEAD, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_PELVIS,
                                    CC_RAGDOLL_LEFT_HIP, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_HIP,
                                    CC_RAGDOLL_LEFT_KNEE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_KNEE,
                                    CC_RAGDOLL_LEFT_ANKLE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_PELVIS,
                                    CC_RAGDOLL_RIGHT_HIP, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_HIP,
                                    CC_RAGDOLL_RIGHT_KNEE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_KNEE,
                                    CC_RAGDOLL_RIGHT_ANKLE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_CHEST,
                                    CC_RAGDOLL_LEFT_SHOULDER, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_SHOULDER,
                                    CC_RAGDOLL_LEFT_ELBOW, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_ELBOW,
                                    CC_RAGDOLL_LEFT_HAND, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_CHEST,
                                    CC_RAGDOLL_RIGHT_SHOULDER, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_SHOULDER,
                                    CC_RAGDOLL_RIGHT_ELBOW, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_ELBOW,
                                    CC_RAGDOLL_RIGHT_HAND, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_HIP,
                                    CC_RAGDOLL_RIGHT_HIP, 0.00001f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_SHOULDER,
                                    CC_RAGDOLL_RIGHT_SHOULDER, 0.00001f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_PELVIS,
                                    CC_RAGDOLL_CHEST, 0.00002f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_HIP,
                                    CC_RAGDOLL_RIGHT_SHOULDER, 0.00004f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_HIP,
                                    CC_RAGDOLL_LEFT_SHOULDER, 0.00004f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_ANKLE,
                                    CC_RAGDOLL_LEFT_HEEL, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_ANKLE,
                                    CC_RAGDOLL_LEFT_TOE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_LEFT_HEEL,
                                    CC_RAGDOLL_LEFT_TOE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_ANKLE,
                                    CC_RAGDOLL_RIGHT_HEEL, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_ANKLE,
                                    CC_RAGDOLL_RIGHT_TOE, 0.0f);
    valid = valid && AddRagdollBone(gait, CC_RAGDOLL_RIGHT_HEEL,
                                    CC_RAGDOLL_RIGHT_TOE, 0.0f);
    if (!valid) {
        CcBiomechRagdollInit(&gait->ragdoll);
        return false;
    }
    gait->ragdoll.active = true;
    gait->ragdoll_time = 0.0f;
    gait->ragdoll_settled_time = 0.0f;
    gait->recovery_time = 0.0f;
    gait->recovery_error = 0.0f;
    gait->recovery_speed = 0.0f;
    gait->recovering = false;
    gait->ground_reaction = (CcLimbVec3){0};
    return true;
}

static bool ProbeRagdollCollision(void *raw_context,
                                  CcBiomechVec3 previous_position,
                                  CcBiomechVec3 position, float radius,
                                  CcBiomechVec3 *corrected_position,
                                  CcBiomechVec3 *surface_normal)
{
    CcHumanoidRagdollProbeContext *context = raw_context;
    CcLimbVec3 point = {position.x, context->fallback_ground, position.z};
    CcLimbVec3 normal = {0.0f, 1.0f, 0.0f};
    if (context->probe != NULL) {
        CcLimbVec3 origin = {position.x,
                             fmaxf(position.y + 1.0f,
                                   context->fallback_ground + 2.5f),
                             position.z};
        if (!context->probe(context->probe_context, origin, 6.0f,
                            &point, &normal)) return false;
    }
    if (!context->grounded &&
        point.y > context->fallback_ground + 0.015f) {
        return false;
    }
    if (position.y - radius >= point.y) return false;
    float top_tolerance = fmaxf(0.055f, radius * 0.35f);
    if (previous_position.y - radius < point.y - top_tolerance) {
        return false;
    }
    *corrected_position = position;
    corrected_position->y = point.y + radius;
    *surface_normal = ToBiomech(normal);
    return true;
}

static void ResolveRagdollPose(CcHumanoidGait *gait, float body_yaw)
{
    CcBiomechRagdoll *ragdoll = &gait->ragdoll;
    gait->pose.pelvis = FromBiomech(
        ragdoll->particles[CC_RAGDOLL_PELVIS].position);
    gait->pose.spine = FromBiomech(
        ragdoll->particles[CC_RAGDOLL_SPINE].position);
    gait->pose.chest = FromBiomech(
        ragdoll->particles[CC_RAGDOLL_CHEST].position);
    gait->pose.neck = FromBiomech(
        ragdoll->particles[CC_RAGDOLL_NECK].position);
    gait->pose.head = FromBiomech(
        ragdoll->particles[CC_RAGDOLL_HEAD].position);
    static const CcHumanoidRagdollNode hips[2] = {
        CC_RAGDOLL_LEFT_HIP, CC_RAGDOLL_RIGHT_HIP
    };
    static const CcHumanoidRagdollNode knees[2] = {
        CC_RAGDOLL_LEFT_KNEE, CC_RAGDOLL_RIGHT_KNEE
    };
    static const CcHumanoidRagdollNode ankles[2] = {
        CC_RAGDOLL_LEFT_ANKLE, CC_RAGDOLL_RIGHT_ANKLE
    };
    static const CcHumanoidRagdollNode shoulders[2] = {
        CC_RAGDOLL_LEFT_SHOULDER, CC_RAGDOLL_RIGHT_SHOULDER
    };
    static const CcHumanoidRagdollNode elbows[2] = {
        CC_RAGDOLL_LEFT_ELBOW, CC_RAGDOLL_RIGHT_ELBOW
    };
    static const CcHumanoidRagdollNode hands[2] = {
        CC_RAGDOLL_LEFT_HAND, CC_RAGDOLL_RIGHT_HAND
    };
    static const CcHumanoidRagdollNode heels[2] = {
        CC_RAGDOLL_LEFT_HEEL, CC_RAGDOLL_RIGHT_HEEL
    };
    static const CcHumanoidRagdollNode toes[2] = {
        CC_RAGDOLL_LEFT_TOE, CC_RAGDOLL_RIGHT_TOE
    };
    CcLimbVec3 forward = Forward(body_yaw);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        gait->pose.hip[leg] = FromBiomech(ragdoll->particles[hips[leg]].position);
        gait->pose.knee[leg] = FromBiomech(ragdoll->particles[knees[leg]].position);
        gait->pose.ankle[leg] = FromBiomech(
            ragdoll->particles[ankles[leg]].position);
        gait->pose.heel[leg] = FromBiomech(
            ragdoll->particles[heels[leg]].position);
        gait->pose.toe[leg] = FromBiomech(
            ragdoll->particles[toes[leg]].position);
        gait->pose.ball[leg] = Lerp(gait->pose.heel[leg],
                                    gait->pose.toe[leg], 0.77419355f);
        CcLimbVec3 upper = NormalizeOr(Subtract(gait->pose.knee[leg],
                                                gait->pose.hip[leg]), forward);
        CcLimbVec3 lower = NormalizeOr(Subtract(gait->pose.ankle[leg],
                                                gait->pose.knee[leg]), forward);
        gait->pose.knee_flexion[leg] = acosf(Clamp(Dot(upper, lower),
                                                   -1.0f, 1.0f));
        gait->pose.foot_pitch[leg] = 0.0f;
        gait->feet[leg].contact = CC_HUMANOID_CONTACT_AIR;
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        gait->pose.shoulder[arm] = FromBiomech(
            ragdoll->particles[shoulders[arm]].position);
        gait->pose.elbow[arm] = FromBiomech(
            ragdoll->particles[elbows[arm]].position);
        gait->pose.hand[arm] = FromBiomech(
            ragdoll->particles[hands[arm]].position);
    }
    gait->pose.pelvis_yaw = 0.0f;
    gait->pose.pelvis_roll = 0.0f;
    gait->pose.pelvis_pitch = 0.0f;
    gait->pose.chest_yaw = 0.0f;
    gait->pose.chest_roll = 0.0f;
    gait->pose.chest_pitch = 0.0f;
    gait->body.root.position =
        ragdoll->particles[CC_RAGDOLL_PELVIS].position;
    gait->body.root.velocity = CcBiomechRagdollParticleVelocity(
        ragdoll, CC_RAGDOLL_PELVIS, gait->last_delta_time);
    gait->root_velocity = FromBiomech(gait->body.root.velocity);
    gait->planted_count = 0;
}

static float MeanRagdollSpeed(const CcHumanoidGait *gait)
{
    float total = 0.0f;
    for (int32_t particle = 0;
         particle < gait->ragdoll.particle_count; ++particle) {
        CcBiomechVec3 velocity = CcBiomechRagdollParticleVelocity(
            &gait->ragdoll, particle, gait->last_delta_time);
        float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y +
                            velocity.z * velocity.z);
        total += speed;
    }
    return gait->ragdoll.particle_count > 0 ?
           total / (float)gait->ragdoll.particle_count : 0.0f;
}

static int32_t RagdollContactCount(const CcHumanoidGait *gait,
                                   float support_height)
{
    int32_t count = 0;
    for (int32_t particle = 0;
         particle < gait->ragdoll.particle_count; ++particle) {
        const CcBiomechRagdollParticle *body =
            &gait->ragdoll.particles[particle];
        float contact_height = body->position.y - body->radius;
        if (body->collided &&
            fabsf(contact_height - support_height) < 0.20f) count += 1;
    }
    return count;
}

static CcLimbVec3 RecoveryBracePoint(const CcHumanoidGait *gait,
                                     CcHumanoidRagdollNode node)
{
    CcLimbVec3 base = gait->recovery_origin;
    CcLimbVec3 forward = Forward(gait->recovery_yaw);
    CcLimbVec3 right = Right(gait->recovery_yaw);
    float ground = fminf(gait->recovery_target_pose.heel[0].y,
                         gait->recovery_target_pose.heel[1].y);
    switch (node) {
        case CC_RAGDOLL_PELVIS:
            return Add(base, Add(Scale(forward, -0.05f),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.38f,
                                              0.0f}));
        case CC_RAGDOLL_SPINE:
            return Add(base, Add(Scale(forward, 0.03f),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.53f,
                                              0.0f}));
        case CC_RAGDOLL_CHEST:
            return Add(base, Add(Scale(forward, 0.15f),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.70f,
                                              0.0f}));
        case CC_RAGDOLL_NECK:
            return Add(base, Add(Scale(forward, 0.21f),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.86f,
                                              0.0f}));
        case CC_RAGDOLL_HEAD:
            return Add(base, Add(Scale(forward, 0.24f),
                                 (CcLimbVec3){0.0f, ground - base.y + 1.05f,
                                              0.0f}));
        case CC_RAGDOLL_LEFT_HIP:
        case CC_RAGDOLL_RIGHT_HIP: {
            float side = node == CC_RAGDOLL_LEFT_HIP ? -0.13f : 0.13f;
            return Add(base, Add(Add(Scale(right, side),
                                      Scale(forward, -0.04f)),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.34f,
                                              0.0f}));
        }
        case CC_RAGDOLL_LEFT_KNEE:
        case CC_RAGDOLL_RIGHT_KNEE: {
            float side = node == CC_RAGDOLL_LEFT_KNEE ? -0.16f : 0.16f;
            return Add(base, Add(Add(Scale(right, side),
                                      Scale(forward, 0.23f)),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.075f,
                                              0.0f}));
        }
        case CC_RAGDOLL_LEFT_ANKLE:
        case CC_RAGDOLL_RIGHT_ANKLE: {
            float side = node == CC_RAGDOLL_LEFT_ANKLE ? -0.16f : 0.16f;
            return Add(base, Add(Add(Scale(right, side),
                                      Scale(forward, -0.20f)),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.075f,
                                              0.0f}));
        }
        case CC_RAGDOLL_LEFT_SHOULDER:
        case CC_RAGDOLL_RIGHT_SHOULDER: {
            float side = node == CC_RAGDOLL_LEFT_SHOULDER ? -0.23f : 0.23f;
            return Add(base, Add(Add(Scale(right, side),
                                      Scale(forward, 0.15f)),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.70f,
                                              0.0f}));
        }
        case CC_RAGDOLL_LEFT_ELBOW:
        case CC_RAGDOLL_RIGHT_ELBOW: {
            float side = node == CC_RAGDOLL_LEFT_ELBOW ? -0.29f : 0.29f;
            return Add(base, Add(Add(Scale(right, side),
                                      Scale(forward, 0.34f)),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.34f,
                                              0.0f}));
        }
        case CC_RAGDOLL_LEFT_HAND:
        case CC_RAGDOLL_RIGHT_HAND: {
            float side = node == CC_RAGDOLL_LEFT_HAND ? -0.30f : 0.30f;
            return Add(base, Add(Add(Scale(right, side),
                                      Scale(forward, 0.46f)),
                                 (CcLimbVec3){0.0f, ground - base.y + 0.055f,
                                              0.0f}));
        }
        case CC_RAGDOLL_LEFT_HEEL:
        case CC_RAGDOLL_RIGHT_HEEL: {
            CcHumanoidRagdollNode ankle = node == CC_RAGDOLL_LEFT_HEEL ?
                CC_RAGDOLL_LEFT_ANKLE : CC_RAGDOLL_RIGHT_ANKLE;
            CcLimbVec3 target = RecoveryBracePoint(gait, ankle);
            target = Add(target, Scale(forward, -0.11f));
            target.y = ground + 0.012f;
            return target;
        }
        case CC_RAGDOLL_LEFT_TOE:
        case CC_RAGDOLL_RIGHT_TOE: {
            CcHumanoidRagdollNode ankle = node == CC_RAGDOLL_LEFT_TOE ?
                CC_RAGDOLL_LEFT_ANKLE : CC_RAGDOLL_RIGHT_ANKLE;
            CcLimbVec3 target = RecoveryBracePoint(gait, ankle);
            target = Add(target, Scale(forward, 0.20f));
            target.y = ground + 0.010f;
            return target;
        }
        case CC_RAGDOLL_NODE_COUNT:
        default: return base;
    }
}

static CcLimbVec3 RecoveryKneelPoint(const CcHumanoidGait *gait,
                                     CcHumanoidRagdollNode node)
{
    CcLimbVec3 standing = RagdollPosePoint(&gait->recovery_target_pose, node);
    CcLimbVec3 forward = Forward(gait->recovery_yaw);
    float ground = fminf(gait->recovery_target_pose.heel[0].y,
                         gait->recovery_target_pose.heel[1].y);
    switch (node) {
        case CC_RAGDOLL_PELVIS: standing.y -= 0.42f; break;
        case CC_RAGDOLL_SPINE: standing.y -= 0.34f; break;
        case CC_RAGDOLL_CHEST: standing.y -= 0.27f; break;
        case CC_RAGDOLL_NECK: standing.y -= 0.20f; break;
        case CC_RAGDOLL_HEAD: standing.y -= 0.14f; break;
        case CC_RAGDOLL_LEFT_HIP:
        case CC_RAGDOLL_RIGHT_HIP: standing.y -= 0.40f; break;
        case CC_RAGDOLL_LEFT_KNEE:
        case CC_RAGDOLL_RIGHT_KNEE:
            standing = Add(standing, Scale(forward, 0.18f));
            standing.y = ground + 0.075f;
            break;
        case CC_RAGDOLL_LEFT_ANKLE:
        case CC_RAGDOLL_RIGHT_ANKLE:
            standing = Add(standing, Scale(forward, -0.24f));
            standing.y = ground + 0.075f;
            break;
        case CC_RAGDOLL_LEFT_SHOULDER:
        case CC_RAGDOLL_RIGHT_SHOULDER: standing.y -= 0.25f; break;
        case CC_RAGDOLL_LEFT_ELBOW:
        case CC_RAGDOLL_RIGHT_ELBOW:
            standing = Add(standing, Scale(forward, 0.28f));
            standing.y = ground + 0.42f;
            break;
        case CC_RAGDOLL_LEFT_HAND:
        case CC_RAGDOLL_RIGHT_HAND:
            standing = Add(standing, Scale(forward, 0.38f));
            standing.y = ground + 0.055f;
            break;
        case CC_RAGDOLL_LEFT_HEEL:
        case CC_RAGDOLL_RIGHT_HEEL: {
            CcHumanoidRagdollNode ankle = node == CC_RAGDOLL_LEFT_HEEL ?
                CC_RAGDOLL_LEFT_ANKLE : CC_RAGDOLL_RIGHT_ANKLE;
            standing = RecoveryKneelPoint(gait, ankle);
            standing = Add(standing, Scale(forward, -0.11f));
            standing.y = ground + 0.012f;
            break;
        }
        case CC_RAGDOLL_LEFT_TOE:
        case CC_RAGDOLL_RIGHT_TOE: {
            CcHumanoidRagdollNode ankle = node == CC_RAGDOLL_LEFT_TOE ?
                CC_RAGDOLL_LEFT_ANKLE : CC_RAGDOLL_RIGHT_ANKLE;
            standing = RecoveryKneelPoint(gait, ankle);
            standing = Add(standing, Scale(forward, 0.20f));
            standing.y = ground + 0.010f;
            break;
        }
        case CC_RAGDOLL_NODE_COUNT:
        default: break;
    }
    return standing;
}

static CcLimbVec3 RecoveryMotorTarget(const CcHumanoidGait *gait,
                                      CcHumanoidRagdollNode node)
{
    CcLimbVec3 start = RagdollPosePoint(&gait->recovery_start_pose, node);
    CcLimbVec3 brace = RecoveryBracePoint(gait, node);
    CcLimbVec3 kneel = RecoveryKneelPoint(gait, node);
    CcLimbVec3 standing = RagdollPosePoint(&gait->recovery_target_pose, node);
    if (gait->recovery_time < 0.65f) {
        return Lerp(start, brace, Smooth01(gait->recovery_time / 0.65f));
    }
    if (gait->recovery_time < 1.50f) {
        return Lerp(brace, kneel,
                    Smooth01((gait->recovery_time - 0.65f) / 0.85f));
    }
    return Lerp(kneel, standing,
                Smooth01((gait->recovery_time - 1.50f) / 1.35f));
}

static void DriveRagdollRecovery(CcHumanoidGait *gait, float delta_time)
{
    float gain = gait->recovery_time < 0.65f ? 23.0f :
                 gait->recovery_time < 1.50f ? 31.0f : 42.0f;
    float damping = gait->recovery_time < 1.50f ? 7.2f : 9.0f;
    for (int32_t node = 0; node < gait->ragdoll.particle_count; ++node) {
        CcBiomechRagdollParticle *particle = &gait->ragdoll.particles[node];
        CcLimbVec3 target = RecoveryMotorTarget(
            gait, (CcHumanoidRagdollNode)node);
        CcBiomechVec3 velocity = CcBiomechRagdollParticleVelocity(
            &gait->ragdoll, node, delta_time);
        CcBiomechVec3 acceleration = {
            (target.x - particle->position.x) * gain - velocity.x * damping -
                gait->ragdoll.gravity.x,
            (target.y - particle->position.y) * gain - velocity.y * damping -
                gait->ragdoll.gravity.y,
            (target.z - particle->position.z) * gain - velocity.z * damping -
                gait->ragdoll.gravity.z
        };
        float magnitude = sqrtf(acceleration.x * acceleration.x +
                                acceleration.y * acceleration.y +
                                acceleration.z * acceleration.z);
        if (magnitude > 48.0f) {
            float scale = 48.0f / magnitude;
            acceleration.x *= scale;
            acceleration.y *= scale;
            acceleration.z *= scale;
        }
        particle->acceleration.x += acceleration.x;
        particle->acceleration.y += acceleration.y;
        particle->acceleration.z += acceleration.z;
    }
}

static float RagdollRecoveryError(const CcHumanoidGait *gait)
{
    float maximum_error = 0.0f;
    for (int32_t node = 0; node < gait->ragdoll.particle_count; ++node) {
        CcLimbVec3 target = RagdollPosePoint(
            &gait->recovery_target_pose, (CcHumanoidRagdollNode)node);
        CcLimbVec3 actual = FromBiomech(gait->ragdoll.particles[node].position);
        CcLimbVec3 error = Subtract(target, actual);
        maximum_error = fmaxf(maximum_error, Length(error));
    }
    return maximum_error;
}

static void BeginRagdollRecovery(CcHumanoidGait *gait,
                                 CcLimbVec3 body_position, float body_yaw,
                                 CcLimbTerrainProbe probe,
                                 void *probe_context)
{
    SetAction(gait, CC_HUMANOID_ACTION_RECOVER);
    CcHumanoidGait standing;
    CcHumanoidGaitInit(&standing, body_position, body_yaw,
                       probe, probe_context);
    gait->recovery_start_pose = gait->pose;
    gait->recovery_target_pose = standing.pose;
    gait->recovery_origin = body_position;
    gait->recovery_yaw = body_yaw;
    gait->recovery_time = 0.0f;
    gait->recovering = true;
    gait->ragdoll.driven = true;
    gait->ragdoll.damping = 0.055f;
    gait->ragdoll.collision_friction = 0.58f;
    gait->ragdoll.contact_damping = 0.08f;
}

static bool StepRagdoll(CcHumanoidGait *gait, CcLimbVec3 body_position,
                        float body_yaw, bool grounded, float delta_time,
                        CcLimbTerrainProbe probe, void *probe_context)
{
    CcHumanoidRagdollProbeContext collision = {
        .probe = probe,
        .probe_context = probe_context,
        .fallback_ground = body_position.y,
        .grounded = grounded
    };
    if (gait->recovering) DriveRagdollRecovery(gait, delta_time);
    CcBiomechRagdollStep(&gait->ragdoll, delta_time, 12,
                         ProbeRagdollCollision, &collision);
    gait->ragdoll_time += delta_time;
    if (gait->recovering) gait->recovery_time += delta_time;
    ResolveRagdollPose(gait, body_yaw);
    if (gait->recovering) {
        gait->recovery_error = RagdollRecoveryError(gait);
        gait->recovery_speed = MeanRagdollSpeed(gait);
        if (gait->recovery_time > 2.85f && gait->recovery_error < 0.035f &&
            gait->recovery_speed < 0.22f) {
            CcHumanoidGaitInit(gait, body_position, body_yaw,
                               probe, probe_context);
            return false;
        }
        return true;
    }
    int32_t contact_count = RagdollContactCount(gait, body_position.y);
    float mean_speed = MeanRagdollSpeed(gait);
    if (grounded && contact_count >= 3 && mean_speed < 0.72f) {
        gait->ragdoll_settled_time += delta_time;
    } else {
        gait->ragdoll_settled_time = 0.0f;
    }
    if (grounded && gait->ragdoll_time > 1.20f &&
        ((contact_count >= 3 && gait->ragdoll_settled_time > 0.42f) ||
         (contact_count >= 3 && gait->ragdoll_time > 3.0f))) {
        BeginRagdollRecovery(gait, body_position, body_yaw,
                             probe, probe_context);
    }
    return true;
}

static void SpringStep(CcHumanoidSpring *spring, float target,
                       float frequency, float damping, float delta_time)
{
    float acceleration = (target - spring->value) * frequency * frequency -
                         spring->velocity * 2.0f * damping * frequency;
    spring->velocity += acceleration * delta_time;
    spring->value += spring->velocity * delta_time;
}

static CcLimbVec3 ProbeGround(CcLimbVec3 desired, CcLimbVec3 body_position,
                              CcLimbTerrainProbe probe, void *probe_context,
                              CcLimbVec3 *normal)
{
    if (probe == NULL) {
        desired.y = body_position.y;
        *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
        return desired;
    }
    CcLimbVec3 origin = desired;
    origin.y = body_position.y + 1.45f;
    CcLimbVec3 point = desired;
    if (probe(probe_context, origin, 3.0f, &point, normal)) return point;
    desired.y = body_position.y;
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return desired;
}

static CcHumanoidContact ContactForPhase(float local_phase)
{
    if (local_phase < 0.08f) return CC_HUMANOID_CONTACT_HEEL;
    if (local_phase < 0.48f) return CC_HUMANOID_CONTACT_FLAT;
    if (local_phase < 0.62f) return CC_HUMANOID_CONTACT_TOE;
    return CC_HUMANOID_CONTACT_SWING;
}

static float ContactPitch(CcHumanoidContact contact, float local_phase)
{
    switch (contact) {
        case CC_HUMANOID_CONTACT_HEEL:
            return 0.20f * (1.0f - Smooth01(local_phase / 0.08f));
        case CC_HUMANOID_CONTACT_TOE:
            return -0.32f * Smooth01((local_phase - 0.48f) / 0.14f);
        case CC_HUMANOID_CONTACT_SWING: {
            float swing = Clamp((local_phase - 0.62f) / 0.38f, 0.0f, 1.0f);
            float leave_toe = -0.18f * (1.0f - Smooth01(swing / 0.24f));
            float dorsiflex = 0.11f * sinf(swing * CC_HUMANOID_PI);
            float prepare_heel = 0.13f * Smooth01((swing - 0.67f) / 0.33f);
            return leave_toe + dorsiflex + prepare_heel;
        }
        case CC_HUMANOID_CONTACT_AIR:
            return 0.08f;
        case CC_HUMANOID_CONTACT_FLAT:
        default:
            return 0.0f;
    }
}

static CcLimbVec3 PlanFootTarget(const CcHumanoidGait *gait, int32_t leg,
                                 CcLimbVec3 body_position,
                                 CcLimbTerrainProbe probe, void *probe_context,
                                 CcLimbVec3 *normal)
{
    CcLimbVec3 forward = Forward(gait->travel_yaw);
    CcLimbVec3 right = Right(gait->travel_yaw);
    float side = leg == 0 ? -1.0f : 1.0f;
    float preview_seconds = (0.38f + 0.31f) /
                            fmaxf(0.70f, gait->cadence);
    float lead = Clamp(gait->speed.value * preview_seconds, 0.16f, 0.88f);
    CcLimbVec3 desired = Add(body_position, Scale(forward, lead));
    desired = Add(desired, Scale(right, side * 0.135f));
    return ProbeGround(desired, body_position, probe, probe_context, normal);
}

static void BeginSwing(CcHumanoidGait *gait, int32_t leg,
                       CcLimbVec3 body_position, CcLimbTerrainProbe probe,
                       void *probe_context)
{
    CcHumanoidFoot *foot = &gait->feet[leg];
    foot->swing_start = foot->current_point;
    foot->swing_target = PlanFootTarget(gait, leg, body_position, probe,
                                       probe_context, &foot->normal);
    foot->contact = CC_HUMANOID_CONTACT_SWING;
}

static void UpdateFoot(CcHumanoidGait *gait, int32_t leg, float old_phase,
                       float new_phase, CcLimbVec3 body_position,
                       bool grounded, float delta_time,
                       CcLimbTerrainProbe probe, void *probe_context)
{
    CcHumanoidFoot *foot = &gait->feet[leg];
    float offset = leg == 0 ? 0.0f : 0.5f;
    float old_local = Wrap01(old_phase + offset);
    float local = Wrap01(new_phase + offset);
    bool crossed_toe_off = old_local < 0.62f && local >= 0.62f;
    bool crossed_heel_strike = local < old_local;

    if (!grounded) {
        foot->contact = CC_HUMANOID_CONTACT_AIR;
        CcLimbVec3 right = Right(gait->travel_yaw);
        CcLimbVec3 forward = Forward(gait->travel_yaw);
        float side = leg == 0 ? -1.0f : 1.0f;
        CcLimbVec3 pelvis = FromBiomech(gait->body.root.position);
        CcLimbVec3 tucked = Add(pelvis, Scale(right, side * 0.15f));
        tucked = Add(tucked, Scale(forward, leg == 0 ? 0.08f : -0.03f));
        tucked.y -= leg == 0 ? 0.66f : 0.71f;
        float smoothing = 1.0f - expf(-9.0f * delta_time);
        foot->current_point = Lerp(foot->current_point, tucked, smoothing);
        foot->local_phase = local;
        SpringStep(&foot->pitch, ContactPitch(foot->contact, local),
                   10.0f, 0.82f, delta_time);
        return;
    }

    if (crossed_toe_off) BeginSwing(gait, leg, body_position, probe, probe_context);
    if (crossed_heel_strike) {
        foot->planted_point = foot->swing_target;
        foot->current_point = foot->planted_point;
    }

    foot->contact = ContactForPhase(local);
    if (foot->contact == CC_HUMANOID_CONTACT_SWING) {
        float swing = Clamp((local - 0.62f) / 0.38f, 0.0f, 1.0f);
        CcLimbVec3 revised_normal = foot->normal;
        CcLimbVec3 revised_target = PlanFootTarget(gait, leg, body_position,
                                                   probe, probe_context,
                                                   &revised_normal);
        float revision_window = 1.0f - Smooth01((swing - 0.68f) / 0.28f);
        float revision = (1.0f - expf(-11.0f * delta_time)) * revision_window;
        foot->swing_target = Lerp(foot->swing_target, revised_target, revision);
        foot->normal = Lerp(foot->normal, revised_normal, revision);
        float travel = Smooth01(swing);
        foot->current_point = Lerp(foot->swing_start, foot->swing_target, travel);
        float lift = sinf(swing * CC_HUMANOID_PI) *
                     (0.105f + Clamp(gait->speed.value / 1.5f, 0.0f, 1.0f) * 0.035f);
        foot->current_point.y += lift;
    } else {
        foot->current_point = foot->planted_point;
    }
    foot->local_phase = local;
    float pitch_frequency = foot->contact == CC_HUMANOID_CONTACT_FLAT ? 18.0f : 12.0f;
    SpringStep(&foot->pitch, ContactPitch(foot->contact, local),
               pitch_frequency, 0.84f, delta_time);
}

static CcLimbVec3 SolveKnee(CcLimbVec3 hip, CcLimbVec3 ankle,
                            CcLimbVec3 pole, float upper, float lower,
                            float *flexion)
{
    CcLimbVec3 delta = Subtract(ankle, hip);
    float raw_distance = Length(delta);
    CcLimbVec3 direction = NormalizeOr(delta, (CcLimbVec3){0.0f, -1.0f, 0.0f});
    float distance = Clamp(raw_distance, fabsf(upper - lower) + 0.001f,
                           upper + lower - 0.001f);
    pole = Subtract(pole, Scale(direction, Dot(pole, direction)));
    pole = NormalizeOr(pole, (CcLimbVec3){0.0f, 0.0f, 1.0f});
    float along = (upper * upper - lower * lower + distance * distance) /
                  (2.0f * distance);
    float height = sqrtf(fmaxf(0.0f, upper * upper - along * along));
    float cosine = Clamp((upper * upper + lower * lower - distance * distance) /
                         (2.0f * upper * lower), -1.0f, 1.0f);
    *flexion = CC_HUMANOID_PI - acosf(cosine);
    return Add(Add(hip, Scale(direction, along)), Scale(pole, height));
}

static void ResolveFootGeometry(CcHumanoidGait *gait, int32_t leg,
                                CcLimbVec3 forward)
{
    CcHumanoidFoot *foot = &gait->feet[leg];
    float pitch = foot->pitch.value;
    float sine = sinf(pitch);
    float cosine = cosf(pitch);
    CcLimbVec3 center = foot->current_point;
    if (foot->contact == CC_HUMANOID_CONTACT_HEEL) {
        center.y += 0.11f * sine;
    } else if (foot->contact == CC_HUMANOID_CONTACT_TOE) {
        center.y -= 0.20f * sine;
    }
    gait->pose.heel[leg] = Add(center, Scale(forward, -0.11f * cosine));
    gait->pose.heel[leg].y += -0.11f * sine;
    gait->pose.ball[leg] = Add(center, Scale(forward, 0.13f * cosine));
    gait->pose.ball[leg].y += 0.13f * sine;
    gait->pose.toe[leg] = Add(center, Scale(forward, 0.20f * cosine));
    gait->pose.toe[leg].y += 0.20f * sine;
    gait->pose.ankle[leg] = center;
    gait->pose.ankle[leg].y += 0.085f;
    gait->pose.foot_pitch[leg] = pitch;
}

void CcHumanoidGaitResolvePose(CcHumanoidGait *gait,
                               CcLimbVec3 body_position, float body_yaw)
{
    if (gait == NULL || !gait->initialized) return;
    if (gait->ragdoll.active) {
        ResolveRagdollPose(gait, body_yaw);
        return;
    }
    (void)body_position;
    CcLimbVec3 forward = Forward(body_yaw);
    CcLimbVec3 right = Right(body_yaw);
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    gait->pose.pelvis = Add(FromBiomech(gait->body.root.position),
                            Scale(right, gait->pelvis_sway.value));
    gait->pose.pelvis = Add(gait->pose.pelvis,
                            Scale(forward, 0.012f * sinf(gait->phase * 4.0f *
                                                       CC_HUMANOID_PI)));
    gait->pose.pelvis_yaw = gait->pelvis_yaw.value;
    gait->pose.pelvis_roll = gait->pelvis_roll.value;
    float forward_acceleration = gait->body.root.acceleration.x * forward.x +
                                 gait->body.root.acceleration.z * forward.z;
    gait->pose.pelvis_pitch = Clamp(
        forward_acceleration * 0.018f +
        Clamp(gait->speed.value / 1.5f, 0.0f, 1.0f) * 0.022f,
        -0.075f, 0.11f);

    float pelvis_frame_yaw = body_yaw + gait->pose.pelvis_yaw;
    CcLimbVec3 pelvis_right = Right(pelvis_frame_yaw);
    float support_compression = 0.0f;
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        CcHumanoidContact contact = gait->feet[leg].contact;
        if (contact == CC_HUMANOID_CONTACT_SWING ||
            contact == CC_HUMANOID_CONTACT_AIR) continue;
        float side = leg == 0 ? -1.0f : 1.0f;
        CcLimbVec3 hip = Add(gait->pose.pelvis,
                             Scale(pelvis_right, side * 0.155f));
        CcLimbVec3 ankle = gait->feet[leg].current_point;
        ankle.y += 0.085f;
        float x = ankle.x - hip.x;
        float z = ankle.z - hip.z;
        float horizontal_squared = x * x + z * z;
        float reach_squared = 0.935f * 0.935f;
        float allowed_vertical = sqrtf(fmaxf(0.02f,
                                             reach_squared -
                                             horizontal_squared));
        support_compression = fmaxf(support_compression,
                                    hip.y - ankle.y - allowed_vertical);
    }
    float constrained_drop = Clamp(support_compression, 0.0f, 0.16f);
    gait->pose.pelvis.y -= constrained_drop;
    gait->body.root.position.y -= constrained_drop;
    if (constrained_drop > 0.0f && gait->body.root.velocity.y > 0.0f) {
        gait->body.root.contact_impulse.y -= gait->body.root.velocity.y *
                                             gait->body.total_mass;
        gait->body.root.velocity.y = 0.0f;
    }
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        float side = leg == 0 ? -1.0f : 1.0f;
        gait->pose.hip[leg] = Add(gait->pose.pelvis,
                                  Scale(pelvis_right, side * 0.155f));
        gait->pose.hip[leg].y += side * sinf(gait->pose.pelvis_roll) * 0.155f;
        ResolveFootGeometry(gait, leg, forward);
        if (gait->feet[leg].contact == CC_HUMANOID_CONTACT_SWING ||
            gait->feet[leg].contact == CC_HUMANOID_CONTACT_AIR) {
            CcLimbVec3 hip_to_ankle = Subtract(gait->pose.ankle[leg],
                                               gait->pose.hip[leg]);
            float reach = Length(hip_to_ankle);
            if (reach > 0.935f) {
                CcLimbVec3 ankle = Add(gait->pose.hip[leg],
                    Scale(hip_to_ankle, 0.935f / reach));
                gait->feet[leg].current_point = ankle;
                gait->feet[leg].current_point.y -= 0.085f;
                ResolveFootGeometry(gait, leg, forward);
            }
        }
        CcLimbVec3 knee_pole = Add(forward, Scale(pelvis_right, side * 0.055f));
        gait->pose.knee[leg] = SolveKnee(gait->pose.hip[leg],
                                         gait->pose.ankle[leg], knee_pole,
                                         0.465f, 0.475f,
                                         &gait->pose.knee_flexion[leg]);
        CcLimbVec3 upper = Subtract(gait->pose.knee[leg], gait->pose.hip[leg]);
        CcLimbVec3 lower = Subtract(gait->pose.ankle[leg], gait->pose.knee[leg]);
        float hip_angle = atan2f(Dot(upper, forward), -upper.y);
        float shin_angle = atan2f(Dot(lower, forward), -lower.y);
        int32_t hip_joint = leg == 0 ? CC_HUMANOID_LEFT_HIP :
                                       CC_HUMANOID_RIGHT_HIP;
        int32_t knee_joint = leg == 0 ? CC_HUMANOID_LEFT_KNEE :
                                        CC_HUMANOID_RIGHT_KNEE;
        int32_t ankle_joint = leg == 0 ? CC_HUMANOID_LEFT_ANKLE :
                                         CC_HUMANOID_RIGHT_ANKLE;
        CcBiomechRigConstrainJoint(&gait->body, hip_joint, hip_angle,
                                   gait->last_delta_time);
        CcBiomechRigConstrainJoint(&gait->body, knee_joint,
                                   gait->pose.knee_flexion[leg],
                                   gait->last_delta_time);
        CcBiomechRigConstrainJoint(&gait->body, ankle_joint,
                                   gait->pose.foot_pitch[leg] - shin_angle,
                                   gait->last_delta_time);
    }

    gait->pose.spine = Add(gait->pose.pelvis, Scale(up, 0.10f));
    gait->pose.chest = Add(gait->pose.pelvis, Scale(up, 0.48f));
    gait->pose.chest = Add(gait->pose.chest,
                           Scale(forward,
                                 (CcBiomechRigJointAngle(&gait->body,
                                      CC_HUMANOID_SPINE_PITCH) +
                                  gait->pose.pelvis_pitch) * 0.14f));
    gait->pose.neck = Add(gait->pose.chest, Scale(up, 0.25f));
    gait->pose.head = Add(gait->pose.neck, Scale(up, 0.18f));
    gait->pose.chest_yaw = CcBiomechRigJointAngle(
        &gait->body, CC_HUMANOID_SPINE_YAW);
    gait->pose.chest_roll = CcBiomechRigJointAngle(
        &gait->body, CC_HUMANOID_SPINE_ROLL);
    gait->pose.chest_pitch = CcBiomechRigJointAngle(
        &gait->body, CC_HUMANOID_SPINE_PITCH);

    float chest_frame_yaw = body_yaw + gait->pose.chest_yaw;
    CcLimbVec3 chest_forward = Forward(chest_frame_yaw);
    CcLimbVec3 chest_right = Right(chest_frame_yaw);
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        float side = arm == 0 ? -1.0f : 1.0f;
        gait->pose.shoulder[arm] = Add(gait->pose.chest,
                                       Scale(chest_right, side * 0.285f));
        gait->pose.shoulder[arm].y += 0.06f +
                                      side * sinf(gait->pose.chest_roll) * 0.285f;
        int32_t shoulder_joint = arm == 0 ? CC_HUMANOID_LEFT_SHOULDER :
                                            CC_HUMANOID_RIGHT_SHOULDER;
        int32_t elbow_joint = arm == 0 ? CC_HUMANOID_LEFT_ELBOW :
                                         CC_HUMANOID_RIGHT_ELBOW;
        float swing = CcBiomechRigJointAngle(&gait->body, shoulder_joint);
        float elbow_angle = swing +
            CcBiomechRigJointAngle(&gait->body, elbow_joint);
        CcLimbVec3 upper_direction = Add(Scale(up, -cosf(swing)),
                                         Scale(chest_forward, sinf(swing)));
        CcLimbVec3 lower_direction = Add(Scale(up, -cosf(elbow_angle)),
                                         Scale(chest_forward, sinf(elbow_angle)));
        gait->pose.elbow[arm] = Add(gait->pose.shoulder[arm],
                                    Scale(upper_direction, 0.34f));
        gait->pose.hand[arm] = Add(gait->pose.elbow[arm],
                                   Scale(lower_direction, 0.35f));
    }
}

static CcLimbVec3 ClampClimbTarget(CcLimbVec3 root, CcLimbVec3 target,
                                   float maximum_reach)
{
    CcLimbVec3 delta = Subtract(target, root);
    float distance = Length(delta);
    if (distance <= maximum_reach || distance <= 0.0001f) return target;
    return Add(root, Scale(delta, maximum_reach / distance));
}

static CcLimbVec3 TransportClimbPole(
    CcLimbVec3 root, CcLimbVec3 target, CcLimbVec3 previous_joint,
    CcLimbVec3 desired_pole, float maximum_angle)
{
    CcLimbVec3 axis = NormalizeOr(Subtract(target, root),
                                   (CcLimbVec3){0.0f, -1.0f, 0.0f});
    CcLimbVec3 current = Subtract(previous_joint, root);
    current = Subtract(current, Scale(axis, Dot(current, axis)));
    current = NormalizeOr(current, (CcLimbVec3){0.0f, 0.0f, 1.0f});
    CcLimbVec3 desired = Subtract(desired_pole,
                                  Scale(axis, Dot(desired_pole, axis)));
    desired = NormalizeOr(desired, current);
    float cosine = Clamp(Dot(current, desired), -1.0f, 1.0f);
    float angle = acosf(cosine);
    if (angle <= maximum_angle || angle <= 0.0001f) return desired;
    CcLimbVec3 waypoint = desired;
    float waypoint_angle = angle;
    if (cosine < -0.985f) {
        waypoint = NormalizeOr(Cross(axis, current),
                               (CcLimbVec3){1.0f, 0.0f, 0.0f});
        waypoint_angle = 0.5f * CC_HUMANOID_PI;
    }
    float amount = Clamp(maximum_angle / waypoint_angle, 0.0f, 1.0f);
    return NormalizeOr(Lerp(current, waypoint, amount), current);
}

static CcLimbVec3 ClimbBlendPoint(CcLimbVec3 entry, CcLimbVec3 climbing,
                                  CcLimbVec3 standing, float acquisition,
                                  float exit_weight)
{
    return Lerp(Lerp(entry, climbing, acquisition), standing, exit_weight);
}

void CcHumanoidGaitBeginClimb(CcHumanoidGait *gait)
{
    if (gait == NULL || !gait->initialized || gait->ragdoll.active) return;
    gait->climb_entry_pose = gait->pose;
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        gait->climb_entry_foot_normal[leg] = gait->feet[leg].normal;
    }
    gait->previous_pose = gait->pose;
    gait->root_velocity = (CcLimbVec3){0};
    gait->climbing = true;
    gait->grounded = true;
    SetAction(gait, CC_HUMANOID_ACTION_CLAMBER);
}

void CcHumanoidGaitAdvanceClimb(
    CcHumanoidGait *gait, CcLimbVec3 body_position, float body_yaw,
    const CcLimbVec3 hand_targets[CC_HUMANOID_ARM_COUNT],
    const CcLimbVec3 foot_targets[CC_HUMANOID_LEG_COUNT],
    const CcLimbVec3 foot_normals[CC_HUMANOID_LEG_COUNT],
    float climb_progress, float delta_time,
    CcLimbTerrainProbe probe, void *probe_context)
{
    if (gait == NULL || !gait->initialized || gait->ragdoll.active ||
        hand_targets == NULL || foot_targets == NULL || foot_normals == NULL) {
        return;
    }
    if (!gait->climbing) CcHumanoidGaitBeginClimb(gait);
    if (!gait->climbing) return;

    delta_time = Clamp(delta_time, 1.0f / 240.0f, 1.0f / 30.0f);
    climb_progress = Clamp(climb_progress, 0.0f, 1.0f);
    float acquisition = Smooth01(climb_progress / 0.24f);
    float exit_weight = Smooth01((climb_progress - 0.78f) / 0.22f);
    float lean = sinf(climb_progress * CC_HUMANOID_PI) * 0.095f *
                 (1.0f - exit_weight);
    CcLimbVec3 forward = Forward(body_yaw);
    CcLimbVec3 right = Right(body_yaw);
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};

    CcHumanoidGait standing;
    CcHumanoidGaitInit(&standing, body_position, body_yaw,
                       probe, probe_context);
    if (!standing.initialized) return;

    gait->previous_pose = gait->pose;
    CcHumanoidPose pose = gait->pose;
    CcLimbVec3 climbing_pelvis = {
        body_position.x + forward.x * lean * 0.24f,
        body_position.y + 0.935f,
        body_position.z + forward.z * lean * 0.24f
    };
    CcLimbVec3 climbing_spine = Add(climbing_pelvis, Scale(up, 0.10f));
    climbing_spine = Add(climbing_spine, Scale(forward, lean * 0.32f));
    CcLimbVec3 climbing_chest = Add(climbing_pelvis, Scale(up, 0.48f));
    climbing_chest = Add(climbing_chest, Scale(forward, lean));
    CcLimbVec3 climbing_neck = Add(climbing_chest, Scale(up, 0.25f));
    climbing_neck = Add(climbing_neck, Scale(forward, lean * 0.18f));
    CcLimbVec3 climbing_head = Add(climbing_neck, Scale(up, 0.18f));

    pose.pelvis = ClimbBlendPoint(gait->climb_entry_pose.pelvis,
                                  climbing_pelvis,
                                  standing.pose.pelvis,
                                  acquisition, exit_weight);
    pose.spine = ClimbBlendPoint(gait->climb_entry_pose.spine,
                                 climbing_spine, standing.pose.spine,
                                 acquisition, exit_weight);
    pose.chest = ClimbBlendPoint(gait->climb_entry_pose.chest,
                                 climbing_chest, standing.pose.chest,
                                 acquisition, exit_weight);
    pose.neck = ClimbBlendPoint(gait->climb_entry_pose.neck,
                                climbing_neck, standing.pose.neck,
                                acquisition, exit_weight);
    pose.head = ClimbBlendPoint(gait->climb_entry_pose.head,
                                climbing_head, standing.pose.head,
                                acquisition, exit_weight);
    pose.pelvis_yaw = LerpScalar(
        LerpScalar(gait->climb_entry_pose.pelvis_yaw, 0.0f, acquisition),
        standing.pose.pelvis_yaw, exit_weight);
    pose.pelvis_roll = LerpScalar(
        LerpScalar(gait->climb_entry_pose.pelvis_roll, 0.0f, acquisition),
        standing.pose.pelvis_roll, exit_weight);
    pose.pelvis_pitch = LerpScalar(
        LerpScalar(gait->climb_entry_pose.pelvis_pitch,
                   lean * 0.42f, acquisition),
        standing.pose.pelvis_pitch, exit_weight);
    pose.chest_yaw = LerpScalar(
        LerpScalar(gait->climb_entry_pose.chest_yaw, 0.0f, acquisition),
        standing.pose.chest_yaw, exit_weight);
    pose.chest_roll = LerpScalar(
        LerpScalar(gait->climb_entry_pose.chest_roll, 0.0f, acquisition),
        standing.pose.chest_roll, exit_weight);
    pose.chest_pitch = LerpScalar(
        LerpScalar(gait->climb_entry_pose.chest_pitch,
                   lean * 0.64f, acquisition),
        standing.pose.chest_pitch, exit_weight);

    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        float side = leg == 0 ? -1.0f : 1.0f;
        CcLimbVec3 climbing_hip = Add(climbing_pelvis,
                                      Scale(right, side * 0.155f));
        pose.hip[leg] = ClimbBlendPoint(gait->climb_entry_pose.hip[leg],
                                        climbing_hip,
                                        standing.pose.hip[leg],
                                        acquisition, exit_weight);

        CcLimbVec3 normal = NormalizeOr(Lerp(
            Lerp(gait->climb_entry_foot_normal[leg], foot_normals[leg],
                 acquisition),
            standing.feet[leg].normal, exit_weight), up);
        float wall_weight = 1.0f - Clamp(normal.y, 0.0f, 1.0f);
        CcLimbVec3 foot_axis = NormalizeOr(
            Add(Scale(forward, 1.0f - wall_weight),
                Scale(up, wall_weight)), forward);
        CcLimbVec3 climbing_ankle = Add(foot_targets[leg],
                                        Scale(normal, 0.085f));
        CcLimbVec3 climbing_heel = Add(foot_targets[leg],
                                       Scale(foot_axis, -0.11f));
        CcLimbVec3 climbing_ball = Add(foot_targets[leg],
                                       Scale(foot_axis, 0.13f));
        CcLimbVec3 climbing_toe = Add(foot_targets[leg],
                                      Scale(foot_axis, 0.20f));
        pose.ankle[leg] = ClimbBlendPoint(
            gait->climb_entry_pose.ankle[leg], climbing_ankle,
            standing.pose.ankle[leg], acquisition, exit_weight);
        pose.heel[leg] = ClimbBlendPoint(
            gait->climb_entry_pose.heel[leg], climbing_heel,
            standing.pose.heel[leg], acquisition, exit_weight);
        pose.ball[leg] = ClimbBlendPoint(
            gait->climb_entry_pose.ball[leg], climbing_ball,
            standing.pose.ball[leg], acquisition, exit_weight);
        pose.toe[leg] = ClimbBlendPoint(
            gait->climb_entry_pose.toe[leg], climbing_toe,
            standing.pose.toe[leg], acquisition, exit_weight);
        CcLimbVec3 requested_ankle = pose.ankle[leg];
        pose.ankle[leg] = ClampClimbTarget(pose.hip[leg], requested_ankle,
                                           0.935f);
        CcLimbVec3 ankle_correction = Subtract(pose.ankle[leg],
                                               requested_ankle);
        pose.heel[leg] = Add(pose.heel[leg], ankle_correction);
        pose.ball[leg] = Add(pose.ball[leg], ankle_correction);
        pose.toe[leg] = Add(pose.toe[leg], ankle_correction);

        CcLimbVec3 climbing_pole = Add(
            Add(Scale(normal, 0.88f), Scale(right, side * 0.16f)),
            Scale(up, 0.08f));
        CcLimbVec3 standing_pole = Subtract(standing.pose.knee[leg],
                                            standing.pose.hip[leg]);
        CcLimbVec3 desired_pole = Lerp(climbing_pole, standing_pole,
                                       exit_weight);
        CcLimbVec3 knee_pole = TransportClimbPole(
            pose.hip[leg], pose.ankle[leg], gait->previous_pose.knee[leg],
            desired_pole, 6.0f * delta_time);
        pose.knee[leg] = SolveKnee(
            pose.hip[leg], pose.ankle[leg], knee_pole,
            0.465f, 0.475f, &pose.knee_flexion[leg]);
        CcLimbVec3 foot_direction = Subtract(pose.toe[leg],
                                              pose.heel[leg]);
        float foot_horizontal = sqrtf(foot_direction.x * foot_direction.x +
                                       foot_direction.z * foot_direction.z);
        pose.foot_pitch[leg] = atan2f(foot_direction.y,
                                      fmaxf(0.0001f, foot_horizontal));

        CcLimbVec3 solved_contact = Subtract(pose.ankle[leg],
                                              Scale(normal, 0.085f));
        gait->feet[leg].current_point = solved_contact;
        gait->feet[leg].planted_point = solved_contact;
        gait->feet[leg].normal = normal;
        gait->feet[leg].contact = CC_HUMANOID_CONTACT_FLAT;
        gait->feet[leg].pitch.value = pose.foot_pitch[leg];
    }

    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        float side = arm == 0 ? -1.0f : 1.0f;
        CcLimbVec3 climbing_shoulder = Add(
            climbing_chest, Scale(right, side * 0.285f));
        climbing_shoulder.y += 0.06f;
        pose.shoulder[arm] = ClimbBlendPoint(
            gait->climb_entry_pose.shoulder[arm], climbing_shoulder,
            standing.pose.shoulder[arm], acquisition, exit_weight);
        pose.hand[arm] = ClimbBlendPoint(
            gait->climb_entry_pose.hand[arm], hand_targets[arm],
            standing.pose.hand[arm], acquisition, exit_weight);
        pose.hand[arm] = ClampClimbTarget(pose.shoulder[arm], pose.hand[arm],
                                          0.685f);
        CcLimbVec3 climbing_pole = Add(Scale(right, side),
                                       Scale(forward, -0.24f));
        CcLimbVec3 standing_pole = Subtract(standing.pose.elbow[arm],
                                            standing.pose.shoulder[arm]);
        CcLimbVec3 desired_elbow_pole = Lerp(climbing_pole, standing_pole,
                                             exit_weight);
        CcLimbVec3 elbow_pole = TransportClimbPole(
            pose.shoulder[arm], pose.hand[arm],
            gait->previous_pose.elbow[arm], desired_elbow_pole,
            6.0f * delta_time);
        float elbow_flexion = 0.0f;
        pose.elbow[arm] = SolveKnee(
            pose.shoulder[arm], pose.hand[arm], elbow_pole,
            0.34f, 0.35f, &elbow_flexion);

        CcLimbVec3 upper = Subtract(pose.elbow[arm], pose.shoulder[arm]);
        float shoulder_angle = atan2f(Dot(upper, forward), -upper.y);
        int32_t shoulder_joint = arm == 0 ? CC_HUMANOID_LEFT_SHOULDER :
                                            CC_HUMANOID_RIGHT_SHOULDER;
        int32_t elbow_joint = arm == 0 ? CC_HUMANOID_LEFT_ELBOW :
                                         CC_HUMANOID_RIGHT_ELBOW;
        CcBiomechRigDriveJoint(&gait->body, shoulder_joint,
                               shoulder_angle, 0.76f);
        CcBiomechRigDriveJoint(&gait->body, elbow_joint,
                               elbow_flexion, 0.82f);
    }

    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_PITCH,
                           pose.chest_pitch, 0.78f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_ROLL,
                           pose.chest_roll, 0.70f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_YAW,
                           pose.chest_yaw, 0.70f);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        CcLimbVec3 upper = Subtract(pose.knee[leg], pose.hip[leg]);
        CcLimbVec3 lower = Subtract(pose.ankle[leg], pose.knee[leg]);
        float hip_angle = atan2f(Dot(upper, forward), -upper.y);
        float shin_angle = atan2f(Dot(lower, forward), -lower.y);
        int32_t hip_joint = leg == 0 ? CC_HUMANOID_LEFT_HIP :
                                       CC_HUMANOID_RIGHT_HIP;
        int32_t knee_joint = leg == 0 ? CC_HUMANOID_LEFT_KNEE :
                                        CC_HUMANOID_RIGHT_KNEE;
        int32_t ankle_joint = leg == 0 ? CC_HUMANOID_LEFT_ANKLE :
                                         CC_HUMANOID_RIGHT_ANKLE;
        CcBiomechRigDriveJoint(&gait->body, hip_joint, hip_angle, 0.82f);
        CcBiomechRigDriveJoint(&gait->body, knee_joint,
                               pose.knee_flexion[leg], 0.88f);
        CcBiomechRigDriveJoint(&gait->body, ankle_joint,
                               pose.foot_pitch[leg] - shin_angle, 0.76f);
    }
    CcBiomechRigStep(&gait->body, delta_time);

    CcBiomechVec3 root_position = ToBiomech(pose.pelvis);
    CcLimbVec3 pelvis_delta = Subtract(pose.pelvis,
                                       gait->previous_pose.pelvis);
    CcBiomechVec3 root_velocity = ToBiomech(
        Scale(pelvis_delta, 1.0f / delta_time));
    CcBiomechRigConstrainBody(&gait->body, root_position, root_velocity);
    gait->root_velocity = FromBiomech(root_velocity);
    gait->ground_reaction = (CcLimbVec3){0.0f,
        gait->body.total_mass * 9.81f, 0.0f};
    gait->speed.value = 0.0f;
    gait->speed.velocity = 0.0f;
    gait->planted_count = CC_HUMANOID_LEG_COUNT;
    gait->grounded = true;
    gait->pose = pose;
    gait->last_delta_time = delta_time;
}

void CcHumanoidGaitFinishClimb(CcHumanoidGait *gait,
                               CcLimbVec3 body_position, float body_yaw,
                               CcLimbTerrainProbe probe,
                               void *probe_context)
{
    if (gait == NULL || !gait->initialized) return;
    CcHumanoidGait standing;
    CcHumanoidGaitInit(&standing, body_position, body_yaw,
                       probe, probe_context);
    if (!standing.initialized) return;
    *gait = standing;
    gait->climbing = false;
}

bool CcHumanoidGaitClimbReady(const CcHumanoidGait *gait,
                              CcLimbVec3 body_position, float body_yaw,
                              CcLimbTerrainProbe probe,
                              void *probe_context, float tolerance)
{
    if (gait == NULL || !gait->initialized || !gait->climbing) return false;
    CcHumanoidGait standing;
    CcHumanoidGaitInit(&standing, body_position, body_yaw,
                       probe, probe_context);
    if (!standing.initialized) return false;
    float maximum_error = Length(Subtract(gait->pose.pelvis,
                                           standing.pose.pelvis));
#define INCLUDE_CLIMB_ERROR(point) \
    maximum_error = fmaxf(maximum_error, \
        Length(Subtract(gait->pose.point, standing.pose.point)))
    INCLUDE_CLIMB_ERROR(spine);
    INCLUDE_CLIMB_ERROR(chest);
    INCLUDE_CLIMB_ERROR(neck);
    INCLUDE_CLIMB_ERROR(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.hip[leg], standing.pose.hip[leg])));
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.knee[leg], standing.pose.knee[leg])));
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.ankle[leg], standing.pose.ankle[leg])));
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.heel[leg], standing.pose.heel[leg])));
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.ball[leg], standing.pose.ball[leg])));
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.toe[leg], standing.pose.toe[leg])));
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.shoulder[arm],
                            standing.pose.shoulder[arm])));
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.elbow[arm], standing.pose.elbow[arm])));
        maximum_error = fmaxf(maximum_error,
            Length(Subtract(gait->pose.hand[arm], standing.pose.hand[arm])));
    }
#undef INCLUDE_CLIMB_ERROR
    return maximum_error <= fmaxf(0.001f, tolerance);
}

void CcHumanoidGaitInit(CcHumanoidGait *gait, CcLimbVec3 body_position,
                        float body_yaw, CcLimbTerrainProbe probe,
                        void *probe_context)
{
    if (gait == NULL) return;
    *gait = (CcHumanoidGait){0};
    if (!ConfigureBiomechanicalBody(&gait->body)) return;
    gait->phase = 0.02f;
    gait->travel_yaw = body_yaw;
    gait->cadence = 0.82f;
    gait->grounded = true;
    gait->support_leg = 0;
    gait->last_delta_time = 1.0f / 60.0f;
    CcLimbVec3 right = Right(body_yaw);
    CcLimbVec3 forward = Forward(body_yaw);
    float contact_height = 0.0f;
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        float side = leg == 0 ? -1.0f : 1.0f;
        CcLimbVec3 desired = Add(body_position, Scale(right, side * 0.135f));
        desired = Add(desired, Scale(forward, leg == 0 ? 0.055f : -0.025f));
        CcHumanoidFoot *foot = &gait->feet[leg];
        foot->planted_point = ProbeGround(desired, body_position, probe,
                                          probe_context, &foot->normal);
        foot->swing_start = foot->planted_point;
        foot->swing_target = foot->planted_point;
        foot->current_point = foot->planted_point;
        foot->local_phase = Wrap01(gait->phase + (leg == 0 ? 0.0f : 0.5f));
        foot->contact = ContactForPhase(foot->local_phase);
        foot->pitch.value = ContactPitch(foot->contact, foot->local_phase);
        contact_height += foot->current_point.y;
    }
    contact_height /= (float)CC_HUMANOID_LEG_COUNT;
    CcBiomechRigSetBodyState(&gait->body,
        (CcBiomechVec3){body_position.x, contact_height + 0.90f,
                        body_position.z},
        (CcBiomechVec3){0});
    gait->pelvis_height.value = 0.0f;
    gait->pelvis_sway.value = -0.035f;
    gait->planted_count = CC_HUMANOID_LEG_COUNT;
    gait->initialized = true;
    gait->action = CC_HUMANOID_ACTION_LOCOMOTION;
    gait->previous_action = CC_HUMANOID_ACTION_LOCOMOTION;
    gait->action_blend = 1.0f;
    CcHumanoidGaitResolvePose(gait, body_position, body_yaw);
    gait->previous_pose = gait->pose;
}

void CcHumanoidGaitSetGuarded(CcHumanoidGait *gait, bool guarded)
{
    if (gait == NULL || !gait->initialized || gait->ragdoll.active ||
        gait->climbing || gait->action == CC_HUMANOID_ACTION_SWIM) return;
    gait->guard_requested = guarded;
    if (guarded) {
        if (gait->action == CC_HUMANOID_ACTION_LOCOMOTION) {
            SetAction(gait, CC_HUMANOID_ACTION_GUARD);
        }
    } else if (gait->action == CC_HUMANOID_ACTION_GUARD) {
        SetAction(gait, CC_HUMANOID_ACTION_LOCOMOTION);
    }
}

bool CcHumanoidGaitBeginStrike(CcHumanoidGait *gait, int32_t striking_arm)
{
    if (gait == NULL || !gait->initialized || gait->ragdoll.active ||
        gait->climbing || gait->action == CC_HUMANOID_ACTION_SWIM ||
        gait->action == CC_HUMANOID_ACTION_JUMP ||
        gait->action == CC_HUMANOID_ACTION_STRIKE ||
        CcHumanoidGaitStrikeRecovering(gait)) return false;
    bool from_guard = gait->action == CC_HUMANOID_ACTION_GUARD;
    gait->strike_side = striking_arm == 0 ? 0 : 1;
    gait->strike_style = CC_HUMANOID_STRIKE_CUT;
    gait->strike_impact_pending = false;
    gait->strike_impact_emitted = false;
    gait->strike_recovery_seconds = 0.0f;
    SetAction(gait, CC_HUMANOID_ACTION_STRIKE);
    if (from_guard) gait->action_blend = 1.0f;
    return true;
}

void CcHumanoidGaitSetStrikeStyle(CcHumanoidGait *gait,
                                  CcHumanoidStrikeStyle style)
{
    if (gait == NULL || gait->action != CC_HUMANOID_ACTION_STRIKE) return;
    if (style < CC_HUMANOID_STRIKE_CUT ||
        style > CC_HUMANOID_STRIKE_SWEEP) {
        style = CC_HUMANOID_STRIKE_CUT;
    }
    gait->strike_style = style;
}

float CcHumanoidStrikeDuration(void)
{
    return CC_HUMANOID_STRIKE_DURATION;
}

bool CcHumanoidGaitStrikeRecovering(const CcHumanoidGait *gait)
{
    return gait != NULL && gait->strike_recovery_seconds > 0.0f;
}

static void StabilizeStrikeRecovery(CcHumanoidGait *gait, float delta_time)
{
    if (!CcHumanoidGaitStrikeRecovering(gait)) return;
    static const int32_t RECOVERY_JOINTS[] = {
        CC_HUMANOID_SPINE_PITCH,
        CC_HUMANOID_SPINE_ROLL,
        CC_HUMANOID_SPINE_YAW,
        CC_HUMANOID_LEFT_SHOULDER,
        CC_HUMANOID_LEFT_ELBOW,
        CC_HUMANOID_RIGHT_SHOULDER,
        CC_HUMANOID_RIGHT_ELBOW,
    };
    float weight = Clamp(gait->strike_recovery_seconds /
                         CC_HUMANOID_STRIKE_RECOVERY_DURATION, 0.0f, 1.0f);
    float damping = expf(-(8.0f + weight * 10.0f) * delta_time);
    for (size_t index = 0;
         index < sizeof(RECOVERY_JOINTS) / sizeof(RECOVERY_JOINTS[0]);
         ++index) {
        gait->body.joints[RECOVERY_JOINTS[index]].angular_velocity *= damping;
    }
}

bool CcHumanoidGaitBeginJump(CcHumanoidGait *gait)
{
    if (gait == NULL || !gait->initialized || gait->ragdoll.active ||
        gait->climbing || gait->action == CC_HUMANOID_ACTION_SWIM ||
        gait->action == CC_HUMANOID_ACTION_STRIKE ||
        gait->action == CC_HUMANOID_ACTION_JUMP || !gait->grounded) {
        return false;
    }
    gait->jump_airborne = false;
    SetAction(gait, CC_HUMANOID_ACTION_JUMP);
    return true;
}

void CcHumanoidGaitApplyImpact(CcHumanoidGait *gait,
                               CcLimbVec3 direction, float strength)
{
    if (gait == NULL || !gait->initialized || gait->ragdoll.active) return;
    gait->impact_direction = NormalizeOr(
        direction, (CcLimbVec3){0.0f, 0.0f, 1.0f});
    gait->impact_response = fmaxf(gait->impact_response,
                                  Clamp(strength, 0.0f, 1.0f));
}

bool CcHumanoidGaitKnockDown(CcHumanoidGait *gait)
{
    return gait != NULL && gait->initialized && ActivateRagdoll(gait);
}

bool CcHumanoidGaitConsumeStrikeImpact(CcHumanoidGait *gait)
{
    if (gait == NULL || !gait->strike_impact_pending) return false;
    gait->strike_impact_pending = false;
    return true;
}

void CcHumanoidGaitAdvance(CcHumanoidGait *gait, CcLimbVec3 body_position,
                           float body_yaw, CcLimbVec3 desired_velocity,
                           bool grounded, float delta_time,
                           CcLimbTerrainProbe probe, void *probe_context)
{
    if (gait == NULL) return;
    if (!gait->initialized) {
        CcHumanoidGaitInit(gait, body_position, body_yaw, probe, probe_context);
    }
    delta_time = Clamp(delta_time, 0.0f, 1.0f / 30.0f);
    gait->last_delta_time = fmaxf(delta_time, 1.0f / 240.0f);
    bool controlled_jump = gait->action == CC_HUMANOID_ACTION_JUMP;
    if (!grounded && !gait->ragdoll.active && !controlled_jump) {
        (void)ActivateRagdoll(gait);
    }
    if (gait->ragdoll.active) {
        (void)StepRagdoll(gait, body_position, body_yaw, grounded, delta_time,
                          probe, probe_context);
        gait->grounded = grounded;
        return;
    }
    AdvanceAction(gait, delta_time);
    if (controlled_jump && !grounded) gait->jump_airborne = true;
    gait->body.root.position.x = body_position.x;
    gait->body.root.position.z = body_position.z;
    float desired_speed = sqrtf(desired_velocity.x * desired_velocity.x +
                                desired_velocity.z * desired_velocity.z);
    if (desired_speed > 0.025f) {
        float target_yaw = atan2f(desired_velocity.x, desired_velocity.z);
        float difference = WrapAngle(target_yaw - gait->travel_yaw);
        gait->travel_yaw = WrapAngle(gait->travel_yaw +
                                     difference * fminf(1.0f, delta_time * 8.0f));
    }

    int32_t support_count = 0;
    float contact_height = 0.0f;
    CcLimbVec3 support_normal = {0};
    CcLimbVec3 support_center = {0};
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        CcHumanoidContact contact = gait->feet[leg].contact;
        if (contact == CC_HUMANOID_CONTACT_SWING ||
            contact == CC_HUMANOID_CONTACT_AIR) continue;
        support_count += 1;
        contact_height += gait->feet[leg].current_point.y;
        support_normal = Add(support_normal, gait->feet[leg].normal);
        support_center = Add(support_center, gait->feet[leg].current_point);
    }
    gait->ground_reaction = (CcLimbVec3){0};
    if (grounded && support_count > 0) {
        contact_height /= (float)support_count;
        support_normal = NormalizeOr(support_normal,
                                     (CcLimbVec3){0.0f, 1.0f, 0.0f});
        support_center = Scale(support_center, 1.0f / (float)support_count);
        float minimum_height = contact_height + 0.70f;
        if (gait->body.root.position.y < minimum_height) {
            gait->body.root.position.y = minimum_height;
            if (gait->body.root.velocity.y < 0.0f) {
                gait->body.root.contact_impulse.y +=
                    -gait->body.root.velocity.y * gait->body.total_mass;
                gait->body.root.velocity.y *= -0.08f;
            }
        }
        float target_height = contact_height + 0.90f +
                              gait->pelvis_height.value;
        float vertical_error = target_height - gait->body.root.position.y;
        float support_acceleration = 9.81f + vertical_error * 54.0f -
                                     gait->body.root.velocity.y * 12.5f;
        float support_force = Clamp(support_acceleration * gait->body.total_mass,
                                    0.0f, gait->body.total_mass * 27.0f);

        bool stationary_combat = desired_speed < 0.04f &&
            (gait->action == CC_HUMANOID_ACTION_GUARD ||
             gait->action == CC_HUMANOID_ACTION_STRIKE);
        float drive_gain = stationary_combat ? 12.0f :
                           desired_speed < 0.04f ? 7.0f : 4.8f;
        CcLimbVec3 desired_acceleration = {
            (desired_velocity.x - gait->body.root.velocity.x) * drive_gain,
            0.0f,
            (desired_velocity.z - gait->body.root.velocity.z) * drive_gain
        };
        CcLimbVec3 balance_right = Right(gait->travel_yaw);
        CcLimbVec3 support_offset = Subtract(
            support_center, FromBiomech(gait->body.root.position));
        float lateral_error = Dot(support_offset, balance_right);
        float lateral_velocity = gait->body.root.velocity.x * balance_right.x +
                                 gait->body.root.velocity.z * balance_right.z;
        float balance_damping = stationary_combat ? 6.4f : 3.8f;
        float balance_acceleration = Clamp(lateral_error * 12.0f -
                                           lateral_velocity * balance_damping,
                                           -0.85f, 0.85f);
        desired_acceleration = Add(desired_acceleration,
                                   Scale(balance_right,
                                         balance_acceleration));
        float acceleration_length = Length(desired_acceleration);
        if (acceleration_length > 5.8f) {
            desired_acceleration = Scale(desired_acceleration,
                                         5.8f / acceleration_length);
        }
        CcLimbVec3 drive_force = Scale(desired_acceleration,
                                       gait->body.total_mass);
        float drive_length = Length(drive_force);
        float friction_limit = support_force * 0.72f;
        if (drive_length > friction_limit && drive_length > 0.0001f) {
            drive_force = Scale(drive_force, friction_limit / drive_length);
        }
        CcLimbVec3 support = Scale(support_normal, support_force);
        gait->ground_reaction = Add(support, drive_force);
        CcBiomechRigApplyBodyForce(&gait->body,
                                   ToBiomech(gait->ground_reaction));

        CcLimbVec3 forward = Forward(body_yaw);
        float force_per_leg = support_force / (float)support_count;
        for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
            CcHumanoidContact contact = gait->feet[leg].contact;
            if (contact == CC_HUMANOID_CONTACT_SWING ||
                contact == CC_HUMANOID_CONTACT_AIR) continue;
            float lever = Dot(Subtract(gait->feet[leg].current_point,
                                       FromBiomech(gait->body.root.position)),
                              forward);
            int32_t hip = leg == 0 ? CC_HUMANOID_LEFT_HIP :
                                     CC_HUMANOID_RIGHT_HIP;
            CcBiomechRigApplyTorque(&gait->body, hip,
                                    -lever * force_per_leg * 0.035f);
        }
    }
    CcBiomechRigStepBody(&gait->body, delta_time);
    bool stationary_combat = grounded && desired_speed < 0.04f &&
        (gait->action == CC_HUMANOID_ACTION_GUARD ||
         gait->action == CC_HUMANOID_ACTION_STRIKE);
    if (stationary_combat) {
        float horizontal_damping = expf(-12.0f * delta_time);
        gait->body.root.velocity.x *= horizontal_damping;
        gait->body.root.velocity.z *= horizontal_damping;
    }
    gait->root_velocity = FromBiomech(gait->body.root.velocity);
    float actual_speed = sqrtf(gait->root_velocity.x * gait->root_velocity.x +
                               gait->root_velocity.z * gait->root_velocity.z);
    SpringStep(&gait->speed, actual_speed, actual_speed < gait->speed.value ?
               11.0f : 8.0f, 0.92f, delta_time);
    gait->speed.value = Clamp(gait->speed.value, 0.0f, 1.60f);
    gait->cadence = 0.82f + gait->speed.value * 0.34f;

    float old_phase = gait->phase;
    bool swing_in_progress = false;
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        if (gait->feet[leg].contact == CC_HUMANOID_CONTACT_SWING) {
            swing_in_progress = true;
        }
    }
    float motion = Smooth01(gait->speed.value / 0.24f);
    if (swing_in_progress) motion = fmaxf(motion, 0.38f);
    if (grounded) gait->phase = Wrap01(gait->phase + gait->cadence * motion *
                                       delta_time);

    CcLimbVec3 foot_base = body_position;
    foot_base.x = gait->body.root.position.x;
    foot_base.z = gait->body.root.position.z;
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        UpdateFoot(gait, leg, old_phase, gait->phase, foot_base, grounded,
                   delta_time, probe, probe_context);
    }

    gait->grounded = grounded;
    gait->support_leg = gait->phase < 0.5f ? 0 : 1;
    gait->planted_count = 0;
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        if (gait->feet[leg].contact != CC_HUMANOID_CONTACT_SWING &&
            gait->feet[leg].contact != CC_HUMANOID_CONTACT_AIR) {
            gait->planted_count += 1;
        }
    }

    float stride = gait->phase * 2.0f * CC_HUMANOID_PI;
    if (controlled_jump) {
        CcLimbVec3 pelvis = FromBiomech(gait->body.root.position);
        CcLimbVec3 forward = Forward(body_yaw);
        CcLimbVec3 right = Right(body_yaw);
        float jump_time = Clamp(gait->action_time / 0.95f, 0.0f, 1.0f);
        float tuck = sinf(jump_time * CC_HUMANOID_PI);
        for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
            float side = leg == 0 ? -1.0f : 1.0f;
            CcLimbVec3 foot = Add(pelvis, Scale(right, side * 0.15f));
            foot = Add(foot, Scale(forward, 0.05f + tuck * 0.14f));
            foot.y -= 0.80f - tuck * (leg == 0 ? 0.20f : 0.13f);
            gait->feet[leg].current_point = foot;
            gait->feet[leg].planted_point = foot;
            gait->feet[leg].contact = CC_HUMANOID_CONTACT_AIR;
            gait->feet[leg].pitch.value = 0.08f + tuck * 0.08f;
        }
        gait->planted_count = 0;
    }
    float gait_weight = grounded && gait->speed.value > 0.08f ?
                        Smooth01(gait->speed.value / 0.30f) : 0.0f;
    float height_target = -cosf(stride * 2.0f) * 0.026f * gait_weight;
    float sway_target = -cosf(stride) * 0.047f * gait_weight;
    float pelvis_roll_target = sinf(stride) * 0.050f * gait_weight;
    float pelvis_yaw_target = sinf(stride) * 0.065f * gait_weight;
    SpringStep(&gait->pelvis_height, height_target, 12.0f, 0.86f, delta_time);
    SpringStep(&gait->pelvis_sway, sway_target, 10.0f, 0.80f, delta_time);
    SpringStep(&gait->pelvis_roll, pelvis_roll_target, 9.0f, 0.76f, delta_time);
    SpringStep(&gait->pelvis_yaw, pelvis_yaw_target, 8.0f, 0.74f, delta_time);
    bool combat_pose = gait->action == CC_HUMANOID_ACTION_GUARD ||
                       gait->action == CC_HUMANOID_ACTION_STRIKE;
    bool previous_combat_pose =
        gait->previous_action == CC_HUMANOID_ACTION_GUARD ||
        gait->previous_action == CC_HUMANOID_ACTION_STRIKE;
    bool leaving_combat_pose = !combat_pose && previous_combat_pose &&
                               gait->action_blend < 0.999f;
    bool jump_pose = gait->action == CC_HUMANOID_ACTION_JUMP;
    if (combat_pose || jump_pose) gait_weight *= 1.0f - gait->action_blend;
    if (leaving_combat_pose) gait_weight *= gait->action_blend;
    float left_arm = -cosf(stride) * 0.34f * gait_weight;
    float right_arm = -left_arm;
    float left_flex = 0.24f + fmaxf(0.0f, left_arm) * 0.34f;
    float right_flex = 0.24f + fmaxf(0.0f, right_arm) * 0.34f;
    float action_spine_yaw = 0.0f;
    float action_spine_pitch = 0.0f;
    if (combat_pose || leaving_combat_pose) {
        float guard_left_arm = 0.62f;
        float guard_right_arm = 0.76f;
        float guard_left_flex = 0.92f;
        float guard_right_flex = 0.66f;
        float target_left_arm = guard_left_arm;
        float target_right_arm = guard_right_arm;
        float target_left_flex = guard_left_flex;
        float target_right_flex = guard_right_flex;
        if (combat_pose && gait->action == CC_HUMANOID_ACTION_STRIKE) {
            float phase = Clamp(gait->action_time /
                                CC_HUMANOID_STRIKE_DURATION, 0.0f, 1.0f);
            const float chamber_end = 0.28f;
            const float cut_end = 0.62f;
            int32_t arm = gait->strike_side == 0 ? 0 : 1;
            float chamber_arm = gait->strike_style ==
                                CC_HUMANOID_STRIKE_HEAVY ? -0.28f : 0.18f;
            float driven_arm = gait->strike_style ==
                               CC_HUMANOID_STRIKE_HEAVY ? 1.56f :
                               gait->strike_style == CC_HUMANOID_STRIKE_SWEEP ?
                               1.46f : 1.36f;
            float chamber_flex = gait->strike_style ==
                                 CC_HUMANOID_STRIKE_HEAVY ? 1.34f : 1.12f;
            float driven_flex = gait->strike_style ==
                                CC_HUMANOID_STRIKE_HEAVY ? 0.22f : 0.10f;
            float striking_arm = StrikePoseCurve(
                guard_right_arm, chamber_arm, driven_arm, phase,
                chamber_end, cut_end);
            float striking_flex = StrikePoseCurve(
                guard_right_flex, chamber_flex, driven_flex, phase,
                chamber_end, cut_end);
            if (arm == 0) {
                target_left_arm = striking_arm;
                target_left_flex = striking_flex;
            } else {
                target_right_arm = striking_arm;
                target_right_flex = striking_flex;
            }
            if (gait->strike_style == CC_HUMANOID_STRIKE_HEAVY) {
                float support_arm = StrikePoseCurve(
                    guard_left_arm, 0.28f, 1.18f, phase,
                    chamber_end, cut_end);
                float support_flex = StrikePoseCurve(
                    guard_left_flex, 1.18f, 0.36f, phase,
                    chamber_end, cut_end);
                if (arm == 0) {
                    target_right_arm = support_arm;
                    target_right_flex = support_flex;
                } else {
                    target_left_arm = support_arm;
                    target_left_flex = support_flex;
                }
            }
            float yaw_scale = gait->strike_style == CC_HUMANOID_STRIKE_SWEEP ?
                              0.38f : 0.26f;
            float pitch_scale = gait->strike_style ==
                                CC_HUMANOID_STRIKE_HEAVY ? 0.17f : 0.10f;
            action_spine_yaw = (arm == 0 ? -1.0f : 1.0f) *
                StrikePoseCurve(0.0f, -yaw_scale * 0.38f, yaw_scale,
                                phase, chamber_end, cut_end);
            action_spine_pitch = StrikePoseCurve(
                0.0f, -pitch_scale * 0.28f, pitch_scale, phase,
                chamber_end, cut_end);
        }
        float blend = combat_pose ? gait->action_blend :
                                    1.0f - gait->action_blend;
        left_arm = LerpScalar(left_arm, target_left_arm, blend);
        right_arm = LerpScalar(right_arm, target_right_arm, blend);
        left_flex = LerpScalar(left_flex, target_left_flex, blend);
        right_flex = LerpScalar(right_flex, target_right_flex, blend);
    } else if (jump_pose) {
        float jump_time = Clamp(gait->action_time / 0.95f, 0.0f, 1.0f);
        float rising = 1.0f - Smooth01(jump_time / 0.55f);
        float landing = Smooth01((jump_time - 0.52f) / 0.48f);
        float target_arm = LerpScalar(0.42f, 0.92f, rising);
        target_arm = LerpScalar(target_arm, 0.24f, landing);
        float target_flex = LerpScalar(0.62f, 0.26f, rising);
        target_flex = LerpScalar(target_flex, 0.74f, landing);
        float blend = gait->action_blend;
        left_arm = LerpScalar(left_arm, target_arm, blend);
        right_arm = LerpScalar(right_arm, target_arm, blend);
        left_flex = LerpScalar(left_flex, target_flex, blend);
        right_flex = LerpScalar(right_flex, target_flex, blend);
        action_spine_pitch = LerpScalar(-0.045f, 0.085f, landing);
    }
    CcLimbVec3 impact_forward_axis = Forward(body_yaw);
    CcLimbVec3 impact_right_axis = Right(body_yaw);
    float impact_forward = Dot(gait->impact_direction, impact_forward_axis);
    float impact_side = Dot(gait->impact_direction, impact_right_axis);
    float impact = gait->impact_response;
    action_spine_yaw += impact_side * impact * 0.18f;
    action_spine_pitch += impact_forward * impact * 0.10f;
    left_arm += impact * (0.12f - impact_side * 0.09f);
    right_arm += impact * (0.12f + impact_side * 0.09f);
    left_flex += impact * 0.16f;
    right_flex += impact * 0.16f;
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_ROLL,
                           -pelvis_roll_target * 0.55f +
                           impact_side * impact * 0.12f, 0.58f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_YAW,
                           -pelvis_yaw_target * 0.68f + action_spine_yaw,
                           combat_pose || jump_pose ? 0.74f : 0.56f);
    CcLimbVec3 motion_forward = Forward(body_yaw);
    float forward_acceleration = gait->body.root.acceleration.x *
                                     motion_forward.x +
                                 gait->body.root.acceleration.z *
                                     motion_forward.z;
    float spine_pitch_target = gait_weight * 0.035f +
                               Clamp(forward_acceleration * 0.016f,
                                     -0.05f, 0.08f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_PITCH,
                           spine_pitch_target + action_spine_pitch,
                           combat_pose || jump_pose ? 0.72f : 0.62f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_LEFT_SHOULDER,
                           left_arm, 0.62f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_RIGHT_SHOULDER,
                           right_arm, 0.62f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_LEFT_ELBOW,
                           left_flex, 0.52f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_RIGHT_ELBOW,
                           right_flex, 0.52f);
    if (gait_weight <= 0.01f) {
        CcBiomechRigApplyTorque(&gait->body, CC_HUMANOID_LEFT_SHOULDER,
            -gait->body.joints[CC_HUMANOID_LEFT_SHOULDER].angular_velocity *
                0.12f);
        CcBiomechRigApplyTorque(&gait->body, CC_HUMANOID_RIGHT_SHOULDER,
            -gait->body.joints[CC_HUMANOID_RIGHT_SHOULDER].angular_velocity *
                0.12f);
        CcBiomechRigApplyTorque(&gait->body, CC_HUMANOID_LEFT_ELBOW,
            -gait->body.joints[CC_HUMANOID_LEFT_ELBOW].angular_velocity *
                0.08f);
        CcBiomechRigApplyTorque(&gait->body, CC_HUMANOID_RIGHT_ELBOW,
            -gait->body.joints[CC_HUMANOID_RIGHT_ELBOW].angular_velocity *
                0.08f);
    }
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        CcHumanoidFoot *foot = &gait->feet[leg];
        float cycle = foot->local_phase * 2.0f * CC_HUMANOID_PI;
        float hip_target = -cosf(cycle) * 0.30f * gait_weight;
        float knee_target = 0.10f;
        if (foot->contact == CC_HUMANOID_CONTACT_SWING) {
            float swing = Clamp((foot->local_phase - 0.62f) / 0.38f,
                                0.0f, 1.0f);
            knee_target += sinf(swing * CC_HUMANOID_PI) * 0.78f;
        } else if (foot->contact == CC_HUMANOID_CONTACT_TOE) {
            knee_target += 0.15f;
        }
        int32_t hip_joint = leg == 0 ? CC_HUMANOID_LEFT_HIP :
                                       CC_HUMANOID_RIGHT_HIP;
        int32_t knee_joint = leg == 0 ? CC_HUMANOID_LEFT_KNEE :
                                        CC_HUMANOID_RIGHT_KNEE;
        int32_t ankle_joint = leg == 0 ? CC_HUMANOID_LEFT_ANKLE :
                                         CC_HUMANOID_RIGHT_ANKLE;
        CcBiomechRigDriveJoint(&gait->body, hip_joint, hip_target, 0.78f);
        CcBiomechRigDriveJoint(&gait->body, knee_joint, knee_target, 0.82f);
        CcBiomechRigDriveJoint(&gait->body, ankle_joint,
                               foot->pitch.value, 0.72f);
    }
    StabilizeStrikeRecovery(gait, delta_time);
    CcBiomechRigStep(&gait->body, delta_time);

    gait->previous_pose = gait->pose;
    CcHumanoidGaitResolvePose(gait, body_position, body_yaw);
}

void CcHumanoidGaitAdvanceSwim(CcHumanoidGait *gait,
                               CcLimbVec3 body_position, float body_yaw,
                               CcLimbVec3 desired_velocity,
                               float water_surface, float immersion,
                               float delta_time)
{
    if (gait == NULL) return;
    if (!gait->initialized) {
        CcHumanoidGaitInit(gait, body_position, body_yaw, NULL, NULL);
    }
    if (!gait->initialized || gait->ragdoll.active || gait->climbing) return;
    delta_time = Clamp(delta_time, 1.0f / 240.0f, 1.0f / 30.0f);
    if (gait->action != CC_HUMANOID_ACTION_SWIM) {
        gait->swim_entry_pose = gait->pose;
        SetAction(gait, CC_HUMANOID_ACTION_SWIM);
    }
    AdvanceAction(gait, delta_time);
    gait->last_delta_time = delta_time;
    gait->immersion = Clamp(immersion, 0.0f, 1.0f);
    gait->swim_phase = Wrap01(gait->swim_phase +
                               delta_time * (0.72f +
                               Length(desired_velocity) * 0.44f));
    gait->travel_yaw = body_yaw;

    gait->body.root.position.x = body_position.x;
    gait->body.root.position.z = body_position.z;
    float target_pelvis_height = water_surface - 0.07f;
    CcLimbVec3 acceleration = {
        (desired_velocity.x - gait->body.root.velocity.x) * 3.4f,
        9.81f * gait->immersion +
            (target_pelvis_height - gait->body.root.position.y) * 22.0f -
            gait->body.root.velocity.y * 7.5f,
        (desired_velocity.z - gait->body.root.velocity.z) * 3.4f
    };
    CcBiomechRigApplyBodyForce(
        &gait->body, ToBiomech(Scale(acceleration, gait->body.total_mass)));
    CcBiomechRigStepBody(&gait->body, delta_time);
    gait->root_velocity = FromBiomech(gait->body.root.velocity);
    gait->ground_reaction = (CcLimbVec3){0.0f,
        gait->body.total_mass * 9.81f * gait->immersion, 0.0f};

    CcLimbVec3 pelvis = FromBiomech(gait->body.root.position);
    CcLimbVec3 forward = Forward(body_yaw);
    CcLimbVec3 right = Right(body_yaw);
    float stroke = gait->swim_phase * 2.0f * CC_HUMANOID_PI;
    float contact_release = Smooth01(gait->action_time / 0.42f);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        float side = leg == 0 ? -1.0f : 1.0f;
        float kick = sinf(stroke + (leg == 0 ? 0.0f : CC_HUMANOID_PI));
        CcLimbVec3 foot = Add(pelvis, Scale(right, side * 0.15f));
        foot = Add(foot, Scale(forward, -0.10f + kick * 0.11f));
        foot.y -= 0.80f - fmaxf(0.0f, kick) * 0.10f;
        CcLimbVec3 entry_foot = gait->swim_entry_pose.ankle[leg];
        entry_foot.y -= 0.085f;
        foot = Lerp(entry_foot, foot, contact_release);
        gait->feet[leg].current_point = foot;
        gait->feet[leg].planted_point = foot;
        gait->feet[leg].normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
        gait->feet[leg].contact = CC_HUMANOID_CONTACT_AIR;
        gait->feet[leg].local_phase = Wrap01(
            gait->swim_phase + (leg == 0 ? 0.0f : 0.5f));
        gait->feet[leg].pitch.value = 0.12f + kick * 0.06f;
    }
    gait->planted_count = 0;
    gait->grounded = false;
    gait->speed.value = Length(gait->root_velocity);

    float left_reach = 0.58f + sinf(stroke) * 0.52f;
    float right_reach = 0.58f - sinf(stroke) * 0.52f;
    float left_elbow = 0.46f + fmaxf(0.0f, -sinf(stroke)) * 0.48f;
    float right_elbow = 0.46f + fmaxf(0.0f, sinf(stroke)) * 0.48f;
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_PITCH,
                           0.14f, 0.66f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_ROLL,
                           sinf(stroke) * 0.09f, 0.58f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_SPINE_YAW,
                           sinf(stroke) * 0.07f, 0.54f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_LEFT_SHOULDER,
                           left_reach, 0.72f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_RIGHT_SHOULDER,
                           right_reach, 0.72f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_LEFT_ELBOW,
                           left_elbow, 0.68f);
    CcBiomechRigDriveJoint(&gait->body, CC_HUMANOID_RIGHT_ELBOW,
                           right_elbow, 0.68f);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        float kick = sinf(stroke + (leg == 0 ? 0.0f : CC_HUMANOID_PI));
        int32_t hip = leg == 0 ? CC_HUMANOID_LEFT_HIP :
                                 CC_HUMANOID_RIGHT_HIP;
        int32_t knee = leg == 0 ? CC_HUMANOID_LEFT_KNEE :
                                  CC_HUMANOID_RIGHT_KNEE;
        int32_t ankle = leg == 0 ? CC_HUMANOID_LEFT_ANKLE :
                                   CC_HUMANOID_RIGHT_ANKLE;
        CcBiomechRigDriveJoint(&gait->body, hip, kick * 0.22f, 0.62f);
        CcBiomechRigDriveJoint(&gait->body, knee,
                               0.22f + fmaxf(0.0f, kick) * 0.46f, 0.70f);
        CcBiomechRigDriveJoint(&gait->body, ankle, 0.10f, 0.56f);
    }
    CcBiomechRigStep(&gait->body, delta_time);
    gait->previous_pose = gait->pose;
    CcHumanoidGaitResolvePose(gait, body_position, body_yaw);
}

void CcHumanoidGaitEndSwim(CcHumanoidGait *gait,
                           CcLimbVec3 body_position, float body_yaw,
                           CcLimbTerrainProbe probe, void *probe_context)
{
    if (gait == NULL || !gait->initialized ||
        gait->action != CC_HUMANOID_ACTION_SWIM) return;
    CcLimbVec3 right = Right(body_yaw);
    CcLimbVec3 forward = Forward(body_yaw);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        float side = leg == 0 ? -1.0f : 1.0f;
        CcLimbVec3 desired = Add(body_position, Scale(right, side * 0.135f));
        desired = Add(desired, Scale(forward, leg == 0 ? 0.04f : -0.03f));
        CcHumanoidFoot *foot = &gait->feet[leg];
        foot->current_point = ProbeGround(desired, body_position, probe,
                                          probe_context, &foot->normal);
        foot->planted_point = foot->current_point;
        foot->swing_start = foot->current_point;
        foot->swing_target = foot->current_point;
        foot->contact = CC_HUMANOID_CONTACT_FLAT;
        foot->pitch.value = 0.0f;
    }
    gait->immersion = 0.0f;
    gait->grounded = true;
    gait->planted_count = CC_HUMANOID_LEG_COUNT;
    SetAction(gait, CC_HUMANOID_ACTION_LOCOMOTION);
}

void CcHumanoidGaitConstrainMotion(CcHumanoidGait *gait,
                                  CcLimbVec3 actual_position,
                                  CcLimbVec3 actual_velocity, bool grounded)
{
    if (gait == NULL || !gait->initialized) return;
    bool controlled_jump = gait->action == CC_HUMANOID_ACTION_JUMP;
    if (!grounded && !gait->ragdoll.active && !controlled_jump) {
        (void)ActivateRagdoll(gait);
    }
    if (gait->ragdoll.active) {
        gait->grounded = grounded;
        return;
    }
    bool landed = grounded && !gait->grounded;
    if (controlled_jump && !grounded) gait->jump_airborne = true;
    if (controlled_jump && grounded && gait->jump_airborne) {
        gait->jump_airborne = false;
        gait->pelvis_height.velocity = fminf(gait->pelvis_height.velocity,
                                             -0.28f);
        SetAction(gait, CC_HUMANOID_ACTION_LOCOMOTION);
    }
    CcBiomechVec3 position = gait->body.root.position;
    CcBiomechVec3 velocity = gait->body.root.velocity;
    position.x = actual_position.x;
    position.z = actual_position.z;
    velocity.x = actual_velocity.x;
    velocity.z = actual_velocity.z;
    if (!grounded) velocity.y = actual_velocity.y;
    if (grounded) {
        float minimum_height = actual_position.y + 0.70f;
        if (position.y < minimum_height) position.y = minimum_height;
        if (landed && velocity.y < -3.2f) velocity.y = -3.2f;
    }
    CcBiomechRigConstrainBody(&gait->body, position, velocity);
    gait->root_velocity = FromBiomech(gait->body.root.velocity);
    float actual_speed = sqrtf(actual_velocity.x * actual_velocity.x +
                               actual_velocity.z * actual_velocity.z);
    if (actual_speed + 0.12f < gait->speed.value) {
        gait->speed.value = fmaxf(0.0f, actual_speed + 0.12f);
        gait->speed.velocity = fminf(gait->speed.velocity, 0.0f);
    }
    gait->grounded = grounded;
}

const char *CcHumanoidContactName(CcHumanoidContact contact)
{
    switch (contact) {
        case CC_HUMANOID_CONTACT_HEEL: return "HEEL";
        case CC_HUMANOID_CONTACT_FLAT: return "FLAT";
        case CC_HUMANOID_CONTACT_TOE: return "TOE";
        case CC_HUMANOID_CONTACT_SWING: return "SWING";
        case CC_HUMANOID_CONTACT_AIR:
        default: return "AIR";
    }
}

const char *CcHumanoidActionName(CcHumanoidAction action)
{
    switch (action) {
        case CC_HUMANOID_ACTION_JUMP: return "JUMP";
        case CC_HUMANOID_ACTION_GUARD: return "GUARD";
        case CC_HUMANOID_ACTION_STRIKE: return "STRIKE";
        case CC_HUMANOID_ACTION_CLAMBER: return "CLAMBER";
        case CC_HUMANOID_ACTION_SWIM: return "SWIM";
        case CC_HUMANOID_ACTION_FALL: return "FALL";
        case CC_HUMANOID_ACTION_RECOVER: return "RECOVER";
        case CC_HUMANOID_ACTION_LOCOMOTION:
        default: return "LOCOMOTION";
    }
}
