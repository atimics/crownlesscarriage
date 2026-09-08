#ifndef CROWNLESS_ARCHIVE_INTERNAL_H
#define CROWNLESS_ARCHIVE_INTERNAL_H

#include "sim/cc_sim.h"

/* Current seat and grain rules shared by inspection and archive work. */
const CcSettlement *CcArchiveSeat(const CcSim *sim);
/* The simulation is required; an unavailable settlement yields zero grain. */
int32_t CcArchiveSpareGrain(const CcSim *sim, const CcSettlement *place);

#endif
