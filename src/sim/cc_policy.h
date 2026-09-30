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
    CC_POLICY_GOSSIP_SHARE,       /* 0 = share the story with this town (target = story event, value = the
                                     carried version's confidence), 1 = withhold it for now */
    CC_POLICY_TRADE_ROUTE,        /* a carriage's next cargo: one option per legal (good, source, destination);
                                     target = destination, value = the rule's score for it */
    CC_POLICY_RAID_TARGET,        /* which nearby town an idle bandit band scouts; target = town, value = the
                                     rule's score (stock minus twice the security) */
    CC_POLICY_KINGDOM_RELIEF,     /* which hungry town of a kingdom gets 28 crowns of grain relief, or none:
                                     one option per town with hunger >= 38 (target = town, value = its hunger), then
                                     "hold" as the last option. The actor is the kingdom's id. */
    CC_POLICY_DRAGON_TARGET,      /* which town the dragon burns for a theft it is owed for: one option per
                                     town other than its lair (target = town, value = the rule's "richness" score) */
    CC_POLICY_RAID_LAUNCH,        /* whether an eligible band raids the chosen town now: 0 = raid (target = town,
                                     value = its largest stock), 1 = hold. The actor is the band's id. */
    CC_POLICY_KIND_COUNT
} CcPolicyKind;

typedef struct CcPolicyOption {
    CcId target_id;   /* the town, camp or place the option concerns */
    int32_t value;    /* days, crowns or another cost, as documented per kind */
    /* Extra detail for kinds that need it (zero otherwise). */
    CcId source_id;
    int32_t good, need, surplus, urgent, path_cost;
} CcPolicyOption;

/* An option with only a target and a value; the other fields stay zero. */
#define CC_POLICY_OPTION(target, val) ((CcPolicyOption){.target_id = (target), .value = (val)})

typedef int32_t (*CcPolicyFn)(void *user, const CcSim *sim, CcPolicyKind kind, CcId actor_id,
                              const CcPolicyOption *options, int32_t count, int32_t fallback);

void CcSimSetPolicy(CcPolicyFn fn, void *user);
/* Returns the chosen option index; an index the hook returns outside [0, count) is
 * ignored in favour of `fallback`. */
int32_t CcSimPolicyChoose(const CcSim *sim, CcPolicyKind kind, CcId actor_id,
                          const CcPolicyOption *options, int32_t count, int32_t fallback);

#endif
