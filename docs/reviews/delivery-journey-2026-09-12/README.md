# Combined delivery journey review

This branch combines #686, #688, #690, and #691 on the #687 performance
foundation. It keeps schema 95 and generator 25. The merge preserves the
handoff, presented-target, and conversation-focus input regressions together.

The browser review of combined commit `6a4a257e` showed a remaining action-row
problem: card order changed as residents moved while the player read the row.
Commit `f087ec48` adds a stable nearby row. Resident movement keeps its order;
an active approach holds the row. Once the player moves more than three local
units from the row's origin, nearby choices refresh. A delivery handoff keeps
first place. Removed targets and replaced characters receive current cards,
while click handling still checks the identity and availability actually shown.

The expanded input regression tests resident movement, holding a row while
approaching, refreshing after moving into a new nearby area, current
availability, replaced residents, removed targets, and returning from another
view. The real town and hall target builders still put the delivery first.

Validation completed so far:

- Strict native Release build passed.
- Static analysis passed with one reviewed baseline item.
- All 173 combined native tests passed after the steady-row change.
- The final steady-row web build passed.
- Browser captures show the same row before and after resident movement. Clicking
  Harvest board opens Local promises. Mara stays visible while offering the job.
  Accepting it loads eight cargo boxes on day 1 with 42 crowns. Saving reports
  both the journal and local scene checkpoint saved.

The browser run uses a fresh offline world on a local server. The initial
review reached a town-guard conversation with the speaker visible. The updated
review also reached the named sponsor Mara and saved the accepted cargo. Full
multi-town delivery and later food recovery remain follow-up play checks.
