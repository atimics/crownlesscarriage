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
    float terminal_length;
    float step_height;
    float step_threshold;
    float duty_factor;
    int32_t segment_count;
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

static CcLimbVec3 Add(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x + b.x, a.y + b.y, a.z + b.z};
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
                .morphology = CC_MORPHOLOGY_BIPED,
                .body_height = 0.78f,
                .body_width = 0.42f,
                .body_depth = 0.30f,
                .body_length = 0.34f,
                .socket_half_width = 0.14f,
                .socket_half_length = 0.02f,
                .contact_half_width = 0.17f,
                .contact_half_length = 0.06f,
                .upper_length = 0.43f,
                .lower_length = 0.45f,
                .step_height = 0.14f,
                .step_threshold = 0.20f,
                .duty_factor = 0.62f,
                .segment_count = 2,
            };
            return true;
        case CC_CREATURE_RIG_HORSE:
            *dimensions = (CcCreatureRigDimensions){
                .morphology = CC_MORPHOLOGY_QUADRUPED,
                .body_height = 0.94f,
                .body_width = 0.78f,
                .body_depth = 0.82f,
                .body_length = 1.32f,
                .socket_half_width = 0.31f,
                .socket_half_length = 0.47f,
                .contact_half_width = 0.35f,
                .contact_half_length = 0.58f,
                .upper_length = 0.48f,
                .lower_length = 0.50f,
                .step_height = 0.24f,
                .step_threshold = 0.21f,
                .duty_factor = 0.69f,
                .segment_count = 2,
            };
            return true;
        case CC_CREATURE_RIG_COW:
            *dimensions = (CcCreatureRigDimensions){
                .morphology = CC_MORPHOLOGY_QUADRUPED,
                .body_height = 0.98f,
                .body_width = 0.82f,
                .body_depth = 0.86f,
                .body_length = 1.54f,
                .socket_half_width = 0.34f,
                .socket_half_length = 0.49f,
                .contact_half_width = 0.38f,
                .contact_half_length = 0.61f,
                .upper_length = 0.52f,
                .lower_length = 0.55f,
                .step_height = 0.13f,
                .step_threshold = 0.20f,
                .duty_factor = 0.74f,
                .segment_count = 2,
            };
            return true;
        case CC_CREATURE_RIG_DRAGON:
            *dimensions = (CcCreatureRigDimensions){
                .morphology = CC_MORPHOLOGY_QUADRUPED,
                .body_height = 1.12f,
                .body_width = 1.08f,
                .body_depth = 0.92f,
                .body_length = 2.12f,
                .socket_half_width = 0.45f,
                .socket_half_length = 0.67f,
                .contact_half_width = 0.50f,
                .contact_half_length = 0.83f,
                .upper_length = 0.62f,
                .lower_length = 0.68f,
                .step_height = 0.23f,
                .step_threshold = 0.28f,
                .duty_factor = 0.70f,
                .segment_count = 2,
            };
            return true;
        case CC_CREATURE_RIG_HEXAPOD:
            *dimensions = (CcCreatureRigDimensions){
                .morphology = CC_MORPHOLOGY_HEXAPOD,
                .body_height = 0.56f,
                .body_width = 0.84f,
                .body_depth = 0.46f,
                .body_length = 1.18f,
                .socket_half_width = 0.38f,
                .socket_half_length = 0.45f,
                .contact_half_width = 0.72f,
                .contact_half_length = 0.72f,
                .upper_length = 0.50f,
                .lower_length = 0.48f,
                .step_height = 0.16f,
                .step_threshold = 0.18f,
                .duty_factor = 0.58f,
                .segment_count = 2,
            };
            return true;
        case CC_CREATURE_RIG_OCTOPOD:
            *dimensions = (CcCreatureRigDimensions){
                .morphology = CC_MORPHOLOGY_OCTOPOD,
                .body_height = 0.48f,
                .body_width = 0.88f,
                .body_depth = 0.48f,
                .body_length = 1.06f,
                .socket_half_width = 0.34f,
                .socket_half_length = 0.43f,
                .contact_half_width = 0.82f,
                .contact_half_length = 0.78f,
                .upper_length = 0.38f,
                .lower_length = 0.42f,
                .terminal_length = 0.38f,
                .step_height = 0.15f,
                .step_threshold = 0.17f,
                .duty_factor = 0.76f,
                .segment_count = 3,
            };
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
    dimensions->terminal_length *= scale;
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
        int32_t pair_count = morphology->limb_count / 2;
        int32_t pair = limb / 2;
        float along = pair_count > 1 ?
            (float)pair / (float)(pair_count - 1) : 0.5f;
        float end = 1.0f - along * 2.0f;
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
        float lateral_bend = dimensions->morphology >= CC_MORPHOLOGY_HEXAPOD ?
                             0.44f : 0.04f;
        spec->bend_local = (CcLimbVec3){
            side * lateral_bend * dimensions->body_height, 0.0f,
            (dimensions->morphology == CC_MORPHOLOGY_BIPED ? 0.30f :
             -end * 0.20f) * dimensions->body_height};
        spec->segment_count = dimensions->segment_count;
        spec->segment_length[0] = dimensions->upper_length;
        spec->segment_length[1] = dimensions->lower_length;
        if (dimensions->segment_count >= 3) {
            spec->segment_length[2] = dimensions->terminal_length;
        }
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
                             const CcLimbMorphology *skeleton,
                             CcBiomechRig *rig)
{
    if (dimensions == NULL || skeleton == NULL || rig == NULL) return false;
    CcBiomechMorphology morphology;
    CcBiomechMorphologyInit(&morphology);
    float scale = dimensions->body_height / 1.0f;
    int32_t root = CcBiomechAddBone(
        &morphology, "body", -1, dimensions->body_length,
        18.0f * scale, 0.50f);
    if (root < 0) return false;
    for (int32_t limb = 0; limb < skeleton->limb_count; ++limb) {
        const CcLimbSpec *spec = &skeleton->limbs[limb];
        int32_t parent = root;
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            const char *bone_name = segment == 0 ? "proximal limb" :
                                    segment == 1 ? "middle limb" :
                                                   "terminal limb";
            const char *joint_name = segment == 0 ? "proximal" :
                                     segment == 1 ? "middle" : "terminal";
            float mass = (3.8f - (float)segment * 1.2f) * scale;
            int32_t bone = CcBiomechAddBone(
                &morphology, bone_name, parent, spec->segment_length[segment],
                fmaxf(0.8f * scale, mass), 0.44f);
            if (bone < 0) return false;
            float rest_angle = segment == 0 ? 0.0f : 0.18f;
            float lower_limit = segment == 0 ? -2.80f : 0.0f;
            float inertia = (0.12f - (float)segment * 0.025f) * scale;
            int32_t joint = CcBiomechAddJoint(
                &morphology, joint_name, parent, bone, rest_angle,
                lower_limit, 2.85f, fmaxf(0.045f * scale, inertia),
                1.2f - (float)segment * 0.18f,
                0.9f - (float)segment * 0.10f,
                24.0f + (float)segment * 8.0f);
            float force = (920.0f - (float)segment * 160.0f) * scale;
            float arm = (0.040f - (float)segment * 0.004f) * scale;
            if (joint < 0 ||
                !AddMusclePair(&morphology, joint, force, arm)) {
                return false;
            }
            parent = bone;
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
                                    CcLimbVec3 forward, float delta_time)
{
    int32_t joint_index = 0;
    for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
        const CcLimbRuntime *runtime = &skeleton->limbs[limb];
        int32_t segment_count = skeleton->morphology.limbs[limb].segment_count;
        float limb_phase = Wrap01(phase +
            skeleton->morphology.limbs[limb].phase_offset);
        float lead = sinf(limb_phase * 2.0f * CC_CREATURE_PI) *
                     0.18f * movement;
        CcLimbVec3 previous = {0.0f, -1.0f, 0.0f};
        for (int32_t segment = 0; segment < segment_count; ++segment) {
            CcLimbVec3 current = Subtract(runtime->joints[segment + 1],
                                          runtime->joints[segment]);
            float angle = segment == 0 ?
                atan2f(Dot(current, forward), -current.y) :
                AngleBetween(previous, current);
            CcBiomechRigConstrainJoint(muscles, joint_index, angle,
                                       fmaxf(delta_time, 1.0f / 240.0f));
            float bend_lead = segment == 0 ? lead :
                              fabsf(lead) * (0.72f - 0.12f * (float)segment);
            CcBiomechRigDriveJoint(
                muscles, joint_index, angle + bend_lead,
                0.38f + movement * (0.52f - 0.06f * (float)segment));
            previous = current;
            joint_index += 1;
        }
    }
    delta_time = Clamp(delta_time, 1.0f / 240.0f, 1.0f / 30.0f);
    float muscle_step = delta_time / 6.0f;
    for (int32_t step = 0; step < 6; ++step) {
        CcBiomechRigStep(muscles, muscle_step);
    }
    joint_index = 0;
    for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
        int32_t segment_count = pose->limbs[limb].segment_count;
        for (int32_t segment = 0; segment < segment_count; ++segment) {
            int32_t muscle = joint_index * 2;
            pose->limbs[limb].segment_activation[segment] =
                (muscles->muscles[muscle].activation +
                 muscles->muscles[muscle + 1].activation) * 0.5f;
            joint_index += 1;
        }
    }
    pose->mean_activation = CcBiomechRigMeanActivation(muscles);
}

static float SmootherStep(float amount)
{
    amount = Clamp(amount, 0.0f, 1.0f);
    return amount * amount * amount *
           (amount * (amount * 6.0f - 15.0f) + 10.0f);
}

static float SmoothLift(float amount)
{
    amount = Clamp(amount, 0.0f, 1.0f);
    float product = amount * (1.0f - amount);
    return 64.0f * product * product * product;
}

static bool FillPose(CcCreatureRigProfile profile,
                     const CcCreatureRigDimensions *dimensions,
                     const CcLimbRig *skeleton, CcBiomechRig *muscles,
                     CcLimbVec3 body, CcLimbVec3 offset,
                     CcLimbVec3 forward, CcLimbVec3 right,
                     float phase, float movement, float delta_time,
                     CcCreatureRigPose *pose)
{
    if (dimensions == NULL || skeleton == NULL || muscles == NULL ||
        pose == NULL || !skeleton->initialized || !muscles->initialized) {
        return false;
    }
    *pose = (CcCreatureRigPose){0};
    pose->profile = profile;
    pose->phase = Wrap01(phase);
    pose->movement = Clamp(movement, 0.0f, 1.0f);
    pose->body = Add(body, offset);
    pose->forward = forward;
    pose->right = right;
    pose->body_width = dimensions->body_width;
    pose->body_depth = dimensions->body_depth;
    pose->body_length = dimensions->body_length;
    pose->limb_count = skeleton->morphology.limb_count;
    pose->support_margin = skeleton->support_margin;
    pose->drive_scale = skeleton->drive_scale;
    pose->biomech_bone_count = muscles->morphology.bone_count;
    pose->biomech_joint_count = muscles->morphology.joint_count;
    pose->biomech_muscle_count = muscles->morphology.muscle_count;
    for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
        CcCreatureRigLimbPose *limb_pose = &pose->limbs[limb];
        const CcLimbRuntime *runtime = &skeleton->limbs[limb];
        int32_t segment_count = skeleton->morphology.limbs[limb].segment_count;
        limb_pose->state = runtime->state;
        limb_pose->segment_count = segment_count;
        if (runtime->state == CC_LIMB_STANCE) pose->planted_count += 1;
        if (runtime->state == CC_LIMB_SWING) pose->swinging_count += 1;
        for (int32_t joint = 0; joint <= segment_count; ++joint) {
            limb_pose->joints[joint] = Add(runtime->joints[joint], offset);
        }
    }
    ResolveMuscleActivation(muscles, skeleton, pose, pose->phase,
                            pose->movement, forward, delta_time);
    pose->valid = true;
    return true;
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
        float half_stride = dimensions.step_threshold * 1.25f * movement;
        bool swinging = movement > 0.05f &&
                        limb_phase >= morphology.duty_factor;
        float swing_progress = swinging ?
            (limb_phase - morphology.duty_factor) /
            (1.0f - morphology.duty_factor) : 0.0f;
        if (swinging) {
            float eased = SmootherStep(swing_progress);
            contact_local.z += -half_stride + 2.0f * half_stride * eased;
            contact_local.y += SmoothLift(swing_progress) *
                               dimensions.step_height;
        } else if (movement > 0.05f) {
            float stance_progress = limb_phase / morphology.duty_factor;
            contact_local.z += half_stride * (1.0f - 2.0f * stance_progress);
        }
        CcLimbVec3 contact = TransformPoint(body, contact_local, yaw);
        CcLimbRigPinContact(&skeleton, limb, body, yaw, contact,
                            (CcLimbVec3){0.0f, 1.0f, 0.0f});
        skeleton.limbs[limb].state = swinging ? CC_LIMB_SWING : CC_LIMB_STANCE;
        skeleton.limbs[limb].swing_progress = swinging ? swing_progress : 1.0f;
    }

    CcBiomechRig muscles;
    if (!ConfigureMuscles(&dimensions, &morphology, &muscles)) {
        return false;
    }
    skeleton.planted_count = 0;
    skeleton.swinging_count = 0;
    for (int32_t limb = 0; limb < morphology.limb_count; ++limb) {
        if (skeleton.limbs[limb].state == CC_LIMB_STANCE) {
            skeleton.planted_count += 1;
        } else if (skeleton.limbs[limb].state == CC_LIMB_SWING) {
            skeleton.swinging_count += 1;
        }
    }
    return FillPose(profile, &dimensions, &skeleton, &muscles, body,
                    (CcLimbVec3){0},
                    (CcLimbVec3){sinf(yaw), 0.0f, cosf(yaw)},
                    (CcLimbVec3){cosf(yaw), 0.0f, -sinf(yaw)},
                    phase, movement, 1.0f / 60.0f, pose);
}

bool CcCreatureRigControllerInit(CcCreatureRigController *controller,
                                 CcCreatureRigProfile profile,
                                 float phase, float scale)
{
    if (controller == NULL || !isfinite(phase) || !isfinite(scale) ||
        scale <= 0.0f) {
        return false;
    }
    *controller = (CcCreatureRigController){0};
    CcCreatureRigDimensions dimensions;
    if (!DimensionsForProfile(profile, &dimensions)) return false;
    ScaleDimensions(&dimensions, scale);
    CcLimbMorphology morphology;
    if (!ConfigureSkeleton(&dimensions, &morphology)) return false;
    CcLimbVec3 body = {0.0f, dimensions.body_height, 0.0f};
    CcLimbRigInit(&controller->skeleton, &morphology, body, 0.0f,
                  NULL, NULL);
    if (!controller->skeleton.initialized ||
        !ConfigureMuscles(&dimensions, &morphology, &controller->muscles)) {
        *controller = (CcCreatureRigController){0};
        return false;
    }
    controller->skeleton.gait_phase = Wrap01(phase);
    controller->profile = profile;
    controller->gait = CC_CREATURE_RIG_GAIT_WALK;
    controller->requested_gait = CC_CREATURE_RIG_GAIT_WALK;
    controller->scale = scale;
    controller->initialized = true;
    return true;
}

static bool ConfigureHorseGait(CcLimbMorphology *morphology,
                               CcCreatureRigGait gait)
{
    if (morphology == NULL ||
        morphology->preset != CC_MORPHOLOGY_QUADRUPED) {
        return false;
    }
    switch (gait) {
        case CC_CREATURE_RIG_GAIT_WALK:
            morphology->minimum_supports = 3;
            morphology->maximum_swings = 1;
            morphology->duty_factor = 0.69f;
            morphology->swing_seconds = 0.24f;
            morphology->velocity_lead = 0.12f;
            morphology->limbs[0].phase_offset = 0.00f;
            morphology->limbs[1].phase_offset = 0.50f;
            morphology->limbs[2].phase_offset = 0.75f;
            morphology->limbs[3].phase_offset = 0.25f;
            return true;
        case CC_CREATURE_RIG_GAIT_TROT:
            morphology->minimum_supports = 2;
            morphology->maximum_swings = 2;
            morphology->duty_factor = 0.58f;
            morphology->swing_seconds = 0.20f;
            morphology->velocity_lead = 0.15f;
            morphology->limbs[0].phase_offset = 0.00f;
            morphology->limbs[1].phase_offset = 0.50f;
            morphology->limbs[2].phase_offset = 0.50f;
            morphology->limbs[3].phase_offset = 0.00f;
            return true;
        case CC_CREATURE_RIG_GAIT_CANTER:
            morphology->minimum_supports = 2;
            morphology->maximum_swings = 2;
            morphology->duty_factor = 0.54f;
            morphology->swing_seconds = 0.18f;
            morphology->velocity_lead = 0.18f;
            morphology->limbs[0].phase_offset = 0.50f;
            morphology->limbs[1].phase_offset = 0.68f;
            morphology->limbs[2].phase_offset = 0.00f;
            morphology->limbs[3].phase_offset = 0.18f;
            return true;
        case CC_CREATURE_RIG_GAIT_COUNT:
        default:
            return false;
    }
}

bool CcCreatureRigControllerSetGait(CcCreatureRigController *controller,
                                    CcCreatureRigGait gait)
{
    if (controller == NULL || !controller->initialized || gait < 0 ||
        gait >= CC_CREATURE_RIG_GAIT_COUNT) {
        return false;
    }
    if (controller->profile != CC_CREATURE_RIG_HORSE) {
        return gait == CC_CREATURE_RIG_GAIT_WALK;
    }
    controller->requested_gait = gait;
    if (controller->gait == gait) return true;

    int32_t maximum_swings = gait == CC_CREATURE_RIG_GAIT_WALK ? 1 : 2;
    if (controller->skeleton.swinging_count > maximum_swings) {
        return false;
    }
    if (!ConfigureHorseGait(&controller->skeleton.morphology, gait)) {
        return false;
    }
    controller->gait = gait;
    return true;
}

bool CcCreatureRigControllerStep(CcCreatureRigController *controller,
                                 float forward_speed, float movement,
                                 float delta_time,
                                 CcCreatureRigPose *pose)
{
    if (controller == NULL || pose == NULL || !controller->initialized ||
        !isfinite(forward_speed) || !isfinite(movement) ||
        !isfinite(delta_time) || delta_time < 0.0f) {
        return false;
    }
    movement = Clamp(movement, 0.0f, 1.0f);
    delta_time = Clamp(delta_time, 0.0f, 0.25f);
    float response = 1.0f - expf(-12.0f * delta_time);
    controller->movement += (movement - controller->movement) * response;
    float speed_limit = 6.0f * controller->scale;
    float speed = Clamp(forward_speed, -speed_limit, speed_limit) *
                  controller->movement;
    float remaining = delta_time;
    do {
        float step = fminf(remaining, 1.0f / 60.0f);
        if (delta_time <= 0.0f) step = 0.0f;
        controller->ground_position.z += speed * step;
        CcLimbVec3 body = controller->ground_position;
        body.y += controller->skeleton.morphology.body_height;
        int32_t declared_maximum_swings =
            controller->skeleton.morphology.maximum_swings;
        bool walking_requested =
            controller->profile == CC_CREATURE_RIG_HORSE &&
            controller->gait != CC_CREATURE_RIG_GAIT_WALK &&
            controller->requested_gait == CC_CREATURE_RIG_GAIT_WALK;
        if (walking_requested) {
            /* Let already-airborne hooves finish, but do not schedule a
               replacement pair while the walking policy is waiting. */
            controller->skeleton.morphology.maximum_swings = 1;
        }
        CcLimbRigUpdate(&controller->skeleton, body, 0.0f,
                        (CcLimbVec3){0.0f, 0.0f, speed}, true, step,
                        NULL, NULL);
        controller->skeleton.morphology.maximum_swings =
            declared_maximum_swings;
        if (walking_requested &&
            controller->skeleton.swinging_count <= 1 &&
            ConfigureHorseGait(&controller->skeleton.morphology,
                               CC_CREATURE_RIG_GAIT_WALK)) {
            controller->gait = CC_CREATURE_RIG_GAIT_WALK;
        }
        remaining -= step;
    } while (remaining > 0.000001f);

    CcCreatureRigDimensions dimensions;
    if (!DimensionsForProfile(controller->profile, &dimensions)) return false;
    ScaleDimensions(&dimensions, controller->scale);
    CcLimbVec3 body = controller->ground_position;
    body.y += dimensions.body_height;
    CcLimbVec3 offset = {-controller->ground_position.x,
                         -controller->ground_position.y,
                         -controller->ground_position.z};
    return FillPose(controller->profile, &dimensions, &controller->skeleton,
                    &controller->muscles, body, offset,
                    (CcLimbVec3){0.0f, 0.0f, 1.0f},
                    (CcLimbVec3){1.0f, 0.0f, 0.0f},
                    controller->skeleton.gait_phase, controller->movement,
                    fmaxf(delta_time, 1.0f / 240.0f), pose);
}

const char *CcCreatureRigProfileName(CcCreatureRigProfile profile)
{
    switch (profile) {
        case CC_CREATURE_RIG_GOBLIN: return "GOBLIN";
        case CC_CREATURE_RIG_HORSE: return "PONY";
        case CC_CREATURE_RIG_COW: return "COW";
        case CC_CREATURE_RIG_DRAGON: return "DRAGON";
        case CC_CREATURE_RIG_HEXAPOD: return "HEXAPOD";
        case CC_CREATURE_RIG_OCTOPOD: return "OCTOPOD";
        case CC_CREATURE_RIG_PROFILE_COUNT:
        default:
            return "UNKNOWN";
    }
}

const char *CcCreatureRigGaitName(CcCreatureRigGait gait)
{
    switch (gait) {
        case CC_CREATURE_RIG_GAIT_WALK: return "WALK";
        case CC_CREATURE_RIG_GAIT_TROT: return "TROT";
        case CC_CREATURE_RIG_GAIT_CANTER: return "CANTER";
        case CC_CREATURE_RIG_GAIT_COUNT:
        default:
            return "UNKNOWN";
    }
}
