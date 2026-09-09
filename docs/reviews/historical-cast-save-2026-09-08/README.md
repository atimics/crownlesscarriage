# Historical quest cast save recovery

This PR extracts the historical-cast repair from #622 onto main ce4f6c91473f08977f2ce9a8e4203fa28b1663a4. It supplies a focused saved-world regression relevant to #614.

A completed quest retains the lifetime IDs and names of its sponsor and affected person. Live character slots later hold other people. Validation used the issued-ID rule for completed quests only at the current schema. Supported older schemas instead required those people to remain in the live array. The repair applies the same historical rule across supported schemas with quest casts. Active quests still require the live cast. Unknown future IDs and IDs of other entity types fail validation.

## Exact reproduction

`tests/fixtures/shipped/schema-73-retired-cast.ccsave` is a generated world containing zero private player data. Seed 2654435769 advances daily from day 1 to day 12411 under schema 73 and generator 25. A failed quest names Mara Venn and Holla Rosewell; its cast includes a person who has left the live array.

`load-results.json` records the fixture hash, size, and both loader outcomes. Main rejects the file with `Situation data is invalid.` The repair loads the same file as schema 74 at day 12411. Build `load-probe.c` against each revision's simulation and persistence libraries, with `-lsqlite3 -lm`, then pass a fresh copy of the fixture to each executable. Save loading may upgrade that copy.

The fixture generator is `quest_cast_tests --write-history-fixture`. It uses the portable save encoder, so the shipped fixture is a complete snapshot. Tests copy it before loading. They compare the migrated world hash against the generated source world, and check player coins, historical IDs, and names directly. A further day is written to the journal, then resumed and checked for equal world hashes. Negative cases cover an active retired cast, a future character ID, and a settlement ID used as a person.

## Scope and checks

This changes validation only. Schema 74, generator 25, SQLite schema 32, gameplay, and saved fields retain their existing formats. Final strict build, full tests, native persistence checks, and static analysis are running.

Issue #614 remains open. Its actual hosted saved world still requires a private backup and reproduction, followed by recovery and restart checks on the live service. This generated fixture proves a specific old-save failure and its repair.
