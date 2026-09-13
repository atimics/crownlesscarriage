# Road network snapshot evidence

This change addresses connectivity measurement in #266. Base: PR #577, dfc5faf. Simulation schema 73, generator 25, JSON protocol 6.

`road_network` reports the undirected graph formed by routes with `closed == false`. Settlement nodes include ruins, so an open path through a ruin joins its neighboring nodes. Every settlement reports its current habitation, component identity, and the number of inhabited settlements in that component. The smallest settlement ID in a component identifies it. Component identity remains stable across storage reordering and changes when the graph's membership changes.

The report states the edge rule, direction, and treatment of ruins. These fields define a physical connection measurement. Official carriage eligibility appears in each route's context; company passage, dispatch readiness, actor knowledge, and world-health thresholds have separate rules and acceptance work. Zero inhabited settlements and one inhabited settlement appear explicitly in the total and per-component counts.

## Checks

- Strict release headless build and all 109 CTest tests passed.
- Static analysis passed with one reviewed baseline item.
- Eight controlled fixtures cover a cycle, an alternate path after closure, two disconnected pairs, transit through a ruin, all ruins, one inhabited town, storage reordering, and an empty graph. Every fixture checks full simulation state preservation.
- Save/load JSON tests verify the same network snapshot after reload.
- Two seeds ran for 40 years against the base and draft. All existing JSON fields matched at 82 checkpoints after removing only the new `road_network` object. See `parity.json`. Final measurements are in `endpoints.json`, and controlled snapshots are in `fixtures.json`.

The implementation uses arrays bounded by settlement capacity. Each route joins the roots of its endpoints. A second pass counts inhabitants and chooses component identities. Reporting consumes the current state and leaves it intact.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
out/build/headless/crownless_sim_runner --seed 42 --years 40 --json
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --json
```
