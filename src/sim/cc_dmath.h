#ifndef CROWNLESS_DMATH_H
#define CROWNLESS_DMATH_H

/* Deterministic math for the simulation.

   Everything that feeds the simulation state (and so its hash) must give
   the same bits on macOS arm64, Linux x86_64 and WebAssembly. The system
   libm does not promise that: Apple libm, glibc and musl (emscripten) each
   have their own sinf, cosf and atan2f, and their results can differ in the
   last bit. These functions use only IEEE-754 double +, -, *, / and sqrt,
   which are correctly rounded everywhere, so they give the same bits on
   every platform.

   Two build rules make that hold, and CMake sets both for every target:
   - -ffp-contract=off, so the compiler never fuses a*b+c into one FMA
     (clang on arm64 does by default, GCC on x86_64 does not);
   - no -ffast-math.
   FLT_EVAL_METHOD must be 0 (no x87 excess precision); this is checked at
   compile time. CcDmathContractionIsOff() lets a test catch a build that
   lost the flag.

   Accuracy: sin, cos and atan2 are evaluated in double with an error far
   below one float ulp (about 1e-16 relative), then rounded once to float.
   The float result is the correctly rounded value except in rare cases
   where the true value lies within about 1e-16 of a float rounding
   boundary. Inputs must be finite. sin and cos reduce the argument with a
   three-part pi/2 and are accurate for |x| < 1e6; the simulation only
   passes headings of a few radians. */

#include <float.h>
#include <stdbool.h>

#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0
#error "The simulation needs FLT_EVAL_METHOD == 0 (SSE2, arm64 or wasm)."
#endif

float CcDmathSinf(float x);
float CcDmathCosf(float x);
float CcDmathAtan2f(float y, float x);
/* 2^x in double, for finite x. Relative error about 1e-16. */
double CcDmathExp2(double x);
/* sqrt is correctly rounded by IEEE-754 on every target; this wrapper keeps
   all simulation math in one place. */
float CcDmathSqrtf(float x);

/* True when this library was built without FMA contraction. */
bool CcDmathContractionIsOff(void);

#endif
