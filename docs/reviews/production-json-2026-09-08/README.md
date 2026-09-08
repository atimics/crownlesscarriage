# Repeated production capture

Protocol 2 adds town production accounting; see [the extension](../town-production-accounting-2026-09-08/README.md). The manifest below records the original protocol 1 capture.

This reporting slice of #394 adds JSON Lines to the existing simulation runner. Run the complete baseline protocol from a clean checkout with:

```sh
make production-baseline
```

The command builds a headless Release runner. It captures seed `0x5EED0001` for 40 years twice with baseline inputs and twice with an explicitly opened/funded mill, forge and farm. Each report contains day 1 plus all 40 annual checkpoints through day 14601. Structural validation runs each year. The wrapper requires each pair of reports to match byte for byte.

Output goes to `out/reports/production-baseline`. Choose a fresh folder for another capture with `PRODUCTION_OUTPUT=out/reports/another-capture`. Each run has a final SQLite save and a JSONL report. The manifest records the commit, working-tree status, build mode, runner digest, exact commands, numeric seed mapping, checkpoint days, report digests, save digests and final state hashes. This is a wrapper around the existing runner's world loop.

## Report contract

Use `crownless_sim_runner --json --seed 0x5EED0001 --years 40 --report-every 1` for a direct capture. `--opened-production-pilots` selects the explicit fixture. It opens sites 2, 11 and 15 and gives each its working Tool plus four recipe batches and the input reserve. Initial stocks appear in the first checkpoint. A loaded save uses `--load`; its first day becomes the accounting start. Counters begin at zero on each invocation.

Protocol 1 uses decimal strings for entity IDs and hexadecimal strings for state hashes. Goods arrays follow the `goods` labels. Production gate arrays follow `production_gates`. Quantities are goods units. Civilian nutrition value comes from the existing goods definitions.

The report includes:

- Town stocks, reserves, population, hunger, coins, food use, age loss, cap overflow and smithy inputs/outputs/tool wear.
- Site locations, stocks, condition, recipe inputs/outputs/work, gate counts, road repair, both loading points, both unloading points and road losses.
- Carriage and shipment custody, status, route and timing; road condition, capacity and open/closed day counts.
- Crown treasuries, dragon hoard, goblin coins, player coins and monastery reserve.

Open/closed days sample the road's closed flag after each daily update. Exact closure causes, site wear and other town-production totals remain `null` until their accounting is available. The listed coin balances are named ledger accounts. Broader physical coin custody, town recipe totals, dragon-slain policy comparisons and multi-seed outcome work remain under the related issues.

Inspection uses const state. The JSON mode advances the same daily world loop with caller-owned accounting. Tests compare its state hashes against ordinary yearly text output and both final saves, then reconcile site stocks and all freight quantities. The report includes the actual stock and cargo, so endpoint stock can be checked separately from production and transport.

Validation covers the strict Debug build, the headless suite and a focused JSON test: repeated bytes, exact checkpoint days, baseline/opened fixtures, current save reload, read-only reporting, daily/yearly hash parity, reserve-aware material conservation, cargo conservation, incompatible options and manifest provenance.

The recorded clean run used commit `457bef52f0829d915454ae30b51f9db5524265f3` in Release mode. All four 40-year runs completed; each fixture produced 41 checkpoints and matched its repeat byte for byte. `manifest.json` preserves their exact commands and digests. All 80 Debug headless tests passed; the focused JSON test also passed against this Release runner.
