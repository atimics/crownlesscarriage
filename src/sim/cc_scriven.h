#ifndef CC_SCRIVEN_H
#define CC_SCRIVEN_H
#include "sim/cc_sim.h"
#include "sim/cc_calendar.h"

typedef enum CcScrivenAction {
    CC_SCRIVEN_READ = 1, CC_SCRIVEN_BORROW, CC_SCRIVEN_RETURN,
    CC_SCRIVEN_OBSERVE, CC_SCRIVEN_HEARING, CC_SCRIVEN_COPY,
    CC_SCRIVEN_DELIVER, CC_SCRIVEN_SKY, CC_SCRIVEN_WAIT, CC_SCRIVEN_COMMISSION
} CcScrivenAction;
void CcScrivenInit(CcSim *sim);
void CcScrivenAdvance(CcSim *sim);
void CcScrivenAnchor(CcSim *sim);
void CcScrivenFreeze(CcSim *sim, CcId book_id);
void CcScrivenRebind(CcSim *sim, const int32_t slots[4], CcId new_id);
bool CcScrivenReserved(const CcSim *sim, CcId book_id);
bool CcScrivenTravelling(const CcSim *sim, CcId person_id);
bool CcScrivenCarries(const CcSim *sim, CcId book_id, CcId holder_id);
const CcScrivenBook *CcScrivenBookById(const CcSim *sim, CcId id);
bool CcScrivenBookAccessible(const CcSim *sim, CcId book_id, CcId place_id);
bool CcScrivenApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity);
bool CcScrivenValidate(const CcSim *sim);
void CcScrivenDescribe(const CcSim *sim, char *text, size_t capacity);
uint64_t CcScrivenHash(const CcScrivenState *state);
size_t CcScrivenEncode(const CcScrivenState *state, uint8_t *bytes, size_t capacity);
bool CcScrivenDecode(CcScrivenState *state, const uint8_t *bytes, size_t length);
#endif
