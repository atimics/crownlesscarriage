/* Deterministic math: the build must not fuse a*b+c, and the functions
   must stay within one float ulp of a double reference. Cross-platform
   bit equality is checked by the determinism-trace CI job. */
#include "sim/cc_dmath.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        failures += 1; \
    } \
} while (0)

static int64_t UlpDistance(float first, float second)
{
    int32_t a = 0;
    int32_t b = 0;
    memcpy(&a, &first, sizeof(a));
    memcpy(&b, &second, sizeof(b));
    int64_t left = a < 0 ? (int64_t)INT32_MIN - a : a;
    int64_t right = b < 0 ? (int64_t)INT32_MIN - b : b;
    return left > right ? left - right : right - left;
}

int main(void)
{
    CHECK(CcDmathContractionIsOff());
    int64_t worst_sin = 0;
    int64_t worst_cos = 0;
    int64_t worst_atan = 0;
    uint32_t state = UINT32_C(0x9e3779b9);
    for (int32_t i = 0; i < 200000; ++i) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        float x = ((float)(state >> 8) / 16777216.0f - 0.5f) * 40.0f;
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        float y = ((float)(state >> 8) / 16777216.0f - 0.5f) * 800.0f;
        int64_t sin_error = UlpDistance(CcDmathSinf(x),
                                        (float)sin((double)x));
        int64_t cos_error = UlpDistance(CcDmathCosf(x),
                                        (float)cos((double)x));
        int64_t atan_error = UlpDistance(
            CcDmathAtan2f(y, x), (float)atan2((double)y, (double)x));
        if (sin_error > worst_sin) worst_sin = sin_error;
        if (cos_error > worst_cos) worst_cos = cos_error;
        if (atan_error > worst_atan) worst_atan = atan_error;
        double power = -(double)(i % 5000) / 365.0;
        double exp_error = fabs(CcDmathExp2(power) / exp2(power) - 1.0);
        CHECK(exp_error < 1e-14);
    }
    CHECK(worst_sin <= 1);
    CHECK(worst_cos <= 1);
    CHECK(worst_atan <= 1);
    CHECK(CcDmathAtan2f(0.0f, 0.0f) == 0.0f);
    CHECK(CcDmathAtan2f(0.0f, -1.0f) == 3.14159265f);
    CHECK(CcDmathAtan2f(-1.0f, 0.0f) == -1.57079633f);
    CHECK(CcDmathSinf(0.0f) == 0.0f && CcDmathCosf(0.0f) == 1.0f);
    CHECK(CcDmathExp2(0.0) == 1.0 && CcDmathExp2(-1.0) == 0.5);
    CHECK(CcDmathSqrtf(2.0f) == sqrtf(2.0f));
    if (failures != 0) return EXIT_FAILURE;
    (void)printf("dmath: worst ulp sin %lld cos %lld atan2 %lld\n",
                 (long long)worst_sin, (long long)worst_cos,
                 (long long)worst_atan);
    return EXIT_SUCCESS;
}
