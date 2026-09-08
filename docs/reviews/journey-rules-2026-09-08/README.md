# Journey timing and road-house rules

Continues #260 after capture and benchmark orchestration. A separate
`cc_journey.c` module now owns pace names and rates, carriage speed, journey
watch/stop/ETA queries, and road-house names, positions, prices, and distances.
It moves 187 lines from `cc_sim.c`.

The public query interface stays in `cc_sim.h`. Four shared helpers have a small
internal header for execution, validation, and road-house generation. The moved
rule bodies match the parent after the private helper names are normalized.

## Evidence

- Strict Release headless, native client, and browser builds passed.
- All 119 client CTest checks passed on the combined branch, including journey,
  journal replay, and save fixtures.
- Local Cppcheck 2.20.0 passed the repository static-analysis gate.
- `query_probe.c` compares 16,320 observations across two seeds, every initial
  route, both directions, 3–12 watches, all three paces, and travelling/resting
  phases. Watch numbers, stops, ETA, road-house values and pace names match the
  parent bytes exactly. Each query group also preserves the simulation hash.
- `parity_probe.c` compares 720 annual state hashes across two seeds and schema
  rule modes 26, 27, 33, 34, 36, 37, 58, 59, and 60. Each mode starts from the
  same seeded initialization and runs for 40 years. Every row matches.
- `parity.json` records row counts and SHA-256 digests for both versions.
- Native `run_roadbook_qa` passed with 19 captures. Route rendering met its budget
  at 512.4 FPS / 4.721 ms p95; network rendering at 431.9 FPS / 4.304 ms p95.

The probes were linked separately against the parent and extracted simulation
libraries. The branch also includes #540's tested map-mask guard correction.
Distance policy remains tracked in #432; journey command execution is the next
boundary in #260.
