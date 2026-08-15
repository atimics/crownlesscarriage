# Runtime Environment Design

## Direction

Crownless Carriage environments are navigable places first and illustrations
second. The camera may be oblique, but the town must not read as a collection
of generic isometric miniatures. Every visible mass should explain where the
player can walk, what the place is for, who controls it, and what has recently
happened there.

The old general Blender library is a concept and pipeline prototype. It is not
loaded by the client and is therefore not the production environment source of
truth. New environment work belongs in the runtime scene grammar until an
explicit model-loading boundary exists. The engine-driven hero GLB is different:
it is copied into the application bundle, loaded by raylib, and posed from the
physical skeleton, so improvements to that export do reach the game.

## The view

The exterior uses an asymmetric three-quarter orthographic camera. Orthographic
projection preserves reliable surface picking and a readable relationship
between the hero and nearby contact geometry. The unequal horizontal offsets
avoid the visual shorthand of a perfect 45-degree isometric grid and expose
more of the camera-facing facades.

This is a street-scale view, not a whole-settlement map. Buildings may frame or
leave the viewport as the hero moves. Future roof and wall cutaways should be
driven by actual hero occlusion rather than by making every structure short.

## Three layers of environment information

1. **Spatial skeleton** — roads, thresholds, courtyards, walls, water, ledges,
   and building footprints. These are authoritative for movement and collision.
2. **Place grammar** — foundations, massing, roofs, facade bays, porches,
   chimneys, stalls, loading areas, and local landmarks. These turn a collision
   footprint into a culturally and economically legible place.
3. **World-state projection** — stock, queues, guards, damage, repairs,
   faction marks, displaced people, traffic, vegetation, and maintenance. These
   respond to simulation state without replacing the spatial skeleton.

The layers compose. A shortage does not swap a market for a special shortage
model; it empties the stalls, changes crowd behaviour, adds ration control, and
degrades maintenance in the same market the player already knows.

## Settlement identity

An environment should be derived from four questions before geometry is added:

- **Why does this place exist?** Agricultural basin, crossroads market, mine,
  fortress, capital, frontier depot, or another material function.
- **How does work shape it?** Cart clearances, storage, drainage, livestock,
  workshops, hoists, defensive sightlines, and worker routes create its plan.
- **Who built and controls it?** Kingdom materials, faction symbols, permitted
  entrances, toll infrastructure, and repair quality establish power.
- **What condition is it in today?** Hunger, prosperity, security, traffic,
  weather, and recent events alter dressing and behaviour.

This produces bounded variation with meaning. Random roof colors and scattered
props do not constitute a city grammar.

## Runtime authoring contract

- Collision footprints and surface heights remain the shared authority for
  input, navigation, testing, and rendering.
- A building archetype decomposes that footprint into visible foundation,
  facade, roof, access, and landmark elements.
- Camera-facing detail must remain readable at the play camera; tiny catalog
  detail that disappears in motion is not production detail.
- Diagnostic skeletons, contacts, and force vectors are opt-in overlays. They
  are not part of the default art presentation.
- A future Blender environment asset is production-ready only after the client
  loads it through a runtime asset registry and binds it to the same semantic
  footprint and state-layer contract.
- Strategic variables must alter at least two visible channels. For example,
  hunger affects both land maintenance and market/crowd presentation, while
  prosperity affects paving quality and visible stock or repair.

## Production visual style

The shared target is **grounded dark-fantasy action figures in a handcrafted
world**. Characters use adult heroic proportions, visible garment layers,
profession-readable equipment, restrained pigments, and broad details that
survive the play camera. Environments use the same bevel scale, material
families, warm-light/cool-shadow balance, and semantic state dressing. The
runtime grade applies one final contrast, saturation, warmth, vignette, and
subtle grain treatment to code-drawn geometry and glTF assets alike.

The runtime visual loading layer currently owns the hero, carriage, cargo rack,
bridge checkpoint, market/granary, mine entrance, three market state layers,
and the shared grade shader. Assets resolve from the source tree, build tree,
or application bundle, enforce mesh budgets, cache once, and preserve a
procedural fallback. Visual replacement never changes the collision footprint.

Local people use deterministic appearance recipes. A seed and semantic role
derive stature, mass, muscularity, shoulder proportion, head shape, age, skin
tone, hair, facial hair, garment layers, leather and metal palettes, and
equipment. Guards, raiders, merchants, laborers, travellers, refugees, scouts,
and healers therefore read differently without paying for a unique skinned GLB
per background person. Recurring simulated agents retain their seed when a
scene is reset.

## Current first pass

The runtime now provides:

- A closer asymmetric exterior camera
- Clean hero and inhabitant silhouettes without default rig overlays
- Pitched roofs with gables, eaves, ridge lines, and chimneys
- Facade structure with foundations, frames, windows, doors, and canopies
- A collision-aligned civic plaza rather than one undifferentiated road patch
- Ground color response to settlement hunger and prosperity
- Authored carriage, cargo, market, mine, and bridge models in the client
- Simulation-driven shortage, enforcement, and recovery dressing
- Deterministic role-based background and animated NPC appearances
- Shared full-scene color treatment across procedural and authored art

## Next passes

1. Split the four building styles into material-economic archetypes rather than
   recolors of one massing rule.
2. Give the market hall a loading court, guarded granary threshold, and stock
   presentation driven by the three goods.
3. Add hero-occlusion-aware roof and upper-wall cutaways.
4. Define route, mine, farm, and keep grammars from their work and defensive
   needs.
5. Expand the shared style contract with material roughness, trim scale, and
   silhouette budgets before adding more environment families.
