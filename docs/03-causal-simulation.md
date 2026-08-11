# Causal Simulation

## Purpose

The simulation exists to produce understandable pressure, competing interests,
and consequences for journeys. It is not intended to reproduce a complete
historical society.

The first model aggregates most people and businesses. Individuals are
promoted into persistent named characters only when they anchor a situation or
institution the player can meaningfully affect.

## Authoritative calendar

The simulation uses integer calendar days. Local rendering frame rate never
changes strategic outcomes.

Suggested daily pipeline:

1. Apply environmental and temporary modifiers.
2. Advance production.
3. Resolve consumption and spoilage.
4. Update reserves and expected needs.
5. Create or update delayed shipments.
6. Move shipments and resolve route losses.
7. Process arrivals, taxes, and payments.
8. Update prices and wages within bounded rates.
9. Resolve population pressure, security, and unrest.
10. Allocate government and faction resources.
11. Detect threshold crossings.
12. Emit causal events and evaluate playable situations.

Slow political and demographic effects may apply only on every seventh day,
but still consume the same authoritative calendar.

## Population and labour

Settlements track cohorts rather than individual households:

- Population
- Available labour
- Employed labour by sector
- Displaced population
- Health or subsistence pressure
- Faction support shares

Strategic births, deaths, and migration are summarized. A visible household or
shop may project these conditions through a persistent proprietor without
requiring individual financial simulation for the whole population.

## Goods and constraints

The vertical slice begins with:

- **Food:** consumed by population and expeditions; vulnerable to spoilage.
- **Raw material:** extracted locally and used by manufacturing.
- **Tools:** improve extraction, agriculture, repair, and construction.

Labour, carriage capacity, route capacity, security, and time are constraints,
not freely traded commodities.

Monster materials and relics exist initially as special cargo connected to
situations rather than broad commodity markets.

## Production and accounting

All quantity changes use explicit sources and sinks:

- Production
- Consumption
- Spoilage
- Shipment departure
- Shipment arrival
- Theft or destruction
- Player purchase or delivery
- Relief or emergency reserve release

No system silently injects or removes inventory. Recovery mechanisms may create
production bonuses, alternate imports, or relief shipments, but must identify
their source.

## Prices and trade

Prices react to reserve targets, recent consumption, expected imports, and
local policy. They change at bounded rates unless a named shock authorizes a
larger movement.

Merchants operate with delayed information and limited transport. This prevents
perfect markets from immediately erasing every shortage while keeping their
behaviour understandable.

Shipments are authoritative records containing:

- Origin, current leg destination, and final destination
- Cargo and quantity
- Route
- Departure and expected arrival
- Owner and contractual obligations
- Security or escort
- Current status and loss events

Carriages are instantiated physically near the player or when a situation uses
them. The simulation does not require every shipment to remain a local-map
entity.

The implemented proof plans freight against final regional need, retains that
intent through multiple transfers, unloads limited cargo at distressed hubs,
and reroutes onward if a road changes. Loss is resolved independently on every
leg and remains the parent of later shortages or political responses.

## Government and faction response

Governments collect explicit taxes and allocate limited resources among:

- Roads and bridges
- Patrols and military force
- Emergency relief
- Administrative capacity
- Political patronage
- War or border enforcement

Factions respond based on interests and observed outcomes. They may lobby,
withhold labour, hoard resources, sponsor smugglers, hire bandits, offer relief,
or challenge policy. Their actions consume resources or relationships rather
than appearing from a probability roll alone.

The proof currently models treasury revenue, emergency grain purchased from
outside the region, proactive road maintenance, changing crown/guild/commons
support, and faction-funded deep hunts. These responses are deliberately
capable of superseding a player charter: the world does not reserve its crises
for the player.

## Causal event ledger

Every significant change records:

- Stable event ID
- Date and location
- Event type and magnitude
- Affected and responsible entities
- Parent events or contributing causes
- Player knowledge state
- Strategic consequences
- Required local projections
- Expiration or invalidation conditions

The ledger supports three audiences:

1. The simulation uses it to form situations and delayed consequences.
2. Presentation uses it to generate truthful dialogue, environmental changes,
   and notifications.
3. Developers use it to debug why a world state exists.

## Situation contract

A world condition becomes a playable situation only if it contains:

- A traceable cause
- An affected group
- At least two actors seeking incompatible outcomes
- At least three materially different interventions
- A deadline or escalation rule
- A bounded effect the player can plausibly produce
- A visible before-and-after state
- An invalidation rule if circumstances change

The situation engine presents only the best one or two situations in a region.
It does not expose every threshold crossing as a quest.

Generation II persists relief charters, road compacts, depth warrants, and
quiet black-market commissions. Each stores a sponsor, causal event, target,
progress, reward, deadline, and status. Trading, repairing, and expeditions
resolve the same records shown on the situation board.

## Simulation invariants

- Goods never become negative.
- Goods are conserved except through declared sources and sinks.
- Route capacity and shipment travel time are respected.
- Treasury changes use explicit sources and sinks.
- Every visible consequence references a causal chain.
- Every collapse state has at least one recovery path.
- Identical seed and commands produce identical state hashes.
- Player impact remains bounded and explainable.
- A region cannot demand attention through more than three active crises.

## Expected failure modes

Automated tests and tuning must actively search for:

- Famine and unemployment death spirals
- One early advantage creating an inevitable empire
- All settlements converging to the same equilibrium
- Merchants selecting a single dominant route
- Infinite player arbitrage caused by stale prices
- Crises resolving before the player can reach them
- Too many simultaneous situations
- Governments spending resources they do not possess
- Explanations attributing a condition to a minor or unrelated cause
