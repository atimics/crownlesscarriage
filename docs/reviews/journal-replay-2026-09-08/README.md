# Journal replay boundary

Continues #260 on top of #558 (parent 41ec09f).

`cc_journal_replay.c` owns checkpoint validation and ordered journal replay,
including historical command decoding and stored hash parsing. The loader calls
one replay entry point before runtime migration. The internal journal header
shares operation kinds, record version, and step limits with the writer.
Snapshot loading also shares the same stored-hash parser.

All four moved function bodies match the parent after helper renaming. SQL
queries, ordinal checks, schema/generator checks, pre/post-state hashes, and
command/day/runtime application retain their order and error text.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including journal recovery, ownership,
  checkpoint tampering, historical migrations, and independent saved-field tests.
- Local Cppcheck 2.20.0 passed the repository gate.
- `save_probe.c` decodes all 42 shipped `.ccsave` fixtures from memory. Parent
  and current libraries produce byte-identical encoded saves for every fixture;
  `saved-bytes.json` records each size and SHA-256. Loaded schema/generator
  versions and state hashes match, including after seven additional days.
- `parity_probe.c` produces 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.

- All 19 native road-book captures and both rendering gates passed: route
  431.3 FPS / 4.986 ms p95; network 347.4 FPS / 4.614 ms p95.
  `roadbook.txt` records capture names and performance reports.

Compile the save probe with `-std=c11 -Isrc`, linking the corresponding build's
`libcrownless_persistence.a`, `libcrownless_sim.a`, `-lsqlite3`, and `-lm`.
Pass an existing output directory followed by sorted shipped fixture paths.
Use separate output directories for parent and current and compare every saved
file byte for byte. The annual probe needs the simulation library and `-lm`.
