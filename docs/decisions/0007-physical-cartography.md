# 0007 — Replace the kingdom screen with physical route maps

**Status:** Accepted

## Context

The first client exposed every settlement, route, closure, shipment, and threat
through one secondary kingdom screen. Although the screen was gated by the
carriage, its information remained free, universal, and detached from the
economy. It made a procedural world feel smaller by presenting the entire
region as solved topology.

## Decision

Remove the omniscient kingdom view. Maps are stable simulation objects kept in
a capacity-limited carriage case. Each map depicts exactly one route, has a
maker, owner, survey date, accuracy, recorded claims, legal status, and price,
and can be bought or sold. Route travel requires the corresponding carried
map. Each sheet renders its own distorted projection rather than sharing a
canonical strategic camera.

## Consequences

- Geographic knowledge competes for carriage capacity and money.
- Cartographers, smugglers, archives, theft, copying, and forgery can become
  material professions and quests.
- A chart can be useful, obsolete, politically suppressed, or deliberately
  misleading without changing the authoritative route.
- The player learns the world as an inventory of partial perspectives rather
  than consuming a complete topology.
- Persistence and determinism tests must include maps and ownership.

## Rejected alternatives

- Fog of war on the old kingdom map was rejected because the underlying
  interface remained a universal truth surface waiting to be uncovered.
- A weightless unlimited journal of known routes was rejected because
  knowledge would not participate in carriage capacity or trade.
- Live danger and closure overlays were rejected because they turn old ink into
  telemetry and erase the value of fresh surveys and local information.
