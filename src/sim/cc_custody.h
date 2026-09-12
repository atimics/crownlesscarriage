#ifndef CC_CUSTODY_H
#define CC_CUSTODY_H

#include <stdbool.h>
#include <stdint.h>

#define CC_CUSTODY_CAPACITY 96
#define CC_CUSTODY_MANIFEST_CAPACITY 16

typedef enum {
    CC_CUSTODY_STORE, CC_CUSTODY_SITE, CC_CUSTODY_CARRIER,
    CC_CUSTODY_PLAYER, CC_CUSTODY_CHARACTER, CC_CUSTODY_CAPTOR,
    CC_CUSTODY_CONTAINER_HOLDER
} CcCustodyHolderKind;

typedef struct {
    CcCustodyHolderKind kind;
    uint64_t id;
} CcCustodyHolder;

typedef enum {
    CC_CUSTODY_GOODS, CC_CUSTODY_PURSE, CC_CUSTODY_TREASURE,
    CC_CUSTODY_DOCUMENT, CC_CUSTODY_CONTAINER
} CcCustodyKind;

typedef struct {
    uint64_t id, revision, owner_id, source_id, last_event_id;
    CcCustodyHolder holder;
    CcCustodyKind kind;
    uint64_t reference_id; /* Existing treasure or document work identity. */
    int64_t quantity;
    int32_t good, condition, capacity;
    bool active;
} CcCustodyEntry;

typedef struct {
    uint64_t next_id;
    CcCustodyEntry entries[CC_CUSTODY_CAPACITY];
} CcCustodyState;

typedef struct {
    uint64_t place_id, route_id;
    int32_t progress_milli;
} CcCustodyLocation;

typedef struct {
    void *context;
    bool (*resolve)(void *, CcCustodyHolder, CcCustodyLocation *, int64_t *capacity);
    bool (*permit)(void *, uint64_t actor_id, const CcCustodyEntry *, CcCustodyHolder);
} CcCustodyRules;

typedef struct {
    uint64_t entry_id, revision, actor_id, event_id;
    CcCustodyHolder destination;
    int64_t quantity;
} CcCustodyTransfer;

typedef enum {
    CC_CUSTODY_READY, CC_CUSTODY_INVALID, CC_CUSTODY_STALE,
    CC_CUSTODY_FORBIDDEN, CC_CUSTODY_REMOTE, CC_CUSTODY_FULL
} CcCustodyResult;

/* Game adapters supply physical placement and permission from current state. */
void CcCustodyInit(CcCustodyState *state);
const CcCustodyEntry *CcCustodyFind(const CcCustodyState *state, uint64_t id);
CcCustodyResult CcCustodyPlanTransfer(const CcCustodyState *state,
    const CcCustodyRules *rules, const CcCustodyTransfer *transfer);
CcCustodyResult CcCustodyApplyTransfer(CcCustodyState *state,
    const CcCustodyRules *rules, const CcCustodyTransfer *transfer,
    uint64_t *result_id);

#endif
