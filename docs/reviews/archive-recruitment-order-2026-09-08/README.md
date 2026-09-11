# Saved archive recruitment reservation

Base: PR #601, `351e31d`. Related issues: #446 and #470.

`CcSimBeginArchiveRecruitment` recalculates the recruitment quote, then moves
its money and supplies into one saved reservation. The record keeps the named
recruit, trainer, seat, source town, first route and hop, funders, patrons,
amounts, starting day, training requirements and estimated dates.

The archive ledger contributes the wage purse. Patron recovery first transfers
the quoted donor shares from their actual treasuries. Wheat, paper and tools
move from the seat's store into the reservation; travel wheat comes from the
source town. Goods prices refresh through the existing economy helper. This
stage reserves the resources for the later work steps.

The reservation uses status 1; status 0 is an empty record. A second begin call
returns false and preserves the whole simulation. Recruitment quotes report
`busy` while the reservation exists. Weekly archive updates respect this
pending reservation before making any legacy automatic staffing grant.

`CcSimCancelArchiveRecruitment` returns the reserved goods to their original
stores, donor shares to their treasuries, and the remaining purse to the
archive ledger. It checks all receiving limits before any transfer. A full
store or treasury leaves the complete reservation available for a later retry.
Cancellation also works after the recruit's lifetime ends. A second
cancellation preserves the whole simulation.

## Replayable commands

`CC_COMMAND_RESERVE_ARCHIVE_RECRUITMENT` and
`CC_COMMAND_CANCEL_ARCHIVE_RECRUITMENT` enter through the simulation command
boundary. Their target is the named recruit. The company must be at the archive
and ready for settlement business. Reservation checks the current eligible
person; cancellation checks the person held in the reservation. Stale targets
and duplicate requests preserve the full simulation.

The shared-command names are `reserve_archive_recruitment` and
`cancel_archive_recruitment`. Successful commands record a character interaction
with the company as actor, the recruit as target, and the archive as location.
The native client has matching confirmations.

A journal test begins with an empty reservation, records a reserve command,
recovers it, resumes the journal, records cancellation, and recovers again.
Money and all held goods balance at each step. The test also rejects requests
from another town, during a journey, for a stale person, and after the work has
already been reserved or cancelled.

## Save contract

Schema 78 adds a fixed one-row SQLite table, `archive_recruitment`. Its 23
fields are written, read, hashed and validated. Narrow fields are read as full
SQLite integers before their bounds are checked. A reserved identity remains
an issued character ID after retirement. Donor shares are bounded by the
held purse and donor IDs are distinct.

The simulation runner reports `archive_recruitment_order` with
`stored_reservation` semantics. The separate recruitment field remains the
current quote. Old schemas use an empty reservation after upgrade. Generator
25 and SQLite format 32 remain current. `sizeof(CcSim)` is 184336 bytes, an
increase of 160 bytes for the bounded record.

## Evidence

- Strict headless build and all 121 headless tests passed.
- Static analysis passed with one reviewed baseline item.
- The native play build and five focused native checks passed: recruitment
  reservation, shared command bridge, adventure input, bridge input and world
  card input. Shared command round-trip coverage now includes all 60 commands.
- `archive_order_tests` proves normal and patron-funded reservations conserve
  total money, wheat, paper and tools across the original stores and the held
  reservation. It also checks duplicate requests, capacity-safe cancellation,
  refunds to the actual donor, failed-request immutability, the weekly staffing
  boundary, and cancellation after the recruit dies.
- All 23 stored fields are compared directly after save/load. Twenty-two
  independent field mutations prove their hash sensitivity and saved values;
  status has separate hash and validation checks. The field suite also passes
  in its save-only mode.
- A saved reservation followed by a one-day journal suffix recovers with the
  same state hash. An integer above the 32-bit range is rejected on read.
- A schema-77 fixture drops the new table, replays its day journal, and upgrades.
- Two 40-year runs retain all 82 schema-77 checkpoint hashes from the parent.
  `legacy-probe.c` and `legacy-parity.json` provide the probe and results.

## Remaining work in this draft

The reservation API and saved record are ready for the next work-order stage.
Departure, leg-by-leg travel, training progress, refunds after interruption,
actual named staff appointment and the first archive task remain to implement.
The reserve and cancel commands already enter through these APIs. Player
controls and automatic recruitment can follow the progression steps. The full recruitment issue remains open.
