#include "locomotion/cc_robotics.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool WallProbe(void *context, CcLimbVec3 sample, float search_radius,
                       CcLimbVec3 *point, CcLimbVec3 *normal)
{
    (void)context;
    if (fabsf(sample.z) > search_radius || sample.x < -1.0f ||
        sample.x > 6.0f || sample.y < 0.0f || sample.y > 8.0f ||
        (sample.x > 2.0f && sample.x < 3.0f && sample.y < 4.0f)) {
        return false;
    }
    *point = (CcLimbVec3){sample.x, sample.y, 0.0f};
    *normal = (CcLimbVec3){0.0f, 0.0f, -1.0f};
    return true;
}

static uint64_t HashFloat(uint64_t hash, float value)
{
    return (hash ^ (uint64_t)(int64_t)(value * 10000.0f)) *
           UINT64_C(1099511628211);
}

int main(void)
{
    const int32_t pair_count = 2000000;
    const int32_t route_count = 128;
    int32_t corrections = 0;
    uint64_t checksum = UINT64_C(14695981039346656037);
    clock_t start = clock();
    for (int32_t pair = 0; pair < pair_count; ++pair) {
        CcLimbVec3 first = {0};
        CcLimbVec3 second = {0};
        bool corrected = CcRobotPredictiveAvoidance(
            (CcLimbVec3){(float)(pair % 257) * 0.10f, 0.0f,
                         (float)(pair % 11) * 0.17f},
            (CcLimbVec3){-1.2f, 0.0f, 0.0f}, (CcLimbVec3){0},
            (CcLimbVec3){0.3f, 0.0f, 0.2f}, 0.8f, 0.85f, pair,
            &first, &second);
        if (corrected) {
            if (!isfinite(first.x) || !isfinite(first.z) ||
                fabsf(first.x + second.x) > 0.00001f ||
                fabsf(first.z + second.z) > 0.00001f) return EXIT_FAILURE;
            corrections += 1;
            checksum = HashFloat(checksum, first.x);
            checksum = HashFloat(checksum, first.z);
        }
    }
    double pair_seconds = (double)(clock() - start) / (double)CLOCKS_PER_SEC;
    int32_t routes_found = 0;
    start = clock();
    for (int32_t index = 0; index < route_count; ++index) {
        CcRobotClimbRoute route;
        CcLimbVec3 goal = {index % 4 == 0 ? 8.0f : 5.0f,
                           5.0f + (float)(index % 3) * 0.2f, 0.0f};
        bool found = CcRobotPlanFreeClimb(
            (CcLimbVec3){0.0f, 0.4f, 0.0f},
            (CcLimbVec3){0.0f, 0.0f, -1.0f}, goal,
            (CcLimbVec3){0.0f, 0.0f, -1.0f}, 0.38f, 0.62f,
            WallProbe, NULL, &route);
        if (found != (index % 4 != 0)) return EXIT_FAILURE;
        if (!found) continue;
        routes_found += 1;
        checksum = HashFloat(checksum, route.length);
        for (int32_t node = 0; node < route.count; ++node) {
            checksum = HashFloat(checksum, route.nodes[node].point.x);
            checksum = HashFloat(checksum, route.nodes[node].point.y);
        }
    }
    double route_seconds = (double)(clock() - start) / (double)CLOCKS_PER_SEC;
    (void)printf("avoidance: pairs=%d corrections=%d cpu=%.6fs ns/pair=%.1f\n",
                 pair_count, corrections, pair_seconds,
                 pair_seconds * 1.0e9 / (double)pair_count);
    (void)printf("climbing: requests=%d routes=%d cpu=%.6fs us/request=%.1f\n",
                 route_count, routes_found, route_seconds,
                 route_seconds * 1.0e6 / (double)route_count);
    (void)printf("checksum=%016" PRIx64 "\n", checksum);
    return corrections > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
