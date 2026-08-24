# Screen-first character experiments

## Question

Can Crownless characters use large, quiet shapes like the supplied reference
instead of sending a detailed model through the pixel pipeline?

Yes. The first test is clearer at game size than the current layered hero.

## Test setup

The test keeps the current display rules:

- judge the figure at about 60 art pixels tall;
- enlarge the result with nearest-neighbor sampling;
- use a small shared palette with no dithering;
- check the same result in grayscale;
- keep the crown, dark hair, oxblood scarf, teal body, warm skin, and dark limbs;
- remove trim, thin straps, small armor plates, bracers, knee caps, and surface texture.

The editable Blender scene contains three unrigged proxy characters. It does
not replace the production hero or any NPC asset.

## Variants

| Variant | Added detail | Result at 60 art pixels |
| --- | --- | --- |
| A: five-mass | None | Best read. Head, scarf, body, limbs, and boots stay separate. |
| B: belt and satchel | One wide belt and one bag | The belt survives. The bag starts to fuse with the body. |
| C: cape silhouette | One cape mass behind one shoulder | The cape is too narrow. It needs a wider outer shape, not more surface detail. |

## First verdict

Variant A is the strongest base. The model can still have joints and hidden
structure for animation, but the visible shell should behave like a small set
of large costume masses.

A useful first production budget is:

- six main color families;
- one shape for the scarf and shoulder line;
- one torso shape;
- one visible limb shape per arm or leg segment;
- large hands, head, and boots;
- one optional role prop that changes the outer silhouette.

The next test should rig Variant A to the existing hero skeleton, render the
eight walk poses in the street scene, and compare it with the current hero at
35, 48, and 60 art pixels. Keep the old hero until that motion test passes.

## Version 0.2: closer to the concept

The second model pass keeps the same screen budget but changes the source
shapes to match the concept more closely:

- a larger warm face wrapped by asymmetric hair instead of an inset face;
- a smaller crown pushed to one side;
- two broad scarf folds instead of one flat shoulder plate;
- a longer tunic that reaches the hips;
- short teal sleeves, visible forearms, and larger hands;
- thicker legs and two-part heavy boots;
- an offset satchel and a wider cape that alter the outer silhouette.

At 60 art pixels, the plain wrap is still the cleanest base. The travel strap
and offset bag now survive without becoming a noisy belt across the whole
body. The wider cape also reads as a separate outer mass. This pass is close
enough to the concept to use for the first rigged motion test.

## Version 0.3: reference proportions in real 3D

The third pass corrects the remaining stubby read without changing the final
screen size:

- the upper body moves upward to expose longer legs;
- the torso, scarf, and arm span become narrower;
- hands end higher, like the reference;
- the camera scale increases so the taller model still measures about 60 art
  pixels;
- the head, torso, scarf, sleeves, and hands gain more depth so the front match
  does not collapse into a cardboard side view.

The same plain-wrap mesh is rendered from the front, three-quarter, and side.
There is no redraw or camera-specific geometry. The front proportions are now
close to the pixel reference, while the three-quarter and side views remain
usable 3D volumes. An exact match from every view is not possible because the
reference makes hand-placed, view-specific pixel decisions, but this model is
close enough to carry the same silhouette language through motion.

## Version 0.4: painterly definition

The reference's definition is not a result of ordinary 3D light alone. Its
diagonal tunic value, scarf lip, sleeve blocks, and hand shadows are designed
shapes. A two-band normal shader cannot infer those shapes.

The fourth pass renders the same geometry under uniform light in three ways:

- light only;
- two broad model-space paint masks;
- paint masks plus a small number of sleeve and hand accents.

The offline Blender test uses thin attached surfaces as a quick stand-in for
vertex paint. Production assets should not add those surfaces. Instead, the
indexed character color can carry three values per vertex corner:

- red: semantic palette slot;
- green: authored shadow, neutral, or light weight;
- blue: optional fold or inner-edge strength;
- alpha: fully opaque.

The shader can combine those stable weights with one weak normal-light band.
This keeps the painted definition attached during movement and avoids a
screen-space effect that would crawl across the model. At 60 art pixels, one
diagonal tunic wedge, one scarf lip, and one sleeve or hand accent are enough.

## Version 0.5: first animation test

The painted model now has three short motion tests:

- a quiet breathing idle;
- a walk cycle with clear arm and leg swings;
- a turn that checks the model from front to side.

Each preview shows the source 3D render beside the result reduced to about 60
art pixels. The large color groups survive all three clips. The scarf remains a
clear shoulder shape, the face stays separate from the hair, and the painted
tunic and scarf shadows stay attached during the turn.

This is a rigid-piece motion test, not the final character rig. It proves the
model can move without becoming too busy in the pixel pipeline. The next pass
should use the existing hero skeleton so the knees and elbows can bend and the
feet can make cleaner contact with the ground.

## Version 0.6: real engine test

The proposal now runs as an optional hero skin in the game. It uses:

- the existing physics-driven hero skeleton;
- the real walk and foot-contact poses;
- the production hero shader, fog, and shared palette grade;
- the 457 by 285 art-pixel world target and nearest-neighbor presentation;
- the normal street and market cameras.

The export is one skinned model with 14 material groups, 1,288 triangles, and
all 22 required engine bones. It stays below the 32-mesh animated-skin budget.
The current production hero remains the default. Pass `--screen-first-hero`
when launching the game to select this experiment. The
`CC_SCREEN_FIRST_HERO=1` environment switch remains available for build tools.

The engine comparison is stronger than the offline test. At close range, the
current hero separates into many thin costume pieces. The screen-first skin
keeps one large scarf, one long tunic, one head shape, large hands, and heavy
boots. At normal street scale, it still reads as the player without needing
extra surface detail. The walk cycle bends at the real knees and elbows and
uses the engine's planted-foot contacts.

Version 0.6 still uses thin attached surfaces for painted values. Version 0.7
removes that shortcut from the engine model.

## Version 0.7: cleanup and hair refinement

The refinement pass fixes the overlaps found in the real engine walk:

- the tunic's light, neutral, and shadow wedges are now faces of one closed
  volume instead of stacked paint surfaces;
- sleeve, forearm, thigh, shin, cuff, and boot widths leave clean gaps at the
  joints and between the legs;
- the scarf is slightly narrower and no longer crowds the upper arms;
- the hair is split into a shallow rear shell, a warmer top plane, two side
  locks, and one stepped fringe;
- the rear hair shell sits outside the head instead of cutting through it;
- skin colors compensate for the warm hero light so the final palette lookup
  chooses the skin ramp instead of gold.

The refined export has 31 authored pieces, 1,150 triangles, 14 material groups,
and all 22 engine bones. The eight-pose engine walk keeps both legs and boots
separate through contact, passing, and swing poses. The crown, hair planes, and
face also remain distinct in front, three-quarter, profile, and rear views.

## Version 0.8: tapered hair clumps

The cap-like hair has been replaced by a small piece-based design:

- one small scalp core stays hidden under the roots;
- two bangs, one long side lock, one short side lock, and two rear wedges make
  exactly six opaque clumps;
- every clump follows a curved path with three to five diamond-shaped
  cross-sections, a broad root, and a narrow tip;
- the rear shell, top plate, side blocks, and stepped fringe no longer exist;
- dark brown is the main hair value, while two small front clumps carry the
  second brown value and one plane carries the only highlight;
- `hair.long` and `hair.rear` extend the engine skin to 24 bones. Their roots
  stay on the head while quadratic weights and a small delayed direction move
  only the tips.

The V08 export has 34 authored pieces, 1,160 triangles, 15 material groups,
one runtime mesh, and 24 bones. The engine still uses the same body pose and
cape simulation. The two hair controls borrow only 12 and 18 percent of the
cape's delayed direction, so the locks follow the walk without loose strand
simulation.

The V08 review follows three gates. The exporter first makes a solid black
silhouette at the real 60-pixel character height. The optional engine skin is
then checked under the production two-band shader and shared palette. Finally,
the production walk action is sampled into a 16-frame loop and an eight-pose
contact sheet. The validator also rejects old cap-part names and checks the
exact core, clump, highlight, and bone counts.

The V05 build also has a coverage gate. It checks the exact 80-frame set,
image size, character-colored pixel count, stage position, and visible bounds.
This catches the blank-render regression caused by broken parent transforms
before the preview GIFs are made. Run it with
`make blender-character-animations-check`.

## Files

- `assets/previews/experiments/character_simplification_concepts_v01.png`
- `assets/previews/experiments/screen_first_character_pipeline_v01.png`
- `assets/previews/experiments/screen_first_character_pipeline_v02.png`
- `assets/previews/experiments/screen_first_character_pipeline_v03.png`
- `assets/previews/experiments/screen_first_character_3d_views_v03.png`
- `assets/previews/experiments/screen_first_character_shading_v04.png`
- `assets/previews/experiments/screen_first_character_painted_3d_views_v04.png`
- `assets/previews/experiments/screen_first_character_idle_v05.gif`
- `assets/previews/experiments/screen_first_character_walk_v05.gif`
- `assets/previews/experiments/screen_first_character_turn_v05.gif`
- `assets/previews/experiments/screen_first_engine_comparison_v06.png`
- `assets/previews/experiments/screen_first_engine_close_v06.png`
- `assets/previews/experiments/screen_first_engine_golden_v06.png`
- `assets/previews/experiments/screen_first_engine_walk_sheet_v06.png`
- `assets/previews/experiments/screen_first_engine_walk_v06.gif`
- `assets/previews/experiments/screen_first_engine_refinement_v07.png`
- `assets/previews/experiments/screen_first_engine_close_v07.png`
- `assets/previews/experiments/screen_first_engine_walk_sheet_v07.png`
- `assets/previews/experiments/screen_first_engine_walk_v07.gif`
- `assets/previews/experiments/screen_first_hair_silhouette_v08.png`
- `assets/previews/experiments/screen_first_engine_hair_shading_v08.png`
- `assets/previews/experiments/screen_first_hair_walk_sheet_v08.png`
- `assets/previews/experiments/screen_first_hair_walk_v08.gif`
- `assets/blender/crownless_screen_first_character_experiments.blend`
- `assets/blender/crownless_screen_first_character_experiments_v02.blend`
- `assets/blender/crownless_screen_first_character_experiments_v03.blend`
- `assets/blender/crownless_screen_first_character_experiments_v04.blend`
- `assets/blender/crownless_screen_first_character_animation_v05.blend`
- `assets/exports/hero/crownless_screen_first_engine_rig_v06.glb`
- `assets/exports/hero/crownless_screen_first_engine_rig_v06.json`
- `assets/exports/hero/crownless_screen_first_engine_rig_v07.glb`
- `assets/exports/hero/crownless_screen_first_engine_rig_v07.json`
- `assets/exports/hero/crownless_screen_first_engine_rig_v08.glb`
- `assets/exports/hero/crownless_screen_first_engine_rig_v08.json`
- `tools/blender/render_screen_first_character_experiments.py`
- `tools/blender/compose_screen_first_character_experiments.py`
- `tools/blender/render_screen_first_character_animation.py`
- `tools/blender/compose_screen_first_character_animation.py`
- `tools/blender/export_screen_first_engine_hero.py`
- `tools/blender/render_screen_first_hair_v08.py`
- `tools/blender/compose_screen_first_hair_v08.py`

Run `make blender-character-experiments` to rebuild the Blender scene and the
pipeline comparison sheet.

Run `make blender-character-animations` to rebuild the animation scene and all
three GIF previews.

Run `make blender-character-engine` to rebuild and inspect the optional engine
skin.

Run `make blender-character-hair-v08` to rebuild the engine skin, the
60-pixel silhouette, and both walk previews.
