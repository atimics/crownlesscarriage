# 0003 — Use a route graph with persistent procedural segments

**Status:** Accepted

## Context

A seamless continent would be expensive to generate, navigate, populate, store,
and keep interesting. A sequence of disposable random encounters would make
travel feel like repetitive commuting.

## Decision

The authoritative world is a province-and-route graph. Character-scale
wilderness is generated as deterministic route segments using stable route
identity and current strategic state. Saves retain meaningful mutations rather
than every decorative object.

## Consequences

- The world can feel geographically large without remaining loaded.
- Routine stable travel can be accelerated.
- Current crises determine when a segment becomes playable.
- Repaired bridges, discovered sites, changed control, and dungeon access remain
  persistent.

## Rejected alternatives

- A permanently loaded seamless world was rejected for scope and persistence.
- Pure event cards were rejected because they lose embodied travel and combat.
- Unrelated random encounter rooms were rejected because they weaken causality.
