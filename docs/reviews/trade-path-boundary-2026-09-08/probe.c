#ifdef CC_PARENT_PROBE
#include "sim/cc_sim.c"
#define PATH_QUERY FindTradePath
#else
#include "sim/cc_trade_path_internal.h"
#define PATH_QUERY CcTradeFindPath
#endif
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static CcSim probe_sim, probe_before;
int main(void)
{
    const uint32_t seeds[] = {42, UINT32_C(0x5eed0001)};
    const uint32_t schemas[] = {72, 73};
    for (unsigned seed = 0; seed < 2; ++seed) for (unsigned schema = 0; schema < 2; ++schema) {
        for (int fixture = 0; fixture < 3; ++fixture) {
            CcSimInit(&probe_sim, seeds[seed]);
            probe_sim.schema_version = schemas[schema];
            for (int32_t r = 0; r < probe_sim.route_count; ++r) {
                if (fixture == 1) {
                    probe_sim.routes[r].closed = r % 2 == 0;
                    probe_sim.routes[r].condition = r % 2 == 0 ? 25 : 0;
                }
                if (fixture == 2) probe_sim.routes[r].closed = true;
            }
            probe_before = probe_sim;
            for (int owner = -1; owner < probe_sim.kingdom_count; ++owner)
            for (int borders = 0; borders < 2; ++borders)
            for (int ignore = 0; ignore < 2; ++ignore)
            for (int slots = 1; slots <= 3; slots += 2)
            for (int usage = 0; usage < 2; ++usage) {
                int32_t used[CC_MAX_ROUTES];
                for (int r = 0; r < CC_MAX_ROUTES; ++r) used[r] = usage ? CC_SIM_MAX_UNITS : 0;
                for (int from = 0; from < probe_sim.settlement_count; ++from)
                for (int to = 0; to < probe_sim.settlement_count; ++to) {
                    int32_t route = -7, cost = -7, capacity = -7;
                    CcId hop = UINT64_C(7);
                    bool found = PATH_QUERY(&probe_sim, probe_sim.settlements[from].id,
                        probe_sim.settlements[to].id, CC_GOOD_TOOLS, &route, &hop, &cost, &capacity,
                        usage ? used : NULL, borders != 0,
                        owner < 0 ? 0U : probe_sim.kingdoms[owner].id, ignore != 0, slots);
                    if (!found && (route != -7 || hop != 7U || cost != -7 || capacity != -7)) return 2;
                    printf("%d %d %" PRIu64 " %d %d\n", found ? 1 : 0, route, hop, cost, capacity);
                }
            }
            if (memcmp(&probe_before, &probe_sim, sizeof(probe_sim)) != 0) return 3;
        }
    }
    return 0;
}
