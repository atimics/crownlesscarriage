# Received knowledge retention acceptance

Issue #459 is implemented in main at `85bfb702ba233585d4e3359273f048de1cae7b2f`, schema 74 and generator 25. This audit verifies its five acceptance criteria on that source revision.

| Requirement | Evidence |
| --- | --- |
| A tells B; A dies normally; B retains the account and source | `character_lifetime_tests.c` starts with an account received through the ordinary mine social thread. It triggers the source's scheduled lifecycle death and compares the receiver's entire knowledge record, including original ID and attributed name. |
| Successors have distinct IDs through reuse and eviction | The test repeats source-slot deaths beyond `CC_MAX_HISTORIC_CHARACTERS`, validates each state, checks new IDs and retained source attribution, and verifies that the retired biography lookup becomes unavailable. Four controlled cases verify importance, death-date, stable-ID ties, and reordered storage. |
| Shared minimal lifetime identity contract | `CcHistoricCharacter` holds ID, name, life dates, ancestor, home, generation, role, and importance in the simulation's bounded 32-record store. `CcSimHistoricCharacter` exposes the shared ID lookup. Received knowledge keeps its own actual source-name snapshot alongside the stable source ID. This is the minimum lifetime foundation shared with #250 and available to later document and seal work. |
| Save/load, legacy replay, validation, and independent fields | The lifecycle test saves before and after eviction, validates repeated deaths, compares deterministic replay, and independently mutates 12 saved fields. `CheckPre61KnowledgeJournal` in `persistence_tests.c` replays a schema-60 source death under its original rule, checks its legacy hash, upgrades, and verifies surviving source names. It is called by the persistence test entry point. |
| Cleanup preserves received knowledge and history grants no new knowledge | For schema 62 and later, death records the lifetime instead of deleting receiver entries. The test compares all other characters' knowledge after death and checks that the newborn has zero entries. Historical lookup has no production caller outside its definition; received names are retained within the existing knowledge records. |

The broader lifetime issue #250 retains seal, letter, campaign, relic, and demographic acceptance work. This audit covers the focused received-account defect in #459.

## Fresh validation

The strict headless build and all 112 tests passed on this main revision during the companion smithy audit. This audit reran `received_account_lifetimes`, `sqlite_round_trip`, and `persistence_field_contract`; all three passed. The lifetime executable also passed in `--save-only` mode, including all 12 direct saved-field comparisons. The text files preserve those results.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -R 'received_account_lifetimes|sqlite_round_trip|persistence_field_contract'
out/build/headless/character_lifetime_tests --save-only
```
