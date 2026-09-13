#ifndef CROWNLESS_ARCHIVE_INTERNAL_H
#define CROWNLESS_ARCHIVE_INTERNAL_H

#include "sim/cc_sim.h"

/* Current seat and grain rules shared by inspection and archive work. */
const CcSettlement *CcArchiveSeat(const CcSim *sim);
/* The simulation is required; an unavailable settlement yields zero grain. */
int32_t CcArchiveSpareGrain(const CcSim *sim, const CcSettlement *place);

/* Remember the established seat and its continuous material failure period. */
void CcArchiveRememberSeat(CcSim *sim);

/* Commit a fresh booking through the shared carriage and shipment pool. */
bool CcArchiveDispatchSupply(CcSim *sim, CcId carriage_id);

/* Scribes draw their payroll from the iron ledger. Supply dispatch may only
   spend the surplus above this full-payroll threshold, so freight bookings
   never silence the scriptorium. */
#define CC_ARCHIVE_PAYROLL_FLOOR ((CcMoney)300)

#endif
