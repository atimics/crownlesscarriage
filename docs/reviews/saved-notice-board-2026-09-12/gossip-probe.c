#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static CcSim sim;
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    unsigned seed = (unsigned)strtoul(argv[1], NULL, 0);
    int days = atoi(argv[2]);
    CcId prior[CC_MAX_GOSSIP] = {0};
    int lived[CC_MAX_GOSSIP] = {0};
    uint64_t notice_days=0, account_days=0, town_days=0, versions=0;
    uint64_t retellings=0, low=0, expired=0, lifetime=0;
    int max_retellings=0;
    CcSimInit(&sim, seed);
    for (int day=0; day<days; ++day) {
        CcSimAdvanceDays(&sim, 1);
        for (int i=0; i<CC_MAX_GOSSIP; ++i) {
            const CcGossip *g=&sim.gossip[i];
            if (prior[i] && prior[i]!=g->event_id) { ++expired; lifetime+=(uint64_t)lived[i]; lived[i]=0; }
            prior[i] = g->kind==CC_EVENT_NOTICE_POSTED ? 0 : g->event_id;
            if (!g->event_id) continue;
            if (g->kind==CC_EVENT_NOTICE_POSTED) { ++notice_days; continue; }
            ++lived[i]; ++account_days;
            for (int town=0; town<sim.settlement_count; ++town) {
                if (!(g->settlement_mask & (UINT32_C(1)<<(unsigned)town))) continue;
                ++town_days; ++versions;
                const CcGossipVersion *v=&g->local[town];
                retellings+=(uint64_t)v->retellings;
                if(v->retellings>max_retellings)max_retellings=v->retellings;
                if(v->confidence<40)++low;
            }
        }
        char error[256];
        if (!CcSimValidate(&sim,error,sizeof(error))) { fprintf(stderr,"day %d: %s\n",day,error);return 1; }
    }
    printf("{\"seed\":%u,\"days\":%d,\"schema\":%u,\"notice_slot_days\":%" PRIu64 ",\"account_slot_days\":%" PRIu64 ",\"account_town_days\":%" PRIu64 ",\"town_version_days\":%" PRIu64 ",\"retelling_sum\":%" PRIu64 ",\"max_retellings\":%d,\"confidence_below_40\":%" PRIu64 ",\"evicted_accounts\":%" PRIu64 ",\"evicted_account_lifetime_sum\":%" PRIu64 "}\n",seed,days,sim.schema_version,notice_days,account_days,town_days,versions,retellings,max_retellings,low,expired,lifetime);
    return 0;
}
