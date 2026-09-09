# Saved first archive convoy leg

Issue #240. Draft #635, based on #632. Implementation `b8c16e8`.

World updates now advance a reserved archive convoy through its first road leg.
Departure rechecks the living sponsor, exact books, current route, available
road slots, held wheat, held toll money, and the toll receiver's capacity. A
changed quote leaves the reservation ready for cancellation or a later retry.
A valid departure pays the toll, consumes the quoted crew provisions, reserves
road slots, and places each book's location on the actual carriage ID.

The order saves departure and arrival days. Its states are reserved (1),
travelling (2), arrived (3), lost (4), and waiting at a blocked road (5). When the
arrival is due, a failed road holds the convoy. Reopening permits arrival with
the ordinary road-danger roll and war-border adjustment. A safe arrival places
the books at the first hop. Road loss marks the books destroyed and records the
lost lore. The carriage returns to ordinary service after arrival or loss.

Book ownership stays with its recorded owner. Moving books use validated
carriage custody and remain outside shelf decay, local binding, kingdom archive
burning, and destroyed-slot reuse while in transit. The active order checks
book identity, ownership, live condition, and carriage location. Completed
orders retain historical book IDs. Cancellation then returns spare provisions
at the actual stop and spare money to the original funder.

Departure, a blocked road, arrival, and loss receive chronicle events tied to
the carriage and available named sponsor. Schema 90 saves the travel timing in
its own table. Generation 25 and SQLite schema 32 stay the same; simulation size
is 184,584 bytes. Schema-89 orders retain their reservation-only world behavior
during legacy replay.

## Verification

- Strict headless build and all 127 tests passed.
- Additional changed-custody and active schema-89 save-upgrade cases passed
  the convoy test after the full run.
- Strict desktop build and all 17 focused archive, quest, freight, and
  persistence checks passed with the additional cases.
- Static analysis passed with one reviewed baseline item.
- Emscripten 5.0.4 compiled the convoy module with warnings treated as errors,
  including the simulation-size assertion.
- All 32 current worlds completed 100 years with valid annual states.
- All 3,200 schema-89 annual hashes match the parent behavior.

The journey fixtures prove exact toll conservation and crew-food consumption,
carriage book custody, deferred cancellation during travel, a saved blocked
road, reopening and arrival, destroyed-book records and lost lore, a journal
restart in transit, and matching continuation hashes. They also cover changed
book custody before departure, a sponsor who dies before departure, refund
capacity, and an active old-schema reservation that stays pending and upgrades.

The century probe covers ordinary worlds and compatibility. The direct journey
fixtures supply the active transport evidence. `results.json` retains annual
hashes and run statuses; `summary.json` records the comparison. `manifest.json`
binds source revisions, compiler, probe, and library.

## Reproduce

Build the headless library at the implementation revision, then run:

```sh
cc -O2 -Isrc docs/reviews/archive-convoy-first-leg-2026-09-08/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/archive-leg-probe
python3 docs/reviews/archive-convoy-first-leg-2026-09-08/run-study.py /tmp/archive-leg-probe
ctest --test-dir out/build/headless -R archive_convoy_reservation --output-on-failure
```

## Remaining journey work

The order now completes the first hop. Later hops need fresh provisions and
tolls, alternate-route handling, and a final seat-change decision. A blocked
first road currently resumes when that road reopens. Player or automatic
reservation initiation and rival institution state remain part of #240.
