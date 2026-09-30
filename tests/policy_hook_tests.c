/* The decision hook must not change the simulation unless it chooses differently. */
#include "sim/cc_policy.h"

#include <stdio.h>
#include <stdlib.h>

#define DAYS 500
#define SEEDS 4

static CcSim sim;
static long calls[CC_POLICY_KIND_COUNT];

static int32_t Fallback(void *user, const CcSim *world, CcPolicyKind kind, CcId actor,
                        const CcPolicyOption *options, int32_t count, int32_t fallback)
{
    (void)user; (void)world; (void)actor; (void)options; (void)count;
    calls[kind] += 1;
    return fallback;
}
static int32_t Last(void *user, const CcSim *world, CcPolicyKind kind, CcId actor,
                    const CcPolicyOption *options, int32_t count, int32_t fallback)
{
    (void)user; (void)world; (void)kind; (void)actor; (void)options; (void)fallback;
    return count - 1;
}
static int32_t Outside(void *user, const CcSim *world, CcPolicyKind kind, CcId actor,
                       const CcPolicyOption *options, int32_t count, int32_t fallback)
{
    (void)user; (void)world; (void)kind; (void)actor; (void)options; (void)fallback;
    return count + 7;
}

static uint64_t Run(uint32_t seed, CcPolicyFn fn, int *valid)
{
    char error[256];
    CcSimSetPolicy(fn, NULL);
    CcSimInit(&sim, seed);
    CcSimAdvanceDays(&sim, DAYS);
    *valid = CcSimValidate(&sim, error, sizeof(error));
    uint64_t hash = CcSimHash(&sim);
    CcSimSetPolicy(NULL, NULL);
    return hash;
}

int main(void)
{
    int failures = 0, changed = 0;
    for (uint32_t seed = 1; seed <= SEEDS; ++seed) {
        int ok;
        uint64_t plain = Run(seed, NULL, &ok);
        if (!ok) { printf("seed %u: plain world invalid\n", seed); ++failures; }
        if (Run(seed, Fallback, &ok) != plain || !ok) { printf("seed %u: fallback hook changed the world\n", seed); ++failures; }
        if (Run(seed, Outside, &ok) != plain || !ok) { printf("seed %u: out-of-range answer was not ignored\n", seed); ++failures; }
        uint64_t other = Run(seed, Last, &ok);
        if (!ok) { printf("seed %u: a different choice left an invalid world\n", seed); ++failures; }
        changed += other != plain;
    }
    if (calls[CC_POLICY_TRAVEL_DESTINATION] == 0) { puts("the travel decision was never offered"); ++failures; }
    if (calls[CC_POLICY_MEAL] == 0) { puts("the meal decision was never offered"); ++failures; }
    if (changed == 0) { puts("choosing differently never changed a world"); ++failures; }
    printf("offered: travel %ld, meal %ld, lodging %ld, bandit %ld; %d of %d worlds changed\n",
           calls[CC_POLICY_TRAVEL_DESTINATION], calls[CC_POLICY_MEAL], calls[CC_POLICY_LODGING],
           calls[CC_POLICY_BANDIT_JOIN], changed, SEEDS);
    puts(failures ? "FAIL" : "ok");
    return failures ? 1 : 0;
}
