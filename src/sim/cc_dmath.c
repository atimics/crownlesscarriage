#include "sim/cc_dmath.h"

#include <math.h>
#include <stdint.h>

/* Clang honours this pragma; GCC ignores it and relies on the
   -ffp-contract=off flag that CMake sets for every target. */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

#define DMATH_PI 3.14159265358979311600e+00
#define DMATH_PI_2 1.57079632679489655800e+00
/* pi/2 split so that k * PIO2_1 and k * PIO2_2 are exact for |k| < 2^20. */
#define PIO2_1 1.57079632673412561417e+00
#define PIO2_2 6.07710050630396597660e-11
#define PIO2_3 2.02226624879595063154e-21

/* Taylor series on |r| <= pi/4. The first dropped term is below 1e-18. */
static double SinKernel(double r)
{
    double r2 = r * r;
    double p = 1.0 / 355687428096000.0;
    p = -1.0 / 1307674368000.0 + r2 * p;
    p = 1.0 / 6227020800.0 + r2 * p;
    p = -1.0 / 39916800.0 + r2 * p;
    p = 1.0 / 362880.0 + r2 * p;
    p = -1.0 / 5040.0 + r2 * p;
    p = 1.0 / 120.0 + r2 * p;
    p = -1.0 / 6.0 + r2 * p;
    return r + r * (r2 * p);
}

static double CosKernel(double r)
{
    double r2 = r * r;
    double p = 1.0 / 20922789888000.0;
    p = -1.0 / 87178291200.0 + r2 * p;
    p = 1.0 / 479001600.0 + r2 * p;
    p = -1.0 / 3628800.0 + r2 * p;
    p = 1.0 / 40320.0 + r2 * p;
    p = -1.0 / 720.0 + r2 * p;
    p = 1.0 / 24.0 + r2 * p;
    p = -0.5 + r2 * p;
    return 1.0 + r2 * p;
}

/* x = k * pi/2 + r with |r| <= about pi/4. Returns k mod 4. */
static int32_t Reduce(double x, double *r)
{
    double q = x * (2.0 / DMATH_PI);
    double k = q >= 0.0 ? floor(q + 0.5) : ceil(q - 0.5);
    *r = ((x - k * PIO2_1) - k * PIO2_2) - k * PIO2_3;
    int64_t whole = (int64_t)k;
    return (int32_t)(whole & 3);
}

float CcDmathSinf(float x)
{
    double r = 0.0;
    switch (Reduce((double)x, &r)) {
        case 0: return (float)SinKernel(r);
        case 1: return (float)CosKernel(r);
        case 2: return (float)-SinKernel(r);
        default: return (float)-CosKernel(r);
    }
}

float CcDmathCosf(float x)
{
    double r = 0.0;
    switch (Reduce((double)x, &r)) {
        case 0: return (float)CosKernel(r);
        case 1: return (float)-SinKernel(r);
        case 2: return (float)-CosKernel(r);
        default: return (float)SinKernel(r);
    }
}

/* atan on [0, 1]. Two half-angle steps, atan(t) = 2 atan(t / (1 +
   sqrt(1 + t^2))), bring t below tan(pi/16) ~ 0.199, where the odd Taylor
   series to t^23 leaves an error below 1e-18. */
static double AtanUnit(double t)
{
    t = t / (1.0 + sqrt(1.0 + t * t));
    t = t / (1.0 + sqrt(1.0 + t * t));
    double t2 = t * t;
    double q = 1.0 / 23.0;
    q = 1.0 / 21.0 - t2 * q;
    q = 1.0 / 19.0 - t2 * q;
    q = 1.0 / 17.0 - t2 * q;
    q = 1.0 / 15.0 - t2 * q;
    q = 1.0 / 13.0 - t2 * q;
    q = 1.0 / 11.0 - t2 * q;
    q = 1.0 / 9.0 - t2 * q;
    q = 1.0 / 7.0 - t2 * q;
    q = 1.0 / 5.0 - t2 * q;
    q = 1.0 / 3.0 - t2 * q;
    return 4.0 * (t - t * (t2 * q));
}

float CcDmathAtan2f(float y, float x)
{
    double ay = fabs((double)y);
    double ax = fabs((double)x);
    double angle = 0.0;
    if (ax == 0.0 && ay == 0.0) {
        angle = 0.0;
    } else if (ay <= ax) {
        angle = AtanUnit(ay / ax);
    } else {
        angle = DMATH_PI_2 - AtanUnit(ax / ay);
    }
    if (signbit(x)) angle = DMATH_PI - angle;
    if (signbit(y)) angle = -angle;
    return (float)angle;
}

/* 2^x = 2^n * e^(f ln 2) with n = floor(x) and 0 <= f < 1. The Taylor
   series of e^u for u < ln 2 to u^20 leaves an error below 1e-22. */
double CcDmathExp2(double x)
{
    if (x > 1024.0) return HUGE_VAL;
    if (x < -1080.0) return 0.0;
    double whole = floor(x);
    double u = (x - whole) * 0.69314718055994528623;
    double p = 1.0;
    for (int32_t term = 20; term >= 1; --term) {
        p = 1.0 + u * p / (double)term;
    }
    return ldexp(p, (int)whole);
}

float CcDmathSqrtf(float x)
{
    return sqrtf(x);
}

/* With FMA contraction a * b + c is fused: (1 + e)(1 - e) - 1 gives -e^2
   instead of the correctly rounded 0. The volatile loads stop constant
   folding. */
bool CcDmathContractionIsOff(void)
{
    volatile double a = 1.0 + 0x1p-30;
    volatile double b = 1.0 - 0x1p-30;
    volatile double c = -1.0;
    double x = a;
    double y = b;
    double z = c;
    return x * y + z == 0.0;
}
