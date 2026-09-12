#ifndef CC_EVENT_PIN_SET_INTERNAL_H
#define CC_EVENT_PIN_SET_INTERNAL_H
#include "sim/cc_sim.h"

#define CC_EVENT_PIN_SET_SIZE 4096U
_Static_assert((CC_EVENT_PIN_SET_SIZE & (CC_EVENT_PIN_SET_SIZE - 1U)) == 0U,
               "Event pin capacity must be a power of two.");

typedef struct CcEventPinSet {
    CcId slots[CC_EVENT_PIN_SET_SIZE];
    bool overflow;
    CcId query_id;
    bool query_found;
} CcEventPinSet;

static inline uint32_t EventPinHash(CcId id)
{
    uint64_t value = (uint64_t)id;
    value ^= value >> 33;
    value *= UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33;
    return (uint32_t)value;
}

static inline void PinEvent(CcEventPinSet *set, CcId id)
{
    if (id == 0U) return;
    if (set->query_id != 0U) {
        if (id == set->query_id) set->query_found = true;
        return;
    }
    uint32_t index = EventPinHash(id) & (CC_EVENT_PIN_SET_SIZE - 1U);
    for (uint32_t probes = 0; probes < CC_EVENT_PIN_SET_SIZE; ++probes) {
        if (set->slots[index] == 0U) { set->slots[index] = id; return; }
        if (set->slots[index] == id) return;
        index = (index + 1U) & (CC_EVENT_PIN_SET_SIZE - 1U);
    }
    set->overflow = true;
}

static inline bool EventIsPinned(const CcEventPinSet *set, CcId id)
{
    if (id == 0U) return false;
    uint32_t index = EventPinHash(id) & (CC_EVENT_PIN_SET_SIZE - 1U);
    for (uint32_t probes = 0; probes < CC_EVENT_PIN_SET_SIZE; ++probes) {
        if (set->slots[index] == 0U) return false;
        if (set->slots[index] == id) return true;
        index = (index + 1U) & (CC_EVENT_PIN_SET_SIZE - 1U);
    }
    return false;
}

#endif
