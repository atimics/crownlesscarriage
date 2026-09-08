# Road-site wear from completed work

Related: #393 under #396. Base: draft #516, including the event-text correction at `b1e5f38`.

Recipe-bearing road sites lose condition after completed production work. At condition 80 or above, each weekly receipt costs half its work, rounded up. Below 80, it costs one condition point per work unit. Below 65, capacity falls from two batches per week to one. Below 50, the existing condition gate pauses production. Idle and blocked production have zero wear. Repairs retain their full effect and use the funded maintenance path from #516.

This applies to goods recipes and road crews. A road crew's heavier work costs more condition. Road houses remain outside production wear. Town building condition and scrapping remain later parts of #393.

The work changes existing site condition, with no extra saved fields. Simulation schema 68 enables wear and the capacity reduction. Generator 25 and SQLite schema 31 remain unchanged. A schema 67 journal suffix retains the older production rate and condition; the next week after upgrade uses the new rule.

JSON protocol 6 reports a numeric `wear` total for each site. Initial condition plus maintenance gain less wear equals current condition in the unattended capture. Production, repair materials and freight remain separate.

## Validation

- All 83 local headless tests passed before the inherited repair-message correction. Focused checks cover the final merged code.
- `road_site_use_wear` starts a mill at condition 60 with twenty Wheat and one working Tool. It makes eleven Bread, slows to condition 49, and pauses with nine Wheat left. After a funded local repair fixture, it resumes at one batch per week. The carriage-funded recovery path is covered separately by `funded_site_maintenance`.
- The wear test also checks forge capacity, heavier road-crew work, idle wear, saved condition and the legacy control.
- The persistence test proves schema 67 journal replay before the wear boundary.
- The ordinary production and JSON tests check observer parity, material conservation and condition accounting.

Use `make production-baseline` for the repeated four-case 40-year Release capture. The recorded outcomes distinguish the natural baseline, opened sites and whole-policy dragon controls.

Recorded on clean Release commit `2bdc1e7b16fab7af48420625f2abaf18323c51e1`: four policy/site cases, two matching runs each, 41 checkpoints per run. All condition, material and wear/work bounds pass at every checkpoint. All 960 historical annual hashes match the parent across the schemas in `parity.json`. The final focused checks passed after the inherited message correction. `manifest.json` and `outcomes.json` pin the capture provenance and site results.
