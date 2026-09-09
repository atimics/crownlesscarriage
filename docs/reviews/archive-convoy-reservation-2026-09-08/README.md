# Saved archive convoy reservation

Issue #240. Draft #632, based on #631. Final implementation `19716cb`.

`CcSimReserveArchiveConvoy` rechecks the relocation quote and stores its origin,
destination, sponsor, funding kingdom, carriage, first route and hop, and exact
book IDs. It transfers the first-leg toll from the sponsor kingdom and crew
wheat from the origin into the saved order. Public coin and goods totals include
that held money and food. Wheat prices refresh after each transfer.

The chosen carriage enters `Reserved for archive books`. Ordinary freight and
royal movement leave it at the origin while the order is reserved. Recruitment
and convoy reservations use their busy gates to keep these jobs separate.
The order records intended book IDs while books keep their current custody.
Departure must recheck each book's existence, condition, ownership, and location.

`CcSimCancelArchiveConvoy` checks both refund destinations before any mutation.
It returns the purse to the original kingdom, returns wheat to the origin,
releases the carriage, and clears the order. Full stores or treasuries preserve
the complete reservation for a later refund.

Schema 89 adds the order and its save table, hash entries, validation, and the
reserved-carriage mode. The simulation grows by 112 bytes to 184,576. Generation
25 and SQLite schema 32 stay the same. Schema-88 replay retains its prior rules.
Sponsor and book references use issued IDs, so historical records can survive
changes in live people and treasure slots. Departure will handle changed cargo.

## Validation

- Strict headless build and all 127 tests passed.
- The final carriage-invariant and refund checks passed the 16 focused
  headless checks after the full run.
- Strict desktop build and all 17 focused archive, quest, freight, and
  persistence checks passed.
- Static analysis passed with one reviewed baseline item.
- Emscripten 5.0.4 compiled the convoy module with warnings treated as errors,
  including the 184,576-byte simulation-size assertion.
- All 32 current worlds completed 100 years with valid annual states.
- All 3,200 schema-88 annual hashes match the parent behavior.

The complete-world reservation fixture checks exact coin and wheat conservation,
unchanged book custody, repeated reservation, recruitment exclusion, exact
reserve/cancel round-trip hash, a week of world time, journal restart, full
refund destinations, malformed order fields, oversized saved integers, and low
funds. Its pending carriage remains attached to the saved order throughout the
weekly world step.

The century probe exercises ordinary worlds and legacy compatibility. The
reservation fixture supplies the active-order save and refund evidence.
`results.json` retains all annual hashes and run statuses. `summary.json` records
the old-world comparison and current failures. `manifest.json` binds source,
compiler, probe, and library. The parent reference comes from #630's source-bound
schema-88 results; #631 verified unchanged world hashes for its quote addition.

## Reproduce

Build the headless library at the final implementation revision, then run:

```sh
cc -O2 -Isrc docs/reviews/archive-convoy-reservation-2026-09-08/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/archive-convoy-probe
python3 docs/reviews/archive-convoy-reservation-2026-09-08/run-study.py /tmp/archive-convoy-probe
ctest --test-dir out/build/headless -R archive_convoy_reservation --output-on-failure
```

## Next work

Departure must recheck the held books and route, pay the first toll, consume
crew provisions, and establish book travel custody. Travel interruption,
arrival, later legs, the seat-change event, and player or automatic initiation
remain part of the next saved journey work under #240. This draft exposes the
reservation API and keeps the issue open for that journey.
