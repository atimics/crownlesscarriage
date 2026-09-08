# Identity validation boundary

Partial work for #260. Base: #564 at fd69fa0, schema 73, SQLite format 32.

Move the bounded identity ledger, duplicate checks, entity-kind checks, and serial-counter checks into cc_identity.c. The internal entry point is CcIdentityValidate. CcSimValidate still validates collection counts before calling it. Its call position, traversal order, and error messages remain the same. The ID serial mask now has one shared internal definition.

TrackIdentity and ValidateIdentityState match the parent after the entry-point rename. SetError is a private exact copy. The existing royal-route usage checks retain their position within this block.

[Comparison evidence](parity.json) records 1,788 matching validation results and error messages across six schemas, two seeds, and new/twenty-year worlds. All 24 baselines validated. [Probe](parity_probe.c) checks missing IDs, wrong kinds, future serials, duplicates, and counter bounds. Every call compares the full state before and after validation with memcmp. Parent and draft output files match byte for byte.

This creates a focused validation boundary for future identity consumers such as #434. Custody transfers and new state remain separate implementation work.

Validation: strict Release headless build, all 101 tests, static analysis, and the 1,788-case parent comparison passed.
