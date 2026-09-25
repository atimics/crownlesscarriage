# Carriage horse tack and read review — 2026-09-24

Branch: `art/carriage-horse-tack`. Generator changes are in
`tools/blender/build_creature_library.py` (`build_quadruped` and the new
`add_pony_bridle` / `add_pony_collar` / `add_pony_saddle_pad` helpers, plus
new entries in `quadruped_bone_for_part`). No new bones, no texture, no
material added; the existing 19-bone quadruped rig and 9-slot indexed
palette are unchanged. Cow and sheep code paths are untouched.

## What changed on the horse

1. **Hindquarter and leg read.** The rump is flatter and wider, with two
   haunch bulges added on either side of centerline (`PONY_Haunch_L/R`),
   so the hindquarters read as a pair of quarters instead of one ball.
   The hind stance is 0.11 units wider than the fore stance. Each hind leg
   gets a small hock joint mass (`PONY_Hock_*`) where the gaskin turns into
   the cannon, so the leg reads as bent and working rather than a single
   straight taper hidden under the rump.
2. **Tail.** The flowing tail's first two rings start further back and are
   fuller, so the visible part of the tail starts as a real dock instead of
   only the tapered tip clearing the rump.
3. **Tack**, all bound to existing quadruped bones the same way other parts
   are bound (via `quadruped_bone_for_part`):
   - **Bridle** (`add_pony_bridle`): browband, cheek straps, noseband, and
     two buckle accents, bound to the `head` bone so it rides along with
     the eyes and ears (including the big-eyed face exaggeration pass).
   - **Breast collar** (`add_pony_collar`): a crest strap from the neck to
     the chest, a throat strap, and a trace guide that ends in a metal
     trace ring. The crest binds to `neck`, the rest to `chest`.
   - **Saddle pad / back band** (`add_pony_saddle_pad`): a padded box on
     the withers with a girth strap on each side, bound to `body`, sitting
     in the clear saddle between the shoulder and haunch bulges.

Horse triangle count: 2240 -> 2756 (validator limit is 3200; cow, sheep,
and every dragon stage are unchanged). `validate_creature_library.py`
passes with no failures.

## The trace socket (goal item 2)

The collar's trace point is built at the exact same model-space point the
runtime harness in `road_book.inc` already reads off the chest bone
(`RoadPonySkinPoint`'s `(side*0.47, 1.15, 0.08)` in `CC_QUADRUPED_CHEST`
space, converted through the glTF Y-up export axes to this generator's
Z-up `(x, y, z)` as `(side*0.47, -0.08, body_z+0.17)`). The crest and
throat straps use the same neck (`(0,1.57,0.65)` -> `(0,-0.65,body_z+0.59)`)
and chest-side (`(side*0.44,1.19,0.64)` -> `(side*0.44,-0.64,body_z+0.21)`)
points `DrawRoadHorseHarness` already draws from. Because both the baked
collar and the dynamic hitch trace/harness cylinders read off the same
points on the same bones, no socket constants in `road_book.inc` needed to
move; `TestPonyHarnessAttachment` and the `--carriage-graphics` draw-twice
checks still pass unchanged.

## The travel hitch gap (goal item 3)

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
here.

## Frames

All frames are the shipped binary at game size
(`out/build/play/crownless_carriage.app`), full color and full palette
(no debug overlays other than the normal HUD). "Before" is the previous
`main` (`origin/main` at the time this branch was cut); "after" is this
branch's build.

- `creatures-horse-{before,after}.jpg` — `--capture-creatures horse`, the
  close travelling shot from behind that motivated this review. The
  saddle pad is now clearly visible on the back; `compare-rear-close.jpg`
  crops the same shot side by side.
- `travel-{before,after}.jpg` — `--capture-travel`, the side-on travel
  road. Confirms the hitch gap is unchanged (see above) and the coat and
  general silhouette still read at travel distance.
- `pony-encounter-{before,after}.jpg` — `--capture-pony-encounter`
  (Marmalade). Confirms the close-up chibi charm is preserved: same big
  eyes, same face, now with a simple readable bridle that does not read
  as realistic.
- `road-{before,after}.jpg` — `--capture-road`, the bridge encounter at a
  three-quarter profile where the runtime-drawn harness already shows;
  `compare-road-collar-close.jpg` crops the chest/collar area, where the
  new saddle pad and a metal trace-ring accent are visible next to the
  existing procedural leather straps.
- `animal-skins-{before,after}.jpg` — `renderer_regression_tests
  --graphics`'s `animals` sheet: all 7 in-game coat colors, 4 turns, 2
  gait poses, at the actual runtime palette (not the offline preview
  colors). `compare-animal-skins-top.jpg` crops the first two coat rows
  for a closer side-by-side. This is the clearest small-scale check that
  the tack and hindquarter read hold up across every coat color and turn.

## Verification run

- `python3 tools/blender/validate_creature_library.py` — pass, 0 failures.
- `cmake --preset play && cmake --build --preset play` — succeeds.
- `ulimit -s 65520; ctest --preset play` — 236/237 passed. The one
  failure, `scrivendays_calendar_and_evidence`, is pre-existing on
  `origin/main` and unrelated to this change: it fails because of the
  `CC_SIM_NEWEST_LEGACY_SCHEMA` bump to 115 in "Mine exploration uses town
  clicks and first-person views (#923)", which landed on `main` before
  this branch and touches none of this PR's files. Confirmed it also
  fails standalone (`./scrivendays_tests`) on an unmodified checkout at
  that commit.
- `renderer_regression_tests --carriage-graphics` and `--graphics` — pass,
  including `TestPonyHarnessAttachment` and the repeated-draw
  no-gait-mutation checks.
- No "clamped to bind pose" warnings in any capture log.
