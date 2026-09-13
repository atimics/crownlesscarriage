# Saved archive seat and failure clock

Issue #240. Draft #630, based on #629. Implementation `dce4ad9`.

The archive saves its established seat by settlement ID. On the first weekly
archive update, it remembers the current seat. Later queries resolve that ID.
An abandoned saved seat yields an unavailable work location while its stored
identity remains available for recovery planning. The existing candidate and
seat-plan queries can select a viable destination for a later move.

The weekly check records the first observed day of material failure. A viable
chain clears the clock; a later failure starts a new period. Viability uses the
existing mill, paper, tools, and spare-wheat rule. This is weekly sampling.
Staff and book locations remain their recorded locations.

Schema 88 adds `archives.seat_id` and `archives.seat_failed_since_day`, their
hash entries, an `archive_seat` save table, strict integer reads, and validation.
The simulation grows by 16 bytes to 184,464. Generation 25 and SQLite schema 32
stay the same. Earlier saves begin with an unset seat and bind it on their next
current-version archive update. Schema-87 replay keeps the prior seat lookup.

## Verification

- Strict headless build and all 125 tests passed.
- Strict desktop build and 15 focused archive, quest, freight, and persistence
  tests passed.
- Added fractional saved-clock rejection passed in both headless and desktop
  seat tests after the full suite.
- Static analysis passed with one reviewed baseline item.
- Emscripten 5.0.4 compiled the seat module with warnings treated as errors,
  including the cross-platform simulation-size assertion.
- All 32 schema-88 worlds passed 100 annual validation checks each.
- All 3,200 schema-87 annual hashes match #629's original behavior.

The seat test covers stable identity, persistent failure timing, recovery,
renewed failure, journal restart, wrong entity type, future failure dates,
unset-seat consistency, abandoned-seat behavior, and preserved book custody.
The new schema-87 recruitment journal fixture verifies replay and upgrade.

`results.json` retains every annual hash and run status. `summary.json` records
the legacy comparison and current-world failures. The hash comparison uses
#629's saved source-bound results in the sibling writing-supply review.

## Reproduce

Build the headless library at the implementation revision, then run:

```sh
cc -O2 -Isrc docs/reviews/archive-saved-seat-2026-09-08/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/archive-seat-probe
python3 docs/reviews/archive-saved-seat-2026-09-08/run-study.py /tmp/archive-seat-probe
```

## Remaining #240 work

The saved seat and failure period provide inputs for a funded relocation.
The next change needs a named sponsor, a sustained-failure decision, real book
transport and its risks, and a rival foundation path. Those operations should
use this recorded origin and the existing viable-town plan. This draft keeps
#240 open for that work.
