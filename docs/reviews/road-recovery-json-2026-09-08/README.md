# Evaluated road recovery in JSON

Partial work for #266. Base: #568 at 9919dc7, schema 73, SQLite format 32.

Each route in JSON protocol 6 gains an additive recovery object. Its semantics are evaluated_plan_snapshot. CcSimRoadRecoveryPlan supplies the blocker mask, labor and supplier IDs, available people and materials, next work date, effort, and people used. Text and JSON share the blocker names. An empty reason list means the evaluated plan is ready; an open road reports the existing open gate.

Negative unavailable values from the engine are represented as JSON null. Historical closure_reason remains null because recovery gates describe current work eligibility rather than the event that originally closed the road. Reporting makes no claim about completed repairs or actor-held knowledge.

[Fixtures](fixtures.json) reuse the engine's controlled recovery setup. They cover a ready road; missing food, wood, stone, or tools; restoration to the same ready state; the work calendar; an open road; and an invalid road. Each print checks full-state immutability. Existing engine tests separately verify the real daily recovery response.

[Parent comparison](parity.json) covers 82 checkpoints across two forty-year runs. Every existing JSON field matches after removing the newly added recovery object. This includes state hashes and production accounting.

Validation: strict Release headless build, all 104 tests, static analysis, nine controlled JSON fixtures, and the complete existing-field comparison passed.
