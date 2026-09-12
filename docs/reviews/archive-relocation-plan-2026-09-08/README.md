# Sponsored first archive convoy quote

Issue #240. Draft #631, based on #630. Implementation `9d9e508`.

`CcSimArchiveRelocationPlan` quotes the first book convoy from a saved archive
seat after 365 days of recorded material failure. A healthy current chain keeps
the seat. An active recruitment order is completed or released before a move.

The quote selects up to four live archive books owned by and located at the
origin. Stable book IDs order the selection. Player books, destroyed volumes,
and books held elsewhere retain their custody outside this convoy.

Each viable destination supplies a living monastery patron or ruler as sponsor.
Its kingdom supplies the carriage and toll funds. The carriage starts at the
origin and meets the ordinary idle, condition, and cooldown rules. Each book
uses one cargo slot. Shared path and weekly road capacity bound the load.
A ready destination wins over a blocked candidate; candidate score and stable
settlement ID order equal cases. A different kingdom sets the rival-foundation
flag for the later work order.

The quote includes first route and hop, freight travel days, the current route
danger value, crew wheat, and first-leg toll. Crew wheat is two units for each
started seven-day travel period and comes from grain above the origin's
household reserve. The destination kingdom must hold the toll funds. Existing
archive passage rules apply to the path. Shared freight uses inhabited endpoints;
an abandoned-seat rescue needs its own travel path.

The interface is read-only. Schema 88, generation 25, saved fields, and world
size stay the same. A future work order must recheck the quote before reserving
books, transferring funds and wheat, or starting the carriage.

## Verification

- Strict headless build passed. The full run passed 125 existing tests; the
  corrected new road-capacity fixture passed its targeted rerun, completing
  all 126 checks.
- Strict desktop build and all 16 focused archive, quest, freight, and
  persistence tests passed.
- Static analysis passed with one reviewed baseline item.
- Emscripten 5.0.4 compiled the new module with warnings treated as errors.
- All 32 worlds completed 100 years with valid annual states.
- All 3,200 annual queries preserved the whole simulation byte for byte.
- All 3,200 annual hashes match #630's unchanged source.

The new fixtures cover book custody, destruction, stable order, one remaining
road slot, a blocked high-scoring destination, failure timing, a healthy seat,
active recruitment, missing books, living sponsorship, carriage location,
food, exact toll funds, and rival sponsorship. Every fixture checks whole-world
immutability around the query.

`results.json` records every annual hash and run status. `summary.json` records
the comparisons. `manifest.json` binds source revisions and the tested library
and probe. The study uses #630's source-bound results as its parent control.

## Reproduce

Build the headless library at the implementation revision, then run:

```sh
cc -O2 -Isrc docs/reviews/archive-relocation-plan-2026-09-08/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/archive-move-probe
python3 docs/reviews/archive-relocation-plan-2026-09-08/run-study.py /tmp/archive-move-probe
```

## Next work

The saved work order must bind the sponsor, first-leg payment, crew wheat,
carriage, and exact book IDs. It then needs reservation, departure, interrupted
travel, arrival, book custody transfer, and an explicit seat-change event.
Later loads and rival institutions need their own retained state. These steps
remain part of #240 acceptance.
