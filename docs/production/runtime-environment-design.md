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

The walkable exterior uses an asymmetric three-quarter orthographic camera.
Orthographic projection preserves reliable surface picking and a readable
relationship between the hero and nearby contact geometry. The unequal
horizontal offsets avoid the visual shorthand of a perfect 45-degree isometric
grid and expose more of the camera-facing facades.

This is a street-scale view, not a whole-settlement map. Buildings may frame or
leave the viewport as the hero moves. Roof and wall cutaways are driven by hero
occlusion and leave a stable waist-high shell instead of making every structure
short or opening a distracting circular reveal.

Each town exposes exactly three camera scenes: Arrival, Town Heart, and
Landmark. Arrival is a continuous perspective move that begins behind the
carriage and pulls upward and outward as the team crosses the threshold. Town
Heart and Landmark are fixed adventure-game stages. Each stores a focal point,
story axis, foreground anchor, quiet area, three depth splits, and a light
profile. The camera keeps the hero inside both the authored quiet area and the
stable play-safe frame. Roads, rivers, walls, and building lines should lead
toward the story focus. Navigation may use finer hidden districts, but they do
not create additional visible camera scenes.

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

- **Why does this place exist?** Agricultural basin, road market, mine,
  fortress, capital, frontier depot, or another material function.
- **How does work shape it?** Cart clearances, storage, drainage, livestock,
  workshops, hoists, defensive sightlines, and worker routes create its plan.
- **Who built and controls it?** Kingdom materials, faction symbols, permitted
  entrances, toll infrastructure, and repair quality establish power.
- **What condition is it in today?** Hunger, prosperity, security, traffic,
  weather, and recent events alter dressing and behaviour.

This produces bounded variation with meaning. Random roof colors and scattered
props do not constitute a city grammar.

### Place profile contract

Every settlement function owns a local place profile. The profile supplies a
stable terrain seed, ten room names, a statement of purpose, civic labels, a
material tint, a low plaza mark, and three authored landmark records. More
importantly, it owns the complete building plan and the civic compound plan:
footprints, heights, styles, doors, structure kinds, and the primary hall.
Thornford is dispersed around barns and crofts, Gloamgate gathers tightly
around a radial bazaar, Alderwatch forms a straight bridge garrison,
Silverwick climbs in dense worker rows, Rosespire uses tall processional wards,
and Hollowbarrow leaves open ground around its lantern ward.

Each profile also owns three terrain-scale ideas rather than inheriting one
generic rolling field. Thornford crosses a river shelf toward a threshing green
and hill granaries. Gloamgate rises from its customs causeway into a bazaar bowl
and raised keep. Alderwatch spans a ravine and climbs a fortified muster spine.
Silverwick cuts into a quarry floor below worker terraces and the lower
silverworks. Rosespire follows a processional rise through civic terraces to
the palace. Hollowbarrow gathers around an expedition hollow below the sealed
dungeon ridge. Retaining walls, bridges, rails, paving, water, and standing
stones make those landforms legible at the play camera.

The visible town is also the foreground of a much larger realm. Every exterior
horizon uses deliberate depth bands: a remote landmass and a regional ridge,
with a constructed scale cue where it improves the composition, such as the
fortress pass wall. These horizons are scenic geometry only; they add regional
scale without expanding navigation, collision, or camera-room count.

The current 30-scene runtime review is captured in
[`town-scene-sheet.png`](../images/town-scene-sheet.png). Its rows are the six
towns. The first three columns are Arrival, Town Heart, and Landmark: wide
establishing pages that explain the whole place. The last two columns are
close, playable rooms built around local work and story: a drovers' close,
furnace alley, arcade, armourers' row, rose cloister, lantern gate, and their
counterparts. The close lenses show roughly half as much vertical world space
as the establishing lenses, so the actor, doors, signs, stalls, and tools can
carry a scene without turning the outdoor game into a follow camera.

### Research-to-layout rules

The fixed camera is part of the level plan. Roberta Williams describes the
original *King's Quest* rhythm as walking through a storybook where turning a
page should reveal something new. The town therefore changes composition at
authored thresholds; it does not keep one distant camera moving beside the
hero. The wide pages establish identity, while the close pages pay that
identity off with a person-sized place and a clear possible interaction.
[Roberta Williams' design notes](https://robertawilliams.com/design-philosophy/)

Each close page uses a foreground edge, one dominant, one quieter
counterpoint, and a visible route through the frame. Organic working streets
use asymmetric weight. The fortress gate alone uses strict frontal symmetry,
because the controlled entrance is the subject. These choices follow the
foreground/interest/background layers, observation spots, leading lines, and
static-versus-dynamic composition described in this environment-composition
study.
[Composition in Level Design](https://www.gamedeveloper.com/design/composition-in-level-design)

The medieval plan also supports the camera plan. Castle borough research at
Longtown describes a broad market street stretching from the castle gate with
long plots behind it. Northallerton preserves an even more cinematic pattern:
a managed approach funnels through a narrow entrance, then opens onto the
high-status facade. Our public spine, market heart, and gate reveal use the
same spatial sentence without copying either site.
[Historic England: Longtown](https://historicengland.org.uk/research/results/reports/7322/LongtownHerefordshire_amedievalcastleandborough)
[Historic England: Northallerton](https://historicengland.org.uk/listing/the-list/list-entry/1020719)

Finally, the close rooms stay fixed during ordinary movement and change behind
a brief page fade. That gives them the authored cinematic value of a fixed
third-person angle while leaving the character directly controllable.
[GDC: *Alone in the Dark* postmortem](https://gdcvault.com/play/1015485/Classic-Game-Postmortem-Alone-in)

The six plans share proven travel corridors and camera-room thresholds, not a
generic town underneath. Their different massing, density, skyline, hall, and
compound are authoritative for terrain pads, collision, pathfinding, camera
clearance, picking, cutaways, labels, and rendering. Only Gloamgate uses the
authored market model; the other civic halls use their own procedural
architecture instead of wearing the market as a skin.

The building renderer now treats each gameplay footprint as a small kit of
connected masses. Thornford uses tall barn naves, lean-to sheds, and cottage
annexes; Silverwick stacks timber upper floors over stone workshops and adds
headframes; Gloamgate wraps arcaded trade bays; Alderwatch joins barrack blocks
to defensive towers; Rosespire steps formal wings around tall pavilions; and
Hollowbarrow overlaps rough lodges with old round turrets. The footprint stays
authoritative for collision, but the visible result is no longer one decorated
cube with the same roof in every town. Secondary landmarks now use those same
local kits, so the foreground customs house, watch post, workshop, and lantern
lodge reinforce the town silhouette instead of falling back to box props.
The close-room pass also replaces generic roadside dressing where repetition
became obvious: the shared east prop becomes Thornford's grain store,
Silverwick's ore hopper, Gloamgate's kiosk, Alderwatch's supply block,
Rosespire's rose cloister, or Hollowbarrow's salvage winch. Workshop details
similarly distinguish a mining tram, armourer's polearms, and a goldsmith's
counter.

Regional relief and civic access are separate scale decisions. Ravines,
quarries, ridges, and river shelves can remain severe, but a public keep court
rises only as far as its long gate road can plausibly climb. The complete
castle approach is tested at no more than a fourteen-percent grade. The
terrain grading footprint still includes drainage and movement shoulders,
while the visible road is narrower; this keeps carts safe without making every
street look as wide as a house.

Each landmark record owns its name, family, variant, footprint, and height.
Terrain grading, physical sweeps, pathfinding, camera picking, nearby labels,
and visible construction all consume that same record. The profile is not
allowed to change collision by merely hiding geometry. This keeps local
differentiation honest and gives future place expansion one small, testable
interface.

Secondary-road records own their footprint, direction, surface family, and
name. Terrain grading, surface color, wheel ruts, and nearby road labels consume
the same record. Each road overlaps its destination landmark and joins the
shared primary spine, so a profile cannot add a disconnected decorative road.

Building records are bounded, non-overlapping, and kept clear of their town's
landmarks and compound. Every primary hall retains a tested public threshold,
and every compound leaves the Crown Gate lane physically open. Profile tests
also reject any pair of settlement functions with the same building map.

The civic interior is reusable geometry rather than a universal market. Its
keeper, service description, material palette, wall mark, and role come from
the current place profile. The counter, shelf, exit, and navigation envelope
stay shared and tested; the local purpose and person change with the town.

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

- Five authored camera scenes per town: three establishing pages, two close
  playable rooms, and a moving carriage follow-and-pull-out Arrival
- Bounded close-page triggers, so the main road keeps its establishing view
  until the player actually enters a court, alley, cloister, or gate pass
- Clean hero and inhabitant silhouettes without default rig overlays
- Pitched roofs with gables, eaves, ridge lines, chimneys, and readable dormers
- Thick sloped roof planes, raised shingle battens, cantilevered storeys, and
  material-specific cottage, workshop, civic, and mine-row facade structures
  instead of one recolored building shell
- Facade structure with foundations, frames, deep windows, lintels, sills,
  doors, steps, and canopies on both camera-facing sides
- Stable hero-occlusion cutaways that remove complete roofs and upper walls
  while keeping an inked low shell around the building footprint
- A collision-aligned civic plaza rather than one undifferentiated road patch
- Six seeded, continuous terrain grammars with rivers, quarry cuts, a bazaar
  bowl, a defensive ravine, processional terraces, an expedition hollow, and
  smaller natural variation
- Roads that follow and gently grade the landscape, plus local foundations for
  plazas, buildings, the keep, the dungeon, and the carriage yard
- Terrain-aware walking, targeting, climbing surfaces, camera framing, and
  placement for inhabitants and world props
- A visual country apron beyond the playable border, with seeded grass tufts,
  crop stalks, wheel tracks, wet-lowland color, and slope-biased stones
- A regional horizon for every town: rolling granary hills, quarry teeth,
  crossroads valley layers, fortress peaks, capital spires, or dungeon crags
- A reserved town-edge handoff for later procedural wilderness exploration;
  the wilderness itself is not part of this pass
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
