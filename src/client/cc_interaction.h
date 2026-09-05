#ifndef CROWNLESS_INTERACTION_H
#define CROWNLESS_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>

#define CC_INTERACTION_CAPACITY 64

typedef enum CcInteractionKind {
    CC_INTERACTION_NONE,
    CC_INTERACTION_PERSON,
    CC_INTERACTION_DOOR,
    CC_INTERACTION_COUNTER,
    CC_INTERACTION_BOARD,
    CC_INTERACTION_CARRIAGE,
    CC_INTERACTION_EXIT,
    CC_INTERACTION_SITE,
    CC_INTERACTION_PORTAL,
    CC_INTERACTION_ACTION
} CcInteractionKind;

typedef struct CcInteractionKey {
    uint64_t place;
    uint64_t object;
    CcInteractionKind kind;
} CcInteractionKey;

typedef struct CcInteractionTarget {
    CcInteractionKey key;
    uint64_t character_id;
    uint64_t situation_id;
    int32_t action;
    int32_t amount;
    float x, y, z;
    float width, height, depth;
    float approach_x, approach_z;
    float radius;
    float left, top, right, bottom;
    float camera_distance;
    bool visible;
    bool available;
    char name[64];
    char verb[32];
    char reason[112];
} CcInteractionTarget;

typedef struct CcInteractionPlan {
    CcInteractionTarget targets[CC_INTERACTION_CAPACITY];
    int32_t count;
} CcInteractionPlan;

typedef struct CcInteractionState {
    CcInteractionKey focus;
    CcInteractionKey pending;
    float repath_seconds;
    float elapsed_seconds;
    float stalled_seconds;
    float previous_x, previous_z;
    bool approaching;
    char feedback[112];
} CcInteractionState;

bool CcInteractionKeyEqual(CcInteractionKey a, CcInteractionKey b);
const CcInteractionTarget *CcInteractionFind(const CcInteractionPlan *plan,
                                           CcInteractionKey key);
const CcInteractionTarget *CcInteractionPick(const CcInteractionPlan *plan,
                                           float x, float y);
void CcInteractionCycle(CcInteractionState *state,
                        const CcInteractionPlan *plan, int direction);
bool CcInteractionStart(CcInteractionState *state,
                        const CcInteractionTarget *target, float x, float z);
void CcInteractionCancel(CcInteractionState *state, const char *reason);
/* Returns true once when the requested target is reached and still available. */
bool CcInteractionAdvance(CcInteractionState *state,
                          const CcInteractionPlan *plan,
                          float x, float z, float delta_time);
bool CcInteractionInRange(const CcInteractionTarget *target, float x, float z);

#endif
