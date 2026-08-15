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
