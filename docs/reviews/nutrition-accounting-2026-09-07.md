# Nutrition storage accounting

Base: `b105a1b33c7120f0ef3f655b7a592aff9105c2f4`, schema 53, generator 25.

`CcSimAdvanceDaysWithNutritionAccounting` accepts a caller-owned, zero-initialized ledger. Each weekly town update records stored goods eaten by civilians, natural aging loss, and the remaining stock discarded above the storage cap. Counters are cumulative goods units by settlement slot and good. Each slot records its settlement ID. Subtract two ledger snapshots for a sample; use `CcGoodNutritionValue` to convert units to civilian nutrition.

The measurement scope is the existing weekly town stock consumption and `SpoilStoredNutrition` phase. Production, bakery conversion, animal feed, fresh dairy, and shipment movement retain their existing paths. A real shipment fixture verifies that arriving bread is a transfer. The optional ledger lives outside `CcSim`, saves, journals, hashes, and gameplay events. Ordinary stepping uses the same path with a null ledger. A caller can start a new collection interval with a fresh zeroed ledger.

Run the existing metrics tool with an extra output file:

```sh
out/build/backlog/crownless_sim_metrics --seed 1 --years 40 --nutrition-csv nutrition.csv > metrics.csv
```

The tool's `--seed 1` selects world seed `0x9e3779b9`, as before. Each annual sample has 18 rows: six towns times Bread, Wheat, and Meat. Columns include annual and cumulative aging loss, overflow, stored civilian use, and wasted nutrition. `--final-only` emits the last annual sample with campaign cumulative totals. Abandoned towns retain their earlier totals. Multiple seeds each start a fresh ledger.

## Exact fixture

An isolated town has one ration of weekly civilian demand, ordinary storage, and zero production. Its storage pass runs after civilian consumption:

| Good | Initial units | Civilian use | Aging loss | Overflow | Stored after |
| --- | ---: | ---: | ---: | ---: | ---: |
| Bread | 200 | 1 | 1 | 186 | 12 |
| Wheat | 1000 | 0 | 2 | 974 | 24 |
| Meat | 100 | 0 | 5 | 93 | 2 |

The below-cap fixture starts with 10 Bread, 10 Wheat, and 1 Meat. It consumes one Bread and records zero storage loss. Both fixtures run the actual simulation update. The CSV test checks cumulative sums, nutrition conversion, final-only output, and identical ordinary metrics with collection enabled.

## Baseline and validation

Before instrumentation, the strict Release library at the base revision produced hash `50d8f654db1ed6f8` for seed `0x5EED0001` after 14,600 days. The measured run asserts that frozen hash. Daily and batched collection agree over 365 days, including every authoritative simulation byte and every ledger counter. Schema 52 measured and ordinary stepping also agree byte for byte. All 40 annual metrics rows for tool seed 1 match the uninstrumented metrics executable byte for byte.

```sh
cmake -S . -B out/build/backlog -DCMAKE_BUILD_TYPE=Release -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/backlog -j4
ctest --test-dir out/build/backlog --output-on-failure
```
