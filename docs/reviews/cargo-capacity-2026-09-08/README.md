# Shared cargo capacity rules

Continues #260 on top of #550 (parent a89feaf).

The goods module now owns player cargo totals, player box counts, freight slot
counts, and freight units per slot. Trade, shipments, validation, and player
commands share these functions through the small internal goods interface.

## Confirmed overflow repairs

Two maximum-sized player goods previously produced -2 occupied slots. A maximum
bread freight load produced -268435455 slots. Wider rounding arithmetic now
returns 268435456 freight slots, and player totals saturate at INT32_MAX when the
sum exceeds the return type. Capacity checks therefore see a full load.

`overflow_probe.c` reproduces both results. `parent_helpers.c` contains the three
unchanged parent private helpers with comparison names. Regression checks in
`material_economy_tests.c` cover aggregate cargo, carried treasure, maximum
freight loads, exact/partial slots, empty quantities, and invalid goods. These
boundary assertions also passed with the goods module directly compiled using
`-fsanitize=undefined -fno-sanitize-recover=undefined`.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including the new cargo regressions, material
  economy, replay, and shipped-save fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `cargo_probe.c` produced 2,176 identical ordinary-case rows: each good plus
  invalid endpoints at eleven quantities, and 2,000 mixed player loads.
- `parity_probe.c` produced 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.
- All 19 native road-book captures and both rendering gates passed: route
  450.5 FPS / 4.249 ms p95; network 347.5 FPS / 4.851 ms p95.
  `roadbook.txt` records capture names and performance reports.

Compile probes with `-std=c11 -Isrc` and link the corresponding build's
`libcrownless_sim.a` and `-lm`. Include `parent_helpers.c` when compiling the
parent cargo and overflow probes. Compare ordinary output byte for byte; the
overflow probe records the intentional boundary repair.
