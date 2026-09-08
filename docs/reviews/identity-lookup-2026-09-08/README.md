# Bounded identity lookup

Foundation work for #260, based on #566 at 79e1c0e. Schema 73 and SQLite format 32.

The old identity ledger compared each new ID with all earlier IDs. The replacement mixes each ID into a bounded table and probes occupied slots until it finds the ID or an empty slot. Zero is an empty marker because zero serials are rejected first. The maximum live identity count remains 475; the table has 951 slots, which guarantees space for every allowed entry. Insertion order, serial tracking, and error precedence remain the same.

## Evidence

[Validation comparison](parity.json): all 1,788 results and error messages match the parent across six schemas, two seeds, and new/twenty-year worlds. The existing identity-validation probe checks the full simulation bytes before and after each call.

The new regression test exercises sixteen colliding IDs starting in the last table bucket, wraparound, duplicate rejection, all 475 entries, a rejected extra entry, zero IDs, and wrong entity kinds. Address and undefined-behavior sanitizers passed on this test.

[Benchmark](benchmark.c) measures 3,000 ledger-only calls and 3,000 full-validation calls per world, across two seeds and new/twenty-year worlds. Five paired runs alternate parent/draft order and use process CPU time. [Raw measurements and medians](benchmark.json) show ledger CPU reductions of 85–95% and full-validation reductions of 25–41%. These are local microbenchmark results; overall game and long-sweep speed have separate costs.

The table adds 3,808 bytes to the stack ledger (3,816 to 7,624 bytes on this build). It uses fixed storage and preserves the entity capacity.

Validation: strict Release headless build, all 102 tests, static analysis, focused sanitizers, and parent validation parity passed.
