# Character Readability Contract

## Verdict from the character-design research

The useful common ground between hand-drawn animation, anime production, and
stylized game characters is not a single face shape or rendering technique. It
is controlled simplification: preserve a small number of identity decisions,
hold expressions long enough to read, and spend animation detail on a clear
line of action rather than on constant motion.

The Crownless implementation now treats the pixel look as the final display
contract, not as surface decoration on a high-detail model.

| Research principle | Crownless implementation |
| --- | --- |
| Design on a stable image grid | The 914 x 570 world viewport renders at 457 x 285 and is enlarged with point sampling. |
| Let UI remain readable | Labels, combat bars, prompts, and side panels draw after the world upscale at display resolution. |
| Preserve a few identity anchors | The hero reads through crown, asymmetric hair, oxblood cape, teal torso, and athletic proportions. |
| Use broad painted lighting | The shared character shader uses two cel-light bands, warm skin shadows, sparse highlights, and colored inside-silhouette ink. |
| Build for the delivered size | Hands, boots, shoulders, crown, and cape are proportioned to survive at 35–60 art pixels, not to look anatomically neutral in a turntable. |
| Change representation with distance | Far characters rely on silhouette and costume mass; medium characters gain depth-tested face glyphs; close interactions use pixel portraits. |
| Hold faces instead of making them chatter | Neutral, focused, hurt, and talking expressions are driven by gameplay state and remain stable for that state. |
| Give each person a movement signature | Stable appearance data now includes cadence, stride, bob, lean, and arm-swing traits, with role-specific biases. |
| Judge motion across time | The game can capture eight evenly sampled walk poses for a temporal contact sheet. |

## Display and lighting contract

- The world color and depth target is exactly half the displayed viewport in
  each dimension.
- The render texture must use nearest-neighbor sampling.
- World grading may adjust contrast, saturation, warmth, and vignette, but must
  not resample, add random grain, or introduce a second spatial pixel grid.
- Fog and grading happen before the final shared-palette lookup. Nothing may
  blend new world colors after that lookup.
- The hero costume should resolve as three dominant value families: warm skin,
  a middle-value teal/oxblood costume, and dark hair/limbs. Gold is a small
  recognition accent, not a fourth large mass.
- Hero lighting has one shadow band and one light band. Highlights are reserved
  for a few planes and may not turn the figure back into a smoothly shaded toy.
- Edge treatment must remain inside the projected silhouette. It may separate
  adjacent planes, but must not grow or crawl around the character as the
  camera moves.

## Version 0.9 silhouette pass

- The hero uses a stronger shoulder line, thicker limbs, larger hands and
  boots, a compact waist, and a slightly wider runtime presentation.
- The asymmetric left pauldron and cape produce a readable long-side/short-side
  shape. A three-prong crown replaces the former sub-pixel gold bead.
- NPC roles now combine a generated Blender archetype with deterministic
  stature, shoulder width, body mass, head scale, and costume values. Guards
  and laborers read broad; scouts and refugees read narrow; merchants carry a
  larger conversational head and torso mass.
- The 81 static role-pose GLBs and 30 dynamic rigid modules share a dedicated
  indexed shader. Vertex colors select one of nine uniform palette entries, so
  every identity uses one material with skin-specific ramps and per-material
  ink while the former procedural figure remains the missing-asset and
  rig-debug fallback.

The production direction follows the same constraints documented by the
*Dead Cells* art team—judge the model at its tiny delivered size, render without
antialiasing, use a toon-lit 3D source, and spend detail on readable poses—and
the stable-pixel methods presented for *Never's End*: stepped tonal ramps,
edited silhouette decisions, and camera/model/joint snapping. See
[Using a 3D pipeline for 2D animation in Dead Cells](https://www.gamedeveloper.com/production/art-design-deep-dive-using-a-3d-pipeline-for-2d-animation-in-i-dead-cells-i-)
and [How We Draw a 3D Sprite World: The Stylized Art of Never's End](https://media.gdcvault.com/gdc2026/Slides/Juckett_Ryan_HowWeDrawA3DSpriteWorldTheStylizedArtOfNeversEnd.pdf).

## Character distance ladder

The face-glyph queue measures projected character height in the 457 x 285 art
target.

- Below 16 art pixels: no facial detail. Silhouette, posture, and costume own
  recognition.
- At 16 art pixels and above: a front-facing character receives a two-eye glyph;
  a side-facing character receives one eye. These marks are world-depth-tested,
  so they cannot appear through a wall or roof.
- At 38 art pixels and above: brows and a mouth reinforce the held expression.
- In a named interaction: the panel uses a 20 x 24-cell portrait. Face width,
  four nose and beard families, age, scar, eight hair silhouettes, four
  headwear families, skin value, costume color, role, and expression all come
  from the same stable appearance recipe used by the world character.

## Motion contract

The hero remains the smoothest mover because physical contact, gait, cape
follow-through, guard, strike, and recovery are authoritative. Background and
non-hero people use deterministic movement traits stored with their appearance:

- guards are compact, upright, and low-bob;
- raiders lean forward and swing more aggressively;
- merchants move more conversationally through the arms;
- laborers are heavy and slower through the cycle;
- refugees are reduced and inward;
- scouts are quick, long-striding, and forward;
- healers are measured and restrained.

These are biases, not separate animation systems. Body proportions, equipment,
and movement therefore reinforce the same identity without breaking traversal
or combat ownership.

## Visual acceptance pass

Run these from the repository root after a release build:

```sh
./out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage --capture-golden out/qa/character-readability-golden.png
./out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage --capture-interior out/qa/character-readability-interior.png
./out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage --capture-parley out/qa/character-readability-parley.png
./out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage --capture-walk-cycle out/qa/character-walk
```

Accept the pass only when all of the following are true:

1. The hero is locatable immediately in native color and grayscale.
2. Crown, hair, cape, torso mass, and long-limbed action-figure proportions
   survive the half-resolution target.
3. Mara, a road collector, and the Crownless hero are distinguishable from
   their portrait silhouettes before reading their labels.
4. The eight walk poses describe a clear weight transfer, not a rigid sliding
   figure or an inflated mascot.
5. Face marks remain attached to visible faces and never draw through scenery.
6. Labels and UI type remain crisp while the world stays on the coarse grid.
7. The render benchmark still satisfies the skin-update, mesh, hero, and LOD
   budgets.
