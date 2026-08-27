#include "locomotion/cc_creature.h"

#include <math.h>
#include <stddef.h>

#define CC_CREATURE_PI 3.14159265358979323846f

typedef struct CcCreatureRigDimensions {
    CcMorphologyPreset morphology;
    float body_height;
    float body_width;
    float body_depth;
    float body_length;
    float socket_half_width;
    float socket_half_length;
    float contact_half_width;
    float contact_half_length;
    float upper_length;
    float lower_length;
    float step_height;
    float step_threshold;
    float duty_factor;
} CcCreatureRigDimensions;

static float Clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static float Wrap01(float value)
{
    value = fmodf(value, 1.0f);
    return value < 0.0f ? value + 1.0f : value;
}

static CcLimbVec3 Subtract(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static float Dot(CcLimbVec3 a, CcLimbVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float Length(CcLimbVec3 value)
{
    return sqrtf(Dot(value, value));
}

static CcLimbVec3 NormalizeOr(CcLimbVec3 value, CcLimbVec3 fallback)
{
    float length = Length(value);
    if (length <= 0.00001f) return fallback;
    return (CcLimbVec3){value.x / length, value.y / length, value.z / length};
}

static CcLimbVec3 TransformPoint(CcLimbVec3 origin, CcLimbVec3 local,
                                 float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (CcLimbVec3){origin.x + local.x * cosine + local.z * sine,
                        origin.y + local.y,
                        origin.z - local.x * sine + local.z * cosine};
}

static bool DimensionsForProfile(CcCreatureRigProfile profile,
                                 CcCreatureRigDimensions *dimensions)
{
    if (dimensions == NULL || profile < 0 ||
        profile >= CC_CREATURE_RIG_PROFILE_COUNT) {
        return false;
    }
    switch (profile) {
        case CC_CREATURE_RIG_GOBLIN:
            *dimensions = (CcCreatureRigDimensions){
                CC_MORPHOLOGY_BIPED, 0.78f, 0.42f, 0.30f, 0.34f,
                0.14f, 0.02f, 0.17f, 0.06f, 0.43f, 0.45f,
                0.14f, 0.20f, 0.62f};
            return true;
        case CC_CREATURE_RIG_HORSE:
            *dimensions = (CcCreatureRigDimensions){
                CC_MORPHOLOGY_QUADRUPED, 1.20f, 0.68f, 0.70f, 1.48f,
                0.27f, 0.50f, 0.31f, 0.63f, 0.65f, 0.68f,
                0.20f, 0.24f, 0.66f};
            return true;
        case CC_CREATURE_RIG_COW:
            *dimensions = (CcCreatureRigDimensions){
                CC_MORPHOLOGY_QUADRUPED, 0.98f, 0.82f, 0.86f, 1.54f,
                0.34f, 0.49f, 0.38f, 0.61f, 0.52f, 0.55f,
                0.13f, 0.20f, 0.74f};
            return true;
        case CC_CREATURE_RIG_DRAGON:
            *dimensions = (CcCreatureRigDimensions){
                CC_MORPHOLOGY_QUADRUPED, 1.12f, 1.08f, 0.92f, 2.12f,
                0.45f, 0.67f, 0.50f, 0.83f, 0.62f, 0.68f,
                0.23f, 0.28f, 0.70f};
            return true;
        case CC_CREATURE_RIG_PROFILE_COUNT:
        default:
            return false;
    }
}

static void ScaleDimensions(CcCreatureRigDimensions *dimensions, float scale)
{
    dimensions->body_height *= scale;
    dimensions->body_width *= scale;
    dimensions->body_depth *= scale;
    dimensions->body_length *= scale;
    dimensions->socket_half_width *= scale;
    dimensions->socket_half_length *= scale;
    dimensions->contact_half_width *= scale;
    dimensions->contact_half_length *= scale;
    dimensions->upper_length *= scale;
    dimensions->lower_length *= scale;
    dimensions->step_height *= scale;
    dimensions->step_threshold *= scale;
}

static bool ConfigureSkeleton(const CcCreatureRigDimensions *dimensions,
                              CcLimbMorphology *morphology)
{
    if (!CcLimbMorphologyFromPreset(morphology, dimensions->morphology)) {
        return false;
    }
    morphology->body_height = dimensions->body_height;
    morphology->step_height = dimensions->step_height;
    morphology->step_threshold = dimensions->step_threshold;
    morphology->duty_factor = dimensions->duty_factor;
    morphology->swing_seconds = 0.24f;
    morphology->velocity_lead = 0.12f;
    for (int32_t limb = 0; limb < morphology->limb_count; ++limb) {
        float side = (limb & 1) != 0 ? 1.0f : -1.0f;
        float end = limb < 2 ? 1.0f : -1.0f;
        CcLimbSpec *spec = &morphology->limbs[limb];
        spec->socket_local = (CcLimbVec3){
            side * dimensions->socket_half_width, 0.0f,
            dimensions->morphology == CC_MORPHOLOGY_BIPED ?
                dimensions->socket_half_length :
                end * dimensions->socket_half_length};
        spec->rest_contact_local = (CcLimbVec3){
            side * dimensions->contact_half_width,
            -dimensions->body_height,
            dimensions->morphology == CC_MORPHOLOGY_BIPED ?
                dimensions->contact_half_length :
                end * dimensions->contact_half_length};
        spec->bend_local = (CcLimbVec3){
            side * 0.04f, 0.0f,
            (dimensions->morphology == CC_MORPHOLOGY_BIPED ? 0.30f :
             -end * 0.24f) * dimensions->body_height};
        spec->segment_count = 2;
        spec->segment_length[0] = dimensions->upper_length;
        spec->segment_length[1] = dimensions->lower_length;
    }
    return true;
}

static bool AddMusclePair(CcBiomechMorphology *morphology, int32_t joint,
                          float maximum_force, float moment_arm)
{
    return CcBiomechAddMuscle(morphology, "flexor", joint, moment_arm,
                              maximum_force, 0.0f, 1.8f,
                              18.0f, 9.0f) >= 0 &&
           CcBiomechAddMuscle(morphology, "extensor", joint, -moment_arm,
                              maximum_force, 0.0f, 1.8f,
                              18.0f, 9.0f) >= 0;
}

static bool ConfigureMuscles(const CcCreatureRigDimensions *dimensions,
                             int32_t limb_count, CcBiomechRig *rig)
{
    CcBiomechMorphology morphology;
    CcBiomechMorphologyInit(&morphology);
    float scale = dimensions->body_height / 1.0f;
    int32_t root = CcBiomechAddBone(
        &morphology, "body", -1, dimensions->body_length,
        18.0f * scale, 0.50f);
    if (root < 0) return false;
    for (int32_t limb = 0; limb < limb_count; ++limb) {
        int32_t upper = CcBiomechAddBone(
            &morphology, "upper limb", root, dimensions->upper_length,
            3.8f * scale, 0.44f);
        int32_t lower = CcBiomechAddBone(
            &morphology, "lower limb", upper, dimensions->lower_length,
            2.1f * scale, 0.43f);
        if (lower < 0) return false;
        int32_t hip = CcBiomechAddJoint(
            &morphology, "proximal", root, upper, 0.0f,
            -2.80f, 2.80f, 0.12f * scale, 1.2f, 0.9f, 24.0f);
        int32_t knee = CcBiomechAddJoint(
            &morphology, "distal", upper, lower, 0.18f,
            0.0f, 2.85f, 0.08f * scale, 1.0f, 0.8f, 32.0f);
        if (hip < 0 || knee < 0 ||
            !AddMusclePair(&morphology, hip, 920.0f * scale, 0.040f * scale) ||
            !AddMusclePair(&morphology, knee, 760.0f * scale, 0.036f * scale)) {
            return false;
        }
    }
    return CcBiomechRigInit(rig, &morphology);
}

static float AngleBetween(CcLimbVec3 a, CcLimbVec3 b)
{
    CcLimbVec3 normal_a = NormalizeOr(a, (CcLimbVec3){0.0f, -1.0f, 0.0f});
    CcLimbVec3 normal_b = NormalizeOr(b, (CcLimbVec3){0.0f, -1.0f, 0.0f});
    return acosf(Clamp(Dot(normal_a, normal_b), -1.0f, 1.0f));
}

static void ResolveMuscleActivation(CcBiomechRig *muscles,
                                    const CcLimbRig *skeleton,
                                    CcCreatureRigPose *pose,
                                    float phase, float movement,
                                    CcLimbVec3 forward)
{
    const float delta_time = 1.0f / 60.0f;
    for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
        const CcLimbRuntime *runtime = &skeleton->limbs[limb];
        CcLimbVec3 upper = Subtract(runtime->joints[1], runtime->joints[0]);
        CcLimbVec3 lower = Subtract(runtime->joints[2], runtime->joints[1]);
        float proximal = atan2f(Dot(upper, forward), -upper.y);
        float distal = AngleBetween(upper, lower);
        float limb_phase = Wrap01(phase +
            skeleton->morphology.limbs[limb].phase_offset);
        float lead = sinf(limb_phase * 2.0f * CC_CREATURE_PI) *
                     0.18f * movement;
        int32_t proximal_joint = limb * 2;
        int32_t distal_joint = proximal_joint + 1;
        CcBiomechRigConstrainJoint(muscles, proximal_joint, proximal,
                                   delta_time);
        CcBiomechRigConstrainJoint(muscles, distal_joint, distal,
                                   delta_time);
        CcBiomechRigDriveJoint(muscles, proximal_joint, proximal + lead,
                               0.38f + movement * 0.52f);
        CcBiomechRigDriveJoint(muscles, distal_joint,
                               distal + fabsf(lead) * 0.72f,
                               0.42f + movement * 0.48f);
    }
    for (int32_t step = 0; step < 6; ++step) {
        CcBiomechRigStep(muscles, delta_time);
    }
    for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
        int32_t muscle = limb * 4;
        pose->limbs[limb].upper_activation =
            (muscles->muscles[muscle].activation +
             muscles->muscles[muscle + 1].activation) * 0.5f;
        pose->limbs[limb].lower_activation =
            (muscles->muscles[muscle + 2].activation +
             muscles->muscles[muscle + 3].activation) * 0.5f;
    }
    pose->mean_activation = CcBiomechRigMeanActivation(muscles);
}

bool CcCreatureRigPoseResolve(CcCreatureRigProfile profile, float phase,
                              float movement, CcLimbVec3 ground_position,
                              float yaw, float scale,
                              CcCreatureRigPose *pose)
{
    if (pose == NULL || !isfinite(phase) || !isfinite(movement) ||
        !isfinite(yaw) || !isfinite(scale) || scale <= 0.0f) {
        return false;
    }
    *pose = (CcCreatureRigPose){0};
    CcCreatureRigDimensions dimensions;
    if (!DimensionsForProfile(profile, &dimensions)) return false;
    ScaleDimensions(&dimensions, scale);

    CcLimbMorphology morphology;
    if (!ConfigureSkeleton(&dimensions, &morphology)) return false;
    movement = Clamp(movement, 0.0f, 1.0f);
    phase = Wrap01(phase);
    CcLimbVec3 body = ground_position;
    body.y += dimensions.body_height;
    CcLimbRig skeleton;
    CcLimbRigInit(&skeleton, &morphology, body, yaw, NULL, NULL);
    skeleton.gait_phase = phase;

    for (int32_t limb = 0; limb < morphology.limb_count; ++limb) {
        const CcLimbSpec *spec = &morphology.limbs[limb];
        float limb_phase = Wrap01(phase + spec->phase_offset);
        CcLimbVec3 contact_local = spec->rest_contact_local;
        float stride = dimensions.step_threshold * 1.25f * movement;
        contact_local.z += cosf(limb_phase * 2.0f * CC_CREATURE_PI) * stride;
        bool swinging = movement > 0.05f &&
                        limb_phase >= morphology.duty_factor;
        float swing_progress = swinging ?
            (limb_phase - morphology.duty_factor) /
            (1.0f - morphology.duty_factor) : 0.0f;
        if (swinging) {
            contact_local.y += sinf(swing_progress * CC_CREATURE_PI) *
                               dimensions.step_height;
        }
        CcLimbVec3 contact = TransformPoint(body, contact_local, yaw);
        CcLimbRigPinContact(&skeleton, limb, body, yaw, contact,
                            (CcLimbVec3){0.0f, 1.0f, 0.0f});
        skeleton.limbs[limb].state = swinging ? CC_LIMB_SWING : CC_LIMB_STANCE;
        skeleton.limbs[limb].swing_progress = swinging ? swing_progress : 1.0f;
    }

    CcBiomechRig muscles;
    if (!ConfigureMuscles(&dimensions, morphology.limb_count, &muscles)) {
        return false;
    }

    pose->profile = profile;
    pose->phase = phase;
    pose->body = body;
    pose->forward = (CcLimbVec3){sinf(yaw), 0.0f, cosf(yaw)};
    pose->right = (CcLimbVec3){cosf(yaw), 0.0f, -sinf(yaw)};
    pose->body_width = dimensions.body_width;
    pose->body_depth = dimensions.body_depth;
    pose->body_length = dimensions.body_length;
    pose->limb_count = morphology.limb_count;
    pose->biomech_bone_count = muscles.morphology.bone_count;
    pose->biomech_joint_count = muscles.morphology.joint_count;
    pose->biomech_muscle_count = muscles.morphology.muscle_count;
    for (int32_t limb = 0; limb < morphology.limb_count; ++limb) {
        pose->limbs[limb].state = skeleton.limbs[limb].state;
        for (int32_t joint = 0; joint < CC_CREATURE_RIG_JOINTS_PER_LIMB;
             ++joint) {
            pose->limbs[limb].joints[joint] = skeleton.limbs[limb].joints[joint];
        }
    }
    ResolveMuscleActivation(&muscles, &skeleton, pose, phase, movement,
                            pose->forward);
    pose->valid = true;
    return true;
}

const char *CcCreatureRigProfileName(CcCreatureRigProfile profile)
{
    switch (profile) {
        case CC_CREATURE_RIG_GOBLIN: return "GOBLIN";
        case CC_CREATURE_RIG_HORSE: return "HORSE";
        case CC_CREATURE_RIG_COW: return "COW";
        case CC_CREATURE_RIG_DRAGON: return "DRAGON";
        case CC_CREATURE_RIG_PROFILE_COUNT:
        default:
            return "UNKNOWN";
    }
}
