# Continuous road travel

The road is a journey, not a held button. Selecting a destination starts movement.
During ordinary travel the principal action is **Stop**; while halted it is
**Travel**. Space activates that same visible action on press. Holding either
Space or the pointer is not a second travel mode. Carriage inspection stays
available, and stopped **Road options** exposes stepping down, camping and
reversal. At a turn-off the primary choice is **Travel on** or the named site;
site clearing, repair and stock operations are behind **Site options**. Actual
junctions retain their named route choices. Parking retains one parking action.

The existing time compression ramps to 8x without holding a control. The first
and last ten percent no longer arbitrarily fall back to 1x. Pace (careful,
steady, push), fatigue, wear, weather and route distance are unchanged. Automatic
watch breaks/overnight rests use the existing journal commands and costs.
Explicit camp remains a time-and-provision action; it leaves a manually halted
company halted. Stop itself does not spend campaign time or supplies.

## Motion

Compressed map distance must not be fed straight through metre-scale legs. The
road publisher transports the physical rig's coordinate frame, then gives it
one bounded real-time stride at the current pace. Contacts, swing progress and
gait phase survive the transport. The cached previous pose keeps its *old*
world frame for interpolation. Current targets still use actual route placement
and terrain. Ordinary town arrival grounding and other creatures retain the
existing physical path; route discontinuities retain pose invalidation.

This is deliberately travel presentation, not a claim of literal hoof adhesion
while kilometres are compressed into seconds. Vehicle, harness and team anchors
stay coherent; semantic route progress, not decorative hoof cadence, owns ETA,
encounters, inventory and arrival.

Shared clients interpolate the last received route progress over the measured
snapshot interval (capped at 1.5 seconds). They never invent progress beyond the
last authoritative sample. Route/origin/segment/direction and explicit stop or
decision boundaries rebase the presentation. Stale updates stop locomotion and
show **Waiting for shared world** rather than silently walking indefinitely.

## Shared authority

`stop_travel` / `resume_travel` use the existing authenticated, sequenced,
revision-checked command/receipt channel in both browser and native transports.
The host stores a journey-keyed hold in the additive `road_holds` table and sends
`travel_stopped` metadata. Every crew member sees the same halt. Retries are
idempotent, stale actions are rejected, and halting/resuming discard host tick
catch-up. A stopped campaign blob is not rewritten merely to record that hold.

The table is independent of the campaign save format. New trips, accepted route
choices, visiting a site, withdrawal and company wipes clear obsolete holds;
server restart retains a current hold. Existing host pause and away-world
calendar behavior are separate and unchanged. Deploy the host alongside the
client: an older host rejects the new actions, rather than accepting an unsafe
local-only stop. This does not change who is authorized to drive the shared
company.

## Verification

- `continuous_road_gait` exercises the actual world publisher and physical feet
  at 1x/8x, counts swinging frames, bounds planted-only runs, checks team anchoring,
  preceding pose frames, halting and route changes. Advancing phase alone is not
  accepted as walking.
- `continuous_road_input` feeds ordinary queued Space and pointer events through
  the production journal/input/update path: automatic progress, press-to-stop,
  held-input stability, click-to-resume, junction hold and named onward choice.
- `daily_play_policy` covers bounded delayed snapshot sampling and resets.
- Shared host tests cover authoritative stop, both players, retries, stale/wrong
  route commands, restart and no accumulated catch-up. Browser and native
  transport tests exercise the new actions and metadata.
- Existing site, mine, road save, departure/arrival, camera and carriage tests
  remain required. Expanded site-work fixtures use actual paginated hit bounds.

`crownless_carriage --capture-continuous-road <existing-directory>` records the
same eight-second input regression, 120 PNG frames sampled at 15fps. It starts
from a generated road departure fixture; it is not a human-controlled complete
mine out-and-back playthrough. The read-only Road travel workflow retains a
video, frames, exact source revision, SHA-256 manifest and native test logs.

Physical-phone/Safari observation, live high-latency multiplayer footage and
unrelated passenger seating/legacy scene publishers are outside this cut.
