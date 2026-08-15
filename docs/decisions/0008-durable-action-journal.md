# 0008 — Make committed simulation inputs the durable source of truth

**Status:** Accepted

## Context

Atomic snapshots can reproduce the instant at which they were written, but
they discard every authoritative change made afterward. Real-time carriage
travel makes that gap visible: the world may advance for minutes between
manual saves, and rewriting every normalized table at 60 Hz is neither
necessary nor a sound durability model.

The bounded causal event ledger explains the world to players and developers,
but it is an output of simulation. It cannot reliably reconstruct the inputs
that produced state.

## Decision

Persist validated commands, day advances, and runtime-tick advances in a
versioned append-only SQLite journal. Each journal epoch begins from a
hash-identified checkpoint. Each operation has a monotonic ordinal and stores
the exact pre-state and post-state hashes. Recovery validates the checkpoint,
replays the contiguous suffix deterministically, and refuses any continuity or
hash mismatch.

Use SQLite WAL mode with full synchronous commits. Batch real-time travel into
six-tick records, while flushing immediately at commands, journey phase
transitions, checkpoints, and clean shutdown. Treat normalized snapshots as
replaceable replay accelerators, not as the post-checkpoint source of truth.

## Consequences

- Committed play after the last checkpoint survives process restart.
- A save can be inspected as both normalized state and an ordered input trace.
- Simulation and generator versions become part of every replay contract.
- Runtime persistence costs at most one full commit per 100 ms during travel.
- Failed runtime commits roll speculative simulation back to the durable
  prefix.
- Schema evolution must either preserve deterministic replay or explicitly
  begin a migrated journal epoch.

## Rejected alternatives

- Rewriting the entire snapshot every frame was rejected because it amplifies
  writes and couples frame cadence to persistence cost.
- Treating the causal event ledger as the recovery log was rejected because it
  records consequences rather than complete validated inputs and has bounded
  retention.
- Periodic snapshots without a suffix log were rejected because acknowledged
  actions after the last save would remain unrecoverable.
