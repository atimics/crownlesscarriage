# 0005 — Build the proof in C17, raylib, and SQLite

**Status:** Accepted

## Context

The game's uncertainty is in the causal world design, not in its renderer.
The first implementation must iterate quickly while surviving years of headless
simulation, exact save/load tests, and later projection into local isometric
maps. Networking is possible but unproven as a product requirement.

## Decision

Use portable C17 for the deterministic core and presentation client, raylib 6.0
for platform, input, audio, and graphics, SQLite for versioned local persistence,
and CMake/CTest for builds and verification. Keep native memory layouts out of
the save format. Defer networking while preserving a validated command and
snapshot boundary suitable for an authoritative server.

## Consequences

- The simulation can run and test without a window or GPU.
- SQLite saves can be inspected and migrated instead of depending on compiler
  struct layout.
- Raylib can be replaced or supplemented without rewriting strategic history.
- Gameplay systems require explicit ownership and data-oriented design.
- A future multiplayer layer must serialize commands and snapshots explicitly;
  it cannot expose simulation memory or share a SQLite file.

## Rejected alternatives

- A larger general-purpose engine was deferred because it does not reduce the
  core simulation risk in the first proof.
- Raw binary dumps of C structs were rejected as brittle and unsafe to migrate.
- Building networking before proving the single-player causal loop was rejected
  as premature operational complexity.
