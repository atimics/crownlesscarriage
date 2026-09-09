# Smithy capacity acceptance audit

Issue #461 is implemented in main at `85bfb702ba233585d4e3359273f048de1cae7b2f` (schema 74, generator 25). This audit checks its five acceptance criteria against that revision.

| Requirement | Current evidence |
| --- | --- |
| A supplied working smithy produces tools locally | `CheckSmithyPlan` checks one exact local batch. `CheckSeededSmithyProduction` checks positive Silverwick output at its settlement identity over 40 years for both seeds. |
| Disabled works have an explicit reason | The shared smithy plan reports missing service, zero capacity, missing iron or wood, met reserves, abandoned workers, and full-fire repairs. `CheckSmithyPlan` and `CheckSmithyAccounting` check those gates. |
| Configuration catches the reported contradiction without a blanket grant | New Silverwick worlds have capacity 2. The test pins it and preserves Alderwatch's zero tool capacity and positive weapon production. A matched zero-capacity Silverwick control produces zero tools and consumes zero recipe inputs. |
| Regression and compatible accounting compare the fixture | Both seeded tests validate annually, compare accounting with ordinary simulation hashes, and assert two iron plus one wood per tool. Fresh captures below repeat exactly and preserve ordinary summaries and hashes. |
| Saved state and old journals preserve their rules | `CheckSchema58SmithyCapacity` replays a day journal with zero Silverwick capacity and custom capacity 7 elsewhere, checks the legacy hash, upgrades, and performs another current save round trip. The current persistence and independent saved-field suites passed. |

The first four checks live in `tests/material_economy_tests.c`. The journal check lives in `tests/persistence_tests.c` and is called by its test entry point. Current core and persistence code were inspected alongside these assertions.

## Fresh measurements

Strict Release headless build passed with warnings treated as errors. All 112 CTest tests passed on the audited main revision.

| Seed | Silverwick tools | Iron used | Wood used | Alderwatch weapons |
| --- | ---: | ---: | ---: | ---: |
| `0x5eed0001` | 2428 | 4856 | 2428 | 18 |
| `0xc0a71a9e` | 2506 | 5012 | 2506 | 24 |

Both runs end at day 14601. Each report was repeated independently and matched byte for byte. Removing smithy accounting lines yielded the ordinary report exactly, including hashes. `measurements.json` contains the revision, runner digest, output digests, settlement-name mapping, and full final counters. The text files preserve the complete reports.

These results prove the bounded smithy issue. Archive recovery, wider freight policy, and whole-world health retain their own acceptance work.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --report-every 40 --smithy
out/build/headless/crownless_sim_runner --seed 0xc0a71a9e --years 40 --report-every 40 --smithy
```
