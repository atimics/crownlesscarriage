# Crownless Carriage

**Crownless Carriage** is a living-world isometric RPG about running the last
politically independent carriage line between hostile kingdoms.

You decide who and what may travel: people, goods, secrets, and contraband.
Those choices affect shortages, factions, migration, road safety, and the
people you meet. When a journey becomes dangerous, you deal with it in person.

The project is in pre-production. This repository contains the living design
manual and an executable architecture proof built in C. The proof includes a
deterministic world simulation, SQLite saves, a headless runner, and a raylib
3D client.

![Crownless Carriage action reel](assets/previews/hero/actions/hero_runtime_action_reel_v03.gif)

## Quick start

The current build supports macOS and Linux. You need:

- CMake 3.24 or newer
- A C17 compiler
- Git
- SQLite 3 development headers

CMake downloads the pinned raylib 6.0 source the first time it configures the
project.

Build the normal playtest version:

```sh
cmake --preset play
cmake --build --preset play
```

Run it on macOS:

```sh
open out/build/play/crownless_carriage.app
```

Run it on Linux:

```sh
./out/build/play/crownless_carriage
```

The `play` preset is the main visual-review build. Use the `development` preset
only when you need extra debug checks; it is not a good measure of frame rate.

## Controls

Controls change with the current scene.

| Input | Action |
| --- | --- |
| Left click | Move, or select a raider during combat |
| `F` | Use a nearby door, notice board, carriage, or toll collector |
| `M` | Open or close the carriage map case |
| `Q` | Open the situation board |
| `Tab` | Open the causal event ledger |
| `J` | Jump |
| `Space` | Make a basic attack |
| `X` | Enter or leave guard |
| `1`, `2`, `3` | Use combat skills, choose an encounter response, or trade |
| `G` | Sound the village alarm and start a raid encounter |
| `E` | Start an expedition beside a dungeon entrance |
| `.` | Advance one day while parked |
| `K` | Advance one week while parked |
| `F5` or `Cmd/Ctrl+S` | Start or checkpoint the SQLite action journal |
| `F9` | Restore the latest checkpoint and replay later actions |
| `N` | Start the same crisis with a new deterministic seed |
| `F3` | Toggle the performance and character diagnostic overlay |

In the market, use `1`, `2`, or `3` to buy food, materials, or tools. Hold
`Shift` with the same key to sell.

In the map case, use the arrow keys or click a sheet to select a route. Press
`B` to buy a local chart, `S` to sell one, `Enter` to travel from the correct
endpoint, or `R` to repair a selected contested route. The case holds three
maps.

During combat, `1` uses Crushing Blow, `2` uses Sunder, and `3` uses Second
Wind. Click the ground to stop targeting an enemy and move normally.

## What the prototype proves

The current executable connects these systems in one playable loop:

- A deterministic regional simulation of settlements, factions, trade,
  shortages, bandits, monsters, and a changing dungeon
- A continuous 96 by 72 metre local world with terrain, roads, buildings,
  markets, inhabitants, a castle, and a training ground
- Physical movement with collision, jumping, climbing, swimming, falling,
  ragdolls, and recovery
- Shared combat rules for the player, guards, scouts, and raiders
- Situation-driven charters with sponsors, deadlines, rewards, and reputation
  effects
- Real-time carriage travel with route encounters that can be fought or
  negotiated
- Limited, tradeable maps instead of an all-knowing world map
- An append-only SQLite action journal with checkpoints and deterministic
  replay

This is an architecture proof, not the finished RPG. The current world is one
regional place. Deeper interiors, dungeon maps, balance, and production polish
are still in progress.

## Simulation and tests

After building the `play` preset, run the simulation without graphics:

```sh
./out/build/play/crownless_sim_runner --seed 42 --years 10 --detail
```

Run the test suite:

```sh
ctest --preset play
```

For optimized performance checks:

```sh
cmake --preset release
cmake --build --preset release
./out/build/release/crownless_benchmark
ctest --preset release
```

The benchmark reports CPU time and a checksum, so performance changes cannot
silently remove simulation work.

## Visual checks

The client has deterministic capture modes for art and movement review. For
example:

```sh
./out/build/play/crownless_carriage --capture-golden /tmp/crownless-street.png
./out/build/play/crownless_carriage --capture-face three-quarter /tmp/crownless-face.png
./out/build/play/crownless_carriage --capture-action-reel /tmp/crownless-action
./out/build/play/crownless_carriage --benchmark-render 600 90
```

On macOS, the executable is inside the app bundle:

```sh
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage
```

Run `make art-check` from a desktop session for the full painterly review. It
captures the main locations and checks palette use, subject coverage, local
contrast, edge density, scale, and frame-to-frame flicker. Results are written
to `out/art-check/`.

To rebuild and validate the procedural NPC asset library, use:

```sh
make blender-npc-assets
make blender-npc-assets-check
```

These asset commands require Blender and Python 3.

## Design manual

The manual marks important statements as one of four types:

- **Contract:** a rule the game must obey
- **Target:** an intended design that may change after playtesting
- **Hypothesis:** an idea the vertical slice must prove
- **Deferred:** work deliberately left out of the first serious prototype

Start here:

- [Manual contents](docs/README.md)
- [Creative constitution](docs/01-creative-constitution.md)
- [World and kingdoms](docs/02-world-and-kingdoms.md)
- [Causal simulation](docs/03-causal-simulation.md)
- [Carriage and travel](docs/04-carriage-and-travel.md)
- [Technical architecture](docs/07-technical-architecture.md)
- [Vertical slice](docs/08-vertical-slice.md)
- [Validation strategy](docs/09-validation.md)
- [Production roadmap](docs/10-roadmap.md)
- [Decision log](docs/decisions/README.md)
- [Blender asset system](docs/production/blender-asset-system.md)
- [Hero component system](docs/production/blender-hero-component-system.md)
- [Procedural NPC pipeline](docs/production/procedural-npc-pipeline.md)

## North star

> Build a causal political RPG in which every important journey originates in
> the world simulation, becomes a character-scale adventure, and leaves visible
> immediate and delayed consequences.

Political economy, monster ecology, and persistent dungeons all meet on the
road through the player's carriage company.
