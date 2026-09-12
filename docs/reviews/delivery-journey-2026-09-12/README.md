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
review also reached the named sponsor Mara and saved the accepted cargo. The continued multi-town delivery is recorded below. Later food recovery
remains a follow-up play check.

The continued run travelled from Thornford through Gloamgate toward Alderwatch.
The broken causeway payment took nine crowns, leaving 33 and all eight cargo
boxes. A stopped-road save restored day 3, 44% progress, ETA 52 hours, 33 crowns,
and eight cargo boxes after a browser reload and Play. The road captures show
that checkpoint before and after reload.

On day 4 a click followed the displayed Camp at The Broken Crown card. The
next capture showed the carriage stopped. The Journal's CAMP entry describes
regular overnight rest; `ApplyJourneyStopAction` emits that text, while
`ApplyRoadSiteStop` names the selected roadside site. This entry therefore
supports overnight rest only. The named roadside camp and its time, fatigue,
and risk effects still need a direct check. The promise remained accepted,
with progress 0/8 and a day-36 deadline.

The carriage reached Alderwatch on day 5 with 33 crowns and all eight boxes.
Parking reported that the team was watered and stabled. Saving confirmed both
the journal and local scene checkpoint. The next leg is toward Silverwick.

The next leg reached Silverwick on day 9. During the approach, a camp-card
click reached the mine yard as the branch choices changed. The player returned
to the road through the yard carriage. This is further evidence for reviewing
road-choice stability; the named camp effects remain open.

At Silverwick, the first town card led to Company store. The first counter
card opened Deliver promise with eight Bread selected. The quote offered
40 crowns plus an 18-crown reward. Completing it changed the purse from 33 to
91, cargo from eight to zero, and store Bread from 18 to 26. The store confirmed
the delivery and reward. The player saved both the journal and local scene.
The later captures use a narrower browser viewport than the initial review.

The recipient reply still needs a separate browser check. The lasting food
recovery and chosen camp time, fatigue, and risk checks also remain open.

## Road input follow-up

Commit `1f333a7d` remembers the drawn road cards and their screen bounds. A click
resolves the same action on the same route using current availability. A card
that changes from camp to mine requires a fresh drawing before the mine choice
can receive that click. The checks include the action kind, target, goods,
quantity, drive state, and label. Drawing another view or resetting clears the
road cards.

All 173 native tests passed after the implementation. The final added regression
also passed through the real click handler. The web build passed, and static
analysis passed with one reviewed baseline item. The existing browser captures
remain evidence for the earlier `f087ec48` build. A browser replay of this road
input change remains a follow-up check.
