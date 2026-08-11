#include "locomotion/cc_limb.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#define CC_LIMB_PI 3.14159265358979323846f

typedef struct CcPoint2 {
    float x;
    float z;
} CcPoint2;

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

static float Length(CcLimbVec3 value)
{
    return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

static float Dot(CcLimbVec3 a, CcLimbVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    return Length(Subtract(b, a));
}

static CcLimbVec3 NormalizeOr(CcLimbVec3 value, CcLimbVec3 fallback)
{
    float length = Length(value);
    return length > 0.00001f ? Scale(value, 1.0f / length) : fallback;
}

static CcLimbVec3 Lerp(CcLimbVec3 a, CcLimbVec3 b, float amount)
{
    return Add(a, Scale(Subtract(b, a), amount));
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

static CcLimbVec3 TransformPoint(CcLimbVec3 body, CcLimbVec3 local, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (CcLimbVec3){body.x + local.x * cosine + local.z * sine,
                        body.y + local.y,
                        body.z - local.x * sine + local.z * cosine};
}

static CcLimbVec3 TransformDirection(CcLimbVec3 local, float yaw)
{
    return TransformPoint((CcLimbVec3){0.0f, 0.0f, 0.0f}, local, yaw);
}

static void ConfigureLimb(CcLimbSpec *limb, float side, float socket_z,
                          float contact_z, float phase, int32_t segments)
{
    *limb = (CcLimbSpec){0};
    limb->socket_local = (CcLimbVec3){side * 0.24f, 0.0f, socket_z};
    limb->rest_contact_local = (CcLimbVec3){side * 0.62f, -0.78f, contact_z};
    limb->bend_local = (CcLimbVec3){side * 0.34f, 0.28f, 0.0f};
    limb->phase_offset = phase;
    limb->segment_count = segments;
    if (segments == 2) {
        limb->segment_length[0] = 0.62f;
        limb->segment_length[1] = 0.68f;
    } else {
        for (int32_t segment = 0; segment < segments; ++segment) {
            limb->segment_length[segment] = 0.45f;
        }
    }
}

bool CcLimbMorphologyFromPreset(CcLimbMorphology *morphology,
                                CcMorphologyPreset preset)
{
    if (morphology == NULL || preset < 0 || preset >= CC_MORPHOLOGY_PRESET_COUNT) {
        return false;
    }
    *morphology = (CcLimbMorphology){0};
    morphology->preset = preset;
    morphology->body_height = 0.78f;
    morphology->step_threshold = 0.22f;
    morphology->step_height = 0.22f;
    morphology->swing_seconds = 0.25f;
    morphology->velocity_lead = 0.17f;
    morphology->support_margin = 0.035f;

    if (preset == CC_MORPHOLOGY_BIPED) {
        morphology->name = "BIPED";
        morphology->limb_count = 2;
        morphology->minimum_supports = 1;
        morphology->maximum_swings = 1;
        morphology->body_height = 0.96f;
        morphology->duty_factor = 0.60f;
        morphology->step_threshold = 0.25f;
        morphology->step_height = 0.16f;
        morphology->swing_seconds = 0.24f;
        morphology->velocity_lead = 0.17f;
        morphology->support_margin = 0.025f;
        morphology->dynamic_balance = true;
        ConfigureLimb(&morphology->limbs[0], -1.0f, 0.0f, 0.05f, 0.0f, 2);
        ConfigureLimb(&morphology->limbs[1], 1.0f, 0.0f, 0.05f, 0.5f, 2);
        morphology->limbs[0].socket_local.x = -0.16f;
        morphology->limbs[1].socket_local.x = 0.16f;
        morphology->limbs[0].rest_contact_local =
            (CcLimbVec3){-0.17f, -0.96f, 0.05f};
        morphology->limbs[1].rest_contact_local =
            (CcLimbVec3){0.17f, -0.96f, 0.05f};
        morphology->limbs[0].bend_local = (CcLimbVec3){-0.025f, 0.0f, 0.42f};
        morphology->limbs[1].bend_local = (CcLimbVec3){0.025f, 0.0f, 0.42f};
        for (int32_t limb = 0; limb < morphology->limb_count; ++limb) {
            morphology->limbs[limb].segment_length[0] = 0.54f;
            morphology->limbs[limb].segment_length[1] = 0.56f;
        }
        return true;
    }
    if (preset == CC_MORPHOLOGY_QUADRUPED) {
        morphology->name = "QUADRUPED";
        morphology->limb_count = 4;
        morphology->minimum_supports = 3;
        morphology->maximum_swings = 1;
        morphology->duty_factor = 0.72f;
        ConfigureLimb(&morphology->limbs[0], -1.0f, 0.30f, 0.61f, 0.00f, 2);
        ConfigureLimb(&morphology->limbs[1], 1.0f, 0.30f, 0.61f, 0.50f, 2);
        ConfigureLimb(&morphology->limbs[2], -1.0f, -0.30f, -0.61f, 0.75f, 2);
        ConfigureLimb(&morphology->limbs[3], 1.0f, -0.30f, -0.61f, 0.25f, 2);
        return true;
    }
    if (preset == CC_MORPHOLOGY_HEXAPOD) {
        static const float z[] = {0.42f, 0.42f, 0.0f, 0.0f, -0.42f, -0.42f};
        static const float contact_z[] = {0.72f, 0.72f, 0.0f, 0.0f, -0.72f, -0.72f};
        static const float phase[] = {0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.5f};
        morphology->name = "HEXAPOD";
        morphology->limb_count = 6;
        morphology->minimum_supports = 3;
        morphology->maximum_swings = 3;
        morphology->duty_factor = 0.56f;
        for (int32_t limb = 0; limb < morphology->limb_count; ++limb) {
            ConfigureLimb(&morphology->limbs[limb], (limb & 1) ? 1.0f : -1.0f,
                          z[limb], contact_z[limb], phase[limb], 2);
        }
        return true;
    }

    morphology->name = "OCTOPOD";
    morphology->limb_count = 8;
    morphology->minimum_supports = 6;
    morphology->maximum_swings = 2;
    morphology->duty_factor = 0.76f;
    for (int32_t pair = 0; pair < 4; ++pair) {
        float t = (float)pair / 3.0f;
        float socket_z = 0.51f - t * 1.02f;
        float contact_z = 0.82f - t * 1.64f;
        int32_t left = pair * 2;
        int32_t right = left + 1;
        ConfigureLimb(&morphology->limbs[left], -1.0f, socket_z, contact_z,
                      (float)left / 8.0f, 3);
        ConfigureLimb(&morphology->limbs[right], 1.0f, socket_z, contact_z,
                      (float)right / 8.0f, 3);
    }
    return true;
}

float CcLimbChainLength(const CcLimbRig *rig, int32_t limb_index)
{
    if (rig == NULL || limb_index < 0 || limb_index >= rig->morphology.limb_count) {
        return 0.0f;
    }
    const CcLimbSpec *spec = &rig->morphology.limbs[limb_index];
    float total = 0.0f;
    for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
        total += spec->segment_length[segment];
    }
    return total;
}

static bool ProbeContact(const CcLimbMorphology *morphology,
                         CcLimbTerrainProbe probe, void *context,
                         CcLimbVec3 body_position, CcLimbVec3 desired,
                         CcLimbVec3 *point, CcLimbVec3 *normal)
{
    if (probe == NULL) {
        *point = desired;
        *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
        return true;
    }
    CcLimbVec3 origin = desired;
    origin.y = body_position.y + morphology->body_height;
    return probe(context, origin, morphology->body_height * 3.0f, point, normal);
}

static void InitializeChain(CcLimbRuntime *limb, const CcLimbSpec *spec,
                            CcLimbVec3 root, CcLimbVec3 target, float yaw)
{
    CcLimbVec3 bend = TransformDirection(spec->bend_local, yaw);
    for (int32_t joint = 0; joint <= spec->segment_count; ++joint) {
        float amount = (float)joint / (float)spec->segment_count;
        float arch = sinf(amount * CC_LIMB_PI);
        limb->joints[joint] = Add(Lerp(root, target, amount), Scale(bend, arch));
        limb->previous_joints[joint] = limb->joints[joint];
    }
}

static void SolveTwoBone(CcLimbRuntime *limb, const CcLimbSpec *spec,
                         CcLimbVec3 root, CcLimbVec3 target, float yaw)
{
    float upper = spec->segment_length[0];
    float lower = spec->segment_length[1];
    CcLimbVec3 root_to_target = Subtract(target, root);
    float raw_distance = Length(root_to_target);
    CcLimbVec3 direction = NormalizeOr(root_to_target,
                                       (CcLimbVec3){0.0f, -1.0f, 0.0f});
    float minimum_distance = fabsf(upper - lower) + 0.0001f;
    float maximum_distance = upper + lower;
    float solved_distance = Clamp(raw_distance, minimum_distance, maximum_distance);
    CcLimbVec3 solved_target = Add(root, Scale(direction, solved_distance));

    CcLimbVec3 pole = TransformDirection(spec->bend_local, yaw);
    pole = Subtract(pole, Scale(direction, Dot(pole, direction)));
    if (Length(pole) <= 0.0001f) {
        CcLimbVec3 fallback = TransformDirection((CcLimbVec3){0.0f, 0.0f, 1.0f},
                                                  yaw);
        pole = Subtract(fallback, Scale(direction, Dot(fallback, direction)));
    }
    pole = NormalizeOr(pole, (CcLimbVec3){1.0f, 0.0f, 0.0f});

    float along = (upper * upper - lower * lower +
                   solved_distance * solved_distance) /
                  (2.0f * solved_distance);
    float height = sqrtf(fmaxf(0.0f, upper * upper - along * along));
    CcLimbVec3 knee = Add(Add(root, Scale(direction, along)), Scale(pole, height));

    limb->joints[0] = root;
    limb->joints[1] = knee;
    limb->joints[2] = solved_target;
    for (int32_t joint = 0; joint <= 2; ++joint) {
        limb->previous_joints[joint] = limb->joints[joint];
    }
}

static void SolveChain(CcLimbRuntime *limb, const CcLimbSpec *spec,
                       CcLimbVec3 root, CcLimbVec3 target, float yaw,
                       float delta_time)
{
    if (spec->segment_count == 2) {
        SolveTwoBone(limb, spec, root, target, yaw);
        return;
    }
    CcLimbVec3 bend = TransformDirection(spec->bend_local, yaw);
    for (int32_t joint = 1; joint < spec->segment_count; ++joint) {
        CcLimbVec3 old = limb->joints[joint];
        CcLimbVec3 inertia = Scale(Subtract(old, limb->previous_joints[joint]), 0.72f);
        float amount = (float)joint / (float)spec->segment_count;
        CcLimbVec3 pole = Add(Lerp(root, target, amount),
                              Scale(bend, sinf(amount * CC_LIMB_PI)));
        limb->previous_joints[joint] = old;
        limb->joints[joint] = Add(Add(old, inertia),
                                  (CcLimbVec3){0.0f,
                                               -9.8f * delta_time * delta_time,
                                               0.0f});
        limb->joints[joint] = Lerp(limb->joints[joint], pole, 0.18f);
    }
    limb->joints[0] = root;
    limb->joints[spec->segment_count] = target;
    float reach = 0.0f;
    for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
        reach += spec->segment_length[segment];
    }
    if (Distance(root, target) >= reach - 0.0001f) {
        CcLimbVec3 direction = NormalizeOr(Subtract(target, root),
                                            (CcLimbVec3){0.0f, -1.0f, 0.0f});
        limb->joints[0] = root;
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            limb->joints[segment + 1] = Add(limb->joints[segment],
                                            Scale(direction,
                                                  spec->segment_length[segment]));
        }
        return;
    }
    for (int32_t iteration = 0; iteration < 7; ++iteration) {
        limb->joints[spec->segment_count] = target;
        for (int32_t segment = spec->segment_count - 1; segment >= 0; --segment) {
            CcLimbVec3 direction = NormalizeOr(
                Subtract(limb->joints[segment], limb->joints[segment + 1]),
                (CcLimbVec3){0.0f, 1.0f, 0.0f});
            limb->joints[segment] = Add(limb->joints[segment + 1],
                                        Scale(direction,
                                              spec->segment_length[segment]));
        }
        limb->joints[0] = root;
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            CcLimbVec3 direction = NormalizeOr(
                Subtract(limb->joints[segment + 1], limb->joints[segment]),
                (CcLimbVec3){0.0f, -1.0f, 0.0f});
            limb->joints[segment + 1] = Add(limb->joints[segment],
                                            Scale(direction,
                                                  spec->segment_length[segment]));
        }
    }
    limb->previous_joints[0] = limb->joints[0];
    limb->previous_joints[spec->segment_count] = limb->joints[spec->segment_count];
}

static float Cross2(CcPoint2 origin, CcPoint2 a, CcPoint2 b)
{
    return (a.x - origin.x) * (b.z - origin.z) -
           (a.z - origin.z) * (b.x - origin.x);
}

static void SortPoints(CcPoint2 *points, int32_t count)
{
    for (int32_t i = 1; i < count; ++i) {
        CcPoint2 value = points[i];
        int32_t position = i;
        while (position > 0 &&
               (points[position - 1].x > value.x ||
                (points[position - 1].x == value.x &&
                 points[position - 1].z > value.z))) {
            points[position] = points[position - 1];
            position -= 1;
        }
        points[position] = value;
    }
}

static float DistanceToSegment(CcPoint2 point, CcPoint2 a, CcPoint2 b)
{
    float x = b.x - a.x;
    float z = b.z - a.z;
    float length_squared = x * x + z * z;
    float amount = length_squared > 0.000001f ?
                   ((point.x - a.x) * x + (point.z - a.z) * z) / length_squared : 0.0f;
    amount = Clamp(amount, 0.0f, 1.0f);
    float dx = point.x - (a.x + x * amount);
    float dz = point.z - (a.z + z * amount);
    return sqrtf(dx * dx + dz * dz);
}

static float SignedSupportMargin(CcLimbVec3 body, const CcLimbVec3 *contacts,
                                 int32_t count)
{
    CcPoint2 point = {body.x, body.z};
    if (count <= 0) return -FLT_MAX;
    if (count == 1) {
        float dx = point.x - contacts[0].x;
        float dz = point.z - contacts[0].z;
        return 0.08f - sqrtf(dx * dx + dz * dz);
    }
    if (count == 2) {
        return 0.06f - DistanceToSegment(point,
                                         (CcPoint2){contacts[0].x, contacts[0].z},
                                         (CcPoint2){contacts[1].x, contacts[1].z});
    }
    CcPoint2 points[CC_LIMB_MAX_COUNT];
    CcPoint2 hull[CC_LIMB_MAX_COUNT * 2];
    for (int32_t i = 0; i < count; ++i) {
        points[i] = (CcPoint2){contacts[i].x, contacts[i].z};
    }
    SortPoints(points, count);
    int32_t hull_count = 0;
    for (int32_t i = 0; i < count; ++i) {
        while (hull_count >= 2 &&
               Cross2(hull[hull_count - 2], hull[hull_count - 1], points[i]) <= 0.0f) {
            hull_count -= 1;
        }
        hull[hull_count++] = points[i];
    }
    int32_t lower_count = hull_count;
    for (int32_t i = count - 2; i >= 0; --i) {
        while (hull_count > lower_count &&
               Cross2(hull[hull_count - 2], hull[hull_count - 1], points[i]) <= 0.0f) {
            hull_count -= 1;
        }
        hull[hull_count++] = points[i];
    }
    if (hull_count > 1) hull_count -= 1;
    if (hull_count < 3) {
        return 0.06f - DistanceToSegment(point, hull[0], hull[hull_count - 1]);
    }
    float margin = FLT_MAX;
    for (int32_t i = 0; i < hull_count; ++i) {
        CcPoint2 a = hull[i];
        CcPoint2 b = hull[(i + 1) % hull_count];
        float edge_x = b.x - a.x;
        float edge_z = b.z - a.z;
        float edge_length = sqrtf(edge_x * edge_x + edge_z * edge_z);
        if (edge_length <= 0.00001f) continue;
        float signed_distance = (edge_x * (point.z - a.z) -
                                 edge_z * (point.x - a.x)) / edge_length;
        margin = fminf(margin, signed_distance);
    }
    return margin;
}

static int32_t GatherContacts(const CcLimbRig *rig, int32_t excluded,
                              bool grounded, CcLimbVec3 *contacts,
                              CcLimbVec3 *center)
{
    int32_t count = 0;
    *center = (CcLimbVec3){0};
    if (!grounded) return 0;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        const CcLimbRuntime *runtime = &rig->limbs[limb];
        if (limb == excluded || runtime->state != CC_LIMB_STANCE ||
            runtime->health <= 0.0f) continue;
        contacts[count++] = runtime->planted_contact;
        *center = Add(*center, runtime->planted_contact);
    }
    if (count > 0) *center = Scale(*center, 1.0f / (float)count);
    return count;
}

static bool CanLift(const CcLimbRig *rig, int32_t candidate,
                    CcLimbVec3 body_position, bool grounded, bool emergency)
{
    if (!grounded) return true;
    CcLimbVec3 contacts[CC_LIMB_MAX_COUNT];
    CcLimbVec3 center;
    int32_t count = GatherContacts(rig, candidate, true, contacts, &center);
    if (count < rig->morphology.minimum_supports) return false;
    if (rig->morphology.dynamic_balance || count < 3) return true;
    float margin = SignedSupportMargin(body_position, contacts, count);
    return margin >= -0.045f || emergency;
}

void CcLimbRigInit(CcLimbRig *rig, const CcLimbMorphology *morphology,
                   CcLimbVec3 body_position, float body_yaw,
                   CcLimbTerrainProbe probe, void *probe_context)
{
    if (rig == NULL || morphology == NULL) return;
    *rig = (CcLimbRig){0};
    rig->morphology = *morphology;
    rig->traction = 1.0f;
    rig->drive_scale = 1.0f;
    rig->active_pose_limb = -1;
    for (int32_t index = 0; index < morphology->limb_count; ++index) {
        const CcLimbSpec *spec = &morphology->limbs[index];
        CcLimbRuntime *limb = &rig->limbs[index];
        CcLimbVec3 root = TransformPoint(body_position, spec->socket_local, body_yaw);
        CcLimbVec3 desired = TransformPoint(body_position, spec->rest_contact_local,
                                             body_yaw);
        CcLimbVec3 normal = {0.0f, 1.0f, 0.0f};
        (void)ProbeContact(morphology, probe, probe_context, body_position, desired,
                           &desired, &normal);
        limb->planted_contact = desired;
        limb->desired_contact = desired;
        limb->contact_start = desired;
        limb->contact_target = desired;
        limb->contact_normal = normal;
        limb->state = CC_LIMB_STANCE;
        limb->swing_progress = 1.0f;
        limb->health = 1.0f;
        InitializeChain(limb, spec, root, desired, body_yaw);
        SolveChain(limb, spec, root, desired, body_yaw, 0.0f);
    }
    rig->initialized = true;
}

static void UpdateSwing(CcLimbRuntime *limb, const CcLimbMorphology *morphology,
                        float delta_time)
{
    if (limb->state != CC_LIMB_SWING) return;
    float duration = morphology->swing_seconds / fmaxf(0.35f, limb->health);
    limb->swing_progress = fminf(1.0f, limb->swing_progress + delta_time / duration);
    float amount = limb->swing_progress;
    float eased = amount * amount * (3.0f - 2.0f * amount);
    limb->planted_contact = Lerp(limb->contact_start, limb->contact_target, eased);
    limb->planted_contact.y += sinf(amount * CC_LIMB_PI) * morphology->step_height;
    if (amount >= 1.0f) {
        limb->planted_contact = limb->contact_target;
        limb->state = CC_LIMB_STANCE;
    }
}

static void StartSwing(CcLimbRig *rig, int32_t limb_index)
{
    CcLimbRuntime *limb = &rig->limbs[limb_index];
    limb->contact_start = limb->planted_contact;
    limb->contact_target = limb->desired_contact;
    limb->swing_progress = 0.0f;
    limb->state = CC_LIMB_SWING;
    if (rig->morphology.preset == CC_MORPHOLOGY_BIPED) {
        rig->active_pose_limb = limb_index;
    }
}

static void CalculateSupport(CcLimbRig *rig, CcLimbVec3 body_position,
                             float body_yaw, bool grounded)
{
    CcLimbVec3 contacts[CC_LIMB_MAX_COUNT];
    rig->planted_count = GatherContacts(rig, -1, grounded, contacts,
                                        &rig->support_center);
    rig->swinging_count = 0;
    int32_t healthy_count = 0;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        if (rig->limbs[limb].health > 0.0f) healthy_count += 1;
        if (rig->limbs[limb].state == CC_LIMB_SWING) rig->swinging_count += 1;
    }
    rig->support_margin = SignedSupportMargin(body_position, contacts,
                                              rig->planted_count);
    rig->body_acceleration = (CcLimbVec3){0};
    rig->drive_scale = grounded ? 1.0f : 0.18f;
    if (rig->planted_count > 0) {
        float urgency = Clamp((rig->morphology.support_margin - rig->support_margin) *
                              5.0f, 0.0f, 1.0f);
        CcLimbVec3 correction = Subtract(rig->support_center, body_position);
        correction.y = 0.0f;
        if (rig->morphology.dynamic_balance) {
            CcLimbVec3 right = TransformDirection((CcLimbVec3){1.0f, 0.0f, 0.0f},
                                                   body_yaw);
            float lateral_error = Clamp(Dot(correction, right), -0.18f, 0.18f);
            rig->body_acceleration = Scale(right, lateral_error * 1.8f * urgency);
            float greatest_extension = 0.0f;
            for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
                const CcLimbRuntime *runtime = &rig->limbs[limb];
                if (runtime->state != CC_LIMB_STANCE || runtime->health <= 0.0f) {
                    continue;
                }
                const CcLimbSpec *spec = &rig->morphology.limbs[limb];
                CcLimbVec3 root = TransformPoint(body_position, spec->socket_local,
                                                 body_yaw);
                float chain_length = CcLimbChainLength(rig, limb);
                if (chain_length <= 0.0001f) continue;
                float extension = Distance(root, runtime->planted_contact) /
                                  chain_length;
                greatest_extension = fmaxf(greatest_extension, extension);
            }
            rig->drive_scale = Clamp((0.985f - greatest_extension) / 0.065f,
                                     0.12f, 1.0f);
        } else {
            rig->body_acceleration = Scale(correction, 3.5f * urgency);
        }
        float average_height = 0.0f;
        for (int32_t contact = 0; contact < rig->planted_count; ++contact) {
            average_height += contacts[contact].y;
        }
        average_height /= (float)rig->planted_count;
        float desired_offset = Clamp(average_height + rig->morphology.body_height -
                                     body_position.y, -0.22f, 0.22f);
        rig->supported_height_offset +=
            (desired_offset - rig->supported_height_offset) * 0.16f;
    }
    float contact_ratio = rig->morphology.minimum_supports > 0 ?
                          (float)rig->planted_count /
                          (float)rig->morphology.minimum_supports : 1.0f;
    float health_ratio = rig->morphology.limb_count > 0 ?
                         (float)healthy_count / (float)rig->morphology.limb_count : 0.0f;
    rig->traction = grounded ? Clamp(fminf(contact_ratio, 1.0f) * health_ratio,
                                     0.12f, 1.0f) : 0.12f;
}

void CcLimbRigUpdate(CcLimbRig *rig, CcLimbVec3 body_position, float body_yaw,
                     CcLimbVec3 body_velocity, bool body_grounded,
                     float delta_time, CcLimbTerrainProbe probe,
                     void *probe_context)
{
    if (rig == NULL || !rig->initialized) return;
    delta_time = Clamp(delta_time, 0.0f, 1.0f / 30.0f);
    float speed = sqrtf(body_velocity.x * body_velocity.x +
                        body_velocity.z * body_velocity.z);
    if (speed > 0.025f) {
        float cycle_distance = fmaxf(0.30f, rig->morphology.step_threshold * 3.2f);
        rig->gait_phase = Wrap01(rig->gait_phase + speed * delta_time /
                                 cycle_distance);
    }

    for (int32_t index = 0; index < rig->morphology.limb_count; ++index) {
        CcLimbRuntime *limb = &rig->limbs[index];
        const CcLimbSpec *spec = &rig->morphology.limbs[index];
        CcLimbVec3 desired = TransformPoint(body_position, spec->rest_contact_local,
                                             body_yaw);
        if (!body_grounded) {
            float smoothing = 1.0f - expf(-14.0f * delta_time);
            limb->desired_contact = Lerp(limb->desired_contact, desired, smoothing);
            limb->contact_normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
            if (limb->health > 0.0f) limb->state = CC_LIMB_SEARCHING;
            continue;
        }
        desired.x += body_velocity.x * rig->morphology.velocity_lead;
        desired.z += body_velocity.z * rig->morphology.velocity_lead;
        CcLimbVec3 normal = {0.0f, 1.0f, 0.0f};
        CcLimbVec3 probed = desired;
        if (ProbeContact(&rig->morphology, probe, probe_context, body_position,
                         desired, &probed, &normal)) {
            float smoothing = 1.0f - expf(-10.0f * delta_time);
            limb->desired_contact = Lerp(limb->desired_contact, probed, smoothing);
            limb->contact_normal = NormalizeOr(Lerp(limb->contact_normal, normal,
                                                    smoothing),
                                                (CcLimbVec3){0.0f, 1.0f, 0.0f});
            if (limb->state == CC_LIMB_SEARCHING && limb->health > 0.0f) {
                int32_t end = spec->segment_count;
                limb->planted_contact = limb->joints[end];
                StartSwing(rig, index);
            }
        } else if (limb->state == CC_LIMB_SWING) {
            limb->state = CC_LIMB_SEARCHING;
        }
        UpdateSwing(limb, &rig->morphology, delta_time);
    }

    int32_t active_swings = 0;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        if (rig->limbs[limb].state == CC_LIMB_SWING) active_swings += 1;
    }
    for (int32_t slot = active_swings; slot < rig->morphology.maximum_swings; ++slot) {
        int32_t candidate = -1;
        float candidate_score = 0.0f;
        for (int32_t index = 0; index < rig->morphology.limb_count; ++index) {
            CcLimbRuntime *limb = &rig->limbs[index];
            const CcLimbSpec *spec = &rig->morphology.limbs[index];
            if (limb->state != CC_LIMB_STANCE || limb->health <= 0.0f) continue;
            float stretch = Distance(limb->planted_contact, limb->desired_contact);
            float phase = Wrap01(rig->gait_phase + spec->phase_offset);
            bool scheduled = phase >= rig->morphology.duty_factor;
            bool emergency = stretch > rig->morphology.step_threshold * 1.65f;
            float trigger = rig->morphology.dynamic_balance ?
                            rig->morphology.step_threshold * 0.72f :
                            rig->morphology.step_threshold;
            if (stretch < trigger || (!scheduled && !emergency)) {
                continue;
            }
            if (!CanLift(rig, index, body_position, body_grounded, emergency)) continue;
            float score = stretch / rig->morphology.step_threshold +
                          (scheduled ? 0.35f : 0.0f);
            if (score > candidate_score) {
                candidate = index;
                candidate_score = score;
            }
        }
        if (candidate < 0) break;
        StartSwing(rig, candidate);
    }

    for (int32_t index = 0; index < rig->morphology.limb_count; ++index) {
        CcLimbRuntime *limb = &rig->limbs[index];
        const CcLimbSpec *spec = &rig->morphology.limbs[index];
        CcLimbVec3 root = TransformPoint(body_position, spec->socket_local, body_yaw);
        CcLimbVec3 target = limb->planted_contact;
        if (limb->state == CC_LIMB_DISABLED) {
            target = TransformPoint(body_position, spec->rest_contact_local, body_yaw);
            target.y += 0.18f;
        } else if (limb->state == CC_LIMB_SEARCHING) {
            target = limb->desired_contact;
        }
        SolveChain(limb, spec, root, target, body_yaw, delta_time);
    }
    if (rig->morphology.preset == CC_MORPHOLOGY_BIPED &&
        rig->active_pose_limb >= 0 && rig->active_pose_limb < 2) {
        float progress = rig->limbs[rig->active_pose_limb].swing_progress;
        rig->pose_phase = rig->active_pose_limb == 0 ? progress * 0.5f :
                          Wrap01(0.5f + progress * 0.5f);
    }
    CalculateSupport(rig, body_position, body_yaw, body_grounded);
}

void CcLimbRigPinContact(CcLimbRig *rig, int32_t limb_index,
                         CcLimbVec3 body_position, float body_yaw,
                         CcLimbVec3 contact, CcLimbVec3 normal)
{
    if (rig == NULL || limb_index < 0 ||
        limb_index >= rig->morphology.limb_count) return;
    CcLimbRuntime *limb = &rig->limbs[limb_index];
    if (limb->health <= 0.0f) return;
    const CcLimbSpec *spec = &rig->morphology.limbs[limb_index];
    limb->planted_contact = contact;
    limb->desired_contact = contact;
    limb->contact_start = contact;
    limb->contact_target = contact;
    limb->contact_normal = NormalizeOr(normal, (CcLimbVec3){0.0f, 1.0f, 0.0f});
    limb->state = CC_LIMB_STANCE;
    limb->swing_progress = 1.0f;
    CcLimbVec3 root = TransformPoint(body_position, spec->socket_local, body_yaw);
    CcLimbSpec contact_spec = *spec;
    if (rig->morphology.preset == CC_MORPHOLOGY_BIPED &&
        fabsf(limb->contact_normal.y) < 0.5f) {
        /* A climber faces into the wall, so the ordinary forward knee pole would
           put the knee through it. Wall contacts deliberately bend away. */
        contact_spec.bend_local.z = -fabsf(contact_spec.bend_local.z);
    }
    SolveChain(limb, &contact_spec, root, contact, body_yaw, 0.0f);
}

void CcLimbRigSetHealth(CcLimbRig *rig, int32_t limb_index, float health)
{
    if (rig == NULL || limb_index < 0 || limb_index >= rig->morphology.limb_count) return;
    CcLimbRuntime *limb = &rig->limbs[limb_index];
    limb->health = Clamp(health, 0.0f, 1.0f);
    if (limb->health <= 0.0f) {
        limb->state = CC_LIMB_DISABLED;
    } else if (limb->state == CC_LIMB_DISABLED) {
        limb->state = CC_LIMB_SEARCHING;
    }
}

const char *CcLimbStateName(CcLimbState state)
{
    switch (state) {
        case CC_LIMB_STANCE: return "STANCE";
        case CC_LIMB_SWING: return "SWING";
        case CC_LIMB_SEARCHING: return "SEARCH";
        case CC_LIMB_DISABLED: return "DISABLED";
        default: return "UNKNOWN";
    }
}
