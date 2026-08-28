# Production Roadmap

## Stage 0 — Manual and contracts

### Work

- Establish the creative constitution.
- Specify the world, simulation, journey, threat, dungeon, and persistence
  contracts.
- Define the vertical slice and validation gates.
- Record major design decisions.

### Exit condition

The complete Empty Granary crisis can be traced from strategic cause through
journey, local intervention, immediate projection, and delayed echo without an
undefined core system.

## Stage 1 — Deterministic simulation kernel

### Work

- Create a raylib-free C library.
- Implement stable typed IDs.
- Add authoritative calendar and fixed update pipeline.
- Model three goods, labour, reserves, routes, and delayed shipments.
- Implement commands, causal events, snapshots, hashing, and versioned saves.
- Build headless runner and inspectors.

### Exit condition

Deterministic ten-year simulations pass accounting, identity, replay, and
save/load tests.

## Stage 2 — Material world generation

### Work

- Generate geography and province graph.
- Place viable, interdependent settlements.
- Form three kingdoms around routes and dependencies.
- Create material factions and guaranteed starting tensions.
- Place the bridge, bandit pressure, monster habitat, and mine-dungeon.

### Exit condition

Every generated settlement has a machine-readable reason to exist, a visible
grammar, an external dependency, and a recoverable vulnerability.

## Stage 3 — Physical cartography and commitments

### Work

- Build capacity-limited, tradeable route-map objects and the carriage map case.
- Present every outgoing road as physical geometry at a carriage-stage
  junction; use maps as advice, never as travel permission.
- Put dungeons and major lairs on separate local maps reached by short seeded
  wilderness roads.
- Implement exact travel duration and the deterministic uncharted penalty.
- Add manifests, passengers, cargo, crew, and modules.
- Instantiate shipments and important travellers visually.
- Support routine travel resolution and contextual interruption.

### Exit condition

The player can make a constrained manifest choice, complete a journey, and
produce deterministic strategic results.

## Stage 4 — One living city

### Current buildout

The shared exterior now resolves each settlement through a stable place
profile. Farming, market, fortress, mining, capital, and dungeon-frontier towns
have their own terrain seed, district language, civic labels, material tint,
plaza mark, three named physical landmarks, and three secondary roads that join
those landmarks to the shared regional spine. Each profile also turns the
trade-room skeleton into its own staffed civic hall, with a stable keeper,
service, palette, wall mark, and stock display. Place records are the single
source for grading, movement, camera picking, labels, and rendering, so each
town has a distinct walkable local layout without ghost geometry. The remaining
gate is deeper interior grammars and a stateful recurring cast.

### Work

- Create one authored city grammar.
- Project shortage, security, faction, refugee, and prosperity states.
- Add reusable interiors and changing services.
- Promote a small recurring cast.
- Present causal information through people and places.

### Exit condition

Testers identify the city's purpose and current crisis without opening the
strategic ledger.

## Stage 5 — Threat systems

### Work

- Implement one strategic bandit faction with recruitment and supplies.
- Implement one subterranean monster population with habitat pressure.
- Tie both to routes, settlements, and faction actions.
- Support combat, negotiation, and structural resolutions.

### Exit condition

Bandits and monsters create observably different problems and respond
differently to player intervention.

## Stage 6 — Persistent dungeon

### Work

- Build the mine-dungeon from an authored grammar.
- Add expedition supplies, return paths, and persistent shortcuts.
- Connect occupants and colony state to the province simulation.
- Support sealing, reopening, criminal control, and public-route outcomes.

### Exit condition

At least three dungeon outcomes visibly change the city, route network, faction
power, and future monster pressure.

## Stage 7 — Complete vertical slice

### Work

- Connect the failed harvest, shipment, bridge, bandits, and dungeon.
- Implement official, reparative, smuggling, and refusal paths.
- Add immediate consequences and delayed echoes.
- Tune attention limits, timing, and recovery.
- Run automated and player validation.

### Exit condition

All go/no-go requirements in [Validation](09-validation.md) pass.

## Stage 8 — Controlled expansion

Only after the go decision:

- Add city grammars one at a time.
- Add new goods only when they create distinct route choices.
- Add monster families through ecology templates.
- Add dungeon grammars with unique regional transformations.
- Expand faction instruments and political commitments.
- Add war, culture, religion, or dynasties only when the existing simulation
  cannot express a proven gameplay need.

## Deferred feature register

- Individual household economics
- Full family trees and inheritance
- Detailed cultural and religious simulation
- Seamless permanently loaded wilderness
- Fully dynamic borders and tile armies
- Unique interior for every building
- Unrestricted generated dialogue
- Multiplayer
- Realm founding and direct rulership
