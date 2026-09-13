/* Connectivity of the undirected graph formed by routes whose closed flag is false. */
static int32_t NetworkRoot(const int32_t *parents, int32_t slot)
{
    while (parents[slot] != slot) slot = parents[slot];
    return slot;
}

static void JsonRoadNetwork(const CcSim *sim)
{
    int32_t parents[CC_MAX_SETTLEMENTS];
    int32_t inhabited[CC_MAX_SETTLEMENTS] = {0};
    CcId identities[CC_MAX_SETTLEMENTS] = {0};
    for (int32_t i = 0; i < sim->settlement_count; ++i) parents[i] = i;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->closed) continue;
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        if (from == NULL || to == NULL) continue;
        int32_t a = NetworkRoot(parents, (int32_t)(from - sim->settlements));
        int32_t b = NetworkRoot(parents, (int32_t)(to - sim->settlements));
        if (a != b) parents[b] = a;
    }
    int32_t total_inhabited = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        int32_t root = NetworkRoot(parents, i);
        CcId id = sim->settlements[i].id;
        if (identities[root] == 0 || id < identities[root]) identities[root] = id;
        if (sim->settlements[i].population > 0) {
            inhabited[root]++;
            total_inhabited++;
        }
    }
    printf("{\"semantics\":\"snapshot\",\"edge_rule\":\"route_closed_false\",\"direction\":\"undirected\",\"ruins\":\"included_as_nodes\",\"inhabited_settlements\":%d,\"settlements\":[", total_inhabited);
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        int32_t root = NetworkRoot(parents, i);
        printf("%s{\"id\":\"%" PRIu64 "\",\"inhabited\":%s,\"component_id\":\"%" PRIu64 "\",\"component_inhabited_settlements\":%d}",
            i ? "," : "", sim->settlements[i].id,
            sim->settlements[i].population > 0 ? "true" : "false",
            identities[root], inhabited[root]);
    }
    printf("]}");
}
