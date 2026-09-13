#ifndef CROWNLESS_IDENTITY_INTERNAL_H
#define CROWNLESS_IDENTITY_INTERNAL_H

#include "sim/cc_sim.h"
#include <stddef.h>

#define CC_ID_SERIAL_MASK UINT64_C(0x00ffffffffffffff)

/* Called after CcSimValidate checks all collection counts. Reads IDs in the
 * established order, checks uniqueness, and verifies the next issued serial. */
bool CcIdentityValidate(const CcSim *sim, char *error, size_t error_capacity);

#endif
