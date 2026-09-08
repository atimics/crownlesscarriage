# Journey departure execution

Continues #260 on top of #546 (parent a90c0f4).

`cc_journey_departure.c` owns departure validation, contract checks, payments,
fodder, cargo spoilage, and the initial journey/carriage state. The simulation
command dispatcher calls one internal entry point. It supplies an immutable
service table for existing entity lookups, random draws, gossip, local causes,
road discovery, and event recording. These services preserve their existing
owners and the departure module controls their call order.

The departure and spoilage bodies match the parent after replacing private
helper calls with service calls. Schema-specific payments and spoilage remain
in the same positions, including the day advance before departure.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including departure, travel animation, roadside
  stops, journal replay, and shipped-save fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `departure_probe.c` produced 8,470 identical rows from separately linked
  parent and current libraries: 5,654 command results and 2,816 runtime
  transitions after accepted departures. It covers two seeds, eleven schema
  modes from 13 through 60, every initial route in both directions, and sixteen
  prepared variants. Variants exercise opening/midday travel, charts, hunger,
  dragon shadow, closed roads, cargo spoilage, funds, and approaching foaling.
  Comparisons include error messages and full state hashes.
- `parity_probe.c` produced 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run.
- All 19 native road-book captures passed. Rendering gates passed at
  516.0 FPS / 3.981 ms p95 for route and 418.1 FPS / 4.043 ms p95 for network.
  `roadbook.txt` records the capture names and performance reports.

## Reproduce the comparisons

Compile each probe with `-std=c11 -Isrc` and link it against the corresponding
build's `libcrownless_sim.a` and `-lm`. Run both binaries and compare their text
outputs byte for byte. `parity.json` records row counts and SHA-256 digests.

Encounter resolution and runtime transitions are the next journey boundaries.
