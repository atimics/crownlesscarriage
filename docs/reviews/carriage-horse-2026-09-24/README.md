# Carriage horse tack and read review — 2026-09-24

Branch: `art/carriage-horse-tack`. Generator changes are in
`tools/blender/build_creature_library.py` (`build_quadruped` and the new
`add_pony_bridle` / `add_pony_collar` / `add_pony_saddle_pad` helpers, plus
new entries in `quadruped_bone_for_part`). No new bones, no texture, no
material added; the existing 19-bone quadruped rig and 9-slot indexed
palette are unchanged. Cow and sheep code paths are untouched.

This review went through a first pass and then a revision after human
review at game size flagged three problems. Both passes are recorded here;
the "Revision" section is the current state.

## What changed on the horse (first pass)

1. **Hindquarter and leg read.** The rump is flatter and wider, with two
   haunch bulges added on either side of centerline, so the hindquarters
   read as a pair of quarters instead of one ball. The hind stance is 0.11
   units wider than the fore stance. Each hind leg gets a small hock joint
   mass where the gaskin turns into the cannon.
2. **Tail.** The flowing tail's first two rings start further back and are
   fuller, so the visible part of the tail starts as a real dock instead of
   only the tapered tip clearing the rump.
3. **Tack**, all bound to existing quadruped bones the same way other parts
   are bound (via `quadruped_bone_for_part`): a bridle on the head, a
   breast collar on the neck/chest, a saddle pad and girth on the body.

## Revision: three problems found at game size

The tack bound correctly, the harness/trace alignment held up, and the
hitch-gap analysis stood, but three things did not read well in the actual
game-size captures:

1. **The saddle pad was the loudest thing on the horse** — a bright
   yellow/cream box that read like a sticker. Fix: it now uses the
   `leather` palette slot (the same slot as the collar and hooves) instead
   of `cloth`, and is smaller (`0.40 x 0.24 x 0.07` down from
   `0.56 x 0.32 x 0.12`). See `creatures-horse-before/after.jpg` and
   `compare-rear-close.jpg`: the pad is now a dark, low-contrast patch, not
   a sticker, and the eye goes to the collar and head first.

2. **The collar and bridle barely read in the side view, and the bridle
   read as goggles on Marmalade's close-up.** Two separate bugs:
   - The collar's leather bands are now noticeably wider (crest strap
     `0.052-0.048` -> `0.066-0.060` radius, etc.) and carry **two** metal
     rings (a mid-collar hame ring plus the trace ring) instead of one, so
     it reads as hardware against the coat at side-view size
     (`compare-road-collar-close.jpg`).
   - The bridle's original cheek strap ran from the browband to the
     noseband along the side of the face. On this chibi head the eye
     nearly fills that side of the head, so *any* strap between brow and
     nose in that area reads as a lens rim around the eye — moving the
     strap further out only made a bigger circle around a bigger eye,
     never clearing it (tried three routings; see the commit history on
     this file's PR for the rejected attempts). The fix that actually
     worked: drop the side cheek strap entirely and connect the browband
     to the noseband with a single strap straight down the nose bridge, on
     the centerline between the eyes, with a buckle top and bottom. Nothing
     bridle-related sits off-center any more. `compare-marmalade-face-close.jpg`
     shows the eyes now completely clear, with only a small buckle accent
     near each side of the muzzle.
   - A second, unrelated bug surfaced in the same close-up: the collar's
     crest strap was bound to the `neck` bone while its other end was bound
     to `chest`. A single mesh segment can only rigidly follow one bone, so
     whenever the neck bowed for the "meet the pony" encounter pose, the
     neck-bound end swung independently of the chest-bound end and cut
     across the face — this, not the bridle, was the real source of the
     "goggles" look in the encounter shot. Fix: the whole collar, including
     the crest, now binds to `chest` only. The chest barely moves compared
     to the neck, so the collar stays put across every pose tested
     (idle, travel, and the pony encounter's head-down greeting).

3. **The rear/three-quarter view was still mostly a pink blob.** The haunch
   and hock masses were there in the mesh but invisible: `PONY_RoundRump`
   and the haunch bulges are large overlapping ellipsoids with no boolean
   union, and the smaller haunch/seam shapes were sitting *inside* the much
   larger rump and chest masses, never reaching a visible surface — adding
   value contrast by choosing a dark material only works if the geometry
   carrying it actually reaches the rendered surface. Two fixes:
   - The haunches now extend further back than the round rump's own
     centerline reach, so they are not recessed behind it.
   - A new **croup ridge** (`PONY_CroupRidge`), a small box in the `eye`
     palette slot (a guaranteed-dark value regardless of lighting angle,
     the same trick already used for the tail tip and the hind cannons),
     sits proud of the rump's own top surface just ahead of the tail —
     the same way the saddle pad already worked, since a box sitting above
     the local surface renders reliably where an embedded ellipsoid did
     not. It gives a visible dark line down the middle of the hindquarters
     from directly behind (`compare-rear-close.jpg`), and combines with the
     hind cannons/hocks (also `eye` slot, unchanged from the first pass) so
     the near hind leg reads as a dark "sock" breaking away from the coat.

   Also unchanged from the first pass and re-verified after this revision:
   the hind cannon-to-hoof and hock use the `eye` slot (a guaranteed-dark
   value) rather than `hide`, and the tail's back ~40% of its rings do too,
   so the leg and tail separation shown in `compare-animal-skins-top.jpg`
   (all 7 coat colors) still holds.

Horse triangle count: 2240 -> 2740 (validator limit is 3200; cow, sheep,
and every dragon stage are unchanged — confirmed unaffected again after the
revision). `validate_creature_library.py` passes with no failures.

## The trace socket (goal item 2, unaffected by the revision)

The collar's trace point is built at the exact same model-space point the
runtime harness in `road_book.inc` already reads off the chest bone
(`RoadPonySkinPoint`'s `(side*0.47, 1.15, 0.08)` in `CC_QUADRUPED_CHEST`
space, converted through the glTF Y-up export axes to this generator's
Z-up `(x, y, z)` as `(side*0.47, -0.08, body_z+0.17)`). The crest and
throat straps use the same neck-crest point (`(0,1.57,0.65)` ->
`(0,-0.65,body_z+0.59)`, now used only as a coordinate, not a bind bone —
see above) and chest-side point (`(side*0.44,1.19,0.64)` ->
`(side*0.44,-0.64,body_z+0.21)`) `DrawRoadHorseHarness` already draws
from. Both the baked collar and the dynamic hitch trace/harness cylinders
read off the same points, so no socket constants in `road_book.inc` needed
to move; `TestPonyHarnessAttachment` and the `--carriage-graphics`
draw-twice checks still pass.

## The travel hitch gap (goal item 3, unaffected by the revision)

`CcLocalRoadHorseLongitudinalOffsetInternal()` (5.55 units) sets how far
ahead of the carriage base the hitched team stands, both in ordinary
travel and in the bridge encounter. It looked like a placement constant,
but it is gameplay-coupled, not just cosmetic:

- `tests/local_movement_tests.c` (`TestRoadBridgeSupport`) asserts this
  exact value against the authored bridge causeway/deck support surfaces
  (`CcLocalRoadCheckpointSurfaceYInternal`), so the team lands on the
  bridge's supported rectangles.
- `tests/client_bridge_scene.inc` asserts
  `CcLocalRoadEncounterCarriageXInternal() +
  CcLocalRoadHorseLongitudinalOffsetInternal() + 1.0f <
  local.agent.position.x`, i.e. "the horse team must stop behind the
  hero" for the bridge ambush's targeting and combat layout to work.

The same constant drives both the cosmetic travel spacing and this
combat/level-geometry placement, so it is reported here rather than
changed. Shortening the visual gap safely would need a second, travel-only
constant decoupled from the bridge-encounter and bridge-support checks;
that is a small but separate change from this PR's scope, and is not made
here. `travel-before/after.jpg` confirm the gap is visually unchanged.

## Frames

All frames are the shipped binary at game size
(`out/build/play/crownless_carriage.app`), captured directly (no sandbox
workaround needed this pass), full color and full palette (no debug
overlays other than the normal HUD). "Before" is `origin/main` at the time
this branch was cut; "after" is this branch's current, revised build.

- `creatures-horse-{before,after}.jpg` / `compare-rear-close.jpg` —
  `--capture-creatures horse`, the close travelling shot from behind that
  motivated this review. The saddle pad is now a dark, low-contrast patch
  (not a sticker), a dark croup ridge line runs down the middle of the
  hindquarters, and the near hind leg shows a dark sock.
- `travel-{before,after}.jpg` — `--capture-travel`, the side-on travel
  road. Confirms the hitch gap is unchanged and the coat/silhouette still
  read at travel distance.
- `pony-encounter-{before,after}.jpg` / `compare-marmalade-face-close.jpg`
  — `--capture-pony-encounter` (Marmalade). Confirms the close-up chibi
  charm is preserved — same big eyes, same face — and that the bridle no
  longer crosses the eyes (the revision's second fix).
- `road-{before,after}.jpg` / `compare-road-collar-close.jpg` —
  `--capture-road`, the bridge encounter at a three-quarter profile. The
  wider collar bands and the two metal rings are visible next to the
  existing procedural leather straps; the saddle pad and hind sock read
  clearly too.
- `animal-skins-{before,after}.jpg` / `compare-animal-skins-top.jpg` —
  `renderer_regression_tests --graphics`'s `animals` sheet: all 7 in-game
  coat colors, 4 turns, 2 gait poses, at the actual runtime palette (not
  the offline preview colors). This is the clearest small-scale check that
  the tack and leg/tail darkening hold up across every coat color and
  turn, and that the bridle no longer shows on the front-facing views.

## Verification run (after the revision)

- `python3 tools/blender/validate_creature_library.py` — pass, 0 failures,
  horse at 2740 triangles.
- `cmake --preset play && cmake --build --preset play` — succeeds.
- `ulimit -s 65520; ctest --preset play` — 238/241 passed after rebasing
  onto the current `origin/main` (44 commits ahead of this branch's
  original base, mostly a "living world" census/districts/clock merge
  train) and re-running for the revision. The three failures are all
  pre-existing simulation/persistence determinism issues unrelated to this
  PR's files (`tools/blender/build_creature_library.py` and generated
  assets only): `scrivendays_calendar_and_evidence` (a
  `CC_SIM_NEWEST_LEGACY_SCHEMA` bump in an unrelated merged PR),
  `crisis_contested_succession`, and `long_history_recovery` (both
  `CcSimHash` mismatches in `succession_tests`/`long_history_recovery_tests`,
  pure simulation/persistence code this PR never touches). All three
  reproduce standalone, deterministically, with no Blender/creature/road
  code in their call path.
- `renderer_regression_tests --carriage-graphics` and `--graphics` — pass,
  including `TestPonyHarnessAttachment` and the repeated-draw
  no-gait-mutation checks (the collar's re-bind to `chest` does not change
  gait/controller state, only its own static bind).
- No "clamped to bind pose" warnings in any capture log.
- Frames captured with the binary invoked directly (no `launchctl` or other
  sandbox workaround); the window server was reachable this pass.
