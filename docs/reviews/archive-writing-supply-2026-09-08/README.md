# Archive writing-supply budget experiment

Issue #606. Draft #629, based on #628. Implementation `78b9e10`, schema 87,
generation 25. This remains a policy experiment: staff continuity and spending
improve, while writing readiness and retained lore fall against the parent.

## Policy

Archive dispatch gives paper first choice, then tools, then wheat. It buys up
to one missing paper unit, one missing tool, and one week of scribe wheat.
Incoming cargo counts toward each target. A blocked source, route, or payment
gives the next good a chance to use the carriage. Earlier saved worlds keep
their original wheat-first policy and town food-deficit calculation.

Freight preserves the inherited-staff threshold and at least 50 crowns for
the next recruitment. Real source stock, payment, carriage capacity, border
rules, journey risk, and archive work's household-food reserve continue to
apply. The wheat target uses actual local stock; it bounds the archive-funded
load even while the town has a larger food deficit.

## Paired study

Thirty-two identical seeds run for 100 years per arm. Each arm has 166,848
weekly observations. Parent results come from #628's protected dispatch arm.
The first pass is retained in the sibling `archive-writing-supply-first-pass-2026-09-08`
review. The final pass adds the minimum 50-crown recruitment reserve.

| Measure | Parent | First pass | Final draft |
| --- | ---: | ---: | ---: |
| Archive spending, crowns | 139,308 | 77,843 | 70,111 |
| Weekly zero-staff observations | 101,813 | 101,061 | 101,061 |
| Sum of weekly staff counts | 97,050 | 123,475 | 123,475 |
| Weekly writing-ready observations | 411 | 191 | 318 |
| Sum of weekly retained lore | 1,875,150 | 550,343 | 1,008,161 |
| Endpoint lore | 11 | 0 | 0 |
| Purchases crossing below 50 crowns | 131 | 74 | 0 |
| Paper purchased | 0 | 0 | 0 |

The final draft spends less in all 32 pairs and raises staff totals in 31.
Writing readiness falls in 20 pairs and rises in seven; five tie. Retained
lore falls in 27 pairs and rises in five. These outcomes keep #606 open.
Paper supply and a viable archive seat remain central follow-up work under
#240. Future policy changes should improve useful writing alongside staff
continuity and funding.

Every final study world passes all 100 annual state checks. Every traced
purchase has enough funds. All units reconcile as delivered, redirected,
lost, or unresolved. `results.json` and `traces/` retain each run; `paired.json`
keeps the per-seed comparisons. `manifest.json` binds source revisions and the
generated probe files and binaries.

## Validation

- Strict headless build and all 125 tests passed.
- Strict desktop build and 16 focused archive, save, quest, freight, and
  material-economy checks passed.
- Static analysis passed with one reviewed baseline item.
- All 3,200 schema-86 annual hashes match the parent behavior.
- All 3,200 final-policy annual hashes match between the ordinary simulation
  and the instrumented observer.
- The new schema-86 recruitment journal fixture replays and upgrades.
- Query regressions cover priority, unavailable paper sources, bounded wheat,
  incoming cargo, lost cargo, and the recruitment floor.
- Existing current-schema dispatch tests cover exact payment, low funds,
  travel disruption, arrival, save/load, and journal restart.

The ordinary tool-convoy fixture now sets its archive ledger to zero so its
assertions exercise town-market funding. The new priority had allowed archive
funding to win that booking. The test keeps its exact buyer, seller, toll,
cargo, and whole-world coin checks.

## Reproduce

Build the headless library from the implementation revision. Then run:

```sh
python3 docs/reviews/archive-writing-supply-2026-09-08/build-probes.py . out/build/headless/libcrownless_sim.a /tmp/archive-writing-supply
python3 docs/reviews/archive-writing-supply-2026-09-08/run-study.py /tmp/archive-writing-supply
cc -O2 -Isrc docs/reviews/archive-writing-supply-2026-09-08/hash-probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/writing-hash-probe
python3 docs/reviews/archive-writing-supply-2026-09-08/check-hashes.py /tmp/writing-hash-probe
python3 docs/reviews/archive-writing-supply-2026-09-08/verify-results.py
```

The trace format and counting contract follow #628. Purchase spending includes
source payment and the first-leg toll. Weekly readiness and exact pre-staff
balances observe separate moments in the weekly update. A successful local
check establishes implementation consistency; the paired world results above
establish the current policy tradeoff.
