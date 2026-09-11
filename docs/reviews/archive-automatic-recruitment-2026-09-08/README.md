# Automatic archive recruitment

This draft continues #446 and #470 on top of #616. Schema 82 connects weekly
institutional hiring and patron recovery to the saved recruitment path. The
saved fields, generator 25 and SQLite 32 stay the same. `CcSim` is 184448 bytes.

## Weekly decisions

The ledger's existing staffing thresholds set the desired number of places.
A healthy archive commissions a named recruit when it can afford another place.
After the existing five-year silence, eligible patrons can fund a replacement
from their actual treasuries. Both cases use the current quote and the same
reservation, road journey, training, payment, appointment and first-task rules.
Staff growth happens through appointment.

An active order keeps its funds and supplies. On the weekly check, ended orders
return their remaining resources through the existing capacity-checked refund.
A completed course whose recruit has died also returns its remaining supplies.
The next quote follows that refund. A full receiving store keeps the order
pending. Refund and commission events identify the archive place, recruit and
available institutional authority. Patron commissions retain the actual patron
and the stored donor shares.

## Accounting and replay

The world money and goods totals now include the held recruitment purse,
wheat, paper and tools for schema 82. Reservation transfers therefore preserve
these totals. Training consumes the held food and paper, and wages transfer
from the held purse to the named person's purse.

The earlier reset and ledger-rebound fixtures explicitly test schema 81.
The new test proves the schema 82 path. Its full journal begins before a weekly
patron commission, crosses actual travel and training, and reaches named staff
and a recorded task. It preserves the tracked money total and the replay hash.
Other cases cover normal hiring, early silence, broken roads, missing tools,
duplicate checks, weekly timing and failed-order refunds.

All 125 headless tests passed, including the existing raid, mine and war money
conservation tests that exposed the missing held-purse accounting. Static
analysis passed with one reviewed baseline item. The native build and nine
focused checks passed.

The older-schema probe compares two 40-year runs against parent
`78e1c5ff637d07e1ac5e25238ca24b148b824c9b`. All 82 schema 81 checkpoints match.
`legacy-parity.json` records the hashes. The schema 81 journal fixture also
proves replay before the behavior upgrade.

## Ordinary-world results and next work

`world-probe.c` compares schemas 81 and 82 for seeds 42 and 0x5eed over 40 years.
It records commissions, appointments, working named staff and daily gate
samples, with periodic full-world validation. `world-measurements.json` stores
the results.

Each schema 82 trial starts one own-ledger commission and reaches zero named
appointments. Seed 42 spends 2285 sampled days at a materials gate and 12292 at
a seat gate. Seed 0x5eed spends 9431 days at a materials gate and 5152 at a seat
gate. The fully supplied recovery fixture proves the path through appointment;
the ordinary worlds expose the supply work still needed for reliable recovery.

Supply recovery is the next integration work for #446 and #248. Player controls
also remain in scope. These measurements keep the full issues open.
