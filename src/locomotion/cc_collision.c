#include "locomotion/cc_collision.h"

#include <math.h>
#include <stddef.h>

static CcLimbVec3 Add(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static CcLimbVec3 Subtract(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CcLimbVec3 Scale(CcLimbVec3 v, float scale)
{
    return (CcLimbVec3){v.x * scale, v.y * scale, v.z * scale};
}

static float Dot(CcLimbVec3 a, CcLimbVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static bool Finite(CcLimbVec3 v)
{
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

static bool ValidBox(const CcCollisionBox *box)
{
    return box != NULL && Finite(box->minimum) && Finite(box->maximum) &&
        isfinite(box->yaw) && box->maximum.x >= box->minimum.x &&
        box->maximum.y >= box->minimum.y && box->maximum.z >= box->minimum.z;
}

static CcLimbVec3 Center(const CcCollisionBox *box)
{
    return Add(box->minimum, Scale(Subtract(box->maximum, box->minimum), 0.5f));
}

static CcLimbVec3 ToLocal(const CcCollisionBox *box, CcLimbVec3 point)
{
    CcLimbVec3 delta = Subtract(point, Center(box));
    float cosine = cosf(box->yaw);
    float sine = sinf(box->yaw);
    return (CcLimbVec3){cosine * delta.x - sine * delta.z, delta.y,
                        sine * delta.x + cosine * delta.z};
}

static CcLimbVec3 ToWorldNormal(const CcCollisionBox *box, CcLimbVec3 v)
{
    float cosine = cosf(box->yaw);
    float sine = sinf(box->yaw);
    return (CcLimbVec3){cosine * v.x + sine * v.z, v.y,
                        -sine * v.x + cosine * v.z};
}

static CcLimbVec3 Closest(CcLimbVec3 point, CcLimbVec3 half)
{
    return (CcLimbVec3){fmaxf(-half.x, fminf(point.x, half.x)),
                        fmaxf(-half.y, fminf(point.y, half.y)),
                        fmaxf(-half.z, fminf(point.z, half.z))};
}

static CcLimbVec3 ContactNormal(CcLimbVec3 point, CcLimbVec3 half,
                               float *distance)
{
    CcLimbVec3 delta = Subtract(point, Closest(point, half));
    *distance = sqrtf(Dot(delta, delta));
    if (*distance > 0.000001f) return Scale(delta, 1.0f / *distance);
    const float gaps[] = {point.x + half.x, half.x - point.x,
                          point.y + half.y, half.y - point.y,
                          point.z + half.z, half.z - point.z};
    const CcLimbVec3 normals[] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
                                  {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    int32_t face = 0;
    for (int32_t i = 1; i < 6; ++i) {
        if (gaps[i] < gaps[face]) face = i;
    }
    *distance = -gaps[face];
    return normals[face];
}

bool CcCollisionPushSphere(const CcCollisionBox *box, float radius,
                           CcLimbVec3 *position, CcLimbVec3 *normal)
{
    if (!ValidBox(box) || position == NULL || normal == NULL ||
        !Finite(*position) || !isfinite(radius) || radius <= 0.0f) return false;
    CcLimbVec3 half = Scale(Subtract(box->maximum, box->minimum), 0.5f);
    CcLimbVec3 local = ToLocal(box, *position);
    if (fabsf(local.x) > half.x + radius || fabsf(local.y) > half.y + radius ||
        fabsf(local.z) > half.z + radius) return false;
    float distance;
    CcLimbVec3 local_normal = ContactNormal(local, half, &distance);
    if (distance >= radius - 0.000001f) return false;
    *normal = ToWorldNormal(box, local_normal);
    *position = Add(*position, Scale(*normal, radius - distance));
    return true;
}

bool CcCollisionSweepSphere(const CcCollisionBox *box, CcLimbVec3 start,
                            CcLimbVec3 end, float radius,
                            CcCollisionHit *hit)
{
    if (!ValidBox(box) || hit == NULL || !Finite(start) || !Finite(end) ||
        !isfinite(radius) || radius <= 0.0f) return false;
    CcLimbVec3 half = Scale(Subtract(box->maximum, box->minimum), 0.5f);
    CcLimbVec3 first = ToLocal(box, start);
    CcLimbVec3 last = ToLocal(box, end);
    if (fminf(first.x, last.x) > half.x + radius ||
        fmaxf(first.x, last.x) < -half.x - radius ||
        fminf(first.y, last.y) > half.y + radius ||
        fmaxf(first.y, last.y) < -half.y - radius ||
        fminf(first.z, last.z) > half.z + radius ||
        fmaxf(first.z, last.z) < -half.z - radius) return false;
    CcLimbVec3 move = Subtract(last, first);
    float distance;
    CcLimbVec3 normal = ContactNormal(first, half, &distance);
    if (distance <= radius) {
        if (Dot(move, normal) >= -0.000001f) return false;
        *hit = (CcCollisionHit){0.0f, ToWorldNormal(box, normal)};
        return true;
    }

    /* Distance to a box is quadratic between crossings of its six planes.
       Solve each interval exactly, including rounded edges and corners. */
    const float starts[] = {first.x, first.y, first.z};
    const float moves[] = {move.x, move.y, move.z};
    const float extents[] = {half.x, half.y, half.z};
    float times[8] = {0.0f, 1.0f};
    int32_t count = 2;
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (fabsf(moves[axis]) < 0.000001f) continue;
        for (int32_t side = -1; side <= 1; side += 2) {
            float time = ((float)side * extents[axis] - starts[axis]) /
                         moves[axis];
            if (time <= 0.0f || time >= 1.0f) continue;
            int32_t slot = count++;
            while (slot > 0 && times[slot - 1] > time) {
                times[slot] = times[slot - 1];
                slot -= 1;
            }
            times[slot] = time;
        }
    }
    for (int32_t interval = 0; interval + 1 < count; ++interval) {
        float begin = times[interval];
        float finish = times[interval + 1];
        float middle = (begin + finish) * 0.5f;
        double a = 0.0;
        double b = 0.0;
        double c = -(double)radius * (double)radius;
        for (int32_t axis = 0; axis < 3; ++axis) {
            float sample = starts[axis] + moves[axis] * middle;
            if (sample >= -extents[axis] && sample <= extents[axis]) continue;
            float edge = sample < 0.0f ? -extents[axis] : extents[axis];
            double offset = (double)starts[axis] + (double)moves[axis] * begin - edge;
            a += (double)moves[axis] * moves[axis];
            b += offset * moves[axis];
            c += offset * offset;
        }
        if (a <= 0.0 || b >= 0.0) continue;
        double discriminant = b * b - a * c;
        if (discriminant < 0.0) continue;
        double root = (-b - sqrt(discriminant)) / a;
        if (root < -0.000001 || root > (double)(finish - begin) + 0.000001) continue;
        float time = fmaxf(begin, fminf(finish, begin + (float)root));
        CcLimbVec3 point = Add(first, Scale(move, time));
        normal = ContactNormal(point, half, &distance);
        if (Dot(move, normal) >= -0.000001f) continue;
        *hit = (CcCollisionHit){time, ToWorldNormal(box, normal)};
        return true;
    }
    return false;
}

static CcCollisionBox CapsuleBox(const CcCollisionBox *box, float segment)
{
    CcCollisionBox expanded = *box;
    expanded.minimum.y -= segment;
    return expanded;
}

static CcCollisionMove MoveCapsule(const CcCollisionBox *boxes,
                                      int32_t count, CcLimbVec3 feet,
                                      CcLimbVec3 displacement,
                                      float radius, float height, float skin)
{
    CcCollisionMove result = {.position = feet, .normal = {0, 0, 0}};
    if (!Finite(feet) || !Finite(displacement) || !isfinite(radius) ||
        !isfinite(height) || radius <= 0.0f || height < radius * 2.0f ||
        count < 0 || (count > 0 && boxes == NULL)) return result;
    float segment = height - radius * 2.0f;
    CcLimbVec3 center = Add(feet, (CcLimbVec3){0.0f, radius, 0.0f});
    float clearance = radius + skin;
    for (int32_t pass = 0; pass < 8; ++pass) {
        bool corrected = false;
        for (int32_t i = 0; i < count; ++i) {
            CcCollisionBox box = CapsuleBox(&boxes[i], segment);
            CcLimbVec3 normal;
            if (CcCollisionPushSphere(&box, clearance, &center, &normal)) {
                result.normal = normal;
                result.collided = corrected = true;
            }
        }
        if (!corrected) break;
    }
    CcLimbVec3 remaining = displacement;
    for (int32_t pass = 0; pass < 8 && Dot(remaining, remaining) > 1.0e-12f;
         ++pass) {
        CcCollisionHit nearest = {.fraction = 1.0f, .normal = {0, 0, 0}};
        bool collided = false;
        for (int32_t i = 0; i < count; ++i) {
            CcCollisionBox box = CapsuleBox(&boxes[i], segment);
            CcCollisionHit hit;
            if (CcCollisionSweepSphere(&box, center, Add(center, remaining),
                                       clearance, &hit) &&
                (!collided || hit.fraction < nearest.fraction)) {
                nearest = hit;
                collided = true;
            }
        }
        if (!collided) {
            center = Add(center, remaining);
            break;
        }
        center = Add(center, Scale(remaining, nearest.fraction));
        remaining = Scale(remaining, 1.0f - nearest.fraction);
        float inward = Dot(remaining, nearest.normal);
        if (inward < 0.0f) {
            remaining = Subtract(remaining, Scale(nearest.normal, inward));
        }
        result.normal = nearest.normal;
        result.collided = true;
    }
    result.position = Subtract(center, (CcLimbVec3){0.0f, radius, 0.0f});
    return result;
}

CcCollisionMove CcCollisionMoveCapsule(const CcCollisionBox *boxes, int32_t count,
                                      CcLimbVec3 feet, CcLimbVec3 displacement,
                                      float radius, float height)
{
    return MoveCapsule(boxes, count, feet, displacement, radius, height,
                        CC_COLLISION_SKIN);
}

CcCollisionMove CcCollisionMoveSphere(const CcCollisionBox *boxes, int32_t count,
                                     CcLimbVec3 center, CcLimbVec3 displacement,
                                     float radius)
{
    if (!isfinite(radius) || radius <= 0.0f) {
        return (CcCollisionMove){.position = center};
    }
    CcLimbVec3 feet = Subtract(center, (CcLimbVec3){0, radius, 0});
    CcCollisionMove result = MoveCapsule(boxes, count, feet, displacement,
                                        radius, radius * 2.0f, 0.0f);
    result.position.y += radius;
    return result;
}
