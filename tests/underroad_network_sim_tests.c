#include "metagame/cc_metagame.h"
#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static void RemoveSave(const char *path)
{
    char sidecar[256];
    (void)remove(path);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-wal", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-shm", path);
    (void)remove(sidecar);
}

static int32_t Connected(const CcUnderroadNetwork *network)
{
    int32_t parent[CC_MAX_UNDERROAD_NODES];
    for (int32_t i = 0; i < network->node_count; ++i) parent[i] = i;
    for (int32_t i = 0; i < network->road_count; ++i) {
        int32_t a = network->roads[i].from_node;
        int32_t b = network->roads[i].to_node;
        while (parent[a] != a) a = parent[a];
        while (parent[b] != b) b = parent[b];
        if (a != b) parent[a] = b;
    }
    int32_t root = parent[0];
    while (parent[root] != root) root = parent[root];
    for (int32_t i = 1; i < network->node_count; ++i) {
        int32_t node = i;
        while (parent[node] != node) node = parent[node];
        if (node != root) return 0;
    }
    return 1;
}

int main(void)
{
    char error[256] = {0};
    CcSim first;
    CcSim second;
    CcSim other;
    CcSimInit(&first, 7);
    CcSimInit(&second, 7);
    CcSimInit(&other, 8);

    const CcUnderroadNetwork *network = CcSimUnderroadNetwork(&first);
    CC_CHECK(network != NULL);
    CC_CHECK(network->generated);
    CC_CHECK(network->node_count == first.settlement_count + 4);
    CC_CHECK(network->road_count >= network->node_count - 1);
    CC_CHECK(network->road_count <= CC_MAX_UNDERROAD_ROADS);
    CC_CHECK(Connected(network) == 1);
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    CC_CHECK(CcSimHash(&first) != CcSimHash(&other));
    CC_CHECK(CcSimValidate(&first, error, sizeof(error)));

    for (int32_t i = 0; i < network->road_count; ++i) {
        const CcUnderroadRoad *road = &network->roads[i];
        CC_CHECK(road->from_node >= 0 && road->from_node < network->node_count);
        CC_CHECK(road->to_node >= 0 && road->to_node < network->node_count);
        CC_CHECK(road->from_node != road->to_node);
        CC_CHECK(road->depth >= 0 && road->depth < CC_UNDERROAD_LAYERS);
        CC_CHECK(road->length_cells >= 1);
        CC_CHECK(road->clearance >= 1);
    }

    const char *save_path = "underroad-network-round-trip.ccsave";
    RemoveSave(save_path);
    uint64_t saved_hash = CcSimHash(&first);
    CC_CHECK(CcSaveWrite(save_path, &first, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(save_path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == saved_hash);
    CC_CHECK(restored.underroad.generated);
    CC_CHECK(restored.underroad.node_count == network->node_count);
    CC_CHECK(restored.underroad.road_count == network->road_count);
    RemoveSave(save_path);

    CcMetagame metagame;
    CcMetagameInit(&metagame, UINT32_C(0x71a7e5));
    char output[8192] = {0};
    CC_CHECK(CcMetagameExecute(&metagame, "underroad network", output,
                               sizeof(output)));
    CC_CHECK(strstr(output, "UNDERROAD NETWORK") != NULL);

    printf("underroad network sim: ok\n");
    return 0;
}
