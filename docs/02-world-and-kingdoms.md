# World and Kingdoms

## World scale

The first serious prototype contains:

- 8–12 provinces
- 3 kingdoms
- 5–7 settlements
- One public carriage network
- One politically contested bottleneck
- One smuggling route
- Several persistent wilderness segments
- One dungeon capable of changing regional development

This is large enough to produce interdependence and small enough for players to
recognize places and recurring people.

## Current realm rules

The playable slice treats the three kingdoms as the three top-level powers.
Every pair is at war. A road between different kingdoms is therefore a war
border: ordinary freight will not cross it, but the neutral Crownless Carriage
can. This makes the carriage useful even before a contract is attached to a
journey.

Each kingdom starts with two settlements that cover different parts of its
economy. A kingdom must be viable inside its own borders, but no single town is
self-sufficient.

## Tiny town builder

Settlements have a size and a strict service budget:

| Size | Service slots |
| --- | ---: |
| Hamlet | 2 |
| Village | 4 |
| Town | 6 |
| City | 9 |
| Capital | 12 |

Services are concrete parts of the economy and local presentation: farms,
granaries, markets, inns, smithies, healers, stables, shrines, barracks,
cartographers, guildhalls, mines, black markets, and dungeon wards. The starting
set comes from a settlement's size and reason for existing, so towns do not
receive one shared checklist.

A settlement can build one service at a time. Construction occupies a future
slot, spends local material and tools plus kingdom treasury, and completes over
world days. Open services then affect production, hunger, prosperity, or
security.

## Generation order

The world is constructed from material constraints rather than placing towns
first and inventing explanations afterward.

1. Generate terrain, climate, water, and elevation.
2. Assign fertility, timber, ore, and unusual ecological or magical features.
3. Identify rivers, passes, crossings, and navigable route candidates.
4. Score viable settlement sites by food, transport, defence, and resources.
5. Place settlements with distinct primary functions.
6. Create dependencies that no settlement can satisfy alone.
7. Form kingdoms around administrative reach and route control.
8. Assign institutions and factions to sources of material power.
9. Place ruins and dungeons whose origins fit the geography and generated
   political history.
10. Guarantee starting tensions and recovery paths.

The generator does not need to forward-simulate centuries. It constructs a
coherent present and synthesizes a concise history from established facts.

## Province model

Each province records:

- Terrain and climate
- Seasonal fertility
- Extractive resources
- Population capacity
- Settlement and controlling kingdom
- Adjacent provinces and route edges
- Route capacity, condition, toll, and security
- Monster habitat pressure
- Bandit influence
- Known ruins and dungeon effects
- Temporary modifiers with explicit sources and expiration

Province data is strategic. Tile decoration is generated locally and is never
the authoritative source for control, resources, or danger.

## Why settlements exist

Every settlement receives:

- A primary economic function
- At least one geographic advantage
- At least one important imported dependency
- A controlling institution or faction
- A recognizable physical grammar
- A current contradiction

### Settlement examples

**Agricultural basin**

- Exports food
- Depends on tools and seasonal labour
- Organized around fields, granaries, mills, and river access
- Vulnerable to weather, requisition, pests, and worker flight

**Mining town**

- Exports raw material
- Depends on imported food and tools
- Organized around yards, lifts, workshops, and worker housing
- Vulnerable to tunnel disasters, monster incursions, and guild conflict

**Crossroads market**

- Produces little directly
- Depends on route safety and throughput
- Organized around warehouses, coaching inns, toll offices, and markets
- Vulnerable to war, border closures, bandit control, and alternate routes

**Fortress settlement**

- Exists to control a pass, bridge, or frontier
- Depends on government spending and imported provisions
- Organized around walls, barracks, checkpoints, and military suppliers
- Vulnerable to unpaid soldiers, requisition, smuggling, and desertion

**Dungeon-adjacent town**

- Extracts relics, rare materials, knowledge, or pilgrim revenue
- Depends on containment and expedition traffic
- Organized around outfitting, quarantine, shrines, and salvage markets
- Vulnerable to ecological disruption, curses, collapse, and faction capture

## Kingdom formation

Kingdoms are formed around the ability to administer routes and dependencies,
not merely around Voronoi territory.

Each kingdom begins with:

- A capital and one or more dependent settlements
- A governing form
- A source of state revenue
- Three factions with material power
- One vital external dependency
- One geographic bottleneck or contested interest
- One founding political contradiction
- Relationships, commitments, and active disputes with neighbours

## Factions and material power

A faction is not just an approval score. It must control or influence something
concrete:

- Land and food reserves
- Workshops and skilled labour
- Religious legitimacy and sanctuary
- Armed force and checkpoints
- Trade credit, warehouses, or carriages
- Smuggling routes and criminal enforcement
- Monster-hunting expertise
- Dungeon access or relic knowledge

Factions possess goals, policy preferences, imperfect information, memory of
bargains, and a limited set of instruments. Initial politics should use a small
number of meaningful axes rather than separate simulations for culture,
religion, class, and ideology.

## Generated identity

Kingdom identity should emerge through consequences:

- Which goods and routes it depends upon
- Who controls its government
- Which laws it enforces
- How it treats outsiders and monsters
- Where it spends revenue
- Which places flourish or suffer
- Which contradiction the player repeatedly encounters

Names, colours, crests, and generated history support identity but cannot
substitute for different behaviour and physical presentation.

## World-generation invariants

Every generated starting world must contain:

- One important dependency per kingdom
- One geographic bottleneck
- One faction conflict per kingdom
- One contested route or resource
- One vulnerable settlement
- At least two recovery mechanisms for each essential dependency
- No settlement that is economically self-sufficient
- No kingdom that begins in unavoidable collapse
- At least one bandit, monster, and dungeon pressure connected to human systems
