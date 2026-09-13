# Departure preview and shared route rules

Continues #260 on top of #545 (parent 9e455e2).

The journey module owns the departure preview: travel watches, fodder,
readiness, map claims, road-house details, and deterministic rain. A small
internal route-rules module owns tolls and dragon shadow danger. Player
travel and trade use the same toll function. Public interfaces stay stable.
Five moved function bodies match the parent after helper renaming.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including departure, journey choices, journal
  replay, and shipped-save fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `preview_probe.c` produced 2,838 matching rows against separately linked
  parent and current simulation libraries. It covers two seeds, eleven schema
  modes (13 through 60), every initial route in both directions, and eight
  combinations of opening travel, midday departure, map availability, hungry
  horses, dragon shadow, and closed roads. Each query checks that the complete
  simulation hash stays unchanged. Missing destinations are included.
- `parity_probe.c` produced 720 matching annual state hashes over two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.
- Native road-book QA produced all 19 captures and passed both performance
  gates: route 443.6 FPS / 4.616 ms p95; network 337.7 FPS / 4.925 ms p95.
  `roadbook.txt` records the capture names and performance reports.

Departure execution and encounter resolution are the next journey boundaries.
