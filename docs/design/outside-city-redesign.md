# Beyond the walls: parallax roads and first-person Underroad

The outside of cities becomes two connected experiences. Travel above ground uses
a side-scrolling parallax view, like the Oregon Trail. The Underroad below uses a
first-person dungeon crawler view, like `stone_dungeon_pre.png` from OpenGameArt:
tile floors, textured walls, torch light, doors ahead. Cities stay as they are.

The seam is deliberate. A side-scrolling road walk carries the carriage to a
dungeon mouth; the player walks forward into it and the view slides into
first person. Walking out reverses it. The journey does not break at the door.

This document covers everything outside city gates: roads, roadside places,
camps, encounters, the Low Silver Pit yard, the Silverwick mine, the Underroad,
goblin approaches and the dragon's mountain. It names what stays, what changes,
and the order of delivery.

## What stays authoritative

The simulation keeps owning outcomes. Presentation changes; authority does not.

- Journeys still hold one route, origin, destination, elapsed subticks, pace,
  danger, encounter state, and the roadside-stop bitmask
  (`src/sim/cc_sim.h:1798`). Scrolling distance derives from saved route
  progress. Pixels are presentation, never the campaign clock.
- Pace still trades speed against arrival time: careful, steady, and push
  advance at 24/30/38 subticks per tick while the world clock holds 30
  (`src/sim/cc_journey.c:20`). W/up and S/down keep changing pace.
- Departure rules stay: free departure, fodder at departure, spoilage, near-foal
  mares blocking, exhausted teams departing carefully
  (`src/sim/cc_journey_departure.c`).
- Encounters still resolve through parley, evade, and force with their existing
  outcomes and risks (`src/sim/cc_journey_encounter.c`).
- Roadside sites keep their identity, condition, blockers, storage, repairs,
  and clearance work (`src/sim/cc_sim.c:4137`).
- Camp, lodging, feeding, and watch rules keep their receipts and costs
  (`src/sim/cc_journey_commands.c`).
- The seven named ponies keep their identities, bonds, and quests
  (`src/sim/cc_ponies.c`). The parallax carriage shows these ponies, not
  anonymous horses.
- Save/replay authority, journal hashing, and shared-company command flow are
  untouched in shape. New presentation state stays outside campaign hashes;
  new gameplay state enters through schema migration with replay
  (`src/persistence/cc_save.c`).

## What changes

| Area | Today | Redesign |
| --- | --- | --- |
| Road view | Isometric-ish road scene with wagons, alerts | Side-scroll parallax: layered hills, road, trees, sky; carriage and ponies in profile |
| Roadside stops | Proximity window on a progress bar | The view slows and the site building appears at the roadside; stop to enter |
| Camps and inns | Auto-selected by tick wrapper, or abstract choice | Oregon Trail decisions: press on, midday rest, camp, lodge; still bound to existing costs |
| Underroad | Node graph with drawn links | First-person grid crawler; the graph remains the campaign topology and automap |
| Mine view | Overhead fixed cameras | Mine Mouth becomes the same first-person view as the Underroad |
| Dungeon navigation | Click room nodes | Step and turn through cells; typed or keyboard movement; automap on a key |
| Encounters | Text cards | Side-view standoffs on the road; face-to-face meetings in the Underroad; same outcomes |
| Entrance transition | Menu jump to a graph screen | Walk into the doorway; the camera pushes forward through it into first person |

## Travel above ground

### Scene composition

Five parallax layers, drawn back to front:

1. Sky with day-night tint and weather from campaign time.
2. Far terrain silhouette, generated from route identity and seed.
3. Mid terrain with landmarks: mills, shrines, ruins, walls.
4. Road and near ground under the carriage.
5. Foreground grass, posts, and passing details.

Scroll position maps linearly to saved journey progress. The carriage sits
lower-center; the pony team animates from their real fatigue and pace. Reverse
trips replay landmarks in canonical order so a road is one place, not two.

Scenery generation is deterministic from world seed, route id, and direction.
The renderer may cache layers, but any visible landmark must be derivable from
saved state, never from local-only time.

### Time, pace, and stops

The existing tick policy stays the single clock. The client keeps its
moving/stopped/decision-required states, and the shared host keeps its
tick-by-tick stop checks. Presentation interpolates between authoritative
positions; it never advances them.

The tick wrapper's automatic midday breaks and overnight camps become visible
choices at their moments, defaulting to the current behavior if the player does
nothing within a grace window. This keeps solo pacing fluid and co-op fair
without a new vote system; the departure-proposal work in #316 remains separate.

### Encounters on the road

An ambush warning slides the parallax to a halt and shows riders ahead at the
warning's progress point, as today around 45% with resolution near 60%. The
existing outcome commands appear as buttons over the scene. Combat presentation
reuses the current session combat state; this redesign changes how it looks,
not how it resolves.

### Roadside places

All 24 site families become visible buildings on their side of the road. The
mine branch gets a full scene: the yard, the carriage parked beside the road,
the timber doorway. Ordinary sites keep their transfers and repairs; their
interaction panel opens over the side view.

The mine's exact authoritative stop becomes the model for interactive stops:
travel clamps to the site's subtick, the site scene takes over, and returning
resumes the journey. The old proximity window remains for non-interactive
pass-by sites only.

## The Underroad in first person

### View

A grid-based first-person crawler in the stone-dungeon style of the reference:
repeating wall textures, tile floor, dark ceiling, doors and arches, warm
torchlight falloff, a chest silhouette for loot. Movement steps one cell with a
short glide; turns are 90 degrees with a short rotation. Neither animation
changes position faster than the simulation step allows; the view waits for
the authoritative step.

Web and native share the renderer path. The 256 MiB web memory budget means one
dungeon region is resident at a time, with atlased textures.

### Grid and graph

The 24-room graph remains the campaign truth: rooms, links, secrets, shortcuts,
searched flags, cleared flags, encounters, and outcomes. Each graph room becomes
one authored first-person cell block. A link becomes a doorway between blocks;
secrets and shortcuts become hidden doors that only open through play.

The player's position is a cell inside the current room's block plus a facing.
The simulation stores the position, facing, and revision with the expedition;
movement requests carry the revision like mine steps do
(`src/sim/cc_mine.c:103`). Six dungeon turns still consume one ration from the
carriage; tile steps consume light and noise exactly as the existing turn model
already does. The day boundary arrives through the same accounting, not a new
clock.

The automap survives: a key or button overlays the discovered graph with the
player's marker, so the graph keeps its campaign role.

### The natural seam

Side-scrolling road, roadside branch, mine yard, doorway, first person:

1. The carriage stops at the Low Silver Pit branch; the yard scene appears.
2. Pack food, walk to the timber doorway as today.
3. Walking into the doorway keeps the camera moving forward; the doorway fills
   the screen, a beat of darkness, and the first-person view opens inside.
4. The same forward walk out reverses it: darkness, the doorway recedes, the
   yard scene, the carriage, the road.

The same seam serves every Underroad entrance: the mine's Lamp Hall stair, the
goblin trail cave mouth, the dragon's mountain gate. The transition is one
shared camera animation, authored once.

### Controls

- W/S/forward and back: step one cell; A/D or arrows: turn 90 degrees.
- E or Use: doors, bars, surveys, searches, loot, campfire.
- M: automap overlay of discovered graph rooms.
- Touch: visible step, turn left/right, use, and map buttons; no pointer lock.
- Escape: pause and save as today.

Keyboard repeat and step costs reuse the mine's revisioned-step pattern so a
held key cannot double-step. Reduced-motion preference replaces glides with cuts.

### Light, sound, and dread

Torch radius comes from remaining light. Noise raises encounter chances through
the existing rules. Footsteps, drips, distant bell tolls, and heartbeat-adjacent
lows use the existing soundscape cues with new adapters, because current audio
follows locomotion that first person will not run
(`src/client/main.c:9925`). The eaten-bell fragments render as readable
plaques in world space, not pop-up text.

## Cities stay

The town street, market, interiors, services, conversations, the Company Book,
and gate arrival keep their current renderer and flow. Departure and arrival
are the seams: the gate scene hands the carriage to the road view and takes it
back. No shared town helper changes in the early phases.

## Persistence and shared play

- Dungeon position, facing, and region enter saved state through one schema
  migration with journal replay and old-save upgrade, following the mine's
  schema-59 example (`docs/design/silverwick-mine-slice.md:25`).
- Shared-company expeditions keep one shared position. The control convention:
  any member may move; the host serializes; a use action's receipt binds to the
  revision like mine steps already do.
- Save, reload, reconnect, and duplicate-use tests run at every transition:
  town exit, road, yard, doorway, dungeon, return.
- The legacy graph commands keep working during migration, as the mine slice
  allowed both systems to coexist.

## What does not change

- Campaign economy, gossip, quests, lifecycles, war, dragons, goblins.
- Route rules, tolls, danger, and freight.
- Encounter outcomes and their risks.
- Pony identity and bonds.
- Save hashing, replay, and co-op command publication.
- The graph as data and automap.

## Delivery phases

1. **Seam proof.** One route, both directions, in parallax; the mine branch,
   yard, doorway transition into the existing six-chamber mine in first person;
   return the same way; save/reload and shared commands at every step. This is
   the vertical slice that proves the whole loop.
2. **First-person Underroad.** Map graph rooms to cell blocks with doors,
   secrets, searches, encounters, and the automap; retire the graph screen when
   parity holds.
3. **Roadside places.** All 24 site families visible and enterable where they
   have interiors; clamp interactive stops to exact subticks.
4. **Camps, inns, encounters.** Oregon Trail decisions surfaced over the side
   view; combat presentation over existing session combat.
5. **Goblin trail and dragon mountain.** The remaining exterior families in
   first person; the eaten-bell fragments become plaques in the Underroad
   instead of panel text.

Each phase ships behind the existing capture and test harness, with
`--benchmark-render` guarding frame budget and the web size checks guarding
memory.

## Acceptance for the first phase

- Leave a city, cross one route, stop at the branch, enter the mine, return,
  and arrive, with save/reload at each transition and both directions.
- Pace changes, feeding, spoilage, and encounter outcomes match current tests.
- The doorway transition reads as walking forward, with no loading screen.
- Touch and keyboard both complete the loop; reduced motion holds.
- Native and web builds pass existing CI, including the WebGL memory budget.

## Open decisions

1. Facing: authoritative state (saved, replayed) versus local camera comfort.
   Recommendation: authoritative, for shared play and encounters.
2. Loot flow in the Underroad: pack first then carriage at the yard, or direct
   to carriage cargo. Recommendation: pack first, matching the mine.
3. Whether ordinary sites with interiors beyond the yard enter first person in
   phase 3 or stay panels until phase 4.
4. Whether the graph automap is a flat map or a rotated minimap. Flat first.
