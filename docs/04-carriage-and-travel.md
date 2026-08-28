# Carriage and Travel

## The Crownless company

The player operates an independent carriage company under an old charter. The
charter provides a reason to cross borders, serve incompatible factions, carry
sealed messages, and enter disputes without placing the player above the world.

The company provides continuity across procedural campaigns through:

- The carriage
- A small recurring crew
- Passenger relationships
- Depots and safehouses
- Reputation and legal status
- Routes opened, damaged, or transformed by prior journeys

## Capacity creates decisions

The carriage has a small number of understandable capacities:

- Passenger seats
- General cargo
- Supplies
- Guard or scout positions
- Specialized modules
- Map-case slots

A royal courier and sealed dispatch use the company's one accepted-charter
slot. The court's war, peace, alliance, or muster decision remains only an
intention until the carriage reaches the named royal seat. Encounters can
damage the message's reliability; completing the road journey delivers the
information and only then changes the political simulation.

Passengers and cargo compete directly. Carrying refugees may mean leaving
commercial goods behind. An armoured coach may survive ambushes but arrive too
late. A hidden compartment may occupy the space needed for medical treatment.

## Carriage modules

Progression unlocks new choices rather than only larger numbers:

- Armoured body
- Light racing suspension
- Medical bunk
- Additional passenger bench
- Expedition supply rack
- Hidden contraband compartment
- Monster cage
- Relic containment chamber
- Scout perch
- Diplomatic document safe

Modules should create political, narrative, and route opportunities in
addition to modifying combat or capacity.

## Walker-chassis hypothesis

**Hypothesis:** the Crownless Carriage is an old articulated walker built to
keep using imperial roads after bridges, causeways, and mountain cuts began to
collapse. This would make the generalized limb system part of the central
travel decision rather than an unrelated character-animation feature.

The walker must expand route choice without erasing geography:

- An intact road remains faster, quieter, cheaper, and easier on cargo.
- A ruined direct road may be crossable, but strains particular limbs, consumes
  replacement parts, exposes the carriage at bottlenecks, and risks a fall.
- The intact detour costs days and provisions and may cross monster habitat,
  hostile jurisdiction, or a smuggling corridor.
- Cargo mass, distribution, passengers, weather, traction, and limb damage
  change which crossings are currently safe.
- Losing a limb changes the available gait and support margin instead of merely
  subtracting vehicle hit points.
- Repairs require mechanics, salvage, specialist workshops, or black-market
  components, tying locomotion back into settlement economies.

The vertical slice should retain this hypothesis only if route tests show a
legible choice between a dangerous physical crossing and a costly ecological
detour. A restricted road remains passable, but sanctions, tolls, low freight
capacity, poor condition, and encounters must make that choice materially
different from an open road.

## Roads before maps

**Contract:** there is no free, omniscient kingdom map, and travel never starts
by selecting a line on a chart. At the edge of a place, the player chooses one
of the roads that physically leaves it. The choice shows only the immediate
fork, not the wider route graph. The fork is a local carriage scene with the
coach, road surfaces, signs, vegetation, damage, and closures in the world.

A map is a tradeable set of traveller's notes carried in the carriage map case.
Each sheet describes one route and its two termini.

A chart records:

- Stable map and route identity
- Maker and place of origin
- Survey date and accuracy
- The road condition and danger claimed when surveyed
- Legal or contraband status
- Current owner and market price

The carriage carries only a small number of sheets. Buying better road notes may
mean selling the illicit tunnel chart needed later. A settlement may sell maps
made there, while smugglers trade suppressed routes.

Owning a chart can reveal the end of an unmarked track, reduce wayfinding delay,
and provide old claims about danger and condition. It does not create or unlock
the road. An uncharted fork can still be taken, but the player may not know its
destination and the journey is slower and more dangerous. A local guide can
provide the same immediate wayfinding help without transferring a map.

Every cartographer chooses different names, omissions, and warnings. The notes
are arguments about the road, not live telemetry. Closures, troop movements,
or new dangers may therefore contradict the ink.

The player selects a physical road, manifest, companions, and legal posture
before committing. Maps remain supporting evidence. Wider atlases or copying
equipment may later be carriage modules, but cannot restore a universal
strategic screen.

## Travel commitment

A departure creates a persistent journey whose exact duration is measured by
the authoritative world clock. The company remains at the origin for command
validation while the carriage records its route, endpoints, progress, speed,
condition, and reserved fare. Every 60 Hz travel tick moves that state forward;
calendar boundaries run the same daily world update used by headless play.
Arrival changes the company location only after the full duration has elapsed.

Before confirmation the game presents:

- Expected travel time
- Known route costs
- Cargo and passengers
- Known inspections or legal conflicts
- Current danger sources
- Contract deadlines affected
- What must be left behind

Information may be incomplete, but mechanical stakes should remain legible.
Uncertainty should focus on motives and responsibility rather than hiding basic
rules.

## Routine versus meaningful travel

Travel on a stable, recently traversed route runs in a carriage-following road
view and may later support acceleration. The game turns that journey into an
interactive local interruption only when:

- A current crisis changes the route
- The player deliberately explores
- A passenger creates a dilemma
- A faction intercepts the carriage
- A monster migration or nest affects the road
- A dungeon, ruin, or hidden route becomes relevant
- Cargo, weather, or carriage condition creates a decision

An encounter must offer a choice beyond winning a repeated fight. Combat may
be one answer alongside negotiation, concealment, abandoning cargo, changing
destination, taking a detour, or sacrificing time.

## Procedural route segments

The strategic world is a province-and-route graph. Local segments are generated
from stable inputs such as:

```text
world seed
+ generator version
+ route ID
+ segment index
+ season
+ strategic modifiers
```

The save records meaningful mutations rather than terrain decoration:

- Discovered sites
- Opened shortcuts
- Repaired or destroyed infrastructure
- Changed territorial control
- Defeated or relocated named groups
- Dungeon entrances and seals
- Built camps, depots, and carriage stations

Decorative trees, rocks, and temporary creatures regenerate unless promoted to
authoritative state.

## Off-road travel

The player may leave the road for exploration or avoidance. Off-road travel:

- Consumes more time and supplies
- Reduces access to inns, repairs, and information
- Increases ecological danger
- May avoid tolls, armies, or known bandits
- Can reveal ruins, monster habitats, and smuggling paths

The world may feel enormous without requiring every coordinate to remain loaded
or permanently stored.

## Carriage outcomes

Journey results write back through explicit commands:

- Shipment delivered, delayed, stolen, or destroyed
- Passenger arrived, abandoned, captured, or killed
- Route surveyed, damaged, repaired, or secured
- Bandit agreement made or violated
- Monster nest disturbed, reduced, or relocated
- Dungeon access opened or sealed
- Contraband discovered or successfully transferred

Presentation never mutates strategic arrays directly.
