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
One pair begins at war; the other courts begin at peace. Relations can later be
peace, war, or an alliance against the dragon. A declaration does not change a
matrix by magic. A named courier must carry its sealed message between royal
seats over the actual road graph. The rider can be delayed, lost, or arrive
with damaged wording. A corrupt mayor can suppress the message or report its
opposite. The Crownless Carriage may accept the dispatch and carry it as a
charter, improving its chance of arriving intact.

On an active war border, ordinary freight crosses at sharply reduced capacity
and pays a four-crown sanction. A hidden smuggler road substitutes a two-crown
bribe, and the neutral Crownless Carriage can cross openly if it can pay the
same real dues and accept the risk. This makes both lawful and hidden routes
materially important.

War burden is local rather than a single realm-wide number. It rises where a
settlement touches contested roads, especially when those roads are unsafe or
restricted. Fortresses and capitals carry extra burden. That pressure also raises
the garrison's need for pay, wheat, tools, and weapons. If the material chain fails,
shortages and unpaid soldiers make the cost of war fall unevenly and inequality
rises through those real conditions.

War has no weekly money sink. The crown moves existing gold from its treasury
into a named settlement's war chest. Wages move that gold into the local market.
The war chest pays a named supplier before food, tools, or weapons leave its stock. The
goods then occupy a real shipment, travel on real roads, and may be lost.

A border war does not make the dragon attack. A fortress or capital may form a
Crown Levy only when burden is high, pay or supplies are failing, the liquid war
funds are low, and the kingdom has weak legitimacy. The levy must travel to the
cave, steal real hoard treasure, and carry it back into the local war chest.
Only that theft starts the omens. This keeps the cause readable: material war
failure creates the motive, people commit the theft, and unpaid theft causes the
fire.

Each kingdom starts with two settlements that cover different parts of its
economy. A kingdom must be viable inside its own borders, but no single town is
self-sufficient.

## Physical toy economy

The ordinary goods are Food, Iron, Tools, Weapons, Raw Gold, and Gems. They are
inventories, not bonuses.

- A working farm on real fields creates Food. Without tools it works slowly.
- A working mine removes Iron from a mountain deposit. Without tools it can
  still recover a little Iron, preventing a permanent production lock.
- A gold seam yields one Raw Gold after twelve equipped mine batches.
- A gem seam yields one Gem after forty-eight equipped mine batches.
- A smith spends two Iron on one Tools bundle or three Iron on one Weapons
  bundle. Tools wear at farms, mines, smithies, and sieges.
- A market or capital smith can commit Raw Gold and a Gem to three weeks of
  work. The result is one named treasure with a maker, owner, location,
  material content, appraisal, and history.

A cargo slot holds eight Food, four Iron, two Tools, two Weapons, one Gold
strongbox, one Gem case, or one named treasure. Treasure is valuable because it
compresses rare materials and craft work into one guarded slot. It is not
spendable coin.

Prosperity, inequality, border burden, and supply crisis are derived readings.
They can guide decisions, but they never create, remove, or teleport goods.

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
slot, spends local Iron and tools plus kingdom treasury, and completes over
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

- Exports Iron and may expose rare Gold or Gem seams
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
- War changes local inequality only through moved coin, missing goods, prices,
  hunger, and political power; it never directly causes dragon fire
