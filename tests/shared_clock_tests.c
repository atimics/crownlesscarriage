#include "sim/cc_shared_clock.h"
#include "test_support.h"

#include <limits.h>
#include <string.h>

#define HOUR INT64_C(3600)

int main(void)
{
    CcSharedClock clock = {0};
    CC_CHECK(CcSharedClockValid(&clock));
    CC_CHECK(CcSharedClockAddCompany(&clock, 2));
    CC_CHECK(CcSharedClockAddCompany(&clock, 1));
    CC_CHECK(clock.companies[0].company_id == 1);
    CC_CHECK(clock.companies[1].company_id == 2);
    CcSharedClock unchanged = clock;
    CC_CHECK(!CcSharedClockAddCompany(&clock, 1));
    CC_CHECK(memcmp(&clock, &unchanged, sizeof(clock)) == 0);
    CC_CHECK(CcSharedClockBook(&clock, 2, CC_SHARED_ACTIVITY_TRAVEL,
                              48 * HOUR));
    CcSharedStep step = CcSharedClockAdvance(&clock);
    CC_CHECK(step.state == CC_SHARED_STEP_WAITING);
    CC_CHECK(step.waiting_count == 1);
    CC_CHECK(clock.world_second == 0);
    CC_CHECK(CcSharedClockBook(&clock, 1, CC_SHARED_ACTIVITY_WORK,
                              6 * HOUR));
    CC_CHECK(!CcSharedClockBook(&clock, 1, CC_SHARED_ACTIVITY_WAIT, HOUR));
    step = CcSharedClockAdvance(&clock);
    CC_CHECK(step.state == CC_SHARED_STEP_ADVANCED);
    CC_CHECK(step.advanced_seconds == 6 * HOUR);
    CC_CHECK(step.completed_count == 1 && step.completed_ids[0] == 1);
    CC_CHECK(clock.companies[1].remaining_seconds == 42 * HOUR);

    /* One company explores at normal speed while the other keeps travelling. */
    step = CcSharedClockTick(&clock, 30);
    CC_CHECK(step.advanced_seconds == 30 && step.completed_count == 0);
    CC_CHECK(clock.companies[1].remaining_seconds == 42 * HOUR - 30);
    CcSharedClock checkpoint = clock;

    const int64_t local_acts[] = {
        8 * HOUR, 6 * HOUR, 8 * HOUR, 6 * HOUR, 8 * HOUR, 6 * HOUR - 30
    };
    for (int32_t i = 0; i < 6; ++i) {
        CcSharedActivity activity = i % 2 == 0 ?
            CC_SHARED_ACTIVITY_REST : CC_SHARED_ACTIVITY_WORK;
        CC_CHECK(CcSharedClockBook(&clock, 1, activity, local_acts[i]));
        step = CcSharedClockAdvance(&clock);
        CC_CHECK(step.state == CC_SHARED_STEP_ADVANCED);
        CC_CHECK(step.advanced_seconds == local_acts[i]);
        CC_CHECK(CcSharedClockValid(&clock));
        if (i < 5)
            CC_CHECK(step.completed_count == 1 &&
                     step.completed_ids[0] == 1);
    }
    CC_CHECK(clock.world_second == 48 * HOUR);
    CC_CHECK(step.completed_count == 2);
    CC_CHECK(step.completed_ids[0] == 1 && step.completed_ids[1] == 2);
    CC_CHECK(CcSharedClockAdvance(&clock).state == CC_SHARED_STEP_WAITING);
    for (int32_t i = 0; i < 6; ++i) {
        CcSharedActivity activity = i % 2 == 0 ?
            CC_SHARED_ACTIVITY_REST : CC_SHARED_ACTIVITY_WORK;
        CC_CHECK(CcSharedClockBook(&checkpoint, 1, activity, local_acts[i]));
        CC_CHECK(CcSharedClockAdvance(&checkpoint).state ==
                 CC_SHARED_STEP_ADVANCED);
    }
    CC_CHECK(memcmp(&checkpoint, &clock, sizeof(clock)) == 0);

    CcSharedClock crossing = {0};
    CC_CHECK(CcSharedClockAddCompany(&crossing, 3));
    CC_CHECK(CcSharedClockBook(&crossing, 3, CC_SHARED_ACTIVITY_TRAVEL, 10));
    step = CcSharedClockTick(&crossing, 100);
    CC_CHECK(step.advanced_seconds == 10 && step.completed_ids[0] == 3);
    CC_CHECK(crossing.world_second == 10);
    CC_CHECK(CcSharedClockAddCompany(&crossing, 2));
    CC_CHECK(crossing.companies[0].company_id == 2);
    CC_CHECK(crossing.companies[1].company_id == 3);
    CC_CHECK(CcSharedClockAdvance(&crossing).waiting_count == 2);
    crossing.world_second = INT64_MAX - 5;
    CC_CHECK(!CcSharedClockBook(&crossing, 3, CC_SHARED_ACTIVITY_WAIT, 6));
    CC_CHECK(CcSharedClockBook(&crossing, 3, CC_SHARED_ACTIVITY_WAIT, 5));
    unchanged = crossing;
    CC_CHECK(CcSharedClockTick(&crossing, 6).state ==
             CC_SHARED_STEP_INVALID);
    CC_CHECK(memcmp(&crossing, &unchanged, sizeof(crossing)) == 0);
    CC_CHECK(CcSharedClockTick(&crossing, 5).state ==
             CC_SHARED_STEP_ADVANCED);
    CC_CHECK(crossing.world_second == INT64_MAX);
    CC_CHECK(CcSharedClockValid(&crossing));
    return 0;
}
