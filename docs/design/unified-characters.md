# Unified characters

Status: goblins migrated (this change). Dragons, NPCs, and a pig follow.

## Decision

Every character and creature is **one Blender-built, skinned GLB**, posed at
runtime by the procedural motion code. We stop using held-pose mesh swaps and
hand-written C creature rigs.

Why:

- One art path. A creature's look lives in `tools/blender/`, not half in
  Blender and half in C draw calls.
- One shading path. Skinned characters draw through the pack-controlled
  skinned shader (`npc_skinned`, style packs from #922), so a style pack
  changes every character the same way.
- One motion path. The motion code already solves feet, bodies and gaits. A
  skin just follows it, at any phase, with no pose snapping.
- Fewer files. Goblins went from 27 GLBs to 3. Dragons will go from 20 to 4.
  NPCs will go from about 170 to one per archetype.

## Today (before this change)

| Actor | Mesh | Posed by |
| --- | --- | --- |
| Hero | one skinned GLB, 24 bones (18 body, 4 cape, 2 hair) | `CcHumanoidSkinPoseResolve` via `DrawHeroSkin` |
| Horse, cow, sheep | one skinned GLB each, 19 bones | `CcQuadrupedPoseResolve[FromRig]` via `PoseQuadrupedCreature` |
| Goblins (3) | 27 held-pose GLBs, never drawn | C draw calls: `DrawGoblinRig`, `DrawGoblinHair` |
| Dragons (4) | 20 held-pose GLBs, never drawn | C draw calls: `DrawDragonRig` and helpers |
| NPCs | 36 body skins + 34 garment modules + 99 held-pose archetypes | body skin by the humanoid solver, modules per bone, archetypes by pose swap |

## Skeleton families

A skeleton family fixes the bone names a GLB must carry and the solver that
poses them. The C side is `CcSkeletonFamily` in
`src/locomotion/cc_character_skin.h`. The manifest names the family per
creature (`"skeleton"` in `assets/creature_manifest.json`), and the generated
catalog carries it to the runtime (`CcCreatureDefinition.skeleton`).

### Rest-pose conventions (all families)

- glTF axes: +y up, +z forward, meters. Blender authors -Y forward, +Z up and
  exports with `export_yup`, so Blender (x, y, z) becomes glTF (x, z, -y).
- The model origin is the ground point under the body. The runtime draws the
  model at the creature's ground position, yaw and scale.
- Each bone's local +y axis points from its head to its tail (Blender's bone
  axis; the exporter keeps it). The runtime reads the rest direction of a
  bone as `bindPose.rotation * (0, 1, 0)`. Bone roll does not matter.
- `.L` bones sit on the -x side of the model and `.R` bones on +x. The motion
  code's limb 0 and arm 0 are `.L`. (Facing +z, -x is the character's own
  right hand; the names follow the solver, not anatomy. Keep them as they are:
  the hero, NPC body skins and the solver all agree.)
- Bind pose = the family solver's idle pose for that body. Then an idle
  character is exactly the mesh as authored, and a posed one only turns rigid
  parts about their joints.

### Humanoid (hero, NPCs, goblins)

18 core bones, the `CcHumanoidSkinBone` enum:

```
root
  pelvis
    spine
      chest
        neck
          head
        upper_arm.L  forearm.L  hand.L
        upper_arm.R  forearm.R  hand.R
    thigh.L  shin.L  foot.L
    thigh.R  shin.R  foot.R
```

- Solver: `CcHumanoidSkinPoseResolve` (`src/locomotion/cc_humanoid_skin.c`)
  turns a `CcHumanoidPose` (joint points) into a world frame per bone: y along
  the bone, x from the body forward (feet: from the body up).
- Hero and NPCs fill `CcHumanoidPose` from the full humanoid gait
  (`CcHumanoidGait`: walk, run, strikes, jumps, climbs, knock-downs).
- Goblins fill it from their two-legged creature rig
  (`CcCreatureRigPose`, profile `CC_CREATURE_RIG_GOBLIN`) through
  `CcHumanoidPoseFromBipedRig` and a body plan (`CcBipedBodyPlan`). The rig
  solves pelvis and legs; the plan adds spine, head and arms (arms swing
  against the leg on the same side; a carrier holds both hands forward).
- **Can goblins share the hero's solver? Yes.** They do now. Goblin
  proportions live in two places that must agree: the body plan in
  `cc_character_skin.c` and `GOBLIN_BIND` in `build_creature_library.py`.
  `character_skin_tests --print-bind` prints the table, and the validator
  checks every exported joint against it.
- Extension bones (later): the hero's `cape.0-3` and `hair.long/rear` are
  extra bones solved by attachment solvers (cloth, hair lag). The family will
  allow named extension bones on top of the 18 core bones. A bone the runtime
  does not know stays at bind pose.

### Quadruped (horse, cow, sheep, pig)

19 bones, the `CcQuadrupedBone` enum:

```
root
  body
    chest
      neck
        head
      upper_leg.FL  lower_leg.FL  hoof.FL
      upper_leg.FR  lower_leg.FR  hoof.FR
    upper_leg.HL  lower_leg.HL  hoof.HL
    upper_leg.HR  lower_leg.HR  hoof.HR
    tail.root
      tail
```

- Solver: `CcQuadrupedPoseResolve` (stateless phase) or
  `CcQuadrupedPoseResolveFromRig` (world gait controller) in
  `src/locomotion/cc_quadruped.c`, per `CcQuadrupedMorphology`.
- The solver gives bone heads and directions in model space. The runtime
  turns each bone by the rotation from the solver's own rest direction to its
  target direction (direction only, no twist).
- Bind pose comes from `quadruped_bone_points` in the builder and is close to
  the solver's rest, not equal. When the pig lands, make the builder print
  the solver's rest instead, like goblins.

### Dragon (designed, not built)

Quadruped plus a longer spine and neck, a tail chain, a jaw and wings. It
must fit the 32 bone matrices of the skinned shaders:

| Group | Bones | Count |
| --- | --- | --- |
| Core | `root`, `body`, `spine`, `chest` | 4 |
| Neck and head | `neck.1` .. `neck.4`, `head`, `jaw` | 6 |
| Legs | `upper_leg`, `lower_leg`, `claw` x FL FR HL HR | 12 |
| Tail | `tail.1` .. `tail.6` | 6 |
| Wings | `wing_arm.L`, `wing_hand.L`, `wing_arm.R`, `wing_hand.R` | 4 |
| **Total** | | **32** |

- Solver: the dragon creature rig (`CC_CREATURE_RIG_DRAGON`, four legs) for
  body and legs, then a new `CcDragonPoseResolve` for the chains: neck and
  tail as follow-through chains that lag the body turn, jaw open for threat,
  wings folded at rest and lifted for threat.
- The deep wyrm keeps the same skeleton with near-zero legs (its legs are
  vestigial); unused bones still count toward 32.
- No room is left. If the dragon needs wing fingers, see "Bone limit" below
  before adding bones.

## GLB contract

A character GLB is valid when:

1. **One skinned mesh, one primitive, one material** (`MAT_CREATURE_INDEXED`).
2. **Bones.** Exactly the family's bone names (core set; extensions later),
   each once. Bone count at most 32.
3. **Weights.** `JOINTS_0` and `WEIGHTS_0` only, so at most 4 weights per
   vertex; every vertex weighted, weights sum to 1. Creatures today use rigid
   one-bone weights per part (low-poly pieces turning about their joints).
4. **COLOR_0 paint contract** (`tools/blender/paint_channels.py`):
   - r: palette slot, `(index + 0.5) / 9` over the material order
     `skin, secondary, hide, cloth, leather, horn, metal, accent, eye`;
   - g: value step (0.25, 0.5, 0.75) from the face normal, with a per-family
     lift (goblins +0.25) and per-part pins (goblin pupils and mouth: 0.25);
   - b: fold strength;
   - a: surface label for surface-labelled assets, else 1.
   The runtime fills the 9 palette colors per draw (`CreaturePalette`), so
   one GLB serves many coats. `accent` carries the dragon court color on
   goblins (hair, scarf, pennant, offering).
5. **Axes and origin** as in the rest-pose conventions above.
6. **No baked animation.**
7. **glTF extras** (Blender custom properties, `export_extras=True`):
   - on the mesh node: `cc_asset_id`, `cc_family`, `cc_variant`,
     `cc_library_version`, `cc_material_contract`, `cc_skin_contract`
     (`CcHumanoidSkinPose` or `CcQuadrupedPose`);
   - on the armature node: `cc_rig_contract`, `cc_skeleton`
     (humanoid rigs), `cc_bone_count`.
   The runtime does not read extras; the manifest is the runtime's source.
   Extras are for tools and people.
8. **Manifest entry** in `assets/creature_manifest.json`: `skinned: true`,
   `skeleton`, `bones` (ordered list), `export`, `material_order`.

`tools/blender/validate_creature_library.py` checks 1-8, plus, for humanoid
skins, that every joint sits at the runtime's idle joint.

### Bone limit

The skinned shaders hold `boneMatrices[32]`. Raising it costs 4 uniform
vectors per bone; WebGL/GLES2 only promises 128 vertex uniform vectors, and
32 bones already use all of them. So: **stay at 32**. If a family truly needs
more, split the character into two draws (for example body and wings) that
share the pose, rather than raising the array for every platform.

## Runtime contract

In `src/client/local3d/asset_loading.inc`:

- `LoadSkinnedCharacter(path, family, cache)` loads one GLB, checks one mesh,
  the family's bone count and every family bone present once, maps each model
  bone to its family bone, and precomputes the inverse rest frame per bone
  (`HumanoidRestFrame` of the bind direction). On any failure it unloads the
  model and warns once.
- Family producers turn motion into per-bone targets in model space
  (`SkinnedCharacterPose`):
  - `PoseHumanoidCreature`: creature rig -> `CcHumanoidPoseFromBipedRig` ->
    `CcHumanoidSkinPoseResolve` -> bone frames;
  - `PoseQuadrupedCreature`: quadruped solver -> bone heads and turns.
- `PoseSkinnedCharacter(cache, pose, variant, controlled, moving)` is the one
  writer: for each model bone, `rotation = delta * bind` (humanoid:
  `delta = target_frame * inverse(rest_frame)`), translation = target head.
  It checks the whole bone map before writing any bone, keeps the bind pose
  for any bone with a non-finite or out-of-range transform and logs
  `"...; clamped to bind pose"` once per variant and bone, then uploads one
  frame with `UpdateModelAnimation`.
- `DrawCreatureGait3D` draws the posed model at position, yaw and scale with
  the indexed palette. If a skinned-only creature (goblin) cannot be posed it
  logs `"CREATURE: variant N not drawn; rig pose failed ..."` once and draws
  nothing; there is no hand-drawn fallback.
- `DrawCreature3D(variant, held_pose, ...)` still accepts a held pose name;
  for a skinned creature it becomes a gait phase and a moving flag.

The hero and NPC body skins still use their own loops (`DrawHeroSkin`,
`DrawNpcBodySkin`). They move onto `PoseSkinnedCharacter` in the NPC step,
together with extension bones.

### Gameplay proportion scales

The hero is drawn with runtime scale hacks: head and hair x1.32, hands x0.90,
and the whole model 0.98 wide and 1.07 tall. Rule from now on: **bake
proportions into the mesh.** A bone scale in the pose breaks the rigid pieces
at their joints and hides the true silhouette from the Blender review
renders. Goblins bake theirs (big head, ears and hands are modeled at game
size, no runtime scale). The hero's scales move into
`export_screen_first_engine_hero.py` when the hero joins the generic path;
then the scale lines in `DrawHeroSkin` go away. The only runtime scale left
is the uniform per-instance draw scale (a smaller goblin in the mine, a whelp
vs a crowned dragon).

## Migration plan

Each step is one PR with before/after frames at game size.

### 1. Goblins (this change)

- Humanoid skeleton, 18 bones; one skinned GLB per variant (scavenger,
  raider, tribute bearer). Shapes, sizes and palette slots follow the old C
  rig.
- Callers draw goblins with a continuous gait phase instead of 8 stepped
  poses.
- Deleted: 24 goblin held-pose GLBs (`creature_goblin_*_{contact,down,passing,up}_{a,b}_v01.glb`)
  and their catalog entries; `DrawGoblinRig`; `DrawGoblinHair`; the goblin
  branches of `DrawCreatureMuscleLimbs`; `BIPED_POSES` and the held-pose
  goblin builder.

### 2. Dragons (includes a silhouette redo)

- Build the dragon family (table above) and `CcDragonPoseResolve`.
- Rebuild the four stages as one skinned GLB each, keeping the #935
  silhouettes (mass, structured wings, readable heads) as the target.
- Map threat, stalk and rest to solver inputs (jaw, wing lift, neck coil),
  not to meshes.
- Deleted: the 20 dragon held-pose GLBs, `DRAGON_POSES`, `DrawDragonRig`,
  `DrawDragonWing`, `DrawDragonMane`, `DrawDragonFace`, `DrawDragonClaws`,
  `DrawCreatureTube` (goblins no longer use it), `DrawCreatureMuscleLimbs`
  and `DrawFarmAnimalRig` if no creature still falls back to them, plus
  `DragonCreatureGrowthScale` special cases that a baked mesh size replaces.

### 3. NPCs (one skin per archetype)

- Wait for the NPC proportion and face work in the NPC generators to land.
- One humanoid skin per archetype (role x body frame), with clothes, hair and
  head joined into the one mesh and weighted to the same 18 bones (plus
  extension bones for capes and long hair).
- Move `DrawHeroSkin` and `DrawNpcBodySkin` onto `PoseSkinnedCharacter`;
  bake the hero's head and hand scales.
- Deleted: `wk_body_skin_*` (36), `npc_module_*` (34), the held-pose
  archetypes `npc_<role><pose>_v01.glb` (99) and their loaders
  (`LoadNpcArchetypes`, `LoadNpcDynamicModules`, the body skin table), the
  per-bone module attachment code, and `NPC_ARCHETYPE_POSE_*`.

### 4. Pig (new quadruped variant)

**Pigs do not exist anywhere in the sim or content today.** Settlements
track cows, sheep and ponies only (`CcSettlement.cow_*`, `sheep_*`,
`pony_*` in `src/sim/cc_sim.h`), with save, hash, food economy and metagame
support. A pig needs:

- sim: `pig_adults`, `piglets`, `pig_condition`, `pig_hunger` on
  `CcSettlement`; food economy (feed, meat); save format and version bump;
  sim hash; metagame census;
- content: `CC_CREATURE_PIG`, `CC_QUADRUPED_PIG` morphology and a creature rig
  profile (or the cow profile at a smaller scale);
- art: a quadruped-family GLB from `build_quadruped`;
- client: roadside and town draws like the sheep, a capture family `pig`.

Deleted: nothing; it is new. It is the first creature built straight on the
contract, so it is the test that the contract is complete.

## Checks

- `make blender-creature-assets blender-creature-assets-check`
- `make blender-exports-check`
- `character_skin_tests` (family tables, idle = bind, model space, arm
  swing), `creature_catalog_tests`, `renderer_regression_tests
  --creature-captures <dir>` (review sheets, including `goblins-walk.png`).
- Captures at game size: `--capture-creatures goblins`,
  `--capture-creature-reel goblins`, `--capture-mine-contest`.
- Watch the log for `clamped to bind pose` and `not drawn; rig pose failed`.
