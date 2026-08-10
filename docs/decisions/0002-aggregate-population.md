# 0002 — Use aggregate simulation with named-character promotion

**Status:** Accepted

## Context

Simulating every household, wage, rent, birth, shop, and job would create large
amounts of hidden state with little visible benefit. It would also multiply
persistence, debugging, and presentation costs.

## Decision

Population, labour, and most businesses are simulated as cohorts and production
sectors. Persistent individuals are promoted when they anchor a situation,
institution, place, passenger relationship, or remembered consequence.

## Consequences

- Strategic population remains tractable and testable.
- Important people can still retain identity and memory.
- Shops project sector state through a proprietor rather than simulating every
  firm's finances.
- New individual simulation requires a demonstrated gameplay need.

## Rejected alternatives

- Universal individual simulation was rejected as invisible complexity.
- Completely anonymous cohorts were rejected because they cannot provide
  emotional continuity.
