# Shared production calculations

Continues #260 on top of #553 (parent ea0c807).

`cc_production.c` owns bakery capacity and effective production, including
seasonal grain factors, hunger, tools, local labor, mine deposits, and public
mine access. Weekly production and the food report share its two-function
internal interface. Settlement monster pressure joins the route-rules module,
where production, town conditions, and road danger share the same calculation.

All four moved function bodies match the parent after helper renaming. The
weekly mutation order and older-schema production rules stay stable.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including grain, material economy, animal
  economy, replay, and shipped-save fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `production_probe.c` produced 12,096 identical rows across two seeds, six
  schema modes (26, 28, 29, 32, 40, 60), all initial settlements, all goods,
  and twelve prepared variants. Cases cover all four seasons, hunger bands,
  tool shortages, small population, empty deposits, zero field yield,
  removed services, and public dungeon access. Each fixture verifies that
  production queries preserve the complete simulation hash.
- `parity_probe.c` produced 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.

- All 19 native road-book captures and both rendering gates passed: route
  471.4 FPS / 4.782 ms p95; network 392.3 FPS / 4.368 ms p95.
  `roadbook.txt` records capture names and performance reports.

Compile the production probe with `-std=c11 -Isrc -Itests` and the annual probe
with `-std=c11 -Isrc`. Link the corresponding build's `libcrownless_sim.a` and
`-lm`. Include `parent_helpers.c` for the parent production probe; its bodies
were verified against the parent after helper renaming. Compare outputs byte
for byte.
