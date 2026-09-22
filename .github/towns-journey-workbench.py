from pathlib import Path

p = Path('src/sim/cc_sim.c')
s = p.read_text()
old = '''        UnderroadAddNode(
            sim, CC_UNDERROAD_NODE_ENTRANCE, i, -1, settlement->name,
            settlement->map_x + UnderroadRange(&state, 41) - 20,
            settlement->map_y + UnderroadRange(&state, 41) - 20, &state);'''
new = '''        /* Consume x, then y, then depth in UnderroadAddNode. C does not
           specify argument evaluation order; inline RNG calls gave GCC and
           Clang different networks when loading the same old campaign. */
        int32_t map_x = settlement->map_x + UnderroadRange(&state, 41) - 20;
        int32_t map_y = settlement->map_y + UnderroadRange(&state, 41) - 20;
        UnderroadAddNode(sim, CC_UNDERROAD_NODE_ENTRANCE, i, -1,
                         settlement->name, map_x, map_y, &state);'''
assert s.count(old) == 1
s = s.replace(old, new)
old = '''        UnderroadAddNode(sim, CC_UNDERROAD_NODE_LAIR, lair_settlement, clan,
                         lair_names[clan],
                         anchor_x + UnderroadRange(&state, 81) - 40,
                         anchor_y + UnderroadRange(&state, 81) - 40, &state);'''
new = '''        int32_t map_x = anchor_x + UnderroadRange(&state, 81) - 40;
        int32_t map_y = anchor_y + UnderroadRange(&state, 81) - 40;
        UnderroadAddNode(sim, CC_UNDERROAD_NODE_LAIR, lair_settlement, clan,
                         lair_names[clan], map_x, map_y, &state);'''
assert s.count(old) == 1
p.write_text(s.replace(old, new))

p = Path('tests/underroad_network_sim_tests.c')
s = p.read_text()
new = '''/* This small layout pins the RNG order, rather than comparing two worlds
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

'''
assert s.count('int main(void)\n{') == 1
s = s.replace('int main(void)\n{', new + 'int main(void)\n{\n    TestCanonicalCoordinateOrder();')
old = '    uint64_t saved_hash = CcSimHash(&first);'
assert s.count(old) == 1
s = s.replace(old, '''    /* Existing schema-108 saves may contain the older compiler's layout.
       Preserve stored nodes on load rather than regenerating a canonical one. */
    first.underroad.nodes[0].map_x += 9;
    first.underroad.revision++;
    uint64_t saved_hash = CcSimHash(&first);''')
old = '    CC_CHECK(restored.underroad.generated);'
assert s.count(old) == 1
s = s.replace(old, '''    CC_CHECK(restored.underroad.generated);
    CC_CHECK(restored.underroad.nodes[0].map_x == first.underroad.nodes[0].map_x);
    CcSimInitializeUnderroadNetwork(&restored);
    CC_CHECK(CcSimHash(&restored) == saved_hash);''')
p.write_text(s)
