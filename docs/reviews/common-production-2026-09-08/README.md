# Common production contract

Advances #392 and supplies the recipe path for #391. Base: PR #502 at 4d0b641, schema 62 / generator 25. Save schema and SQLite layout stay 62 and 30.

`CcProductionRecipe` declares goods per batch, input reserve floors, work per batch, minimum condition, required tools, and hunger capacity policy. `CcProductionContext` identifies the producer, actual storage and actual location. It supplies the stock array at that store, capacity, output ceiling, available work, condition and hunger.

`CcProductionPlan` previews an act. `CcProductionRun` reads current stock, plans the act, then consumes inputs and adds output once. Its receipt carries the same producer/store/location identity, exact input/output/work totals, or a gate reason. Duplicate inputs and self-consuming recipes are rejected. Intermediate arithmetic stays within signed integer bounds.

Callers own the schedule and work budget. A context describes one work allocation; callers charge receipt.work against any shared labor pool before scheduling another act. Receipts are observations. A replayed command goes through the ordinary journal rules. Freight and transfers remain responsible for physically moving stock between stores.

The weekly town schedule now calls this path for bakery and smithy output. Towns supply their own co-located stock. Smithies consume tools-first, then weapons, followed by existing tool wear. The bakery retains its current place in the weekly schedule, its scriptorium wheat floor, and its 86/72 percent hunger capacity policy. That hunger policy is now recipe data. Paper and treasure migration follow in separate PRs; road-site storage and delivery follow under #391.

Verification:

- Strict Debug build and all 74 headless CTest checks passed.
- Common contract tests exercise local stocks, exact receipts, reserve protection across successive consumers, work and output limits, required tools, condition, hunger policy changes, invalid recipes and integer bounds.
- Two 40-year runs match all annual reports and smithy accounting against 4d0b641. Run each version of `crownless_sim_runner --seed SEED --years 40 --report-every 1 --smithy` and compare its complete output.
- `parity_probe.c`, compiled against each version of libcrownless_sim.a, matches 560 annual hashes across schemas 26, 27, 58, 59, 60, 61 and 62. These fixtures select historical rules after current initialization. The SQLite suite separately checks shipped saves and legacy journals.
- `parity.json` records seeds, versions and output digests.
