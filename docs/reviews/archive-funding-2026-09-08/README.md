# Archive funding plan

This supports exact funding diagnostics in #266 and recovery work in #446. Base: PR #584 at 6cf50a0. Schema 73, generator 25, JSON protocol 6.

`CcSimArchiveFundingPlan` reports the archive seat, selected donor kingdoms, each share, the total treasury top-up, and its evaluated blocker. The existing recovery transfer consumes this plan. JSON adds `archive_funding` with `treasury_top_up_plan_snapshot` semantics.

The plan evaluates held funds and the existing route rules. Recovery silence and weekly timing remain separate gates in the archive step. Under schema 58 and later, a solvent host treasury can fund the archive directly. Otherwise the existing search requires two solvent kingdoms connected through open roads and inhabited endpoints. The first selected kingdom pays the larger share of an odd top-up. The schema-57 ten-coin cap remains in place.

The blocker identifies the exact evaluated return path: `unavailable` for unsupported query state, `archive_seat` for an absent seat, `ledger_funded` when the ledger already meets the target, `top_up_limit` for the earlier contribution cap, or `connected_solvent_donors` when the search finds fewer than two eligible donors. `ready` accompanies a positive plan. Timing remains a separate gate.

## Validation

- Strict release headless build and all 111 tests passed.
- Static analysis passed with one reviewed baseline item.
- Controlled tests verify host funding, missing host solvency, connected donors, exact 3/2 shares, a second donor below the threshold, restored solvency, the legacy cap, a full ledger, and an absent seat. Tests assert each blocker and its name. Each query preserves the entire simulation state.
- Existing material-chain tests cover the recovery wait, actual treasury transfers, conservation, and restoration events through the normal archive step.
- JSON tests check donor totals, string identities, and save/load equality.
- All existing JSON fields and simulation hashes match at 82 checkpoints across two 40-year runs. Only the new `archive_funding` field is removed for comparison. See `parity.json`.

Named recruitment, actual arrival, supply eligibility, and the wider recovery outcome remain acceptance work in #446. This plan exposes the current funding policy and keeps its transfer tied to the reported amounts.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
out/build/headless/crownless_sim_runner --seed 42 --years 40 --json
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --json
```
