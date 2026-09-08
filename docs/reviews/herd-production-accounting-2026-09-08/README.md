# Herd production accounting

Related issue: #394. Base: draft #513 at `6c3007f`.

JSON protocol 4 replaces `herd_production_totals: null` with `herds`. Each town records cows, sheep and ponies separately. Each species reports current adults, young, hunger and condition, plus cumulative feed units, stored output units and output lost at the hard stock limit. Arrays follow the report's named `goods` list.

Cow dairy has separate nutrition counters: `dairy_nutrition`, `dairy_used` and `dairy_unused`. The first equals the sum of the other two. Current dairy feeds people directly. Slaughter adds Meat to stock; sheep shearing adds Wool. These quantities use their own units. Historical versions before schema 32 put dairy into Bread stock, so those receipts appear in cow output instead.

Feed is measured at the animal meal. Outputs are measured at each stock write. This preserves both sides when a legacy cow consumes Bread and also produces Bread during the same week. Full stores produce a separate cap-loss receipt. Pony feed records the cost of keeping working animals. Current pony goods output is zero.

All counters belong to the caller. The saved world remains schema 65, generator 25 and SQLite schema 31. Accounting starts at the report's `accounting_start_day`; loaded reports begin with fresh counters. Animal births and deaths remain visible through current herd counts and existing events; cumulative birth/death counters remain future work.

## Validation

The town accounting tests cover each species' winter feed, spring shearing, output at a nearly full store, winter slaughter, legacy Bread feed/output, and daily versus annual observer parity. The JSON test checks repeated reports, policy cases, save reload, herd array dimensions and dairy totals. It also selects Paper by its goods name for the existing paper output assertion.

Run the standard headless suite:

```sh
cmake -S . -B out/build/foundation -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Debug -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/foundation -j4
ctest --test-dir out/build/foundation --output-on-failure -j4
```

Use `make production-baseline` for the common 40-year Release capture. Its manifest records the commit, binary hash, build mode, seed, commands, checkpoint dates and final save/state hashes.

Recorded on clean Release commit `a18cdff0fae82f767f54505dcb371e7303fc321f`: four policy/site cases, two matching runs each, 41 checkpoints per run. All 81 local Debug tests passed. All 800 annual hashes matched parent `6c3007f` across the ten schemas and two seeds in `parity.json`. `manifest.json` pins the capture provenance and `outcomes.json` records the final herd totals.
