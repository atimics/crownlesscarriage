#ifndef CC_ARCHIVE_STAFF_H
#define CC_ARCHIVE_STAFF_H
#include "sim/cc_sim.h"
bool CcSimArchiveStaffMember(const CcSim *sim, CcId person_id);
bool CcSimArchiveStaffWorking(const CcSim *sim, CcId person_id);
int32_t CcSimArchiveStaffSlots(const CcSim *sim);
int32_t CcSimArchiveStaffCount(const CcSim *sim);
void CcSimRefreshArchiveStaff(CcSim *sim);
bool CcSimArchiveStaffValid(const CcSim *sim);
#endif
