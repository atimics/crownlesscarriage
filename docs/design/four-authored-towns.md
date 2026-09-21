# Four authored towns — scene rebuild

This branch implements the four-town concept board approved in the conversation.
It is a scene/terrain/composition task, not a quest, economy, or save-schema rewrite.
The linked PR remains a draft until its code and rendered checks are ready.

## Authored scenes

| Place | Four scene commitments | Distinguishing spatial idea |
| --- | --- | --- |
| Thornford | River Crossing; Threshing Green; Granary Rise; Cartwright Yard | A working river village: horizontal fields and low crofts below raised food stores; water and wheelwork explain the settlement. |
| Gloamgate | Market Circle; Archive Steps; Cloth Yard; Coach Court | A bowl-shaped market and branching courts: close awnings, a headless queen, tall archive facade and a covered coach court. |
| Silverwick | Foundry Terrace; Company Store; Workers' Lane; Ore Wagon Yard | Industrial shelves: warm furnace mouths under dark gantries, retaining walls and a worn worker street, not another square with grey cottages. |
| Alderwatch | Contested Bridge; Muster Spine; Keep; Lower Bailey | A fortress across a visible void: long controlled crossing, compact military street, dominant keep and a sheltered supply court. |

The painted concept is a composition reference, not a demand for photorealistic
assets or a new rendering engine. Express its forms with the existing model,
material, lighting, and terrain system. Keep Crownless's scale and visual language.

## Acceptance

- Existing gates, carriage parking, service doors, actor approaches and saved
  positions remain reachable. New visible solid geometry must agree with collision.
- The player learns different silhouettes and street relationships, not four
  colour variants of the same camera and building grid.
- Each town has four intentionally composed playable views, including an ordinary
  walking route between yard and service. Scenes are not disconnected dioramas.
- Existing stock, fire, repair and security presentation still follows simulation
  state. Scenery must not manufacture resources or change goods custody.
- Review actual captures for all four towns. CI correctness alone does not certify
  visual quality, phone readability, or a complete ordinary-control walkthrough.

## Review lane

`.github/workflows/four-town-scenes.yml` builds the native client on relevant PRs,
checks the existing route/interaction/save tests, and publishes rendered scenes
with the exact source revision. Its short-lived source receipt omits audio,
Blender sources and documentation media; it is for reproducing scene review, not
for distribution as a complete game. The normal browser CI remains separate.
