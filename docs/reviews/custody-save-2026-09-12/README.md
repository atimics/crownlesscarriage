# Custody save checks

Source: `27d34458b917099cdfc740a2416a88f6c5d0d9ef`.
Base: `0edd7347dc954ff3b5e0e0bc01b85888fdd3bab0` (#695).

All 140 strict headless tests pass. Repository static analysis passes. The custody save test checks binary and file
round trips with goods, a purse, a container, and a retired slot. It checks world
totals, damaged rows, unchanged destinations after failed reads, and schema 98
upgrade. The transfer tests cover each field in the custody hash.

`probe.c` was compiled against each build's simulation and persistence libraries,
with SQLite and libm. The compressed outputs retain all 3,193 version checks and
61 save/replay pairs. The only admission change is schema 99 with generator 25.
For the replay comparison, the decoded world's schema is set to 98 before hashing,
so the comparison checks existing fields under their existing hash contract.
The new schema's custody state is checked separately by `custody_save_tests`.

The WebAssembly compiler accepted `cc_sim.c` and `cc_sim_hash.c` with strict
warnings. The quick benchmark passed its 90,000 ns/day limit at 32,366.3 ns/day.

This stage persists goods, purses, and containers rooted in settlement stores.
Carrier adapters, named item migration, document work identity, creation costs,
and the player packing journey remain part of the open draft.
