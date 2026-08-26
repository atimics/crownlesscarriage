# Crownless World Kit

## The decision

Crownless should be built like an **80s action-figure line and its playsets**.
This is a construction rule, not a plastic-toy material effect.

The world uses a small set of strong, compatible parts. A person is assembled
from a skeleton frame, inflatable muscle modules, mixable soft-tissue modules,
a derived skin surface,
fitted clothing and armour shells, a head, molded hair or headgear, and bold
equipment. A building is assembled from foundations, frames,
facade bays, thresholds, roof forms, work modules, and state dressing.

The result should feel designed, sturdy, and slightly exaggerated. It should
not look like Lego, Playmobil, a collection of cubes, or a one-off character
sculpt.

The V08 and V09 hero experiments stop here. They remain useful evidence, but
they are not the source of the next character. The next hero is a recipe made
from the approved shared kit.

## What we keep and what we replace

The current project already has several useful foundations:

- one metre is one world metre
- generated source assets and stable manifests
- a shared physics skeleton and rigid character modules
- semantic material colors instead of unique textures for every person
- working carriage sockets
- additive shortage, enforcement, and recovery state layers

Those systems stay. The weak part is the shape grammar:

- the character library has one body family and one head family
- roles are created largely by freely scaling the same parts
- the hero has been treated as a separate final model
- the environment library exports large finished sets instead of reusable bays
- procedural houses still begin as one box with details attached to it

The new kit replaces free scaling and finished one-off models with named shape
families and strict attachment rules.

## Lessons borrowed from modular scene systems

The kit borrows structure from digital construction systems, not their visual
style.

### LDraw: parts are references, not copied geometry

An LDraw model is a small text scene graph. A type-1 line names a part and gives
it a color, position, and 3 × 3 transform. Parts can reference subparts, and the
library gives parts stable names, categories, keywords, winding rules, and
validation. Crownless adopts the same basic split:

- a published component owns geometry
- an assembly stores component IDs and transforms
- repeated geometry is referenced rather than copied
- categories and search keywords are part of the asset contract
- counter-clockwise winding and outward normals are validated
- a recipe may contain smaller named assemblies

LDraw's inherited color also suggests an important Crownless rule: a reusable
part declares material roles, while the assembly recipe supplies the final
palette. A tunic is not permanently teal and a pauldron is not permanently tied
to one faction. See the official [LDraw file format specification](https://www.ldraw.org/article/218.html),
[category and keyword extension](https://www.ldraw.org/article/340.html), and
[back-face culling extension](https://www.ldraw.org/article/415.html).

### LDCad: keep snap data beside the shape

LDCad adds connection information through a shadow library. A shadow file has
the same identity as an official part and adds snap points without changing the
official geometry. Snap data can be included, inherited, cleared, and replaced.

Crownless follows this directly. Mesh exports remain visual components. A
separate connection file owns sockets, grip cylinders, surface envelopes,
clearance, orientation, compatibility, inheritance, and overrides. We can fix a
bad grip or add a new compatible socket without regenerating every mesh. See
LDCad's [shadow library](https://www.melkert.net/LDCad/tech/shadowLib) and
[snap metadata](https://www.melkert.net/LDCad/tech/meta) documentation.

### OpenUSD: components, assemblies, and named variants

OpenUSD separates terminal components from assemblies that reference them. Its
VariantSets provide named, switchable alternatives, and stronger layers can
add or override scene decisions without damaging the source asset.

Crownless uses the same ideas in a smaller JSON contract:

- `component` is a terminal published body part, shell, prop, or building piece
- `assembly` is a figure, place, or playset made from component references
- `state_layer` adds shortage, control, damage, repair, or prosperity
- `skeleton`, `muscle`, `soft_tissue`, `head`, `hair`, `wear`, `roof`, and `state` are named variant sets
- variants replace unbounded three-axis scaling

See the official OpenUSD introduction to [references, payloads, and VariantSets](https://openusd.org/release/intro.html)
and its [model hierarchy](https://openusd.org/release/api/_usd__page__properties_of_scene_description.html).

### glTF: delivery, metadata, and instancing

glTF remains the runtime delivery format. It supports node hierarchies and
application data in `extras`, while repeated facade, prop, and crowd components
are candidates for GPU instancing. The readable JSON manifests remain the
source of truth; the same stable IDs are also embedded into GLB extras for
inspection. See the official [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

## What “80s action figure” means here

- Adult, heroic proportions rather than child or mascot proportions.
- Strong chest, shoulder, boot, glove, hair, and equipment shapes.
- Clear plane changes around joints, clothing layers, and armour.
- A small number of molded details that survive at the game camera.
- Controlled asymmetry: one shoulder piece, one side lock, one tool, one cape,
  or one large belt item can carry identity.
- Parts meet at believable clothing seams and hard edges. The construction
  system stays mostly hidden inside the design.
- Color is grouped into large areas. Small paint operations are reserved for a
  face mark, emblem, metal edge, or other identity signal.
- Accessories look made to be held, worn, carried, and swapped.

It does **not** mean plastic shine, visible toy screws, universal round heads,
mitten hands, peg feet, or identical bodies in different colors.

## One scale for the whole world

One world unit is one metre. All kits use the same figure standard.

| Measure | Standard | Allowed use |
| --- | ---: | --- |
| Adult body, heel to scalp | 1.90 m | Main reference for all human modules |
| Normal adult body range | 1.75–2.02 m | Set by skeleton dimensions, never final-model scale |
| Hair and headgear allowance | up to 0.12 m | May exceed the body height without changing the body |
| Door clear opening | 1.02 × 2.10 m | Existing town doors already follow this closely |
| Door outer frame | about 1.30 × 2.24 m | Keeps the opening readable at play scale |
| Counter or workbench | 0.92 m high | Existing market counter standard |
| Seat | 0.46 m high | Shared furniture and pose target |
| Rail or guard | 1.05 m high | Shared environment contact standard |
| Normal floor-to-floor height | 2.8–3.2 m | Keeps buildings related to the cast |
| Public travel lane | at least 2.5 m | Fits people, encounters, and small traffic |

The hero uses the same standard skeleton and scale rules as everyone else. Hero
status comes from silhouette, costume, animation, and color—not from a hidden
global size multiplier.

No renderer may scale the complete head, hair, or character to fix a design
problem. Proportion changes belong in a named skeleton or anatomy family and must be
visible in the scale lineup.

## The figure kit

### 1. Skeleton frames

Start with three reusable skeleton frames. They own bone length, shoulder and
hip joint placement, posture limits, hand grips, foot contacts, and equipment
sockets. They contain no muscle, body fat, clothing, or armour.

| Frame | Shape | Main use |
| --- | --- | --- |
| Lean | Narrow shoulder and hip frames, light joints | Scouts, healers, young adults, quick roles |
| Standard | Balanced shoulder and hip frames | Hero, travellers, guards, most townspeople |
| Heavy | Broad shoulder and hip frames, planted joints | Laborers, veterans, merchants, heavy guards |

These are different joint arrangements, not a dressed body stretched on three
axes. All three use the same named bone, muscle-bed, soft-tissue-bed, garment,
and equipment sockets.

Gameplay proportions should stay around 5.75–6.25 heads tall. This keeps faces
readable at roughly 60 pixels without creating a balloon-headed figure. Hands
and boots may be about 10–15% larger than neutral anatomy, but must keep fingers,
soles, and directional shape rather than becoming blocks.

### 2. Muscle modules and inflation

Muscle pieces click onto named beds on the skeleton: chest, back, abdomen,
glute, deltoid, upper arm, forearm, thigh, calf, and neck. Each module has a
controlled inflation value between zero and one.

Inflation changes only the local cross-section. It may not change bone length,
move a joint, move a socket, or scale the complete figure. A muscle recipe may
use different values per group, so a broad laborer, narrow runner, and strong-
armed smith do not require three complete bodies.

### 3. Soft-tissue modules

Soft-tissue pieces sit over the muscle envelope and add local padding in
metres. The initial regions are chest, abdomen, waist, hip, upper arm, forearm,
thigh, and calf. Every region may be mixed independently.

The `low`, `balanced`, `central`, and `lower-body` recipes are safe starting
points, not locked body types. A character may combine a low-tissue chest,
central abdomen, broad hips, and average limbs without creating a new complete
body mesh.

Soft tissue may smooth and enlarge a local cross-section. It may not change
bone length, joint centres, posture, muscle attachment points, or equipment
sockets. The approved range is zero to 0.06 m of local padding.

### 4. Derived skin surface

The skin surface is generated from the skeleton, inflated muscle, and soft-
tissue envelope. It is an output, not a clothing mesh and not the source of
anatomy. The runtime body is one continuous neck-down molded surface, including
bare hands and feet. The head remains a separate identity part at the hidden
neck mount.

The first runtime set bakes all 36 safe combinations of three frames, three
muscle profiles, and four soft-tissue profiles. They share the same 18 bone
names and are skinned on the GPU. Clothing, boots, armour, head, hair, and props
remain separate pieces fitted over that body. No bare elbow, knee, wrist, or
ankle is assembled from visible blocks.

### 5. Head families

Do not make every face by scaling one head. Begin with four reusable head
families:

- straight brow and square jaw
- long face and narrow jaw
- broad cheek and short jaw
- older face with stronger brow and nose plane

Each family owns the skull, jaw, nose base, ear placement, and face-glyph
anchors. Small identity values may change brow, nose, beard, age, scar, and skin
tone inside that family.

The back of every head must be a rounded skull volume. Hair may sit over it, but
must not replace it with a flat wall or rectangular shell.

### 6. Molded hair pieces

Hair is assembled as a small number of opaque tapered clumps around a hidden
scalp core.

- Use 4–8 large clumps for a normal style.
- Give every clump a root, path, width, thickness, and tapered tip.
- Use triangular or diamond-like sections rather than flat cards.
- Use gaps only between the largest locks.
- Treat bangs, side locks, crown mass, and rear mass as named slots.
- Long styles may add one or two simple motion chains; roots remain firm.
- Use two large hair values and at most one controlled highlight plane.

Do not use a rectangular back shell, flat top plate, sphere cap, transparent
cards, or thin strand noise.

### 7. Garment and armour shells

Garments fit the derived body envelope with cloth clearance. Armour fits above
the garment envelope with hard-shell clearance. They create role and status
without defining or replacing the body below them.

Required shell slots:

| Area | Example families |
| --- | --- |
| Chest | tunic, coat, vest, cuirass, apron bib |
| Waist | belt, sash, skirt tabs, coat tails |
| Shoulder | mantle, single pauldron, paired armour, hood base |
| Forearm | cuff, bracer, glove top |
| Shin | boot top, greave, wrapped cloth |
| Back | cape, pack, bedroll, shield, tool rack |
| Head | hair, hood, hat, helmet, crown or emblem |

A shell declares which skeleton frames and envelope ranges it fits. Runtime
scaling outside a narrow fit range is rejected. If a shell does not fit two frames well, make two fitted
versions that share the same style ID.

### 8. Figure sockets

All skeleton frames expose the same sockets:

- left and right hand grip
- left and right forearm
- left and right shoulder
- chest front
- upper back
- lower back
- left and right belt
- head top
- face front
- left and right foot contact

Every socket has a fixed origin, forward direction, up direction, size class,
and allowed load. Parts attach to sockets; they do not guess their position from
the final mesh bounds.

## Props and equipment

Props are action-figure accessories: simple, readable, and clearly connected to
the body or world.

Use four size classes:

| Class | Typical size | Examples |
| --- | --- | --- |
| Hand | 0.25–0.60 m | knife, cup, hammer, lantern, book |
| Two-hand | 0.80–1.40 m | sword, axe, shovel, crossbow |
| Pole or back | 1.40–2.20 m | spear, staff, banner, long tool |
| World | human-scaled | crate, barrel, counter, cart, shrine |

Each held prop declares a grip point, a second-hand target when needed, its
carried socket, contact points, and a safe motion envelope. A prop can be
slightly oversized for readability, but its function and weight must still make
sense beside a door, hand, and counter.

Small surface detail is replaced by molded ridges, large wraps, blade planes,
guards, pommels, straps, and emblems. One clear accessory is better than five
tiny belt objects.

## The building and playset kit

Buildings use action-figure **playset logic**, not building-block visuals.
They should look like believable places built from large, repeatable structural
parts.

### Structural pieces

- foundation edge, corner, stair, and raised pad
- 1.5 m facade bay and 3.0 m double bay
- solid wall, window wall, door wall, and work opening
- inside and outside corner frame
- post, beam, brace, lintel, sill, and balcony edge
- straight roof span, gable end, hip corner, ridge, and lean-to
- chimney, awning, porch, loading platform, and sign mount

The 1.5 m bay is a layout guide, not a visible block grid. Final walls overlap
and join so the building reads as one structure.

### Place modules

Structural parts gain meaning through larger work modules:

- market counter and stock wall
- forge hearth and workbench
- granary door and hoist
- stable bay and hitch rail
- guard gate and inspection table
- home hearth and storage wall
- mine portal and ore handling

Each place should be built from three to five main masses, one repeating rhythm,
and one or two story accents. Roof wedges, porches, towers, awnings, stairs, and
work openings must break the plain box silhouette.

### State modules

Prosperity, shortage, control, damage, repair, and occupation are additive
parts. They attach to named building or ground sockets and never replace the
base structure.

Examples include stocked or empty baskets, repaired or broken braces, faction
signs, ration barriers, guard furniture, scaffolds, carts, and displaced-person
bundles.

## Shared material language

Characters, props, vehicles, and buildings use the same material families:

- warm skin
- cloth and dyed leather
- dark natural leather
- warm directional wood
- cool sparse stone
- narrow-highlight metal
- glass, water, and emissive accents

At play distance, a figure gets two or three large color masses plus one small
accent. A building gets a base material, a frame or trim family, a roof family,
and one controlled faction or state accent.

Shape carries most of the design. Texture and vertex paint support the planes;
they do not create detail that the geometry and silhouette failed to provide.

## Procedural recipe grammar

The generator builds from semantic choices:

`skeleton frame + muscle recipe + soft-tissue recipe + derived skin + garment layers + armor/loadout + palette + wear`

An example recipe is:

`standard frame + slight muscle + central abdomen + broad hips + long-face head + trousers + road coat + left mantle + satchel + dark teal/oxblood + worn hems`

The generator follows these rules:

1. Pick a skeleton frame and stature inside its approved range.
2. Click muscle modules onto their bone-local beds and apply one controlled
   inflation recipe. Inflation changes cross-section, never bone length.
3. Add soft-tissue values per region or begin from a safe recipe. Soft tissue
   may add volume but may not move the frame or its sockets.
4. Derive the skin surface from the combined muscle and soft-tissue envelope.
5. Pick a head family independently from role.
6. Fit trousers, boots, tunics, and other garments over the body envelope.
7. Fit armor and carried gear over the garment envelope.
8. Pick one main silhouette piece, usually on the shoulder, back, head, or hand.
9. Check all compatibility tags, clearances, and motion envelopes.
10. Apply a controlled palette and material wear state.
11. Use a stable seed so the same person keeps the same parts.

Role changes the probability of a part. It does not force all guards, merchants,
or refugees onto one body and head.

## Module data contract

Every reusable part must declare:

- stable ID and version
- slot and anchor socket
- compatible skeleton, muscle-bed, soft-tissue-bed, garment, building, vehicle,
  or prop families
- world-space bounds at standard scale
- forward and up directions
- allowed scale range, normally very narrow
- silhouette tags such as broad, tall, rear-heavy, or left-heavy
- material roles and allowed palette regions
- collision or contact shape when required
- motion envelope and secondary-motion chain when required
- triangle budget and distance rule

Missing data is a failed module, not permission for the renderer to guess.

## Composition limits

To stop procedural output from becoming noisy:

- one dominant silhouette idea per person or prop
- one secondary counter-shape
- one controlled asymmetry
- no more than three large color masses and one accent on a person
- no stacked headwear unless the pair is authored as compatible
- no cape, pack, and large back weapon fighting for the same space
- no large shoulder piece on both sides unless the role calls for a rigid,
  formal silhouette
- no tiny detail whose only purpose is random variation

## What we reject

- one bespoke complete mesh for each named character
- one universal body stretched freely in width, height, and depth
- final-model or renderer scale used to fix proportions
- large round or square heads shared by the whole cast
- sphere-cap or box-shell hair
- mitten hands, peg feet, tube limbs, and box torsos
- every attachment centered and symmetrical
- accessories that float because they lack sockets
- buildings made from unbroken boxes with decoration pasted on top
- visible studs, toy-block seams, or glossy plastic as the main style signal
- random color and prop selection without role, place, or state meaning

## Review sheets and acceptance gates

The kit is reviewed before a hero or named NPC is designed.

### Sheet 1: world scale lineup

Show the three skeleton frames, their derived bodies, four head families, a
door, counter, seat, rail, cart,
facade bay, and small house in one orthographic scene. Show their dimensions.
Reject any part that needs a final-model scale exception.

### Sheet 2: figure parts board

Show every skeleton part, muscle module, soft-tissue module, derived skin part,
garment, and armor shell on its named socket. Show the same garments and armor
on every compatible body envelope from front, three-quarter, side, and back
views.

### Sheet 3: procedural cast

Generate at least 24 stable recipes. Review them first as solid black
silhouettes at the real game size, then with two-tone shading. The cast must
look related without looking cloned.

### Sheet 4: motion and contact

Show walk, run, guard, strike, climb, sit, carry, and knockdown tests. Hands,
feet, clothing breaks, props, and back items must stay connected and readable.

### Sheet 5: playset combinations

Build at least three different structures from the same facade, threshold, and
roof kit. Add two world-state variants to each without changing its collision
footprint.

### Final game check

Place the hero, three NPC recipes, a doorway, market counter, cart, and house in
one normal camera frame. Check the 60-pixel silhouettes, two-tone shading, and
walk cycle. The hero may be clearer than the NPCs, but must belong to their
physical world.

## First production slice

Do not rebuild the full cast at once. The first kit review should contain:

- eight reusable skeleton parts arranged into three frame families
- ten reusable muscle modules and three inflation recipes
- eight reusable soft-tissue modules and four safe starting recipes
- derived skin surfaces for the approved frame, muscle, and tissue combinations
- four heads
- six molded hair families
- trousers, boots, and tunics as separate garments
- two armor chest shells, two waist shells, two shoulder shells, and two back shells
- one glove, bracer, boot, and greave family
- eight held or carried props
- one 1.5 m facade kit with door, window, corner, roof, awning, and sign pieces
- one market work module and two state variants

Use those parts to make the Crownless hero, a guard, a merchant, a laborer, and
a traveller. If those five figures and one small market corner work in the same
scale lineup, the kit is ready to expand.

## Build order

1. Add the shared scale table, socket definitions, and compatibility fields to
   the manifests and validators.
2. Build the reusable skeleton parts and assemble the three approved frames.
3. Build muscle modules that inflate only across each bone.
4. Build soft-tissue modules that add regional padding without moving joints or
   sockets, then derive the skin surfaces and add the four heads.
5. Produce the world scale lineup and correct it before making more parts.
6. Add the garment, armor, hair, and prop set from the first production slice.
7. Generate the five-person procedural cast and test it at the real camera.
8. Build the facade and market corner from shared playset pieces.
9. Only after those passes, assemble the Crownless hero as a normal recipe with
   a unique combination of shared parts.
