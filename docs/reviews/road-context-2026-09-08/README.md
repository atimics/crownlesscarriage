# Road context evidence

This change addresses road measurement semantics in #266, based on PR #573 at 2b74064. Schema 73, generator 25, and JSON protocol 6 remain current.

Each route gains two JSON objects:

- `context` is a snapshot. Endpoint records give settlement identity, kingdom identity, population, and habitation. A missing endpoint is `null`. Border and smuggler flags use the current route state. Each kingdom has its own `official_route_eligible` result from `CcSimRoyalCarriageCanUseRoute`.
- `observation` describes the existing open-day and closed-day counters. They cover the run interval after `start_day_exclusive` through `end_day_inclusive`. The runner samples closure after each daily advance. Loading a save starts a fresh interval with zero observed days.

Official carriage eligibility describes the engine's official-route rule. Physical closure is the existing route `closed` field. Cargo, location, and dispatch readiness have their own rules. The exact historical closure cause remains unavailable (`closure_reason: null`). The endpoint kingdom identifies settlement ownership.

## Validation

- Strict release headless build and all 107 CTest tests passed. After strengthening the kingdom-specific fixture, both affected JSON tests passed again.
- Static analysis passed with one reviewed baseline item.
- Ten controlled fixtures cover inhabited endpoints, one ruin, two ruins, physical closure, war, smuggling, zero condition, a domestic road, distinct kingdom permissions, and a missing endpoint. Every emission verifies the entire simulation state with a byte comparison.
- Save/load tests compare road context and check that a loaded run starts an empty observation interval.
- Seeds 42 and 0x5eed0001 ran for 40 years against the base and draft. All existing JSON fields matched at all 82 checkpoints. Only the new `context` and `observation` fields were removed for comparison. See `parity.json` and `fixtures.json`.

These engine diagnostics support world-health analysis. Player-facing knowledge, company-specific passage, outage streaks, repair attempt counters, and broader health and reachability targets remain separate acceptance work in #266.
