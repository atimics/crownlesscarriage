# Road-site production

Advances #391 through the common #392 recipe path. Base #505 at 84547da, schema 63 / generator 25. This change uses schema 64 / generator 25. SQLite layout stays 31. Stores and recipes retain their existing site IDs and declared input/output goods.

On days divisible by seven, after town production and route upkeep, each of the 16 production/work sites receives one work allocation in site order. Road houses retain their store role. Work requires an open site, condition at least 50, and one Tool. Stores supply all materials locally. Each site has one authoritative production act per week.

| Site kind | Batch input | Batch result | Input floor | Weekly batches |
| --- | --- | --- | --- | --- |
| Farm | 1 Tool | 4 Wheat | 1 Tool | 2 |
| Woodlot | 1 Tool | 4 Wood | 1 Tool | 2 |
| Quarry | 1 Tool | 4 Stone | 1 Tool | 2 |
| Mine | 1 Tool | 2 Iron | 1 Tool | 2 |
| Mill / bakery | 1 Wheat | 1 Bread | 2 Wheat | 2 |
| Pasture | 2 Wheat | 1 Meat | 2 Wheat | 2 |
| Smithy | 2 Iron, 1 Wood | 1 Tool | 1 Iron, 1 Wood | 2 |
| Road crew | 2 Stone | Up to 3 route condition | 1 Stone | 2 |

The required Tool remains at the works. Recipes that consume Tools use additional bundles above that floor. Each goods batch uses one work unit. Each road-repair batch uses three. Goods output stays within the store's existing 24 cargo slots. Capacity planning uses space available before the act. Other town consumers continue to use their own stores and their established order.

Road crews use the shared recipe's work-only mode, then apply the receipt to their own route. The mode consumes inputs and work without indexing a goods output. A single weekly event aggregates successful works, goods bundles and route repair.

`CcSimPlanRoadSite` is a read-only preview. `CcSimAdvanceDaysWithProductionAccounting` supplies caller-owned totals per site: exact inputs, outputs, work, repair and gate counts. The runner's `--sites` flag prints those totals plus current stock. Gate columns follow CcProductionGate: ready, invalid, closed, condition, capacity, output full, work, tools, input.

Validation covers all site recipes, exact stock reconciliation, blocked and unfunded controls, reserve protection, store capacity, route work, read-only previews, capture/hash parity, small/batched time, save/resume and journal replay. A schema-63 journal crosses a weekly boundary with stores intact; after migration, future weeks run the schema-64 recipes.

The explicit 40-year pilot fixture funds Stag's Mill, Crown Forge and Ashfield Farm once. Blocked controls receive identical initial stocks. Both 0x5eed0001 and 0xc0a71a9e produce 4 Bread, 4 Tools and 16 Wheat in the opened fixture and zero in the blocked control. Final average hunger is 33 in every case. These are local production results. Carrier pickup and relief delivery remain the next #391 work; the original gold-seam study remains recorded with #500.

Reproduce the fixture with pilot_fixture.c linked against crownless_persistence and crownless_sim. Arguments are SEED, OPENED (0/1), and save path. Then run crownless_sim_runner --load PATH --years 40 --report-every 1 --sites --smithy. measurements.json records the fixture inputs, complete-output digests, final world report and pilot-site totals.

The strict Debug build and all 75 headless CTest checks passed. The focused road-site production check also passed after the final control and accounting-parity additions.
