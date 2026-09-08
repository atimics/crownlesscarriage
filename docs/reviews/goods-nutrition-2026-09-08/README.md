# Shared goods and nutrition foundation

Continues #260 on top of #549 (parent 83c63f7).

`cc_goods.c` owns goods definitions, schema availability, nutrition values,
consumption, and preferred nutrition selection. Public declarations stay in
`cc_sim.h`; the internal header exposes one selection helper to the economy.
Seven moved function bodies match the parent. Consumption uses wider arithmetic
for whole-unit rounding and accumulated delivery.

## Confirmed overflow repair

With 10 bread and an `INT32_MAX` civilian nutrition request, the parent returns
-2147483648 and leaves 1073741834 bread. The fixed function returns 20 and leaves
zero. `overflow_probe.c` reproduces this case against either library.

Regression checks in `grain_economy_tests.c` cover limited bread stock, maximum
bread stock, mixed bread/meat, and animal wheat at the maximum request. The
boundary checks also pass with the goods module directly compiled using
`-fsanitize=undefined -fno-sanitize-recover=undefined`. Whole-unit rounding and
consumption order remain stable for ordinary requests.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including the new arithmetic regressions,
  nutrition accounting, material economy, replay, and shipped-save fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `nutrition_probe.c` produces 13,824 matching rows from separately linked
  parent and current libraries. It covers three nutrition purposes, eight
  quantities for each food (including negative, empty, and simulation maximum),
  and nine requests. It compares available nutrition, returned consumption,
  and all three remaining food stocks.
- `parity_probe.c` produces 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.

- All 19 native road-book captures and both rendering gates passed: route
  437.3 FPS / 4.550 ms p95; network 358.6 FPS / 4.863 ms p95.
  `roadbook.txt` records capture names and performance reports.

Compile each probe with `-std=c11 -Isrc`, link the corresponding build's
`libcrownless_sim.a` and `-lm`, and compare the ordinary-case outputs byte for
byte. The overflow probe records the intentional repaired boundary behavior.
