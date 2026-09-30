#include "sim/cc_policy.h"

static CcPolicyFn policy_fn;
static void *policy_user;

void CcSimSetPolicy(CcPolicyFn fn, void *user)
{
    policy_fn = fn;
    policy_user = user;
}

int32_t CcSimPolicyChoose(const CcSim *sim, CcPolicyKind kind, CcId actor_id,
                          const CcPolicyOption *options, int32_t count, int32_t fallback)
{
    if (policy_fn == NULL || count <= 0 || fallback < 0 || fallback >= count) return fallback;
    int32_t pick = policy_fn(policy_user, sim, kind, actor_id, options, count, fallback);
    return pick >= 0 && pick < count ? pick : fallback;
}
