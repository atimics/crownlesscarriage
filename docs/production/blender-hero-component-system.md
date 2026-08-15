# Blender Hero Component System

The hero library translates the modular character concept into a rebuildable
Blender source file. Version 0.7 is a gameplay-silhouette production prototype:
it proves component boundaries, rig attachment, cloth authoring, export
metadata, and code integration while establishing the hero's visual language.

## Generated artifacts

- Blender source: `assets/blender/crownless_hero_components.blend`
- Component exports: `assets/exports/hero_glb/`
- Component manifest: `assets/hero_component_manifest.json`
- Review renders: `assets/previews/hero/`
- Procedural variation sheet: `assets/previews/hero/hero_procedural_variants.png`
- Procedural variation source: `assets/blender/crownless_procedural_character_variants.blend`
- Builder: `tools/blender/build_hero_component_library.py`
- Body/wearable grammar: `tools/blender/procedural_character.py`
- Validator: `tools/blender/validate_hero_component_library.py`

## Collection bands

| Collection | Responsibility |
| --- | --- |
| `10_RIG` | Canonical humanoid armature and semantic sockets |
| `20_ANATOMY` | Neutral body and biomechanical muscle guides |
| `30_GARMENTS` | Padded underlayer and tunic |
| `40_CLOTH` | Cape render mesh and pinned control cage |
| `50_ARMOR` | Cuirass plus independently equipped left/right armor |
| `60_ACCESSORIES` | Hair, gloves, boots, belt, strap, and satchel |

The library contains 19 exportable components, an assembled GLB, 18 semantic
body bones, four presentation-only cape bones, and 15 sockets. Left and right
pauldrons, bracers, greaves, gloves, and boots remain separate so asymmetric
equipment and damage do not require new meshes.

The current art pass treats the hero as a brigandine-equipped crownless
wayfarer rather than a miniature tournament knight. A deep petrol split tunic
keeps the legs readable. The close-fitted, oxblood textile torso defense uses
six oversized rivet landmarks to imply hidden plates and two overlapping lower
lames to protect the abdomen. A short forked road-cape and compact travel cowl
clear the knees during climbing, while the small, flat shoulder bag reads as
travel gear without dominating the chest. The left pauldron remains the strong
side; the right shoulder stays quiet. Two broad hair shapes open the face, and
the palette becomes progressively darker below the waist so the head remains
the focal point at the gameplay camera. These changes remain inside the
original 19 component collections and do not change runtime IDs, sockets,
export names, or skeleton topology.

## Procedural body and wearable grammar

Version 0.7 makes the action-figure body a recipe instead of a one-off mesh.
`BodyParameters` controls muscularity, body fat, shoulder, chest, waist, hip,
arm, leg, neck, hand, and foot scale on the canonical skeleton. It generates
ordered elliptical cross-sections for the torso, pelvis, neck, upper arms,
forearms, thighs, and shins. The production hero uses the
`action_figure_wayfarer` preset; `lean_scout` and `heavy_vanguard` exist as
boundary examples and are rendered in the procedural variation sheet.

`ShellRecipe` derives clothing or equipment from those anatomical sections.
Garments use positive ease and clearance; rigid equipment can combine smaller
clearance with controlled compression and regional width/depth shaping.
Compression reduces that fit allowance but never shrinks a shell through the
exported body surface; an actual flesh-compression pass must deform or cull the
covered anatomy explicitly. Validation checks the closest shell radius after
front/back center offsets, so asymmetric shaping cannot hide an intersection.
The current padded underlayer, fitted tunic, cuirass, bracers, and greaves all
consume these derived profiles. Coverage, trims, seams, rivets, damage marks,
and other authored design details remain separate. This division makes fit and
silhouette reusable without reducing every costume to the same surface detail.

The recipe is dependency-free Python and validates without launching Blender.
The serialized body, garment, and equipment values are also written into
`assets/hero_component_manifest.json`, so generated assets remain traceable to
their source parameters.

## Action-figure form language

The 0.6–0.7 body passes replace the earlier capsule-and-block construction with an
articulated action-figure anatomy. Twelve-sided lofts establish an adult
head-to-body ratio, a V-tapered rib cage, a shaped pelvis, and distinct biceps,
forearm, quadriceps, calf, knee, and ankle transitions around the unchanged
rig. The anatomy guide adds pectoral, latissimus, trapezius, oblique, and
abdominal masses as intentional sculpting landmarks. Garment sleeves, bracers,
greaves, and cuffs follow those same changing profiles instead of reading as
straight tubes. Smaller hands gain thumbs and individual knuckles, while the
feet and boots taper through the ankle, instep, and toe rather than ending in
rectangular blocks. These changes preserve the component, socket, and skeleton
contracts while moving the silhouette from construction toy to collectible
fantasy figure.

## Research basis

This is a fantasy design, not a historical reconstruction, but its visual logic
is grounded in primary museum material and production guidance:

- The Royal Armouries describes field plate as a distributed system of fitted
  components that preserved agility, while its exceptional fully enclosing
  tournament armour carried roughly twice the weight of normal battle armour.
  That distinction supports partial protection and open joints for this mobile
  hero: [Hundred Years' War armour](https://royalarmouries.org/objects-and-stories/stories/the-hundred-years-war-1337-1453)
  and [Henry VIII's foot-combat armour](https://royalarmouries.org/objects-and-stories/stories/henry-viiis-foot-combat-armour).
- The Metropolitan Museum's circa-1450 brigandine breastplate combines steel,
  copper alloy, and textile. The set abstracts that layered construction into a
  dark textile facing, sparse brass rivets, and exposed lower lames:
  [right breastplate from a brigandine](https://www.metmuseum.org/art/collection/search/23152).
- Surviving and depicted pilgrim equipment repeatedly combines a travel cloak,
  shoulder bag or knapsack, hat, and a small identifying emblem. The hero keeps
  only the cloak, bag, and one waymark so the reference does not become costume:
  [Germanisches Nationalmuseum pilgrim ensemble](https://objektkatalog.gnm.de/objekt/T551)
  and [Yper Museum pilgrim collection](https://www.ypermuseum.be/en/collection/pilgrims/).
- Riot's character-art guidance prioritizes believable form and readability at
  tiny in-game scale; Valve's Team Fortress 2 presentation likewise treats
  silhouette and shading as gameplay communication. The build therefore emits
  `hero_gameplay_read.png` at 320×320 with the hero occupying only part of the
  frame: [Riot character art](https://www.riotgames.com/en/artedu/character-art)
  and [Valve illustrative rendering](https://steamcdn-a.akamaihd.net/apps/valve/2007/NPAR07_IllustrativeRenderingInTeamFortress2_Slides.pdf).
- Material identity comes primarily from controlled value and roughness, not
  micro-detail. Cloth is broad and rough, leather is quieter, and metal receives
  tighter highlights: [Adobe PBR guide](https://www.adobe.com/us/learn/substance-3d-designer/web/the-pbr-guide-part-2).
- Four Horsemen Studios describes Mythic Legions as a highly articulated
  fantasy-figure system built around interchangeable parts and accessories.
  That is the closest product-language reference for this pass: pronounced but
  connected anatomy, readable joint breaks, fitted layered equipment, compact
  fists and feet, and modularity that does not sacrifice silhouette:
  [Mythic Legions](https://www.sourcehorsemen.com/).
- Blender's Skin modifier is designed to turn radius-controlled vertex/edge
  structures into organic base meshes, while Shrinkwrap, Solidify, and Surface
  Deform provide the established non-destructive vocabulary for fitted shells,
  physical thickness, and proxy-driven deformation. The current generator uses
  deterministic profile sweeps for production export, but follows that same
  separation of base body, fitted surface, thickness, and deformation:
  [Skin](https://docs.blender.org/manual/en/latest/modeling/modifiers/generate/skin.html),
  [Shrinkwrap](https://docs.blender.org/manual/en/latest/modeling/modifiers/deform/shrinkwrap.html),
  [Solidify](https://docs.blender.org/manual/en/latest/modeling/modifiers/generate/solidify.html), and
  [Surface Deform](https://docs.blender.org/manual/en/latest/modeling/modifiers/deform/surface_deform.html).

## Rig contract

The armature uses the same semantic landmarks as `CcHumanoidPose`: pelvis,
spine, chest, neck, head, upper arm, forearm, hand, thigh, shin, and foot.
Blender faces `-Y` with `+Z` up. A runtime adapter may convert the current
world-space pose into matrices for these names without making Blender animation
authoritative.

Rigid pieces carry `cc_anchor_bone`. Soft torso meshes use a small pelvis,
spine, and chest weight set. Component collections carry stable IDs, logical
slots, render layers, coverage declarations, and export paths.

## Cape

The cape is authored as a 6×7 control mesh. Its top row belongs to the
`PIN_COLLAR` vertex group, and the source file contains a configured cloth
modifier plus a visible control-cage guide collection. Runtime cloth should use
the same coarse topology but remain presentation-only; cloth particle state is
not saved or included in deterministic simulation hashes.

## Rebuild

```sh
make blender-hero-assets
make blender-hero-assets-check
make blender-hero-procedural-check
make blender-hero-procedural-preview
make blender-hero-animation
make blender-hero-actions
make blender-hero-engine
```

The first target rebuilds the `.blend`, exports each GLB, writes the manifest,
and renders assembled, small-scale gameplay-read, anatomy, and exploded views.
It then composes the three full-size views into a review triptych. The second
target reopens the saved library and checks components, bones, sockets, cloth
pins, exports, identity landmarks, review renders, and view layers.

The animation target creates `crownless_hero_animation.blend` and renders a
four-second looping GIF. It adds a full turntable, subtle breathing and head
sway, a camera-following fill light, and a non-destructive cape breeze shape
key. The editable Blender animation runs at 24 fps; the review GIF is sampled
at 12 fps for compact playback. The source component library remains unchanged.

The actions target creates `crownless_hero_actions.blend`, individual walk,
jump, climb, swim, and fight GIFs, and a combined action reel. Climbing uses a
presentation-only wall and holds, swimming stows the cape and adds water guides,
and fighting attaches presentation-only sword and shield props. These props and
motions do not alter component exports or deterministic simulation state.

The engine target rebuilds and validates the component source, authors a small
set of motion-pose previews from that exact source, and exports the GLB loaded
by the client. This is the authoritative bridge from component edits to the
game; the older self-contained action-reel builder is retained only for legacy
comparison.

## Runtime integration order

1. Map `CcHumanoidPose` into the manifest's canonical bone transforms.
2. Load the assembled GLB as a renderer proof without changing locomotion.
3. Resolve equipped component IDs into a visual assembly.
4. Apply rigid component transforms from their anchor bones.
5. Drive the cape control cage from chest and shoulder sockets.
6. Derive equipment mass and flexibility separately for biomechanical load.

The Blender file is generated. Preserve stable component IDs when replacing
blockout meshes with authored production geometry.
