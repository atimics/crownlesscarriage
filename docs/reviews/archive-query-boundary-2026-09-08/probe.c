#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static CcSim sim, before;
static int Emit(uint32_t schema, int scenario)
{
    before = sim;
    CcMaterialChainSnapshot q = CcSimMaterialChainSnapshot(&sim);
    if (memcmp(&before, &sim, sizeof(sim)) != 0) return 1;
    printf("%u %d %" PRIu64 " %s %d %d %d %d %d %d %d %d %d\n", schema, scenario,
        q.scriptorium_id, CcMaterialChainBlockerName(q.blocker), q.scribes, q.wheat,
        q.paper, q.tools, q.iron, q.gold, q.gems, q.incoming_tools, q.incoming_iron);
    return 0;
}
int main(void)
{
    const uint32_t schemas[] = {33, 37, 57, 58, 73};
    const uint32_t seeds[] = {42, UINT32_C(0x5eed0001)};
    for (unsigned seed = 0; seed < 2; ++seed) {
        for (unsigned version = 0; version < 5; ++version) {
            for (int scenario = 0; scenario < 8; ++scenario) {
                CcSimInit(&sim, seeds[seed]);
                sim.schema_version = schemas[version];
                sim.archives.scribes = 1;
                for (int32_t i = 0; i < sim.settlement_count; ++i) {
                    sim.settlements[i].stock[CC_GOOD_GOLD] = 20;
                    sim.settlements[i].stock[CC_GOOD_GEMS] = 20;
                }
                CcId seat = CcSimMaterialChainSnapshot(&sim).scriptorium_id;
                CcSettlement *town = CcSimSettlementMutable(&sim, seat);
                if (town == NULL) return 2;
                town->stock[CC_GOOD_WHEAT] = 10000;
                town->stock[CC_GOOD_PAPER] = 20;
                town->stock[CC_GOOD_TOOLS] = 20;
                if (scenario == 1) for (int32_t i = 0; i < sim.settlement_count; ++i) sim.settlements[i].population = 0;
                if (scenario == 2) strcpy(town->name, "Changed seat name");
                if (scenario == 3) sim.archives.scribes = 0;
                if (scenario == 4) for (int32_t i = 0; i < sim.settlement_count; ++i) {
                    sim.settlements[i].stock[CC_GOOD_GOLD] = 0;
                    sim.settlements[i].stock[CC_GOOD_GEMS] = 0;
                }
                if (scenario == 5) town->stock[CC_GOOD_TOOLS] = 0;
                if (scenario == 6) memset(town->stock, 0, sizeof(town->stock));
                if (scenario == 6) { town->stock[CC_GOOD_TOOLS] = 20; town->stock[CC_GOOD_PAPER] = 20; }
                if (scenario == 7) town->stock[CC_GOOD_PAPER] = 0;
                if (Emit(schemas[version], scenario) != 0) return 3;
            }
        }
    }
    return CcSimMaterialChainSnapshot(NULL).blocker == CC_MATERIAL_CHAIN_NO_SCRIBES ? 0 : 4;
}
