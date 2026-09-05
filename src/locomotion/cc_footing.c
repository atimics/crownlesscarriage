#include "locomotion/cc_footing.h"

#include <math.h>
#include <stddef.h>

static bool Finite(CcLimbVec3 v)
{
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

bool CcFootingProbe(CcLimbTerrainProbe probe, void *context,
                    CcLimbVec3 desired, float rise, float drop,
                    CcLimbVec3 *point, CcLimbVec3 *normal)
{
    if (point == NULL || normal == NULL || !Finite(desired) ||
        !isfinite(rise) || !isfinite(drop) || rise < 0.0f || drop < 0.0f) {
        return false;
    }
    CcLimbVec3 hit = desired;
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    if (probe != NULL) {
        CcLimbVec3 origin = desired;
        origin.y += rise;
        if (!probe(context, origin, rise + drop, &hit, &up)) return false;
    }
    if (!Finite(hit) || !Finite(up) || hit.y > desired.y + rise + 0.001f ||
        hit.y < desired.y - drop - 0.001f) return false;
    float length = sqrtf(up.x * up.x + up.y * up.y + up.z * up.z);
    if (length < 0.001f || up.y / length < 0.55f) return false;
    *point = hit;
    *normal = (CcLimbVec3){up.x / length, up.y / length, up.z / length};
    return true;
}

bool CcFootingContactValid(CcLimbTerrainProbe probe, void *context,
                           CcLimbVec3 contact, float tolerance,
                           CcLimbVec3 *normal)
{
    CcLimbVec3 hit;
    if (!CcFootingProbe(probe, context, contact, tolerance, tolerance,
                        &hit, normal)) return false;
    float dx = hit.x - contact.x;
    float dy = hit.y - contact.y;
    float dz = hit.z - contact.z;
    return dx * dx + dy * dy + dz * dz <= tolerance * tolerance;
}

CcLimbVec3 CcFootingSwingPoint(CcLimbVec3 start, CcLimbVec3 target,
                               float travel, float lift)
{
    travel = fmaxf(0.0f, fminf(1.0f, travel));
    float arch = travel > 0.0f && travel < 1.0f ?
        sinf(travel * 3.14159265358979323846f) : 0.0f;
    return (CcLimbVec3){
        start.x + (target.x - start.x) * travel,
        start.y + (target.y - start.y) * travel + arch * lift,
        start.z + (target.z - start.z) * travel};
}

bool CcFootingPlanSwing(CcLimbVec3 start, CcLimbVec3 target,
                        float base_lift, float maximum_lift, float radius,
                        CcLimbTerrainProbe terrain,
                        CcBiomechRagdollCollisionProbe collision,
                        void *context, float *lift)
{
    if (lift == NULL || !Finite(start) || !Finite(target) ||
        !isfinite(base_lift) || !isfinite(maximum_lift) || !isfinite(radius) ||
        base_lift < 0.0f || maximum_lift < base_lift || radius <= 0.0f) return false;
    float dx = target.x - start.x;
    float dz = target.z - start.z;
    float distance = sqrtf(dx * dx + dz * dz);
    int32_t samples = (int32_t)fminf(64.0f, fmaxf(8.0f, ceilf(distance / radius)));
    float clearance = base_lift;
    for (int32_t i = 1; i < samples; ++i) {
        float t = (float)i / (float)samples;
        CcLimbVec3 line = CcFootingSwingPoint(start, target, t, 0.0f);
        CcLimbVec3 surface;
        CcLimbVec3 normal;
        if (CcFootingProbe(terrain, context, line, fmaxf(2.0f, maximum_lift * 4.0f),
                           maximum_lift, &surface, &normal)) {
            float arch = sinf(t * 3.14159265358979323846f);
            clearance = fmaxf(clearance, (surface.y - line.y) / arch + radius);
        }
    }
    if (clearance > maximum_lift) return false;
    if (collision != NULL) {
        CcBiomechVec3 previous = {start.x, start.y + radius, start.z};
        for (int32_t i = 0; i <= samples; ++i) {
            CcLimbVec3 p = CcFootingSwingPoint(start, target,
                (float)i / (float)samples, clearance);
            CcBiomechVec3 resolved;
            CcBiomechVec3 normal;
            CcBiomechVec3 next = {p.x, p.y + radius, p.z};
            if (collision(context, previous, next, radius, &resolved, &normal)) return false;
            previous = next;
        }
    }
    *lift = clearance;
    return true;
}
