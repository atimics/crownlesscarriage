#ifndef CROWNLESS_LEGACY_RUNTIME_INTERNAL_H
#define CROWNLESS_LEGACY_RUNTIME_INTERNAL_H

#include "sim/cc_sim.h"

/* Called after the loader verifies the old snapshot and replays its journal. */
bool CcSaveUpgradeLegacyRuntime(CcSim *sim, char *error, size_t error_capacity);

#endif
