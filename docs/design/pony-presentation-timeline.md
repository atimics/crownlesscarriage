# Carriage and pony presentation timeline

Follow-up to the 20–21 September graphics review and PRs #859, #861,
#863, #865, #867, #869 and #870. The integration review is on #870.
GFX-03's remaining local-only road scenes moved to the update path in the
24 September follow-up; see the last section.

## Active world travel

`AdvanceCarriagePresentationSteps` is the production journey update, also
exercised by `--test-carriage-client`. Every local fixed step advances the
fractional journey accumulator, advances the campaign when a journey tick is
due, publishes the carriage sample, publishes world-space pony targets, and
then steps their controllers. An unchanged local step is still a sample.
A frame with several steps therefore retains the actual penultimate local
sample, not the preceding rendered frame. Rendering blends that pair using
the local fixed-step remainder. There is no extrapolation or backwards replay
of an old interval when a fractional journey rate produces no new tick.

Direct placement, departure, arrival and restoration publish position,
heading and distance together through `CcLocalCarriagePublishPose`. Explicit
`presentation_valid` state avoids inferring initialization from coordinates.
The three render accessors use the same physical/presented selection rule.

`CcLocalOpenWorldCarriageTargetsInternal` shares placement, terrain samples,
base lift and arrival scale with the renderer, but takes the authoritative
carriage root and heading. It never substitutes the legacy x/40 road layout.
It includes the encounter pony as well as the hitched team. Controller time
advances locally; accelerated or wrapped campaign time is cosmetic input and
cannot by itself reset the controller. World carriage drawing does not
republish targets. Arrival/departure transitions that bypass the travel input
step publish and advance their team in the pre-draw update stage.

## Pose and scale continuity

Creature caches retain previous/current fixed-step poses and their respective
world origins/yaws. Carriage creatures interpolate points in world space:
a hoof planted at the same world point in both samples stays there while the
body/root moves. Teleport and scene resets invalidate the preceding pose;
ordinary scale changes do not reset it. Other creature users retain their
current-pose default rather than inheriting the carriage interpolation alpha.

`CcCreatureRigControllerSetScale` replaces anatomical dimensions and muscle
rest geometry, preserving phase, requested gait, limb state, swing progress,
health and world contact records. The next physical step resolves the resized
chains and validates their contacts against terrain. It does not promise that
an arbitrarily large resize preserves a physically reachable contact.

Wheel rolling integrates signed distance increments divided by assembly
scale. The existing base world radius already includes the 0.92 asset scale.
Changing scale while stopped does not spin the wheel; a route or explicit
large heading rebase preserves angular phase. A smoothly varying scale uses
trapezoidal integration of inverse scale rather than reinterpreting all prior
distance at the latest radius. Captures without rolling history have an
explicit one-shot fallback, not draw-time state mutation.

## Regression coverage and limits

`carriage_presentation_timeline` exercises production sample publication at
regular/irregular cadences, zero/fractional/normal/accelerated rates, multiple
steps per frame, heading wrap, direct transitions, invalid alpha, scaled
rolling/reversal/rebase, a changing radius, and contact-preserving resizing.
`carriage_client_update_order` exercises the actual journal/route/target step
path. Existing departure and arrival regressions now check displayed heading
and distance as well as the physical root.

The renderer regressions check world target placement, campaign-clock jumps,
read-only pose retrieval and a stationary world hoof under moving pose
origins. `renderer_regression_tests --carriage-graphics <unused-output-path>`
draws the same open-world carriage twice under a real graphics context and
checks that its pony controller/command caches do not change.

This is not a new renderer or a change to the campaign's travel-rate rules.
Full passenger skeletal tilt/seat fit, an ordinary-control out-and-back
video, browser/GPU variation and contact quality during the complete arrival
blend still require visual review. Passing a helper or framebuffer test is
not that review.

## Remaining road scenes moved to the update path (24 September)

The fork, encounter/combat/parley, remote-site and town-street convoy
(arrival/departure) scenes drew a hitched team but published its gait
targets at draw time, the same ordering issue the travel path had before
the fix above. They now publish from the update path too, before that
frame's draw dispatch in `main.c`:

- `CcLocalRoadForkHorseTargetsInternal` shares the fork carriage's turn/
  branch math with `CcLocalDrawFork3D` through a new `ForkCarriagePose`
  helper (built on `ForkSelectedBranchEnd`), the same way
  `RoadTravelCarriageBase` keeps the travelling draw and its publisher in
  step.
- `CcLocalRoadEncounterHorseTargetsInternal` covers the stopped encounter,
  combat and parley carriage (`CcLocalDrawRoad3D`'s `!travelling` case); the
  travelling case is unchanged, still published by
  `CcLocalRoadTravelHorseTargetsInternal`.
- `CcLocalRoadSiteHorseTargetsInternal` covers the remote-site carriage
  (`CcLocalDrawSite3D`), parked or driving the approach road; it is a no-op
  for the dragon cave, which never hitches a team.
- `CcLocalRoadConvoyHorseTargetsInternal` covers the town-street convoy
  (`CcLocalDrawStreet3D`'s `convoy_visible` case) while departing or
  arriving.

Each publisher also covers the "pony on the road" (a met pony walking
beside the team) through a new shared `PublishRoadPonyOnRoadTarget`, so
`DrawRoadCarriage`'s pony-on-road block, like `DrawRoadHorseTeam`, is now a
pure read (`CcLocalCreatureGaitPoseInternal` only). Every caller of both
functions -- travel, fork, encounter/combat/parley, remote site, and the
town-street convoy -- now publishes from the update path, so the old
draw-time guard in `DrawRoadHorseTeam` was removed outright rather than
extended.

`renderer_regression_tests --carriage-graphics` gained one draw-twice check
per scene (`TestForkDrawReadOnly`, `TestEncounterDrawReadOnly`,
`TestSiteDrawReadOnly`, `TestConvoyDrawReadOnly`), alongside the existing
open-world check. All five scenes captured pixel-identical before and after
except arrival, whose small (132/972800 pixel) difference reproduces
between two runs of the same unchanged binary and is unrelated jitter, not
a regression. See `docs/reviews/gfx03-road-scenes-2026-09-24/`.

The stable/cow publishers (`DrawStableHorseTeam` and the roadside/street cow
targets) still publish at draw time; migrating those is separate work, as
before.
