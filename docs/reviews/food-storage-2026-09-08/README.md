# Shared food storage and reserve rules

Continues #260 on top of #552 (parent 5bdf4b8).

`cc_food_economy.c` owns reserve targets, civilian and wartime food use,
nutrition storage, spoilage accounting, incoming nutrition, and price refreshes.
Production, trade, relief planning, and simulation reports use its internal
interface. Ten moved function bodies match the parent after helper renaming.
The weekly world update keeps its existing call order and food policy.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including material economy, grain, nutrition
  accounting, replay, and shipped-save fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `rules_probe.c` produced 5,400 identical rows from parent and current builds.
  It covers two seeds, five schema modes (26, 29, 32, 33, 60), every initial
  settlement, all goods, and six variants: generated state, empty stock with
  services removed, maximum stock with a granary, small population, zero food
  consumption, and fortress function.
- Comparisons include prices, reserve targets, storage capacity, food use,
  incoming nutrition, full state hashes after spoilage, and per-good aged and
  overflow accounting.
- `parity_probe.c` produced 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.

- All 19 native road-book captures and both rendering gates passed: route
  451.9 FPS / 4.586 ms p95; network 353.3 FPS / 4.767 ms p95.
  `roadbook.txt` records capture names and performance reports.

Compile probes with `-std=c11 -Isrc` and link the corresponding build's
`libcrownless_sim.a` and `-lm`. Include `parent_helpers.c` for the parent rules
probe: its bodies were verified against the parent's private functions after
renaming. Compare parent and current outputs byte for byte.
