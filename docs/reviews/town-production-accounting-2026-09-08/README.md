# Town production accounting

This slice of #394 extends the caller-owned production ledger to towns. `CcProductionAccounting` holds both town and site rows. Existing callers can keep the `CcRoadProductionAccounting` alias. The simulation owns its ordinary state; a caller owns these cumulative observations.

The town row records primary output accepted into stock, production lost at the hard stock limit, rare gold/gem output, and primary tool wear. Bakery and paper rows record recipe inputs, outputs, work and gate counts. Paper tool wear has its own count. Treasure rows record the materials committed to a saved work order, its work steps and completed artifacts. Active and inactive weeks make each town's accounting interval explicit.

Primary cap loss is the hard `CC_SIM_MAX_UNITS` limit. The existing nutrition ledger separately records storage overflow, age loss and civilian use. Bakery and paper counts come directly from the recipe receipt, before later consumers can use the output. Treasure material commitment happens once; three work steps produce one completed treasure. Rare-mine receipts count actual new gold/gems after seam work advances.

## JSON protocol 2

The existing `--json` report and `make production-baseline` capture now use protocol 2. Each town has a `production` object. It contains `primary_output`, `primary_cap_loss`, `rare_mine_output`, `primary_tools_worn`, `active_weeks`, `inactive_weeks`, `bakery`, `paper`, `treasure` and `treasures_completed`. Recipe rows contain per-good input/output arrays, work, tool wear and the existing named gate order.

Paper gate counts include its existing service, hunger, Tool and food-buffer checks. Input gates include both the food buffer and recipe input floors. Completed treasure work can have a work gate while it awaits an artifact slot. Herd output remains explicitly unavailable as `herd_production_totals: null`.

World schema 65, generator 25 and SQLite 31 continue from the parent. The ledger remains outside saved state. Its totals begin at the capture's start day.

## Evidence

The fixed 40-year seed `0x5eed0001` retains final state hash `82c5fa3dcf2eaa91`. The receipts show 9,757 Bread from Gloamgate and 43,595 Bread from Rosespire; paper and treasure production are zero in that baseline. `baseline.json` preserves all town totals. A funded fixture separately proves a partial three-Paper batch costs one Wood and one work, records a Tool wearing out, and proves one treasure costs one Gold, one Gem and three work steps.

The strict Debug build and headless tests cover current and historical paper input policies, gate counts, hard-cap loss, rare seams, partial paper output, treasure commitment and observer purity. Daily and yearly advances produce the same state and ledger. The JSON test checks the new fields and retains save/text/hash and freight conservation checks.

Against parent `9f257e9`, 800 annual state hashes match across two seeds and schemas 26, 27, 33, 34, 36, 37, 61, 63, 64 and 65. Compile the included `parity_probe.c` separately against each checkout's headers and simulation library. `parity.json` stores the comparison digest. Reproduce the baseline with `crownless_sim_runner --json --seed 0x5eed0001 --years 40`.
