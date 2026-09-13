# Archive recruitment quote

Base: PR #600, `7ad882b`. Related issues: #446 and #470.

The quote names one recruit, a trainer when needed, the current archive seat,
the first route and hop, travel time, supplies, wages and any funding patrons.
It uses the actual archive seat and the shared seat viability, spare grain,
funding, silence and courier path rules. The runner reports the result as
`archive_recruitment`, with `recruitment_quote_snapshot` semantics.

## Terms

A trained scribe needs seven days of orientation. A local apprentice needs
28 days with a named trainer. Both receive a wage quote of 50 from the archive
ledger. The source town reserves two wheat per seven days of travel, rounded
up, after its household reserve. The seat reserves these totals:

| Quote | Wheat | Paper | Tools | Recruit days | Trainer days |
| --- | ---: | ---: | ---: | ---: | ---: |
| Trained hire | 4 | 2 | 1 | 7 | 0 |
| Local apprenticeship | 18 | 5 | 1 | 28 | 28 |

Each total includes two wheat and one paper for a first archive task. Tools
are the retained work kit. The future work order must separate training use
from that first-task reserve and use the existing archive tool-wear rule.
These are proposed recruitment terms for review.

Candidates are living adults at an inhabited town, working, and free from a
bandit camp, a ruling or patron office, and an active situation's named cast.
The query ranks ready quotes first, then trained hires, earliest completion,
and stable character ID. Travel follows the same path policy as couriers and
adds each leg's real travel days. Closed roads, war borders and high danger are
marked as dangerous. A destroyed route or an abandoned intermediate town can
block the journey. Arrival and completion dates are estimates from this
snapshot; the future journey must recheck each leg.

For planning, occupied archive places are assigned to available local scribes
in stable ID order, up to the current staff count. These inferred incumbents
can train apprentices and are excluded from new hires. This assumption is
explicit: the next saved work-order stage must store actual staff identities.

When the ledger needs funds, recovery uses the existing five-year silence gate
and the existing treasury top-up quote. Each donor must have a living named
monastery patron or ruler in its kingdom. The quote retains donor shares and
patron IDs. The current ledger can fund normal recruitment directly.

## Evidence

- Strict headless build and all 120 headless tests passed.
- Static analysis passed with one reviewed baseline item.
- Focused fixtures cover a trained hire, apprenticeship, first-task reserves,
  trainer absence, full staff, death, childhood, travel activity, ruling office,
  stable selection after slot reordering, a two-leg dangerous journey, source
  food, destroyed roads, abandoned intermediate towns, materials, silence,
  treasury funds, named patron death, old schemas, null input and calendar bounds.
- Every focused query compares the entire simulation before and after.
- JSON checks cover save/load and completion date arithmetic.
- Two 40-year comparisons give 82 complete checkpoints. Removing only the new
  report field leaves every existing JSON field equal to the parent build.
  Schema 77, generator 25 and the saved layout remain the same.

| Simulation seed | Annual checkpoints | Ready | Seat supplies or viability | Trainer |
| --- | ---: | ---: | ---: | ---: |
| 42 | 41 | 1 | 39 | 1 |
| 0x5eed0001 | 41 | 1 | 38 | 2 |

`measurements.json` contains each quote and its day. Reproduce with the runner's
`--seed SEED --years 40 --json` options. Compare against the parent executable,
then remove `archive_recruitment` before comparing the remaining objects.

## Next implementation

The quote is ready to drive a saved work order. That stage must commit funds
and materials once, record actual staff IDs, track the named person's journey,
handle interruption and death, complete training, and permit a bounded first
archive task. The full recruitment issue remains open until those behaviors,
conservation and replay are proved.
