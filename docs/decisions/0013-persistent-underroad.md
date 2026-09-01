# 0013 — Generate the Underroad once and let play change it

**Status:** Accepted

## Context

The mine dungeon used to be one command that changed a regional state. It had
no rooms, map, supplies, retreat, or uncertainty. A maze that reshuffled on
every visit would add variety, but it would make player maps and opened routes
meaningless.

## Decision

Each dungeon receives a stable room graph from its world seed and identity.
The first Underroad has 24 locations across four depths, with loops, secret
passages, hazards, faction territory, physical caches, and a lasting freight
shortcut.

The layout is generated once and saved. Rooms, discovered passages, opened
shortcuts, recovered goods, and the current expedition are authoritative
simulation state. Temporary encounters use a separate dungeon random stream.

Dungeon play uses turns. Movement and searching burn light. Six turns consume
Food and advance one day. Noise raises encounter risk. Encounters use reaction
scores and allow parley, evasion, force, or retreat. Fighting is dangerous and
is not assumed to be the correct answer.

The player must reach the Hoard Threshold before creating a public road, a
smuggler road, or a lasting seal. Each choice also needs physical groundwork:
the freight shortcut must be opened for the public road, the Old Smuggler Cut
must be searched for the hidden road, and the Chain Bridge supports must be
studied before resealing. These outcomes remain part of the regional
simulation, but they now follow things the party did in the dungeon.

## Consequences

- A drawn dungeon map remains useful on later expeditions.
- Retreat and resupply are normal parts of play.
- Food, Tools, Weapons, cargo space, light, and time create concrete choices.
- Goblin society and dragon signs appear as dungeon conditions rather than a
  chain of required fights.
- Save files and the action journal can resume an expedition in its exact room.
- New monster populations can occupy tagged rooms without replacing the map.

## Rejected alternatives

- Reshuffling the maze after every visit
- A linear sequence of story encounters
- Choosing a dungeon outcome before reaching its far side
- Building a large tactical monster roster before the expedition loop works
