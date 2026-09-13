# Codebase health acceptance

This audit completes the verification matrix in #386 on main 4f7635cc3a115492470a60ef9223dab0d3c859db. The audited rules are simulation schema 74, generator 25, and SQLite format 32. CcSim is 184,152 bytes.

## Implemented changes on main

- 648eb99 contains the stale-cache macOS target guard, SettlementUnmetNeed, BuyerPurchasingPower, and the independent supported-version test. It is the landed source for these changes; the older closed #388 record is historical context.
- 7426ec2 adds independent saved-field comparisons. 8be362a extends them to every grain-delivery field. 64a6bba names the selected test mode in its output.
- 79e1c0e moves the compatibility decision into cc_sim_versions.c. 8bcd73c adds schema 74 to the supported rules.

These commits are included in the audited main history.

## macOS targets

The current-source macos_configuration_contract configures fresh, stale-empty, and explicit-override caches and builds a runner in each. The exact play-preset run in #597 passed that contract. A second direct vtool inspection confirms the resulting executables:

| Case | Cached target | Executable minimum |
| --- | --- | --- |
| Fresh | 14.0 | 14.0 |
| Stale empty value | 14.0 | 14.0 |
| Explicit override | 15.0 | 15.0 |

macos-targets.json contains the inspected load commands. Signing and notarization stay under #121.

## Independent field coverage

The default field test passes 592 separate hash and saved-value mutations. Its save-only mode passes all 592 direct saved-value checks. The lifetime test also covers received source identity and the retained biography fields.

Two deliberate faults were repeated on this exact source:

1. Omit HASH_VALUE(g->redirected) from cc_sim_hash.c. The test reports the omitted field and exits 1.
2. Keep that omission and write zero in place of g->redirected in cc_save_grain.inc. The save-only test reaches the independent decoded-value comparison, reports the omitted field, and exits 1.

The CcSim declaration and its size stay unchanged throughout both experiments. field-omission.json records each expected failure and the restored passing runs. The production files were restored byte for byte. The size assertion is a shape alarm; these field tests provide the separate value evidence.

## Saved rules and migration

The independent accepted-pair test passes. The public query makes 2,676 recorded decisions, including maximum-integer boundaries; 1,075 pairs are supported. All 45 shipped fixture pairs belong to that set.

The production loader decodes all 45 fixtures, including historical journal fixtures. Each then advances seven days and passes CcSimValidate. fixtures.json records the original metadata; loaded-fixtures.txt records the upgraded schema/generator and state hashes before and after advancing. fixture-probe.c performs the validation.

Historical journal replay uses each record's saved rule version and hash before the runtime upgrades to schema 74. Existing persistence tests cover the historical replay checks and the upgraded values. The current supported-version range, original fixture metadata, and upgraded behavior are recorded together here.

## Shared trade calculations

The two helpers retain their narrow responsibilities: unmet reserve after current stock and incoming cargo, and buying power from the relevant purse plus eligible ledger credit. The planner loops and their ordering remain separate.

An inline control restores those calculations at their seven call sites in the current source: four unmet-need calls and three buying-power calls. The expressions follow the removed calculations in 648eb99, with the current reserve helper name. Two 40-year runs match the ordinary build byte for byte at all 82 complete JSON checkpoints, including state hashes. inline-trade-control.patch makes the comparison reproducible; trade-equivalence.json records its results. The source was restored afterward.

## Final checks

- Strict Release headless targets build successfully.
- After restoring both experiments, the independent field, lifetime, and full SQLite round-trip tests pass.
- Static analysis passes with one reviewed baseline item.
- The exact same production source passes all 151 local play-preset tests in #597. The completed CI head 540dd3f has the same full source tree as audited main and passes 149 Linux and 151 macOS client tests.
- Capture-harness acceptance is handled separately in #597 for #389.

[Completed native CI](https://github.com/atimics/crownlesscarriage/actions/runs/34270839992) supplies the platform checks. The local build used CC_BUILD_CLIENT=OFF, Release, CC_ENABLE_STRICT_WARNINGS=ON, and CC_WARNINGS_AS_ERRORS=ON. Restored CTest results are in restored-tests.txt. Static analysis used CC_CPPCHECK_JOBS=4 with tools/static_analysis.py.
