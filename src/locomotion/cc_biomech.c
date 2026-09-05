#include "locomotion/cc_biomech.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static float Clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static CcBiomechVec3 AddVec3(CcBiomechVec3 a, CcBiomechVec3 b)
{
    return (CcBiomechVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static CcBiomechVec3 SubtractVec3(CcBiomechVec3 a, CcBiomechVec3 b)
{
    return (CcBiomechVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CcBiomechVec3 ScaleVec3(CcBiomechVec3 value, float scale)
{
    return (CcBiomechVec3){value.x * scale, value.y * scale, value.z * scale};
}

static float DotVec3(CcBiomechVec3 a, CcBiomechVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float LengthVec3(CcBiomechVec3 value)
{
    return sqrtf(DotVec3(value, value));
}

static void CopyName(char destination[CC_BIOMECH_NAME_LENGTH], const char *name)
{
    if (name == NULL) name = "unnamed";
    (void)snprintf(destination, CC_BIOMECH_NAME_LENGTH, "%s", name);
}

void CcBiomechMorphologyInit(CcBiomechMorphology *morphology)
{
    if (morphology == NULL) return;
    *morphology = (CcBiomechMorphology){0};
}

int32_t CcBiomechAddBone(CcBiomechMorphology *morphology, const char *name,
                         int32_t parent_bone, float length, float mass,
                         float center_of_mass)
{
    if (morphology == NULL || morphology->bone_count >= CC_BIOMECH_MAX_BONES ||
        parent_bone >= morphology->bone_count || parent_bone < -1 ||
        length < 0.0f || mass <= 0.0f) return -1;
    int32_t index = morphology->bone_count;
    morphology->bone_count += 1;
    CcBiomechBoneSpec *bone = &morphology->bones[index];
    CopyName(bone->name, name);
    bone->parent_bone = parent_bone;
    bone->length = length;
    bone->mass = mass;
    bone->center_of_mass = Clamp(center_of_mass, 0.0f, 1.0f);
    return index;
}

int32_t CcBiomechAddJoint(CcBiomechMorphology *morphology, const char *name,
                          int32_t parent_bone, int32_t child_bone,
                          float rest_angle, float lower_limit,
                          float upper_limit, float inertia,
                          float passive_stiffness, float damping,
                          float ligament_stiffness)
{
    if (morphology == NULL || morphology->joint_count >= CC_BIOMECH_MAX_JOINTS ||
        parent_bone < 0 || parent_bone >= morphology->bone_count ||
        child_bone < 0 || child_bone >= morphology->bone_count ||
        lower_limit >= upper_limit || inertia <= 0.0f) return -1;
    int32_t index = morphology->joint_count;
    morphology->joint_count += 1;
    CcBiomechJointSpec *joint = &morphology->joints[index];
    CopyName(joint->name, name);
    joint->parent_bone = parent_bone;
    joint->child_bone = child_bone;
    joint->rest_angle = Clamp(rest_angle, lower_limit, upper_limit);
    joint->lower_limit = lower_limit;
    joint->upper_limit = upper_limit;
    joint->inertia = inertia;
    joint->passive_stiffness = fmaxf(0.0f, passive_stiffness);
    joint->damping = fmaxf(0.0f, damping);
    joint->ligament_stiffness = fmaxf(0.0f, ligament_stiffness);
    return index;
}

int32_t CcBiomechAddMuscle(CcBiomechMorphology *morphology, const char *name,
                           int32_t joint, float moment_arm,
                           float maximum_force, float optimal_angle,
                           float operating_width, float activation_rate,
                           float relaxation_rate)
{
    if (morphology == NULL || morphology->muscle_count >= CC_BIOMECH_MAX_MUSCLES ||
        joint < 0 || joint >= morphology->joint_count ||
        fabsf(moment_arm) < 0.0001f || maximum_force <= 0.0f ||
        operating_width <= 0.0f || activation_rate <= 0.0f ||
        relaxation_rate <= 0.0f) return -1;
    int32_t index = morphology->muscle_count;
    morphology->muscle_count += 1;
    CcBiomechMuscleSpec *muscle = &morphology->muscles[index];
    CopyName(muscle->name, name);
    muscle->joint = joint;
    muscle->moment_arm = moment_arm;
    muscle->maximum_force = maximum_force;
    muscle->optimal_angle = optimal_angle;
    muscle->operating_width = operating_width;
    muscle->activation_rate = activation_rate;
    muscle->relaxation_rate = relaxation_rate;
    return index;
}

bool CcBiomechRigInit(CcBiomechRig *rig,
                      const CcBiomechMorphology *morphology)
{
    if (rig == NULL) return false;
    *rig = (CcBiomechRig){0};
    if (morphology == NULL || morphology->bone_count <= 0 ||
        morphology->bone_count > CC_BIOMECH_MAX_BONES ||
        morphology->joint_count <= 0 ||
        morphology->joint_count > CC_BIOMECH_MAX_JOINTS ||
        morphology->muscle_count < 0 ||
        morphology->muscle_count > CC_BIOMECH_MAX_MUSCLES) return false;
    rig->morphology = *morphology;
    for (int32_t bone = 0; bone < morphology->bone_count; ++bone) {
        rig->total_mass += morphology->bones[bone].mass;
    }
    for (int32_t joint = 0; joint < morphology->joint_count; ++joint) {
        rig->joints[joint].angle = morphology->joints[joint].rest_angle;
        rig->joints[joint].target_angle = morphology->joints[joint].rest_angle;
        rig->joints[joint].effort = 0.45f;
    }
    rig->root.gravity = (CcBiomechVec3){0.0f, -9.81f, 0.0f};
    rig->root.linear_damping = 0.16f;
    rig->initialized = true;
    return true;
}

void CcBiomechRigDriveJoint(CcBiomechRig *rig, int32_t joint,
                            float target_angle, float effort)
{
    if (rig == NULL || !rig->initialized || joint < 0 ||
        joint >= rig->morphology.joint_count) return;
    const CcBiomechJointSpec *spec = &rig->morphology.joints[joint];
    rig->joints[joint].target_angle = Clamp(target_angle, spec->lower_limit,
                                             spec->upper_limit);
    rig->joints[joint].effort = Clamp(effort, 0.0f, 1.0f);
}

void CcBiomechRigApplyTorque(CcBiomechRig *rig, int32_t joint, float torque)
{
    if (rig == NULL || !rig->initialized || joint < 0 ||
        joint >= rig->morphology.joint_count) return;
    rig->joints[joint].external_torque += torque;
}

void CcBiomechRigSetBodyState(CcBiomechRig *rig, CcBiomechVec3 position,
                              CcBiomechVec3 velocity)
{
    if (rig == NULL || !rig->initialized) return;
    rig->root.position = position;
    rig->root.velocity = velocity;
    rig->root.acceleration = (CcBiomechVec3){0};
    rig->root.accumulated_force = (CcBiomechVec3){0};
    rig->root.last_applied_force = (CcBiomechVec3){0};
    rig->root.contact_impulse = (CcBiomechVec3){0};
}

void CcBiomechRigApplyBodyForce(CcBiomechRig *rig, CcBiomechVec3 force)
{
    if (rig == NULL || !rig->initialized) return;
    rig->root.accumulated_force = AddVec3(rig->root.accumulated_force, force);
}

void CcBiomechRigStepBody(CcBiomechRig *rig, float delta_time)
{
    if (rig == NULL || !rig->initialized || rig->total_mass <= 0.0f) return;
    if (!isfinite(delta_time) || delta_time <= 0.0f) return;
    delta_time = Clamp(delta_time, 0.0f, 1.0f / 30.0f);
    CcBiomechVec3 gravity_force = ScaleVec3(rig->root.gravity,
                                            rig->total_mass);
    CcBiomechVec3 damping_force = ScaleVec3(
        rig->root.velocity, -rig->root.linear_damping * rig->total_mass);
    CcBiomechVec3 force = AddVec3(rig->root.accumulated_force, gravity_force);
    force = AddVec3(force, damping_force);
    rig->root.last_applied_force = rig->root.accumulated_force;
    rig->root.acceleration = ScaleVec3(force, 1.0f / rig->total_mass);
    rig->root.velocity = AddVec3(rig->root.velocity,
                                 ScaleVec3(rig->root.acceleration, delta_time));
    rig->root.position = AddVec3(rig->root.position,
                                 ScaleVec3(rig->root.velocity, delta_time));
    rig->root.accumulated_force = (CcBiomechVec3){0};
    rig->root.contact_impulse = ScaleVec3(
        rig->root.contact_impulse, expf(-18.0f * delta_time));
}

void CcBiomechRigConstrainBody(CcBiomechRig *rig,
                               CcBiomechVec3 constrained_position,
                               CcBiomechVec3 constrained_velocity)
{
    if (rig == NULL || !rig->initialized) return;
    CcBiomechVec3 delta_velocity = {
        constrained_velocity.x - rig->root.velocity.x,
        constrained_velocity.y - rig->root.velocity.y,
        constrained_velocity.z - rig->root.velocity.z
    };
    rig->root.contact_impulse = ScaleVec3(delta_velocity, rig->total_mass);
    rig->root.position = constrained_position;
    rig->root.velocity = constrained_velocity;
}

static float LigamentTorque(const CcBiomechJointSpec *spec, float angle)
{
    float range = spec->upper_limit - spec->lower_limit;
    float zone = fmaxf(0.025f, range * 0.14f);
    float low_start = spec->lower_limit + zone;
    float high_start = spec->upper_limit - zone;
    if (angle < low_start) {
        float stretch = (low_start - angle) / zone;
        return spec->ligament_stiffness * stretch * stretch;
    }
    if (angle > high_start) {
        float stretch = (angle - high_start) / zone;
        return -spec->ligament_stiffness * stretch * stretch;
    }
    return 0.0f;
}

void CcBiomechRigStep(CcBiomechRig *rig, float delta_time)
{
    if (rig == NULL || !rig->initialized) return;
    if (!isfinite(delta_time) || delta_time <= 0.0f) return;
    delta_time = Clamp(delta_time, 0.0f, 1.0f / 30.0f);
    float contact_retention = expf(-18.0f * delta_time);
    for (int32_t joint = 0; joint < rig->morphology.joint_count; ++joint) {
        rig->joints[joint].muscle_torque = 0.0f;
        rig->joints[joint].passive_torque = 0.0f;
        rig->joints[joint].contact_reaction_torque *= contact_retention;
    }

    for (int32_t muscle = 0; muscle < rig->morphology.muscle_count; ++muscle) {
        const CcBiomechMuscleSpec *spec = &rig->morphology.muscles[muscle];
        CcBiomechMuscleRuntime *runtime = &rig->muscles[muscle];
        CcBiomechJointRuntime *joint = &rig->joints[spec->joint];
        float direction = spec->moment_arm > 0.0f ? 1.0f : -1.0f;
        float stretch_error = direction * (joint->target_angle - joint->angle);
        float velocity_error = direction * -joint->angular_velocity;
        float co_contraction = 0.035f + joint->effort * 0.075f;
        float reflex = stretch_error * 2.7f + velocity_error * 0.055f;
        runtime->excitation = Clamp(co_contraction +
                                    joint->effort * Clamp(reflex, 0.0f, 1.0f),
                                    0.0f, 1.0f);
        float rate = runtime->excitation > runtime->activation ?
                     spec->activation_rate : spec->relaxation_rate;
        runtime->activation += (runtime->excitation - runtime->activation) *
                               (1.0f - expf(-rate * delta_time));
        float normalized_length = (joint->angle - spec->optimal_angle) /
                                  spec->operating_width;
        float force_length = 0.30f + 0.70f * expf(-normalized_length *
                                                  normalized_length);
        runtime->tension = runtime->activation * spec->maximum_force *
                           force_length;
        joint->muscle_torque += runtime->tension * spec->moment_arm;
    }

    for (int32_t joint = 0; joint < rig->morphology.joint_count; ++joint) {
        const CcBiomechJointSpec *spec = &rig->morphology.joints[joint];
        CcBiomechJointRuntime *runtime = &rig->joints[joint];
        runtime->passive_torque =
            -(runtime->angle - spec->rest_angle) * spec->passive_stiffness -
            runtime->angular_velocity * spec->damping +
            LigamentTorque(spec, runtime->angle);
        float torque = runtime->muscle_torque + runtime->passive_torque +
                       runtime->external_torque;
        float acceleration = torque / spec->inertia;
        runtime->angular_velocity += acceleration * delta_time;
        runtime->angular_velocity = Clamp(runtime->angular_velocity, -18.0f, 18.0f);
        runtime->angle += runtime->angular_velocity * delta_time;
        if (runtime->angle < spec->lower_limit) {
            runtime->angle = spec->lower_limit;
            if (runtime->angular_velocity < 0.0f) runtime->angular_velocity *= -0.08f;
        } else if (runtime->angle > spec->upper_limit) {
            runtime->angle = spec->upper_limit;
            if (runtime->angular_velocity > 0.0f) runtime->angular_velocity *= -0.08f;
        }
        runtime->external_torque = 0.0f;
    }
}

void CcBiomechRigConstrainJoint(CcBiomechRig *rig, int32_t joint,
                                float constrained_angle, float delta_time)
{
    if (rig == NULL || !rig->initialized || joint < 0 ||
        joint >= rig->morphology.joint_count) return;
    const CcBiomechJointSpec *spec = &rig->morphology.joints[joint];
    CcBiomechJointRuntime *runtime = &rig->joints[joint];
    float old_velocity = runtime->angular_velocity;
    float old_angle = runtime->angle;
    runtime->angle = Clamp(constrained_angle, spec->lower_limit, spec->upper_limit);
    if (delta_time > 0.00001f) {
        runtime->angular_velocity = Clamp((runtime->angle - old_angle) /
                                          delta_time, -18.0f, 18.0f);
        runtime->contact_reaction_torque = Clamp(
            (runtime->angular_velocity - old_velocity) * spec->inertia /
            delta_time, -240.0f, 240.0f);
    }
}

float CcBiomechRigJointAngle(const CcBiomechRig *rig, int32_t joint)
{
    if (rig == NULL || !rig->initialized || joint < 0 ||
        joint >= rig->morphology.joint_count) return 0.0f;
    return rig->joints[joint].angle;
}

float CcBiomechRigMeanActivation(const CcBiomechRig *rig)
{
    if (rig == NULL || !rig->initialized || rig->morphology.muscle_count <= 0) {
        return 0.0f;
    }
    float total = 0.0f;
    for (int32_t muscle = 0; muscle < rig->morphology.muscle_count; ++muscle) {
        total += rig->muscles[muscle].activation;
    }
    return total / (float)rig->morphology.muscle_count;
}

void CcBiomechRagdollInit(CcBiomechRagdoll *ragdoll)
{
    if (ragdoll == NULL) return;
    *ragdoll = (CcBiomechRagdoll){0};
    ragdoll->gravity = (CcBiomechVec3){0.0f, -9.81f, 0.0f};
    ragdoll->damping = 0.004f;
    ragdoll->restitution = 0.015f;
    ragdoll->collision_friction = 0.30f;
    ragdoll->contact_damping = 0.48f;
    ragdoll->resting_contact_damping = 0.040f;
}

int32_t CcBiomechRagdollAddParticle(CcBiomechRagdoll *ragdoll,
                                    CcBiomechVec3 position,
                                    float inverse_mass, float radius)
{
    if (ragdoll == NULL ||
        ragdoll->particle_count >= CC_BIOMECH_MAX_RAGDOLL_PARTICLES ||
        inverse_mass < 0.0f || radius < 0.0f) return -1;
    int32_t index = ragdoll->particle_count;
    ragdoll->particle_count += 1;
    CcBiomechRagdollParticle *particle = &ragdoll->particles[index];
    particle->position = position;
    particle->previous_position = position;
    particle->inverse_mass = inverse_mass;
    particle->radius = radius;
    return index;
}

int32_t CcBiomechRagdollAddConstraint(CcBiomechRagdoll *ragdoll,
                                      int32_t particle_a,
                                      int32_t particle_b, float compliance)
{
    if (ragdoll == NULL ||
        ragdoll->constraint_count >= CC_BIOMECH_MAX_RAGDOLL_CONSTRAINTS ||
        particle_a < 0 || particle_a >= ragdoll->particle_count ||
        particle_b < 0 || particle_b >= ragdoll->particle_count ||
        particle_a == particle_b || compliance < 0.0f) return -1;
    int32_t index = ragdoll->constraint_count;
    ragdoll->constraint_count += 1;
    CcBiomechRagdollConstraint *constraint = &ragdoll->constraints[index];
    constraint->particle_a = particle_a;
    constraint->particle_b = particle_b;
    CcBiomechVec3 difference = {
        ragdoll->particles[particle_b].position.x -
            ragdoll->particles[particle_a].position.x,
        ragdoll->particles[particle_b].position.y -
            ragdoll->particles[particle_a].position.y,
        ragdoll->particles[particle_b].position.z -
            ragdoll->particles[particle_a].position.z
    };
    constraint->rest_length = sqrtf(difference.x * difference.x +
                                    difference.y * difference.y +
                                    difference.z * difference.z);
    constraint->compliance = compliance;
    return index;
}

int32_t CcBiomechRagdollAddAngleConstraint(
    CcBiomechRagdoll *ragdoll, int32_t particle_a, int32_t joint_particle,
    int32_t particle_b, float minimum_angle, float maximum_angle,
    float compliance)
{
    if (ragdoll == NULL ||
        ragdoll->angle_constraint_count >=
            CC_BIOMECH_MAX_RAGDOLL_ANGLE_CONSTRAINTS ||
        particle_a < 0 || particle_a >= ragdoll->particle_count ||
        joint_particle < 0 || joint_particle >= ragdoll->particle_count ||
        particle_b < 0 || particle_b >= ragdoll->particle_count ||
        particle_a == joint_particle || joint_particle == particle_b ||
        particle_a == particle_b || minimum_angle < 0.0f ||
        maximum_angle > 3.14159265358979323846f ||
        minimum_angle >= maximum_angle || compliance < 0.0f) {
        return -1;
    }
    int32_t index = ragdoll->angle_constraint_count++;
    CcBiomechRagdollAngleConstraint *constraint =
        &ragdoll->angle_constraints[index];
    *constraint = (CcBiomechRagdollAngleConstraint){
        .particle_a = particle_a,
        .joint_particle = joint_particle,
        .particle_b = particle_b,
        .minimum_angle = minimum_angle,
        .maximum_angle = maximum_angle,
        .compliance = compliance,
    };
    return index;
}

int32_t CcBiomechRagdollAddHingeConstraint(
    CcBiomechRagdoll *ragdoll, int32_t particle_a, int32_t joint_particle,
    int32_t particle_b, int32_t axis_particle_a, int32_t axis_particle_b,
    float minimum_angle, float maximum_angle, float maximum_splay_angle,
    float compliance)
{
    if (ragdoll == NULL ||
        ragdoll->hinge_constraint_count >=
            CC_BIOMECH_MAX_RAGDOLL_HINGE_CONSTRAINTS ||
        particle_a < 0 || particle_a >= ragdoll->particle_count ||
        joint_particle < 0 || joint_particle >= ragdoll->particle_count ||
        particle_b < 0 || particle_b >= ragdoll->particle_count ||
        axis_particle_a < 0 || axis_particle_a >= ragdoll->particle_count ||
        axis_particle_b < 0 || axis_particle_b >= ragdoll->particle_count ||
        particle_a == joint_particle || joint_particle == particle_b ||
        particle_a == particle_b || axis_particle_a == axis_particle_b ||
        minimum_angle < 0.0f ||
        maximum_angle > 3.14159265358979323846f ||
        minimum_angle >= maximum_angle || maximum_splay_angle <= 0.0f ||
        maximum_splay_angle >= 1.57079632679489661923f ||
        compliance < 0.0f) {
        return -1;
    }
    CcBiomechVec3 axis = SubtractVec3(
        ragdoll->particles[axis_particle_b].position,
        ragdoll->particles[axis_particle_a].position);
    float axis_length = LengthVec3(axis);
    if (axis_length <= 0.00001f) return -1;
    axis = ScaleVec3(axis, 1.0f / axis_length);
    CcBiomechVec3 child = SubtractVec3(
        ragdoll->particles[particle_b].position,
        ragdoll->particles[joint_particle].position);
    int32_t index = ragdoll->hinge_constraint_count++;
    ragdoll->hinge_constraints[index] = (CcBiomechRagdollHingeConstraint){
        .particle_a = particle_a,
        .joint_particle = joint_particle,
        .particle_b = particle_b,
        .axis_particle_a = axis_particle_a,
        .axis_particle_b = axis_particle_b,
        .minimum_angle = minimum_angle,
        .maximum_angle = maximum_angle,
        .rest_lateral_offset = DotVec3(child, axis),
        .maximum_splay_angle = maximum_splay_angle,
        .passive_splay_angle = maximum_splay_angle,
        .compliance = compliance,
    };
    return index;
}

int32_t CcBiomechRagdollAddCollisionSegment(
    CcBiomechRagdoll *ragdoll, int32_t particle_a, int32_t particle_b,
    float radius)
{
    if (ragdoll == NULL ||
        ragdoll->collision_segment_count >=
            CC_BIOMECH_MAX_RAGDOLL_COLLISION_SEGMENTS ||
        particle_a < 0 || particle_a >= ragdoll->particle_count ||
        particle_b < 0 || particle_b >= ragdoll->particle_count ||
        particle_a == particle_b || radius <= 0.0f) {
        return -1;
    }
    int32_t index = ragdoll->collision_segment_count++;
    ragdoll->collision_segments[index] =
        (CcBiomechRagdollCollisionSegment){particle_a, particle_b, radius};
    return index;
}

int32_t CcBiomechRagdollAddExclusion(CcBiomechRagdoll *ragdoll,
                                     int32_t particle_a,
                                     int32_t particle_b,
                                     float minimum_distance)
{
    if (ragdoll == NULL ||
        ragdoll->exclusion_count >= CC_BIOMECH_MAX_RAGDOLL_EXCLUSIONS ||
        particle_a < 0 || particle_a >= ragdoll->particle_count ||
        particle_b < 0 || particle_b >= ragdoll->particle_count ||
        particle_a == particle_b || minimum_distance <= 0.0f) {
        return -1;
    }
    int32_t index = ragdoll->exclusion_count++;
    ragdoll->exclusions[index] =
        (CcBiomechRagdollExclusion){particle_a, particle_b, minimum_distance};
    return index;
}

void CcBiomechRagdollSetVelocity(CcBiomechRagdoll *ragdoll,
                                 CcBiomechVec3 velocity, float delta_time)
{
    if (ragdoll == NULL || delta_time <= 0.0f) return;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        CcBiomechRagdollParticle *runtime = &ragdoll->particles[particle];
        runtime->previous_position.x = runtime->position.x -
                                       velocity.x * delta_time;
        runtime->previous_position.y = runtime->position.y -
                                       velocity.y * delta_time;
        runtime->previous_position.z = runtime->position.z -
                                       velocity.z * delta_time;
    }
}

static void SolveRagdollConstraint(CcBiomechRagdoll *ragdoll,
                                   const CcBiomechRagdollConstraint *constraint,
                                   float delta_time)
{
    CcBiomechRagdollParticle *a =
        &ragdoll->particles[constraint->particle_a];
    CcBiomechRagdollParticle *b =
        &ragdoll->particles[constraint->particle_b];
    CcBiomechVec3 delta = {b->position.x - a->position.x,
                           b->position.y - a->position.y,
                           b->position.z - a->position.z};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y +
                         delta.z * delta.z);
    float weight = a->inverse_mass + b->inverse_mass;
    if (length <= 0.00001f || weight <= 0.0f) return;
    float softness = constraint->compliance /
                     fmaxf(delta_time * delta_time, 0.000001f);
    float correction_scale = (length - constraint->rest_length) /
                             (length * (weight + softness));
    CcBiomechVec3 correction = ScaleVec3(delta, correction_scale);
    a->position = AddVec3(a->position,
                          ScaleVec3(correction, a->inverse_mass));
    b->position = AddVec3(b->position,
                          ScaleVec3(correction, -b->inverse_mass));
}

static void SolveRagdollAngleConstraint(
    CcBiomechRagdoll *ragdoll,
    const CcBiomechRagdollAngleConstraint *constraint, float delta_time)
{
    CcBiomechRagdollParticle *a =
        &ragdoll->particles[constraint->particle_a];
    CcBiomechRagdollParticle *joint =
        &ragdoll->particles[constraint->joint_particle];
    CcBiomechRagdollParticle *b =
        &ragdoll->particles[constraint->particle_b];
    CcBiomechVec3 arm_a = SubtractVec3(a->position, joint->position);
    CcBiomechVec3 arm_b = SubtractVec3(b->position, joint->position);
    float length_a = LengthVec3(arm_a);
    float length_b = LengthVec3(arm_b);
    if (length_a <= 0.00001f || length_b <= 0.00001f) return;
    CcBiomechVec3 normal_a = ScaleVec3(arm_a, 1.0f / length_a);
    CcBiomechVec3 normal_b = ScaleVec3(arm_b, 1.0f / length_b);
    float cosine = Clamp(DotVec3(normal_a, normal_b), -1.0f, 1.0f);
    float angle = acosf(cosine);
    float target = Clamp(angle, constraint->minimum_angle,
                         constraint->maximum_angle);
    float error = angle - target;
    if (fabsf(error) <= 0.00001f) return;
    float sine = sqrtf(fmaxf(1.0f - cosine * cosine, 0.0f));
    if (sine <= 0.0001f) return;

    CcBiomechVec3 gradient_a = ScaleVec3(
        SubtractVec3(ScaleVec3(normal_a, cosine), normal_b),
        1.0f / (length_a * sine));
    CcBiomechVec3 gradient_b = ScaleVec3(
        SubtractVec3(ScaleVec3(normal_b, cosine), normal_a),
        1.0f / (length_b * sine));
    CcBiomechVec3 gradient_joint =
        ScaleVec3(AddVec3(gradient_a, gradient_b), -1.0f);
    float weighted_gradient =
        a->inverse_mass * DotVec3(gradient_a, gradient_a) +
        joint->inverse_mass * DotVec3(gradient_joint, gradient_joint) +
        b->inverse_mass * DotVec3(gradient_b, gradient_b);
    float softness = constraint->compliance /
        fmaxf(delta_time * delta_time, 0.000001f);
    if (weighted_gradient + softness <= 0.000001f) return;
    float lambda = -error / (weighted_gradient + softness);
    a->position = AddVec3(
        a->position, ScaleVec3(gradient_a, lambda * a->inverse_mass));
    joint->position = AddVec3(
        joint->position,
        ScaleVec3(gradient_joint, lambda * joint->inverse_mass));
    b->position = AddVec3(
        b->position, ScaleVec3(gradient_b, lambda * b->inverse_mass));
}

static void SolveRagdollHingeConstraint(
    CcBiomechRagdoll *ragdoll,
    const CcBiomechRagdollHingeConstraint *constraint, float delta_time)
{
    CcBiomechRagdollAngleConstraint bend = {
        .particle_a = constraint->particle_a,
        .joint_particle = constraint->joint_particle,
        .particle_b = constraint->particle_b,
        .minimum_angle = constraint->minimum_angle,
        .maximum_angle = constraint->maximum_angle,
        .compliance = constraint->compliance,
    };
    SolveRagdollAngleConstraint(ragdoll, &bend, delta_time);

    CcBiomechRagdollParticle *joint =
        &ragdoll->particles[constraint->joint_particle];
    CcBiomechRagdollParticle *child =
        &ragdoll->particles[constraint->particle_b];
    CcBiomechVec3 axis = SubtractVec3(
        ragdoll->particles[constraint->axis_particle_b].position,
        ragdoll->particles[constraint->axis_particle_a].position);
    float axis_length = LengthVec3(axis);
    CcBiomechVec3 child_arm = SubtractVec3(child->position, joint->position);
    float child_length = LengthVec3(child_arm);
    float weight = joint->inverse_mass + child->inverse_mass;
    if (axis_length <= 0.00001f || child_length <= 0.00001f ||
        weight <= 0.0f) {
        return;
    }
    axis = ScaleVec3(axis, 1.0f / axis_length);
    float lateral = DotVec3(child_arm, axis);
    float allowed = child_length * sinf(constraint->maximum_splay_angle);
    float target = Clamp(
        lateral,
        constraint->rest_lateral_offset - allowed,
        constraint->rest_lateral_offset + allowed);
    float error = lateral - target;
    if (fabsf(error) <= 0.00001f) return;
    CcBiomechVec3 tangent = SubtractVec3(child_arm, ScaleVec3(axis, lateral));
    float tangent_length = LengthVec3(tangent);
    if (tangent_length <= 0.00001f) {
        CcBiomechVec3 parent_arm = SubtractVec3(
            ragdoll->particles[constraint->particle_a].position,
            joint->position);
        tangent = SubtractVec3(parent_arm,
                               ScaleVec3(axis, DotVec3(parent_arm, axis)));
        tangent_length = LengthVec3(tangent);
        if (tangent_length <= 0.00001f) {
            CcBiomechVec3 reference = fabsf(axis.y) < 0.82f ?
                (CcBiomechVec3){0.0f, 1.0f, 0.0f} :
                (CcBiomechVec3){1.0f, 0.0f, 0.0f};
            tangent = SubtractVec3(reference,
                                   ScaleVec3(axis, DotVec3(reference, axis)));
            tangent_length = LengthVec3(tangent);
        }
    }
    target = Clamp(target, -child_length, child_length);
    float target_tangent = sqrtf(fmaxf(
        child_length * child_length - target * target, 0.0f));
    CcBiomechVec3 target_arm = AddVec3(
        ScaleVec3(axis, target),
        ScaleVec3(tangent, target_tangent / tangent_length));
    // Rotate the arm to its splay limit while preserving its bone length.
    CcBiomechVec3 correction = SubtractVec3(target_arm, child_arm);
    float softness = constraint->compliance /
        fmaxf(delta_time * delta_time, 0.000001f);
    CcBiomechVec3 child_correction =
        ScaleVec3(correction, child->inverse_mass / (weight + softness));
    CcBiomechVec3 joint_correction =
        ScaleVec3(correction, -joint->inverse_mass / (weight + softness));
    child->position = AddVec3(child->position, child_correction);
    joint->position = AddVec3(joint->position, joint_correction);
}

static void SolveRagdollExclusions(CcBiomechRagdoll *ragdoll)
{
    for (int32_t exclusion_index = 0;
         exclusion_index < ragdoll->exclusion_count; ++exclusion_index) {
        const CcBiomechRagdollExclusion *exclusion =
            &ragdoll->exclusions[exclusion_index];
        CcBiomechRagdollParticle *a =
            &ragdoll->particles[exclusion->particle_a];
        CcBiomechRagdollParticle *b =
            &ragdoll->particles[exclusion->particle_b];
        CcBiomechVec3 delta = SubtractVec3(b->position, a->position);
        float distance = LengthVec3(delta);
        float total_weight = a->inverse_mass + b->inverse_mass;
        if (distance >= exclusion->minimum_distance ||
            distance <= 0.00001f || total_weight <= 0.0f) {
            continue;
        }
        CcBiomechVec3 correction = ScaleVec3(
            delta, (exclusion->minimum_distance - distance) /
                       (distance * total_weight));
        a->position = AddVec3(
            a->position, ScaleVec3(correction, -a->inverse_mass));
        b->position = AddVec3(
            b->position, ScaleVec3(correction, b->inverse_mass));
    }
}

static void CollideRagdollParticles(CcBiomechRagdoll *ragdoll,
                                    CcBiomechRagdollCollisionProbe probe,
                                    void *context)
{
    if (probe == NULL) return;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        CcBiomechRagdollParticle *runtime = &ragdoll->particles[particle];
        CcBiomechVec3 corrected = runtime->position;
        CcBiomechVec3 normal = {0.0f, 1.0f, 0.0f};
        if (!probe(context, runtime->previous_position, runtime->position,
                   runtime->radius,
                   &corrected, &normal)) continue;
        float normal_length = sqrtf(normal.x * normal.x + normal.y * normal.y +
                                    normal.z * normal.z);
        normal = normal_length > 0.00001f ?
                 ScaleVec3(normal, 1.0f / normal_length) :
                 (CcBiomechVec3){0.0f, 1.0f, 0.0f};
        runtime->position = corrected;
        runtime->contact_normal = normal;
        runtime->collided = true;
    }
}

static void CollideRagdollSegments(CcBiomechRagdoll *ragdoll,
                                   CcBiomechRagdollCollisionProbe probe,
                                   void *context)
{
    if (probe == NULL) return;
    static const float samples[] = {0.25f, 0.50f, 0.75f};
    for (int32_t segment_index = 0;
         segment_index < ragdoll->collision_segment_count; ++segment_index) {
        const CcBiomechRagdollCollisionSegment *segment =
            &ragdoll->collision_segments[segment_index];
        CcBiomechRagdollParticle *a =
            &ragdoll->particles[segment->particle_a];
        CcBiomechRagdollParticle *b =
            &ragdoll->particles[segment->particle_b];
        for (int32_t sample_index = 0;
             sample_index < (int32_t)(sizeof(samples) / sizeof(samples[0]));
             ++sample_index) {
            float amount = samples[sample_index];
            CcBiomechVec3 position = AddVec3(
                ScaleVec3(a->position, 1.0f - amount),
                ScaleVec3(b->position, amount));
            CcBiomechVec3 previous = AddVec3(
                ScaleVec3(a->previous_position, 1.0f - amount),
                ScaleVec3(b->previous_position, amount));
            CcBiomechVec3 corrected = position;
            CcBiomechVec3 normal = {0.0f, 1.0f, 0.0f};
            if (!probe(context, previous, position, segment->radius,
                       &corrected, &normal)) {
                continue;
            }
            float normal_length = LengthVec3(normal);
            normal = normal_length > 0.00001f && isfinite(normal_length) ?
                ScaleVec3(normal, 1.0f / normal_length) :
                (CcBiomechVec3){0.0f, 1.0f, 0.0f};
            CcBiomechVec3 correction = SubtractVec3(corrected, position);
            float weight_a = a->inverse_mass * (1.0f - amount);
            float weight_b = b->inverse_mass * amount;
            float total_weight = weight_a * (1.0f - amount) +
                                 weight_b * amount;
            if (total_weight <= 0.00001f) continue;
            // The interpolated sample must receive the full contact correction.
            a->position = AddVec3(
                a->position,
                ScaleVec3(correction, weight_a / total_weight));
            b->position = AddVec3(
                b->position,
                ScaleVec3(correction, weight_b / total_weight));
            a->collided = true;
            b->collided = true;
            a->contact_normal = normal;
            b->contact_normal = normal;
        }
    }
}

static void DampRagdollImpact(CcBiomechRagdoll *ragdoll, float delta_time)
{
    int32_t contact_count = 0;
    int32_t new_contact_count = 0;
    int32_t support_contact_count = 0;
    CcBiomechVec3 contact_normal = {0};
    CcBiomechVec3 support_normal = {0};
    CcBiomechVec3 center_velocity = {0};
    float total_mass = 0.0f;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        CcBiomechRagdollParticle *runtime = &ragdoll->particles[particle];
        if (runtime->collided) {
            contact_count += 1;
            if (!runtime->previously_collided) new_contact_count += 1;
            contact_normal = AddVec3(contact_normal, runtime->contact_normal);
            if (runtime->contact_normal.y > 0.35f) {
                support_contact_count += 1;
                support_normal = AddVec3(
                    support_normal, runtime->contact_normal);
            }
        }
        if (runtime->inverse_mass <= 0.0f) continue;
        float mass = 1.0f / runtime->inverse_mass;
        CcBiomechVec3 velocity = {
            runtime->position.x - runtime->previous_position.x,
            runtime->position.y - runtime->previous_position.y,
            runtime->position.z - runtime->previous_position.z
        };
        center_velocity = AddVec3(center_velocity, ScaleVec3(velocity, mass));
        total_mass += mass;
    }
    if (contact_count <= 0 || total_mass <= 0.0f) return;
    float normal_length = sqrtf(contact_normal.x * contact_normal.x +
                                contact_normal.y * contact_normal.y +
                                contact_normal.z * contact_normal.z);
    if (normal_length <= 0.00001f) {
        contact_normal = (CcBiomechVec3){0.0f, 1.0f, 0.0f};
    } else {
        contact_normal = ScaleVec3(contact_normal, 1.0f / normal_length);
    }
    center_velocity = ScaleVec3(center_velocity, 1.0f / total_mass);
    float support_normal_length = LengthVec3(support_normal);
    support_normal = support_normal_length > 0.00001f ?
        ScaleVec3(support_normal, 1.0f / support_normal_length) :
        (CcBiomechVec3){0.0f, 1.0f, 0.0f};
    float outward_center = center_velocity.x * contact_normal.x +
                           center_velocity.y * contact_normal.y +
                           center_velocity.z * contact_normal.z;
    CcBiomechVec3 rebound_removal = {0};
    if (outward_center > 0.0f && !ragdoll->driven) {
        rebound_removal = ScaleVec3(
            contact_normal,
            outward_center * (1.0f - Clamp(ragdoll->restitution, 0.0f, 1.0f)));
    }
    float impact_damping = ragdoll->contact_damping *
        fminf((float)new_contact_count / 2.0f, 1.0f);
    float support_alignment = Clamp(
        (support_normal.y - 0.65f) / 0.25f, 0.0f, 1.0f);
    float resting_damping = ragdoll->resting_contact_damping *
        fminf((float)support_contact_count / 3.0f, 1.0f) *
        support_alignment;
    float body_retention = 1.0f - Clamp(impact_damping + resting_damping,
                                        0.0f, 0.85f);
    float tangent_retention = 1.0f - Clamp(ragdoll->collision_friction,
                                           0.0f, 1.0f);
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        CcBiomechRagdollParticle *runtime = &ragdoll->particles[particle];
        if (runtime->inverse_mass <= 0.0f) continue;
        CcBiomechVec3 velocity = {
            runtime->position.x - runtime->previous_position.x -
                rebound_removal.x,
            runtime->position.y - runtime->previous_position.y -
                rebound_removal.y,
            runtime->position.z - runtime->previous_position.z -
                rebound_removal.z
        };
        velocity = ScaleVec3(velocity, body_retention);
        if (support_alignment > 0.70f && support_contact_count >= 1) {
            float outward_speed = DotVec3(velocity, support_normal);
            float support_center_speed =
                DotVec3(center_velocity, support_normal);
            float maximum_outward =
                fmaxf(0.0f, support_center_speed * body_retention) +
                0.20f * delta_time;
            if (outward_speed > maximum_outward) {
                velocity = AddVec3(
                    velocity,
                    ScaleVec3(support_normal,
                              maximum_outward - outward_speed));
            }
        }
        if (runtime->collided) {
            CcBiomechVec3 normal = runtime->contact_normal;
            float normal_speed = velocity.x * normal.x +
                                 velocity.y * normal.y +
                                 velocity.z * normal.z;
            CcBiomechVec3 normal_velocity = ScaleVec3(normal, normal_speed);
            CcBiomechVec3 tangent_velocity = {
                velocity.x - normal_velocity.x,
                velocity.y - normal_velocity.y,
                velocity.z - normal_velocity.z
            };
            tangent_velocity = ScaleVec3(tangent_velocity, tangent_retention);
            float outward_retention = ragdoll->driven ? 1.0f :
                Clamp(ragdoll->restitution, 0.0f, 1.0f);
            float retained_normal = normal_speed > 0.0f ?
                normal_speed * outward_retention : 0.0f;
            velocity = AddVec3(tangent_velocity,
                               ScaleVec3(normal, retained_normal));
        }
        runtime->previous_position = (CcBiomechVec3){
            runtime->position.x - velocity.x,
            runtime->position.y - velocity.y,
            runtime->position.z - velocity.z
        };
    }
}

void CcBiomechRagdollStep(CcBiomechRagdoll *ragdoll, float delta_time,
                          int32_t constraint_iterations,
                          CcBiomechRagdollCollisionProbe collision_probe,
                          void *collision_context)
{
    if (ragdoll == NULL || !ragdoll->active) return;
    if (!isfinite(delta_time) || delta_time <= 0.0f) return;
    delta_time = Clamp(delta_time, 0.0f, 1.0f / 30.0f);
    constraint_iterations = constraint_iterations < 1 ? 1 :
                            constraint_iterations > 16 ? 16 :
                            constraint_iterations;
    float retained_velocity = Clamp(1.0f - ragdoll->damping, 0.0f, 1.0f);
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        CcBiomechRagdollParticle *runtime = &ragdoll->particles[particle];
        runtime->previously_collided = runtime->collided;
        runtime->collided = false;
        runtime->contact_normal = (CcBiomechVec3){0};
        if (runtime->inverse_mass <= 0.0f) continue;
        CcBiomechVec3 velocity = {
            (runtime->position.x - runtime->previous_position.x) *
                retained_velocity,
            (runtime->position.y - runtime->previous_position.y) *
                retained_velocity,
            (runtime->position.z - runtime->previous_position.z) *
                retained_velocity
        };
        runtime->previous_position = runtime->position;
        CcBiomechVec3 acceleration = AddVec3(ragdoll->gravity,
                                             runtime->acceleration);
        runtime->position = AddVec3(runtime->position, velocity);
        runtime->position = AddVec3(runtime->position,
            ScaleVec3(acceleration, delta_time * delta_time));
        runtime->acceleration = (CcBiomechVec3){0};
    }
    for (int32_t iteration = 0; iteration < constraint_iterations; ++iteration) {
        for (int32_t constraint = 0;
             constraint < ragdoll->constraint_count; ++constraint) {
            SolveRagdollConstraint(ragdoll, &ragdoll->constraints[constraint],
                                   delta_time);
        }
        for (int32_t constraint = 0;
             constraint < ragdoll->angle_constraint_count; ++constraint) {
            SolveRagdollAngleConstraint(
                ragdoll, &ragdoll->angle_constraints[constraint], delta_time);
        }
        if (iteration == constraint_iterations - 1 &&
            ragdoll->hinge_constraint_count > 0) {
            for (int32_t constraint = 0;
                 constraint < ragdoll->hinge_constraint_count; ++constraint) {
                SolveRagdollHingeConstraint(
                    ragdoll, &ragdoll->hinge_constraints[constraint],
                    delta_time);
            }
            for (int32_t cleanup = 0; cleanup < 5; ++cleanup) {
                for (int32_t constraint = 0;
                     constraint < ragdoll->constraint_count; ++constraint) {
                    SolveRagdollConstraint(
                        ragdoll, &ragdoll->constraints[constraint],
                        delta_time);
                }
                for (int32_t constraint = 0;
                     constraint < ragdoll->angle_constraint_count;
                     ++constraint) {
                    SolveRagdollAngleConstraint(
                        ragdoll, &ragdoll->angle_constraints[constraint],
                        delta_time);
                }
                SolveRagdollExclusions(ragdoll);
                CollideRagdollParticles(
                    ragdoll, collision_probe, collision_context);
                CollideRagdollSegments(
                    ragdoll, collision_probe, collision_context);
            }
        }
        SolveRagdollExclusions(ragdoll);
        CollideRagdollParticles(ragdoll, collision_probe, collision_context);
        CollideRagdollSegments(ragdoll, collision_probe, collision_context);
    }
    DampRagdollImpact(ragdoll, delta_time);
}

CcBiomechVec3 CcBiomechRagdollParticleVelocity(
    const CcBiomechRagdoll *ragdoll, int32_t particle, float delta_time)
{
    if (ragdoll == NULL || particle < 0 ||
        particle >= ragdoll->particle_count || delta_time <= 0.0f) {
        return (CcBiomechVec3){0};
    }
    const CcBiomechRagdollParticle *runtime = &ragdoll->particles[particle];
    return (CcBiomechVec3){
        (runtime->position.x - runtime->previous_position.x) / delta_time,
        (runtime->position.y - runtime->previous_position.y) / delta_time,
        (runtime->position.z - runtime->previous_position.z) / delta_time
    };
}

CcBiomechVec3 CcBiomechRagdollCenterOfMass(
    const CcBiomechRagdoll *ragdoll)
{
    if (ragdoll == NULL) return (CcBiomechVec3){0};
    CcBiomechVec3 weighted = {0};
    float total_mass = 0.0f;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        const CcBiomechRagdollParticle *body = &ragdoll->particles[particle];
        if (body->inverse_mass <= 0.0f) continue;
        float mass = 1.0f / body->inverse_mass;
        weighted = AddVec3(weighted, ScaleVec3(body->position, mass));
        total_mass += mass;
    }
    return total_mass > 0.0f ? ScaleVec3(weighted, 1.0f / total_mass) :
                               (CcBiomechVec3){0};
}

CcBiomechVec3 CcBiomechRagdollCenterVelocity(
    const CcBiomechRagdoll *ragdoll, float delta_time)
{
    if (ragdoll == NULL || delta_time <= 0.0f) return (CcBiomechVec3){0};
    CcBiomechVec3 momentum = {0};
    float total_mass = 0.0f;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        const CcBiomechRagdollParticle *body = &ragdoll->particles[particle];
        if (body->inverse_mass <= 0.0f) continue;
        float mass = 1.0f / body->inverse_mass;
        CcBiomechVec3 velocity = CcBiomechRagdollParticleVelocity(
            ragdoll, particle, delta_time);
        momentum = AddVec3(momentum, ScaleVec3(velocity, mass));
        total_mass += mass;
    }
    return total_mass > 0.0f ? ScaleVec3(momentum, 1.0f / total_mass) :
                               (CcBiomechVec3){0};
}
