# Workshop processing

Continues #260 on top of #554 (parent 881cffd).

The production module now owns bakery conversion, smithy batches, paper-mill
conversion, rare-mine progress, and shared tool wear. The weekly simulation
retains its original call order. An immutable three-service table supplies event
recording, local event causes, and treasure completion from the core simulation.
Farm, mine, and archive work share the extracted tool-wear rule.

All five moved function bodies match the parent after service-call substitution.
Older schema recipes, food buffers, rounding, tool wear, and event order keep
their existing behavior.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including material economy, grain, replay,
  and shipped-save fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `weekly_probe.c` produced 1,440 matching full-state hashes across two seeds,
  nine schema modes (26, 27, 29, 33, 34, 36, 37, 54, 60), five prepared variants,
  and sixteen weekly observations. Variants cover generated state, metal/wood
  shortages, staple-food shortages, stocked workshops near treasure and seam
  thresholds, and high hunger. The first update crosses a production-report day.
- `parity_probe.c` produced 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.

- All 19 native road-book captures and both rendering gates passed: route
  431.8 FPS / 5.089 ms p95; network 343.3 FPS / 5.091 ms p95.
  `roadbook.txt` records capture names and performance reports.

Compile each probe with `-std=c11 -Isrc` and link the corresponding build's
`libcrownless_sim.a` and `-lm`. Run parent and current binaries and compare their
text output byte for byte.
