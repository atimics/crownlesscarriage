#ifndef CROWNLESS_COOP_H
#define CROWNLESS_COOP_H

#include "sim/cc_sim.h"

#if defined(_WIN32)
#define CC_COOP_API __declspec(dllexport)
#else
#define CC_COOP_API
#endif

#define CC_COOP_PROTOCOL_VERSION 1
#define CC_COOP_JSON_CAPACITY 131072

CC_COOP_API CcSim *CcCoopCreate(uint32_t seed);
CC_COOP_API void CcCoopDestroy(CcSim *sim);
CC_COOP_API bool CcCoopApply(CcSim *sim, const char *action, CcId target,
                            int32_t good, int32_t amount,
                            char *error, size_t capacity);
CC_COOP_API bool CcCoopAdvance(CcSim *sim, int32_t ticks,
                              char *error, size_t capacity);
CC_COOP_API bool CcCoopSnapshot(const CcSim *sim, char *json, size_t capacity);
CC_COOP_API bool CcCoopEncode(const CcSim *sim, unsigned char **bytes,
                             size_t *length, char *error, size_t capacity);
CC_COOP_API bool CcCoopDecode(CcSim *sim, const unsigned char *bytes,
                             size_t length, char *error, size_t capacity);
CC_COOP_API void CcCoopFree(void *bytes);

#endif
