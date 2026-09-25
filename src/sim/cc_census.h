#ifndef CROWNLESS_CENSUS_H
#define CROWNLESS_CENSUS_H

#include "sim/cc_sim.h"

/* The census uses the same identity for a resident and their rich character. */
void CcCensusInit(CcSim *sim);
void CcCensusReconcile(CcSim *sim);
CcId CcCensusClaimResident(CcSim *sim, CcId settlement_id);
const CcCensusResident *CcCensusResidentById(const CcSim *sim, CcId id);
int32_t CcCensusPopulation(const CcSim *sim, CcId settlement_id);
int32_t CcCensusDistrictPopulation(const CcSim *sim, CcId district_id);
bool CcCensusValidate(const CcSim *sim);
uint64_t CcCensusHash(const CcCensus *census);
uint64_t CcCensusIssuedIdCount(const CcCensus *census);
size_t CcCensusEncodedSize(const CcCensus *census);
size_t CcCensusEncode(const CcCensus *census, uint8_t *bytes, size_t capacity);
bool CcCensusDecode(CcCensus *census, const uint8_t *bytes, size_t length);

#endif
