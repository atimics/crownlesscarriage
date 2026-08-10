# Technical Architecture

## Architecture principle

The strategic simulation is a pure C library with no dependency on raylib,
rendering frame time, input state, or active local maps. The existing isometric
prototype can become a presentation and character-play client after the core
contracts are established.

```text
Player input
    |
    v
Presentation / local world
    |
    | validated commands
    v
Deterministic simulation core
    |
    | snapshots + causal events
    v
Projection systems and UI
```

## Stable identity

Array positions are not persistent identities. Use typed stable IDs for:

- Province
- Settlement
- Kingdom
- Faction
- Character
- Route
- Shipment
- Bandit group
- Monster population
- Dungeon
- Situation
- Causal event

Dense arrays may store entities internally, but references use IDs resolved
through validated lookup tables. Deletion uses tombstones or generations so a
stale ID cannot silently refer to a different entity.

## Simulation core responsibilities

- World records and stable identity
- Authoritative integer calendar
- Fixed daily and weekly pipeline
- Production and accounting
- Shipments and route resolution
- Government and faction decisions
- Bandit and monster strategic state
- Dungeon regional states
- Causal event emission
- Situation detection
- Command validation
- Save/load and state hashing

## Presentation responsibilities

- Kingdom-map visualization
- Active city or route segment
- Local navigation, animation, and combat
- Instantiating strategic entities near the player
- Projecting strategic conditions into visuals and behaviour
- Collecting local outcomes into commands
- Communicating forecasts and causal explanations

Presentation may interpolate carriage and shipment icons, but interpolation is
never authoritative.

## Command boundary

Local play submits explicit commands such as:

```text
BEGIN_TRAVEL
SHIPMENT_DELIVERED
SHIPMENT_DESTROYED
PASSENGER_ARRIVED
ROUTE_REPAIRED
BANDIT_LEADER_CAPTURED
BANDIT_AGREEMENT_CREATED
MONSTER_NEST_REDUCED
MONSTER_POPULATION_RELOCATED
DUNGEON_STATE_CHANGED
RELIEF_ALLOCATED
```

The core validates entity identity, preconditions, date, quantities, and
authority before applying a command and emitting follow-up events.

## Deterministic randomness

Each subsystem owns an independent seeded random stream. Adding a cosmetic draw
or changing monster iteration order must not alter market history.

Streams should be derived from stable identifiers and explicit counters rather
than ambient global random state. Debug output records the stream and draw that
caused a significant event.

## Numeric representation

Strategic quantities use integers or documented fixed-point units. Floating
point may be used for presentation, but not where platform or iteration
differences could change long-term authoritative outcomes.

Every quantity declares:

- Unit
- Valid range
- Source and sink operations
- Overflow behaviour
- Serialization representation

## Save structure

A save contains:

- Format version
- Generator version
- World seed
- Immutable-generation identifiers
- Current calendar date
- Complete mutable strategic snapshot
- Stable ID tables and generations
- Active situations and contracts
- Causal event history required by play
- Route and dungeon mutations
- Discovered information
- Player company, carriage, crew, cargo, and reputation
- Deterministic stream states or counters

Procedural decoration is regenerated. Meaningful mutation is stored.

## Generator versioning

Changing generation code can move cities, remove camps, or invalidate dungeon
references. Every save records a generator version. Supported approaches are:

- Preserve the old generator for existing saves
- Migrate the immutable generated base explicitly
- Snapshot generated authoritative records in the save

The project must choose one before public save compatibility is promised.

## Causal event retention

Not every event remains forever. Events are classified as:

- Permanent historical milestone
- Active causal dependency
- Player-known history
- Debug-only trace
- Expirable operational event

Compaction may remove debug-only events after their children have materialized,
but cannot break explanations, active situations, or character memory.

## Headless tools

Required developer tools include:

- Multi-year simulation runner
- Seed batch runner
- State inspector by entity ID
- Route and shipment inspector
- Causal-chain viewer
- Pause-on-condition breakpoints
- Command/event replay
- State-hash comparison
- Save/load equivalence runner
- CSV or JSON metric export

The simulation inspector is core infrastructure, not optional polish.

## Test layers

1. Unit tests for accounting, IDs, routes, and event creation.
2. Property and invariant tests over randomized commands.
3. Deterministic replay tests.
4. Save/load equivalence tests at arbitrary dates.
5. Multi-seed long-run stability tests.
6. Projection tests proving strategic states select correct local changes.
7. Vertical-slice scenario tests for each intervention.
