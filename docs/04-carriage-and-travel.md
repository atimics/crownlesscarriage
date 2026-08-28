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

## V1 working animals

The first playable animal economy contains only horses and cows. They share
the quadruped movement system, but they have different jobs in the world.

The Crownless company owns two named horses. Their age, health, fatigue, and
hunger persist. Every journey reserves real fodder from the departure market.
Cargo weight and a poor road tire the team, while food, time, and a settlement
stable restore it. A tired team travels more slowly; an exhausted or hungry
team cannot depart.

Farm settlements keep cattle as adult cows and calves. A herd consumes stored
Food as fodder and adds a small weekly food yield. Good conditions let calves
join the adult herd and allow new calves to be born. Fodder failure weakens the
herd and can force a cow to be slaughtered, exchanging future production for
four immediate Food. Dragon hunts remove actual cows before taking anonymous
stored food.

Named horses are individual simulation records because they stay with the
player. Cows remain settlement herds until a later situation needs to promote
one animal. V1 does not include riding, mounted combat, breeds, genetics, milk
as a separate good, or any other domestic animal species.

## Stable breeding

Settlement stables now support a small Crownless breeding program. Bracken is
a stallion and Morrow is a mare. Breeding requires both horses to be present,
mature, trained, healthy, fed, and rested. The stable charges 20 crowns and
uses 2 Food. A pregnancy lasts 330 days, and a mare in her last 30 days cannot
leave on a carriage journey.

A foal receives a persistent identity, sex, sire, dam, and three inherited
working traits: strength, temperament, and hardiness. Small deterministic
variation keeps siblings from being copies. Young horses are boarded where
they are born. Stable care consumes weekly Food, restores health, lowers
fatigue, and trains them. A horse must be at least three years old, have 60
training, and be healthy before it can pull the carriage.

The company can board up to six horses in addition to its two-horse team. At a
stable, `stable breed MARE STALLION` starts a pregnancy and
`stable team SLOT HORSE` swaps a ready boarded horse into team slot 1 or 2.
Numbers come from the `animals` view. The outgoing team horse remains owned by
the company and is boarded at that settlement.

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
by selecting a line on a chart. At the edge of a place, the carriage follows a
cart track. Each choice scene contains that continuous track and no more than
one side branch. The player can take the branch or keep moving until the next
one. The scene includes the coach, road surfaces, signs, vegetation, damage,
and closures in the world.

The strategic graph may connect a place to many routes. `Drive out` commits
only to leaving the yard: the carriage moves through the town and gate before
the first route choice is shown. The local presentation orders outgoing routes
into a short sequence of roadside encounters. It never turns every graph edge
into a hub-shaped crossroads or lets the player leaf through branches while
the carriage stands in town. Continuing puts the carriage back in motion;
taking the visible branch commits to that route.

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
the road. An uncharted branch can still be taken, but the player may not know
its destination and the journey is slower and more dangerous. A local guide
can provide the same immediate wayfinding help without transferring a map.

Every cartographer chooses different names, omissions, and warnings. The notes
are arguments about the road, not live telemetry. Closures, troop movements,
or new dangers may therefore contradict the ink.

The player selects a physical road, manifest, companions, and legal posture
before committing. Maps remain supporting evidence. Wider atlases or copying
equipment may later be carriage modules, but cannot restore a universal
strategic screen.

## Travel commitment

Leaving the yard begins a local road-seeking sequence, not a planned journey.
No destination, fare, provisions, or danger roll is committed until the player
turns onto a branch encountered in the world. That choice creates a persistent
journey whose exact duration is measured by the authoritative world clock. The
company remains at the origin for command validation while the carriage
records its route, endpoints, progress, speed, condition, and reserved fare.
Every 60 Hz travel tick moves that state forward; calendar boundaries run the
same daily world update used by headless play. Arrival changes the company
location only after the full duration has elapsed.

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

## Continuous convoy presentation

The route graph remains the source of truth, but the player should see one
carriage move through the whole journey. A normal trip follows this visible
sequence:

1. The carriage waits in the town yard while its two horses rest beside the
   hitch rail.
2. Choosing a road at the carriage yard hitches the team. Walking to a town
   boundary does not summon or move the carriage.
3. The player changes playback pace with `W`, slows with `S`, and pauses with
   `Space`. `Enter` advances to the next real decision. The road view does not
   claim to offer steering when route movement is automatic.
4. The town road, gate approach, and route segment play as one movement. The
   route clock does not advance until the carriage clears the gate.
5. A road interruption keeps the carriage at its saved route progress instead
   of rebuilding it at a generic encounter point.
6. Arrival continues through the destination gate and ends with the horses
   resting in that town's yard.

The local presentation may be rebuilt after loading, but the simulation's
route, endpoints, clock, progress, condition, and stopped or moving state stay
authoritative. This keeps old saves compatible and prevents local scenes from
inventing a second journey state.

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
