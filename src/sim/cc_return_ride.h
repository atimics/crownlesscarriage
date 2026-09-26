#ifndef CROWNLESS_RETURN_RIDE_H
#define CROWNLESS_RETURN_RIDE_H

#include "sim/cc_sim.h"

#include <stddef.h>

/* Shared "ride to a town by the real roads" driver for The Return
   (docs/design/the-return.md). Both the review tool
   (tools/return_digest.c) and the --capture-return client capture use this
   to leave a town, cross the region, and come back, so a returning
   traveller's digest reflects a ride the sim actually produced. Every leg
   is an ordinary journey: pauses are passed through, forks take the leg
   toward the target, and encounters are skipped so a review ride is never
   at the mercy of combat. */

/* The settlement whose name matches, or 0 if none does. */
CcId CcReturnRideTownByName(const CcSim *sim, const char *name);

/* The shortest path by leg count from `from` to `to`, written into `path`
   (capacity CC_MAX_SETTLEMENTS). Ties go to the lower route slot. Returns
   the leg count, or 0 if there is no path (including from == to). */
int32_t CcReturnRideRoadPath(const CcSim *sim, CcId from, CcId to, CcId *path);

/* Ride the company from its current location to `destination` over the real
   roads, one leg at a time. Returns false and fills `error` if a leg could
   not be applied or the ride did not end at `destination`. */
bool CcReturnRideAlongPath(CcSim *sim, CcId destination, char *error,
                          size_t error_capacity);

#endif
