# Carriage and pony presentation timeline

Follow-up to the 20–21 September graphics review and PRs #859, #861,
#863, #865, #867, #869 and #870. The integration review is on #870.

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
The legacy local-only fork, encounter, remote-site and stable/cow publishers
still have draw-time publication; migrating those consumers remains separate
work. Full passenger skeletal tilt/seat fit, an ordinary-control out-and-back
video, browser/GPU variation and contact quality during the complete arrival
blend still require visual review. Passing a helper or framebuffer test is
not that review.
