# 0004 — Separate the deterministic simulation core from raylib

**Status:** Accepted

## Context

Frame-time updates, rendering objects, array indices, and local-map state cannot
safely serve as the foundation for a persistent multi-year world. They introduce
nondeterminism, brittle references, and difficult save compatibility.

## Decision

Build the strategic simulation as a raylib-free C library with typed stable IDs,
an integer calendar, fixed update order, deterministic random streams, commands,
events, versioned saves, and state hashes.

## Consequences

- Rendering frame rate cannot alter history.
- Headless batch testing is possible.
- The isometric client consumes snapshots and submits validated commands.
- Presentation interpolation is never authoritative.
- Existing prototype structures must be adapted rather than expanded directly
  into the strategic model.

## Rejected alternatives

- Extending one central game-state structure was rejected as brittle.
- Using presentation entities as persistent IDs was rejected as unsafe.
- Allowing local gameplay to mutate market arrays directly was rejected because
  it bypasses accounting and causal provenance.
