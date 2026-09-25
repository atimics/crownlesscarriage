#ifndef CC_WANTS_TYPES_H
#define CC_WANTS_TYPES_H
#include <stdint.h>

#define CC_PERSONAL_WANTS 32
#define CC_BELONGINGS 16
#define CC_BELONGINGS_CARRIED 4
#define CC_WANTS_WIRE_CAPACITY 8192

typedef enum CcWantKind {
    CC_WANT_NONE, CC_WANT_MEAL, CC_WANT_WORK_SUPPLIES,
    CC_WANT_RECOVER, CC_WANT_REPAIR, CC_WANT_REPAIR_IRON
} CcWantKind;
typedef enum CcWantStatus {
    CC_WANT_ACTIVE = 1, CC_WANT_FULFILLED, CC_WANT_SETTLED, CC_WANT_CLOSED
} CcWantStatus;
typedef enum CcWantAction {
    CC_WANT_LEARN = 1, CC_WANT_TAKE, CC_WANT_GIVE, CC_WANT_LEAVE
} CcWantAction;

typedef struct CcPersonalWant {
    uint64_t id, person_id, item_id, parent_id, source_place_id, home_id;
    uint64_t cause_event_id, outcome_event_id;
    int64_t escrow;
    int32_t kind, status, good, quantity, created_day, known_day, settled_day, revision;
} CcPersonalWant;

/* Physical placement, ownership, condition and transfer revisions live in
 * custody. These records add an everyday object's name and material content. */
typedef struct CcBelonging {
    uint64_t id, custody_id, home_id, last_place_id, cause_event_id;
    int32_t repair_iron, last_wear_day;
    char name[48];
} CcBelonging;

typedef struct CcWantsState {
    CcPersonalWant wants[CC_PERSONAL_WANTS];
    CcBelonging items[CC_BELONGINGS];
    int32_t initialized, last_day;
} CcWantsState;
#endif
