#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <inttypes.h>
#include <stdio.h>

static CcSim scheduled, daily, restored;

typedef struct ReplayCheck {
    int32_t observed_days;
} ReplayCheck;

static void CheckDay(const CcSim *world, void *context)
{
    ReplayCheck *check = context;
    CcSimAdvanceDays(&daily, 1);
    ++check->observed_days;
    uint64_t expected = CcSimHash(&daily);
    uint64_t actual = CcSimHash(world);
    if (expected != actual)
        (void)fprintf(stderr, "day %d: daily=%" PRIu64 " scheduled=%" PRIu64 "\n",
                      world->current_day, expected, actual);
    CC_CHECK(world->current_day == daily.current_day);
    CC_CHECK(expected == actual);
}

int main(void)
{
    char error[256];
    const char *path = "scheduled-events.ccsave";
    CcSimInit(&scheduled, UINT32_C(0x9e3779b9));
    daily = scheduled;
    ReplayCheck check = {0};

    /* A dated story can reach a later person after an earlier person was checked. */
    CcSimAdvanceDaysObserved(&scheduled, 365, NULL, CheckDay, &check);
    CC_CHECK(check.observed_days == 365);
    CC_CHECK(CcSimValidate(&scheduled, error, sizeof(error)));
    CC_CHECK(CcSaveWrite(path, &scheduled, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&scheduled) == CcSimHash(&restored));
    scheduled = restored;

    CcSimAdvanceDaysObserved(&scheduled, 365, NULL, CheckDay, &check);
    CC_CHECK(check.observed_days == 730);
    CC_CHECK(CcSimValidate(&scheduled, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);
    return 0;
}
