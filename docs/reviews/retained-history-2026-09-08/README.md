# Retained event history evidence

This change addresses part of issue #266. The text summary calls the event ring count `retained_events`. JSON adds `retained_history` with snapshot semantics, capacity, count, and the earliest and latest retained days. The unavailable lifetime total is represented by `null`. The day bounds describe the records held in the ring. Historical coverage and actor knowledge require separate evidence.

Base: 736fafd (PR #570). Simulation schema 73, generator 25, JSON protocol 6. This is an additive JSON field. Text consumers should use the new `retained_events` key.

## Checks

- Strict release headless build passed.
- All 106 CTest tests passed.
- Static analysis passed with one reviewed baseline item.
- Three fixtures cover an empty ring, a wrapped ring with dates out of order, and a full ring. Each fixture checks that reporting preserves the entire simulation state.
- The production JSON test checks retained history after save and load.
- Two seeds, 42 and 0x5eed0001, ran for 40 years against the base and draft. All existing JSON fields matched at all 82 checkpoints, including the initial state. `parity.json` records the comparison; `fixtures.json` records the controlled cases.

The wider world health and reachability work in #266 remains open.
