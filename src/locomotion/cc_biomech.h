#ifndef CROWNLESS_BIOMECH_H
#define CROWNLESS_BIOMECH_H

#include <stdbool.h>
#include <stdint.h>

#define CC_BIOMECH_MAX_BONES 32
#define CC_BIOMECH_MAX_JOINTS 32
#define CC_BIOMECH_MAX_MUSCLES 64
#define CC_BIOMECH_MAX_RAGDOLL_PARTICLES 48
#define CC_BIOMECH_MAX_RAGDOLL_CONSTRAINTS 64
#define CC_BIOMECH_MAX_RAGDOLL_ANGLE_CONSTRAINTS 32
#define CC_BIOMECH_MAX_RAGDOLL_HINGE_CONSTRAINTS 8
#define CC_BIOMECH_MAX_RAGDOLL_COLLISION_SEGMENTS 32
#define CC_BIOMECH_MAX_RAGDOLL_EXCLUSIONS 48
#define CC_BIOMECH_NAME_LENGTH 24

typedef struct CcBiomechVec3 {
    float x;
    float y;
    float z;
} CcBiomechVec3;

typedef struct CcBiomechBoneSpec {
    char name[CC_BIOMECH_NAME_LENGTH];
    int32_t parent_bone;
    float length;
    float mass;
    float center_of_mass;
} CcBiomechBoneSpec;

typedef struct CcBiomechJointSpec {
    char name[CC_BIOMECH_NAME_LENGTH];
    int32_t parent_bone;
    int32_t child_bone;
    float rest_angle;
    float lower_limit;
    float upper_limit;
    float inertia;
    float passive_stiffness;
    float damping;
    float ligament_stiffness;
} CcBiomechJointSpec;

typedef struct CcBiomechMuscleSpec {
    char name[CC_BIOMECH_NAME_LENGTH];
    int32_t joint;
    float moment_arm;
    float maximum_force;
    float optimal_angle;
    float operating_width;
    float activation_rate;
    float relaxation_rate;
} CcBiomechMuscleSpec;

typedef struct CcBiomechMorphology {
    CcBiomechBoneSpec bones[CC_BIOMECH_MAX_BONES];
    CcBiomechJointSpec joints[CC_BIOMECH_MAX_JOINTS];
    CcBiomechMuscleSpec muscles[CC_BIOMECH_MAX_MUSCLES];
    int32_t bone_count;
    int32_t joint_count;
    int32_t muscle_count;
} CcBiomechMorphology;

typedef struct CcBiomechJointRuntime {
    float angle;
    float angular_velocity;
    float target_angle;
    float effort;
    float muscle_torque;
    float passive_torque;
    float contact_reaction_torque;
    float external_torque;
} CcBiomechJointRuntime;

typedef struct CcBiomechMuscleRuntime {
    float excitation;
    float activation;
    float tension;
} CcBiomechMuscleRuntime;

typedef struct CcBiomechBodyRuntime {
    CcBiomechVec3 position;
    CcBiomechVec3 velocity;
    CcBiomechVec3 acceleration;
    CcBiomechVec3 accumulated_force;
    CcBiomechVec3 last_applied_force;
    CcBiomechVec3 contact_impulse;
    CcBiomechVec3 gravity;
    float linear_damping;
} CcBiomechBodyRuntime;

typedef struct CcBiomechRagdollParticle {
    CcBiomechVec3 position;
    CcBiomechVec3 previous_position;
    CcBiomechVec3 acceleration;
    CcBiomechVec3 contact_normal;
    float inverse_mass;
    float radius;
    bool previously_collided;
    bool collided;
} CcBiomechRagdollParticle;

typedef struct CcBiomechRagdollConstraint {
    int32_t particle_a;
    int32_t particle_b;
    float rest_length;
    float compliance;
} CcBiomechRagdollConstraint;

typedef struct CcBiomechRagdollAngleConstraint {
    int32_t particle_a;
    int32_t joint_particle;
    int32_t particle_b;
    float minimum_angle;
    float maximum_angle;
    float compliance;
} CcBiomechRagdollAngleConstraint;

typedef struct CcBiomechRagdollHingeConstraint {
    int32_t particle_a;
    int32_t joint_particle;
    int32_t particle_b;
    int32_t axis_particle_a;
    int32_t axis_particle_b;
    float minimum_angle;
    float maximum_angle;
    float rest_lateral_offset;
    float maximum_splay_angle;
    float passive_splay_angle;
    float compliance;
} CcBiomechRagdollHingeConstraint;

typedef struct CcBiomechRagdollCollisionSegment {
    int32_t particle_a;
    int32_t particle_b;
    float radius;
} CcBiomechRagdollCollisionSegment;

typedef struct CcBiomechRagdollExclusion {
    int32_t particle_a;
    int32_t particle_b;
    float minimum_distance;
} CcBiomechRagdollExclusion;

typedef bool (*CcBiomechRagdollCollisionProbe)(
    void *context, CcBiomechVec3 previous_position,
    CcBiomechVec3 position, float radius,
    CcBiomechVec3 *corrected_position, CcBiomechVec3 *surface_normal);

typedef struct CcBiomechRagdoll {
    CcBiomechRagdollParticle particles[CC_BIOMECH_MAX_RAGDOLL_PARTICLES];
    CcBiomechRagdollConstraint constraints[CC_BIOMECH_MAX_RAGDOLL_CONSTRAINTS];
    CcBiomechRagdollAngleConstraint
        angle_constraints[CC_BIOMECH_MAX_RAGDOLL_ANGLE_CONSTRAINTS];
    CcBiomechRagdollHingeConstraint
        hinge_constraints[CC_BIOMECH_MAX_RAGDOLL_HINGE_CONSTRAINTS];
    CcBiomechRagdollCollisionSegment
        collision_segments[CC_BIOMECH_MAX_RAGDOLL_COLLISION_SEGMENTS];
    CcBiomechRagdollExclusion exclusions[CC_BIOMECH_MAX_RAGDOLL_EXCLUSIONS];
    CcBiomechVec3 gravity;
    float damping;
    float restitution;
    float collision_friction;
    float contact_damping;
    float resting_contact_damping;
    int32_t particle_count;
    int32_t constraint_count;
    int32_t angle_constraint_count;
    int32_t hinge_constraint_count;
    int32_t collision_segment_count;
    int32_t exclusion_count;
    bool driven;
    bool active;
} CcBiomechRagdoll;

typedef struct CcBiomechRig {
    CcBiomechMorphology morphology;
    CcBiomechJointRuntime joints[CC_BIOMECH_MAX_JOINTS];
    CcBiomechMuscleRuntime muscles[CC_BIOMECH_MAX_MUSCLES];
    CcBiomechBodyRuntime root;
    float total_mass;
    bool initialized;
} CcBiomechRig;

void CcBiomechMorphologyInit(CcBiomechMorphology *morphology);
int32_t CcBiomechAddBone(CcBiomechMorphology *morphology, const char *name,
                         int32_t parent_bone, float length, float mass,
                         float center_of_mass);
int32_t CcBiomechAddJoint(CcBiomechMorphology *morphology, const char *name,
                          int32_t parent_bone, int32_t child_bone,
                          float rest_angle, float lower_limit,
                          float upper_limit, float inertia,
                          float passive_stiffness, float damping,
                          float ligament_stiffness);
int32_t CcBiomechAddMuscle(CcBiomechMorphology *morphology, const char *name,
                           int32_t joint, float moment_arm,
                           float maximum_force, float optimal_angle,
                           float operating_width, float activation_rate,
                           float relaxation_rate);
bool CcBiomechRigInit(CcBiomechRig *rig,
                      const CcBiomechMorphology *morphology);
void CcBiomechRigDriveJoint(CcBiomechRig *rig, int32_t joint,
                            float target_angle, float effort);
void CcBiomechRigApplyTorque(CcBiomechRig *rig, int32_t joint, float torque);
void CcBiomechRigSetBodyState(CcBiomechRig *rig, CcBiomechVec3 position,
                              CcBiomechVec3 velocity);
void CcBiomechRigApplyBodyForce(CcBiomechRig *rig, CcBiomechVec3 force);
void CcBiomechRigStepBody(CcBiomechRig *rig, float delta_time);
void CcBiomechRigConstrainBody(CcBiomechRig *rig,
                               CcBiomechVec3 constrained_position,
                               CcBiomechVec3 constrained_velocity);
void CcBiomechRigStep(CcBiomechRig *rig, float delta_time);
void CcBiomechRigConstrainJoint(CcBiomechRig *rig, int32_t joint,
                                float constrained_angle, float delta_time);
float CcBiomechRigJointAngle(const CcBiomechRig *rig, int32_t joint);
float CcBiomechRigMeanActivation(const CcBiomechRig *rig);

void CcBiomechRagdollInit(CcBiomechRagdoll *ragdoll);
int32_t CcBiomechRagdollAddParticle(CcBiomechRagdoll *ragdoll,
                                    CcBiomechVec3 position,
                                    float inverse_mass, float radius);
int32_t CcBiomechRagdollAddConstraint(CcBiomechRagdoll *ragdoll,
                                      int32_t particle_a,
                                      int32_t particle_b, float compliance);
int32_t CcBiomechRagdollAddAngleConstraint(
    CcBiomechRagdoll *ragdoll, int32_t particle_a, int32_t joint_particle,
    int32_t particle_b, float minimum_angle, float maximum_angle,
    float compliance);
int32_t CcBiomechRagdollAddHingeConstraint(
    CcBiomechRagdoll *ragdoll, int32_t particle_a, int32_t joint_particle,
    int32_t particle_b, int32_t axis_particle_a, int32_t axis_particle_b,
    float minimum_angle, float maximum_angle, float maximum_splay_angle,
    float compliance);
int32_t CcBiomechRagdollAddCollisionSegment(
    CcBiomechRagdoll *ragdoll, int32_t particle_a, int32_t particle_b,
    float radius);
int32_t CcBiomechRagdollAddExclusion(CcBiomechRagdoll *ragdoll,
                                     int32_t particle_a,
                                     int32_t particle_b,
                                     float minimum_distance);
void CcBiomechRagdollSetVelocity(CcBiomechRagdoll *ragdoll,
                                 CcBiomechVec3 velocity, float delta_time);
void CcBiomechRagdollStep(CcBiomechRagdoll *ragdoll, float delta_time,
                          int32_t constraint_iterations,
                          CcBiomechRagdollCollisionProbe collision_probe,
                          void *collision_context);
CcBiomechVec3 CcBiomechRagdollParticleVelocity(
    const CcBiomechRagdoll *ragdoll, int32_t particle, float delta_time);
CcBiomechVec3 CcBiomechRagdollCenterOfMass(
    const CcBiomechRagdoll *ragdoll);
CcBiomechVec3 CcBiomechRagdollCenterVelocity(
    const CcBiomechRagdoll *ragdoll, float delta_time);

#endif
