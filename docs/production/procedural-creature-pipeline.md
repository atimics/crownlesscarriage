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

All forms are generated from code rather than hand-edited exports. The GLBs
remain the editable silhouette and palette prototypes. Runtime movement comes
from the same skeletal and muscular foundation as the human cast: planted
contacts, two-bone joint solves, flexor/extensor pairs, and activation-shaped
muscle envelopes. The outer creature shapes are assembled around those live
bones instead of swapping flattened pose meshes.

## Generated artifacts

- Source library: `assets/blender/crownless_creature_library.blend`
- Shape-reference exports: `assets/exports/creatures/creature_*_v01.glb`
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

The macOS application bundle keeps the manifest and GLBs as art references.
The active game renderer resolves each visible creature through the runtime rig.

## Variant grammar

| Variant | Main silhouette rule | Gameplay read |
| --- | --- | --- |
| Goblin scavenger | narrow, low, back-heavy | carries recovered material |
| Goblin raider | forward armored wedge, high spear | leaves the lair to take goods |
| Goblin tribute bearer | broad burden around a bright chest | moves offerings toward the dragon |
| Horse | high shoulder, arched neck, long open legs | travel, carriage, speed |
| Cow | deep barrel, low horn line, short planted legs | food economy and livestock pressure |
| Dragon | long grounded predator, crown horns, folded wings | singular hoard power and retaliation |

Goblins and farm animals each have idle plus eight held walk poses. The dragon
has idle, two stalk poses, threat, and rest. It should not loop like an ambient
crowd actor. Its smaller authored set gives each appearance a clear dramatic
purpose.

## Runtime contract

Every reference export contains one mesh, one `MAT_CREATURE_INDEXED` material,
no skin, and no animation tracks. `COLOR_0` stores one of nine semantic palette
indices plus broad value and fold channels:

`skin`, `secondary`, `hide`, `cloth`, `leather`, `horn`, `metal`, `accent`,
and `eye`.

The manifest records one of three gait contracts. They now select a runtime
skeletal profile rather than a baked mesh sequence:

- `npc_stepped`: select the goblin pose with the current biped gait phase.
- `quadruped_stepped`: select the horse or cow pose with the tested
  `CC_MORPHOLOGY_QUADRUPED` phase.
- `dragon_authored`: select a named dramatic state rather than treating the
  dragon as ambient livestock.

`CcCreatureRigPoseResolve` derives each body from `CcLimbRig` and
`CcBiomechRig`. Goblins use the biped contact layout. Horses, cows, and the
dragon use separately proportioned quadruped layouts. Each joint has opposing
flexor and extensor muscles; their activation changes the visible muscle
envelope. The road scene uses rig-driven horses and food-linked cattle.
Settlement scenes show rig-driven goblins at their lair or raid target, the
tribute bearer during delivery, and the dragon at its lair.

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
`COLOR_0`, skins, animation tracks, excessive triangle counts, and implausible
bounds. The family preview remains the visual gate for silhouette, scale, and
palette balance.

Runtime art checks can capture the state-driven settlement compositions:

```sh
./crownless_carriage --capture-creatures goblins goblins.png
./crownless_carriage --capture-creatures dragon dragon.png
./crownless_carriage --capture-creatures animals animals.png
```
