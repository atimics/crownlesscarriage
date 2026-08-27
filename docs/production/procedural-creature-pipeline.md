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

All forms are generated from code rather than hand-edited exports. The shipped
files are static GLBs in held motion poses, matching the deliberate stepped
cadence used by the ambient human cast.

## Generated artifacts

- Source library: `assets/blender/crownless_creature_library.blend`
- Runtime exports: `assets/exports/creatures/creature_*_v01.glb`
- Contract manifest: `assets/creature_manifest.json`
- Family preview: `assets/previews/creatures/creature_family_sheet.png`
- Generator: `tools/blender/build_creature_library.py`
- Validator: `tools/blender/validate_creature_library.py`

Generate and verify the library from the repository root:

```sh
make blender-creature-assets
make blender-creature-assets-check
```

The macOS application bundle copies the manifest and every creature GLB through
the existing asset packaging target.

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

Every export contains one mesh, one `MAT_CREATURE_INDEXED` material, no skin,
and no animation tracks. `COLOR_0` stores one of nine semantic palette indices
plus broad value and fold channels:

`skin`, `secondary`, `hide`, `cloth`, `leather`, `horn`, `metal`, `accent`,
and `eye`.

The manifest records one of three gait contracts:

- `npc_stepped`: select the goblin pose with the current biped gait phase.
- `quadruped_stepped`: select the horse or cow pose with the tested
  `CC_MORPHOLOGY_QUADRUPED` phase.
- `dragon_authored`: select a named dramatic state rather than treating the
  dragon as ambient livestock.

This first asset expansion intentionally stops at the packaged model contract.
Scene spawning should consume these manifest fields instead of adding another
hard-coded path table to `cc_local3d.c`.

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
