# NPC figure readability pass

Before/after frames for the NPC readability work on `tools/blender/build_npc_archetype_library.py`. NPCs are static baked meshes (no bone skin), so they cannot reuse the hero's runtime head/hand gameplay scale (`DrawHeroSkin`, `asset_loading.inc`: 1.32x head/hair, 0.90x hand). The generator now bakes an equivalent proportion boost directly into the geometry, plus a brow/nose face mass, softer hands and boots, a relaxed idle arm hang, and a real apron shape for merchant/laborer/healer in place of the old flat boxes.

Every frame pair uses the same camera, seed, and game build; only the NPC archetype exports differ (checked out from the pre-edit commit for "before", from this branch for "after").

## Close-up review (`--capture-npc-review`)

The close-up tool is useful for judging body proportions and silhouette, but it is not the final word on face readability: the game also draws a separate, always-on procedural face overlay (`DrawWorldFace`) on top of the head mesh once the projected face gets big enough on screen, which is exactly what a close-up produces. At true gameplay distance that overlay self-disables (`projected_face_height <= 3px` skips it), and the baked mesh plus vertex-paint value contrast is what actually reads. The town/interior frames below are the real acceptance test.

- [Front — before](before-npc-front.webp) / [after](after-npc-front.webp)
- [Side — before](before-npc-side.webp) / [after](after-npc-side.webp)
- [Motion — before](before-npc-motion.webp) / [after](after-npc-motion.webp)

## Game-size frames (the real test)

- [Market square — before](before-town1.webp) / [after](after-town1.webp)
- [Mining town — before](before-town3.webp) / [after](after-town3.webp)
- [Granary interior — before](before-interior.webp) / [after](after-interior.webp)

## Close-up crops

Two tight crops that make the change easy to see without pixel-peeping the full frame:

- [Laborer torso — before](before-laborer-closeup.webp) / [after](after-laborer-closeup.webp): the flat grey apron bib and two flat skirt boxes become a shaped, tapered bib and split skirt with a stitched center fold; the relaxed arm hang is visible too.
- [Granary keeper face, interior scene — before](before-face-closeup.webp) / [after](after-face-closeup.webp): the bigger head reads as an actual face at conversation distance instead of a small blank dome, and the hand reads as a rounded fist instead of a thin blob.

## What changed, per archetype

- **All nine archetypes**: head grown ~1.3x (`HEAD_GROWTH`) to approximate the hero's 1.32x runtime head boost; hands shrunk ~0.9x (`HAND_SHRINK`) to match the hero's hand scale and rounded out (more subdivisions, less elongated); boots given a bigger, multi-segment bevel and lower/longer proportions instead of a sharp brick; a brow bar and nose wedge added to the head (no new eye/mouth geometry — the existing per-face-normal value shading already turns the brow's underside dark and its top bright); the default idle arm pose (used directly by wayfarer/guard, and as the base every other posture offsets from) now tucks the elbow near the ribs and the hand near the hip instead of bowing out past the shoulder line.
- **Guard, laborer, raider**: added a dark jaw/beard mass for silhouette variety.
- **Merchant, laborer, healer**: the flat box apron bib and two flat box skirt panels are replaced with a shaped bib panel and two tapered, curved split-skirt panels, plus a stitched center-fold accent line.
- **Everyone else** (wayfarer, traveller, refugee, scout): proportion and arm-pose changes only; their mantle/pack/hood equipment already reads as a distinct silhouette.

No changes to shaders, palettes, or C rendering code — geometry and vertex paint only, in `tools/blender/build_npc_archetype_library.py`.

## Contract note

`assets/world_kit_manifest.json`'s `figure_standard.head_count_range` (5.75-6.25 heads) documents the world-kit body system used by the hero and buildings, not the NPC archetype library, which is a fully self-contained generator with its own hardcoded proportions. No world-kit geometry changed, so that contract still accurately describes the (untouched) world-kit modules and was left as-is.

## Validation

- `validate_npc_archetype_library.py`: 99 pose assets, 181280 triangles total, all under the 6500-triangle-per-archetype budget.
- `validate_npc_dynamic_modules.py`: unaffected, still 34 modules / 5056 triangles.
- `make blender-exports-check`: 0 failures.
- `cmake --preset play && cmake --build --preset play`, then `ctest` (with `ulimit -s 65520`): 237/237 tests passed.
