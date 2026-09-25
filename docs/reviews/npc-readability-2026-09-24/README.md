# NPC figure readability pass

Before/after frames for the NPC readability work on `tools/blender/build_npc_archetype_library.py` and, after the first review round, `src/client/local3d/actor_rendering.inc`. NPCs are static baked meshes (no bone skin), so they cannot reuse the hero's runtime head/hand gameplay scale (`DrawHeroSkin`, `asset_loading.inc`: 1.32x head/hair, 0.90x hand). The generator bakes an equivalent proportion boost directly into the geometry, plus softer hands and boots, a relaxed idle arm hang, and a real apron shape for merchant/laborer/healer in place of the old flat boxes.

Every frame pair uses the same camera, seed, and game build; only the NPC archetype exports (and, for the second round, the small runtime change below) differ between before and after.

## Second review round: hair, face overlay, apron contrast

The first round grew the archetype mesh's head ~1.3x but missed three things a reviewer caught looking at the game capture, not a Blender render:

1. **Hair/hats/helmets/hoods didn't scale with the head.** They are drawn separately from the archetype mesh, in `DrawNpcArchetypePose3D` (`actor_rendering.inc`), sized off the *old* head. The bigger head stuck out from underneath them, so a character with hair looked bald with a small patch on top. Fixed by scaling `identity_head`/`molded_hair` (which both hair and headwear use) by the same growth factor about the same head point (`NPC_ARCHETYPE_HEAD_GROWTH`, kept in sync with Python's `HEAD_GROWTH`).
2. **The procedural face overlay was scattered.** `DrawWorldFace` (also in `actor_rendering.inc`) paints the actual eyes/brow/nose/mouth/scars on top of the head mesh (`CcNpcPaintFaceFeatures` in `cc_npc_appearance.c`) once the projected face is big enough on screen — this is a separate, always-on system, not something Blender geometry controls. It was also still sized for the old head, so its painted features landed in the wrong places once the head grew. Fixed by scaling its half-width/half-height/half-depth and vertical offset by the same factor. Once that was in place, an A/B render confirmed the mesh brow bar and nose wedge added in round one were now pure redundant clutter next to the overlay's own painted brow/eyes/nose — removed them.
3. **The apron still read as a flat grey block.** Round one fixed its shape (a boxy bib -> a tapered panel) but not its color: it used the "underlayer" palette slot, which runs close in rendered value to the "outer" tunic for many NPCs, so the apron visually fused with the shirt underneath. Switched it to "leather" (a separate, reliably darker color pool at runtime) plus a slight extra `cc_value_offset` darkening.

The before/after pairs below are from this second, corrected build. The "before" state is `origin/main` (this branch's base); "after" is this branch after both rounds.

## Close-up review (`--capture-npc-review`)

The close-up tool is useful for judging body proportions, silhouette, and (now) the face overlay's fit, but it is not the final word on face readability: the same `DrawWorldFace` overlay self-disables at true gameplay distance (`projected_face_height <= 3px` skips it entirely), so the baked mesh plus vertex-paint value contrast is what actually reads there. The town/interior frames are the check for that.

- [Front — before](before-npc-front.webp) / [after](after-npc-front.webp)
- [Side — before](before-npc-side.webp) / [after](after-npc-side.webp)
- [Motion — before](before-npc-motion.webp) / [after](after-npc-motion.webp)

## Game-size frames (the real test)

- [Market square — before](before-town1.webp) / [after](after-town1.webp)
- [Mining town — before](before-town3.webp) / [after](after-town3.webp)
- [Granary interior — before](before-interior.webp) / [after](after-interior.webp)

## Close-up crops

- [Laborer torso — before](before-laborer-closeup.webp) / [after](after-laborer-closeup.webp): the apron is now a shaped, dark-leather bib and split skirt with a visible fold, clearly separate from the lighter tunic, instead of a flat grey-blue block fused to it. The relaxed arm hang is visible too.
- [Raider face — before](before-raider-face-closeup.webp) / [after](after-raider-face-closeup.webp): a clean, unobstructed close-up of the face-overlay fix — properly sized, well-spaced eyes, brow, and mouth on the bigger head, instead of tiny features lost on a small one.
- [Granary keeper, interior scene — before](before-face-closeup.webp) / [after](after-face-closeup.webp): the same conversation shot the second review round was called out on. Hair now fully covers the bigger head instead of leaving a bald patch showing underneath it.

## What changed, per archetype

- **All nine archetypes**: head grown ~1.3x (`HEAD_GROWTH` / `NPC_ARCHETYPE_HEAD_GROWTH`, kept in sync between the Python generator and the C renderer) to approximate the hero's 1.32x runtime head boost, with hair/headwear/the face overlay scaled to match; hands shrunk ~0.9x (`HAND_SHRINK`) to match the hero's hand scale and rounded out (more subdivisions, less elongated); boots given a bigger, multi-segment bevel and lower/longer proportions instead of a sharp brick; the default idle arm pose (used directly by wayfarer/guard, and as the base every other posture offsets from) now tucks the elbow near the ribs and the hand near the hip instead of bowing out past the shoulder line. No brow/eye/nose mesh geometry: the runtime's procedural face overlay already draws a full face, correctly scaled now, and mesh geometry there only doubled up as clutter.
- **Guard, laborer, raider**: a dark jaw/beard mesh mass for silhouette variety (independent of, and can coexist with, the overlay's own per-seed beard blocks).
- **Merchant, laborer, healer**: the flat box apron bib and two flat box skirt panels are replaced with a shaped bib panel and two tapered, curved split-skirt panels in a darker "leather" color, plus a stitched center-fold accent line.
- **Everyone else** (wayfarer, traveller, refugee, scout): proportion and arm-pose changes only; their mantle/pack/hood equipment already reads as a distinct silhouette.

No shader or palette changes. `actor_rendering.inc` changes only reposition/rescale existing draws (hair/headwear transform, face overlay anchor) to match the bigger baked head; they do not touch shaders or the palette system.

## Contract note

`assets/world_kit_manifest.json`'s `figure_standard.head_count_range` (5.75-6.25 heads) documents the world-kit body system used by the hero and buildings, not the NPC archetype library, which is a fully self-contained generator with its own hardcoded proportions. No world-kit geometry changed, so that contract still accurately describes the (untouched) world-kit modules and was left as-is.

## Validation

- `validate_npc_archetype_library.py`: 99 pose assets, 159896 triangles total (down from round one's 181280 after removing the redundant brow/nose mesh), all under the 6500-triangle-per-archetype budget.
- `validate_npc_dynamic_modules.py`: unaffected, still 34 modules / 5056 triangles.
- `make blender-exports-check`: 0 failures.
- Rebased on `origin/main`. `cmake --preset play && cmake --build --preset play`, then `ctest` (with `ulimit -s 65520`): 240/243 passed. The 3 failures (`crisis_contested_succession`, `long_history_recovery`, `scrivendays_calendar_and_evidence`) are pre-existing on `origin/main` and unrelated to this change; no new failures.
