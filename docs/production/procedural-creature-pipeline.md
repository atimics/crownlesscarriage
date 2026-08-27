# Procedural Creature Pipeline

## Production result

The creature library expands the human cast with three distinct visual and
motion families:

- Goblins are short bipeds with oversized ears, heads, hands, and carried
  goods. Scavenger, raider, and tribute-bearer variants make the lair economy
  readable before any label appears.
- Horses and cows share one quadruped locomotion contract but not one body.
  Horses use a high shoulder, arched neck, mane, long lower legs, and open leg
  gaps. Cows use a deep barrel, low head, horns, short legs, and an udder.
- The dragon starts from four planted limbs but has an authored neck, jaw,
  tail, crown horns, back spines, and wings. It is a unique world actor, not a
  scaled farm animal.

All forms are generated from code rather than hand-edited exports. The horse
and cow each ship as one rigged, skinned GLB. Their four legs are driven by the
same planted contacts, two-bone joint solves, and flexor/extensor model as the
rest of the cast. Goblins and the dragon keep their generated GLBs as editable
shape references while the game assembles their visible forms around live
bones.

## Generated artifacts

- Source library: `assets/blender/crownless_creature_library.blend`
- Runtime and shape-reference exports: `assets/exports/creatures/creature_*_v01.glb`
- Contract manifest: `assets/creature_manifest.json`
- Family preview: `assets/previews/creatures/creature_family_sheet.png`
- Generator: `tools/blender/build_creature_library.py`
- Validator: `tools/blender/validate_creature_library.py`
- Runtime rig: `src/locomotion/cc_creature.c`

Generate and verify the library from the repository root:

```sh
make blender-creature-assets
make blender-creature-assets-check
```

The macOS application bundle keeps the manifest and GLBs. The horse and cow
use their skins at runtime. Goblins and the dragon use the procedural runtime
rig, with their GLBs kept as art references.

## Variant grammar

| Variant | Main silhouette rule | Gameplay read |
| --- | --- | --- |
| Goblin scavenger | narrow, low, back-heavy | carries recovered material |
| Goblin raider | forward armored wedge, high spear | leaves the lair to take goods |
| Goblin tribute bearer | broad burden around a bright chest | moves offerings toward the dragon |
| Horse | high shoulder, arched neck, long open legs | travel, carriage, speed |
| Cow | deep barrel, low horn line, short planted legs | food economy and livestock pressure |
| Dragon | long grounded predator, crown horns, folded wings | singular hoard power and retaliation |

Goblins have idle plus eight held walk poses. The horse and cow each have one
skinned asset with 19 shared runtime bones. The dragon has idle, two stalk
poses, threat, and rest. It should not loop like an ambient crowd actor. Its
smaller authored set gives each appearance a clear dramatic purpose.

## Runtime contract

Every export contains one mesh and one `MAT_CREATURE_INDEXED` material.
`COLOR_0` stores one of nine semantic palette indices plus broad value and fold
channels:

`skin`, `secondary`, `hide`, `cloth`, `leather`, `horn`, `metal`, `accent`,
and `eye`.

The manifest records one of three gait contracts. They now select a runtime
skeletal profile rather than a baked mesh sequence:

- `npc_stepped`: select the goblin pose with the current biped gait phase.
- `quadruped_runtime_skin`: load one horse or cow skin and drive its 19 bones
  from `CcQuadrupedPoseResolve`. The horse and cow share bone names and contact
  timing while keeping different proportions.
- `dragon_authored`: select a named dramatic state rather than treating the
  dragon as ambient livestock.

`CcCreatureRigPoseResolve` derives each body from `CcLimbRig` and
`CcBiomechRig`. Goblins use the biped contact layout. Horses, cows, and the
dragon use separately proportioned quadruped layouts. Each joint has opposing
flexor and extensor muscles; their activation changes the visible muscle
envelope. `CcQuadrupedPoseResolve` maps the horse and cow results onto the 19
exported skin bones, while keeping the authored neck, head, body, and tail
anchors. The generated C catalog keeps those assets in sync with the manifest.

The quadruped gait is analytic and deterministic. It does not need a neural
net, training data, or an inference runtime. A learned controller may be useful
later for difficult terrain or reactive behavior, but it should be tested
against this simple gait before it replaces it.

## Gameplay-scale rules

- Judge forms at the fixed adventure camera and 35–80 art pixels, not only in
  Blender close-ups.
- A goblin must remain distinct from a small green human in black silhouette.
- Horse and cow must remain distinct with materials removed.
- Keep space between quadruped legs so foot phases survive point sampling.
- Treat horns, ears, hooves, hands, jaw, wing tips, and carried goods as
  silhouette anchors and exaggerate them when necessary.
- The dragon may fill much more of the frame, but its head, chest, wing, and
  tail masses must still separate at the gameplay camera.
- Add new variants through generator recipes and the manifest. Do not maintain
  hand-edited GLBs outside the source library.

## Validation

The validator rejects missing pose families, reordered palette semantics,
unexpected body-plan contracts, multiple material primitives, absent
`COLOR_0`, wrong skins, missing quadruped bones or weights, baked animation
tracks, excessive triangle counts, and implausible bounds. The family preview
remains the visual gate for silhouette, scale, and palette balance.

Runtime art checks can capture the state-driven settlement compositions:

```sh
./crownless_carriage --capture-creatures goblins goblins.png
./crownless_carriage --capture-creatures dragon dragon.png
./crownless_carriage --capture-creatures animals animals.png
./crownless_carriage --capture-creature-reel goblins goblins/frame
./crownless_carriage --capture-creature-reel dragon dragon/frame
./crownless_carriage --capture-creature-reel animals animals/frame
```

Each reel command records 45 deterministic gameplay frames at 15 frames per
second, ready to assemble into a three-second clip.
