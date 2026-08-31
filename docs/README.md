# Living World Design and Production Manual

This manual defines the creative, systemic, technical, and production
contracts for **Crownless Carriage**. It is written to prevent invisible
complexity: every simulation system must create, constrain, explain, or visibly
respond to a player decision.

## Recommended reading order

1. [Creative constitution](01-creative-constitution.md)
2. [World and kingdoms](02-world-and-kingdoms.md)
3. [Causal simulation](03-causal-simulation.md)
4. [Carriage and travel](04-carriage-and-travel.md)
5. [Bandits, monsters, and dungeons](05-threats-and-dungeons.md)
6. [Cities, characters, and situations](06-cities-characters-situations.md)
7. [Technical architecture](07-technical-architecture.md)
8. [Vertical slice](08-vertical-slice.md)
9. [Validation](09-validation.md)
10. [Production roadmap](10-roadmap.md)
11. [Text-first metagame playtest](11-metagame-playtest.md)
12. [Story, quests, and narration](12-story-quests-and-narration.md)
13. [The OSR release cut](13-osr-release-cut.md)
14. [Glossary](glossary.md)
15. [Decision log](decisions/README.md)

## Full table of contents

### Part I — Creative constitution

- The Crownless fantasy
- Design pillars
- Definition of a living world
- Causal and emotional legibility
- Scope boundaries

### Part II — World model

- Time and simulation scale
- Geography and provinces
- Why settlements exist
- Kingdom formation
- Material political power

### Part III — Causal simulation

- Population cohorts and labour
- Production, consumption, and reserves
- Markets and delayed shipments
- Government and factions
- Causal event ledger
- Situation selection

### Part IV — The carriage and the road

- Company charter and player identity
- Passenger and cargo capacity
- Carriage progression
- Physical cartography and route commitments
- Explicit travel commitments
- Persistent procedural route segments

### Part V — Threats and adventure

- Human bandit factions
- Goblin tribute, war and social pressure, dragon hoards, theft, omens, and repayment
- Smuggling networks and black markets
- Monster habitats and migration
- Monster pressure on human systems
- Persistent dungeons
- Dungeon transformation and reoccupation

### Part VI — Local life and narrative

- City grammars
- Projection of strategic conditions
- Persistent named characters
- Causal situations
- Immediate consequences and delayed echoes
- Exploration, investigation, and combat

### Part VII — Architecture

- Raylib-free simulation core
- Stable entity identity
- Fixed calendar pipeline
- Commands, events, and projections
- Append-only action journal, checkpoints, and versioned replay
- Versioned schemas and generator versions
- Determinism, replay, and debugging

### Part VIII — Production proof

- Failed-harvest vertical slice
- [Introduction video](production/introduction-video.md)
- [Character physics foundation](production/character-physics-foundation.md)
- [Procedural creature pipeline](production/procedural-creature-pipeline.md)
- [Runtime environment design](production/runtime-environment-design.md)
- [Robotics algorithms for character movement](production/robotics-algorithms.md)
- Automated simulation validation
- Player-legibility tests
- [Text-first metagame playtest](11-metagame-playtest.md)
- Go/no-go gates
- Expansion roadmap

## Document maintenance rules

1. New systems must identify the player decisions they create or constrain.
2. New simulation variables must identify at least two visible projections.
3. Changes to a contract require a decision-log entry.
4. Features added to the roadmap require a measurable exit condition.
5. Deferred systems remain out of scope until the vertical slice passes its
   go/no-go gates.
