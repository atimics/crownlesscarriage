# Grain delivery saved-field contract

Partial work for #386, based on main 6e4818c (schema 73, SQLite format 32).

The current-main field test covered settlement goods, tool wear, ponies, road sites, and archives. This adds every field in CcGrainSupply. Twelve fields are mutated separately in each settlement slot. The shipment reference uses a real wheat shipment created through the funded organiser command. Each changed value is compared directly after save and read.

The test performs 592 checks. Its default mode also requires every single-field mutation to change the simulation hash. The optional `--save-only` mode isolates the direct save assertions for fault injection.

## Deliberate omission evidence

[Recorded results](fault-injection.json) show both expected failures. Neither experiment changes the structure size.

1. Temporarily omit `HASH_VALUE(g->redirected)` in `CcSimHash`, build `persistence_fields_tests`, and run it normally. It reports `Hash omitted grain_supplies[town].redirected`.
2. Keep that hash omission and temporarily replace `BindInt(statement, 11, g->redirected)` with a zero value in the grain writer. Run the test with `--save-only`. Decoding accepts the intentionally incomplete hash, and the independent field comparison reports `Save round trip omitted grain_supplies[town].redirected`.
3. Restore both source files, rebuild, and run the full headless suite plus `persistence_fields_tests --save-only`.

Both injected runs exited 1 at the intended assertion. The committed production writer and hash match the named base. The default test retains hash checks.

## Scope

This extends independent coverage for the newly landed grain delivery state. The rest of #386 still requires its full current-main verification matrix, including macOS target checks, compatibility evidence, and shared trade logic. Historical fixture and journal tests run as part of the headless suite.

Validation after restoration: strict Release headless build passed, all 101 tests passed, all 592 save-only field checks passed, and static analysis passed with its reviewed baseline.

## Current-base macOS target checks

The same 101-test run executed `macos_configuration_contract` against this source, based on main 6e4818c. It configured three separate directories, built `crownless_sim_runner` in each, and inspected each executable. A second direct `xcrun vtool -show-build` pass confirmed the records in [macos-targets.json](macos-targets.json).

| Case | Cached target | Executable minimum |
| --- | --- | --- |
| Fresh configuration | 14.0 | 14.0 |
| Reused cache with empty target | 14.0 | 14.0 |
| Explicit override | 15.0 | 15.0 |

This supplies current-source evidence for the deployment-target portion of #386. Signing and notarization remain under #121.
