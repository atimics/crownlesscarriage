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

/* This small layout pins the RNG order, rather than comparing two worlds
   produced by the same compiler (which cannot expose argument-order drift). */
static void TestCanonicalCoordinateOrder(void)
{
    static CcSim fixture;
    fixture = (CcSim){0};
    fixture.schema_version = 108U;
    fixture.world_seed = 7U;
    fixture.random_state = UINT32_C(0x12345678);
    fixture.settlement_count = 2;
    fixture.settlements[0].id = 1U;
    fixture.settlements[0].map_x = 100;
    fixture.settlements[0].map_y = 200;
    fixture.settlements[1].id = 2U;
    fixture.settlements[1].map_x = -80;
    fixture.settlements[1].map_y = 40;
    fixture.goblins.lair_settlement_id = 2U;
    fixture.dragon.lair_settlement_id = 1U;
    CcSimInitializeUnderroadNetwork(&fixture);
    static const int32_t expected[6][3] = {
        {112, 186, 1}, {-95, 57, 3},
        {-109, 38, 1}, {-56, 60, 0}, {-47, 58, 2}, {100, 200, 1}
    };
    CC_CHECK(fixture.underroad.layout_seed == UINT32_C(3454611271));
    CC_CHECK(fixture.underroad.node_count == 6);
    for (int32_t i = 0; i < 6; ++i) {
        CC_CHECK(fixture.underroad.nodes[i].map_x == expected[i][0]);
        CC_CHECK(fixture.underroad.nodes[i].map_y == expected[i][1]);
        CC_CHECK(fixture.underroad.nodes[i].depth == expected[i][2]);
    }
    CC_CHECK(fixture.random_state == UINT32_C(0x12345678));
    /* Already generated topology is historical state, not a recipe to rerun. */
    fixture.underroad.nodes[0].map_x += 9;
    CcUnderroadNetwork retained = fixture.underroad;
    CcSimInitializeUnderroadNetwork(&fixture);
    CC_CHECK(memcmp(&retained, &fixture.underroad, sizeof(retained)) == 0);
}

int main(void)
{
    TestCanonicalCoordinateOrder();
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
    /* Existing schema-108 saves may contain the older compiler's layout.
       Preserve stored nodes on load rather than regenerating a canonical one. */
    first.underroad.nodes[0].map_x += 9;
    first.underroad.revision++;
    uint64_t saved_hash = CcSimHash(&first);
    CC_CHECK(CcSaveWrite(save_path, &first, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(save_path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == saved_hash);
    CC_CHECK(restored.underroad.generated);
    CC_CHECK(restored.underroad.nodes[0].map_x == first.underroad.nodes[0].map_x);
    CcSimInitializeUnderroadNetwork(&restored);
    CC_CHECK(CcSimHash(&restored) == saved_hash);
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
