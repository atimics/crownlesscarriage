# Foundation backlog review — 8 September 2026

Base: `289d55e` (schema 58, generator 25). Read the titles of all 130 open
issues and the four open PRs, then inspected the production, custody, source
lifetime and persistence issue contracts against this base. The issue notes
from 7 September often name schema 52; implementation should use the actual
base of each PR.

## First draft PRs

| PR | Delivery | Issues advanced |
| --- | --- | --- |
| #496 | Shared smithy batch plan, stop reasons, material-use checks | #461, #392, #394 |
| #497 | Fresh, stale and explicit macOS target checks; override fix | #386 |
| #498 | 519 independent hash and saved-field mutations | #386; supports later production and herd work |

The smithy plan is the production contract slice of #461. Silverwick's seeded
Tools and Weapons capacities remain zero. Alderwatch keeps its weapon line.
The next capacity change needs a chosen Silverwick output, an explicit
Alderwatch tool-supply rule, and saved-rule migration tests. The plan already
reports why a line stops and allocates local Iron and Wood in execution order.
Tool wear applies after gross output, as before.

## Next delivery order

1. Finish the narrow smithy capacity decision under #461. Use the shared plan
   for player-facing status and #394 production accounting. Keep the #392
   building refactor incremental.
2. Add actual production counters and pinned checkpoints to the existing
   metrics runner under #394. The current waste ledger already distinguishes
   civilian use, ageing and storage overflow. Carry those definitions into
   #266 and the policy comparisons for #400/#401.
3. Define source lifetime identity under #459/#250, then implement the bounded
   transfer slice under #434. These unlock documents, purses, bodies, freight,
   and the first saved treasure collection in #484–#486.
4. Add the reachable clearing command under #390. Current road-site validation
   requires blocked sites, so the command and its saved-state rules need to
   advance together. Pair this with local output custody in #391.
5. Implement one site-to-town trip under #471 after custody and saved road
   anchors in #323. Measure goods at the source, in transit, and at arrival.

PR #453 owns the archive-seat experiment. The open mine, Underroad design and
Wyrmheart comparison PRs remain separate deliveries. Their issue acceptance
should follow their current implementation and measurements.

## Local evidence

The smithy refactor passed the 71-test headless suite. The speech server test
required local network access and passed on rerun. Strict Debug compilation
passed. All 40 annual reports matched the base byte for byte for each seed:

| Seed | Final day | Final state hash |
| --- | --- | --- |
| `0x5eed0001` | 14601 | `fd3fe3b41fdbc51d` |
| `0xc0a71a9e` | 14601 | `c6f5afda85e5b080` |

Comparison command for each base and changed binary:

```sh
crownless_sim_runner --seed 0x5eed0001 --years 40 --report-every 1
crownless_sim_runner --seed 0xc0a71a9e --years 40 --report-every 1
```

PR #498 passed 519 field mutations and the SQLite historical-save/journal
suite. Temporarily omitting `paper_tool_wear` from the hash produced the
expected field-specific failure while struct size stayed fixed. Production
source was restored before the final tests.

PR #497 passed all three cache cases and inspected each built executable:
fresh and stale targets were 14.0; the explicit override was 15.0. CI results
are recorded on each draft PR.
