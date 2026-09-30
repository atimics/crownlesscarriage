#ifndef CC_POLICY_H
#define CC_POLICY_H

#include "sim/cc_sim.h"

/* Optional decision hook. At a few choice points the simulation builds its usual
 * list of legal options, works out the option its own rule would take, and asks
 * the hook to choose. With no hook installed the rule's option is returned, so
 * behaviour, the random stream and the state hash are unchanged. The hook is
 * process-wide and is not part of CcSim, so it is never saved or hashed. It sees
 * the world read-only and can only pick among options the rule already allowed. */
typedef enum CcPolicyKind {
    CC_POLICY_TRAVEL_DESTINATION, /* options: neighbouring towns, then "stay" (target = here) */
    CC_POLICY_MEAL,               /* 0 = buy the cheapest meal (value = cost), 1 = go without */
    CC_POLICY_LODGING,            /* 0 = pay for the inn (value = cost), 1 = sleep rough */
    CC_POLICY_BANDIT_JOIN,        /* 0 = join the camp (target = camp), 1 = hold out */
    CC_POLICY_KIND_COUNT
} CcPolicyKind;

typedef struct CcPolicyOption {
    CcId target_id;   /* the town, camp or place the option concerns */
    int32_t value;    /* days, crowns or another cost, as documented per kind */
} CcPolicyOption;

typedef int32_t (*CcPolicyFn)(void *user, const CcSim *sim, CcPolicyKind kind, CcId actor_id,
                              const CcPolicyOption *options, int32_t count, int32_t fallback);

void CcSimSetPolicy(CcPolicyFn fn, void *user);
/* Returns the chosen option index; an index the hook returns outside [0, count) is
 * ignored in favour of `fallback`. */
int32_t CcSimPolicyChoose(const CcSim *sim, CcPolicyKind kind, CcId actor_id,
                          const CcPolicyOption *options, int32_t count, int32_t fallback);

#endif
