#include "locomotion/cc_multileg.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

static CcBiomechVec3 ToBiomech(CcLimbVec3 value)
{
    return (CcBiomechVec3){value.x, value.y, value.z};
}

static CcLimbVec3 FromBiomech(CcBiomechVec3 value)
{
    return (CcLimbVec3){value.x, value.y, value.z};
}

static CcLimbVec3 Subtract(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CcLimbVec3 Scale(CcLimbVec3 value, float scale)
{
    return (CcLimbVec3){value.x * scale, value.y * scale,
                        value.z * scale};
}

static float Length(CcLimbVec3 value)
{
    return sqrtf(value.x * value.x + value.y * value.y +
                 value.z * value.z);
}

static float Clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static CcLimbVec3 FramePoint(CcLimbVec3 center, float x, float y,
                             float z, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (CcLimbVec3){center.x + x * cosine + z * sine,
                        center.y + y,
                        center.z - x * sine + z * cosine};
}

static void BodyExtents(const CcLimbRig *rig, float *half_width,
                        float *half_length)
{
    *half_width = 0.27f;
    *half_length = 0.32f;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        *half_width = fmaxf(*half_width, fabsf(spec->socket_local.x) + 0.08f);
        *half_length = fmaxf(*half_length,
                             fabsf(spec->socket_local.z) + 0.10f);
    }
}

static void BodyFramePoints(const CcLimbRig *rig, CcLimbVec3 center,
                            float yaw,
                            CcLimbVec3 points[CC_MULTILEG_BODY_PARTICLE_COUNT])
{
    float half_width;
    float half_length;
    BodyExtents(rig, &half_width, &half_length);
    points[CC_MULTILEG_BODY_CENTER] = center;
    points[CC_MULTILEG_BODY_FRONT] =
        FramePoint(center, 0.0f, 0.0f, half_length, yaw);
    points[CC_MULTILEG_BODY_BACK] =
        FramePoint(center, 0.0f, 0.0f, -half_length, yaw);
    points[CC_MULTILEG_BODY_LEFT] =
        FramePoint(center, -half_width, 0.0f, 0.0f, yaw);
    points[CC_MULTILEG_BODY_RIGHT] =
        FramePoint(center, half_width, 0.0f, 0.0f, yaw);
}

static bool AddConstraint(CcMultilegRagdoll *body, int32_t first,
                          int32_t second, float compliance)
{
    return CcBiomechRagdollAddConstraint(
               &body->physics, first, second, compliance) >= 0;
}

static bool AddFrameConstraints(CcMultilegRagdoll *body)
{
    const int32_t *node = body->body_particle;
    bool valid = true;
    for (int32_t point = CC_MULTILEG_BODY_FRONT;
         point < CC_MULTILEG_BODY_PARTICLE_COUNT; ++point) {
        valid = valid && AddConstraint(
            body, node[CC_MULTILEG_BODY_CENTER], node[point], 0.0f);
    }
    valid = valid && AddConstraint(
        body, node[CC_MULTILEG_BODY_FRONT], node[CC_MULTILEG_BODY_BACK], 0.0f);
    valid = valid && AddConstraint(
        body, node[CC_MULTILEG_BODY_LEFT], node[CC_MULTILEG_BODY_RIGHT], 0.0f);
    valid = valid && AddConstraint(
        body, node[CC_MULTILEG_BODY_FRONT], node[CC_MULTILEG_BODY_LEFT], 0.0f);
    valid = valid && AddConstraint(
        body, node[CC_MULTILEG_BODY_FRONT], node[CC_MULTILEG_BODY_RIGHT], 0.0f);
    valid = valid && AddConstraint(
        body, node[CC_MULTILEG_BODY_BACK], node[CC_MULTILEG_BODY_LEFT], 0.0f);
    valid = valid && AddConstraint(
        body, node[CC_MULTILEG_BODY_BACK], node[CC_MULTILEG_BODY_RIGHT], 0.0f);
    return valid;
}

static int32_t AddParticle(CcMultilegRagdoll *body, CcLimbVec3 point,
                           CcLimbVec3 velocity, float inverse_mass,
                           float radius)
{
    int32_t particle = CcBiomechRagdollAddParticle(
        &body->physics, ToBiomech(point), inverse_mass, radius);
    if (particle >= 0) {
        body->physics.particles[particle].previous_position = ToBiomech(
            Subtract(point, Scale(velocity, 1.0f / 60.0f)));
    }
    return particle;
}

static bool AddLimbParticles(CcMultilegRagdoll *body, const CcLimbRig *rig,
                             CcLimbVec3 velocity)
{
    bool valid = true;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        for (int32_t joint = 0; joint <= spec->segment_count; ++joint) {
            float inverse_mass = joint == 0 ? 0.28f : 0.42f +
                (float)joint * 0.12f;
            float radius = joint == 0 ? 0.080f :
                           joint == spec->segment_count ? 0.072f : 0.060f;
            int32_t particle = AddParticle(
                body, rig->limbs[limb].joints[joint], velocity,
                inverse_mass, radius);
            body->limb_particle[limb][joint] = particle;
            valid = valid && particle >= 0;
        }
    }
    return valid;
}

static bool AddLimbConstraints(CcMultilegRagdoll *body,
                               const CcLimbRig *rig)
{
    bool valid = true;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        int32_t socket = body->limb_particle[limb][0];
        int32_t longitudinal = spec->socket_local.z >= 0.0f ?
            body->body_particle[CC_MULTILEG_BODY_FRONT] :
            body->body_particle[CC_MULTILEG_BODY_BACK];
        int32_t lateral = spec->socket_local.x < 0.0f ?
            body->body_particle[CC_MULTILEG_BODY_LEFT] :
            body->body_particle[CC_MULTILEG_BODY_RIGHT];
        valid = valid && AddConstraint(
            body, socket, body->body_particle[CC_MULTILEG_BODY_CENTER], 0.0f);
        valid = valid && AddConstraint(body, socket, longitudinal, 0.0f);
        valid = valid && AddConstraint(body, socket, lateral, 0.0f);
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            int32_t first = body->limb_particle[limb][segment];
            int32_t second = body->limb_particle[limb][segment + 1];
            valid = valid && AddConstraint(body, first, second, 0.0f);
            valid = valid && CcBiomechRagdollAddCollisionSegment(
                &body->physics, first, second,
                segment == 0 ? 0.064f : 0.052f) >= 0;
        }
        valid = valid && CcBiomechRagdollAddAngleConstraint(
            &body->physics,
            body->body_particle[CC_MULTILEG_BODY_CENTER], socket,
            body->limb_particle[limb][1], 0.22f, 2.96f,
            0.000008f) >= 0;
        for (int32_t joint = 1; joint < spec->segment_count; ++joint) {
            valid = valid && CcBiomechRagdollAddAngleConstraint(
                &body->physics,
                body->limb_particle[limb][joint - 1],
                body->limb_particle[limb][joint],
                body->limb_particle[limb][joint + 1],
                0.30f, 3.08f, 0.000006f) >= 0;
        }
    }
    return valid;
}

static bool AddFrameCollision(CcMultilegRagdoll *body)
{
    static const int32_t edges[][2] = {
        {CC_MULTILEG_BODY_FRONT, CC_MULTILEG_BODY_LEFT},
        {CC_MULTILEG_BODY_FRONT, CC_MULTILEG_BODY_RIGHT},
        {CC_MULTILEG_BODY_BACK, CC_MULTILEG_BODY_LEFT},
        {CC_MULTILEG_BODY_BACK, CC_MULTILEG_BODY_RIGHT},
        {CC_MULTILEG_BODY_FRONT, CC_MULTILEG_BODY_BACK},
        {CC_MULTILEG_BODY_LEFT, CC_MULTILEG_BODY_RIGHT},
    };
    bool valid = true;
    for (int32_t edge = 0;
         edge < (int32_t)(sizeof(edges) / sizeof(edges[0])); ++edge) {
        valid = valid && CcBiomechRagdollAddCollisionSegment(
            &body->physics,
            body->body_particle[edges[edge][0]],
            body->body_particle[edges[edge][1]], 0.12f) >= 0;
    }
    return valid;
}

static void ApplyPointImpact(CcMultilegRagdoll *body,
                             CcLimbVec3 direction, CcLimbVec3 point,
                             float speed)
{
    float direction_length = Length(direction);
    if (direction_length <= 0.0001f || speed <= 0.0f) return;
    direction = Scale(direction, 1.0f / direction_length);
    int32_t nearest = -1;
    float nearest_distance = FLT_MAX;
    for (int32_t particle = 0;
         particle < body->physics.particle_count; ++particle) {
        float distance = Length(Subtract(
            FromBiomech(body->physics.particles[particle].position), point));
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = particle;
        }
    }
    if (nearest < 0) return;
    CcBiomechRagdollParticle *particle = &body->physics.particles[nearest];
    CcLimbVec3 previous = FromBiomech(particle->previous_position);
    previous = Subtract(previous,
                        Scale(direction, Clamp(speed, 0.0f, 12.0f) /
                                         60.0f));
    particle->previous_position = ToBiomech(previous);
}

bool CcMultilegRagdollCollapse(
    CcMultilegRagdoll *body, CcLimbRig *rig,
    CcLimbVec3 body_center, float body_yaw, CcLimbVec3 velocity,
    CcLimbVec3 impact_direction, CcLimbVec3 impact_point,
    float impact_speed, bool recovery_allowed)
{
    if (body == NULL || rig == NULL || !rig->initialized ||
        rig->morphology.preset == CC_MORPHOLOGY_BIPED) {
        return false;
    }
    *body = (CcMultilegRagdoll){0};
    for (int32_t limb = 0; limb < CC_LIMB_MAX_COUNT; ++limb) {
        for (int32_t joint = 0; joint < CC_LIMB_MAX_JOINTS; ++joint) {
            body->limb_particle[limb][joint] = -1;
        }
    }
    CcBiomechRagdollInit(&body->physics);
    CcLimbVec3 frame[CC_MULTILEG_BODY_PARTICLE_COUNT];
    BodyFramePoints(rig, body_center, body_yaw, frame);
    static const float inverse_mass[] = {0.055f, 0.14f, 0.14f, 0.16f, 0.16f};
    static const float radius[] = {0.16f, 0.13f, 0.13f, 0.12f, 0.12f};
    bool valid = true;
    for (int32_t node = 0; node < CC_MULTILEG_BODY_PARTICLE_COUNT; ++node) {
        body->body_particle[node] = AddParticle(
            body, frame[node], velocity, inverse_mass[node], radius[node]);
        valid = valid && body->body_particle[node] >= 0;
    }
    valid = valid && AddLimbParticles(body, rig, velocity);
    valid = valid && AddFrameConstraints(body);
    valid = valid && AddLimbConstraints(body, rig);
    valid = valid && AddFrameCollision(body);
    if (!valid) {
        *body = (CcMultilegRagdoll){0};
        return false;
    }
    body->physics.damping = 0.018f;
    body->physics.collision_friction = 0.42f;
    body->physics.contact_damping = 0.34f;
    body->physics.resting_contact_damping = 0.045f;
    body->physics.active = true;
    body->body_center = body_center;
    body->body_velocity = velocity;
    body->body_yaw = body_yaw;
    body->control_authority = 0.0f;
    body->recovery_allowed = recovery_allowed;
    body->initialized = true;
    body->active = true;
    ApplyPointImpact(body, impact_direction, impact_point, impact_speed);
    rig->support_state = CC_LIMB_SUPPORT_UNSUPPORTED;
    rig->control_authority = 0.0f;
    return true;
}

static float MeanSpeed(const CcMultilegRagdoll *body, float delta_time)
{
    if (delta_time <= 0.0f || body->physics.particle_count <= 0) return 0.0f;
    float total = 0.0f;
    for (int32_t particle = 0;
         particle < body->physics.particle_count; ++particle) {
        total += Length(FromBiomech(CcBiomechRagdollParticleVelocity(
            &body->physics, particle, delta_time)));
    }
    return total / (float)body->physics.particle_count;
}

int32_t CcMultilegRagdollSupportContactCount(
    const CcMultilegRagdoll *body)
{
    if (body == NULL || !body->active) return 0;
    int32_t count = 0;
    for (int32_t particle = 0;
         particle < body->physics.particle_count; ++particle) {
        const CcBiomechRagdollParticle *runtime =
            &body->physics.particles[particle];
        if (runtime->collided && runtime->contact_normal.y > 0.45f) {
            count += 1;
        }
    }
    return count;
}

static void SyncRig(CcMultilegRagdoll *body, CcLimbRig *rig)
{
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        CcLimbRuntime *runtime = &rig->limbs[limb];
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        for (int32_t joint = 0; joint <= spec->segment_count; ++joint) {
            runtime->previous_joints[joint] = runtime->joints[joint];
            int32_t particle = body->limb_particle[limb][joint];
            runtime->joints[joint] = FromBiomech(
                body->physics.particles[particle].position);
        }
        int32_t foot_particle =
            body->limb_particle[limb][spec->segment_count];
        const CcBiomechRagdollParticle *foot =
            &body->physics.particles[foot_particle];
        runtime->planted_contact = runtime->joints[spec->segment_count];
        runtime->desired_contact = runtime->planted_contact;
        runtime->contact_normal = foot->collided ?
            FromBiomech(foot->contact_normal) : (CcLimbVec3){0.0f, 1.0f, 0.0f};
        runtime->state = runtime->health <= 0.0f ? CC_LIMB_DISABLED :
                         foot->collided ? CC_LIMB_STANCE : CC_LIMB_SEARCHING;
    }
}

static bool RecoveryTarget(const CcMultilegRagdoll *body,
                           const CcLimbRig *rig,
                           CcLimbTerrainProbe terrain_probe,
                           void *probe_context, CcLimbRig *target_rig,
                           CcLimbVec3 *ground)
{
    CcLimbVec3 origin = body->body_center;
    origin.y += rig->morphology.body_height + 2.0f;
    CcLimbVec3 normal = {0.0f, 1.0f, 0.0f};
    *ground = (CcLimbVec3){body->body_center.x, 0.0f, body->body_center.z};
    if (terrain_probe != NULL &&
        !terrain_probe(probe_context, origin, 8.0f, ground, &normal)) {
        return false;
    }
    CcLimbVec3 center = {ground->x,
                         ground->y + rig->morphology.body_height,
                         ground->z};
    CcLimbRigInit(target_rig, &rig->walking_morphology, center,
                  body->body_yaw, terrain_probe, probe_context);
    return target_rig->initialized;
}

static CcLimbVec3 RecoveryParticleTarget(
    const CcMultilegRagdoll *body, const CcLimbRig *target_rig,
    const CcLimbVec3 frame[CC_MULTILEG_BODY_PARTICLE_COUNT],
    int32_t particle)
{
    for (int32_t node = 0; node < CC_MULTILEG_BODY_PARTICLE_COUNT; ++node) {
        if (body->body_particle[node] == particle) return frame[node];
    }
    for (int32_t limb = 0;
         limb < target_rig->morphology.limb_count; ++limb) {
        const CcLimbSpec *spec = &target_rig->morphology.limbs[limb];
        for (int32_t joint = 0; joint <= spec->segment_count; ++joint) {
            if (body->limb_particle[limb][joint] == particle) {
                return target_rig->limbs[limb].joints[joint];
            }
        }
    }
    return body->body_center;
}

static bool DriveRecovery(CcMultilegRagdoll *body, CcLimbRig *rig,
                          float delta_time,
                          CcLimbTerrainProbe terrain_probe,
                          void *probe_context)
{
    CcLimbRig target_rig;
    CcLimbVec3 ground;
    if (!RecoveryTarget(body, rig, terrain_probe, probe_context,
                        &target_rig, &ground)) {
        return false;
    }
    body->recovery_ground = ground;
    CcLimbVec3 target_center = {
        ground.x, ground.y + rig->morphology.body_height, ground.z};
    CcLimbVec3 frame[CC_MULTILEG_BODY_PARTICLE_COUNT];
    BodyFramePoints(&target_rig, target_center, body->body_yaw, frame);
    float amount = Clamp(body->recovery_seconds / 1.65f, 0.0f, 1.0f);
    float stiffness = 20.0f + amount * 54.0f;
    float damping = 8.0f + amount * 6.0f;
    float error_sum = 0.0f;
    for (int32_t particle = 0;
         particle < body->physics.particle_count; ++particle) {
        CcBiomechRagdollParticle *runtime = &body->physics.particles[particle];
        CcLimbVec3 position = FromBiomech(runtime->position);
        CcLimbVec3 target = RecoveryParticleTarget(
            body, &target_rig, frame, particle);
        CcLimbVec3 error = Subtract(target, position);
        CcLimbVec3 velocity = FromBiomech(CcBiomechRagdollParticleVelocity(
            &body->physics, particle, delta_time));
        CcLimbVec3 acceleration = Subtract(
            Scale(error, stiffness), Scale(velocity, damping));
        runtime->acceleration.x += acceleration.x;
        runtime->acceleration.y += acceleration.y;
        runtime->acceleration.z += acceleration.z;
        error_sum += Length(error);
    }
    body->recovery_error = error_sum /
                           (float)body->physics.particle_count;
    body->control_authority = Clamp(0.15f + amount * 0.85f, 0.0f, 1.0f);
    float speed = MeanSpeed(body, delta_time);
    if ((body->recovery_seconds >= 1.65f &&
         body->recovery_error < 0.075f && speed < 0.42f) ||
        body->recovery_seconds >= 3.20f) {
        *rig = target_rig;
        body->body_center = target_center;
        body->body_velocity = (CcLimbVec3){0};
        body->physics.active = false;
        body->active = false;
        body->recovering = false;
        body->control_authority = 1.0f;
        return true;
    }
    return false;
}

bool CcMultilegRagdollStep(
    CcMultilegRagdoll *body, CcLimbRig *rig, float delta_time,
    CcLimbTerrainProbe terrain_probe,
    CcBiomechRagdollCollisionProbe collision_probe, void *probe_context)
{
    if (body == NULL || rig == NULL || !body->active ||
        !body->physics.active || !isfinite(delta_time) ||
        delta_time <= 0.0f) {
        return false;
    }
    delta_time = Clamp(delta_time, 1.0f / 240.0f, 1.0f / 30.0f);
    if (body->recovering &&
        DriveRecovery(body, rig, delta_time, terrain_probe, probe_context)) {
        return false;
    }
    CcBiomechRagdollStep(&body->physics, delta_time, 12,
                          collision_probe, probe_context);
    body->active_seconds += delta_time;
    if (body->recovering) body->recovery_seconds += delta_time;

    CcLimbVec3 previous_center = body->body_center;
    body->body_center = FromBiomech(
        body->physics.particles[
            body->body_particle[CC_MULTILEG_BODY_CENTER]].position);
    body->body_velocity = Scale(
        Subtract(body->body_center, previous_center), 1.0f / delta_time);
    CcLimbVec3 front = FromBiomech(
        body->physics.particles[
            body->body_particle[CC_MULTILEG_BODY_FRONT]].position);
    CcLimbVec3 back = FromBiomech(
        body->physics.particles[
            body->body_particle[CC_MULTILEG_BODY_BACK]].position);
    CcLimbVec3 forward = Subtract(front, back);
    if (forward.x * forward.x + forward.z * forward.z > 0.0001f) {
        body->body_yaw = atan2f(forward.x, forward.z);
    }
    SyncRig(body, rig);
    body->support_contacts = CcMultilegRagdollSupportContactCount(body);
    float mean_speed = MeanSpeed(body, delta_time);
    if (body->support_contacts >= 3 && mean_speed < 0.58f) {
        body->settled_seconds += delta_time;
    } else {
        body->settled_seconds = 0.0f;
    }
    if (!body->recovering && body->recovery_allowed &&
        body->active_seconds >= 0.85f && body->settled_seconds >= 0.34f) {
        body->recovering = true;
        body->recovery_seconds = 0.0f;
        body->physics.driven = true;
        body->physics.damping = 0.050f;
        body->physics.collision_friction = 0.58f;
    }
    rig->support_state = body->recovering ? CC_LIMB_SUPPORT_RECOVERING :
                         body->support_contacts > 0 ?
                         CC_LIMB_SUPPORT_MARGINAL :
                         CC_LIMB_SUPPORT_UNSUPPORTED;
    rig->control_authority = body->control_authority;
    return true;
}

void CcMultilegRagdollAllowRecovery(CcMultilegRagdoll *body)
{
    if (body != NULL && body->active) body->recovery_allowed = true;
}

const char *CcMultilegRagdollStateName(const CcMultilegRagdoll *body)
{
    if (body == NULL || !body->active) return "CONTROLLED";
    return body->recovering ? "RECOVERING" : "RAGDOLL";
}
