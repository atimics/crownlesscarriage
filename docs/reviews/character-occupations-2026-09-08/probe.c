#include "sim/cc_occupations.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static CcSim sim;
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    CcSimInit(&sim, (uint32_t)strtoul(argv[1], NULL, 0));
    sim.schema_version = (uint32_t)strtoul(argv[2], NULL, 0);
    uint64_t distinct_pairs = 0, pairs = 0, herd_accounts = 0, direct_herds = 0;
    for (int day = 0; day <= 40 * 365; ++day) {
        if (day % 365 == 0) printf("hash %d %016" PRIx64 "\n", day, CcSimHash(&sim));
        if (day % 7 == 0) {
            for (int i = 0; i < sim.character_count; ++i) {
                const CcCharacter *person = &sim.characters[i];
                const CcGossipCarrier *carrier = CcSimGossipCarrier(&sim, person->id);
                if (!carrier) continue;
                for (int j = i + 1; j < sim.character_count; ++j) {
                    if (person->current_settlement_id != sim.characters[j].current_settlement_id) continue;
                    const CcGossipCarrier *other = CcSimGossipCarrier(&sim, sim.characters[j].id);
                    if (other) { ++pairs; if (carrier->stories != other->stories) ++distinct_pairs; }
                }
                for (int j = 0; j < CC_MAX_GOSSIP; ++j) {
                    if (!(carrier->stories & (UINT32_C(1) << (uint32_t)j))) continue;
                    if (CcGossipTopicMatches(CC_GOSSIP_TOPIC_HERDS, sim.gossip[j].kind)) {
                        ++herd_accounts;
                        if (carrier->versions[j].retellings == 0) ++direct_herds;
                    }
                }
            }
        }
        if (day < 40 * 365) CcSimAdvanceDays(&sim, 1);
    }
    char error[256]; if (!CcSimValidate(&sim, error, sizeof(error))) { puts(error); return 1; }
    printf("totals pairs=%" PRIu64 " distinct=%" PRIu64 " herd_accounts=%" PRIu64 " direct_herds=%" PRIu64 " size=%zu\n", pairs, distinct_pairs, herd_accounts, direct_herds, sizeof(sim));
    return 0;
}
