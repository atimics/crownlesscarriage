# Procedural NPC Archetype Pipeline

## Production result

The ambient cast is no longer assembled from runtime cylinders and boxes in
normal play. Blender generates nine curated role archetypes in one idle and
eight stepped locomotion poses as static GLBs; the client selects one by
`CcNpcRole` and gait phase, then
applies the deterministic `CcNpcAppearance` palette, stature, mass, and
shoulder variation at draw time.
The former primitive renderer remains available when an asset is missing and
when the rig diagnostic overlay is enabled.

This is a hybrid procedural system. The generator owns repeatable anatomy,
poses, garment fit, blank head volumes, and equipment. Curated role
recipes own the few silhouette decisions that need art direction. Runtime code
owns identity, including the shared world/portrait face recipe, eight hair
silhouettes, four headwear families, simulation state, color, scale, placement,
and camera-readable presentation.

## Generated artifacts

- Source library: `assets/blender/crownless_npc_archetypes.blend`
- Runtime exports: `assets/exports/npc/npc_*_v01.glb`
- Contract manifest: `assets/npc_archetype_manifest.json`
- Contact sheet: `assets/previews/npc/npc_archetype_sheet.png`
- Generator: `tools/blender/build_npc_archetype_library.py`
- Validator: `tools/blender/validate_npc_archetype_library.py`

Physics-driven people use a companion rigid-module library:

- Source library: `assets/blender/crownless_npc_dynamic_modules.blend`
- Runtime exports: `assets/exports/npc/npc_module_*_v01.glb`
- Contract manifest: `assets/npc_dynamic_module_manifest.json`
- Generator: `tools/blender/build_npc_dynamic_modules.py`
- Validator: `tools/blender/validate_npc_dynamic_modules.py`

Generate and verify the complete library from the repository root:

```sh
make blender-npc-assets
make blender-npc-assets-check
```

The application bundle copies the manifest and every NPC GLB through the
`crownless_hero_assets` build target.

## Role grammar

| Role | Silhouette rule | Primary equipment |
| --- | --- | --- |
| Wayfarer | balanced V-shape, asymmetric road mantle | mantle, satchel |
| Guard | broad, upright, planted | armor, helmet, polearm |
| Raider | forward, top-heavy, broken symmetry | half armor, crest, weapon |
| Merchant | heavier center, open conversational arm | apron, hat, satchel |
| Laborer | thick limbs and boots, wide stance | work apron, tool |
| Traveller | neutral road silhouette with back mass | coat, mantle, pack |
| Refugee | narrow, inward hands, protected head | hood, mantle, pack |
| Scout | lean, forward step, short hem | short mantle, satchel, weapon |
| Healer | calm vertical line with clear identity mark | layered apron, satchel |

Each of the 81 exports uses one joined mesh and one indexed material with no
skin or animation. `COLOR_0` stores one of nine semantic palette indices:
`skin`, `hair`, `underlayer`, `outer`, `trousers`, `leather`, `metal`, `accent`,
or `eye`. The dedicated NPC shader uploads a nine-color palette per identity,
applies material-specific ink strength, and gives skin its own light ramp.
Contact, down, passing, and up poses on both sides are
swapped at held phase thresholds to produce intentional pseudo-pixel movement
without vertex uploads. This reduces each ambient body from nine primitives to
one while keeping palette variation entirely at runtime.

## Gameplay-scale rules

- Build the silhouette for the 35–60-art-pixel result, not the Blender camera.
- Keep lower legs continuous and dark; alternating pale limb sections read as
  sticks after point sampling.
- Hands, boots, headwear, tools, and the outer garment hem may be exaggerated.
- Walk motion uses eight held whole-body poses with short transitions: contact,
  down, passing, and up on both sides. Do not turn the static assets into
  smooth miniature motion; the held silhouette is part of the style.
- Faces use only eyes, expression brows and mouth, one of four noses, beard and
  age marks, an optional scar, one of eight hair silhouettes, and one of four
  headwear decisions. World glyphs and UI portraits consume the same recipe.
- Ambient headings are biased toward the fixed adventure-game camera. These
  people are composition actors; a broad three-quarter read is more valuable
  than presenting an exact travel vector as a one-pixel profile.
- Material variation comes from the deterministic runtime appearance recipe,
  so a rebuilt GLB does not change an established NPC's identity.

The nine role families currently total 80,316 triangles across 81 pose assets.
Only one pose per ambient person is drawn, and the models therefore add no
animated skin uploads. Physics-driven guards, raiders, travellers, witnesses,
climbers, swimmers, interior actors, and encounter participants retain their
authoritative biomechanical pose path while drawing from 30 generated rigid
body, hair, headwear, garment, pack, and tool modules against its resolved bone
frames. The entire module library is 1,968 triangles, contains no
skin or animation tracks, and is shared by every dynamic NPC. Contact IK,
combat, climbing, swimming, knockdown, and ragdoll state therefore change the
same visible body without a deformed-vertex upload per actor. The former
primitive figure is now only the missing-asset and rig-diagnostic fallback.

## Extension points

New shape families should be expressed as a role recipe or a reusable
generator helper, not a hand-edited exported GLB. Add a role to `ARCHETYPES`,
keep the palette-index mapping unchanged, regenerate, and extend `CcNpcRole` and the
runtime role-path table together. The validator rejects missing roles, reordered
palette semantics, missing `COLOR_0`, skins, animations, implausible bounds,
and assets above 6,500
triangles.
