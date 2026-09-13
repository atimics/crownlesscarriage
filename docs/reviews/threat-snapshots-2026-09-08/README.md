# Identified threat snapshots

Partial work for #266. Base: #567 at 3d1cb01. Schema 73 and SQLite format 32.

The text summary previously printed the influence and pressure of array slot zero under global-looking names. It now reports `bandit_groups`, `bandit_influence_max`, `monster_groups`, and `monster_pressure_max`. Empty populations have count zero and an `unavailable` maximum. Consumers of the old `bandit_influence` and `monster_pressure` text keys should use these explicit fields.

JSON protocol 6 receives an additive `threats` object with `semantics: snapshot`. Its bandit rows identify the group, route, camp, name, members, supplies, influence, and current raid phase. Monster rows identify the population, dungeon, name, population count, pressure, and hunting pressure. IDs use the existing string convention. Empty populations produce empty arrays. These are current-state values at the checkpoint day.

[Fixtures](fixtures.json) cover zero, one, and three groups, with the highest values deliberately in the second slot. They verify ID uniqueness, escaped names, maxima, and full-state immutability while printing. The production JSON test also checks that save/load preserves these snapshots.

[Parent comparison](parity.json) records matching annual state hashes for two seeds over forty years each. Reporting preserves simulation decisions and RNG state. These engine diagnostics remain separate from actor-visible knowledge.

Validation: strict Release headless build, all 103 tests, static analysis, and the focused production JSON test passed. The broader transition, access, and recovery measurements in #266 remain follow-up work.
