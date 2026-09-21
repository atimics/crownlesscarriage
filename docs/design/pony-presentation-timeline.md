# Pony presentation timeline (GFX-03)

This note records the agreed design for the pony update/render split from the
20 September 2026 graphics report, so the work can continue from a clean state.
Change 1 of that report (GFX-01/02 and the `TestTravelForestCameraTurn` fixture)
is merged as #859: `PublishCarriagePose` publishes position, heading, and
travelled as previous/current samples with render blends, and the storybook
camera, carriage, crew, and wheels read the render values.

## Problem

`CcLocalCreatureGaitsFixedStepInternal()` advances each gait controller and
stores one pose. `CcLocalWorldUpdate()` interpolates the agent and the course,
but has no creature-pose interpolation.

`DrawRoadHorseTeam()` and four other draw sites (`road_book.inc` lines 175,
319, 371, 1112, and 1840) call `CcLocalCreatureGaitPoseInternal()`
(`asset_loading.inc:1907`) while drawing. That function is not a getter: it
writes `cache->command`, `last_clock`, and `requested_gait`, and can step the
controller. Because local fixed stepping happens before the new journey
position is published, the controller can step toward the previous rendered
target.

Moving the draw origin alone is not a fix. `CreaturePosePointForDraw()`
reconstructs the cached controller-world point and expresses it relative to a
new draw origin; reapplying that origin during model drawing cancels the
apparent movement. In a translation-only example,
`origin + (cached_world - origin) == cached_world`. Interpolating the carriage
root does not interpolate the pony's cached world pose.

## Plan

1. Split `CcLocalCreatureGaitPoseInternal()` into a read-only pose function
   (returns the cached pose transformed for draw, no writes) used by all five
   draw sites, and a separate command publication function.
2. Call the publication from the update path *before*
   `CcLocalCreatureGaitsFixedStepInternal()`, so the controller steps toward
   the current target instead of the previous frame's.
3. Carry previous/current creature poses on the same render timeline as the
   carriage, using the same alpha source as the #859 render blends.

Known blocker: the pony harness targets (ground position plus yaw) are
currently derived from the carriage pose at draw time. Moving publication into
the update path means computing those targets in the update path. This is a
real refactor, not a rename.

## Acceptance

Separate command publication, fixed-step simulation, and read-only rendering.
Preserve planted world-space contacts; indiscriminately interpolating every
hoof is not an acceptable cure for foot sliding.

Cover walk, trot, canter, halt, resume, turns, scene reanchor, valid team
swaps, and arrival. Drawing the same frame twice must not change queued
controller commands or animation state; this read-only render contract is
stronger than checking phase alone. Explicitly verify that every transition in
which the team visibly moves continues to receive animation updates.

Keep the existing frame test's determinism check (it submits a target before
stepping and compares final results across frame rates), and add client-order
and frame-by-frame relative-motion checks.

## Next step before this refactor: GFX-04

The wheel rotation fix is one line and independent of the pony split. The
wheel-radius helper returns model-space radii (0.81 and 0.63) while the model is
drawn at `CARRIAGE_ASSET_SCALE == 0.92`, and rotation is traveled world
distance divided by the unscaled radius. After one displayed circumference the
wheel turns 331.2 degrees instead of 360, an 8% under-rotation. Convert
distance and radius into the same coordinate system: divide by
`radius * CARRIAGE_ASSET_SCALE`, use the render travelled from #859, preserve
signed distance, and define spoke phase at route and scene handoffs.
`travel_animation_tests.c` checks internally consistent distance/radius
arithmetic against the unscaled radii, so it cannot catch this; test the final
rendered wheel's world-space radius, with reverse travel and both axle sizes.
