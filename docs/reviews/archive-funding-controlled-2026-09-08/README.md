# Controlled archive funding study

Issue #606. Draft #628, based on #627.

The same archive source produces much stronger staff continuity and retained
lore when earmarked archive dispatch is paused. Ordinary trade continues in
that arm. The protected reserve helps inherited staff, while its combined
writing-readiness result falls slightly. The next policy experiment should
bound archive-funded wheat and give the writing kit a fair chance to arrive.

## Sources and controls

Simulation source: `0f059ee` from #627, schema 86, generation 25.
Probe implementation: `a0e809d`.
`manifest.json` binds every source file, compiler version, generated overlay,
object, library, and executable by SHA-256.

All three arms use this source and the same 32 seeds for 100 years each:

- **Protected:** current archive dispatch and its inherited-staff reserve floor.
- **Unprotected:** one controlled edit returns the entire ledger as freight
  funds. All other archive, staffing, and lifetime rules stay shared.
- **Paused:** one controlled edit returns from archive dispatch before booking.
  Ordinary freight, grain relief, staffing, and all other world work continue.

The build script places its overlays in a temporary output directory. The
tracked simulation source stays intact. The earlier current-main and original
batch-dispatch comparisons are recorded in the reviews for #622 and #624.
This study isolates two policy switches within the full archive stack.

## Results

All 96 runs pass their 100 annual state checks. Every arm has 166,848 weekly
observations. Totals below combine its 32 worlds.

| Measure | Protected | Unprotected | Paused |
| --- | ---: | ---: | ---: |
| Weekly observations with zero staff | 101,813 | 104,096 | 82,750 |
| Weekly observations ready to write | 411 | 434 | 1,072 |
| Sum of weekly staff counts | 97,050 | 88,172 | 183,628 |
| Sum of weekly retained lore | 1,875,150 | 1,666,074 | 10,879,040 |
| Endpoint staff | 1 | 0 | 4 |
| Endpoint lore | 11 | 0 | 516 |
| Archive purchases | 11,212 | 10,056 | 0 |
| Archive purchase and first-leg toll crowns | 139,308 | 125,792 | 0 |
| Reserve below 50 before staffing | 153,520 | 161,030 | 88,043 |
| Purchases crossing from at least 50 to below 50 | 131 | 208 | 0 |

The protected rule improves the sum of weekly staff counts in all 32 pairs
against the unprotected arm. It improves lore in 21 pairs and reduces it in
nine; two tie. Writing readiness improves in 11, falls in 18, and ties in three.

Pausing dispatch improves weekly retained lore in all 32 pairs against the
protected arm. Writing readiness improves in 28 pairs and staff totals improve
in 31. This supports a funding and allocation problem within archive dispatch.
The pause also changes carriage use and later world events; it measures the
whole effect of that switch. The reserve-floor comparison isolates that rule.

## Archive cargo

| Cargo outcome | Protected | Unprotected |
| --- | ---: | ---: |
| Wheat ordered | 129,197 | 116,483 |
| Wheat delivered to the booked archive seat | 118,941 | 108,309 |
| Wheat redirected to another place | 30 | 34 |
| Wheat lost | 10,226 | 8,140 |
| Tools ordered / delivered / lost | 2 / 1 / 1 | 6 / 5 / 1 |
| Paper ordered | 0 | 0 |
| Unresolved units at endpoint | 0 | 0 |

The planner checks wheat before tools and paper. Its wheat quote includes the
seat town's food deficit. These source rules and the measured cargo mix make
bounded archive wheat and writing-kit priority the next concrete experiment.
Staffing thresholds remain only part of the problem.

## Trace and verification contract

`traces/*.csv.gz` records every weekly balance immediately before staffing and
every archive purchase. The purchase balance is reconstructed as the balance
immediately after its debit plus its exact charge, within the same purchase
function. Each purchase carries its shipment ID, units, day, and staff count.
Arrival and loss events follow those IDs, with redirected cargo kept separate.

The probe uses an external bookkeeping array. Its counters leave world state
intact. All 3,200 protected-arm annual hashes match #627's unchanged simulation.
The probe fails on an unexpected shipment size or exhausted bookkeeping array.
The verifier checks all 96 compressed trace hashes, unique terminal outcomes,
purchase affordability, spend totals, and the cargo identity:

`ordered = delivered + redirected + lost + unresolved`

Purchase spending includes the source payment and first-leg toll. Later route
charges remain part of the shared world simulation. Readiness is sampled after
each full weekly world step. The pre-staff balance comes from the exact staffing
call. These observations describe different moments in that step.

`results.json` keeps every run, annual hash, and failure field. `summary.json`
contains arm totals, `paired.json` contains per-seed comparisons, and
`verification.json` records the verification result. The temporary probes build
with C17, warnings, and warnings treated as errors.

## Reproduce

Build the headless library from the source revision above, then run from the
repository root:

```sh
python3 docs/reviews/archive-funding-controlled-2026-09-08/build-probes.py . out/build/headless/libcrownless_sim.a /tmp/archive-funding-controlled
python3 docs/reviews/archive-funding-controlled-2026-09-08/run-study.py /tmp/archive-funding-controlled
python3 docs/reviews/archive-funding-controlled-2026-09-08/verify-results.py
```

The full #606 acceptance still includes a bounded supply-policy repair and its
low-fund, interrupted-delivery, and recovery save/replay checks. This evidence
supports that next repair and keeps the dispatch stack under draft review.
