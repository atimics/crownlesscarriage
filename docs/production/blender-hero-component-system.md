# Blender Hero Component System

The hero library translates the modular character concept into a rebuildable
Blender source file. Version 0.2 is a detailed low-poly production prototype:
it proves component boundaries, rig attachment, cloth authoring, export
metadata, and code integration while establishing the hero's visual language.

## Generated artifacts

- Blender source: `assets/blender/crownless_hero_components.blend`
- Component exports: `assets/exports/hero_glb/`
- Component manifest: `assets/hero_component_manifest.json`
- Review renders: `assets/previews/hero/`
- Builder: `tools/blender/build_hero_component_library.py`
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
bones, and 15 sockets. Left and right pauldrons, bracers, greaves, gloves, and
boots remain separate so asymmetric equipment and damage do not require new
meshes.

The current art pass adds a shaped six-sided cuirass, segmented pauldrons,
rivets and plate ridges, quilt seams, tunic placket and buttons, articulated
glove fingers, layered boots, a flat stitched satchel strap, facial planes,
layered hair, and a broad shoulder mantle. These details remain inside their
original component collections and do not change runtime IDs.

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
make blender-hero-animation
make blender-hero-actions
```

The first target rebuilds the `.blend`, exports each GLB, writes the manifest,
and renders assembled, anatomy, and exploded views. It then composes the three
views into a review triptych. The second target reopens the saved library and
checks components, bones, sockets, cloth pins, exports, and view layers.

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

## Runtime integration order

1. Map `CcHumanoidPose` into the manifest's canonical bone transforms.
2. Load the assembled GLB as a renderer proof without changing locomotion.
3. Resolve equipped component IDs into a visual assembly.
4. Apply rigid component transforms from their anchor bones.
5. Drive the cape control cage from chest and shoulder sockets.
6. Derive equipment mass and flexibility separately for biomechanical load.

The Blender file is generated. Preserve stable component IDs when replacing
blockout meshes with authored production geometry.
