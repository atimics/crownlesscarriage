# 0006 — Freight retains intent and the world may answer its own crises

**Status:** Accepted

## Context

The first simulation moved goods only between adjacent settlements and allowed
markets to forget why cargo had arrived. Transit hubs protected their own
reserves while distant settlements collapsed. Governments observed these
failures but did not spend resources to change them, leaving the player as the
only meaningful actor.

## Decision

Every strategic shipment retains a final destination while advancing through
explicit route legs. It may unload a bounded share at a distressed transfer
hub, reroute when geography changes, and face loss on each leg. Governments and
factions spend treasury or power on relief, roads, political support, and
monster control. Their actions may fulfill or invalidate an opportunity before
the player reaches it.

## Consequences

- Market hubs can distribute rather than permanently absorb regional freight.
- A single convoy produces a causal chain across departure, transfer, loss or
  arrival, shortage, and political response.
- Waiting is a decision: contracts expire and other powers intervene.
- A region can recover without the player, but the path, cost, beneficiaries,
  and political result differ from player intervention.
- Multi-seed tests must reject universal collapse and runaway ecological
  pressure without requiring every generated history to reach the same state.

## Rejected alternatives

- Teleporting goods between nonadjacent settlements was rejected because it
  erased roads, travel time, and ambush risk.
- Requiring transit stock to exceed the hub's reserve was rejected because it
  caused permanent hoarding and destroyed declared shipment intent.
- Freezing every crisis until the player arrived was rejected because it made
  the world theatrical rather than living.
