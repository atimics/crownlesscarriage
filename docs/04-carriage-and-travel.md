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

## Kingdom map

The map is a planning surface, not a second management game. It shows:

- Settlements and known locations
- Routes, travel times, tolls, and capacity
- Known security and environmental hazards
- Active closures and political restrictions
- Visible shipments, armies, and important travellers
- Deadlines and estimated arrival
- Information age or uncertainty where relevant

The player selects a destination, route, manifest, companions, and legal
posture before committing.

## Travel commitment

A departure advances an exact number of calendar days. Before confirmation the
game presents:

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

Travel on a stable, recently traversed route can resolve quickly. The game
instantiates a local wilderness segment only when:

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
