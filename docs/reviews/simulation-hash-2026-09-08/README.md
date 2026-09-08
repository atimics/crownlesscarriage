# Simulation hash module review

Partial work for #260. Base: #563 at 5e764c3, on main 6e4818c. Schema 73 and SQLite format 32.

Move CcSimHash and its three private helpers from cc_sim.c into cc_sim_hash.c. The existing public declaration remains the module interface. All 906 moved lines match the parent exactly, including field order, integer byte order, string termination, and historical schema gates. CMake includes the module in the common simulation library.

[Comparison evidence](parity.json) records 960 matching annual hashes across twelve schema versions, two seeds, and forty years per case. [Probe](parity_probe.c) links separately against the parent and draft simulation libraries. Their complete outputs match byte for byte.

The headless suite includes historical saves and journals plus 592 independent saved-field/hash mutations. The writer, replay, migrations, and state layout retain their existing definitions. This separates the hash boundary so future state changes can be reviewed in a dedicated file.

Validation: strict Release headless and native client builds passed. All 101 headless tests and static analysis passed. The 960-row parent comparison passed.
