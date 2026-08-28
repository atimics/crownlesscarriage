# Procedural Creature Pipeline

## Production result

The creature library expands the human cast with three distinct visual and
motion families, backed by reusable 2-, 4-, 6-, and 8-leg rig templates:

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
same persistent planted contacts, joint solves, and flexor/extensor model as
the rest of the cast. Goblins and the dragon keep their generated GLBs as
editable shape references while the game assembles their visible forms around
live bones.

| Runtime template | Legs | Chain | Support rule | Current use |
| --- | ---: | --- | --- | --- |
| Biped | 2 | thigh and lower leg | at least 1 planted | goblins and people |
| Quadruped | 4 | upper and lower leg | at least 3 planted | horse, cow, dragon |
| Hexapod | 6 | upper and lower leg | tripod support | ready for six-leg species |
| Octopod | 8 | upper, middle, and terminal leg | at least 6 planted | ready for eight-leg species |

The six- and eight-leg templates are complete runtime skeletons, not animation
labels. They have independent leg states, joint chains, opposing muscles,
support budgets, and collision samples. A new creature can attach its body art
to either template without changing the locomotion engine.

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

`CcCreatureRigController` owns the live `CcLimbRig` and `CcBiomechRig` state.
It keeps a planted foot fixed in world space until that leg starts its swing.
Swinging feet follow a smooth path whose speed and acceleration both settle at
contact. The gait scheduler limits how many legs can lift together and checks
the remaining support shape before allowing another lift. This prevents the
old skating motion where every foot followed the body throughout a step.

`CcCreatureRigPoseResolve` remains available for held poses and previews.
Goblins use the biped contact layout. Horses, cows, and the dragon use
separately proportioned quadruped layouts. Hexapods use alternating tripods;
octopods use staggered three-segment legs. Each joint has opposing flexor and
extensor muscles, and their activation changes the visible muscle envelope.
`CcQuadrupedPoseResolveFromRig` maps the live horse and cow results onto the 19
exported skin bones while keeping the authored neck, head, body, and tail
anchors. The generated C catalog keeps those assets in sync with the manifest.

Each healthy bone link can also be converted into overlapping collision
spheres by `CcRobotLimbPointSpace`. This gives every supported body plan one
continuous point-space proxy instead of collision only at the joints.

The gait is analytic and deterministic. Animals and people do not need a
neural net, training data, or an inference runtime for normal game movement.
A learned controller may be useful later for unusually difficult terrain or
new reactive tricks, but it should add to this tested controller instead of
replacing the reliable baseline.

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
./crownless_carriage --capture-creatures horse horse.png
./crownless_carriage --capture-creatures cow cow.png
./crownless_carriage --capture-creature-reel goblins goblins/frame
./crownless_carriage --capture-creature-reel dragon dragon/frame
./crownless_carriage --capture-creature-reel horse horse/frame
./crownless_carriage --capture-creature-reel cow cow/frame
```

Each reel command records 45 deterministic gameplay frames at 15 frames per
second, ready to assemble into a three-second clip. The older `animals` name
continues to capture the settlement cow for compatibility.
