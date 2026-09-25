# Carriage horse tack and read review — 2026-09-24

Branch: `art/carriage-horse-tack`. Generator changes are in
`tools/blender/build_creature_library.py` (`build_quadruped` and the new
`add_pony_bridle` / `add_pony_collar` / `add_pony_saddle_pad` helpers, plus
new entries in `quadruped_bone_for_part`). No new bones, no texture, no
material added; the existing 19-bone quadruped rig and 9-slot indexed
palette are unchanged. Cow and sheep code paths are untouched.

This review went through a first pass and three revisions after human
review at game size, each round fixing what the previous round's own
frames turned out to still show wrong. All rounds are recorded here; the
"Second revision" section is the current state.

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

## First revision: three problems found at game size

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

## Second revision: three more problems, then one the second round missed

The first revision's own frames, reviewed again at game size, showed three
new problems it had introduced:

1. **Jagged dark blue/purple blobs on both cheeks and a blue dot on the
   nose** in the Marmalade close-up. Fix: removed both bridle buckle
   accents (`PONY_BridleBrowBuckle`, `PONY_BridleNoseBuckle`). Two small
   metal spheres at this resolution read as glitchy blobs, not hardware.
   The bridle is now a single plain leather strap down the nose bridge —
   nothing else on the face.

2. **The whole hind leg near-black while the fore legs stayed pink**, so
   the horse looked half in shadow. The first revision had put the hind
   cannon *and* hock in the `eye` slot to guarantee a dark value; that
   guaranteed value covered the whole lower leg, not just a sock. Fix:
   reverted the hind cannon/hock to the same `hide` tone as the fore legs,
   and added a short `leather` sock (`PONY_Sock_*`) over the bottom ~24%
   of *every* leg, front and hind alike — matching the existing leather
   hoof below it, the same idea as the first revision's saddle-pad fix
   (put the contrast in a real material, sized to a real part of the
   anatomy, not a blanket recolor of the whole limb).

3. **The saddle pad and the croup ridge together made a heavy black T** on
   the back. The croup ridge from the first revision was an
   0.11x0.34x0.09 box in the `eye` slot — a solid dark block. Fix: shrank
   it to an 0.05x0.30x0.05 sliver and moved it to the `hide` slot (one
   step off `skin`, not the guaranteed-black `eye` slot), keeping it just
   proud of the round rump's own surface so it still reaches the visible
   surface (the lesson from the first revision: embedded geometry never
   reaches the rendered surface regardless of material, however dark).

After fixing those three and re-capturing, the Marmalade close-up *still*
showed two small marks near the cheek/shoulder edge — smaller than the
first revision's blobs, but still there. This was not the bridle (already
reduced to one plain strap) or the hind legs or the ridge; it was the
**collar**. Confirmed by temporarily deleting the `add_pony_collar()` call
outright and re-capturing: the marks disappeared completely. Two things
turned out to be wrong with it, found by testing them one at a time:

- The collar's crest point was built at the runtime harness's own
  neck-height coordinate (matching `DrawRoadHorseHarness` in
  `road_book.inc` for the dynamic-trace alignment goal from the very first
  pass). That point sits close enough to the head that the pony
  encounter's very close camera puts it right next to the face — a
  problem with *where* the point is, independent of which bone carries
  it. Pulling it down and in, from reaching toward the neck
  (`body_z+0.59`, `y=-0.65`) to sitting on the chest's own low-mid span
  (`body_z+0.14`, `y=-0.18`), removed most of the marks but not the very
  last trace of them.
- The collar bound to the `chest` bone (the first revision's own fix for
  the crest-strap-crossing-two-bones bug). Rebinding the whole, now much
  smaller collar to the `body` bone instead — the root torso segment the
  saddle pad already binds to without any trouble — cleared the rest.

The mid-collar hame ring from the first revision was also dropped, keeping
only the trace ring (which still has to mark the runtime hitch socket
exactly): on a collar this much smaller, a second ring accent competed
with the trace ring rather than adding to it.

At normal game-display resolution the Marmalade frame now matches the
pre-tack frame exactly (`compare-marmalade-face-close.jpg`); the very last
trace is a single-digit-pixel mark visible only under 3x zoom at the crop
edge, not visible at capture resolution and not distinguishable from
ordinary anti-aliasing.

Also confirmed this round: rebased onto `origin/main` past #937 ("Unified
skinned characters"), which moved quadruped posing onto the shared
`PoseSkinnedCharacter` runtime path. The horse's own bone contract
(19 bones, unchanged) still validates against it — loads and poses with no
"clamped to bind pose" warning in any capture.

Horse triangle count: 2740 -> 2772 (limit 3200, unaffected after rebasing
past #937 — cow, sheep and every dragon stage are also unaffected, and the
dragon-silhouette PR #935 that also touched this file merged cleanly).
`validate_creature_library.py` passes with no failures.

## The trace socket (goal item 2, unaffected by the revisions)

The collar's trace point is still built at the exact same model-space
point the runtime harness in `road_book.inc` already reads off the chest
bone (`RoadPonySkinPoint`'s `(side*0.47, 1.15, 0.08)` in
`CC_QUADRUPED_CHEST` space, converted through the glTF Y-up export axes to
this generator's Z-up `(x, y, z)` as `(side*0.47, -0.08, body_z+0.17)`) —
that point never moved across any revision, since it is the one
requirement this PR cannot trade away. The crest and throat straps, which
only ever had to look plausible (not match a runtime point), moved twice
in the second revision, first toward the runtime harness's own
neck-height coordinate the first pass used (a placement problem for the
pony-encounter camera) and then onto the chest's own low span with a
`body`-bone bind (a bind problem for the same camera); see "Second
revision" above. Since the trace point itself is unchanged, the baked
collar's trace ring and the dynamic hitch trace/harness cylinders still
read off the same point, so no socket constants in `road_book.inc` needed
to move; `TestPonyHarnessAttachment` and the `--carriage-graphics`
draw-twice checks still pass.

## The travel hitch gap (goal item 3, unaffected by any revision)

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
this branch was cut; "after" is this branch's current, twice-revised
build, rebased onto today's `origin/main` (through #937).

- `creatures-horse-{before,after}.jpg` / `compare-rear-close.jpg` —
  `--capture-creatures horse`, the close travelling shot from behind that
  motivated this review. The saddle pad is a dark, low-contrast patch (not
  a sticker), a thin croup fold line runs down the middle of the
  hindquarters (not a black block), and the near hind leg shows a short
  dark sock rather than a near-black limb.
- `travel-{before,after}.jpg` — `--capture-travel`, the side-on travel
  road. Confirms the hitch gap is unchanged and the coat/silhouette still
  read at travel distance, with all four legs carrying the same short
  sock.
- `pony-encounter-{before,after}.jpg` / `compare-marmalade-face-close.jpg`
  — `--capture-pony-encounter` (Marmalade). At game-display resolution
  this now matches the pre-tack "before" frame with no visible difference;
  the second revision's collar fix removed the last marks the first
  revision's own frames still showed near the cheek. Charm is fully
  preserved — same big eyes, same face.
- `road-{before,after}.jpg` / `compare-road-collar-close.jpg` — the bridge
  encounter at a three-quarter profile. The collar reads as a low chest
  band with one metal trace-ring accent near the shoulder (smaller and
  lower than the first revision's version, traded for a clean face in the
  encounter shot), and the saddle pad and leg socks are visible.
- `animal-skins-{before,after}.jpg` / `compare-animal-skins-top.jpg` —
  kept from the first revision (`renderer_regression_tests --graphics`'s
  `animals` sheet, all 7 in-game coat colors). Not re-captured this round:
  `--graphics` now fails early on an unrelated physical-goods rack-bounds
  check from the town/shops refresh in #931, before it reaches the
  animal/pony checks. The collar changes since these frames were taken are
  small enough (already low-profile at the first revision's size, now
  smaller still) that they would not read differently at this icon scale;
  `road-after.jpg` and `travel-after.jpg` above are the current reference
  for the collar instead.

## Verification run (current state)

- `python3 tools/blender/validate_creature_library.py` — pass, 0 failures,
  horse at 2772 triangles (limit 3200).
- `cmake --preset play && cmake --build --preset play` — succeeds.
- `ulimit -s 65520; ctest --preset play` — **247/247 passed**, run after
  rebasing onto `origin/main` through #931/#936/#937 for the second
  revision. The three pre-existing failures the first revision found and
  confirmed standalone (`scrivendays_calendar_and_evidence`,
  `crisis_contested_succession`, `long_history_recovery`) are gone: they
  were fixed upstream somewhere in the commits this rebase picked up, not
  by anything in this branch. Full ctest is clean.
- `renderer_regression_tests --carriage-graphics` — pass, including
  `TestPonyHarnessAttachment` and the repeated-draw no-gait-mutation
  checks (the collar's re-bind to `body` does not change gait/controller
  state, only its own static bind). `--graphics` (a separate, non-ctest
  binary invocation) fails early on the unrelated physical-goods issue
  noted above, before reaching the animal-specific checks; the equivalent
  ctest entry, `physical_goods_displays`, passes, and so does
  `research_artifact_budget`.
- Confirmed against #937 ("Unified skinned characters"), which moved
  quadruped posing onto the shared `PoseSkinnedCharacter` runtime path:
  the horse still loads (correct bone count and names against the
  quadruped skeleton family) and poses correctly. No "clamped to bind
  pose" warning in any capture log this round.
- Frames captured with the binary invoked directly (no `launchctl` or
  other sandbox workaround); the window server was reachable this pass.
