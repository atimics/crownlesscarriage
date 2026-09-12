#ifndef CC_SIM_VERSIONS_INTERNAL_H
#define CC_SIM_VERSIONS_INTERNAL_H

#include "sim/cc_sim.h"

/* Review the newest saved schema when advancing the current schema. Both
   version admission and the runtime upgrade use this explicit boundary. */
#define CC_SIM_NEWEST_LEGACY_SCHEMA 98U
_Static_assert(CC_SIM_NEWEST_LEGACY_SCHEMA + 1U == CC_SIM_SCHEMA_VERSION,
    "Review the legacy schema boundary when advancing CC_SIM_SCHEMA_VERSION");

#endif
