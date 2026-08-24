# Runtime Environment Design

## Direction

Crownless Carriage environments are navigable places first and illustrations
second. The camera may be oblique, but the town must not read as a collection
of generic isometric miniatures. Every visible mass should explain where the
player can walk, what the place is for, who controls it, and what has recently
happened there.

The visual target is **a hand-painted adventure stage on a fixed pixel grid**.
Every camera room has one story focus. Every frame separates foreground,
story space, and background. Surface variation must be authored or caused by
the simulation; generic noise is only a small supporting layer.

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
leave the viewport as the hero moves. Roof and wall cutaways are driven by hero
occlusion and leave a stable waist-high shell instead of making every structure
short or opening a distracting circular reveal.

Each exterior room also stores a focal point, story axis, foreground anchor,
quiet area, three depth splits, and a light profile. The camera keeps the hero
inside both the authored quiet area and the stable play-safe frame. Roads,
rivers, walls, and building lines should lead toward the story focus.

Five stable light profiles cover clear market morning, cold shortage overcast,
warm recovery light, dangerous road dusk, and the ember-lit interior. A profile
owns its key direction, ambient and shadow colors, fog, depth strength, and
focal contrast. Lighting changes because the place or simulation calls for a
profile, not because of unbounded random time-of-day drift.

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
- Build assets from three to five main masses, one medium rhythm, and one or
  two story accents. Detail smaller than about two art pixels needs a silhouette
  or state-reading reason to exist.
- Diagnostic skeletons, contacts, and force vectors are opt-in overlays. They
  are not part of the default art presentation.
- A future Blender environment asset is production-ready only after the client
  loads it through a runtime asset registry and binds it to the same semantic
  footprint and state-layer contract.
- Strategic variables must alter at least two visible channels. For example,
  hunger affects both land maintenance and market/crowd presentation, while
  prosperity affects paving quality and visible stock or repair.
- Authored environment GLBs may use `COLOR_0` as paint data: red is broad shade
  weight, green is material class, and blue is one accent. It is semantic data,
  not a vertex tint. Runtime material rules keep cloth soft, wood warm and
  directional, stone cool and sparse, metal highlights narrow, and water
  strokes horizontal.

## Production visual style

The shared target is **grounded dark-fantasy action figures in a handcrafted
world**. Characters use adult heroic proportions, visible garment layers,
profession-readable equipment, restrained pigments, and broad details that
survive the play camera. Environments use the same bevel scale, material
families, warm-light/cool-shadow balance, and semantic state dressing. The
runtime grade applies contrast, saturation, warmth, and vignette first, then
maps code-drawn geometry and glTF assets into one shared 37-color palette.
It adds no random full-screen grain after that lookup.

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
- Pitched roofs with gables, eaves, ridge lines, chimneys, and readable dormers
- Roof construction courses and material-specific cottage, workshop, civic,
  and mine-row facade structures instead of one recolored building shell
- Facade structure with foundations, frames, deep windows, lintels, sills,
  doors, steps, and canopies
- Stable hero-occlusion cutaways that remove complete roofs and upper walls
  while keeping an inked low shell around the building footprint
- A collision-aligned civic plaza rather than one undifferentiated road patch
- A seeded, continuous exterior height field with hills, valleys, ridges, and
  smaller natural variation across farms, town, mine, and keep
- Roads that follow and gently grade the landscape, plus local foundations for
  plazas, buildings, the keep, the dungeon, and the carriage yard
- Terrain-aware walking, targeting, climbing surfaces, camera framing, and
  placement for inhabitants and world props
- A visual country apron beyond the playable border, with seeded grass tufts,
  crop stalks, wheel tracks, wet-lowland color, and slope-biased stones
- A world-seed and economy keyed GPU terrain cache, plus camera-local detail
  submission, so the full-resolution landscape is not rebuilt every frame
- Seeded ground-cover clustering, varied tree silhouettes, and directional tree
  shadows that anchor foliage to the terrain
- Kingdom-level tree calligraphy that changes crown proportion, family rhythm,
  pair clustering, vertical or spreading shape, and controlled foliage color
- Camera-forward depth bands: quiet cool distance, full-color story space, and
  darker foreground framing with selective detail
- Ground color response to settlement hunger and prosperity
- Hard-edged broad, middle, and chip-sized ground material layers instead of
  interpolated terrain blur or screen-space grain
- Authored carriage, cargo, market, mine, and bridge models in the client
- A furnished market interior with open shelving, shaped goods, framed walls,
  panelled counters, lamps, and a clear trade runner
- Simulation-driven shortage, enforcement, and recovery dressing
- Deterministic role-based background and animated NPC appearances
- Authored hero and NPC paint channels for palette role, broad value band, and
  fold shadow strength
- Filled combat wedges, tapered slashes, sparks, dust smears, and brush-stroke
  markers in place of diagnostic wire rings and spheres
- Shared full-scene color treatment and one final post-fog palette lookup
  across procedural and authored art

## Next passes

1. Apply the painted-environment channel contract to the bridge, mine, and
   carriage after each one passes a real-size mass and silhouette review.
2. Give the market hall a loading court, guarded granary threshold, and stock
   presentation driven by the three goods.
3. Add authored wear and damp masks tied to traffic, drainage, work, recent
   damage, prosperity, and control.
