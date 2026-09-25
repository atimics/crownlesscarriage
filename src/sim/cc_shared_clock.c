#include "sim/cc_shared_clock.h"

#include <limits.h>
#include <stddef.h>
bool CcSharedClockValid(const CcSharedClock *clock)
{
    if (clock == NULL || clock->world_second < 0 ||
        clock->company_count < 0 ||
        clock->company_count > CC_SHARED_CLOCK_MAX_COMPANIES) return false;
    uint64_t previous = 0;
    for (int32_t i = 0; i < clock->company_count; ++i) {
        const CcSharedCompany *company = &clock->companies[i];
        if (company->company_id <= previous ||
            company->activity < CC_SHARED_ACTIVITY_NONE ||
            company->activity > CC_SHARED_ACTIVITY_WAIT ||
            (company->ready &&
             (company->activity == CC_SHARED_ACTIVITY_NONE ||
              company->remaining_seconds <= 0 ||
              company->remaining_seconds > INT64_MAX - clock->world_second)) ||
            (!company->ready &&
             (company->activity != CC_SHARED_ACTIVITY_NONE ||
              company->remaining_seconds != 0))) return false;
        previous = company->company_id;
    }
    return true;
}

bool CcSharedClockAddCompany(CcSharedClock *clock, uint64_t company_id)
{
    if (!CcSharedClockValid(clock) || company_id == 0 ||
        clock->company_count >= CC_SHARED_CLOCK_MAX_COMPANIES ||
        clock->world_second != 0) return false;
    int32_t slot = 0;
    while (slot < clock->company_count &&
           clock->companies[slot].company_id < company_id) ++slot;
    if (slot < clock->company_count &&
        clock->companies[slot].company_id == company_id) return false;
    for (int32_t i = clock->company_count; i > slot; --i)
        clock->companies[i] = clock->companies[i - 1];
    clock->companies[slot] = (CcSharedCompany){.company_id = company_id};
    ++clock->company_count;
    return true;
}

bool CcSharedClockBook(CcSharedClock *clock, uint64_t company_id,
                       CcSharedActivity activity, int64_t duration_seconds)
{
    if (!CcSharedClockValid(clock) || activity <= CC_SHARED_ACTIVITY_NONE ||
        activity > CC_SHARED_ACTIVITY_WAIT || duration_seconds <= 0 ||
        duration_seconds > INT64_MAX - clock->world_second) return false;
    for (int32_t i = 0; i < clock->company_count; ++i) {
        CcSharedCompany *company = &clock->companies[i];
        if (company->company_id != company_id) continue;
        if (company->ready) return false;
        company->activity = activity;
        company->remaining_seconds = duration_seconds;
        company->ready = true;
        return true;
    }
    return false;
}

static CcSharedStep AdvanceBy(CcSharedClock *clock, int64_t seconds)
{
    CcSharedStep result = {.state = CC_SHARED_STEP_INVALID};
    if (!CcSharedClockValid(clock) || seconds <= 0 ||
        seconds > INT64_MAX - clock->world_second) return result;
    int64_t step = seconds;
    for (int32_t i = 0; i < clock->company_count; ++i) {
        const CcSharedCompany *company = &clock->companies[i];
        if (company->ready && company->remaining_seconds < step)
            step = company->remaining_seconds;
    }
    clock->world_second += step;
    result.state = CC_SHARED_STEP_ADVANCED;
    result.advanced_seconds = step;
    for (int32_t i = 0; i < clock->company_count; ++i) {
        CcSharedCompany *company = &clock->companies[i];
        if (!company->ready) continue;
        company->remaining_seconds -= step;
        if (company->remaining_seconds != 0) continue;
        result.completed_ids[result.completed_count++] = company->company_id;
        company->activity = CC_SHARED_ACTIVITY_NONE;
        company->ready = false;
    }
    return result;
}

CcSharedStep CcSharedClockAdvance(CcSharedClock *clock)
{
    CcSharedStep result = {.state = CC_SHARED_STEP_INVALID};
    if (!CcSharedClockValid(clock) || clock->company_count == 0) return result;
    int64_t earliest = INT64_MAX;
    for (int32_t i = 0; i < clock->company_count; ++i) {
        const CcSharedCompany *company = &clock->companies[i];
        if (!company->ready) {
            ++result.waiting_count;
        } else if (company->remaining_seconds < earliest) {
            earliest = company->remaining_seconds;
        }
    }
    if (result.waiting_count > 0) {
        result.state = CC_SHARED_STEP_WAITING;
        return result;
    }
    return AdvanceBy(clock, earliest);
}

CcSharedStep CcSharedClockTick(CcSharedClock *clock, int64_t seconds)
{
    return AdvanceBy(clock, seconds);
}
