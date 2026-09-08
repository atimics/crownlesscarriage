# Legacy runtime migration boundary

Continues #260 on top of #555 (parent 38a64df).

`cc_legacy_runtime.c` owns the historical runtime migrations behind one internal
entry point. The database loader still verifies the historical snapshot and
replays its journal before migration. Player-knowledge and pony initialization,
final validation, and database operations retain their places in the loader.
The entire moved block matches the parent after entry-point renaming, including
schema/generator branches, name repairs, journey rescaling, and upgrade order.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including historical schema acceptance/rejection,
  shipped snapshots, old journals, migration, and independent saved-field tests.
- Local Cppcheck 2.20.0 passed the repository gate.
- `save_probe.c` decodes all 42 shipped `.ccsave` fixtures from memory, preserving
  the input files. Parent and current libraries produce byte-identical encoded
  saves for every fixture. `saved-bytes.json` records each size and SHA-256.
  Full saved bytes provide a field comparison independent of `CcSimHash`.
- The same probe compares loaded schema/generator versions, state hashes, and
  state hashes after seven additional days. All 42 rows match.
- `parity_probe.c` produces 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.

- All 19 native road-book captures and both rendering gates passed: route
  432.5 FPS / 5.160 ms p95; network 345.9 FPS / 4.867 ms p95.
  `roadbook.txt` records capture names and performance reports.

Compile the save probe with `-std=c11 -Isrc`, linking the corresponding build's
`libcrownless_persistence.a`, `libcrownless_sim.a`, `-lsqlite3`, and `-lm`.
Pass an existing output directory followed by the sorted shipped fixture paths.
Use separate output directories for parent and current and compare every saved
file byte for byte. The annual probe needs only the simulation library and `-lm`.
