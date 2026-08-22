# Engine-driven hero rig

The modular hero now takes its motion from `CcHumanoidGait`. Blender owns the
bind pose, meshes, materials, and component-to-bone assignments; it does not own
the runtime motion.

## Runtime contract

`CcHumanoidSkinPoseResolve()` converts the physical pose landmarks into a
stable 18-bone armature:

```text
root -> pelvis -> spine -> chest -> neck -> head
                         -> upper_arm.L -> forearm.L -> hand.L
                         -> upper_arm.R -> forearm.R -> hand.R
        -> thigh.L -> shin.L -> foot.L
        -> thigh.R -> shin.R -> foot.R
```

It also resolves 16 named equipment sockets for the head, chest, back,
shoulders, forearms, hands, belt, hips, shins, and feet. Bone and socket poses
are world-space, deterministic, orthonormal transforms. They are valid for
locomotion, guard/strike, climb, swim, fall/ragdoll, and recovery because every
state already produces the same `CcHumanoidPose` landmarks.

Four additional `cape.0` through `cape.3` bones are deliberately outside that
stable body contract. Their transforms come from `CcLocalCapeState`: a
five-particle Verlet chain with fixed segment lengths, bend resistance, torso
collision, gravity, water buoyancy, damping, and velocity-driven air drag.

## Blender export

The engine export is regenerated from the `.blend` source with:

```sh
make blender-hero-engine
```

This rebuilds the modular component library first, authors the action source
from those components, validates the rig contract, and then creates the engine
GLB. The shipped hero is therefore no longer maintained as a separate mesh
implementation.

This produces:

- `assets/exports/hero/crownless_hero_engine_rig_v01.glb`
- `assets/exports/hero/crownless_hero_engine_rig_v01.json`

The export keeps every authored object and its component provenance in the JSON
manifest, then joins those objects into one runtime skin before writing the GLB.
Materials remain separate primitives inside that skin, while vertex groups and
armature weights survive the join. Armor, boots, bracers, gloves, and equipment
remain rigid-weighted. The four torso shells and cape retain blended weights;
the cape blends its rows across four cloth bones. The client creates a one-frame
runtime pose from the body and cloth simulation every draw and applies it
through raylib skinning. This preserves modular authoring without paying one
animated vertex-buffer update per authored object.

The client enforces a 32-primitive runtime budget. The release render benchmark
also requires exactly one high-detail player skin update, at least one
low-detail NPC, and no more than 32 animated mesh uploads. An accidentally
unconsolidated export therefore fails the production gate instead of quietly
shipping a large CPU-skinning regression.

## Gameplay-scale visual LOD

The Blender skin is authored for turntables and close inspection: it contains
roughly 21,000 triangles and 19 material primitives. At the fixed street camera
the hero is only 35–50 pixels tall, so the runtime remaps those materials into
three dominant value masses: warm skin, middle-value teal/oxblood costume, and
dark limbs/hair. Gold remains a small crown and equipment accent.

The version 0.9 source recipe keeps the waist and hips compact but strengthens
the shoulder line, limbs, hands, and boots. Small rivets, knuckles, greave
ridges, stacked torso lames, and the boot instep strap remain removed because
they collapse into noise at the half-resolution target. The resulting runtime
skin retains the crown, hair, deliberately asymmetric pauldron and cape, broken
cuirass mark, and satchel as its identity anchors.

Normal play keeps the articulated Blender silhouette and gives it a slight
horizontal presentation gain so the arm, crown, and cape gaps stay open at
isometric scale. A shared character shader gives the hero and procedural cast
one shadow band, one light band, a sparse painted highlight, and stable colored
inside-silhouette ink. The complete world renders into a 457 x 285 target
and point-scales into the 914 x 570 viewport, so the hero, NPCs, authored props,
procedural scenery, and shadows share one stable pixel grid. Labels and UI draw
after the upscale at display resolution. Pixel character therefore comes from
the camera rather than a hero-only effect.
The hero's dark face marks and clean material values are kept free of internal
dithering so the eyes, brows, and mouth hold together after that scene filter.
Those landmarks are authored
as deliberately broad graphic shapes with a mirrored brow line and asymmetric
hair silhouette; they favor recognition over miniature realism. The
procedural humanoid remains only as a graceful fallback if the authored skin
cannot load; both representations use the same `CcHumanoidSkinPose`.

The explicit character distance ladder, portraits, expression states, role
movement signatures, and visual acceptance pass are documented in
`docs/production/character-readability.md`.

## Verification

```sh
cmake -S . -B build -DCC_BUILD_CLIENT=ON -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
build/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-walk-cycle assets/previews/hero/engine/engine_physics_walk
```

`humanoid_skin_tests` guards bone names, hierarchy, landmark mapping,
determinism, normalized rotations, and valid transforms under physical walk,
strike, and swim simulation. `local_movement_tests` additionally guards cape
initialization, attachment, segment lengths, finite state, body response, and
determinism.

## Remaining fidelity work

The cape currently simulates its centerline and deforms the full cloth width
from those four bones. A later pass can replace that chain with a small cloth
grid for lateral folding, edge flutter, and self-collision. Armor remains
hard-weighted by design so plates do not squash.
