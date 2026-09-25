#ifndef CC_SHARED_CLOCK_H
#define CC_SHARED_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define CC_SHARED_CLOCK_MAX_COMPANIES 4

typedef enum {
    CC_SHARED_ACTIVITY_NONE = 0,
    CC_SHARED_ACTIVITY_WORK,
    CC_SHARED_ACTIVITY_REST,
    CC_SHARED_ACTIVITY_TRAVEL,
    CC_SHARED_ACTIVITY_WAIT
} CcSharedActivity;

typedef struct {
    uint64_t company_id;
    CcSharedActivity activity;
    int64_t remaining_seconds;
    bool ready;
} CcSharedCompany;

/* One server-owned clock. An active booking keeps its remaining time when
   another company reaches a decision. */
typedef struct {
    int64_t world_second;
    int32_t company_count;
    CcSharedCompany companies[CC_SHARED_CLOCK_MAX_COMPANIES];
} CcSharedClock;

typedef enum {
    CC_SHARED_STEP_INVALID = 0,
    CC_SHARED_STEP_WAITING,
    CC_SHARED_STEP_ADVANCED
} CcSharedStepState;

typedef struct {
    CcSharedStepState state;
    int64_t advanced_seconds;
    int32_t waiting_count;
    int32_t completed_count;
    uint64_t completed_ids[CC_SHARED_CLOCK_MAX_COMPANIES];
} CcSharedStep;

bool CcSharedClockAddCompany(CcSharedClock *clock, uint64_t company_id);
bool CcSharedClockBook(CcSharedClock *clock, uint64_t company_id,
                       CcSharedActivity activity, int64_t duration_seconds);
CcSharedStep CcSharedClockAdvance(CcSharedClock *clock);
CcSharedStep CcSharedClockTick(CcSharedClock *clock, int64_t seconds);
bool CcSharedClockValid(const CcSharedClock *clock);

#endif
